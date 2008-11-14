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


#define UNICODE
#define _UNICODE

#include <windows.h>
#include <psapi.h>
#include <stdio.h>

#include "condor_softkill.h"

// set to the pid we're trying to softkill
//
static DWORD target_pid;

// flag for whether window has been found
//
static bool window_found = false;

// flag for whether WM_CLOSE was successfully posted
//
static bool message_posted = false;

// a FILE* for debug output
//
static FILE* debug_fp = NULL;

static void
debug(wchar_t* format, ...)
{
	if (debug_fp != NULL) {
		va_list ap;
		va_start(ap, format);
		vfwprintf(debug_fp, format, ap);
		va_end(ap);
	}
}

static BOOL CALLBACK
check_window(HWND wnd, LPARAM = 0)
{
	DWORD window_pid = 0;
	GetWindowThreadProcessId(wnd, &window_pid);
	if (window_pid == target_pid) {

		window_found = true;

		debug(L"posting WM_CLOSE to window owned by thread of pid %u\n", target_pid);

		// if debugging's on, print out the name of the process we're killing
		//
		if (debug_fp != NULL) {
			HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, target_pid);
			if (process == NULL) {
				debug(L"OpenProcess error: %u\n", GetLastError());
				SetLastError(ERROR_SUCCESS);
				return FALSE;
			}
			wchar_t buffer[1024];
			if (GetModuleBaseName(process, NULL, buffer, 1024) == 0) {
				debug(L"GetModuleBaseName error: %u\n", GetLastError());
				SetLastError(ERROR_SUCCESS);
				return FALSE;
			}
			debug(L"executable for process is %s\n", buffer);
		}

		// actually post the WM_CLOSE message
		//
		if (PostMessage(wnd, WM_CLOSE, 0, 0) == TRUE) {
			message_posted = true;
		}
		else {
			debug(L"PostMessage error: %u\n", GetLastError());
		}

		// return FALSE so we stop enumerating
		//
		SetLastError(ERROR_SUCCESS);
		return FALSE;
	}

	return TRUE;
}

static BOOL CALLBACK
check_winsta(wchar_t* winsta_name, LPARAM)
{
	debug(L"entering enum_winsta_proc, winsta_name = %s\n", winsta_name);

	// open the window station and connect to it
	// (TODO: figure out what permissions are really needed here)
	//
	HWINSTA winsta = OpenWindowStation(winsta_name, FALSE, MAXIMUM_ALLOWED);
	if (winsta == NULL) {
		debug(L"OpenWindowStation error: %u\n", GetLastError());
		return TRUE;
	}
	HWINSTA old_winsta = GetProcessWindowStation();
	if (SetProcessWindowStation(winsta) == FALSE) {
		debug(L"SetProcessWindowStation error: %u\n", GetLastError());
		if (CloseWindowStation(winsta) == FALSE) {
			debug(L"CloseWindowStation error: %u\n", GetLastError());
		}
		return TRUE;
	}
	if (CloseWindowStation(old_winsta) == FALSE) {
		debug(L"CloseWindowStation error: %u\n", GetLastError());
	}

	// open the "default" desktop and connect to it (if present)
	// (TODO: figure out what permissions are really needed here)
	//
	HDESK desktop = OpenDesktop(L"default", 0, FALSE, MAXIMUM_ALLOWED);
	if (desktop == NULL) {
		debug(L"OpenDesktop error: %u\n", GetLastError());
		return TRUE;
	}
	HDESK old_desktop = GetThreadDesktop(GetCurrentThreadId());
	if (SetThreadDesktop(desktop) == FALSE) {
		debug(L"SetThreadDesktop error: %u\n", GetLastError());
		if (CloseDesktop(desktop) == FALSE) {
			debug(L"CloseDesktop error: %u\n", GetLastError());
		}
		return TRUE;
	}
	if (CloseDesktop(old_desktop) == FALSE) {
		debug(L"CloseDesktop error: %u\n", GetLastError());
	}

	// it appears that Windows is smart in the way it handles console
	// windows when not on WinSta0: it makes them message-only. while this
	// is probably done to conserve resources, it also means that console
	// windows won't show up when enumerating. thus, we use FindWindowEx
	// to look through the message-only windows
	//
	HWND wnd = NULL;
	while (true) {
		SetLastError(ERROR_SUCCESS);
		wnd = FindWindowEx(HWND_MESSAGE, wnd, L"ConsoleWindowClass", NULL);
		if (wnd == NULL) {
			if (GetLastError() != ERROR_SUCCESS) {
				debug(L"FindWindowEx error: %u\n", GetLastError());
			}
			break;
		}
		check_window(wnd);
		if (window_found) {
			SetLastError(ERROR_SUCCESS);
			return FALSE;
		}
	}

	// see if the process owns a window that lives among the top-level
	// windows on the current desktop
	//
	if ((EnumWindows(check_window, NULL) == FALSE) && (GetLastError() != ERROR_SUCCESS)) {
		debug(L"EnumWindows error: %u\n", GetLastError());
	}
	if (window_found) {
		SetLastError(ERROR_SUCCESS);
		return FALSE;
	}

	// haven't found it yet; keep enumerating
	//
	return TRUE;
}

int WINAPI
wWinMain(HINSTANCE, HINSTANCE, wchar_t*, int)
{
	// usage is:
	//   condor_softkill.exe <target_pid> [debug_output_file]
	//
	// so ensure we have at least one argument
	//
	if (__argc < 2) {
		return SOFTKILL_INVALID_INPUT;
	}

	// get the pid and verify its not bogus input or a zero
	//
	target_pid = _wtoi(__wargv[1]);
	if (target_pid == 0) {
		return SOFTKILL_INVALID_INPUT;
	}

	// see if a debug output file was given
	//
	if (__argc > 2) {
		debug_fp = _wfopen(__wargv[2], L"w");
		if (debug_fp == NULL) {
			return SOFTKILL_INVALID_INPUT;
		}
	}

	// ask windows to enumerate the window stations for us
	//
	if ((EnumWindowStations(check_winsta, NULL) == FALSE) && (GetLastError() != ERROR_SUCCESS)) {
		debug(L"EnumWindowStations error: %u\n", GetLastError());
	}

	if (!window_found) {
		return SOFTKILL_WINDOW_NOT_FOUND;
	}
	else if (!message_posted) {
		return SOFTKILL_POST_MESSAGE_FAILED;
	}
	else {
		return SOFTKILL_SUCCESS;
	}
}