/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#define _POSIX_SOURCE

#include "XInterface.h"
#include <stdio.h>
#include <paths.h>
#include <string.h>
#include <pwd.h>
#include <sys/types.h>
#include <utmp.h>
#include <setjmp.h>
#include <stdlib.h>

const char * PASSWD_FILE = "/etc/passwd";

bool        g_connected;
jmp_buf     jmp;

extern "C"
{
    extern int putenv(const char *);
    extern struct passwd *fgetpwent(FILE *);
}

// When a window has a very short life time, we get messages
// about it, and then we try and select the events on it.  It
// is possible for the window to go away between the time we
// here about it and the time we select the events.  In order
// to deal with this, we need to put in a error handler that
// basically ignores the error.

static int 
CatchFalseAlarm(Display *display, XErrorEvent *err)
{
    char msg[80];
    XGetErrorText(display, err->error_code, msg, 80);
    dprintf(D_FULLDEBUG, "Caught Error code(%d): %s\n", err->error_code, msg);
    return 0;
}

static int 
CatchIOFalseAlarm(Display *display, XErrorEvent *err)
{
    char msg[80];
    g_connected = false;
        
    XGetErrorText(display, err->error_code, msg, 80);
    dprintf(D_FULLDEBUG, "Caught Error code(%d): %s\n", err->error_code, msg);

    longjmp(jmp, 0);
    return 0;
}

// Get the next utmp entry and set our XAUTHORITY environment
// variable to the .Xauthority file in the user's home directory.
int
XInterface::NextEntry()
{
    utmp *utmp_entry;
    passwd *passwd_entry;
    static char env[1024];
    bool end = false;
    int utsize;
    int pwsize;
    int size;
    FILE *etc_passwd;
    
    if(!_tried_root)
    {
	etc_passwd = fopen(PASSWD_FILE, "r");
	passwd_entry = fgetpwent(etc_passwd);
	sprintf(env, "HOME=%s", passwd_entry->pw_dir);
	if(putenv(env) != 0)
	{
	    dprintf(D_ALWAYS, "Putenv failed!.\n");
	    return -1;
	}
	dprintf(D_FULLDEBUG, "Using %s's .Xauthority: \n", 
		passwd_entry->pw_name, env);
	_tried_root = true;
	fclose(etc_passwd);
    }

    do
    {
	utmp_entry = getutent();
	if(!utmp_entry)
	{
	    return -1;
	}
    } while(utmp_entry->ut_type != USER_PROCESS);
 
    utsize = strlen(utmp_entry->ut_user);

    // Cycle through the passwd file until we find the matching
    // passwd entry.
    etc_passwd = fopen(PASSWD_FILE, "r");
    do
    {
	passwd_entry = fgetpwent(etc_passwd);
	if(!passwd_entry)
	{
	    end = true;
	    break;
	}
	pwsize = strlen(passwd_entry->pw_name);
	if(pwsize > utsize)
	{
	    size = utsize;
	}
	else
	{
	    size = pwsize;
	}
    }
    while( strncmp(utmp_entry->ut_user, passwd_entry->pw_name, size) );
    fclose(etc_passwd);
    
    if(end == true)
    {
	// We couldn't find the current user in the passwd file?
	
	return -1;
    }
    else
    {
	
	fflush(stdout);

	sprintf(env, "HOME=%s", passwd_entry->pw_dir);
	if(putenv(env) != 0)
	{
	    dprintf(D_ALWAYS, "Putenv failed!.\n");
	    return -1;
	}
	dprintf(D_FULLDEBUG, "Using %s's .Xauthority: \n", 
		passwd_entry->pw_name, env);
    }
    return 0;
}

XInterface::XInterface()
{


    
    // We may need access to other user's home directories, so we must run
    // as root.
    set_root_priv();
    
    if(geteuid() != 0)
    {
	dprintf(D_ALWAYS, "We should run as root.\n");
    }

    _tried_root = false;
    g_connected = false;
    
    while(!Connect())
    {
	return;
    }
}

XInterface::~XInterface()
{
}

bool
XInterface::Connect()
{
    int err;
    Window root;
    int s;

    setutent(); // Reset utmp file to beginning.

    _tried_root = false;
    while(!(_display = XOpenDisplay("localhost:0.0") ))
    {
	err = NextEntry();
	if(err == -1)
	{
	    dprintf(D_FULLDEBUG, "Exausted all possible attempts to "
		    "connect to X server.\n");
	    return false;
	}
    }
    dprintf(D_ALWAYS, "Connected to X server: localhost:0.0\n");
    g_connected = true;

    // See note above the function to see why we need to do this.
    XSetErrorHandler((XErrorHandler) CatchFalseAlarm);
    XSetIOErrorHandler((XIOErrorHandler) CatchIOFalseAlarm);

    //Select the events on each root window of each screen
    for(s = 0; s < ScreenCount(_display); s++)
    {
	root = RootWindowOfScreen(ScreenOfDisplay(_display, s));
	SelectEvents(root);
    }
    
    // Query pointer stuff
    _pointer_root = DefaultRootWindow(_display);
    _pointer_screen = ScreenOfDisplay(_display, DefaultScreen(_display));
    _pointer_prev_x = -1;
    _pointer_prev_y = -1;
    _pointer_prev_mask = 0;

    return true;
}

bool
XInterface::CheckActivity()
{
    setjmp(jmp);
    while(!g_connected)
    {
	// If we can't connect, we don't know anything....
	if( Connect() == -1 )
	{
	    return false;
	}
    }
    
    if(ProcessEvents())
    {
	return true;
    }
    else if(QueryPointer())
    {
	return true;
    }
    else
    {
	return false;
    }
}


bool 
XInterface::ProcessEvents()
{
    XEvent event;
    
    while(XPending(_display))
    {
	if(XCheckMaskEvent(_display, SubstructureNotifyMask, &event))
	{
	    if(event.type == CreateNotify)
	    {
		SelectEvents(event.xcreatewindow.window);
	    }
	}
	else
	{
	    XNextEvent(_display, &event);
	}
    }
    if( (event.type == KeyPress || event.type == ButtonPress || 
	 event.type == ButtonRelease || event.type == MotionNotify) 
	&& !event.xany.send_event )
    {
	time(&_last_event);
	return true;
    }
    return false;
}


void 
XInterface::SelectEvents(Window win)
{
    Window root;
    Window parent;
    Window *children;
    unsigned int num_children = 0;
    unsigned int i;
    XWindowAttributes attribs;

    if(!XQueryTree(_display, win, &root, &parent, &children, &num_children))
    {
	return;
    }
    
    if(parent == None) // The root of all evil!!!!!!!!!!!!!11!!1!!
    {
	attribs.all_event_masks = 
	    attribs.do_not_propagate_mask = KeyPressMask;
    }
    else if(XGetWindowAttributes(_display, win, &attribs) == 0)
    {
	dprintf(D_ALWAYS, "XGetWindowAttributes() failed.\n");
	return;
    }
    else
    {
	XSelectInput(_display, win, SubstructureNotifyMask | 
		     ((attribs.all_event_masks | attribs.do_not_propagate_mask) 
		      & KeyPressMask));
	
    }
    
    //Recursion in action
    for(i = 0; i < num_children; i++)
    {
	    SelectEvents(children[i]);
    }
    if(num_children > 0)
    {
	XFree(children);
    }
}

bool
XInterface::QueryPointer()
{
    Window dummy_win;
    int dummy;
    unsigned int mask;
    int root_x;
    int root_y;
    int i;
    time_t now;
    bool found = false;

    if(!XQueryPointer(_display, _pointer_root, &_pointer_root, &dummy_win, 
		      &root_x, &root_y, &dummy, &dummy, &mask))
    {
	//Pointer has moved to another screen
	for(i = 0; i < ScreenCount(_display); i++)
	{
	    if(_pointer_root == RootWindow(_display, i))
	    {
		_pointer_screen = ScreenOfDisplay(_display, i);
		found = true;
		break;
	    }
	}
	if(!found)
	{
	    dprintf(D_ALWAYS, "Lost connection to X server.\n");
	    g_connected = false;
	}
    }

    if(root_x == _pointer_prev_x && root_y == _pointer_prev_y && 
       mask == _pointer_prev_mask)
    {
	// Pointer has not moved.
	return false;
    }
    else
    {
	// Pointer has indeed moved.
	time(&now);
	_last_event = now;
	_pointer_prev_x = root_x;
	_pointer_prev_y = root_y;
	_pointer_prev_mask = mask;
	return true;
    }
    
        
}

