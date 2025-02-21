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

#include "XInterface.unix.h"
#include "condor_config.h"
#include <utmp.h>

#ifdef HAVE_XSS
#include "X11/extensions/scrnsaver.h"
#endif

#include <setjmp.h>
#include <filesystem>

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
CatchIOFalseAlarm(Display *)
{
	g_connected = false;
		
	dprintf(D_FULLDEBUG, "Caught IOError\n");

	longjmp(jmp, 0);
	return 0;
}

bool
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
		return false;
	} else {
		sprintf(env, "XAUTHORITY=%s/.Xauthority", passwd_entry->pw_dir);
		if(putenv(env) != 0) {
			EXCEPT("Putenv failed!.");
		}
	}

	if ( need_uninit ) {
		uninit_user_ids();
		need_uninit = false;
	} 

		// passing "root" to init_user_ids is fatal
	if (strcmp(user, "root") == 0) {
		set_root_priv();
	} else {
		if (!init_user_ids( user, NULL )) {
			dprintf(D_ALWAYS, "init_user_ids failed\n");
		}
		set_user_priv();
		need_uninit = true;
	}

	dprintf( D_FULLDEBUG, "Using %s's .Xauthority: \n", passwd_entry->pw_name );
	return true;
}

// Newer display managers have moved the location of the .Xauthority
// file from $HOME into $XDG_RUNTIME_DIR/.xauth_<some_nonce>
bool
XInterface::TryUserXDG(const char *user)
{
	static char env[1024];
	static bool need_uninit = false;
	passwd *passwd_entry;
	std::string xauth_filename_str; 

	passwd_entry = getpwnam(user);
	if(passwd_entry == NULL) {
		// We couldn't find the current user in the passwd file?
		dprintf( D_FULLDEBUG, 
				"Current user cannot be found in passwd file.\n" );
		return false;
	} else {
		std::string xdg_runtime_dir_str;

		// find the first file named .xauth_<something> in XDG_RUNTIME_DIR
		formatstr(xdg_runtime_dir_str, "/var/run/user/%d/", passwd_entry->pw_uid);
		std::filesystem::path xdg_runtime_dir {xdg_runtime_dir_str};
		for (const auto &entry: std::filesystem::directory_iterator(xdg_runtime_dir)) {
			if (entry.path().filename().string().starts_with("xauth_")) {
				dprintf(D_FULLDEBUG, "Trying xauth file %s\n", entry.path().string().c_str());
				xauth_filename_str = entry.path().string();
				break;
			}
		}
		sprintf(env, "XAUTHORITY=%s", xauth_filename_str.c_str());
		if(putenv(env) != 0) {
			EXCEPT("Putenv failed!.");
		}
	}

	if ( need_uninit ) {
		uninit_user_ids();
		need_uninit = false;
	} 

	// passing "root" to init_user_ids is fatal
	if (strcmp(user, "root") == 0) {
		set_root_priv();
	} else {
		if (!init_user_ids( user, NULL )) {
			dprintf(D_ALWAYS, "init_user_ids failed\n");
		}
		set_user_priv();
		need_uninit = true;
	}

	dprintf( D_FULLDEBUG, "Using %s's .Xauthority: %s \n", passwd_entry->pw_name, xauth_filename_str.c_str());
	return true;
}

XInterface::XInterface(int id)
{
	_daemon_core_timer = id;
	logged_on_users = nullptr;
	_display_name = nullptr;

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

	_display_name = param( "X_CONSOLE_DISPLAY" );
	
	/* If there's no specified display name, we'll use the default... */
	if (_display_name == NULL) {
	  _display_name = strdup(DEFAULT_DISPLAY_NAME);
	}

	Connect();
}

XInterface::~XInterface()
{
	if(_display_name != NULL) {
	  free(_display_name);
	}

	if ( logged_on_users ) {
		for (size_t foo =0; foo < logged_on_users->size(); foo++) {
			free((*logged_on_users)[foo]);
		}
		delete logged_on_users;
	}
}

void
XInterface::ReadUtmp() {
	struct utmp utmp_entry;

	if ( logged_on_users ) {
		for (size_t foo =0; foo < logged_on_users->size(); foo++) {
			free((*logged_on_users)[foo]);
		}
		delete logged_on_users;
	}

	logged_on_users = new std::vector< char * >;


	// fopen the Utmp.  If we fail, bail...
	if ((utmp_fp=safe_fopen_wrapper(UtmpName,"r")) == NULL) {
		if ((utmp_fp=safe_fopen_wrapper(AltUtmpName,"r")) == NULL) {                      
			EXCEPT("fopen of \"%s\" (and \"%s\") failed!", UtmpName,
				AltUtmpName);                        
		}                             
	}                                 
 
	while(fread((char *)&utmp_entry,
		sizeof( struct utmp ),
		1, utmp_fp)) {

		if (utmp_entry.ut_type == USER_PROCESS) {
			bool _found_it = false;
			char user[UT_NAMESIZE + 1];
			memcpy(user, utmp_entry.ut_user, UT_NAMESIZE);
			user[UT_NAMESIZE] = 0;
			for (size_t i=0; (i<logged_on_users->size()) && (! _found_it); i++) {
				if (!strcmp(user, (*logged_on_users)[i])) {
					_found_it = true;
				}
			}
			if (! _found_it) {
				dprintf(D_FULLDEBUG, "User %s is logged in.\n",
					user );
				logged_on_users->push_back(strdup(user));
			}
		}
	}
	int fclose_ret = fclose( utmp_fp );
 	if( fclose_ret ) {
		EXCEPT("fclose of \"%s\" (or \"%s\") failed! "
			"This message brought to you by the fatal error %d",
			UtmpName, AltUtmpName, errno);
	}

	return;
}

bool
XInterface::Connect()
{
	dprintf(D_FULLDEBUG, "XInterface::Connect\n");


	// First try as whatever user we entered as, with whatever
	// X credentials we were born with.

	dprintf(D_FULLDEBUG, "Trying to XOpenDisplay\n");
	if ((_display = XOpenDisplay(_display_name))) {
		FinishConnection();
		return true;
	}

	// Ok, that didn't work, try as condor
	set_condor_priv();

	dprintf(D_FULLDEBUG, "Trying to XOpenDisplay as condor\n");
	if ((_display = XOpenDisplay(_display_name))) {
		FinishConnection();
		return true;
	}

	// Time for the big guns, try as root
	set_root_priv();

	dprintf(D_FULLDEBUG, "Trying to XOpenDisplay as root\n");
	if ((_display = XOpenDisplay(_display_name))) {
		FinishConnection();
		return true;
	}

	set_condor_priv();

	
	// If we get here, let's try as each logged in user
	// If this X server is using MIT-MAGIC_COOKIE, set
	// XAUTHORITY to point within the user's home directory.

	ReadUtmp();

	size_t utmpIndex = 0;
	while (utmpIndex < logged_on_users->size()) {

		const char *username = (*logged_on_users)[utmpIndex];

		dprintf(D_FULLDEBUG, "Trying to XOpenDisplay as user %s\n", username);
		bool switched = TryUser(username); // set_priv's and setenv's

		if (switched) {
			_display = XOpenDisplay(_display_name);		

			if (_display) {
				FinishConnection();
				return true;
			}
		}

		bool xdgfound = TryUserXDG(username);
		if (xdgfound) {
			_display = XOpenDisplay(_display_name);		

			if (_display) {
				FinishConnection();
				return true;
			}
		}

		set_condor_priv();
		utmpIndex++;
	}

	dprintf(D_FULLDEBUG, "Exausted all possible attempts to "
		"connect to X server, will try again in 60 seconds.\n");
	daemonCore->Reset_Timer( _daemon_core_timer, 60 ,60 );
	dprintf(D_FULLDEBUG, "Reset timer: %d\n", _daemon_core_timer);

	g_connected = false;
	return false;
}

void
XInterface::FinishConnection() { // not to be confused with the FinnishConnection
	Window root;
	
	dprintf(D_ALWAYS, "Connected to X server: %s\n", _display_name);
	g_connected = true;

	dprintf(D_FULLDEBUG, "Reset timer: %d\n", _daemon_core_timer);
	daemonCore->Reset_Timer( _daemon_core_timer, 5 ,5 );
	
	// See note above the function to see why we need to do this.
	XSetErrorHandler((XErrorHandler) CatchFalseAlarm);
	XSetIOErrorHandler((XIOErrorHandler) CatchIOFalseAlarm);

	//Select the events on each root window of each screen
	for (int s = 0; s < ScreenCount(_display); s++)
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

	// Newly connected display needs to see if it has the extension
	needsCheck = true;
	hasXss = false; 
	
	// unset env needed here?
	set_condor_priv();
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
	bool xss_active = false;

	bool input_active = ProcessEvents();
	if ( ! input_active)
	{
		// TJ: the old code didn't check for pointer movement when there were events -- but I'm not sure that's the right thing to do.
		cursor_active = QueryPointer();
		xss_active = QuerySSExtension();
	}

	if (input_active || cursor_active || xss_active) {
		if (input_active) {
			dprintf(D_FULLDEBUG,"saw input_active\n");
		}
		if (cursor_active) {
			dprintf(D_FULLDEBUG,"saw cursor active\n");
		}
		if (xss_active) {
			dprintf(D_FULLDEBUG,"screensaver reported recent activity\n");
		}
	} else {
		dprintf(D_FULLDEBUG,"saw Idle for %.3f sec\n", (double)time(NULL) - _last_event);
	}
	return input_active || cursor_active || xss_active;
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

bool
XInterface::QuerySSExtension()
{
#ifdef HAVE_XSS
	static XScreenSaverInfo *xssi = 0;

	if (needsCheck) {
		needsCheck = false;

		int notused = 0;
		hasXss = XScreenSaverQueryExtension(_display, &notused, &notused);
		dprintf(D_ALWAYS, "X server %s have the screen saver extension\n", hasXss ? "does" : "does not");
	}

	if (hasXss) {
		if (!xssi) {
			xssi = XScreenSaverAllocInfo();
		}

		XScreenSaverQueryInfo(_display,DefaultRootWindow(_display), xssi);

		dprintf(D_FULLDEBUG, "Screen Saver extension claims idle time is %ld ms\n", xssi->idle);

		if (xssi->idle < 20000l) {
			return true;
		}
	
	}
		return false;

#else
return false;
#endif
}
