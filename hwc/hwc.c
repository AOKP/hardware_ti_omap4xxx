/*
 * Copyright (C) Texas Instruments - http://www.ti.com/
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <errno.h>
#include <malloc.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fb.h>

#include <cutils/properties.h>
#include <cutils/log.h>
#include <cutils/native_handle.h>
#include <hardware/hardware.h>
#include <hardware/hwcomposer.h>
#include <EGL/egl.h>
#include <utils/Timers.h>
#include <hardware_legacy/uevent.h>

#define ASPECT_RATIO_TOLERANCE 0.02f

#ifndef FBIO_WAITFORVSYNC
#define FBIO_WAITFORVSYNC	_IOW('F', 0x20, __u32)
#endif

#define min(a, b) ( { typeof(a) __a = (a), __b = (b); __a < __b ? __a : __b; } )
#define max(a, b) ( { typeof(a) __a = (a), __b = (b); __a > __b ? __a : __b; } )

#include <video/dsscomp.h>

#include "hal_public.h"

#define MAX_HW_OVERLAYS 4
#define NUM_NONSCALING_OVERLAYS 1
#define HAL_PIXEL_FORMAT_BGRX_8888		0x1FF
#define HAL_PIXEL_FORMAT_TI_NV12 0x100
#define HAL_PIXEL_FORMAT_TI_NV12_PADDED 0x101
#define MAX_TILER_SLOT (16 << 20)

#define MIN(a,b)		  ((a)<(b)?(a):(b))
#define MAX(a,b)		  ((a)>(b)?(a):(b))
#define CLAMP(x,low,high) (((x)>(high))?(high):(((x)<(low))?(low):(x)))

enum {
    EXT_ROTATION    = 3,        /* rotation while mirroring */
    EXT_HFLIP       = (1 << 2), /* flip l-r on output (after rotation) */
    EXT_TRANSFORM   = EXT_ROTATION | EXT_HFLIP,
    EXT_MIRROR      = (1 << 3), /* mirroring allowed */
    EXT_DOCK        = (1 << 4), /* docking allowed */
    EXT_TV          = (1 << 5), /* using TV for mirroring */
    EXT_DOCK_TRANSFORM_SHIFT = 6, /* transform for docking */
};

struct omap4_hwc_module {
    hwc_module_t base;

    IMG_framebuffer_device_public_t *fb_dev;
};
typedef struct omap4_hwc_module omap4_hwc_module_t;

struct omap4_hwc_device {
    hwc_composer_device_t base;
    hwc_procs_t *procs;
    pthread_t hdmi_thread;
    pthread_mutex_t lock;
    int dsscomp_fd;
    int fb_fd;
    int hdmi_fb_fd;

    __u16 ext_width;
    __u16 ext_height;
    __u32 ext_xres;
    __u32 ext_yres;
    __u32 last_xres_used;
    __u32 last_yres_used;
    float m[2][3];
    IMG_framebuffer_device_public_t *fb_dev;
    struct dsscomp_setup_dispc_data dsscomp_data;
    struct dsscomp_display_info fb_dis;

    buffer_handle_t *buffers;
    int use_sgx;
    int swap_rb;
    int use_tv;
    int mirror;
    unsigned int post2_layers;
    int last_ext_ovls;
    int last_int_ovls;
    int ext_ovls;
    int ext_ovls_wanted;

    int flags_rgb_order;
    int flags_nv12_only;
    int ext;
    int ext_requested;
    int ext_last;
};
typedef struct omap4_hwc_device omap4_hwc_device_t;

static int debug = 0;

static void dump_layer(hwc_layer_t const* l)
{
    LOGD("\ttype=%d, flags=%08x, handle=%p, tr=%02x, blend=%04x, {%d,%d,%d,%d}, {%d,%d,%d,%d}",
            l->compositionType, l->flags, l->handle, l->transform, l->blending,
            l->sourceCrop.left,
            l->sourceCrop.top,
            l->sourceCrop.right,
            l->sourceCrop.bottom,
            l->displayFrame.left,
            l->displayFrame.top,
            l->displayFrame.right,
            l->displayFrame.bottom);
}

static void dump_dsscomp(struct dsscomp_setup_dispc_data *d)
{
    unsigned i;

    LOGD("[%08x] set: %c%c%c %d ovls\n",
         d->sync_id,
         (d->mode & DSSCOMP_SETUP_MODE_APPLY) ? 'A' : '-',
         (d->mode & DSSCOMP_SETUP_MODE_DISPLAY) ? 'D' : '-',
         (d->mode & DSSCOMP_SETUP_MODE_CAPTURE) ? 'C' : '-',
         d->num_ovls);

    for (i = 0; i < d->num_mgrs; i++) {
        struct dss2_mgr_info *mi = d->mgrs + i;
        LOGD(" (dis%d alpha=%d col=%08x ilace=%d)\n",
            mi->ix,
            mi->alpha_blending, mi->default_color,
            mi->interlaced);
    }

    for (i = 0; i < d->num_ovls; i++) {
            struct dss2_ovl_info *oi = d->ovls + i;
            struct dss2_ovl_cfg *c = &oi->cfg;
            if (c->zonly)
                    LOGE("ovl%d(%s z%d)\n",
                         c->ix, c->enabled ? "ON" : "off", c->zorder);
            else
                    LOGE("ovl%d(%s z%d %x%s *%d%% %d*%d:%d,%d+%d,%d rot%d%s => %d,%d+%d,%d %p/%p|%d)\n",
                         c->ix, c->enabled ? "ON" : "off", c->zorder, c->color_mode,
                         c->pre_mult_alpha ? " premult" : "",
                         (c->global_alpha * 100 + 128) / 255,
                         c->width, c->height, c->crop.x, c->crop.y,
                         c->crop.w, c->crop.h,
                         c->rotation, c->mirror ? "+mir" : "",
                         c->win.x, c->win.y, c->win.w, c->win.h,
                         (void *) oi->ba, (void *) oi->uv, c->stride);
    }
}

static int omap4_hwc_is_valid_format(int format)
{
    switch(format) {
    case HAL_PIXEL_FORMAT_RGB_565:
    case HAL_PIXEL_FORMAT_RGBX_8888:
    case HAL_PIXEL_FORMAT_RGBA_8888:
    case HAL_PIXEL_FORMAT_BGRA_8888:
    case HAL_PIXEL_FORMAT_BGRX_8888:
    case HAL_PIXEL_FORMAT_TI_NV12:
    case HAL_PIXEL_FORMAT_TI_NV12_PADDED:
        return 1;

    default:
        return 0;
    }
}

static int scaled(hwc_layer_t *layer)
{
    int w = layer->sourceCrop.right - layer->sourceCrop.left;
    int h = layer->sourceCrop.bottom - layer->sourceCrop.top;

    if (layer->transform & HWC_TRANSFORM_ROT_90) {
        int t = w;
        w = h;
        h = t;
    }

    return (layer->displayFrame.right - layer->displayFrame.left != w ||
            layer->displayFrame.bottom - layer->displayFrame.top != h);
}

static int sync_id = 0;

#define is_BLENDED(blending) ((blending) != HWC_BLENDING_NONE)

#define is_RGB(format) ((format) == HAL_PIXEL_FORMAT_BGRA_8888 || (format) == HAL_PIXEL_FORMAT_RGB_565 || (format) == HAL_PIXEL_FORMAT_BGRX_8888)
#define is_BGR(format) ((format) == HAL_PIXEL_FORMAT_RGBX_8888 || (format) == HAL_PIXEL_FORMAT_RGBA_8888)
#define is_NV12(format) ((format) == HAL_PIXEL_FORMAT_TI_NV12 || (format) == HAL_PIXEL_FORMAT_TI_NV12_PADDED)

static int dockable(hwc_layer_t *layer)
{
    IMG_native_handle_t *handle = (IMG_native_handle_t *)layer->handle;

    return (handle->usage & GRALLOC_USAGE_EXTERNAL_DISP);
}

static unsigned int mem1d(IMG_native_handle_t *handle)
{
    if (handle == NULL)
            return 0;

    int bpp = is_NV12(handle->iFormat) ? 0 : (handle->iFormat == HAL_PIXEL_FORMAT_RGB_565 ? 2 : 4);
    int stride = ALIGN(handle->iWidth, HW_ALIGN) * bpp;
    return stride * handle->iHeight;
}

static void
omap4_hwc_setup_layer_base(struct dss2_ovl_cfg *oc, int index, int format, int width, int height)
{
    unsigned int bits_per_pixel;

    /* YUV2RGB conversion */
    const struct omap_dss_cconv_coefs ctbl_bt601_5 = {
        298,  409,    0,  298, -208, -100,  298,    0,  517, 0,
    };

    /* convert color format */
    switch (format) {
    case HAL_PIXEL_FORMAT_RGBX_8888:
        /* Should be XBGR32, but this isn't supported */
        oc->color_mode = OMAP_DSS_COLOR_RGB24U;
        bits_per_pixel = 32;
        break;

    case HAL_PIXEL_FORMAT_RGBA_8888:
        /* Should be ABGR32, but this isn't supported */
        oc->color_mode = OMAP_DSS_COLOR_ARGB32;
        bits_per_pixel = 32;
        break;

    case HAL_PIXEL_FORMAT_BGRA_8888:
        oc->color_mode = OMAP_DSS_COLOR_ARGB32;
        bits_per_pixel = 32;
        break;

    case HAL_PIXEL_FORMAT_BGRX_8888:
        oc->color_mode = OMAP_DSS_COLOR_RGB24U;
        bits_per_pixel = 32;
        break;

    case HAL_PIXEL_FORMAT_RGB_565:
        oc->color_mode = OMAP_DSS_COLOR_RGB16;
        bits_per_pixel = 16;
        break;

    case HAL_PIXEL_FORMAT_TI_NV12:
    case HAL_PIXEL_FORMAT_TI_NV12_PADDED:
        oc->color_mode = OMAP_DSS_COLOR_NV12;
        bits_per_pixel = 8;
        oc->cconv = ctbl_bt601_5;
        break;

    default:
        /* Should have been filtered out */
        LOGV("Unsupported pixel format");
        return;
    }

    oc->width = width;
    oc->height = height;
    oc->stride = ALIGN(width, HW_ALIGN) * bits_per_pixel / 8;

    oc->enabled = 1;
    oc->global_alpha = 255;
    oc->zorder = index;
    oc->ix = 0;

    /* defaults for SGX framebuffer renders */
    oc->crop.w = oc->win.w = width;
    oc->crop.h = oc->win.h = height;

    /* for now interlacing and vc1 info is not supplied */
    oc->ilace = OMAP_DSS_ILACE_NONE;
    oc->vc1.enable = 0;
}

static void
omap4_hwc_setup_layer(omap4_hwc_device_t *hwc_dev, struct dss2_ovl_info *ovl,
                      hwc_layer_t *layer, int index,
                      int format, int width, int height)
{
    struct dss2_ovl_cfg *oc = &ovl->cfg;

    //dump_layer(layer);

    if (format == HAL_PIXEL_FORMAT_BGRA_8888 && !is_BLENDED(layer->blending)) {
        format = HAL_PIXEL_FORMAT_BGRX_8888;
    }

    omap4_hwc_setup_layer_base(oc, index, format, width, height);

    /* convert transformation - assuming 0-set config */
    if (layer->transform & HWC_TRANSFORM_FLIP_H)
        oc->mirror = 1;
    if (layer->transform & HWC_TRANSFORM_FLIP_V) {
        oc->rotation = 2;
        oc->mirror = !oc->mirror;
    }
    if (layer->transform & HWC_TRANSFORM_ROT_90) {
        oc->rotation += oc->mirror ? -1 : 1;
        oc->rotation &= 3;
    }

    oc->pre_mult_alpha = layer->blending == HWC_BLENDING_PREMULT;

    /* display position */
    oc->win.x = layer->displayFrame.left;
    oc->win.y = layer->displayFrame.top;
    oc->win.w = layer->displayFrame.right - layer->displayFrame.left;
    oc->win.h = layer->displayFrame.bottom - layer->displayFrame.top;

    /* crop */
    oc->crop.x = layer->sourceCrop.left;
    oc->crop.y = layer->sourceCrop.top;
    oc->crop.w = layer->sourceCrop.right - layer->sourceCrop.left;
    oc->crop.h = layer->sourceCrop.bottom - layer->sourceCrop.top;
}

const float m_unit[2][3] = { { 1., 0., 0. }, { 0., 1., 0. } };

static inline void m_translate(float m[2][3], int dx, int dy)
{
    m[0][2] += dx;
    m[1][2] += dy;
}

static inline void m_scale1(float m[3], int from, int to)
{
    m[0] = m[0] * to / from;
    m[1] = m[1] * to / from;
    m[2] = m[2] * to / from;
}

static inline void m_scale(float m[2][3], int x_from, int x_to, int y_from, int y_to)
{
    m_scale1(m[0], x_from, x_to);
    m_scale1(m[1], y_from, y_to);
}

static void m_rotate(float m[2][3], int quarter_turns)
{
    if (quarter_turns & 2)
        m_scale(m, 1, -1, 1, -1);
    if (quarter_turns & 1) {
        int q;
        q = m[0][0]; m[0][0] = -m[1][0]; m[1][0] = q;
        q = m[0][1]; m[0][1] = -m[1][1]; m[1][1] = q;
        q = m[0][2]; m[0][2] = -m[1][2]; m[1][2] = q;
    }
}

static inline int m_round(float x)
{
    /* int truncates towards 0 */
    return (int) (x < 0 ? x - 0.5 : x + 0.5);
}

/*
 * assuming xratio:yratio original pixel ratio, calculate the adjusted width
 * and height for a screen of xres/yres and physical size of width/height.
 * The adjusted size is the largest that fits into the screen.
 */
static void get_max_dimensions(__u32 orig_xres, __u32 orig_yres,
                               __u32 orig_xratio, __u32 orig_yratio,
                               __u32 scr_xres, __u32 scr_yres,
                               __u32 scr_width, __u32 scr_height,
                               __u32 *adj_xres, __u32 *adj_yres)
{
    /* assume full screen (largest size)*/
    *adj_xres = scr_xres;
    *adj_yres = scr_yres;

    /* assume 1:1 pixel ratios if none supplied */
    if (!scr_width || !scr_height) {
        scr_width = scr_xres;
        scr_height = scr_yres;
    }
    if (!orig_xratio || !orig_yratio) {
        orig_xratio = 1;
        orig_yratio = 1;
    }

    /* trim to keep aspect ratio */
    float x_factor = orig_xres * orig_xratio * (float) scr_height;
    float y_factor = orig_yres * orig_yratio * (float) scr_width;

    /* allow for tolerance so we avoid scaling if framebuffer is standard size */
    if (x_factor < y_factor * (1.f - ASPECT_RATIO_TOLERANCE))
        *adj_xres = (__u32) (x_factor * *adj_xres / y_factor + 0.5);
    else if (x_factor * (1.f - ASPECT_RATIO_TOLERANCE) > y_factor)
        *adj_yres = (__u32) (y_factor * *adj_yres / x_factor + 0.5);
}

static void set_ext_matrix(omap4_hwc_device_t *hwc_dev, int orig_w, int orig_h)
{
    /* assume 1:1 lcd pixel ratio */
    int source_x = 1;
    int source_y = 1;

    /* reorientation matrix is:
       m = (center-from-target-center) * (scale-to-target) * (mirror) * (rotate) * (center-to-original-center) */

    memcpy(hwc_dev->m, m_unit, sizeof(m_unit));
    m_translate(hwc_dev->m, -orig_w >> 1, -orig_h >> 1);
    m_rotate(hwc_dev->m, hwc_dev->ext & 3);
    if (hwc_dev->ext & EXT_HFLIP)
        m_scale(hwc_dev->m, 1, -1, 1, 1);

    if (hwc_dev->ext & EXT_ROTATION & 1) {
        int q = orig_w;
        orig_w = orig_h;
        orig_h = q;
        q = source_x;
        source_x = source_y;
        source_y = q;
    }

    /* get target size */
    __u32 adj_xres, adj_yres;
    get_max_dimensions(orig_w, orig_h, source_x, source_y,
                       hwc_dev->ext_xres, hwc_dev->ext_yres, hwc_dev->ext_width, hwc_dev->ext_height,
                       &adj_xres, &adj_yres);

    m_scale(hwc_dev->m, orig_w, adj_xres, orig_h, adj_yres);
    m_translate(hwc_dev->m, hwc_dev->ext_xres >> 1, hwc_dev->ext_yres >> 1);
}

static void
omap4_hwc_create_ext_matrix(omap4_hwc_device_t *hwc_dev)
{
    /* use VGA external resolution as default */
    if (!hwc_dev->ext_xres ||
        !hwc_dev->ext_yres) {
        hwc_dev->ext_xres = 640;
        hwc_dev->ext_yres = 480;
    }

    /* if docking, we cannot create the matrix ahead of time as it depends on input size */
    if (hwc_dev->ext & EXT_MIRROR)
        set_ext_matrix(hwc_dev, hwc_dev->fb_dev->base.width, hwc_dev->fb_dev->base.height);
}

static void
omap4_hwc_adjust_ext_layer(omap4_hwc_device_t *hwc_dev, struct dss2_ovl_info *ovl)
{
    struct dss2_ovl_cfg *oc = &ovl->cfg;
    float x, y, w, h;

    /* display position */
    x = hwc_dev->m[0][0] * oc->win.x + hwc_dev->m[0][1] * oc->win.y + hwc_dev->m[0][2];
    y = hwc_dev->m[1][0] * oc->win.x + hwc_dev->m[1][1] * oc->win.y + hwc_dev->m[1][2];
    w = hwc_dev->m[0][0] * oc->win.w + hwc_dev->m[0][1] * oc->win.h;
    h = hwc_dev->m[1][0] * oc->win.w + hwc_dev->m[1][1] * oc->win.h;
    oc->win.x = m_round(w > 0 ? x : x + w);
    oc->win.y = m_round(h > 0 ? y : y + h);
    oc->win.w = m_round(w > 0 ? w : -w);
    oc->win.h = m_round(h > 0 ? h : -h);

    /* combining transformations: F^a*R^b*F^i*R^j = F^(a+b)*R^(j+b*(-1)^i), because F*R = R^(-1)*F */
    oc->rotation += (oc->mirror ? -1 : 1) * (hwc_dev->ext & EXT_ROTATION);
    oc->rotation &= 3;
    if (hwc_dev->ext & EXT_HFLIP)
        oc->mirror = !oc->mirror;
}

static struct dsscomp_dispc_limitations {
    __u8 max_xdecim_2d;
    __u8 max_ydecim_2d;
    __u8 max_xdecim_1d;
    __u8 max_ydecim_1d;
    __u32 fclk;
    __u8 max_downscale;
    __u8 min_width;
    __u16 integer_scale_ratio_limit;
} limits = {
    .max_xdecim_1d = 16,
    .max_xdecim_2d = 16,
    .max_ydecim_1d = 16,
    .max_ydecim_2d = 2,
    .fclk = 170666666,
    .max_downscale = 4,
    .min_width = 2,
    .integer_scale_ratio_limit = 2048,
};

static int omap4_hwc_can_scale(int src_w, int src_h, int dst_w, int dst_h, int is_nv12,
                               struct dsscomp_display_info *dis, struct dsscomp_dispc_limitations *limits,
                               __u32 pclk)
{
    __u32 fclk = limits->fclk / 1000;

    /* ERRATAs */
    /* cannot render 1-width layers on DSI video mode panels - we just disallow all 1-width LCD layers */
    if (dis->channel != OMAP_DSS_CHANNEL_DIGIT && dst_w < limits->min_width)
        return 0;

    /* NOTE: no support for checking YUV422 layers that are tricky to scale */

    /* max downscale */
    if (dst_h < src_h / limits->max_downscale / (is_nv12 ? limits->max_ydecim_2d : limits->max_ydecim_1d))
        return 0;

    /* for manual panels pclk is 0, and there are no pclk based scaling limits */
    if (!pclk)
        return (dst_w < src_w / limits->max_downscale / (is_nv12 ? limits->max_xdecim_2d : limits->max_xdecim_1d));

    /* :HACK: limit horizontal downscale well below theoretical limit as we saw display artifacts */
    if (dst_w < src_w / 4)
        return 0;

    /* max horizontal downscale is 4, or the fclk/pixclk */
    if (fclk > pclk * limits->max_downscale)
        fclk = pclk * limits->max_downscale;
    /* for small parts, we need to use integer fclk/pixclk */
    if (src_w < limits->integer_scale_ratio_limit)
        fclk = fclk / pclk * pclk;
    if (dst_w < src_w * pclk / fclk / (is_nv12 ? limits->max_xdecim_2d : limits->max_xdecim_1d))
        return 0;

    return 1;
}

static int omap4_hwc_can_scale_layer(omap4_hwc_device_t *hwc_dev, hwc_layer_t *layer, IMG_native_handle_t *handle)
{
    int src_w = layer->sourceCrop.right - layer->sourceCrop.left;
    int src_h = layer->sourceCrop.bottom - layer->sourceCrop.top;
    int dst_w = layer->displayFrame.right - layer->displayFrame.left;
    int dst_h = layer->displayFrame.bottom - layer->displayFrame.top;

    /* account for 90-degree rotation */
    if (layer->transform & HWC_TRANSFORM_ROT_90) {
        int tmp = src_w;
        src_w = src_h;
        src_h = tmp;
    }

    /* NOTE: layers should be able to be scaled externally since
       framebuffer is able to be scaled on selected external resolution */
    return omap4_hwc_can_scale(src_w, src_h, dst_w, dst_h, is_NV12(handle->iFormat), &hwc_dev->fb_dis, &limits,
                               hwc_dev->fb_dis.timings.pixel_clock);
}

static int omap4_hwc_is_valid_layer(omap4_hwc_device_t *hwc_dev,
                                    hwc_layer_t *layer,
                                    IMG_native_handle_t *handle)
{
    /* Skip layers are handled by SF */
    if ((layer->flags & HWC_SKIP_LAYER) || !handle)
        return 0;

    if (!omap4_hwc_is_valid_format(handle->iFormat))
        return 0;

    /* 1D buffers: no transform, must fit in TILER slot */
    if (!is_NV12(handle->iFormat)) {
        if (layer->transform)
            return 0;
        if (mem1d(handle) > MAX_TILER_SLOT)
            return 0;
    }

    return omap4_hwc_can_scale_layer(hwc_dev, layer, handle);
}

static int omap4_hwc_set_best_hdmi_mode(omap4_hwc_device_t *hwc_dev, __u32 xres, __u32 yres,
					__u32 xratio, __u32 yratio)
{
    struct _qdis {
        struct dsscomp_display_info dis;
        struct dsscomp_videomode modedb[16];
    } d = { .dis = { .ix = 1 } };

    d.dis.modedb_len = sizeof(d.modedb) / sizeof(*d.modedb);
    int ret = ioctl(hwc_dev->dsscomp_fd, DSSCOMP_QUERY_DISPLAY, &d);
    if (ret)
        return ret;

    if (d.dis.timings.x_res * d.dis.timings.y_res == 0 ||
        xres * yres == 0)
        return -EINVAL;

    __u32 i, best = ~0, best_score = 0;
    hwc_dev->ext_width = d.dis.width_in_mm;
    hwc_dev->ext_height = d.dis.height_in_mm;
    hwc_dev->ext_xres = d.dis.timings.x_res;
    hwc_dev->ext_yres = d.dis.timings.y_res;
    __u32 ext_fb_xres, ext_fb_yres;
    for (i = 0; i < d.dis.modedb_len; i++) {
        __u32 score = 0;
        __u32 area = xres * yres;
        __u32 mode_area = d.modedb[i].xres * d.modedb[i].yres;
        __u32 ext_width = d.dis.width_in_mm;
        __u32 ext_height = d.dis.height_in_mm;

        if (d.modedb[i].flag & FB_FLAG_RATIO_4_3) {
            ext_width = 4;
            ext_height = 3;
        } else if (d.modedb[i].flag & FB_FLAG_RATIO_16_9) {
            ext_width = 16;
            ext_height = 9;
        }

        if (mode_area == 0)
            continue;

        get_max_dimensions(xres, yres, xratio, yratio, d.modedb[i].xres, d.modedb[i].yres,
                           ext_width, ext_height, &ext_fb_xres, &ext_fb_yres);

        if (!d.modedb[i].pixclock ||
            d.modedb[i].vmode ||
            !omap4_hwc_can_scale(xres, yres, ext_fb_xres, ext_fb_yres,
                                 hwc_dev->ext & EXT_TRANSFORM, &d.dis, &limits,
                                 1000000000 / d.modedb[i].pixclock))
            continue;

        /* prefer CEA modes */
        if (d.modedb[i].flag & (FB_FLAG_RATIO_4_3 | FB_FLAG_RATIO_16_9))
            score |= (1 << 30);

        /* prefer to upscale (1% tolerance) */
        if (ext_fb_xres >= xres * 99 / 100 && ext_fb_yres >= yres * 99 / 100)
            score |= (1 << 29);

        /* pick closest screen size */
        if (ext_fb_xres * ext_fb_yres > area)
            score |= (1 << 24) * (16 * area / ext_fb_xres / ext_fb_yres);
        else
            score |= (1 << 24) * (16 * ext_fb_xres * ext_fb_yres / area);

        /* pick smallest leftover area */
        score |= (1 << 19) * ((16 * ext_fb_xres * ext_fb_yres + (mode_area >> 1)) / mode_area);

        /* pick highest frame rate */
        score |= (1 << 11) * d.modedb[i].refresh;

        LOGD("#%d: %dx%d %dHz", i, d.modedb[i].xres, d.modedb[i].yres, d.modedb[i].refresh);
        if (debug)
            LOGD("  score=%u adj.res=%dx%d", score, ext_fb_xres, ext_fb_yres);
        if (best_score < score) {
            hwc_dev->ext_width = ext_width;
            hwc_dev->ext_height = ext_height;
            hwc_dev->ext_xres = d.modedb[i].xres;
            hwc_dev->ext_yres = d.modedb[i].yres;
            best = i;
            best_score = score;
        }
    }
    if (~best) {
        struct dsscomp_setup_display_data sdis = { .ix = 1, };
        sdis.mode = d.dis.modedb[best];
        LOGD("picking #%d", best);
        ioctl(hwc_dev->dsscomp_fd, DSSCOMP_SETUP_DISPLAY, &sdis);
    } else {
        __u32 ext_width = d.dis.width_in_mm;
        __u32 ext_height = d.dis.height_in_mm;
        __u32 ext_fb_xres, ext_fb_yres;

        get_max_dimensions(xres, yres, xratio, yratio, d.dis.timings.x_res, d.dis.timings.y_res,
                           ext_width, ext_height, &ext_fb_xres, &ext_fb_yres);
        if (!d.dis.timings.pixel_clock ||
            d.dis.mgr.interlaced ||
            !omap4_hwc_can_scale(xres, yres, ext_fb_xres, ext_fb_yres,
                                 hwc_dev->ext & EXT_TRANSFORM, &d.dis, &limits,
                                 d.dis.timings.pixel_clock)) {
            LOGE("DSS scaler cannot support HDMI cloning");
            return -1;
        }
    }
    hwc_dev->last_xres_used = xres;
    hwc_dev->last_yres_used = yres;
    if (d.dis.channel == OMAP_DSS_CHANNEL_DIGIT)
        hwc_dev->ext |= EXT_TV;
    return 0;
}

struct counts {
    unsigned int possible_overlay_layers;
    unsigned int composited_layers;
    unsigned int scaled_layers;
    unsigned int RGB;
    unsigned int BGR;
    unsigned int NV12;
    unsigned int dockable;
    unsigned int displays;
    unsigned int max_hw_overlays;
    unsigned int max_scaling_overlays;
    unsigned int mem;
};

static inline int can_dss_render_all(omap4_hwc_device_t *hwc_dev, struct counts *num)
{
    int tv = hwc_dev->ext & EXT_TV;
    int nonscaling_ovls = NUM_NONSCALING_OVERLAYS;
    num->max_hw_overlays = MAX_HW_OVERLAYS;

    /*
     * We cannot atomically switch overlays from one display to another.  First, they
     * have to be disabled, and the disabling has to take effect on the current display.
     * We keep track of the available number of overlays here.
     */
    if ((hwc_dev->ext & EXT_DOCK) && !((hwc_dev->ext & EXT_MIRROR) && !num->dockable)) {
        /* some overlays may already be used by the external display, so we account for this */

        /* reserve just a video pipeline for HDMI if docking */
        hwc_dev->ext_ovls = num->dockable ? 1 : 0;
        num->max_hw_overlays -= max(hwc_dev->ext_ovls, hwc_dev->last_ext_ovls);
        hwc_dev->ext &= ~EXT_TRANSFORM & ~EXT_MIRROR;
        hwc_dev->ext |= EXT_DOCK | ((hwc_dev->ext >> EXT_DOCK_TRANSFORM_SHIFT) & EXT_TRANSFORM);
    } else if (hwc_dev->ext & EXT_MIRROR) {
        /*
         * otherwise, manage just from half the pipelines.  NOTE: there is
         * no danger of having used too many overlays for external display here.
         */
        num->max_hw_overlays >>= 1;
        nonscaling_ovls >>= 1;
        hwc_dev->ext_ovls = MAX_HW_OVERLAYS - num->max_hw_overlays;
        hwc_dev->ext &= ~EXT_DOCK;
    } else {
        num->max_hw_overlays -= hwc_dev->last_ext_ovls;
        hwc_dev->ext_ovls = 0;
        hwc_dev->ext = 0;
    }
    int tform = hwc_dev->ext & EXT_TRANSFORM;

    /*
     * :TRICKY: We may not have enough overlays on the external display.  We "reserve" them
     * here to figure out if mirroring is supported, but may not do mirroring for the first
     * frame while the overlays required for it are cleared.
     */
    hwc_dev->ext_ovls_wanted = hwc_dev->ext_ovls;
    hwc_dev->ext_ovls = min(MAX_HW_OVERLAYS - hwc_dev->last_int_ovls, hwc_dev->ext_ovls);

    /* if not docking, we may be limited by last used external overlays */
    if (hwc_dev->ext_ovls && (hwc_dev->ext & EXT_MIRROR) && !(hwc_dev->ext & EXT_DOCK))
         num->max_hw_overlays = hwc_dev->ext_ovls;

    num->max_scaling_overlays = num->max_hw_overlays - nonscaling_ovls;

    return  /* must have at least one layer if using composition bypass to get sync object */
            num->possible_overlay_layers &&
            num->possible_overlay_layers <= num->max_hw_overlays &&
            num->possible_overlay_layers == num->composited_layers &&
            num->scaled_layers <= num->max_scaling_overlays &&
            num->NV12 <= num->max_scaling_overlays &&
            /* fits into TILER slot */
            num->mem <= MAX_TILER_SLOT &&
            /* we cannot clone non-NV12 transformed layers */
            (!tform || num->NV12 == num->possible_overlay_layers) &&
            /* HDMI cannot display BGR */
            (num->BGR == 0 || (num->RGB == 0 && !tv) || !hwc_dev->flags_rgb_order);
}

static inline int can_dss_render_layer(omap4_hwc_device_t *hwc_dev,
            hwc_layer_t *layer)
{
    IMG_native_handle_t *handle = (IMG_native_handle_t *)layer->handle;

    int tv = hwc_dev->ext & EXT_TV;
    int tform = hwc_dev->ext & EXT_TRANSFORM;

    return omap4_hwc_is_valid_layer(hwc_dev, layer, handle) &&
           /* cannot rotate non-NV12 layers on external display */
           (!tform || is_NV12(handle->iFormat)) &&
           /* skip non-NV12 layers if also using SGX (if nv12_only flag is set) */
           (!hwc_dev->flags_nv12_only || (!hwc_dev->use_sgx || is_NV12(handle->iFormat))) &&
           /* make sure RGB ordering is consistent (if rgb_order flag is set) */
           (!(hwc_dev->swap_rb ? is_RGB(handle->iFormat) : is_BGR(handle->iFormat)) ||
            !hwc_dev->flags_rgb_order) &&
           /* TV can only render RGB */
           !(tv && is_BGR(handle->iFormat));
}

static inline int display_area(struct dss2_ovl_info *o)
{
    return o->cfg.win.w * o->cfg.win.h;
}

static int omap4_hwc_prepare(struct hwc_composer_device *dev, hwc_layer_list_t* list)
{
    omap4_hwc_device_t *hwc_dev = (omap4_hwc_device_t *)dev;
    struct dsscomp_setup_dispc_data *dsscomp = &hwc_dev->dsscomp_data;
    struct counts num = { .composited_layers = list ? list->numHwLayers : 0 };
    unsigned int i;

    pthread_mutex_lock(&hwc_dev->lock);
    hwc_dev->ext = hwc_dev->ext_requested;
    memset(dsscomp, 0x0, sizeof(*dsscomp));
    dsscomp->sync_id = sync_id++;

    /* Figure out how many layers we can support via DSS */
    for (i = 0; list && i < list->numHwLayers; i++) {
        hwc_layer_t *layer = &list->hwLayers[i];
        IMG_native_handle_t *handle = (IMG_native_handle_t *)layer->handle;

        layer->compositionType = HWC_FRAMEBUFFER;

        if (omap4_hwc_is_valid_layer(hwc_dev, layer, handle)) {
            num.possible_overlay_layers++;

            /* NV12 layers can only be rendered on scaling overlays */
            if (scaled(layer) || is_NV12(handle->iFormat))
                num.scaled_layers++;

            if (is_BGR(handle->iFormat))
                num.BGR++;
            else if (is_RGB(handle->iFormat))
                num.RGB++;
            else if (is_NV12(handle->iFormat))
                num.NV12++;

            if (dockable(layer))
                num.dockable++;

            num.mem += mem1d(handle);
        }
    }

    /* phase 3 logic */
    if (can_dss_render_all(hwc_dev, &num)) {
        /* All layers can be handled by the DSS -- don't use SGX for composition */
        hwc_dev->use_sgx = 0;
        hwc_dev->swap_rb = num.BGR != 0;
    } else {
        /* Use SGX for composition plus first 3 layers that are DSS renderable */
        hwc_dev->use_sgx = 1;
        hwc_dev->swap_rb = is_BGR(hwc_dev->fb_dev->base.format);
    }
    if (debug) {
        LOGD("prepare (%d) - %s (comp=%d, poss=%d/%d scaled, RGB=%d,BGR=%d,NV12=%d) (ext=%x, %dex/%dmx (last %dex,%din)\n",
         dsscomp->sync_id,
         hwc_dev->use_sgx ? "SGX+OVL" : "all-OVL",
         num.composited_layers,
         num.possible_overlay_layers, num.scaled_layers,
         num.RGB, num.BGR, num.NV12,
         hwc_dev->ext, hwc_dev->ext_ovls, num.max_hw_overlays, hwc_dev->last_ext_ovls, hwc_dev->last_int_ovls);
    }

    /* setup pipes */
    dsscomp->num_ovls = hwc_dev->use_sgx;
    int z = 0;
    int fb_z = -1;
    int scaled_gfx = 0;
    int ix_docking = -1;

    /* set up if DSS layers */
    unsigned int mem_used = 0;
    for (i = 0; list && i < list->numHwLayers; i++) {
        hwc_layer_t *layer = &list->hwLayers[i];
        IMG_native_handle_t *handle = (IMG_native_handle_t *)layer->handle;

        if (dsscomp->num_ovls < num.max_hw_overlays &&
            can_dss_render_layer(hwc_dev, layer) &&
            mem_used + mem1d(handle) < MAX_TILER_SLOT &&
            /* can't have a transparent overlay in the middle of the framebuffer stack */
            !(is_BLENDED(layer->blending) && fb_z >= 0)) {
            /* render via DSS overlay */
            mem_used += mem1d(handle);
            layer->compositionType = HWC_OVERLAY;
            hwc_dev->buffers[dsscomp->num_ovls] = handle;

            omap4_hwc_setup_layer(hwc_dev,
                                  &dsscomp->ovls[dsscomp->num_ovls],
                                  layer,
                                  z,
                                  handle->iFormat,
                                  handle->iWidth,
                                  handle->iHeight);

            dsscomp->ovls[dsscomp->num_ovls].cfg.ix = dsscomp->num_ovls;
            /* just marking dss layers */
            dsscomp->ovls[dsscomp->num_ovls].address = (void *) (dsscomp->num_ovls * 4096 + 0xA0000000);
            dsscomp->ovls[dsscomp->num_ovls].uv = (__u32) hwc_dev->buffers[dsscomp->num_ovls];

            /* ensure GFX layer is never scaled */
            if (dsscomp->num_ovls == 0) {
                scaled_gfx = scaled(layer) || is_NV12(handle->iFormat);
            } else if (scaled_gfx && !scaled(layer) && !is_NV12(handle->iFormat)) {
                /* swap GFX layer with this one */
                dsscomp->ovls[dsscomp->num_ovls].cfg.ix = 0;
                dsscomp->ovls[0].cfg.ix = dsscomp->num_ovls;
                scaled_gfx = 0;
            }

            /* remember largest dockable layer */
            if (dockable(layer) &&
                (ix_docking < 0 ||
                 display_area(dsscomp->ovls + dsscomp->num_ovls) > display_area(dsscomp->ovls + ix_docking)))
                ix_docking = dsscomp->num_ovls;

            dsscomp->num_ovls++;
            z++;
        } else if (hwc_dev->use_sgx) {
            if (fb_z < 0) {
                /* NOTE: we are not handling transparent cutout for now */
                fb_z = z;
                z++;
            } else {
                /* move fb z-order up (by lowering dss layers) */
                while (fb_z < z - 1)
                    dsscomp->ovls[1 + fb_z++].cfg.zorder--;
            }
        }
    }

    /* clear FB above all opaque layers if rendering via SGX */
    if (hwc_dev->use_sgx) {
        for (i = 0; list && i < list->numHwLayers; i++) {
            hwc_layer_t *layer = &list->hwLayers[i];
            IMG_native_handle_t *handle = (IMG_native_handle_t *)layer->handle;
            if (layer->compositionType == HWC_FRAMEBUFFER)
                continue;
            if ((layer->flags & HWC_SKIP_LAYER) || !layer->handle)
                continue;
            if (!is_BLENDED(layer->blending))
                layer->hints |= HWC_HINT_CLEAR_FB;
        }
    }

    /* if scaling GFX (e.g. only 1 scaled surface) use a VID pipe */
    if (scaled_gfx)
        dsscomp->ovls[0].cfg.ix = dsscomp->num_ovls;

    /* assign a z-layer for fb */
    if (hwc_dev->use_sgx && fb_z < 0) {
        fb_z = z;
        z++;
    }

    if (hwc_dev->use_sgx) {
        hwc_dev->buffers[0] = NULL;
        omap4_hwc_setup_layer_base(&dsscomp->ovls[0].cfg, fb_z,
                                   hwc_dev->fb_dev->base.format,
                                   hwc_dev->fb_dev->base.width,
                                   hwc_dev->fb_dev->base.height);
        dsscomp->ovls[0].cfg.pre_mult_alpha = 1;
        dsscomp->ovls[0].uv = (__u32) hwc_dev->buffers[0];
    }

    /* mirror layers */
    hwc_dev->post2_layers = dsscomp->num_ovls;
    if (hwc_dev->ext && hwc_dev->ext_ovls) {
        int ix_back, ix_front, ix;
        if (hwc_dev->ext & EXT_DOCK) {
            /* mirror only 1 external layer */
            ix_back = ix_front = ix_docking;
        } else {
            /* mirror all layers */
            ix_back = 0;
            ix_front = dsscomp->num_ovls - 1;

            /* reset mode if we are coming from docking */
            if (hwc_dev->ext != hwc_dev->ext_last) {
                __u32 xres = (hwc_dev->ext & 1) ? hwc_dev->fb_dev->base.height : hwc_dev->fb_dev->base.width;
                __u32 yres = (hwc_dev->ext & 1) ? hwc_dev->fb_dev->base.width : hwc_dev->fb_dev->base.height;
                omap4_hwc_set_best_hdmi_mode(hwc_dev, xres, yres, 1, 1);
                set_ext_matrix(hwc_dev, hwc_dev->fb_dev->base.width, hwc_dev->fb_dev->base.height);
            }
        }

        for (ix = ix_back; hwc_dev->ext && ix >= 0 && ix <= ix_front; ix++) {
            struct dss2_ovl_info *o = dsscomp->ovls + dsscomp->num_ovls;
            memcpy(o, dsscomp->ovls + ix, sizeof(dsscomp->ovls[ix]));
            o->cfg.zorder += hwc_dev->post2_layers;

            /* reserve overlays at end for other display */
            o->cfg.ix = MAX_HW_OVERLAYS - 1 - (ix - ix_back);
            o->cfg.mgr_ix = 1;
            o->ba = ix;

            if (hwc_dev->ext & EXT_DOCK) {
                /* full screen video */
                o->cfg.win.x = 0;
                o->cfg.win.y = 0;
                if (o->cfg.rotation & 1) {
                    __u16 t = o->cfg.win.w;
                    o->cfg.win.w = o->cfg.win.h;
                    o->cfg.win.h = t;
                }
                o->cfg.rotation = 0;
                o->cfg.mirror = 0;

                /* adjust hdmi mode based on resolution */
                if (o->cfg.crop.w != hwc_dev->last_xres_used ||
                    o->cfg.crop.h != hwc_dev->last_yres_used) {
                    LOGD("set up HDMI for %d*%d\n", o->cfg.crop.w, o->cfg.crop.h);
                    if (omap4_hwc_set_best_hdmi_mode(hwc_dev, o->cfg.crop.w, o->cfg.crop.h,
                                                     o->cfg.win.w * o->cfg.crop.h,
                                                     o->cfg.win.h * o->cfg.crop.w)) {
                        o->cfg.enabled = 0;
                        hwc_dev->ext = 0;
                        continue;
                    }
                }

                set_ext_matrix(hwc_dev, o->cfg.win.w, o->cfg.win.h);
            }
            omap4_hwc_adjust_ext_layer(hwc_dev, o);
            dsscomp->num_ovls++;
            z++;
        }
    }
    hwc_dev->ext_last = hwc_dev->ext;

    if (z != dsscomp->num_ovls || dsscomp->num_ovls > MAX_HW_OVERLAYS)
        LOGE("**** used %d z-layers for %d overlays\n", z, dsscomp->num_ovls);

    dsscomp->mode = DSSCOMP_SETUP_DISPLAY;
    dsscomp->mgrs[0].ix = 0;
    dsscomp->mgrs[0].alpha_blending = 1;
    dsscomp->mgrs[0].swap_rb = hwc_dev->swap_rb;
    dsscomp->num_mgrs = 1;

    if (hwc_dev->ext || hwc_dev->last_ext_ovls) {
        dsscomp->mgrs[1] = dsscomp->mgrs[0];
        dsscomp->mgrs[1].ix = 1;
        dsscomp->num_mgrs++;
        hwc_dev->ext_ovls = dsscomp->num_ovls - hwc_dev->post2_layers;
    }
    pthread_mutex_unlock(&hwc_dev->lock);
    return 0;
}

static void omap4_hwc_reset_screen(omap4_hwc_device_t *hwc_dev)
{
    static int first_set = 1;
    int ret;

    if (first_set) {
        first_set = 0;
        struct dsscomp_setup_dispc_data d = {
                .num_mgrs = 1,
        };
        /* remove bootloader image from the screen as blank/unblank does not change the composition */
        ret = ioctl(hwc_dev->dsscomp_fd, DSSCOMP_SETUP_DISPC, &d);
        if (ret)
            LOGW("failed to remove bootloader image");

        /* blank and unblank fd to make sure display is properly programmed on boot.
         * This is needed because the bootloader can not be trusted.
         */
        ret = ioctl(hwc_dev->fb_fd, FBIOBLANK, FB_BLANK_POWERDOWN);
        if (ret)
            LOGW("failed to blank display");

        ret = ioctl(hwc_dev->fb_fd, FBIOBLANK, FB_BLANK_UNBLANK);
        if (ret)
            LOGW("failed to blank display");
    }
}

static int omap4_hwc_set(struct hwc_composer_device *dev, hwc_display_t dpy,
               hwc_surface_t sur, hwc_layer_list_t* list)
{
    omap4_hwc_device_t *hwc_dev = (omap4_hwc_device_t *)dev;
    struct dsscomp_setup_dispc_data *dsscomp = &hwc_dev->dsscomp_data;
    int err = 0;
    unsigned int i;
    int invalidate;

    pthread_mutex_lock(&hwc_dev->lock);

    omap4_hwc_reset_screen(hwc_dev);

    invalidate = hwc_dev->ext_ovls_wanted && !hwc_dev->ext_ovls;

    char big_log[1024];
    int e = sizeof(big_log);
    char *end = big_log + e;
    e -= snprintf(end - e, e, "set H{");
    for (i = 0; list && i < list->numHwLayers; i++) {
        if (i)
            e -= snprintf(end - e, e, " ");
        hwc_layer_t *layer = &list->hwLayers[i];
        IMG_native_handle_t *handle = (IMG_native_handle_t *)layer->handle;
        e -= snprintf(end - e, e, "%p:%s,", handle, layer->compositionType == HWC_OVERLAY ? "DSS" : "SGX");
        if ((layer->flags & HWC_SKIP_LAYER) || !handle) {
            e -= snprintf(end - e, e, "SKIP");
            continue;
        }
        if (layer->flags & HWC_HINT_CLEAR_FB)
            e -= snprintf(end - e, e, "CLR,");
#define FMT(f) ((f) == HAL_PIXEL_FORMAT_TI_NV12 ? "NV12" : \
                (f) == HAL_PIXEL_FORMAT_BGRX_8888 ? "xRGB32" : \
                (f) == HAL_PIXEL_FORMAT_RGBX_8888 ? "xBGR32" : \
                (f) == HAL_PIXEL_FORMAT_BGRA_8888 ? "ARGB32" : \
                (f) == HAL_PIXEL_FORMAT_RGBA_8888 ? "ABGR32" : \
                (f) == HAL_PIXEL_FORMAT_RGB_565 ? "RGB565" : "??")
        e -= snprintf(end - e, e, "%d*%d(%s)", handle->iWidth, handle->iHeight, FMT(handle->iFormat));
        if (layer->transform)
            e -= snprintf(end - e, e, "~%d", layer->transform);
#undef FMT
    }
    e -= snprintf(end - e, e, "} D{");
    for (i = 0; i < dsscomp->num_ovls; i++) {
        if (i)
            e -= snprintf(end - e, e, " ");
        e -= snprintf(end - e, e, "%d=", dsscomp->ovls[i].cfg.ix);
#define FMT(f) ((f) == OMAP_DSS_COLOR_NV12 ? "NV12" : \
                (f) == OMAP_DSS_COLOR_RGB24U ? "xRGB32" : \
                (f) == OMAP_DSS_COLOR_ARGB32 ? "ARGB32" : \
                (f) == OMAP_DSS_COLOR_RGB16 ? "RGB565" : "??")
        if (dsscomp->ovls[i].cfg.enabled)
            e -= snprintf(end - e, e, "%08x:%d*%d,%s",
                          dsscomp->ovls[i].ba,
                          dsscomp->ovls[i].cfg.width,
                          dsscomp->ovls[i].cfg.height,
                          FMT(dsscomp->ovls[i].cfg.color_mode));
#undef FMT
        else
            e -= snprintf(end - e, e, "-");
    }
    e -= snprintf(end - e, e, "} L{");
    for (i = 0; i < hwc_dev->post2_layers; i++) {
        if (i)
            e -= snprintf(end - e, e, " ");
        e -= snprintf(end - e, e, "%p", hwc_dev->buffers[i]);
    }
    e -= snprintf(end - e, e, "}%s\n", hwc_dev->use_sgx ? " swap" : "");
    if (debug) {
        LOGD("%s", big_log);
    }

    // LOGD("set %d layers (sgx=%d)\n", dsscomp->num_ovls, hwc_dev->use_sgx);

    if (dpy && sur) {
        // list can be NULL which means hwc is temporarily disabled.
        // however, if dpy and sur are null it means we're turning the
        // screen off. no shall not call eglSwapBuffers() in that case.

        if (hwc_dev->use_sgx) {
            if (!eglSwapBuffers((EGLDisplay)dpy, (EGLSurface)sur)) {
                LOGE("eglSwapBuffers error");
                err = HWC_EGL_ERROR;
                goto err_out;
            }
        }

        //dump_dsscomp(dsscomp);

        err = hwc_dev->fb_dev->Post2((framebuffer_device_t *)hwc_dev->fb_dev,
                                 hwc_dev->buffers,
                                 hwc_dev->post2_layers,
                                 dsscomp, sizeof(*dsscomp));

        if (!hwc_dev->use_sgx) {
            __u32 crt = 0;
            int err2 = ioctl(hwc_dev->fb_fd, FBIO_WAITFORVSYNC, &crt);
            if (err2) {
                LOGE("failed to wait for vsync (%d)", errno);
                err = err ? : -errno;
            }
        }
    }
    hwc_dev->last_ext_ovls = hwc_dev->ext_ovls;
    hwc_dev->last_int_ovls = hwc_dev->post2_layers;
    if (err)
        LOGE("Post2 error");

err_out:
    pthread_mutex_unlock(&hwc_dev->lock);

    if (invalidate && hwc_dev->procs && hwc_dev->procs->invalidate)
        hwc_dev->procs->invalidate(hwc_dev->procs);

    return err;
}

static int dump_printf(char *buff, int buff_len, int len, const char *fmt, ...)
{
    va_list ap;

    int print_len;

    va_start(ap, fmt);

    print_len = vsnprintf(buff + len, buff_len - len, fmt, ap);

    va_end(ap);

    return len + print_len;
}

static void omap4_hwc_dump(struct hwc_composer_device *dev, char *buff, int buff_len)
{
    omap4_hwc_device_t *hwc_dev = (omap4_hwc_device_t *)dev;
    struct dsscomp_setup_dispc_data *dsscomp = &hwc_dev->dsscomp_data;
    int len = 0;
    int i;

    len = dump_printf(buff, buff_len, len, "omap4_hwc %d:\n", dsscomp->num_ovls);

    for (i = 0; i < dsscomp->num_ovls; i++) {
        struct dss2_ovl_cfg *cfg = &dsscomp->ovls[i].cfg;

        len = dump_printf(buff, buff_len, len, "  layer %d:\n", i);
        len = dump_printf(buff, buff_len, len, "     enabled: %s\n",
                          cfg->enabled ? "true" : "false");
        len = dump_printf(buff, buff_len, len, "     buff: %p %dx%d stride: %d\n",
                          hwc_dev->buffers[i], cfg->width, cfg->height, cfg->stride);
        len = dump_printf(buff, buff_len, len, "     src: (%d,%d) %dx%d\n",
                          cfg->crop.x, cfg->crop.y, cfg->crop.w, cfg->crop.h);
        len = dump_printf(buff, buff_len, len, "     dst: (%d,%d) %dx%d\n",
                          cfg->win.x, cfg->win.y, cfg->win.w, cfg->win.h);
        len = dump_printf(buff, buff_len, len, "     ix: %d\n", cfg->ix);
        len = dump_printf(buff, buff_len, len, "     zorder: %d\n\n", cfg->zorder);
    }
}


static int omap4_hwc_device_close(hw_device_t* device)
{
    omap4_hwc_device_t *hwc_dev = (omap4_hwc_device_t *) device;;

    if (hwc_dev) {
        if (hwc_dev->dsscomp_fd >= 0)
            close(hwc_dev->dsscomp_fd);
        if (hwc_dev->hdmi_fb_fd >= 0)
            close(hwc_dev->hdmi_fb_fd);
        if (hwc_dev->fb_fd >= 0)
            close(hwc_dev->fb_fd);
        /* pthread will get killed when parent process exits */
        pthread_mutex_destroy(&hwc_dev->lock);
        free(hwc_dev);
    }

    return 0;
}

static int omap4_hwc_open_fb_hal(IMG_framebuffer_device_public_t **fb_dev)
{
    IMG_gralloc_module_public_t *psGrallocModule;
    int err;

    err = hw_get_module(GRALLOC_HARDWARE_MODULE_ID,
                        (const hw_module_t**)&psGrallocModule);
    if(err)
        goto err_out;

    if(strcmp(psGrallocModule->base.common.author,
              "Imagination Technologies"))
    {
        err = -EINVAL;
        goto err_out;
    }

    *fb_dev = psGrallocModule->psFrameBufferDevice;

    return 0;

err_out:
    LOGE("Composer HAL failed to load compatible Graphics HAL");
    return err;
}

static void handle_hotplug(omap4_hwc_device_t *hwc_dev, int state)
{
    pthread_mutex_lock(&hwc_dev->lock);
    hwc_dev->ext = 0;
    if (state) {
        /* check whether we can clone and/or dock */
        char value[PROPERTY_VALUE_MAX];
        property_get("hwc.hdmi.docking.enabled", value, "1");
        hwc_dev->ext |= EXT_DOCK * (atoi(value) > 0);
        property_get("hwc.hdmi.mirroring.enabled", value, "1");
        hwc_dev->ext |= EXT_MIRROR * (atoi(value) > 0);

        /* get cloning transformation */
        property_get("hwc.hdmi.docking.transform", value, "0");
        hwc_dev->ext |= (atoi(value) & EXT_TRANSFORM) << EXT_DOCK_TRANSFORM_SHIFT;
        property_get("hwc.hdmi.mirroring.transform", value, hwc_dev->fb_dev->base.height > hwc_dev->fb_dev->base.width ? "3" : "0");
        hwc_dev->ext |= atoi(value) & EXT_TRANSFORM;

        __u32 xres = (hwc_dev->ext & 1) ? hwc_dev->fb_dev->base.height : hwc_dev->fb_dev->base.width;
        __u32 yres = (hwc_dev->ext & 1) ? hwc_dev->fb_dev->base.width : hwc_dev->fb_dev->base.height;
        int res = omap4_hwc_set_best_hdmi_mode(hwc_dev, xres, yres, 1, 1);
        if (!res)
            ioctl(hwc_dev->hdmi_fb_fd, FBIOBLANK, FB_BLANK_UNBLANK);
        else
            hwc_dev->ext = 0;
    }
    omap4_hwc_create_ext_matrix(hwc_dev);
    LOGI("external display changed (state=%d, mirror={%s tform=%ddeg%s}, dock={%s tform=%ddeg%s}, tv=%d", state,
         (hwc_dev->ext & EXT_MIRROR) ? "enabled" : "disabled",
         (hwc_dev->ext & EXT_ROTATION) * 90,
         (hwc_dev->ext & EXT_HFLIP) ? "+hflip" : "",
         (hwc_dev->ext & EXT_DOCK) ? "enabled" : "disabled",
         ((hwc_dev->ext >> EXT_DOCK_TRANSFORM_SHIFT) & EXT_ROTATION) * 90,
         ((hwc_dev->ext >> EXT_DOCK_TRANSFORM_SHIFT) & EXT_HFLIP) ? "+hflip" : "",
         !!(hwc_dev->ext & EXT_TV));

    hwc_dev->ext_requested = hwc_dev->ext;
    hwc_dev->ext_last = hwc_dev->ext;
    pthread_mutex_unlock(&hwc_dev->lock);

    if (hwc_dev->procs && hwc_dev->procs->invalidate)
            hwc_dev->procs->invalidate(hwc_dev->procs);
}

static void handle_uevents(omap4_hwc_device_t *hwc_dev, const char *s)
{
    if (strcmp(s, "change@/devices/virtual/switch/hdmi"))
        return;

    s += strlen(s) + 1;

    while(*s) {
        if (!strncmp(s, "SWITCH_STATE=", strlen("SWITCH_STATE="))) {
            int state = atoi(s + strlen("SWITCH_STATE="));
            handle_hotplug(hwc_dev, state);
        }

        s += strlen(s) + 1;
    }
}

static void *omap4_hwc_hdmi_thread(void *data)
{
    omap4_hwc_device_t *hwc_dev = data;
    static char uevent_desc[4096];
    uevent_init();

    memset(uevent_desc, 0, sizeof(uevent_desc));

    do {
        /* keep last 2 zeroes to ensure double 0 termination */
        uevent_next_event(uevent_desc, sizeof(uevent_desc) - 2);
        handle_uevents(hwc_dev, uevent_desc);
    } while (1);

    return NULL;
}

static void omap4_hwc_registerProcs(struct hwc_composer_device* dev,
                                    hwc_procs_t const* procs)
{
        omap4_hwc_device_t *hwc_dev = (omap4_hwc_device_t *) dev;

        hwc_dev->procs = (typeof(hwc_dev->procs)) procs;
}

static int omap4_hwc_device_open(const hw_module_t* module, const char* name,
                hw_device_t** device)
{
    omap4_hwc_module_t *hwc_mod = (omap4_hwc_module_t *)module;
    omap4_hwc_device_t *hwc_dev;
    int err = 0;

    if (strcmp(name, HWC_HARDWARE_COMPOSER)) {
        return -EINVAL;
    }

    if (!hwc_mod->fb_dev) {
        err = omap4_hwc_open_fb_hal(&hwc_mod->fb_dev);
        if (err)
            return err;

        if (!hwc_mod->fb_dev) {
            LOGE("Framebuffer HAL not opened before HWC");
            return -EFAULT;
        }
        hwc_mod->fb_dev->bBypassPost = 1;
    }

    hwc_dev = (omap4_hwc_device_t *)malloc(sizeof(*hwc_dev));
    if (hwc_dev == NULL)
        return -ENOMEM;

    memset(hwc_dev, 0, sizeof(*hwc_dev));

    hwc_dev->base.common.tag = HARDWARE_DEVICE_TAG;
    hwc_dev->base.common.version = HWC_API_VERSION;
    hwc_dev->base.common.module = (hw_module_t *)module;
    hwc_dev->base.common.close = omap4_hwc_device_close;
    hwc_dev->base.prepare = omap4_hwc_prepare;
    hwc_dev->base.set = omap4_hwc_set;
    hwc_dev->base.dump = omap4_hwc_dump;
    hwc_dev->base.registerProcs = omap4_hwc_registerProcs;
    hwc_dev->fb_dev = hwc_mod->fb_dev;
    *device = &hwc_dev->base.common;

    hwc_dev->dsscomp_fd = open("/dev/dsscomp", O_RDWR);
    if (hwc_dev->dsscomp_fd < 0) {
        LOGE("failed to open dsscomp (%d)", errno);
        err = -errno;
        goto done;
    }

    hwc_dev->hdmi_fb_fd = open("/dev/graphics/fb1", O_RDWR);
    if (hwc_dev->hdmi_fb_fd < 0) {
        LOGE("failed to open hdmi fb (%d)", errno);
        err = -errno;
        goto done;
    }

    hwc_dev->fb_fd = open("/dev/graphics/fb0", O_RDWR);
    if (hwc_dev->fb_fd < 0) {
        LOGE("failed to open fb (%d)", errno);
        err = -errno;
        goto done;
    }

    hwc_dev->buffers = malloc(sizeof(buffer_handle_t) * MAX_HW_OVERLAYS);
    if (!hwc_dev->buffers) {
        err = -ENOMEM;
        goto done;
    }

    int ret = ioctl(hwc_dev->dsscomp_fd, DSSCOMP_QUERY_DISPLAY, &hwc_dev->fb_dis);
    if (ret) {
        LOGE("failed to get display info (%d): %m", errno);
        err = -errno;
        goto done;
    }

    if (pthread_mutex_init(&hwc_dev->lock, NULL)) {
            LOGE("failed to create mutex (%d): %m", errno);
            err = -errno;
            goto done;
    }
    if (pthread_create(&hwc_dev->hdmi_thread, NULL, omap4_hwc_hdmi_thread, hwc_dev))
    {
            LOGE("failed to create HDMI listening thread (%d): %m", errno);
            err = -errno;
            goto done;
    }

    /* get debug properties */

    /* see if hwc is enabled at all */
    char value[PROPERTY_VALUE_MAX];
    property_get("debug.hwc.rgb_order", value, "1");
    hwc_dev->flags_rgb_order = atoi(value);
    property_get("debug.hwc.nv12_only", value, "0");
    hwc_dev->flags_nv12_only = atoi(value);

    /* read switch state */
    int sw_fd = open("/sys/class/switch/hdmi/state", O_RDONLY);
    int hpd = 0;
    if (sw_fd >= 0) {
        char value;
        if (read(sw_fd, &value, 1) == 1)
            hpd = value == '1';
        close(sw_fd);
    }
    handle_hotplug(hwc_dev, hpd);

    LOGE("omap4_hwc_device_open(rgb_order=%d nv12_only=%d)",
        hwc_dev->flags_rgb_order, hwc_dev->flags_nv12_only);

done:
    if (err && hwc_dev) {
        if (hwc_dev->dsscomp_fd >= 0)
            close(hwc_dev->dsscomp_fd);
        if (hwc_dev->hdmi_fb_fd >= 0)
            close(hwc_dev->hdmi_fb_fd);
        if (hwc_dev->fb_fd >= 0)
            close(hwc_dev->fb_fd);
        pthread_mutex_destroy(&hwc_dev->lock);
        free(hwc_dev->buffers);
        free(hwc_dev);
    }

    return err;
}

static struct hw_module_methods_t omap4_hwc_module_methods = {
    .open = omap4_hwc_device_open,
};

omap4_hwc_module_t HAL_MODULE_INFO_SYM = {
    .base = {
        .common = {
            .tag =                  HARDWARE_MODULE_TAG,
            .version_major =        1,
            .version_minor =        0,
            .id =                   HWC_HARDWARE_MODULE_ID,
            .name =                 "OMAP 44xx Hardware Composer HAL",
            .author =               "Texas Instruments",
            .methods =              &omap4_hwc_module_methods,
        },
    },
};
