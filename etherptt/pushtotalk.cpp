/*
 *   MSX Ethernet Audio
 *
 *   Copyright (C) 2014 Harlan Murphy
 *   Orbis Software - orbisoftware@gmail.com
 *   Based on aplay by Jaroslav Kysela and vplay by Michael Beck
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <stdlib.h>
#include <X11/XF86keysym.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

static Display *disp;
static int screen;
static Window win;
static Atom property;
static XEvent report;
static GC gc;

extern int shutdown_req;
int pust_to_talk_active;

void pushtotalk()
{
    pust_to_talk_active = 0;

    disp = XOpenDisplay(NULL);
    screen = DefaultScreen(disp);

    win = XCreateSimpleWindow(disp, RootWindow(disp, screen), 0, 0,
            100, 100, 0, 0, 0);

    property = XInternAtom(disp, "_NET_WM_STATE_FULLSCREEN", True);
    XChangeProperty(disp, win, XInternAtom(disp, "_NET_WM_STATE", True),
            XA_ATOM, 32, PropModeReplace, (unsigned char*) &property, 1);

    XMapWindow(disp, win);

    gc = XCreateGC(disp, win, 0, 0);

    XStoreName(disp, win, "Push-to-Talk");

    XSelectInput(disp, win,
            KeyPressMask | ButtonPressMask | ButtonReleaseMask);

    while (!shutdown_req)
    {
        XNextEvent(disp, &report);

        switch (report.type)
        {
        case KeyPress:
            if (XLookupKeysym(&report.xkey, 0) == XK_q)
                shutdown_req = true;
            else if (XLookupKeysym(&report.xkey, 0) == XF86XK_AudioRaiseVolume)
            {
            	const char *system_cmd = "amixer -c 1 -q set Speaker playback 10+";
            	system(system_cmd);
            }
            else if (XLookupKeysym(&report.xkey, 0) == XF86XK_AudioLowerVolume)
            {
            	const char *system_cmd = "amixer -c 1 -q set Speaker playback 10-";
            	system(system_cmd);
            }
            break;

        case ButtonPress:
            pust_to_talk_active = true;
            break;

        case ButtonRelease:
            pust_to_talk_active = false;
            break;
        }
    }
}
