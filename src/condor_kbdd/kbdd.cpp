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
#pragma comment( linker, "/subsystem:windows" )
#include "condor_daemon_core.h"
#include "condor_debug.h"
#include "condor_uid.h"
#else
#include "XInterface.unix.h"
#endif

#include "condor_config.h"
#include "condor_query.h"
#include "daemon.h"
#include "subsystem_info.h"

#ifdef WIN32
#include <windows.h>
#else
#include <utmp.h>
#include <sys/file.h>
#include <netinet/in.h>
#include <X11/Xlib.h>

XInterface *xinter = NULL;
#endif

#ifdef WIN32
static void hack_kbdd_registry();
#endif

MSC_DISABLE_WARNING(6262) // warning: Function uses 60K of stack
bool
update_startd()
{
	static Daemon startd( DT_STARTD );
	static ReliSock * rsock = NULL;

	if( rsock == NULL ) {
		// We can't continue to use the original Daemon object, if the
		// reason we lost our connection is because the startd's address
		// changed (e.g., was restarted).  We need to reconstruct on every
		// reconnection attempt, because the address may not have been
		// updated since the connection was lost (e.g., the startd just died).
		startd = Daemon( DT_STARTD );
		rsock = new ReliSock();

		if( ! startd.locate() ) {
			dprintf( D_ALWAYS, "Can't locate startd, aborting (%s)\n",
				startd.error() );
			return false;
		}

		if(! rsock->connect( startd.addr(), 0  )) {
			dprintf( D_ALWAYS, "Can't connect to startd at: %s, "
				"aborting\n", startd.addr() );
			return false;
		}
	}

	if( !startd.sendCommand(X_EVENT_NOTIFICATION, rsock, 3) ) {
		dprintf( D_ALWAYS, "Can't send X_EVENT_NOTIFICATION command "
				 "to startd at: %s, aborting\n", startd.addr() );
		rsock->close();
		delete rsock;
		rsock = NULL;
		return false;
	}
	dprintf( D_FULLDEBUG, "Sent update to startd at: %s\n", startd.addr() );

	return true;
}
MSC_RESTORE_WARNING(6262) // warning: Function uses 60K of stack

static int  small_move_delta = 16;
static int  bump_check_after_idle_time_sec = 15*60;

#ifdef WIN32

// determine the elapsed time between two calls to GetTickCount()
// taking into account that it can roll over every 49 days
// the return value will be the delta time, but it will never exceed MAXDWORD
DWORD TickCountDelta(DWORD later, DWORD earlier, bool * prollever)
{
	DWORD ret;
	bool rollover = (later < earlier) && (earlier > 0x80000000);
	if (rollover) {
		long long later64 = later + 0x100000000i64;
		long long diff = later - (long long)earlier;
		if (diff > MAXDWORD) { ret = MAXDWORD; }
		else { ret = (DWORD)diff; }
	} else {
		ret = later - earlier;
	}
	return ret;
}

bool CheckActivity()
{
	static POINT previous_pos = { 0, 0 };
	static DWORD previous_input_tick = 0;
	static DWORD previous_cursor_tick = 0;

	bool input_active = false;  // GetLastInputInfo is both keyboard and mouse
	bool cursor_active = false; // true if only the cursor has moved..

	LASTINPUTINFO lii;
	lii.cbSize = sizeof(LASTINPUTINFO);
	lii.dwTime = 0;

	if ( !GetLastInputInfo(&lii) ) {
		dprintf(D_ALWAYS, "PollActivity: GetLastInputInfo() failed with err=%d\n", GetLastError());
	}
	else
	{
		// Check if there has been input (keyboard or mouse) since the last check.
		if(lii.dwTime != previous_input_tick)
		{
			previous_input_tick = lii.dwTime;
			input_active = true;
		}
	}

	// try and determine if the input is ONLY mouse movement.
	CURSORINFO ci;
	ci.cbSize = sizeof(CURSORINFO);
	if (!GetCursorInfo(&ci))
	{
		dprintf(D_ALWAYS,"GetCursorInfo() failed (err=%li)\n", GetLastError());
	}
	else
	{
		if ((ci.ptScreenPos.x != previous_pos.x) ||
			(ci.ptScreenPos.y != previous_pos.y))
		{
			int cx = ci.ptScreenPos.x - previous_pos.x, cy = ci.ptScreenPos.y - previous_pos.y;
			bool small_move = (cx < small_move_delta && cx > -small_move_delta) && (cy < small_move_delta && cy > -small_move_delta);
			DWORD now = input_active ? lii.dwTime : GetTickCount();

			// figure out the time since last mousemove. because GetTickCount() can rolloever ever 49 days
			// we need to reset the previous_mouse_tick to 0 when we detect the rollover and it has
			// gone sufficiently far over so that even after the reset the delta will still be large.
			bool rollover = false;
			DWORD ms_since_last_move = TickCountDelta(now, previous_cursor_tick, &rollover);
			if (rollover && (now > 0x10000000)) { previous_cursor_tick = 0; }

			// the mouse has moved!
			// stash new position
			previous_pos = ci.ptScreenPos;

			// if there was no keyboard activity, and the mouse was previously idle for at least 15 minutes
			// do a bump check - i.e. don't report mouse activity unless we see more activity soon.
			bool bump_check =  small_move && (ms_since_last_move > ((DWORD)bump_check_after_idle_time_sec*1000));

			dprintf(D_FULLDEBUG,"mouse moved to %d,%d delta is %d,%d %.3f sec%s\n",
				ci.ptScreenPos.x, ci.ptScreenPos.y, cx, cy, ms_since_last_move/1000.0,
				bump_check ? " performing bump check" : "");

			if (bump_check) {
				Sleep(1000);
				GetCursorInfo(&ci);
				if (ci.ptScreenPos.x != previous_pos.x || ci.ptScreenPos.y != previous_pos.y) {
					// cursor moved again, this is real.
					cursor_active = true;
				} else if (input_active) {
					// check input info again.  if it hasn't changed, assume that the cursor movement we saw is
					// the reason for the input info change we saw above.
					GetLastInputInfo(&lii);
					if (lii.dwTime == now) {
						input_active = false;
					}
				}
			} else {
				// we are reporting active anyway because of keyboard or because of recent mouse activity
				// so we don't care if this is 'real' cursor movement or accidental
				cursor_active = cx || cy;
			}
		}
	}

	if (input_active || cursor_active) {
		previous_cursor_tick = previous_input_tick;
		dprintf(D_FULLDEBUG,"saw %s\n", input_active ? (cursor_active ? "input and cursor active" : "input active") : (cursor_active ? "cursor active" : "Idle"));
	} else {
		dprintf(D_FULLDEBUG,"saw Idle for %.3f sec\n", (GetTickCount() - lii.dwTime) / 1000.0);
	}
	return input_active || cursor_active;
}
#endif

void
PollActivity(int /* tid */)
{
#ifdef WIN32
	if (CheckActivity()) {
		update_startd();
	}
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

void 
main_shutdown_graceful()
{
#ifndef WIN32
    delete xinter;
#endif
    DC_Exit(EXIT_SUCCESS);
}

void 
main_shutdown_fast()
{
	DC_Exit(EXIT_SUCCESS);
}

void 
main_config()
{
}

void
main_init(int, char *[])
{
#ifndef WIN32
	xinter = NULL;
#endif

	small_move_delta = param_integer("KBDD_BUMP_CHECK_SIZE", small_move_delta);
	bump_check_after_idle_time_sec = param_integer("KBDD_BUMP_CHECK_AFTER_IDLE_TIME", bump_check_after_idle_time_sec);

    //Poll for X activity every second.
    int id = daemonCore->Register_Timer(5, 5, PollActivity, "PollActivity");
#ifndef WIN32
	xinter = new XInterface(id);
	if (xinter) { xinter->SetBumpCheck(small_move_delta, bump_check_after_idle_time_sec); }
#endif
}

// on ! Windows, this is just called by main,
// on Windows, WinMain builds an argc,argv, 
// does some Windows specific initialization, then calls this
//
// TODO: make kbdd a simple program and not daemon core, then
// we can ditch this.
//
int
daemon_main( int argc, char **argv )
{
	set_mySubSystem( "KBDD", true, SUBSYSTEM_TYPE_DAEMON );

	dc_main_init = main_init;
	dc_main_config = main_config;
	dc_main_shutdown_fast = main_shutdown_fast;
	dc_main_shutdown_graceful = main_shutdown_graceful;
	return dc_main( argc, argv );
}

#ifdef WIN32
int WINAPI WinMain( __in HINSTANCE hInstance, __in_opt HINSTANCE hPrevInstance, __in LPSTR lpCmdLine, __in int nShowCmd )
{
	// t1031 - tell dprintf not to exit if it can't write to the log
	// we have to do this before dprintf_config is called
	// (which happens inside dc_main), otherwise KBDD on Win32 will
	// except in dprintf_config if the log directory isn't writable
	// by the current user.
	dprintf_config_ContinueOnFailure( TRUE );

	// cons up a "standard" argc,argv for daemon_main.

	/*
	Due to the risk of spaces in paths on Windows, we use the function
	CommandLineToArgvW to extract a list of arguments instead of parsing
	the string using a delimiter.
	*/
	LPWSTR cmdLine = GetCommandLineW();	 // do not free this pointer!
	if(!cmdLine)
	{
		return GetLastError();
	}

	int cArgs = 0;
	LPWSTR* cmdArgs = CommandLineToArgvW(cmdLine, &cArgs);
	if(!cmdArgs)
	{
		return GetLastError();
	}
	char **parameters = (char**)malloc(sizeof(char*)*cArgs + 1);
	ASSERT( parameters != NULL );
	parameters[0] = (char *)"condor_kbdd";
	parameters[cArgs] = NULL;

	/*
	List of strings is in unicode so we need to downconvert it into ascii strings.
	*/
	for(int counter = 1; counter < cArgs; ++counter)
	{
		//There's a *2 on the size to provide some leeway for non-ascii characters being split.  Suggested by TJ.
		int argSize = ((wcslen(cmdArgs[counter]) + 1) * sizeof(char)) * 2;
		parameters[counter] = (char*)malloc(argSize);
		ASSERT( parameters[counter] != NULL );
		int converted = WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK, cmdArgs[counter], -1, parameters[counter], argSize, NULL, NULL);
		if(converted == 0)
		{
			return GetLastError();
		}
	}
	LocalFree((HLOCAL)cmdArgs);

	// check to see if we are running as a service, and if we are
	// add a Run key value to the registry for HKLM so that the kbdd
	// will run as the user whenever a user logs on.
	hack_kbdd_registry();

	// call the daemon main function which is common between Windows and ! Windows.
	return daemon_main(cArgs, parameters);
}

static void hack_kbdd_registry()
{
	HKEY hStart;

	BOOL isService = FALSE;
	SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
	PSID ServiceGroup;
	isService = AllocateAndInitializeSid(
		&ntAuthority,
		1,
		SECURITY_LOCAL_SYSTEM_RID,
		0,0,0,0,0,0,0,
		&ServiceGroup);
	if(isService)
	{
		if(!CheckTokenMembership(NULL, ServiceGroup, &isService))
		{
			dprintf(D_ALWAYS, "Failed to check token membership.\n");
			isService = FALSE;
		}

		FreeSid(ServiceGroup);
	}

	if(isService)
	{
		LONG regResult = RegOpenKeyEx(
			HKEY_LOCAL_MACHINE,
			"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
			0,
			KEY_READ,
			&hStart);

		if(regResult != ERROR_SUCCESS)
		{
			dprintf(D_ALWAYS, "ERROR: Failed to open registry for checking: %d\n", regResult);
		}
		else
		{
			regResult = RegQueryValueEx(
				hStart,
				"CONDOR_KBDD",
				NULL,
				NULL,
				NULL,
				NULL);

			RegCloseKey(hStart);
			if(regResult == ERROR_FILE_NOT_FOUND)
			{
				regResult = RegOpenKeyEx(
					HKEY_LOCAL_MACHINE,
					"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
					0,
					KEY_SET_VALUE,
					&hStart);

				if(regResult != ERROR_SUCCESS)
				{
					dprintf(D_DAEMONCORE, "Error: Failed to open login startup registry key for writing: %d\n", regResult);
					return;
				}
				else
				{
					char* kbddPath = (char*)malloc(sizeof(char)*(MAX_PATH+1));
					if(!kbddPath)
					{
						dprintf(D_ALWAYS, "Error: Unable to find path to KBDD executable.\n");
						RegCloseKey(hStart);
						return;
					}
					int pathSize = GetModuleFileName(NULL, kbddPath, MAX_PATH);
					if(pathSize < MAX_PATH)
					{
						regResult = RegSetValueEx(hStart,
							"CONDOR_KBDD",
							0,
							REG_SZ,
							(byte*)kbddPath,
							(pathSize + 1)*sizeof(char));
					}
					free(kbddPath);

					RegCloseKey(hStart);
				}
			} //if(regResult == ERROR_FILE_NOT_FOUND)
		} //if(regResult != ERROR_SUCCESS)
	} //if(isService)
}

#else // ! WIN32

int
main( int argc, char **argv )
{
	return daemon_main(argc, argv);
}

#endif // WIN32

