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

#include <cutils/properties.h>
#include <cutils/log.h>
#include <cutils/native_handle.h>
#include <hardware/hardware.h>
#include <hardware/hwcomposer.h>
#include <EGL/egl.h>
#include <utils/Timers.h>

#include <video/dsscomp.h>

#include "hal_public.h"

#define MAX_HW_OVERLAYS 4
#define MAX_SCALING_OVERLAYS 3
#define HAL_PIXEL_FORMAT_BGRX_8888		0x1FF
#define HAL_PIXEL_FORMAT_TI_NV12 0x100
#define HAL_PIXEL_FORMAT_TI_NV12_PADDED 0x101
#define MAX_TILER_SLOT (4 << 20)

#define MIN(a,b)		  ((a)<(b)?(a):(b))
#define MAX(a,b)		  ((a)>(b)?(a):(b))
#define CLAMP(x,low,high) (((x)>(high))?(high):(((x)<(low))?(low):(x)))

struct omap4_hwc_module {
    hwc_module_t base;

    IMG_framebuffer_device_public_t *fb_dev;
};
typedef struct omap4_hwc_module omap4_hwc_module_t;

struct omap4_hwc_device {
    hwc_composer_device_t base;

    IMG_framebuffer_device_public_t *fb_dev;
    struct dsscomp_setup_dispc_data dsscomp_data;

    buffer_handle_t *buffers;
    int use_sgx;

    int flags_rgb_order;
    int flags_nv12_only;
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
    struct dss2_mgr_info *mi = &d->mgr;

    LOGE("[%08x] set: %c%c%c(dis%d alpha=%d col=%08x ilace=%d n=%d)\n",
         d->sync_id,
         (d->mode & DSSCOMP_SETUP_MODE_APPLY) ? 'A' : '-',
         (d->mode & DSSCOMP_SETUP_MODE_DISPLAY) ? 'D' : '-',
         (d->mode & DSSCOMP_SETUP_MODE_CAPTURE) ? 'C' : '-',
         mi->ix,
         mi->alpha_blending, mi->default_color,
         mi->interlaced,
         d->num_ovls);

    unsigned i;
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

static int omap4_hwc_prepare(struct hwc_composer_device *dev, hwc_layer_list_t* list)
{
    omap4_hwc_device_t *hwc_dev = (omap4_hwc_device_t *)dev;
    struct dsscomp_setup_dispc_data *dsscomp = &hwc_dev->dsscomp_data;
    unsigned int num_possible_overlay_layers = 0;
    unsigned int num_composited_layers = 0;
    unsigned int num_scaled_layers = 0;
    unsigned int i;
    unsigned int num_RGB = 0;
    unsigned int num_BGR = 0;

    memset(dsscomp, 0x0, sizeof(*dsscomp));
    dsscomp->sync_id = sync_id++;

    /* Figure out how many layers we can support via DSS */
    num_composited_layers = list->numHwLayers;

    for (i = 0; i < list->numHwLayers; i++) {
        hwc_layer_t *layer = &list->hwLayers[i];
        IMG_native_handle_t *handle = (IMG_native_handle_t *)layer->handle;

        layer->compositionType = HWC_FRAMEBUFFER;

        if (omap4_hwc_is_valid_layer(layer, handle)) {
            num_possible_overlay_layers++;

            if (scaled(layer))
                num_scaled_layers++;

            if (is_BGR(handle->iFormat))
                num_BGR++;
            else if (is_RGB(handle->iFormat))
                num_RGB++;
        }
    }

    /* phase 2 logic: all DSS rendering */
    if (num_possible_overlay_layers <= MAX_HW_OVERLAYS &&
        num_possible_overlay_layers == num_composited_layers &&
        num_scaled_layers <= MAX_SCALING_OVERLAYS &&
        (num_BGR == 0 || num_RGB == 0 || !hwc_dev->flags_rgb_order) &&
        num_possible_overlay_layers) {
        /* All layers can be handled by the DSS -- don't use SGX for composition */
        hwc_dev->use_sgx = 0;
        dsscomp->mgr.swap_rb = num_BGR != 0;
    } else {
        /* Use SGX for composition plus first 3 layers that are DSS renderable */
        hwc_dev->use_sgx = 1;
        dsscomp->mgr.swap_rb = is_BGR(hwc_dev->fb_dev->base.format);
    }
    if (debug) {
        LOGD("prepare (%d) %d layers - %s (poss=%d/%d scaled, comp=%d, rgb=%d,bgr=%d)\n",
         dsscomp->sync_id, list->numHwLayers,
         hwc_dev->use_sgx ? "SGX+OVL" : "all-OVL",
         num_possible_overlay_layers, num_scaled_layers, num_composited_layers, num_RGB, num_BGR);
    }

    /* setup pipes */
    dsscomp->num_ovls = hwc_dev->use_sgx;
    int z = 0;
    int fb_z = -1, fb_zmax = 0;
    int scaled_gfx = 0;

    /* set up if DSS layers */
    for (i = 0; i < list->numHwLayers && dsscomp->num_ovls < MAX_HW_OVERLAYS; i++) {
        hwc_layer_t *layer = &list->hwLayers[i];
        IMG_native_handle_t *handle = (IMG_native_handle_t *)layer->handle;

        if (handle &&
            omap4_hwc_is_valid_layer(layer, handle) &&
            /* can't have a transparent overlay in the middle of the framebuffer stack */
            !(is_ALPHA(handle->iFormat) && fb_z >= 0) &&
            /* skip non-NV12 layers if also using SGX (if nv12_only flag is set) */
            (!hwc_dev->flags_nv12_only || (!hwc_dev->use_sgx || is_NV12(handle->iFormat))) &&
            /* make sure RGB ordering is consistent (if rgb_order flag is set) */
            (!(dsscomp->mgr.swap_rb ? is_RGB(handle->iFormat) : is_BGR(handle->iFormat)) ||
             !hwc_dev->flags_rgb_order)) {
            /* render via DSS overlay */
            layer->compositionType = HWC_OVERLAY;
            /* clear layers above DSS layer areas */
            layer->hints |= HWC_HINT_CLEAR_FB;
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

    if (z != dsscomp->num_ovls || dsscomp->num_ovls > MAX_HW_OVERLAYS)
        LOGE("**** used %d z-layers for %d overlays\n", z, dsscomp->num_ovls);

    if (hwc_dev->use_sgx) {
        hwc_dev->buffers[0] = NULL;
        omap4_hwc_setup_layer_base(&dsscomp->ovls[0].cfg, fb_z,
                                   hwc_dev->fb_dev->base.format,
                                   hwc_dev->fb_dev->base.width,
                                   hwc_dev->fb_dev->base.height);
        dsscomp->ovls[0].uv = (__u32) hwc_dev->buffers[0];
    }

    dsscomp->mode = DSSCOMP_SETUP_DISPLAY;
    dsscomp->mgr.ix = 0;
    dsscomp->mgr.alpha_blending = 1;

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
    for (i = 0; i < dsscomp->num_ovls; i++) {
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
                                 dsscomp->num_ovls,
                                 dsscomp, sizeof(*dsscomp));
    if (err)
        LOGE("Post2 error");

err_out:
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

    if (hwc_dev)
        free(hwc_dev);

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

static int omap4_hwc_device_open(const hw_module_t* module, const char* name,
                hw_device_t** device)
{
    omap4_hwc_module_t *hwc_mod = (omap4_hwc_module_t *)module;
    omap4_hwc_device_t *hwc_dev;
    int err;

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
    hwc_dev->fb_dev = hwc_mod->fb_dev;
    *device = &hwc_dev->base.common;

    hwc_dev->buffers = malloc(sizeof(buffer_handle_t) * MAX_HW_OVERLAYS);
    if (!hwc_dev->buffers) {
        free(hwc_dev);
        return -ENOMEM;
    }

    /* get debug properties */

    /* see if hwc is enabled at all */
    char value[PROPERTY_VALUE_MAX];
    property_get("debug.hwc.rgb_order", value, "1");
    hwc_dev->flags_rgb_order = atoi(value);
    property_get("debug.hwc.nv12_only", value, "0");
    hwc_dev->flags_nv12_only = atoi(value);
    LOGE("omap4_hwc_device_open(rgb_order=%d nv12_only=%d)",
        hwc_dev->flags_rgb_order, hwc_dev->flags_nv12_only);

    return 0;
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
