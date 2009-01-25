/*************************************************************************/
/*  _____________                                                        */
/* < yeahconsole >                                                       */
/*  -------------                                                        */
/*         \   ^__^                                                      */
/*          \  (oo)\_______                                              */
/*             (__)\       )\/\                                          */
/*                 ||----w |                                             */
/*                 ||     ||                                             */
/*                                                                       */
/*  Copyright (C) knorke                                                 */
/*                                                                       */
/*  This program is free software; you can redistribute it and/or modify */
/*  it under the terms of the GNU General Public License as published by */
/*  the Free Software Foundation; either version 2 of the License, or    */
/*  (at your option) any later version.                                  */
/*                                                                       */
/*  This program is distributed in the hope that it will be useful,      */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of       */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        */
/*  GNU General Public License for more details.                         */
/*                                                                       */
/*  You should have received a copy of the GNU General Public License    */
/*  along with this program; if not, write to the Free Software          */
/*  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.            */
/*************************************************************************/

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define UP 0
#define DOWN -height -opt_bw
#define transparency_hack() XResizeWindow(dpy, termwin, opt_width, height -1);\
		            XResizeWindow(dpy, termwin, opt_width, height )

Display *dpy;
Window root;
Window win;
Window termwin;
char *progname, command[256];
int revert_to;
int screen;
int opt_x, opt_width, opt_height, opt_delay, opt_bw, opt_step,
  height, opt_restart, opt_restart_hidden;
char *opt_color;
char *opt_term;
KeySym opt_key;
KeySym opt_key_smaller;
KeySym opt_key_bigger;
KeySym opt_key_full;
Cursor cursor;
int resize_inc;
void roll(int i);
void get_defaults(void);
KeySym grab_that_key(char *opt, unsigned int numlockmask);
void init_win(void);
void init_xterm(int move);
void init_command(int argc, char *argv[]);
void resize(void);
int handle_xerror(Display * display, XErrorEvent * e);

int main(int argc, char *argv[])
{
    KeySym key;
    XEvent event;
    int hidden = 1;
    int fullscreen = 0;
    int i;
    int old_height = 0;
    Window last_focused, current_focused;
	
    /* strip the path from argv[0] if there is one */
    progname = strrchr(argv[0], '/');
    if (!progname)
	progname = argv[0];
    else
	progname++;

    for (i = 1; i < argc; i++) {
	if (!strcmp(argv[i], "-h") )
	{
	    printf("%s:\n"
		   "-e: program to execute\n"
		   "you can configure me via xresources:\n"
		   "%s*foo:value\n"
		   "foo can be any standard xterm/urxvt xresource or:\n"
		   "resource               default value\n\n"
		   "term:                  xterm\n"
		   "restart:               0\n"
		   "xOffset:               0\n"
		   "screenWidth:           Display width\n"
		   "consoleHeight:         10\n"
		   "aniDelay:              40\n"
		   "stepSize;              1\n"
		   "toggleKey:             ControlAlt+y\n"
		   "keySmaller:            Control+KP_Subtract\n"
		   "keyBigger:             Control+KP_Add\n"
		   "keyFull:               Alt+F11\n", progname, progname);
	    exit(0);
	}
    }

    if (!(dpy = XOpenDisplay(NULL))) {
	fprintf(stderr, " can not open dpy %s", XDisplayName(NULL));
    }
    screen = DefaultScreen(dpy);
    root = RootWindow(dpy, screen);
    XSetErrorHandler(handle_xerror);
    cursor = XCreateFontCursor(dpy, XC_double_arrow);
    get_defaults();
    init_win();
    init_command(argc, argv);
    init_xterm(1);
    while (1) {
	XNextEvent(dpy, &event);
	switch (event.type) {
	case EnterNotify:
	    XSetInputFocus(dpy, termwin, RevertToPointerRoot, CurrentTime);
	    break;
	case LeaveNotify:
	    if (last_focused && event.xcrossing.detail != NotifyInferior)
		XSetInputFocus(dpy, last_focused, RevertToPointerRoot,
			       CurrentTime);
	    break;
	case KeyPress:
	    key = XKeycodeToKeysym(dpy, event.xkey.keycode, 0);
	    if (key == opt_key) {
		if (!hidden) {
		    XGetInputFocus(dpy, &current_focused, &revert_to);
		    if (last_focused && current_focused == termwin)
			XSetInputFocus(dpy, last_focused,
				       RevertToPointerRoot, CurrentTime);
		    /* else
		       XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime); */
		    if (opt_step && !fullscreen)
			roll(UP);
		    XMoveWindow(dpy, win, opt_x, -height - opt_bw);
		    hidden = 1;
		    XSync(dpy, False);
		} else {
		    XRaiseWindow(dpy, win);
		    XGetInputFocus(dpy, &last_focused, &revert_to);
		    XSetInputFocus(dpy, termwin, RevertToPointerRoot,
				   CurrentTime);

		    if (opt_step && !fullscreen) {
			XGrabServer(dpy);
			roll(DOWN);
			XUngrabServer(dpy);
		    }
		    XMoveWindow(dpy, win, opt_x, 0);

		    transparency_hack();
		    hidden = 0;
		}
		break;
	    }
	    if (!hidden) {
		if (key == opt_key_full) {
		    if (!fullscreen) {
			old_height = height;
			height = DisplayHeight(dpy, screen);
			fullscreen = 1;
		    } else {
			height = old_height;
			fullscreen = 0;
		    }
		}

		if (key == opt_key_bigger)
		    height += resize_inc;
		if (key == opt_key_smaller)
		    height -= resize_inc;
		if (height < resize_inc)
		    height = resize_inc;
		XResizeWindow(dpy, termwin, opt_width, height);
		XResizeWindow(dpy, win, opt_width, height + opt_bw);
		/* XSetInputFocus(dpy, termwin, RevertToPointerRoot, CurrentTime); */
	    }
	    break;
	case ButtonPress:
	    resize();
	    break;
	case UnmapNotify:
	    if (event.xunmap.window == termwin) {
		if (opt_restart) {
                    if (opt_restart_hidden) {
                        roll(UP);
                        hidden = 1;
                    }
		    init_xterm(0);
		    XSync(dpy, False);
                    if (opt_restart_hidden && last_focused)
                        XSetInputFocus(dpy, last_focused,
                                       RevertToPointerRoot, CurrentTime);
                    else
                        XSetInputFocus(dpy, termwin, RevertToPointerRoot,
                                       CurrentTime);
		} else {
		    if (last_focused)
			XSetInputFocus(dpy, last_focused,
				       RevertToPointerRoot, CurrentTime);
		    XSync(dpy, False);
		    exit(0);
		}
	    }
	    break;
	}
    }
    return 0;
}

void roll(int i)
{
    int x = height / 100 + 1 + opt_step;

    if (i == 0)
	x = -x;

    while (1) {
	i += x;
	XMoveWindow(dpy, win, opt_x, i);
	XSync(dpy, False);
	usleep(opt_delay * 100);
	if (i / x == 0 || i < -(height + opt_bw))
	    break;
    }

}

void get_defaults()
{
    char *opt;
    XModifierKeymap *modmap;
    unsigned int numlockmask = 0;
    int i, j;
    char def_key[] = "ControlAlt+y";
    char def_key_bigger[] = "Control+KP_Add";
    char def_key_smaller[] = "Control+KP_Subtract";
    char def_key_full[] = "Alt+F11";

    /* modifier stuff taken from evilwm */
    modmap = XGetModifierMapping(dpy);
    for (i = 0; i < 8; i++) {
	for (j = 0; j < modmap->max_keypermod; j++) {
	    if (modmap->modifiermap[i * modmap->max_keypermod + j] ==
		XKeysymToKeycode(dpy, XK_Num_Lock)) {
		numlockmask = (1 << i);
	    }
	}
    }
    XFreeModifiermap(modmap);
    opt = XGetDefault(dpy, progname, "screenWidth");
    opt_width = opt ? atoi(opt) : DisplayWidth(dpy, screen);
    opt = XGetDefault(dpy, progname, "handleColor");
    opt_color = opt ? opt : "grey70";
    opt = XGetDefault(dpy, progname, "handleWidth");
    opt_bw = opt ? atoi(opt) : 3;
    opt = XGetDefault(dpy, progname, "consoleHeight");
    opt_height = opt ? atoi(opt) : 10;
    opt = XGetDefault(dpy, progname, "xOffset");
    opt_x = opt ? atoi(opt) : 0;
    opt = XGetDefault(dpy, progname, "aniDelay");
    opt_delay = opt ? atoi(opt) : 40;
    opt = XGetDefault(dpy, progname, "stepSize");
    opt_step = opt ? atoi(opt) : 1;
    opt = XGetDefault(dpy, progname, "restart");
    opt_restart = opt ? atoi(opt) : 0;
    opt = XGetDefault(dpy, progname, "restartHidden");
    opt_restart_hidden = opt ? atoi(opt) : 0;
    opt = XGetDefault(dpy, progname, "term");
    opt_term = opt ? opt : "xterm";
    opt = XGetDefault(dpy, progname, "toggleKey");
    opt_key =
	opt ? grab_that_key(opt, numlockmask) : grab_that_key(def_key,
							      numlockmask);
    opt = XGetDefault(dpy, progname, "keySmaller");
    opt_key_smaller =
	opt ? grab_that_key(opt,
			    numlockmask) : grab_that_key(def_key_smaller,
							 numlockmask);
    opt = XGetDefault(dpy, progname, "keyBigger");
    opt_key_bigger =
	opt ? grab_that_key(opt,
			    numlockmask) : grab_that_key(def_key_bigger,
							 numlockmask);
    opt = XGetDefault(dpy, progname, "keyFull");
    opt_key_full =
	opt ? grab_that_key(opt, numlockmask) : grab_that_key(def_key_full,
							      numlockmask);
}

KeySym grab_that_key(char *opt, unsigned int numlockmask)
{
    unsigned int modmask = 0;
    KeySym keysym;
	
    if (strstr(opt, "Control"))
	modmask = modmask | ControlMask;
    if (strstr(opt, "Alt"))
	modmask = modmask | Mod1Mask;
    if (strstr(opt, "Win"))
	modmask = modmask | Mod4Mask;
    if (strstr(opt, "None"))
	modmask = 0;

    opt = strrchr(opt, '+');
    keysym = XStringToKeysym(++opt);
	
    XGrabKey(dpy, XKeysymToKeycode(dpy, keysym), modmask, root, True,
	     GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, keysym), LockMask | modmask, root,
	     True, GrabModeAsync, GrabModeAsync);
    if (numlockmask) {
	XGrabKey(dpy, XKeysymToKeycode(dpy, keysym), numlockmask | modmask,
		 root, True, GrabModeAsync, GrabModeAsync);
	XGrabKey(dpy, XKeysymToKeycode(dpy, keysym),
		 numlockmask | LockMask | modmask, root, True,
		 GrabModeAsync, GrabModeAsync);

    }
    return keysym;
}

void init_win()
{
    XSetWindowAttributes attrib;
    XColor color;
    XColor dummy_color;

    attrib.override_redirect = True;
    attrib.background_pixel = BlackPixel(dpy, screen);
    win = XCreateWindow(dpy, root,
			opt_x, -200, opt_width, 200,
			0, CopyFromParent, InputOutput, CopyFromParent,
			CWOverrideRedirect | CWBackPixel, &attrib);
    XSelectInput(dpy, win,
		 SubstructureNotifyMask | EnterWindowMask | LeaveWindowMask
		 | KeyPressMask | ButtonPressMask | ButtonReleaseMask);
    XAllocNamedColor(dpy, DefaultColormap(dpy, screen), opt_color, &color,
		     &dummy_color);
    XSetWindowBackground(dpy, win, color.pixel);
    XDefineCursor(dpy, win, cursor);
    XMapWindow(dpy, win);
}

void init_xterm(move)
{
    XEvent ev;
    long dummy;
    XSizeHints *size;

    system(command);
    while (1) {
	XMaskEvent(dpy, SubstructureNotifyMask, &ev);
	if (ev.type == CreateNotify || ev.type == MapNotify) {
	    termwin = ev.xcreatewindow.window;
	    break;
	}
    }

    XSetWindowBorderWidth(dpy, termwin, 0);
    size = XAllocSizeHints();
    XGetWMNormalHints(dpy, termwin, size, &dummy);
    resize_inc = size->height_inc;
    if (move)
	height = resize_inc * opt_height;
    XFree(size);
    XResizeWindow(dpy, termwin, opt_width, height);
    XResizeWindow(dpy, win, opt_width, height + opt_bw);
    if (move)
	XMoveWindow(dpy, win, opt_x, -(height + opt_bw));
}

void init_command(int argc, char *argv[])
{

    int i;
    char *pos;
    pos = command;
    if (strstr(opt_term, "urxvt"))
	pos +=
	    sprintf(pos, "%s -b 0 -embed %d -name %s ", opt_term, (int) win,
		    progname);
    else
	pos +=
	    sprintf(pos, "%s -b 0 -into %d -name %s ", opt_term, (int) win,
		    progname);
    for (i = 1; i < argc; i++) {
	pos += sprintf(pos, "%s ", argv[i]);

    }
    sprintf(pos, "&");
}

void resize()
{
    XEvent ev;
    if (!XGrabPointer
	(dpy, root, False,
	 ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
	 GrabModeAsync, GrabModeAsync, None, cursor,
	 CurrentTime) == GrabSuccess)
	return;
    for (;;) {
	XMaskEvent(dpy,
		   ButtonPressMask | ButtonReleaseMask |
		   PointerMotionHintMask, &ev);
	switch (ev.type) {
	case MotionNotify:
	    if (ev.xmotion.y >= resize_inc) {
		height = ev.xmotion.y - ev.xmotion.y % resize_inc;
		XResizeWindow(dpy, termwin, opt_width, height);
		XResizeWindow(dpy, win, opt_width, height + opt_bw);
		XSync(dpy, False);
		break;
	case ButtonRelease:
		XUngrabPointer(dpy, CurrentTime);
		return;

	    }
	}
    }
}

int handle_xerror(Display * display, XErrorEvent * e)
{
    /* this function does nothing, we receive xerrors
       when the last_focused window gets lost.. */
    /* fprintf(stderr, "XError caught\n"); */
    fprintf(stderr, "%d  XError caught\n", e->error_code);
    return 0;
}
