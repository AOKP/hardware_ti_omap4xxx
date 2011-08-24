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

#define min(a, b) ( { typeof(a) __a = (a), __b = (b); __a < __b ? __a : __b; } )
#define max(a, b) ( { typeof(a) __a = (a), __b = (b); __a > __b ? __a : __b; } )

#include <video/dsscomp.h>

#include "hal_public.h"

#define MAX_HW_OVERLAYS 4
#define NUM_NONSCALING_OVERLAYS 1
#define HAL_PIXEL_FORMAT_BGRX_8888		0x1FF
#define HAL_PIXEL_FORMAT_TI_NV12 0x100
#define HAL_PIXEL_FORMAT_TI_NV12_PADDED 0x101
#define MAX_TILER_SLOT (4 << 20)

#define MIN(a,b)		  ((a)<(b)?(a):(b))
#define MAX(a,b)		  ((a)>(b)?(a):(b))
#define CLAMP(x,low,high) (((x)>(high))?(high):(((x)<(low))?(low):(x)))

enum {
    EXT_ROTATION    = 3,        /* rotation while mirroring */
    EXT_HFLIP       = (1 << 2), /* flip l-r on output (after rotation) */
    EXT_TRANSFORM   = EXT_ROTATION | EXT_HFLIP,
    EXT_ON          = (1 << 3), /* copy output to other display */
    EXT_DOCK        = (1 << 4), /* docking only */
    EXT_TV          = (1 << 5), /* using TV for mirroring */
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
    int hdmi_fb_fd;

    IMG_framebuffer_device_public_t *fb_dev;
    struct dsscomp_setup_dispc_data dsscomp_data;

    buffer_handle_t *buffers;
    int use_sgx;
    int swap_rb;
    int use_tv;
    int mirror;
    unsigned int post2_layers;
    int last_ext_ovls;
    int last_int_ovls;
    int ext_ovls;

    int flags_rgb_order;
    int flags_nv12_only;
    int ext;
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
        LOGI("invalid format %d", format);
        return 0;
    }
}

static int scaled(hwc_layer_t *layer)
{
    int w = layer->sourceCrop.right - layer->sourceCrop.left;
    int h = layer->sourceCrop.bottom - layer->sourceCrop.top;
    /* TODO: skip w/h if rotated by 90 */
    return (layer->displayFrame.right - layer->displayFrame.left != w ||
            layer->displayFrame.bottom - layer->displayFrame.top != h);
}

static int sync_id = 0;

#define is_ALPHA(format) ((format) == HAL_PIXEL_FORMAT_BGRA_8888 || (format) == HAL_PIXEL_FORMAT_RGBA_8888)
#define is_RGB(format) ((format) == HAL_PIXEL_FORMAT_BGRA_8888 || (format) == HAL_PIXEL_FORMAT_RGB_565 || (format) == HAL_PIXEL_FORMAT_BGRX_8888)
#define is_BGR(format) ((format) == HAL_PIXEL_FORMAT_RGBX_8888 || (format) == HAL_PIXEL_FORMAT_RGBA_8888)
#define is_NV12(format) ((format) == HAL_PIXEL_FORMAT_TI_NV12 || (format) == HAL_PIXEL_FORMAT_TI_NV12_PADDED)

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

static int omap4_hwc_is_valid_layer(hwc_layer_t *layer,
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
        int bpp = (handle->iFormat == HAL_PIXEL_FORMAT_RGB_565 ? 2 : 4);
        int stride = ALIGN(handle->iWidth, HW_ALIGN) * bpp;
        if (stride * handle->iHeight > MAX_TILER_SLOT)
            return 0;
    }
    return 1;
}

struct counts {
    unsigned int possible_overlay_layers;
    unsigned int composited_layers;
    unsigned int scaled_layers;
    unsigned int RGB;
    unsigned int BGR;
    unsigned int NV12;
    unsigned int displays;
    unsigned int max_hw_overlays;
    unsigned int max_scaling_overlays;
};

static inline int can_dss_render_all(omap4_hwc_device_t *hwc_dev, struct counts *num)
{
    int tv = hwc_dev->ext & EXT_TV;
    int nonscaling_ovls = NUM_NONSCALING_OVERLAYS;
    int tform = hwc_dev->ext & EXT_TRANSFORM;
    num->max_hw_overlays = MAX_HW_OVERLAYS;

    /*
     * We cannot atomically switch overlays from one display to another.  First, they
     * have to be disabled, and the disabling has to take effect on the current display.
     * We keep track of the available number of overlays here.
     */
    if (hwc_dev->ext & EXT_DOCK) {
        /* some overlays may already be used by the external display, so we account for this */

        /* reserve just a video pipeline for HDMI if docking */
        hwc_dev->ext_ovls = num->NV12 ? 1 : 0;
        num->max_hw_overlays -= max(hwc_dev->ext_ovls, hwc_dev->last_ext_ovls);
    } else if (hwc_dev->ext) {
        /*
         * otherwise, manage just from half the pipelines.  NOTE: there is
         * no danger of having used too many overlays for external display here.
         */
        num->max_hw_overlays >>= 1;
        nonscaling_ovls >>= 1;
        hwc_dev->ext_ovls = MAX_HW_OVERLAYS - num->max_hw_overlays;
    } else {
        num->max_hw_overlays -= hwc_dev->last_ext_ovls;
        hwc_dev->ext_ovls = 0;
    }

    /*
     * :TRICKY: We may not have enough overlays on the external display.  We "reserve" them
     * here to figure out if mirroring is supported, but may not do mirroring for the first
     * frame while the overlays required for it are cleared.
     */
    hwc_dev->ext_ovls = min(MAX_HW_OVERLAYS - hwc_dev->last_int_ovls, hwc_dev->ext_ovls);

    /* if not docking, we may be limited by last used external overlays */
    if (hwc_dev->ext_ovls && hwc_dev->ext && !(hwc_dev->ext & EXT_DOCK))
         num->max_hw_overlays = hwc_dev->ext_ovls;

    num->max_scaling_overlays = num->max_hw_overlays - nonscaling_ovls;

    return  /* must have at least one layer if using composition bypass to get sync object */
            num->possible_overlay_layers &&
            num->possible_overlay_layers <= num->max_hw_overlays &&
            num->possible_overlay_layers == num->composited_layers &&
            num->scaled_layers <= num->max_scaling_overlays &&
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

    return omap4_hwc_is_valid_layer(layer, handle) &&
           /* skip non-NV12 layers if also using SGX (if nv12_only flag is set) */
           (!hwc_dev->flags_nv12_only || (!hwc_dev->use_sgx || is_NV12(handle->iFormat))) &&
           /* make sure RGB ordering is consistent (if rgb_order flag is set) */
           (!(hwc_dev->swap_rb ? is_RGB(handle->iFormat) : is_BGR(handle->iFormat)) ||
            !hwc_dev->flags_rgb_order) &&
           /* TV can only render RGB */
           !(tv && is_BGR(handle->iFormat)) &&
           /* we can only rotate TILER2D buffers for external displays */
           !(tform && !is_NV12(handle->iFormat));
}

static int omap4_hwc_prepare(struct hwc_composer_device *dev, hwc_layer_list_t* list)
{
    omap4_hwc_device_t *hwc_dev = (omap4_hwc_device_t *)dev;
    struct dsscomp_setup_dispc_data *dsscomp = &hwc_dev->dsscomp_data;
    struct counts num = { .composited_layers = list->numHwLayers };
    unsigned int i;

    pthread_mutex_lock(&hwc_dev->lock);
    memset(dsscomp, 0x0, sizeof(*dsscomp));
    dsscomp->sync_id = sync_id++;

    /* Figure out how many layers we can support via DSS */
    for (i = 0; i < list->numHwLayers; i++) {
        hwc_layer_t *layer = &list->hwLayers[i];
        IMG_native_handle_t *handle = (IMG_native_handle_t *)layer->handle;

        layer->compositionType = HWC_FRAMEBUFFER;

        if (omap4_hwc_is_valid_layer(layer, handle)) {
            num.possible_overlay_layers++;

            if (scaled(layer))
                num.scaled_layers++;

            if (is_BGR(handle->iFormat))
                num.BGR++;
            else if (is_RGB(handle->iFormat))
                num.RGB++;
            else if (is_NV12(handle->iFormat))
                num.NV12++;
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
        LOGD("prepare (%d) %d layers - %s (comp=%d, poss=%d/%d scaled, RGB=%d,BGR=%d,NV12=%d) (ext=%x, %dex/%dmx (last %dex,%din)\n",
         dsscomp->sync_id, list->numHwLayers,
         hwc_dev->use_sgx ? "SGX+OVL" : "all-OVL",
         num.composited_layers,
         num.possible_overlay_layers, num.scaled_layers,
         num.RGB, num.BGR, num.NV12,
         hwc_dev->ext, hwc_dev->ext_ovls, num.max_hw_overlays, hwc_dev->last_ext_ovls, hwc_dev->last_int_ovls);
    }

    /* setup pipes */
    dsscomp->num_ovls = hwc_dev->use_sgx;
    int z = 0;
    int fb_z = -1, fb_zmax = 0;
    int scaled_gfx = 0;
    int ix_nv12 = -1;

    /* set up if DSS layers */
    for (i = 0; i < list->numHwLayers && dsscomp->num_ovls < num.max_hw_overlays; i++) {
        hwc_layer_t *layer = &list->hwLayers[i];
        IMG_native_handle_t *handle = (IMG_native_handle_t *)layer->handle;

        if (can_dss_render_layer(hwc_dev, layer) &&
            /* can't have a transparent overlay in the middle of the framebuffer stack */
            !(is_ALPHA(handle->iFormat) && fb_z >= 0)) {
            /* render via DSS overlay */
            layer->compositionType = HWC_OVERLAY;
            hwc_dev->buffers[dsscomp->num_ovls] = layer->handle;

            omap4_hwc_setup_layer(hwc_dev,
                                  &dsscomp->ovls[dsscomp->num_ovls],
                                  &list->hwLayers[i],
                                  z,
                                  handle->iFormat,
                                  handle->iWidth,
                                  handle->iHeight);
            /* TODO: we need to use gfx for the a layer that is not scaled */
            dsscomp->ovls[dsscomp->num_ovls].cfg.ix = dsscomp->num_ovls;
            /* just marking dss layers */
            dsscomp->ovls[dsscomp->num_ovls].address = (void *) (dsscomp->num_ovls * 4096 + 0xA0000000);
            dsscomp->ovls[dsscomp->num_ovls].uv = (__u32) hwc_dev->buffers[dsscomp->num_ovls];

            /* ensure GFX layer is never scaled */
            if (dsscomp->num_ovls == 0) {
                scaled_gfx = scaled(layer);
            } else if (scaled_gfx && !scaled(layer)) {
                /* swap GFX layer with this one */
                dsscomp->ovls[dsscomp->num_ovls].cfg.ix = 0;
                dsscomp->ovls[0].cfg.ix = dsscomp->num_ovls;
                scaled_gfx = 0;
            }

            /* remember first NV12 layer */
            if (is_NV12(handle->iFormat) && ix_nv12 < 0)
                ix_nv12 = dsscomp->num_ovls;

            dsscomp->num_ovls++;
            z++;
        } else if (hwc_dev->use_sgx) {
            if (fb_z < 0) {
                /* NOTE: we are not handling transparent cutout for now */
                fb_z = fb_zmax = z;
                z++;
            } else {
                /* move fb z-order up (by lowering dss layers) */
                while (fb_zmax < z - 1)
                    dsscomp->ovls[1 + fb_zmax++].cfg.zorder--;
            }
        }
    }

    /* clear FB above DSS layers */
    /* FIXME: for now assume full screen layers.  later this can be optimized */
    int obstructed = 0;
    if (dsscomp->num_ovls > hwc_dev->use_sgx) {
        for (i = list->numHwLayers; i > 0;) {
            i--;
            hwc_layer_t *layer = &list->hwLayers[i];
            if ((layer->flags & HWC_SKIP_LAYER) || !layer->handle)
                continue;
            if (layer->compositionType != HWC_OVERLAY)
                obstructed = 1;
            else if (obstructed)
                layer->hints |= HWC_HINT_CLEAR_FB;
        }
    }

    /* if scaling GFX (e.g. only 1 scaled surface) use a VID pipe */
    if (scaled_gfx)
        dsscomp->ovls[0].cfg.ix = 1;

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
        dsscomp->ovls[0].uv = (__u32) hwc_dev->buffers[0];
    }

    /* mirror layers */
    hwc_dev->post2_layers = dsscomp->num_ovls;
    if (hwc_dev->ext && hwc_dev->ext_ovls) {
        int ix_back, ix_front, ix;
        if (hwc_dev->ext & EXT_DOCK) {
            /* mirror only NV12 layer */
            ix_back = ix_front = ix_nv12;
        } else {
            /* mirror all layers */
            ix_back = 0;
            ix_front = dsscomp->num_ovls - 1;
        }

        for (ix = ix_back; ix >= 0 && ix <= ix_front; ix++) {
            memcpy(dsscomp->ovls + dsscomp->num_ovls, dsscomp->ovls + ix, sizeof(dsscomp->ovls[ix]));
            dsscomp->ovls[dsscomp->num_ovls].cfg.zorder += hwc_dev->post2_layers;
            /* reserve overlays at end for other display */
            dsscomp->ovls[dsscomp->num_ovls].cfg.ix = MAX_HW_OVERLAYS - 1 - (ix - ix_back);
            dsscomp->ovls[dsscomp->num_ovls].cfg.mgr_ix = 1;
            dsscomp->ovls[dsscomp->num_ovls].ba = ix;
            dsscomp->num_ovls++;
            z++;
        }
    }

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

static int omap4_hwc_set(struct hwc_composer_device *dev, hwc_display_t dpy,
               hwc_surface_t sur, hwc_layer_list_t* list)
{
    omap4_hwc_device_t *hwc_dev = (omap4_hwc_device_t *)dev;
    struct dsscomp_setup_dispc_data *dsscomp = &hwc_dev->dsscomp_data;
    int err;
    unsigned int i;

    if (!list) {
        LOGE("list is NULL");
        return 0;
    }

    pthread_mutex_lock(&hwc_dev->lock);
    char big_log[1024];
    int e = sizeof(big_log);
    char *end = big_log + e;
    e -= snprintf(end - e, e, "set H{");
    for (i = 0; i < list->numHwLayers; i++) {
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

    hwc_dev->last_ext_ovls = hwc_dev->ext_ovls;
    hwc_dev->last_int_ovls = hwc_dev->post2_layers;
    if (err)
        LOGE("Post2 error");

err_out:
    pthread_mutex_unlock(&hwc_dev->lock);
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
        len = dump_printf(buff, buff_len, len, "     buff: %dx%d stride: %d:\n",
                          cfg->width, cfg->height, cfg->stride);
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
        struct dsscomp_display_info dis = { .ix = 1,  };
        int ret = ioctl(hwc_dev->dsscomp_fd, DSSCOMP_QUERY_DISPLAY, &dis);
        if (!ret) {
            hwc_dev->ext = EXT_ON | 3;
            if (dis.channel == OMAP_DSS_CHANNEL_DIGIT)
                hwc_dev->ext |= EXT_TV;
            ioctl(hwc_dev->hdmi_fb_fd, FBIOBLANK, FB_BLANK_UNBLANK);
        }

        /* FIXME set up hwc_dev->ext based on mirroring needs */
        char value[PROPERTY_VALUE_MAX];
        property_get("debug.hwc.ext", value, "0");
        hwc_dev->ext |= atoi(value) & EXT_DOCK;
    }
    LOGI("external display changed (state=%d, on=%d, dock=%d, tv=%d, trform=%ddeg%s)", state,
         !!hwc_dev->ext, !!(hwc_dev->ext & EXT_DOCK),
         !!(hwc_dev->ext & EXT_TV), hwc_dev->ext & EXT_ROTATION,
         (hwc_dev->ext & EXT_HFLIP) ? "+hflip" : "");
    pthread_mutex_unlock(&hwc_dev->lock);

    if (hwc_dev->procs && hwc_dev->procs->invalidate) {
            hwc_dev->procs->invalidate(hwc_dev->procs);
            usleep(30000);
            hwc_dev->procs->invalidate(hwc_dev->procs);
    }
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

    hwc_dev->hdmi_fb_fd = open("/dev/graphics/fb1", O_RDWR);

    hwc_dev->buffers = malloc(sizeof(buffer_handle_t) * MAX_HW_OVERLAYS);
    if (!hwc_dev->buffers) {
        err = -ENOMEM;
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
