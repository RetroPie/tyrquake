/*
Copyright (C) 1996-1997 Id Software, Inc.

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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include <string.h>

#include "client.h"
#include "draw.h"
#include "keys.h"
#include "quakedef.h"
#include "sbar.h"
#include "screen.h"
#include "sound.h"
#include "sys.h"

#ifdef NQ_HACK
#include "host.h"
#endif

cvar_t scr_centertime = { "scr_centertime", "2" };
cvar_t scr_printspeed = { "scr_printspeed", "8" };

static char scr_centerstring[1024];
static float scr_centertime_start;	// for slow victory printing
float scr_centertime_off;
static int scr_center_lines;
static int scr_erase_lines;
static int scr_erase_center;

/*
===============================================================================

CENTER PRINTING

===============================================================================
*/

/*
==============
SCR_CenterPrint

Called for important messages that should stay in the center of the screen
for a few moments
==============
*/
void
SCR_CenterPrint(const char *str)
{
    strncpy(scr_centerstring, str, sizeof(scr_centerstring));
    scr_centerstring[sizeof(scr_centerstring) - 1] = 0;
    scr_centertime_off = scr_centertime.value;
    scr_centertime_start = cl.time;

    /* count the number of lines for centering */
    scr_center_lines = 1;
    while (*str) {
	if (*str == '\n')
	    scr_center_lines++;
	str++;
    }
}

void
SCR_EraseCenterString(void)
{
    int y, height;

    if (scr_erase_center++ > vid.numpages) {
	scr_erase_lines = 0;
	return;
    }

    if (scr_center_lines <= 4)
	y = vid.height * 0.35;
    else
	y = 48;

    /* Make sure we don't draw off the bottom of the screen*/
    height = qmin(8 * scr_erase_lines, ((int)vid.height) - y - 1);

    scr_copytop = 1;
    Draw_TileClear(0, y, vid.width, height);
}

void
SCR_DrawCenterString(void)
{
    char *start;
    int l;
    int j;
    int x, y;
    int remaining;

    scr_copytop = 1;
    if (scr_center_lines > scr_erase_lines)
	scr_erase_lines = scr_center_lines;

    scr_centertime_off -= host_frametime;

    if (scr_centertime_off <= 0 && !cl.intermission)
	return;
    if (key_dest != key_game)
	return;

// the finale prints the characters one at a time
    if (cl.intermission)
	remaining = scr_printspeed.value * (cl.time - scr_centertime_start);
    else
	remaining = 9999;

    scr_erase_center = 0;
    start = scr_centerstring;

    if (scr_center_lines <= 4)
	y = vid.height * 0.35;
    else
	y = 48;

    do {
	// scan the width of the line
	for (l = 0; l < 40; l++)
	    if (start[l] == '\n' || !start[l])
		break;
	x = (vid.width - l * 8) / 2;
	for (j = 0; j < l; j++, x += 8) {
	    Draw_Character(x, y, start[j]);
	    if (!remaining--)
		return;
	}

	y += 8;

	while (*start && *start != '\n')
	    start++;

	if (!*start)
	    break;
	start++;		// skip the \n
    } while (1);
}

//=============================================================================

static const char *scr_notifystring;
qboolean scr_drawdialog;

void
SCR_DrawNotifyString(void)
{
    const char *start;
    int l;
    int j;
    int x, y;

    start = scr_notifystring;

    y = vid.height * 0.35;

    do {
	// scan the width of the line
	for (l = 0; l < 40; l++)
	    if (start[l] == '\n' || !start[l])
		break;
	x = (vid.width - l * 8) / 2;
	for (j = 0; j < l; j++, x += 8)
	    Draw_Character(x, y, start[j]);

	y += 8;

	while (*start && *start != '\n')
	    start++;

	if (!*start)
	    break;
	start++;		// skip the \n
    } while (1);
}


/*
==================
SCR_ModalMessage

Displays a text string in the center of the screen and waits for a Y or N
keypress.
==================
*/
int
SCR_ModalMessage(const char *text)
{
#ifdef NQ_HACK
    if (cls.state == ca_dedicated)
	return true;
#endif

    scr_notifystring = text;

// draw a fresh screen
    scr_fullupdate = 0;
    scr_drawdialog = true;
    SCR_UpdateScreen();
    scr_drawdialog = false;

    S_ClearBuffer();		// so dma doesn't loop current sound

    do {
	key_count = -1;		// wait for a key down and up
	Sys_SendKeyEvents();
    } while (key_lastpress != 'y' && key_lastpress != 'n'
	     && key_lastpress != K_ESCAPE);

    scr_fullupdate = 0;
    SCR_UpdateScreen();

    return key_lastpress == 'y';
}

//=============================================================================

/*
===============
SCR_BringDownConsole

Brings the console down and fades the palettes back to normal
================
*/
void
SCR_BringDownConsole(void)
{
    int i;

    scr_centertime_off = 0;

    for (i = 0; i < 20 && scr_conlines != scr_con_current; i++)
	SCR_UpdateScreen();

    cl.cshifts[0].percent = 0;	// no area contents palette on next frame
    VID_SetPalette(host_basepal);
}

//============================================================================

/*
====================
CalcFov
====================
*/
static float
CalcFov(float fov_x, float width, float height)
{
    float a;
    float x;

    if (fov_x < 1 || fov_x > 179)
	Sys_Error("Bad fov: %f", fov_x);

    x = width / tan(fov_x / 360 * M_PI);
    a = atan(height / x);
    a = a * 360 / M_PI;

    return a;
}


/*
=================
SCR_CalcRefdef

Must be called whenever vid changes
Internal use only
=================
*/
void
SCR_CalcRefdef(void)
{
    vrect_t vrect;
    float size;

    scr_fullupdate = 0;		// force a background redraw
    vid.recalc_refdef = 0;

// force the status bar to redraw
    Sbar_Changed();

//========================================

// bound viewsize
    if (scr_viewsize.value < 30)
	Cvar_Set("viewsize", "30");
    if (scr_viewsize.value > 120)
	Cvar_Set("viewsize", "120");

// bound field of view
    if (scr_fov.value < 10)
	Cvar_Set("fov", "10");
    if (scr_fov.value > 170)
	Cvar_Set("fov", "170");

// intermission is always full screen
    if (cl.intermission)
	size = 120;
    else
	size = scr_viewsize.value;

    if (size >= 120)
	sb_lines = 0;		// no status bar at all
    else if (size >= 110)
	sb_lines = 24;		// no inventory
    else
	sb_lines = 24 + 16 + 8;

// these calculations mirror those in R_Init() for r_refdef, but take no
// account of water warping
    vrect.x = 0;
    vrect.y = 0;
    vrect.width = vid.width;
    vrect.height = vid.height;

#ifdef GLQUAKE
    R_SetVrect(&vrect, &r_refdef.vrect, sb_lines);
#else
    R_SetVrect(&vrect, &scr_vrect, sb_lines);
#endif

    r_refdef.fov_x = scr_fov.value;
    r_refdef.fov_y =
	CalcFov(r_refdef.fov_x, r_refdef.vrect.width, r_refdef.vrect.height);

#ifdef GLQUAKE
    scr_vrect = r_refdef.vrect;
#else
// guard against going from one mode to another that's less than half the
// vertical resolution
    if (scr_con_current > vid.height)
	scr_con_current = vid.height;

// notify the refresh of the change
    R_ViewChanged(&vrect, sb_lines, vid.aspect);
#endif
}

/*
=================
SCR_SizeUp_f

Keybinding command
=================
*/
void
SCR_SizeUp_f(void)
{
    Cvar_SetValue("viewsize", scr_viewsize.value + 10);
    vid.recalc_refdef = 1;
}


/*
=================
SCR_SizeDown_f

Keybinding command
=================
*/
void
SCR_SizeDown_f(void)
{
    Cvar_SetValue("viewsize", scr_viewsize.value - 10);
    vid.recalc_refdef = 1;
}