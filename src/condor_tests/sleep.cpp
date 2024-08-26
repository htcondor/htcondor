/***************************************************************
 *
 * Copyright (C) 2023, John M. Knoeller
 * Condor Team, Computer Sciences Department
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

// this file contains a Windows specific sleep program that builds using MSVC
// but does not use the c-runtime so that it is small and portable.

#define NO_BPRINT_BUFFER 1
#include "tiny_winapi.h"

// Build this file using the following command line for a console app
//    cl /O1 /GS- sleep.cpp /link
// And this command line for a windows app
//    cl /O1 /GS- /DNOCONSOLE=1 sleep.cpp /link /out:sleepw.exe

#ifdef NOCONSOLE
 #define BUILD_MODULE_STRING "sleepw"
#else
 #define BUILD_MODULE_STRING "sleep"
#endif
#define BUILD_VERSION_STRING "1.1.0"


// return true if a file exists, false if it does not
bool check_for_file(wchar_t * killfile)
{
	UINT access = READ_META;
	HANDLE hf = CreateFileW(killfile, access, FILE_SHARE_ALL, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hf == INVALID_HANDLE_VALUE) {
		return false;
	}
	CloseHandle(hf);
	return true;
}

#ifdef NOCONSOLE
extern "C" LRESULT __stdcall msg_window_proc(HWND hwnd, UINT idMsg, WPARAM wParam, LPARAM lParam)
{
	if (idMsg == WM_CLOSE) { ExitProcess(1); }
	return DefWindowProcW(hwnd, idMsg, wParam, lParam);
}
extern "C" LRESULT __stdcall msg_window_proc_no_close(HWND hwnd, UINT idMsg, WPARAM wParam, LPARAM lParam)
{
	if (idMsg == WM_CLOSE) { return 0; }
	return DefWindowProcW(hwnd, idMsg, wParam, lParam);
}

void pump_messages_with_timeout(unsigned int ms, int no_close, wchar_t * killfile)
{
	unsigned __int64 begin_time = GetTickCount64();

	WNDCLASSEXW wc;
	memset((char*)&wc, 0, sizeof(wc));
	wc.cb = sizeof(wc);
	wc.style = 0; //CS_GLOBALCLASS;
	if (no_close) {
		wc.pfnWndProc = msg_window_proc_no_close;
	} else {
		wc.pfnWndProc = msg_window_proc;
	}
	wc.pszClassName = L"ConsoleWindowClass";
	wc.hInstance = GetModuleHandleW(NULL);
	ATOM atm = RegisterClassExW(&wc);
	if ( ! atm) {
		UINT err = GetLastError();
		char buf[100], *p = append_unsafe(buf, "RegisterClass failed err="); p = append_num(p, err);
		MessageBoxExA(NULL, buf, "Error", MB_OK | MB_ICONINFORMATION, 0);
		ExitProcess(1);
	}

	UINT exstyle = 0, style = 0;
	HWND hwnd = CreateWindowExW(exstyle, L"ConsoleWindowClass", L"", style, 0,0,0,0, HWND_MESSAGE, NULL, wc.hInstance, NULL);
	if ( ! hwnd) {
		UINT err = GetLastError();
		char buf[100], *p = append_unsafe(buf, "CreateWindow failed err="); p = append_num(p, err);
		MessageBoxExA(NULL, buf, "Error", MB_OK | MB_ICONINFORMATION, 0);
		ExitProcess(1);
	}

	HANDLE hMe = GetCurrentProcess();
	HANDLE hProc = hMe;
	DuplicateHandle(hMe, hMe, hMe, &hProc, 0, 0, DUPLICATE_SAME_ACCESS);

	for (;;) {
		if (killfile && check_for_file(killfile)) {
			break;
		}
		unsigned __int64 now = GetTickCount64();
		unsigned __int64 elapsed = now - begin_time;
		if (elapsed >= ms) break;
		unsigned int wait_time = ms - (int)elapsed;
		if (killfile) {
			if (wait_time > 1000) wait_time = 1000;
		}

		UINT ix = MsgWaitForMultipleObjects(1, &hProc, false, wait_time, QS_ALLINPUT);
		if (ix == 1) {
			MSG msg;
			while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
				if (msg.id == WM_QUIT)
					break;
				DispatchMessageW(&msg);
			}
		}
	};
}
#endif

extern "C" void __cdecl begin( void )
{
    int multi_link_only = 0;
    int show_usage = 0;
    int dash_quiet = 0;
    int no_close = 0;
    int was_arg = 0;
    int next_arg_is = 0; // 'k' = killfile
    wchar_t * killfile = NULL;

#ifdef NOCONSOLE
	dash_quiet = 1;
#else
	HANDLE hStdOut = GetStdHandle(STD_OUT_HANDLE);
#endif

	const char * ws = " \t\r\n";
	const wchar_t * pcmdline = next_token(GetCommandLineW(), ws); // get command line and skip the command name.
	while (*pcmdline) {
		int cchArg;
		const wchar_t * pArg;
		const wchar_t * pnext = next_token_ref(pcmdline, ws, pArg, cchArg);
		if (next_arg_is) {
			switch (next_arg_is) {
			case 'k':
				killfile = AllocCopyZ(pArg, cchArg);
				break;
			default: show_usage = 1; break;
			}
			next_arg_is = 0;
		} else if (*pArg == '-' || *pArg == '/') {
			const wchar_t * popt = pArg+1;
			for (int ii = 1; ii < cchArg; ++ii) {
				wchar_t opt = pArg[ii];
				switch (opt) {
				 case 'h': show_usage = 1; break;
				 case '?': show_usage = 1; break;
				 case 'q': dash_quiet = 1; break;
				 case 'k': next_arg_is = opt; break;
				#ifdef NOCONSOLE
				 case 'u': no_close   = 1; break;
				#endif
				}
			}
		} else if (*pArg) {
			was_arg = 1;
			unsigned int ms;
			const wchar_t * psz = parse_uint(pcmdline, &ms);
			unsigned int units = 1000;
			if (*psz && (psz - pcmdline) < cchArg) {
				// argument may have a postfix units value
				switch(*psz) {
					case 's': units = 1000; break; // seconds
					case 'm': units = 1; if (psz[1] == 's') break;    // millisec
					case 'M': units = 1000 * 60; break; // minutes
					case 'H': units = 1000 * 60 * 60; break; //hours
					default: units = 0; show_usage = true; break;
				}
			}

			ms *= units;
			if (ms) {
#ifdef NOCONSOLE
				if ((ms <= 1000)) {
					Sleep(ms);
				} else {
					pump_messages_with_timeout(ms, no_close, killfile);
				}
#else
				if ( ! dash_quiet) {
				    char buf[100], *p = append_unsafe(buf, "Sleeping for "); p = append_num(p, ms/1000);
				    if (ms%1000) { p = append_unsafe(p, "."); p = append_num(p, ms%1000, false, 3, '0'); }
				    p = append_unsafe(p, " seconds...");
				    Print(hStdOut, buf, p);
				}
				if (killfile) {
					unsigned __int64 begin = GetTickCount64();
					unsigned __int64 quit_time = begin + ms;
					for (;;) {
						if (check_for_file(killfile)) {
							if ( ! dash_quiet) { Print(hStdOut, "got killfile", 12); }
							break;
						}
						unsigned __int64 now = GetTickCount64();
						if (now >= quit_time)
							break;
						unsigned __int64 interval = quit_time - now;
						if (interval > 1000) { interval = 1000; }
						Sleep((unsigned int)interval);
					}
				} else {
					Sleep(ms);
				}
				if ( ! dash_quiet) { Print(hStdOut, "\r\n", 2); }
#endif
			}
		}
		pcmdline = pnext;
	}

	if (show_usage || ! was_arg) {
#ifdef NOCONSOLE
		MessageBoxExA(NULL,
#else
		Print(hStdOut,
			BUILD_MODULE_STRING " v" BUILD_VERSION_STRING " " BUILD_ARCH_STRING "  Copyright 2023 HTCondor/John M. Knoeller\r\n"
#endif
			"\r\nUsage: sleep [options] <time>[ms|s|M]\r\n"
			"    sleeps for <time>, if <time> is followed by a units specifier it is treated as:\r\n"
			"    ms  time value is in milliseconds\r\n"
			"    s   time value is in seconds. this is the default\r\n"
			"    M   time value is in minutes.\r\n"
			"    H   time value is in hours.\r\n"
			" [options] is one or more of\r\n"
			"   -h        print usage (this output)\r\n"
			"   -k <file> poll for <file> and exit if it exists.\r\n"
#ifdef NOCONSOLE
			"   -u        ignore WM_CLOSE message\r\n"
			" this is a Windows app with a non-visible window that will exit early when sent a WM_CLOSE message.\r\n"
#else
			"   -q        quiet (does not report sleep time)\r\n"
#endif
			"\r\n",
#ifdef NOCONSOLE
			BUILD_MODULE_STRING " v" BUILD_VERSION_STRING " " BUILD_ARCH_STRING "  Copyright 2023 HTCondor/John M. Knoeller",
			MB_OK | MB_ICONINFORMATION, 0);
#else
			-1);
#endif
	}

	ExitProcess(0);
}
