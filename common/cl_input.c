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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/
// cl.input.c  -- builds an intended movement command to send to the server

#include "client.h"
#include "cmd.h"
#include "console.h"
#include "input.h"
#include "quakedef.h"

#ifdef NQ_HACK
#include "host.h"
#include "net.h"
#include "protocol.h"
#endif

#ifdef QW_HACK
static cvar_t cl_nodelta = { "cl_nodelta", "0" };
#endif

/*
===============================================================================

KEY BUTTONS

Continuous button event tracking is complicated by the fact that two different
input sources (say, mouse button 1 and the control key) can both press the
same button, but the button should only be released when both of the
pressing key have been released.

When a key event issues a button command (+forward, +attack, etc), it appends
its key number as a parameter to the command so it can be matched up with
the release.

state bit 0 is the current state of the key
state bit 1 is edge triggered on the up to down transition
state bit 2 is edge triggered on the down to up transition

===============================================================================
*/

kbutton_t in_mlook, in_strafe, in_speed;

static kbutton_t in_klook;
static kbutton_t in_left, in_right, in_forward, in_back;
static kbutton_t in_lookup, in_lookdown, in_moveleft, in_moveright;
static kbutton_t in_use, in_jump, in_attack;
static kbutton_t in_up, in_down;

static int in_impulse;

static void
KeyDown(kbutton_t *b)
{
    int k;
    const char *c;

    c = Cmd_Argv(1);
    if (c[0])
	k = atoi(c);
    else
	k = -1;			// typed manually at the console for continuous down

    if (k == b->down[0] || k == b->down[1])
	return;			// repeating key

    if (!b->down[0])
	b->down[0] = k;
    else if (!b->down[1])
	b->down[1] = k;
    else {
	Con_Printf("Three keys down for a button!\n");
	return;
    }

    if (b->state & 1)
	return;			// still down
    b->state |= 1 + 2;		// down + impulse down
}

static void
KeyUp(kbutton_t *b)
{
    int k;
    const char *c;

    c = Cmd_Argv(1);
    if (c[0])
	k = atoi(c);
    else {			// typed manually at the console, assume for unsticking, so clear all
	b->down[0] = b->down[1] = 0;
	b->state = 4;		// impulse up
	return;
    }

    if (b->down[0] == k)
	b->down[0] = 0;
    else if (b->down[1] == k)
	b->down[1] = 0;
    else
	return;			// key up without coresponding down (menu pass through)
    if (b->down[0] || b->down[1])
	return;			// some other key is still holding it down

    if (!(b->state & 1))
	return;			// still up (this should not happen)
    b->state &= ~1;		// now up
    b->state |= 4;		// impulse up
}

static void
IN_KLookDown(void)
{
    KeyDown(&in_klook);
}

static void
IN_KLookUp(void)
{
    KeyUp(&in_klook);
}

static void
IN_MLookDown(void)
{
    KeyDown(&in_mlook);
    if (!((in_mlook.state & 1) ^ (int)m_freelook.value) && lookspring.value)
	V_StartPitchDrift();
}

static void
IN_MLookUp(void)
{
    KeyUp(&in_mlook);
    if (!((in_mlook.state & 1) ^ (int)m_freelook.value) && lookspring.value)
	V_StartPitchDrift();
}

static void
IN_UpDown(void)
{
    KeyDown(&in_up);
}

static void
IN_UpUp(void)
{
    KeyUp(&in_up);
}

static void
IN_DownDown(void)
{
    KeyDown(&in_down);
}

static void
IN_DownUp(void)
{
    KeyUp(&in_down);
}

static void
IN_LeftDown(void)
{
    KeyDown(&in_left);
}

static void
IN_LeftUp(void)
{
    KeyUp(&in_left);
}

static void
IN_RightDown(void)
{
    KeyDown(&in_right);
}

static void
IN_RightUp(void)
{
    KeyUp(&in_right);
}

static void
IN_ForwardDown(void)
{
    KeyDown(&in_forward);
}

static void
IN_ForwardUp(void)
{
    KeyUp(&in_forward);
}

static void
IN_BackDown(void)
{
    KeyDown(&in_back);
}

static void
IN_BackUp(void)
{
    KeyUp(&in_back);
}

static void
IN_LookupDown(void)
{
    KeyDown(&in_lookup);
}

static void
IN_LookupUp(void)
{
    KeyUp(&in_lookup);
}

static void
IN_LookdownDown(void)
{
    KeyDown(&in_lookdown);
}

static void
IN_LookdownUp(void)
{
    KeyUp(&in_lookdown);
}

static void
IN_MoveleftDown(void)
{
    KeyDown(&in_moveleft);
}

static void
IN_MoveleftUp(void)
{
    KeyUp(&in_moveleft);
}

static void
IN_MoverightDown(void)
{
    KeyDown(&in_moveright);
}

static void
IN_MoverightUp(void)
{
    KeyUp(&in_moveright);
}

static void
IN_SpeedDown(void)
{
    KeyDown(&in_speed);
}

static void
IN_SpeedUp(void)
{
    KeyUp(&in_speed);
}

static void
IN_StrafeDown(void)
{
    KeyDown(&in_strafe);
}

static void
IN_StrafeUp(void)
{
    KeyUp(&in_strafe);
}

static void
IN_AttackDown(void)
{
    KeyDown(&in_attack);
}

static void
IN_AttackUp(void)
{
    KeyUp(&in_attack);
}

static void
IN_UseDown(void)
{
    KeyDown(&in_use);
}

static void
IN_UseUp(void)
{
    KeyUp(&in_use);
}

static void
IN_JumpDown(void)
{
    KeyDown(&in_jump);
}

static void
IN_JumpUp(void)
{
    KeyUp(&in_jump);
}

static void
IN_Impulse(void)
{
    in_impulse = Q_atoi(Cmd_Argv(1));
}

/*
===============
CL_KeyState

Returns 0.25 if a key was pressed and released during the frame,
0.5 if it was pressed and held
0 if held then released, and
1.0 if held for the entire time
===============
*/
static float
CL_KeyState(kbutton_t *key)
{
    float val;
    qboolean impulsedown, impulseup, down;

    impulsedown = key->state & 2;
    impulseup = key->state & 4;
    down = key->state & 1;
    val = 0;

    if (impulsedown && !impulseup) {
	if (down)
	    val = 0.5;		// pressed and held this frame
	else
	    val = 0;		//      I_Error ();
    }
    if (impulseup && !impulsedown) {
        // FIXME: both alternatives zero?
	if (down)
	    val = 0;		//      I_Error ();
	else
	    val = 0;		// released this frame
    }
    if (!impulsedown && !impulseup) {
	if (down)
	    val = 1.0;		// held the entire frame
	else
	    val = 0;		// up the entire frame
    }
    if (impulsedown && impulseup) {
	if (down)
	    val = 0.75;		// released and re-pressed this frame
	else
	    val = 0.25;		// pressed and released this frame
    }

    key->state &= 1;		// clear impulses

    return val;
}




//==========================================================================

cvar_t cl_upspeed = { "cl_upspeed", "200" };
cvar_t cl_forwardspeed = { "cl_forwardspeed", "200" };
cvar_t cl_backspeed = { "cl_backspeed", "200" };
cvar_t cl_sidespeed = { "cl_sidespeed", "350" };

cvar_t cl_movespeedkey = { "cl_movespeedkey", "2.0" };

cvar_t cl_yawspeed = { "cl_yawspeed", "140" };
cvar_t cl_pitchspeed = { "cl_pitchspeed", "150" };

cvar_t cl_anglespeedkey = { "cl_anglespeedkey", "1.5" };

cvar_t cl_run = { "cl_run", "0", CVAR_CONFIG };


/*
================
CL_AdjustAngles

Moves the local angle positions
================
*/
static void
CL_AdjustAngles(void)
{
    float speed;
    float up, down;

    if ((in_speed.state & 1) ^ (int)cl_run.value)
	speed = host_frametime * cl_anglespeedkey.value;
    else
	speed = host_frametime;

    if (!(in_strafe.state & 1)) {
	cl.viewangles[YAW] -=
	    speed * cl_yawspeed.value * CL_KeyState(&in_right);
	cl.viewangles[YAW] +=
	    speed * cl_yawspeed.value * CL_KeyState(&in_left);
	cl.viewangles[YAW] = anglemod(cl.viewangles[YAW]);
    }
    if (in_klook.state & 1) {
	V_StopPitchDrift();
	cl.viewangles[PITCH] -=
	    speed * cl_pitchspeed.value * CL_KeyState(&in_forward);
	cl.viewangles[PITCH] +=
	    speed * cl_pitchspeed.value * CL_KeyState(&in_back);
    }

    up = CL_KeyState(&in_lookup);
    down = CL_KeyState(&in_lookdown);

    cl.viewangles[PITCH] -= speed * cl_pitchspeed.value * up;
    cl.viewangles[PITCH] += speed * cl_pitchspeed.value * down;

    if (up || down)
	V_StopPitchDrift();

    if (cl.viewangles[PITCH] > cl_maxpitch.value)
	cl.viewangles[PITCH] = cl_maxpitch.value;
    if (cl.viewangles[PITCH] < cl_minpitch.value)
	cl.viewangles[PITCH] = cl_minpitch.value;

    if (cl.viewangles[ROLL] > 50)
	cl.viewangles[ROLL] = 50;
    if (cl.viewangles[ROLL] < -50)
	cl.viewangles[ROLL] = -50;

}

/*
================
CL_BaseMove

Send the intended movement message to the server
================
*/
void
CL_BaseMove(usercmd_t *cmd)
{
#ifdef NQ_HACK
    if (cls.state != ca_active)
	return;
#endif

    CL_AdjustAngles();

    memset(cmd, 0, sizeof(*cmd));

#ifdef QW_HACK
    VectorCopy(cl.viewangles, cmd->angles);
#endif
    if (in_strafe.state & 1) {
	cmd->sidemove += cl_sidespeed.value * CL_KeyState(&in_right);
	cmd->sidemove -= cl_sidespeed.value * CL_KeyState(&in_left);
    }

    cmd->sidemove += cl_sidespeed.value * CL_KeyState(&in_moveright);
    cmd->sidemove -= cl_sidespeed.value * CL_KeyState(&in_moveleft);

    cmd->upmove += cl_upspeed.value * CL_KeyState(&in_up);
    cmd->upmove -= cl_upspeed.value * CL_KeyState(&in_down);

    if (!(in_klook.state & 1)) {
	cmd->forwardmove += cl_forwardspeed.value * CL_KeyState(&in_forward);
	cmd->forwardmove -= cl_backspeed.value * CL_KeyState(&in_back);
    }
//
// adjust for speed key
//
    if ((in_speed.state & 1) ^ (int)cl_run.value) {
	cmd->forwardmove *= cl_movespeedkey.value;
	cmd->sidemove *= cl_movespeedkey.value;
	cmd->upmove *= cl_movespeedkey.value;
    }
}

#ifdef NQ_HACK
/*
==============
CL_SendMove
==============
*/
static void
CL_SendMove(usercmd_t *cmd)
{
    int i;
    int bits;
    sizebuf_t buf;
    byte data[128];

    buf.maxsize = 128;
    buf.cursize = 0;
    buf.data = data;

    cl.cmd = *cmd;

    /*
     * send the movement message
     */
    MSG_WriteByte(&buf, clc_move);
    MSG_WriteFloat(&buf, cl.mtime[0]);	/* so server can get ping times */

    for (i = 0; i < 3; i++)
	if (cl.protocol == PROTOCOL_VERSION_FITZ)
	    MSG_WriteAngle16(&buf, cl.viewangles[i]);
	else
	    MSG_WriteAngle(&buf, cl.viewangles[i]);

    MSG_WriteShort(&buf, cmd->forwardmove);
    MSG_WriteShort(&buf, cmd->sidemove);
    MSG_WriteShort(&buf, cmd->upmove);

    /*
     * send button bits
     */
    bits = 0;

    if (in_attack.state & 3)
	bits |= 1;
    in_attack.state &= ~2;

    if (in_jump.state & 3)
	bits |= 2;
    in_jump.state &= ~2;

    MSG_WriteByte(&buf, bits);
    MSG_WriteByte(&buf, in_impulse);
    in_impulse = 0;

    if (cls.demoplayback)
	return;

    /*
     * deliver the message
     */

    /*
     * always dump the first two message, because it may contain leftover
     * inputs from the last level
     */
    if (++cl.movemessages <= 2)
	return;

    if (NET_SendUnreliableMessage(cls.netcon, &buf) == -1) {
	Con_Printf("CL_SendMove: lost server connection\n");
	CL_Disconnect();
    }
}

/*
=================
CL_SendCmd
=================
*/
void
CL_SendCmd(void)
{
    usercmd_t cmd;

    if (cls.state < ca_connected)
	return;

    if (cls.state == ca_active) {
	// get basic movement from keyboard
	CL_BaseMove(&cmd);

	// allow mice or other external controllers to add to the move
	IN_Move(&cmd);

	// send the unreliable message
	CL_SendMove(&cmd);
    }

    if (cls.demoplayback) {
	SZ_Clear(&cls.message);
	return;
    }
// send the reliable message
    if (!cls.message.cursize)
	return;			// no message at all

    if (!NET_CanSendMessage(cls.netcon)) {
	Con_DPrintf("CL_WriteToServer: can't send\n");
	return;
    }

    if (NET_SendMessage(cls.netcon, &cls.message) == -1)
	Host_Error("CL_WriteToServer: lost server connection");

    SZ_Clear(&cls.message);
}
#endif // NQ_HACK

#ifdef QW_HACK
static int
MakeChar(int i)
{
    i &= ~3;
    if (i < -127 * 4)
	i = -127 * 4;
    if (i > 127 * 4)
	i = 127 * 4;
    return i;
}

/*
==============
CL_FinishMove
==============
*/
static void
CL_FinishMove(usercmd_t *cmd)
{
    int i;
    int ms;

//
// always dump the first two message, because it may contain leftover inputs
// from the last level
//
    if (++cl.movemessages <= 2)
	return;
//
// figure button bits
//
    if (in_attack.state & 3)
	cmd->buttons |= 1;
    in_attack.state &= ~2;

    if (in_jump.state & 3)
	cmd->buttons |= 2;
    in_jump.state &= ~2;

    // send milliseconds of time to apply the move
    ms = host_frametime * 1000;
    if (ms > 250)
	ms = 100;		// time was unreasonable
    cmd->msec = ms;

    VectorCopy(cl.viewangles, cmd->angles);

    cmd->impulse = in_impulse;
    in_impulse = 0;


//
// chop down so no extra bits are kept that the server wouldn't get
//
    cmd->forwardmove = MakeChar(cmd->forwardmove);
    cmd->sidemove = MakeChar(cmd->sidemove);
    cmd->upmove = MakeChar(cmd->upmove);

    for (i = 0; i < 3; i++)
	cmd->angles[i] =
	    ((int)(cmd->angles[i] * 65536.0 / 360) & 65535) * (360.0 /
							       65536.0);
}

/*
=================
CL_SendCmd
=================
*/
void
CL_SendCmd(const physent_stack_t *pestack)
{
    sizebuf_t buf;
    byte data[128];
    int i;
    usercmd_t *cmd, *oldcmd;
    int checksumIndex;
    int lost;
    int seq_hash;

    if (cls.demoplayback)
	return;			// sendcmds come from the demo

    // save this command off for prediction
    i = cls.netchan.outgoing_sequence & UPDATE_MASK;
    cmd = &cl.frames[i].cmd;
    cl.frames[i].senttime = realtime;
    cl.frames[i].receivedtime = -1;	// we haven't gotten a reply yet

//      seq_hash = (cls.netchan.outgoing_sequence & 0xffff) ; // ^ QW_CHECK_HASH;
    seq_hash = cls.netchan.outgoing_sequence;

    // get basic movement from keyboard
    CL_BaseMove(cmd);

    // allow mice or other external controllers to add to the move
    IN_Move(cmd);

    // if we are spectator, try autocam
    if (cl.spectator)
	Cam_Track(cmd, pestack);

    CL_FinishMove(cmd);

    Cam_FinishMove(cmd);

// send this and the previous cmds in the message, so
// if the last packet was dropped, it can be recovered
    buf.maxsize = 128;
    buf.cursize = 0;
    buf.data = data;

    MSG_WriteByte(&buf, clc_move);

    // save the position for a checksum byte
    checksumIndex = buf.cursize;
    MSG_WriteByte(&buf, 0);

    // write our lossage percentage
    lost = CL_CalcNet();
    MSG_WriteByte(&buf, (byte)lost);

    i = (cls.netchan.outgoing_sequence - 2) & UPDATE_MASK;
    cmd = &cl.frames[i].cmd;
    MSG_WriteDeltaUsercmd(&buf, &nullcmd, cmd);
    oldcmd = cmd;

    i = (cls.netchan.outgoing_sequence - 1) & UPDATE_MASK;
    cmd = &cl.frames[i].cmd;
    MSG_WriteDeltaUsercmd(&buf, oldcmd, cmd);
    oldcmd = cmd;

    i = (cls.netchan.outgoing_sequence) & UPDATE_MASK;
    cmd = &cl.frames[i].cmd;
    MSG_WriteDeltaUsercmd(&buf, oldcmd, cmd);

    // calculate a checksum over the move commands
    buf.data[checksumIndex] =
	COM_BlockSequenceCRCByte(buf.data + checksumIndex + 1,
				 buf.cursize - checksumIndex - 1, seq_hash);

    // request delta compression of entities
    if (cls.netchan.outgoing_sequence - cl.validsequence >= UPDATE_BACKUP - 1)
	cl.validsequence = 0;

    if (cl.validsequence && !cl_nodelta.value && cls.state == ca_active &&
	!cls.demorecording) {
	cl.frames[cls.netchan.outgoing_sequence & UPDATE_MASK].
	    delta_sequence = cl.validsequence;
	MSG_WriteByte(&buf, clc_delta);
	MSG_WriteByte(&buf, cl.validsequence & 255);
    } else
	cl.frames[cls.netchan.outgoing_sequence & UPDATE_MASK].
	    delta_sequence = -1;

    if (cls.demorecording)
	CL_WriteDemoCmd(cmd);

//
// deliver the message
//
    Netchan_Transmit(&cls.netchan, buf.cursize, buf.data);
}
#endif // QW_HACK

void
CL_Input_AddCommands(void)
{
    Cmd_AddCommand("+moveup", IN_UpDown);
    Cmd_AddCommand("-moveup", IN_UpUp);
    Cmd_AddCommand("+movedown", IN_DownDown);
    Cmd_AddCommand("-movedown", IN_DownUp);
    Cmd_AddCommand("+left", IN_LeftDown);
    Cmd_AddCommand("-left", IN_LeftUp);
    Cmd_AddCommand("+right", IN_RightDown);
    Cmd_AddCommand("-right", IN_RightUp);
    Cmd_AddCommand("+forward", IN_ForwardDown);
    Cmd_AddCommand("-forward", IN_ForwardUp);
    Cmd_AddCommand("+back", IN_BackDown);
    Cmd_AddCommand("-back", IN_BackUp);
    Cmd_AddCommand("+lookup", IN_LookupDown);
    Cmd_AddCommand("-lookup", IN_LookupUp);
    Cmd_AddCommand("+lookdown", IN_LookdownDown);
    Cmd_AddCommand("-lookdown", IN_LookdownUp);
    Cmd_AddCommand("+strafe", IN_StrafeDown);
    Cmd_AddCommand("-strafe", IN_StrafeUp);
    Cmd_AddCommand("+moveleft", IN_MoveleftDown);
    Cmd_AddCommand("-moveleft", IN_MoveleftUp);
    Cmd_AddCommand("+moveright", IN_MoverightDown);
    Cmd_AddCommand("-moveright", IN_MoverightUp);
    Cmd_AddCommand("+speed", IN_SpeedDown);
    Cmd_AddCommand("-speed", IN_SpeedUp);
    Cmd_AddCommand("+attack", IN_AttackDown);
    Cmd_AddCommand("-attack", IN_AttackUp);
    Cmd_AddCommand("+use", IN_UseDown);
    Cmd_AddCommand("-use", IN_UseUp);
    Cmd_AddCommand("+jump", IN_JumpDown);
    Cmd_AddCommand("-jump", IN_JumpUp);
    Cmd_AddCommand("impulse", IN_Impulse);
    Cmd_AddCommand("+klook", IN_KLookDown);
    Cmd_AddCommand("-klook", IN_KLookUp);
    Cmd_AddCommand("+mlook", IN_MLookDown);
    Cmd_AddCommand("-mlook", IN_MLookUp);
}

void
CL_Input_RegisterVariables()
{
#ifdef QW_HACK
    Cvar_RegisterVariable(&cl_nodelta);
#endif
}
