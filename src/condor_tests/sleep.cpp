/***************************************************************
 *
 * Copyright (C) 2015, John M. Knoeller 
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

// disable all of the code checks since they emit code that refers to the c-runtime.
#pragma strict_gs_check(off)
#pragma runtime_checks("scu", off)

// Force the linker to include KERNEL32.LIB and to NOT include the c-runtime
#pragma comment(linker, "/nodefaultlib")
#pragma comment(linker, "/defaultlib:kernel32.lib")
#ifdef NOCONSOLE
#pragma message("building NOCONSOLE")
#pragma comment(linker, "/subsystem:windows")
#pragma comment(linker, "/defaultlib:user32.lib")
#else
#pragma comment(linker, "/subsystem:console")
#endif
#pragma comment(linker, "/entry:begin")

// Build this file using the following command line.
//cl /O1 /GS- sleep.cpp /link /subsystem:console kernel32.lib

// fake the security cookie stuff that the compiler emits unless we pass /GS- on the compile line
// even though we turn off strict_gs_check above.
// 
extern "C" int __security_cookie = 0xBAAD;
extern "C" void _fastcall __security_check_cookie(int cookie) { };

// Note: this code should compile without linking to the c-runtime at all
// it is entirely self-contained other than windows api calls
// because of this, care must be taken to avoid c++ constucts that require
// the c-runtime, this includes avoiding 64 bit multiply/divide.

typedef void* HANDLE;
typedef void* HMODULE;
typedef void* HWND;
typedef void* HLOCAL;
typedef int   BOOL;
typedef unsigned int UINT;
typedef struct _FILETIME { unsigned __int64 DateTime; } FILETIME; //
struct _qword {
   unsigned int lo; unsigned int hi;
   _qword(int l) : lo(l), hi((l < 0) ? -1 : 0) {}
   _qword(unsigned int l) : lo(l), hi(0) {}
   _qword(unsigned int l, unsigned int h) : lo(l), hi(h) {}
   _qword(__int64 qw) : lo((unsigned int)qw), hi((unsigned int)((unsigned __int64)qw >> 32)) {}
   _qword(unsigned __int64 qw) : lo((unsigned int)qw), hi((unsigned int)(qw >> 32)) {}
   operator __int64() { return *(__int64*)&lo; }
   operator unsigned __int64() { return *(unsigned __int64*)&lo; }
};

enum {
	STD_IN_HANDLE = -10,
	STD_OUT_HANDLE = -11,
	STD_ERR_HANDLE = -12
};
#define INVALID_HANDLE_VALUE (HANDLE)-1
#define NULL 0
#define NUMELMS(aa) (int)(sizeof(aa)/sizeof((aa)[0]))

#define READ_CONTROL 0x00020000
#define PROCESS_QUERY_INFORMATION 0x0400
#define PROCESS_QUERY_LIMITED_INFORMATION 0x1000

extern "C" void __stdcall ExitProcess(unsigned int uExitCode);
extern "C" HANDLE __stdcall OpenProcess(unsigned int access, int inherit, unsigned int pid);
extern "C" BOOL __stdcall CloseHandle(HANDLE h);
extern "C" HANDLE __stdcall GetCurrentProcess(void);
extern "C" unsigned int __stdcall GetCurrentProcessId(void);
extern "C" unsigned int __stdcall GetLastError(void);
extern "C" unsigned int __stdcall Sleep(unsigned int millisec);
extern "C" HANDLE __stdcall GetStdHandle(int idHandle);
extern "C" const wchar_t * __stdcall GetCommandLineW(void);
extern "C" BOOL __stdcall WriteFile(HANDLE hFile, char * buffer, unsigned int cbBuffer, unsigned int * pcbWritten, void* over);
extern "C" unsigned __int64 __stdcall GetTickCount64();

extern "C" BOOL __stdcall DuplicateHandle(HANDLE hSrcProc, HANDLE hi, HANDLE hDstProc, HANDLE *ho, UINT access, BOOL inherit, UINT options);
#define DUPLICATE_CLOSE_SOURCE 1
#define DUPLICATE_SAME_ACCESS 2

#define MB_OK                0
#define MB_OKCANCEL          1
#define MB_ABORTRETRYIGNORE  2
#define MB_YESNOCANCEL       3
#define MB_YESNO             4
#define MB_RETRYCANCEL       5
#define MB_CANCELTRYCONTINUE 6
#define MB_ICONERROR       0x10 // stop icon
#define MB_ICONQUESTION    0x20 // ?
#define MB_ICONEXCLAMATION 0x30 // !
#define MB_ICONINFORMATION 0x40 // (i) icon
#define MB_DEFBUTTON2      0x100
#define MB_DEFBUTTON3      0x200
#define MB_DEFBUTTON4      0x300
#define MB_SYSTEMMODAL     0x1000 // topmost
#define MB_TASKMODAL       0x2000 // disables all app toplevel windows, even when passed hwnd is NULL
#define MB_SERVICENOTIFICATION 0x00200000 // display on current active desktop, hwnd MUST BE NULL
extern "C" int __stdcall MessageBoxExA(HWND hwnd, const char * text, const char * caption, UINT mb_flags, unsigned short wLangId);
// return values
#define IDOK     1
#define IDCANCEL 2
#define IDABORT  3
#define IDRETRY  4
#define IDIGNORE 5
#define IDYES    6
#define IDNO     7
#define IDTRYAGAIN 10
#define IDCONTINUE 11


#define LMEM_ZERO 0x40
#define LMEM_HANDLE 0x02
extern "C" HLOCAL __stdcall LocalAlloc(unsigned int flags, size_t cb);
extern "C" HLOCAL __stdcall LocalFree(HLOCAL hmem);

static void* Alloc(size_t cb) { return (void*)LocalAlloc(0, cb); }
static void* AllocZero(size_t cb) { return (void*)LocalAlloc(LMEM_ZERO, cb); }

#pragma pack(push,4)
typedef struct _BY_HANDLE_FILE_INFORMATION {
   UINT dwFileAttributes;
   FILETIME ftCreationTime;
   FILETIME ftLastAccessTime;
   FILETIME ftLastWriteTime;
   UINT dwVolumeSerialNumber;
   UINT nFileSizeHigh;
   UINT nFileSizeLow;
   UINT nNumberOfLinks;
   UINT nFileIndexHigh;
   UINT nFileIndexLow;
   UINT dwOID;
} BY_HANDLE_FILE_INFORMATION;
#pragma pack(pop)
extern "C" BOOL __stdcall GetFileInformationByHandle(HANDLE hFile, BY_HANDLE_FILE_INFORMATION * lpfi);

#define READ_META 0
#define FILE_SHARE_ALL (1|2|4)
#define OPEN_EXISTING 3
#define FILE_ATTRIBUTE_NORMAL 0x80
extern "C" HANDLE __stdcall CreateFileW(wchar_t * name, UINT access, UINT share, void * psa, UINT create, UINT flags, HANDLE hTemplate);

#define QS_ALLEVENTS 0x04BF
#define QS_ALLINPUT  0x04FF
extern "C" unsigned int __stdcall MsgWaitForMultipleObjects(unsigned int count, const HANDLE* phandles, int bWaitAll, unsigned int millisec, unsigned int mask);
#define WAIT_TIMEOUT 258
#define WAIT_FAILED (unsigned int)-1;

#ifdef NOCONSOLE
typedef unsigned short ATOM;
typedef void* LRESULT;
typedef void* WPARAM;
typedef void* LPARAM;
#pragma pack(push,4)
typedef struct _WNDCLASSEXW {
   UINT   cb;
   UINT   style;
   LRESULT (__stdcall * pfnWndProc)(HWND, UINT, WPARAM, LPARAM);
   int     cbClassExtra;
   int     cbWndExtra;
   HMODULE hInstance;
   void*   hIcon;
   void*   hCursor;
   void*   hbrBackground;
   wchar_t* pszMenuName;
   wchar_t* pszClassName;
   void *  hIconSm;
} WNDCLASSEXW;
#pragma pack(pop)
extern "C" ATOM __stdcall RegisterClassExW(WNDCLASSEXW * pcls);
#define CS_GLOBALCLASS 0x4000
#define WM_CLOSE 0x0010
#define WM_QUIT  0x0012
#define HWND_MESSAGE     ((HWND)-3)
extern "C" HWND __stdcall CreateWindowExW(UINT dwExStyle, wchar_t* pszClass, wchar_t* pszWindow, UINT dwStyle, int x, int y, int cx, int cy, HWND hwndParent, void* hmenu, HMODULE hInst, void* lParam);
extern "C" LRESULT __stdcall DefWindowProcW(HWND hwnd, UINT, WPARAM, LPARAM);
extern "C" HMODULE __stdcall GetModuleHandleW(wchar_t* pszModule);

typedef struct _MSG {
	HWND hwnd;
	UINT id;
	void * wparam;
	void * lparam;
	UINT   time;
	int    x;
	int    y;
} MSG;
extern "C" int __stdcall PeekMessageW(MSG * msg, HWND, UINT idMin, UINT idMax, UINT uRemove);
extern "C" LRESULT __stdcall DispatchMessageW(MSG * msg);
#define PM_NOREMOVE 0
#define PM_REMOVE   1
#define PM_NOYIELD  2
#endif


// we need an implementation of memset, because the compiler sometimes generates refs to it.
// extern "C" char * memset(char * ptr, int val, unsigned int cb) { while (cb) { *ptr++ = (char)val; } return ptr; }
extern "C" void * memset(char* p, int value, size_t cb) { while (cb) p[--cb] = value; return p; }

template <class c>
const c* scan(const c* pline, const char* set) {
	c ch = *pline;
	while (ch) {
		if (set) for (int ii = 0; set[ii]; ++ii) { if (ch == set[ii]) return pline; }
		++pline;
		ch = *pline;
	}
	return pline;
}

template <class c>
const c* span(const c* pline, const char set[]) {
	c ch = *pline;
	while (ch) {
		bool in_set = false;
		for (int ii = 0; set[ii]; ++ii) { if (ch == set[ii]) { in_set = true; break; } }
		if ( ! in_set) return pline;
		++pline;
		ch = *pline;
	}
	return pline;
}

template <class t>
t* AllocCopy(const t* in, int ct)
{
   t* pt = (char*)Alloc(ct*sizeof(in[0]));
   if (pt) for (int ii = 0; ii < ct; ++ii) pt[ii] = in[ii];
   return pt;
}

template <class c>
int Length(const c* in) {
   const c* end = scan(in,NULL);
   return (int)(end - in);
}

template <class c>
c* AllocCopyZ(const c* in, int cch=-1)
{
   if (cch < 0) { cch = Length(in); }
   c* psz = (c*)Alloc( (cch+1) * sizeof(in[0]) );
   if (psz) {
      for (int ii = 0; ii < cch; ++ii) { psz[ii] = in[ii]; }
      psz[cch] = 0;
   }
   return psz;
}

template <class t> HLOCAL Free(t* pv) { return LocalFree((void*)pv); }

template <class c>
c* append_hex(c* psz, unsigned __int64 ui, int min_width = 1, c lead = '0')
{
   c temp[18+1];
   int  ii = 0;
   while (ui) {
      unsigned int dig = (unsigned int)(ui & 0xF);
      temp[ii] = (char)((dig < 10) ? (dig + '0') : (dig - 10 + 'A'));
      ui >>= 4;
      ++ii;
   }
   if ( ! ii && min_width) temp[ii++] = '0';
   for (int jj = ii; jj < min_width; ++jj) *psz++ = lead;
   while (ii > 0) *psz++ = temp[--ii];
   *psz = 0;
   return psz;
}


template <class c>
c* append_num(c* psz, unsigned int ui, int min_width = 1, c lead = ' ')
{
   c temp[11+1];
   int  ii = 0;
   while (ui) {
      temp[ii] = ((ui % 10) + '0');
      ui /= 10;
      ++ii;
   }
   if ( ! ii && min_width) temp[ii++] = '0';
   for (int jj = ii; jj < min_width; ++jj) *psz++ = lead;
   while (ii > 0) *psz++ = temp[--ii];
   *psz = 0;
   return psz;
}

// divide by 10 and return both the result of the division and the remainder
// this code comes from http://www.hackersdelight.org/divcMore.pdf
// tj modified it to return the remainder
#pragma optimize("gs", on)  // must optimize this code or it emits references to c-runtime shift functions
unsigned __int64 divu10(unsigned __int64 ui, int * premainder)
{
   unsigned __int64 q, r;
   q = (ui >> 1) + (ui >> 2);
   q = q + (q >> 4);
   q = q + (q >> 8);
   q = q + (q >> 16);
   q = q >> 3;
   r = ui - (q << 1) - (q << 3);
   while (r > 9) {
     q += 1;
     r -= 10;
   }
   if (premainder) *premainder = (int)r;
   return q;
}
#pragma optimize("", on)


template <class c>
c* append_num(c* psz, unsigned __int64 ui, int min_width = 1, c lead = ' ')
{
   c temp[25+1];
   int  ii = 0;
   while (ui) {
      int digit;
      ui = divu10(ui, &digit);
      temp[ii] = digit + '0';
      ++ii;
      if (ii > 24) break;
   }
   if ( ! ii && min_width) temp[ii++] = '0';
   for (int jj = ii; jj < min_width; ++jj) { *psz++ = lead; }
   while (ii > 0) { *psz++ = temp[--ii]; }
   *psz = 0;
   return psz;
}

template <class c, class w>
c* append(c* psz, const w* pin) {
   while (*pin) { *psz++ = (c)*pin++; }
   *psz = 0;
   return psz;
}

template <class c, class w>
c* append(c* psz, const w* pin, c* pend) {
   while (*pin && psz < pend) { *psz++ = (c)*pin++; }
   *psz = 0;
   return psz;
}

template <class c, class c2>
const c* next_token(const c* pline, const c2* ws) {
	pline = span(pline, ws);
	if (*pline == '"') { pline = scan(pline+1, "\""); return span(pline+1, ws); }
	return span(scan(pline, ws), ws);
}

// return a copy of the current token, stripping "" if any, return value is start of next token
template <class c, class c2>
const c* next_token_copy(const c* pline, const c2* ws, c* pbuf, int cchbuf, int * pcchNeeded) {
    const c* pstart = span(pline, ws);
    const c* pend;
    const c* pnext;
    if (*pstart == '"') {
       pstart += 1;
       pend = scan(pstart, "\"");
       pnext = span((*pend == '"') ? pend+1 : pend, ws);
    } else {
       pend = scan(pstart, ws);
       pnext = span(pend, ws);
    }
    if (pcchNeeded) *pcchNeeded = (pend - pstart);
    while (cchbuf > 1 && pstart < pend) {
       *pbuf++ = *pstart++;
       --cchbuf;
    }
    if (cchbuf > 0) *pbuf = 0;
    return pnext;
}

// return a pointer to start & length of the next token, after any "" have been removed.
template <class c, class c2>
const c* next_token_ref(const c* pline, const c2* ws, const c* & pToken, int & cchToken) {
    const c* pstart = span(pline, ws);
    const c* pend;
    const c* pnext;
    if (*pstart == '"') {
       pstart += 1;
       pend = scan(pstart, "\"");
       pnext = span((*pend == '"') ? pend+1 : pend, ws);
    } else {
       pend = scan(pstart, ws);
       pnext = span(pend, ws);
    }
    pToken = pstart;
    cchToken = (pend - pstart);
    return pnext;
}

template <class c>
BOOL Print(HANDLE hf, const c* output, unsigned int cch) {
    unsigned int cbWrote = 0;
    return WriteFile(hf, const_cast<c*>(output), cch * sizeof(c), &cbWrote, 0);
}
template <class c>
BOOL Print(HANDLE hf, const c* output, int cch) {
    if (cch < 0) { cch = Length(output); }
    return Print(hf, output, (unsigned int)cch);
}
template <class c> 
BOOL Print(HANDLE hf, const c* output, const c* pend) {
    int cch = pend - output;
    return Print(hf, output, (unsigned int)cch); 
}

template <class c>
const c* parse_uint(const c* pline, unsigned int * puint) {
	*puint = 0;
	for (c ch = *pline; ch >= '0' && ch <= '9'; ch = *++pline) {
		*puint = (*puint * 10) + (unsigned int)(ch - '0');
	}
	return pline;
}

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
		char buf[100], *p = append(buf, "RegisterClass failed err="); p = append_num(p, err, 1);
		MessageBoxExA(NULL, buf, "Error", MB_OK | MB_ICONINFORMATION, 0);
		ExitProcess(1);
	}

	UINT exstyle = 0, style = 0;
	HWND hwnd = CreateWindowExW(exstyle, L"ConsoleWindowClass", L"", style, 0,0,0,0, HWND_MESSAGE, NULL, wc.hInstance, NULL);
	if ( ! hwnd) {
		UINT err = GetLastError();
		char buf[100], *p = append(buf, "CreateWindow failed err="); p = append_num(p, err, 1);
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
				    char buf[100], *p = append(buf, "Sleeping for "); p = append_num(p, ms/1000, 1);
				    if (ms%1000) { p = append(p, "."); p = append_num(p, ms%1000, 3, '0'); }
				    p = append(p, " seconds...");
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
#endif
			"Usage: sleep [options] <time>[ms|s|M]\r\n"
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
			"Usage", MB_OK | MB_ICONINFORMATION, 0);
#else
			-1);
#endif
	}

	ExitProcess(0);
}
