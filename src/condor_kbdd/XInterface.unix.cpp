/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


/***********
 * Tom's list of things to fix in the kbdd
 * 1. Add support for X terms by grabing displays out of utmp
 * 
 ***********/

#define DEFAULT_DISPLAY_NAME ":0.0"

#define _POSIX_SOURCE

#include "XInterface.unix.h"
#include "condor_config.h"
#include "condor_string.h"
//#include <paths.h>
#if USES_UTMPX  /* SGI IRIX 62/65 */
#	include <utmpx.h>
#else 
#	include <utmp.h>
#endif

#include <setjmp.h>

#if defined(LINUX)
	static const char *UtmpName = "/var/run/utmp";
	static const char *AltUtmpName = "/var/adm/utmpx";
#else
	static const char *UtmpName = "/etc/utmpx";
	static const char *AltUtmpName = "/var/adm/utmpx";
#endif

extern void PollActivity();

bool		g_connected;
jmp_buf	 jmp;


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
	char *tmp;
	static int slot = 0;
 
	if(!_tried_root)
	{
		TryUser("root");
		_tried_root = true;
	}
	else if(!_tried_utmp)
	{
		if ( slot > logged_on_users->getlast() ) {
			_tried_utmp = true;
			slot = 0;	// In case we don't actually connect to the X
						// server and we're going to be back.
			dprintf(D_FULLDEBUG, "Tryed all utmp users, "
				"now moving to XAUTHORITY_USERS param.\n");
			return 0;  // Keep trying....
		} 

		TryUser((*logged_on_users)[slot]);
		slot++;
	}
	else  // Try the others
	{
		if(_xauth_users == NULL)
		{
			dprintf(D_FULLDEBUG, "No XAUTHORITY_USERS specified.\n");
			return -1; // No more entries...
		}
		else
		{
			if((tmp = _xauth_users->next()))
			{
				dprintf(D_FULLDEBUG, "Will try XAUTHORITY_USER '%s'\n", 
					tmp);
				TryUser(tmp);
			}
			else
			{
				return -1; // No more entries...
			}
		}
	}
	return 0; 
}

void
XInterface::TryUser(const char *user)
{
	static char env[1024];
	static bool need_uninit = false;
	passwd *passwd_entry;

	passwd_entry = getpwnam(user);
	if(passwd_entry == NULL) {
		// We couldn't find the current user in the passwd file?
		dprintf( D_FULLDEBUG, 
		 	"Current user cannot be found in passwd file.\n" );
		return;
	} else {
		fflush(stdout);
		sprintf(env, "XAUTHORITY=%s/.Xauthority", passwd_entry->pw_dir);
		if(putenv(env) != 0) {
			EXCEPT("Putenv failed!.");
	}

	if ( need_uninit ) {
		uninit_user_ids();
	} else {
		need_uninit = true;
	}

	init_user_ids( user, NULL );

	dprintf( D_FULLDEBUG, "Using %s's .Xauthority: \n", 
		 passwd_entry->pw_name );
	}
}
	

XInterface::XInterface(int id)
{
	char *tmp;
	_daemon_core_timer = id;
	logged_on_users = 0;
	_display_name = NULL;

	// disable bump check by setting move delta to 0
	_small_move_delta = 0;
	_bump_check_after_idle_time_sec = 15*60;


	// We may need access to other user's home directories, so we must run
	// as root.
	
	set_root_priv();
	
	if( geteuid() != 0 ) {
		dprintf(D_FULLDEBUG, "NOTE: Daemon can't use Xauthority if not"
				" running as root.\n");
	}
	
	set_condor_priv();

	g_connected = false;

	tmp = param( "XAUTHORITY_USERS" );
	if(tmp != NULL) {
		_xauth_users = new StringList();
		_xauth_users->initializeFromString( tmp );
		free( tmp );
	} else {
		_xauth_users = NULL;
	}

	_display_name = param( "X_CONSOLE_DISPLAY" );
	
	/* If there's no specified display name, we'll use the default... */
	if (_display_name == NULL) {
	  _display_name = strdup(DEFAULT_DISPLAY_NAME);
	}

	Connect();
}

XInterface::~XInterface()
{
	if(_xauth_users != NULL) {
		delete _xauth_users;
	}

	if(_display_name != NULL) {
	  free(_display_name);
	}

	if ( logged_on_users ) {
		for (int foo =0; foo <= logged_on_users->getlast(); foo++) {
			delete[] (*logged_on_users)[foo];
		}
		delete logged_on_users;
	}
}

bool
XInterface::Connect()
{
	int rtn;
	Window root;
	int s;
	
#if USES_UTMPX
	struct utmpx utmp_entry;
#else
	struct utmp utmp_entry;
#endif

	if ( logged_on_users ) {
		for (int foo =0; foo <= logged_on_users->getlast(); foo++) {
			delete[] (*logged_on_users)[foo];
		}
		delete logged_on_users;
	}

	logged_on_users = new ExtArray< char * >;


	dprintf(D_FULLDEBUG, "XInterface::Connect\n");
	// fopen the Utmp.  If we fail, bail...
	if ((utmp_fp=safe_fopen_wrapper(UtmpName,"r")) == NULL) {
		if ((utmp_fp=safe_fopen_wrapper(AltUtmpName,"r")) == NULL) {                      
			EXCEPT("fopen of \"%s\" (and \"%s\") failed!", UtmpName,
				AltUtmpName);                        
		}                             
	}                                 
 
	while(fread((char *)&utmp_entry,
#if USES_UTMPX
		sizeof( struct utmpx ),
#else
		sizeof( struct utmp ),
#endif
		1, utmp_fp)) {

		if (utmp_entry.ut_type == USER_PROCESS) {
			bool _found_it = false;
			for (int i=0; (i<=logged_on_users->getlast()) && (! _found_it); i++) {
				if (!strcmp(utmp_entry.ut_user, (*logged_on_users)[i])) {
					_found_it = true;
				}
			}
			if (! _found_it) {
				dprintf(D_FULLDEBUG, "User %s is logged in.\n",
					utmp_entry.ut_user );
				(*logged_on_users)[logged_on_users->getlast()+1] =
					strnewp( utmp_entry.ut_user );
			}
		}
	}
	int fclose_ret = fclose( utmp_fp );
 	if( fclose_ret ) {
		EXCEPT("fclose of \"%s\" (or \"%s\") failed! "
			"This message brought to you by the fatal error %d",
			UtmpName, AltUtmpName, errno);
	}


	set_root_priv();

	_tried_root = false;
	_tried_utmp = false;
	if(_xauth_users != NULL) {
		_xauth_users->rewind();
	}


	while(!(_display = XOpenDisplay(_display_name) ))
	{
		set_condor_priv();
	
		rtn = NextEntry();

		if(rtn == -1)
		{
			dprintf(D_FULLDEBUG, "Exausted all possible attempts to "
				"connect to X server, will try again in 60 seconds.\n");
			daemonCore->Reset_Timer( _daemon_core_timer, 60 ,60 );
			dprintf(D_FULLDEBUG, "Reset timer: %d\n", _daemon_core_timer);
	
			g_connected = false;
			set_condor_priv();
			return false;
		}

		// By this point we've gotten are init_user_ids() call called.
		set_user_priv();
	}

	dprintf(D_ALWAYS, "Connected to X server: %s\n", _display_name);
	g_connected = true;

	dprintf(D_FULLDEBUG, "Reset timer: %d\n", _daemon_core_timer);
	daemonCore->Reset_Timer( _daemon_core_timer, 5 ,5 );
	
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

	set_condor_priv();
	return true;
}

bool
XInterface::CheckActivity()
{
	setjmp(jmp);
	if(!g_connected)
	{
		// If we can't connect, we don't know anything....
		if( Connect() == false )
		{
			return false;
		}
	}
	
	bool cursor_active = false;
	bool input_active = ProcessEvents();
	if ( ! input_active)
	{
		// TJ: the old code didn't check for pointer movement when there were events -- but I'm not sure that's the right thing to do.
		cursor_active = QueryPointer();
	}

	if (input_active || cursor_active) {
		dprintf(D_FULLDEBUG,"saw %s\n", input_active ? (cursor_active ? "input and cursor active" : "input active") : (cursor_active ? "cursor active" : "Idle"));
	} else {
		dprintf(D_FULLDEBUG,"saw Idle for %.3f sec\n", (double)time(NULL) - _last_event);
	}
	return input_active || cursor_active;
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

void
XInterface::SetBumpCheck(int delta_move, int delta_time)
{
	_small_move_delta = delta_move;
	_bump_check_after_idle_time_sec = delta_time;
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
		int cx = root_x - _pointer_prev_x, cy = root_y - _pointer_prev_y;
		bool small_move = (cx < _small_move_delta && cx > -_small_move_delta) && (cy < _small_move_delta && cy > -_small_move_delta);

		time(&now);
		time_t sec_since_last_event = now - _last_event;

		bool mouse_active = false;

		// if we have been idle,
		bool bump_check =  small_move && (sec_since_last_event > (_bump_check_after_idle_time_sec));

		dprintf(D_FULLDEBUG,"mouse moved to %d,%d,%x delta is %d,%d %.3f sec%s\n",
				root_x, root_y, mask, cx, cy, (double)sec_since_last_event,
				bump_check ? " performing bump check" : "");

		if (bump_check) {
			sleep(1);
			// check for further mouse movement
			int x = root_x, y = root_y;
			if ( ! XQueryPointer(_display, _pointer_root, &_pointer_root, &dummy_win,  &x, &y, &dummy, &dummy, &mask) ||
				root_x != x || root_y != y) {
				dprintf(D_FULLDEBUG,"not a bump - mouse moved to %d,%d,%x delta is %d,%d\n", x, y, mask, x-root_x, y-root_y);
				mouse_active = true;
				root_x = x; root_y = y;
			} else {
				mouse_active = false;
			}
		} else {
			mouse_active = true;
		}

		_pointer_prev_x = root_x;
		_pointer_prev_y = root_y;
		_pointer_prev_mask = mask;
		if (mouse_active) {
			// Pointer has indeed moved.
			_last_event = now;
		}
		return mouse_active;
	}
}

