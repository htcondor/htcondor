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

#include "condor_common.h"
#define KBDD
#ifdef WIN32
#include "condor_daemon_core.h"
#include "condor_debug.h"
#include "condor_uid.h"
#else
#include "XInterface.h"
#endif

#include "my_hostname.h"
#include "condor_query.h"
#include "daemon.h"
#include "subsystem_info.h"

#ifdef WIN32
#include <windows.h>
#else
#include <utmp.h>
#include <sys/file.h>
#include <netinet/in.h>
#include <rpc/types.h>
#include <X11/Xlib.h>

XInterface *xinter;
#endif

DECL_SUBSYSTEM( "KBDD", SUBSYSTEM_TYPE_DAEMON );

bool
update_startd()
{
    static SafeSock ssock;
	static bool first_time = true;
	static Daemon startd( DT_STARTD );
	if( first_time ) {
		if( ! startd.locate() ) {
			dprintf( D_ALWAYS, "Can't locate startd, aborting (%s)\n",
					 startd.error() );
			return false;
		}
		if( !ssock.connect(startd.addr(), 0) ) {
			dprintf( D_ALWAYS, "Can't connect to startd at: %s, "
					 "aborting\n", startd.addr() );
			return false;
		}
		first_time = false;
	}
	if( !startd.sendCommand(X_EVENT_NOTIFICATION, &ssock, 3) ) {
		dprintf( D_ALWAYS, "Can't send X_EVENT_NOTIFICATION command "
				 "to startd at: %s, aborting\n", startd.addr() );
		return false;
	}
	dprintf( D_FULLDEBUG, "Sent update to startd at: %s\n", startd.addr() );
	return true;		
}


void 
PollActivity()
{
#ifdef WIN32
	LASTINPUTINFO lii;
	static POINT previous_pos = { 0, 0 }; 
	static DWORD previous_input_tick = 0;

	lii.cbSize = sizeof(LASTINPUTINFO);
	lii.dwTime = 0;

	if ( !GetLastInputInfo(&lii) ) {
		dprintf(D_ALWAYS, "PollActivity: GetLastInputInfo()"
			" failed with err=%d\n", GetLastError());
	}
	else
	{

		//Check if there has been new keyboard input since the last check.
		if(lii.dwTime > previous_input_tick)
		{
			previous_input_tick = lii.dwTime;
			update_startd();
		}

		return;
	}

	//If no change to keyboard input, check if mouse has been moved.
	CURSORINFO cursor_inf;
	cursor_inf.cbSize = sizeof(CURSORINFO);
	if (!GetCursorInfo(&cursor_inf))
	{
		dprintf(D_ALWAYS,"GetCursorInfo() failed (err=%li)\n",
		GetLastError());
	}
	else
	{
		if ((cursor_inf.ptScreenPos.x != previous_pos.x) || 
			(cursor_inf.ptScreenPos.y != previous_pos.y))
		{
			// the mouse has moved!
			// stash new position
			previous_pos.x = cursor_inf.ptScreenPos.x; 
			previous_pos.y = cursor_inf.ptScreenPos.y;
			previous_input_tick = GetTickCount();
			update_startd();
		}
	}

	return;

#else
    if(xinter != NULL)
    {
	if(xinter->CheckActivity())
	{
	    update_startd();
	}
    }
#endif
}

int 
main_shutdown_graceful()
{
#ifndef WIN32
    delete xinter;
#endif
    DC_Exit(EXIT_SUCCESS);
	return TRUE;
}

int 
main_shutdown_fast()
{
	DC_Exit(EXIT_SUCCESS);
	return TRUE;
}

int 
main_config( bool is_full )
{
    return TRUE;
}

int
main_init(int, char *[])
{
    int id;
#ifndef WIN32
	xinter = NULL;
	xinter = new XInterface(id);
#endif
    //Poll for X activity every second.
    id = daemonCore->Register_Timer(5, 5, PollActivity, "PollActivity");

    return TRUE;
}


void
main_pre_dc_init( int, char*[] )
{
}


void
main_pre_command_sock_init( )
{
}

#ifdef WIN32
int WINAPI WinMain( __in HINSTANCE hInstance, __in_opt HINSTANCE hPrevInstance, __in_opt LPSTR lpCmdLine, __in int nShowCmd )
{
	// cons up a "standard" argv for dc_main.
	char **parameters = (char**)malloc(sizeof(char*)*2);
	parameters[0] = "condor_kbdd";
	parameters[1] = NULL;
	dc_main(1, parameters);

	// dc_main should exit() so we probably never get here.
	return 0;
}
#endif

