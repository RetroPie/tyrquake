/*
Copyright (C) 2019 Kevin Shanahan

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/

#include <stdint.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

#include <X11/extensions/xf86vmode.h>

#include "qtypes.h"
#include "sys.h"
#include "vid.h"
#include "x11_core.h"
#include "zone.h"

#ifdef GLQUAKE
#include "glquake.h"
#else
#include "r_shared.h"
#endif

XVisualInfo *x_visinfo;

// The icon header is generated by ImageMagick
#define MagickImage tyrquake_icon_128
#include "tyrquake_icon_128.h"
#undef MagickImage

typedef union {
    uint32_t bgra;
    struct {
        byte blue;
        byte green;
        byte red;
        byte alpha;
    } c;
} bgra_pixel_t;

/*
 * Set the window title and icon
 */
void
VID_X11_SetIcon()
{
#ifndef DISABLE_ICON
    bgra_pixel_t pixel;
    int i, mark, iconsize;
    long *icondata, *dst;
    const byte *src;
    Atom property;

    mark = Hunk_LowMark();

    iconsize = 128 * 128;
    icondata = Hunk_AllocName((iconsize + 2) * sizeof(long), "icondata");

    src = tyrquake_icon_128;
    dst = icondata;
    *dst++ = 128; // width
    *dst++ = 128; // height

    for (i = 0; i < iconsize; i++) {
        pixel.c.red = *src++;
        pixel.c.green = *src++;
        pixel.c.blue = *src++;
        pixel.c.alpha = *src++;
        *dst++ = pixel.bgra;
    }

    property = XInternAtom(x_disp, "_NET_WM_ICON", 0);
    XChangeProperty(x_disp, x_win, property, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)icondata, iconsize + 2);

    Hunk_FreeToLowMark(mark);
#endif
}

void
VID_X11_SetWindowName(const char *name)
{
    char *temp_name;
    XTextProperty text_property;
    XClassHint *class_hint;

    temp_name = Z_StrDup(mainzone, name);

    /* Set name and icon name */
    XmbTextListToTextProperty(x_disp, &temp_name, 1, XTextStyle, &text_property);
    XSetWMName(x_disp, x_win, &text_property);
    XSetWMIconName(x_disp, x_win, &text_property);

    /* Set the class hint - Window manager uses this for alt-tab list */
    class_hint = XAllocClassHint();
    class_hint->res_name = temp_name;
    class_hint->res_class = temp_name;
    XSetClassHint(x_disp, x_win, class_hint);
    XFree(class_hint);

    Z_Free(mainzone, temp_name);
}

void
VID_InitModeList(void)
{
    XF86VidModeModeInfo **xmodes, *xmode;
    qvidmode_t *mode;
    int i, numxmodes;

    /* Init a default windowed mode */
    mode = &vid_windowed_mode;
    mode->width = 640;
    mode->height = 480;
    mode->bpp = x_visinfo->depth;
    mode->refresh = 0;
    mode->min_scale = 1;
    mode->resolution.scale = 1;
    mode->resolution.width = mode->width;
    mode->resolution.height = mode->height;

    XF86VidModeGetAllModeLines(x_disp, x_visinfo->screen, &numxmodes, &xmodes);

    /* Count the valid modes, then allocate space to store them */
    vid_nummodes = 0;
    for (xmode = *xmodes, i = 0; i < numxmodes; i++, xmode++) {
        int scale = 1;
        while (scale < VID_MAX_SCALE) {
            if (xmode->hdisplay / scale <= MAXWIDTH && xmode->vdisplay / scale <= MAXHEIGHT)
                break;
            scale <<= 1;
        }
        if (scale <= VID_MAX_SCALE)
            vid_nummodes++;
    }
    vid_modelist = Hunk_HighAllocName(vid_nummodes * sizeof(qvidmode_t), "vidmodes");
    vid_nummodes = 0;

    /* Init the mode list */
    mode = vid_modelist;
    for (xmode = *xmodes, i = 0; i < numxmodes; i++, xmode++) {

        int scale = 1;
        while (scale < VID_MAX_SCALE) {
            if (xmode->hdisplay / scale <= MAXWIDTH && xmode->vdisplay / scale <= MAXHEIGHT)
                break;
            scale <<= 1;
        }
        if (scale > VID_MAX_SCALE)
            continue;

	mode->width = xmode->hdisplay;
	mode->height = xmode->vdisplay;
        mode->min_scale = scale;
        mode->resolution.scale = scale;
        mode->resolution.width = mode->width / scale;
        mode->resolution.height = mode->height / scale;
	mode->bpp = x_visinfo->depth;
	mode->refresh = 1000 * xmode->dotclock / xmode->htotal / xmode->vtotal;
	vid_nummodes++;
	mode++;
    }
    XFree(xmodes);

    VID_SortModeList(vid_modelist, vid_nummodes);
}
