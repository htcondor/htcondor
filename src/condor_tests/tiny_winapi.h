/***************************************************************
 *
 * Copyright (C) 2022, John M. Knoeller 
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

// define these before including this file to control what is included
// #define TINY_WINAPI_DECLARATIONS_ONLY // don't declare non-templated functions (use this to control which .cpp has the code)
// #define ENABLE_JOB_OBJECTS // declarations and structures for CreateJobObjectW and NtQueryInformationProcess
// #define NO_BPRINT_BUFFER              // omit declarations for the BprintBuffer class
// #define NO_AUTO_GROWING_BPRINT_BUFFER // omit auto-growing code in the BprintBuffer class
// #define NO_UTF8

// disable code checks that emit external refs to c-runtime functions
#pragma strict_gs_check(off)
#pragma runtime_checks("scu", off)

// Force the linker to include KERNEL32.LIB and to NOT include the c-runtime
#pragma comment(linker, "/entry:begin")
#pragma comment(linker, "/nodefaultlib:msvcrt")
#pragma comment(linker, "/defaultlib:kernel32.lib")

#ifdef NOCONSOLE
#pragma message("building NOCONSOLE")
#pragma comment(linker, "/subsystem:windows")
#pragma comment(linker, "/defaultlib:user32.lib")
#else
#pragma comment(linker, "/subsystem:console")
#endif
//  build programs that use this header with one of the following commands
// cl /O1 /GS- program.cpp /link /subsystem:console kernel32.lib
//   or
// cl /O1 /GS- /DNOCONSOLE=1 program.cpp /link /subsystem:windows /out:programw.exe kernel32.lib user32.lib

// msvc PREDEFINED MACROS (as of vs2012)
// _M_AMD64  target x64 instruction set
// _M_X64    target x64 instruction set (same as above?)
// _M_ARM    target ARM instruction set
// _M_IX86   target intel x86 instruction set, 500 = pentium, 600 = Pentium III
// _WCHAR_T_DEFINED defined when /Zc:wchar_t is used
// _CHAR_UNSIGNED   defined when /J is used
// _INTEGRAL_MAX_BITS max size in bits for an integral type (size of long long?)
// _DEBUG     defined when you compile with /LDd /MDd or /MTd
// _cplusplus defined when compiling for c++
//
// macros that change depending on context
// _DATE_    compilation date of the current file : Mmmm dd yyyy
// _TIME_    compilation time of the current file : hh:mm:ss
// _FILE_    name of the source file, quoted, use /FC to get a full pathame
// _LINE_    current line number
// _TIMESTAMP_ last modification time of the current file Dow Mon Date hh:mm:ss yyyy
// _COUNTER_ and integer starting with 0 that increments by 1
// _FUNCDNAME_ decorated name of the function (as a string) e.g. "?example@@YAXXZ"
// _FUNCTION_  undecorated name of the function (as a string) e.g. "example"
// _FUNCSIG_   signature of the function (as a string) e.g."void __cdecl example(void)"

#define QQUOTE(a) #a
#define QUOTE(a) QQUOTE(a)

#ifdef _M_X64
  #pragma message("---tiny_winapi building x64---")
  #define BUILD_ARCH_STRING "x64"
  typedef long long LONG_PTR;
  typedef long long SSIZE_T;
  typedef unsigned long long ULONG_PTR;
  typedef unsigned long long SIZE_T;

  // defines for picking up ... argument lists
  typedef char *  va_list;
  extern "C" void __cdecl __va_start(va_list *, ...); // this is a compiler intrinsic
  #define va_start(ap, x) ( __va_start(&ap, x) )
  // in va_list[] elements that are > 64 bits or not an even power of 2 size are pointers to the actual va_arg
  #define va_arg(ap, t)   \
    ( ( sizeof(t) > sizeof(__int64) || ( sizeof(t) & (sizeof(t) - 1) ) != 0 ) \
        ? **(t **)( ( ap += sizeof(__int64) ) - sizeof(__int64) ) \
        :  *(t  *)( ( ap += sizeof(__int64) ) - sizeof(__int64) ) )
  #define va_end(ap)      ( ap = (va_list)0 )
#else // assume 32bit
  #pragma message("---tiny_winapi building x86---")
  #define BUILD_ARCH_STRING "x86"
  typedef long LONG_PTR;
  typedef long SSIZE_T;
  typedef unsigned long ULONG_PTR;
  typedef unsigned long SIZE_T;

  // defines for picking up ... argument lists
  typedef char *  va_list;
  #define _ADDRESSOF(v)   ( &reinterpret_cast<const char &>(v) )
  #define _INTSIZEOF(n)   ( (sizeof(n) + sizeof(int) - 1) & ~(sizeof(int) - 1) )
  #define va_start(ap,v)  ( ap = (va_list)_ADDRESSOF(v) + _INTSIZEOF(v) )
  // in va_list[] elements are 32-bits in size, but some va_arg's occupy 2 consecutive elements. (and might thus be un-aligned..)
  #define va_arg(ap,t)    ( *(t *)((ap += _INTSIZEOF(t)) - _INTSIZEOF(t)) )
  #define va_end(ap)      ( ap = (va_list)0 )
#endif

// fake security cookie stuff that the compiler emits even though we turned off runtime checks above
extern "C" int __security_cookie = 0xBAADF00D;
extern "C" void _fastcall __security_check_cookie(int cookie) { }

// Note: this code should compile without linking to the c-runtime at all
// it is entirely self-contained other than windows api calls
// because of this, care must be taken to avoid c++ constucts that require
// the c-runtime, this includes avoiding 64 bit multiply/divide and inclusion of windows.h

typedef struct _HINST { int dummy; } * HMODULE;
typedef void* HANDLE;
typedef void* HWND;
typedef void* HLOCAL;
typedef int   BOOL;
typedef unsigned int UINT;
typedef unsigned char BYTE;
typedef struct _FILETIME { unsigned __int64 DateTime; } FILETIME; //
struct _qword {
   unsigned int lo; unsigned int hi;
   _qword(int l) : lo(l), hi((l < 0) ? -1 : 0) {}
   _qword(unsigned int l) : lo(l), hi(0) {}
   _qword(unsigned int l, unsigned int h) : lo(l), hi(h) {}
   _qword(__int64 qw) : lo((unsigned int)qw), hi((unsigned int)((unsigned __int64)qw >> 32)) {}
   _qword(unsigned __int64 qw) : lo((unsigned int)qw), hi((unsigned int)(qw >> 32)) {}
   operator unsigned __int64() { return *(unsigned __int64*)&lo; }
   operator unsigned __int64*() { return (unsigned __int64*)&lo; }
};
struct _sqword {
   unsigned int lo; int hi;
   _sqword() : lo(0), hi(0) {}
   _sqword(int l) : lo(l), hi((l < 0) ? -1 : 0) {}
   _sqword(unsigned int l) : lo(l), hi(0) {}
   _sqword(unsigned int l, int h) : lo(l), hi(h) {}
   _sqword(__int64 qw) : lo((unsigned int)qw), hi((int)((unsigned __int64)qw >> 32)) {}
   _sqword(unsigned __int64 qw) : lo((unsigned int)qw), hi((int)(qw >> 32)) {}
   operator __int64() { return *(__int64*)&lo; }
   operator __int64*() { return (__int64*)&lo; }
};

enum {
	STD_IN_HANDLE = -10,
	STD_OUT_HANDLE = -11,
	STD_ERR_HANDLE = -12
};
#define INVALID_HANDLE_VALUE (HANDLE)-1
#define NULL 0
#define NUMELMS(aa) (int)(sizeof(aa)/sizeof((aa)[0]))
template <class t> inline t MIN(t a, t b) { return a < b ? a : b; }
template <class t> inline t MAX(t a, t b) { return a > b ? a : b; }

#pragma pack(push,2)
typedef struct SYSTEMTIME {
	unsigned short int year, month, dayofweek, day, hour, minute, second, millisec;
} SYSTEMTIME;
#pragma pack(pop)

// used by create process
typedef struct STARTUPINFO {
	unsigned int cb;
	wchar_t *    lpReserved; // must be null
	wchar_t *    lpDesktop;  // desktop or desktop and window station
	wchar_t *    lpTitle;    // NULL unless creating a new console
	unsigned int dwX, dwY, dwXSize, dwYSize, dwXCountChars, dwYCountChars, dwFillAttribute;
	unsigned int dwFlags;	  // 0x2c 0x3c
	unsigned short int wShowWindow, cbReserved2;
	unsigned char * lpReserved2;
	HANDLE       hStdInput;
	HANDLE       hStdOutput;
	HANDLE       hStdError;
	void Init() {
		void**p = (void**)&cb;
		for (int ii = 0; ii < sizeof(STARTUPINFO)/sizeof(void*); ++ii) *p++ = NULL;
		cb = sizeof(STARTUPINFO);
	}
} STARTUPINFO;	  // 0x44 0x68

typedef struct STARTUPINFOEX {
	STARTUPINFO si;       // set si.cb to sizeof(STARTUPINFOEX)
	void *      pAttribs; // see InitializeProcThreadAttributeList
} STARTUPINFOEX;

// STARTUPINFO flags values
#define STARTF_USESHOWWINDOW       0x00000001
#define STARTF_USESIZE             0x00000002
#define STARTF_USEPOSITION         0x00000004
#define STARTF_USECOUNTCHARS       0x00000008
#define STARTF_USEFILLATTRIBUTE    0x00000010
#define STARTF_RUNFULLSCREEN       0x00000020  // ignored for non-x86 platforms
#define STARTF_FORCEONFEEDBACK     0x00000040
#define STARTF_FORCEOFFFEEDBACK    0x00000080
#define STARTF_USESTDHANDLES       0x00000100
// these are WINVER >= 0x400
#define STARTF_USEHOTKEY           0x00000200
#define STARTF_TITLEISLINKNAME     0x00000800
#define STARTF_TITLEISAPPID        0x00001000
#define STARTF_PREVENTPINNING      0x00002000


typedef struct PROCESS_INFORMATION {
	HANDLE hProcess;
	HANDLE hThread;
	unsigned int pid;
	unsigned int tid;
} PROCESS_INFORMATION;

#define READ_CONTROL 0x00020000
#define PROCESS_QUERY_INFORMATION 0x0400
#define PROCESS_QUERY_LIMITED_INFORMATION 0x1000

#define FORMAT_MESSAGE_ALLOCATE_BUFFER 0x00000100
#define FORMAT_MESSAGE_IGNORE_INSERTS  0x00000200
#define FORMAT_MESSAGE_FROM_SYSTEM     0x00001000
#define FORMAT_MESSAGE_ARGUMENT_ARRAY  0x00002000

extern "C" void __stdcall ExitProcess(unsigned int uExitCode);
extern "C" HANDLE __stdcall OpenProcess(unsigned int access, int inherit, unsigned int pid);
extern "C" BOOL __stdcall CloseHandle(HANDLE h);
extern "C" BOOL __stdcall GetExitCodeProcess(HANDLE hProcess, unsigned int * puExitCode);
extern "C" BOOL __stdcall TerminateProcess(HANDLE hProcess, unsigned int uExitCode);
extern "C" HANDLE __stdcall GetCurrentProcess(void);
extern "C" HANDLE __stdcall GetCurrentThread(void);
extern "C" unsigned int __stdcall GetCurrentProcessId(void);
extern "C" unsigned int __stdcall GetLastError(void);
extern "C" unsigned int __stdcall FormatMessageW(unsigned int flags, const void* source, unsigned int msgId, unsigned int langid, wchar_t* buf, int cbBuf, char* args);
extern "C" unsigned int __stdcall Sleep(unsigned int millisec);
extern "C" unsigned __int64 __stdcall GetTickCount64();
extern "C" HANDLE __stdcall GetStdHandle(int idHandle);
extern "C" const wchar_t * __stdcall GetCommandLineW(void);
extern "C" wchar_t * __stdcall GetEnvironmentStringsW(void);
extern "C" char * __stdcall GetEnvironmentStringsA(void);
extern "C" BOOL __stdcall FreeEnvironmentStringsA(char*);
extern "C" BOOL __stdcall SetEnvironmentVariableA(const char * name, const char * value);
extern "C" BOOL __stdcall SetEnvironmentVariableW(const wchar_t * name, const wchar_t * value);
extern "C" UINT __stdcall GetEnvironmentVariableA(const char * name, char * buf, UINT cch);
extern "C" UINT __stdcall GetEnvironmentVariableW(const wchar_t * name, wchar_t * buf, UINT cch);
extern "C" BOOL __stdcall WriteFile(HANDLE hFile, const char * buffer, unsigned int cbBuffer, unsigned int * pcbWritten, void* over);
extern "C" BOOL __stdcall ReadFile(HANDLE hFile, char * buffer, unsigned int cbBuffer, unsigned int * pcbRead, void* over);
extern "C" BOOL __stdcall GetFileSizeEx(HANDLE hFile, __int64 * pcb);
extern "C" BOOL __stdcall QueryPerformanceCounter(unsigned __int64 * counter);
extern "C" BOOL __stdcall CreateProcessW(const wchar_t * app, const wchar_t * cmd, void* sec1, void* sec2, int inherit,
										 unsigned int dwFlags, void* env, const wchar_t * dir,
										 STARTUPINFO* sinfo, PROCESS_INFORMATION * pi);
extern "C" HANDLE __stdcall OpenProcess(unsigned int dwAccess, BOOL inherit_handle, unsigned int pid);
// this requires WinVista or later.
extern "C" BOOL __stdcall QueryFullProcessImageNameW(HANDLE hProcess, unsigned int flags, wchar_t* pszBuf, unsigned int * cchBuf);
extern "C" unsigned int __stdcall ResumeThread(HANDLE h);

extern "C" void __stdcall GetSystemTime(SYSTEMTIME* systime);
extern "C" void __stdcall GetSystemTimeAsFileTime(FILETIME* time);
extern "C" BOOL __stdcall SystemTimeToFileTime(const SYSTEMTIME * systime, FILETIME* time);

extern "C" HMODULE __stdcall LoadLibraryW(const wchar_t * filename);
extern "C" HMODULE __stdcall GetModuleHandleW(const wchar_t * modulename);
extern "C" void* __stdcall GetProcAddress(HMODULE hMod, const char * proc_name);

enum {
	CTRL_C_EVENT=0,
	CTRL_BREAK_EVENT=1,
	CTRL_CLOSE_EVENT=2,
};
extern "C" BOOL __stdcall SetConsoleCtrlHandler(BOOL (__stdcall *fn)(unsigned int eEvent), BOOL add_handler);
extern "C" BOOL __stdcall GenerateConsoleCtrlEvent(unsigned int eEvent, unsigned int pidProcessGroup);


//
// Process dwCreationFlag values
//

#define CREATE_SUSPENDED                  0x00000004
#define CREATE_NEW_PROCESS_GROUP          0x00000200    // ctrl+c handling is disabled by default, is root process of new process group. ignored if used with CREATE_NEW_CONSOLE
#define CREATE_UNICODE_ENVIRONMENT        0x00000400    // env block is unicode
#define EXTENDED_STARTUPINFO_PRESENT      0x00080000

//
#define CREATE_BREAKAWAY_FROM_JOB         0x01000000    // break out of job object (if any)
#define CREATE_DEFAULT_ERROR_MODE         0x04000000    // new process gets default error mode.
#define CREATE_IGNORE_SYSTEM_DEFAULT      0x80000000

// use only one (or none) of these
#define CREATE_NEW_CONSOLE                0x00000010    // create a new console for the process
#define DETACHED_PROCESS                  0x00000008    // does not inherit the console or get a new console
#define CREATE_NO_WINDOW                  0x08000000    // ignored for non-console apps, 

#define REALTIME_PRIORITY_CLASS           0x00000100
#define NORMAL_PRIORITY_CLASS             0x00000020
#define IDLE_PRIORITY_CLASS               0x00000040
#define HIGH_PRIORITY_CLASS               0x00000080
#define BELOW_NORMAL_PRIORITY_CLASS       0x00004000
#define ABOVE_NORMAL_PRIORITY_CLASS       0x00008000

// ------

#define LMEM_ZERO 0x40
#define LMEM_HANDLE 0x02
extern "C" HLOCAL __stdcall LocalAlloc(unsigned int flags, size_t cb);
extern "C" HLOCAL __stdcall LocalFree(HLOCAL hmem);

static void* Alloc(size_t cb) { return (void*)LocalAlloc(0, cb); }
static void* AllocZero(size_t cb) { return (void*)LocalAlloc(LMEM_ZERO, cb); }

#define ERROR_SUCCESS          0
#define ERROR_INVALID_FUNCTION 1
#define ERROR_FILE_NOT_FOUND   2
#define ERROR_PATH_NOT_FOUND   3
#define ERROR_ACCESS_DENIED    5
#define ERROR_OUTOFMEMORY      14


#pragma pack(push,4) // TJ: check to see if this pack is correct for win64
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
#define FILE_READ_DATA 1
#define FILE_WRITE_DATA 2
#define FILE_APPEND_DATA 4
#define OPEN_EXISTING 3
#define OPEN_ALWAYS 4
#define CREATE_ALWAYS 2
#define FILE_ATTRIBUTE_NORMAL 0x80
extern "C" HANDLE __stdcall CreateFileW(wchar_t * name, UINT access, UINT share, void * psa, UINT create, UINT flags, HANDLE hTemplate);
extern "C" HANDLE __stdcall CreateFileA(char * name, UINT access, UINT share, void * psa, UINT create, UINT flags, HANDLE hTemplate);

typedef struct _SECURITY_ATTRIBUTES {
	UINT cb;		   // 0 +4    0 +4
	void* psec;		   // 4 +4	  4 +8
	int bInherit;	   // 8 +4	  c +4
} SECURITY_ATTRIBUTES; // c		  0x10


extern "C" BOOL __stdcall DuplicateHandle(HANDLE hSrcProc, HANDLE hi, HANDLE hDstProc, HANDLE *ho, UINT access, BOOL inherit, UINT options);
#define DUPLICATE_CLOSE_SOURCE 1
#define DUPLICATE_SAME_ACCESS 2

#define QS_ALLEVENTS 0x04BF
#define QS_ALLINPUT  0x04FF
extern "C" unsigned int __stdcall MsgWaitForMultipleObjects(unsigned int count, const HANDLE* phandles, int bWaitAll, unsigned int millisec, unsigned int mask);
extern "C" unsigned int __stdcall WaitForMultipleObjects(unsigned int count, const HANDLE* phandles, int bWaitAll, unsigned int millisec);
#define WAIT_TIMEOUT 258
#define WAIT_FAILED (unsigned int)-1;

#ifndef NO_MESSAGE_BOX
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
#endif

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


#ifdef ENABLE_JOB_OBJECTS
extern "C" HANDLE __stdcall CreateJobObjectW(void* sec, const wchar_t* name);
extern "C" BOOL __stdcall AssignProcessToJobObject(HANDLE hJob, HANDLE hProcess);
extern "C" BOOL __stdcall SetInformationJobObject(HANDLE hJob, int eInfo, void* info, unsigned int cbInfo);
enum {
	JobObjectBasicAccountingInformation=1, // for query only
	JobObjectBasicLimitInformation=2,  // info is JOBOBJECT_BASIC_LIMIT_INFORMATION
	JobObjectBasicProcessIdList=3,     // query JOBOBJECT_BASIC_PROCESS_ID_LIST
	JobObjectBasicUIRestrictions=4,
	JobObjectEndOfJobTimeInformation=6,  // info is address of unsigned int containing 0 or 1. 0 = TERMINATE, 1 = post to compiletion port
	JobObjectBasicAndIoAccountingInformation=8,
	JobObjectExtendedLimitInformation=9,
	JobObjectCpuRateControlInformation=15, // not on Win7
};

typedef struct JOBOBJECT_BASIC_ACCOUNTING_INFORMATION {
	__int64 TotalUserTime;
	__int64 TotalKernelTime;
	__int64 ThisPeriodTotalUserTime;
	__int64 ThisPeriodTotalKernelTime;
	unsigned int TotalPageFaultCount;
	unsigned int TotalProcesses;
	unsigned int ActiveProcesses;
	unsigned int TotalTerminatedProcesses;
} JOBOBJECT_BASIC_ACCOUNTING_INFORMATION;

typedef struct IO_COUNTERS {
	unsigned __int64 ReadOperationCount;   // count of read operations
	unsigned __int64 WriteOperationCount;  // count of write operations
	unsigned __int64 OtherOperationCount;  // count of i/o operations that were neither read nor write
	unsigned __int64 ReadTransferCount;    // bytes read
	unsigned __int64 WriteTransferCount;   // bytes written
	unsigned __int64 OtherTransferCount;   // bytes transfered other than read and write operations
} IO_COUNTERS;

typedef struct JOBOBJECT_BASIC_AND_IO_ACCOUNTING_INFORMATION {
	JOBOBJECT_BASIC_ACCOUNTING_INFORMATION BasicInfo;
	IO_COUNTERS                            IoInfo;
} JOBOBJECT_BASIC_AND_IO_ACCOUNTING_INFORMATION;

typedef struct JOBOBJECT_BASIC_LIMIT_INFORMATION {
	__int64 PerProcessUserTimeLimit;   // 0
	__int64 PerJobUserTimeLimit;	   // 0x8
	unsigned int LimitFlags;		   // 0x10
	SIZE_T       MinWorkingSetSize;
	SIZE_T       MaxWorkingSetSize;
	unsigned int ActiveProcessLimit;
	SIZE_T       Affinity;			   // 0x20
	unsigned int PriorityClass;
	unsigned int SchedulingClass;	   // 0x28
} JOBOBJECT_BASIC_LIMIT_INFORMATION;

// note: On WOW64 (at least on both Windows 7 and Windows 2008 server) the 32bit API does *not* return partial
// information when the supplied buffer size is insufficent for the cProcessesInJobObject.
// It simply leaves the structure unchanged in this case.  so on Win32 you must guess how big the buffer needs to be.
typedef struct JOBOBJECT_BASIC_PROCESS_ID_LIST {
  unsigned int cProcessesInJobObject;
  unsigned int cPidsInList;
  ULONG_PTR pids[1];
} JOBOBJECT_BASIC_PROCESS_ID_LIST;

#define ERROR_MORE_DATA 234

typedef struct JOBOBJECT_EXTENDED_LIMIT_INFORMATION {
	JOBOBJECT_BASIC_LIMIT_INFORMATION Basic;
	struct {
		unsigned long long ReadOperationCount;
		unsigned long long WriteOperationCount;
		unsigned long long OtherOperationCount;
		unsigned long long ReadTransferCount;
		unsigned long long WriteTransferCount;
		unsigned long long OtherTransferCount;
	} IoInfo;
	SIZE_T ProcessMemoryLimit;
	SIZE_T JobMemoryLimit;
	SIZE_T PeakProcessMemoryUsed;
	SIZE_T PeakJobMemoryUsed;
} JOBOBJECT_EXTENDED_LIMIT_INFORMATION;

#define JOB_OBJECT_LIMIT_PROCESS_TIME 0x00000002 // PerProcessUserTimeLimit is valid
#define JOB_OBJECT_LIMIT_JOB_TIME 0x00000004 // PerJobUserTimeLimit is valid
#define JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE 0x00002000 // Kill all processes in the job when the job handle is closed, use with EXTENDED_LIMIT
#define JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION 0x00000400 // sets SetErrorMode(SEM_NOGPFAULTERRORBOX), use with EXTENDED_LIMIT

extern "C" BOOL __stdcall QueryInformationJobObject(HANDLE hJob, int eInfo, void* pInfo, unsigned int cbInfo, unsigned int * pcbRetInfo);
extern "C" BOOL __stdcall ReadProcessMemory(HANDLE hProc, void* TheirAddress, void* buf, SIZE_T cbRead, SIZE_T * pcbRead);

enum {
	ProcessBasicInformation=0, // returns a pointer to a PEB structure (useful?)
};

typedef struct _UNICODE_STRING {
    unsigned short Length;
    unsigned short MaximumLength;
    wchar_t *      Buffer;
} UNICODE_STRING;
typedef struct _LIST_ENTRY {
   struct _LIST_ENTRY *Flink;
   struct _LIST_ENTRY *Blink;
} LIST_ENTRY;
typedef struct _PEB_LDR_DATA {
  unsigned char Reserved1[8];
  void *        Reserved2[3];
  LIST_ENTRY    InMemoryOrderModuleList;
} PEB_LDR_DATA;
typedef struct _RTL_USER_PROCESS_PARAMETERS {
  unsigned char  Reserved1[16];
  void*          Reserved2[10];
  UNICODE_STRING ImagePathName;
  UNICODE_STRING CommandLine;
} RTL_USER_PROCESS_PARAMETERS;
typedef struct _PEB {
	unsigned int being_debugged; // and with 0x00FF0000 to extract being debugged boolean value, other bits are reserved.
	void* Reserved3[2];
	PEB_LDR_DATA * Ldr;
	RTL_USER_PROCESS_PARAMETERS * ProcessParams;
	// more shit...
} PEB;
typedef struct PROCESS_BASIC_INFORMATION {
	ULONG_PTR ExitStatus;
	PEB * PebBaseAddress;
	ULONG_PTR AffinityMask;
	ULONG_PTR BasePriority;
	ULONG_PTR UniqueProcessId;
	ULONG_PTR InheritedFromUniqueProcessId;
} PROCESS_BASIC_INFORMATION;

typedef unsigned int (__stdcall * pntQIP)(HANDLE hProc, int eInfo, void* pInfo, unsigned long cbInfo, unsigned long *pcbInfo);

#ifdef TINY_WINAPI_DECLARATIONS_ONLY
	extern "C" pntQIP NtQueryInformationProcess;
#else
	static HMODULE hNtDll = NULL;
	static unsigned int error_ntDllLoad = 0;
	static unsigned int error_ntQIPLoad = 0;
	extern "C" unsigned int __stdcall LoadNtQIP(HANDLE hProc, int eInfo, void* pInfo, unsigned long cbInfo, unsigned long *pcbInfo);
	extern "C" pntQIP NtQueryInformationProcess = LoadNtQIP;
	extern "C" unsigned int __stdcall LoadNtQIP(HANDLE hProc, int eInfo, void* pInfo, unsigned long cbInfo, unsigned long *pcbInfo) {
		if ( ! hNtDll) {
			if (error_ntDllLoad) return error_ntDllLoad;
			hNtDll = LoadLibraryW(L"ntdll.dll");
		}
		if ( ! hNtDll) {
			error_ntDllLoad = GetLastError();
			if ( ! error_ntDllLoad) error_ntDllLoad = 23;
			error_ntDllLoad;
		}

		if (error_ntQIPLoad) return error_ntQIPLoad;
		pntQIP pfn = (pntQIP)GetProcAddress(hNtDll, "NtQueryInformationProcess");
		if ( ! pfn) {
			error_ntQIPLoad = 32;
			return error_ntQIPLoad; // change to NOT_FOUND error code
		}
		NtQueryInformationProcess = pfn; // once we set this, we will never call this function again.
		return pfn(hProc, eInfo, pInfo, cbInfo, pcbInfo);
	}
#endif // TINY_WINAPI_DECLARATIONS_ONLY

#endif // ENABLE_JOB_OBJECTS


// we need an implementation of memset, because the compiler sometimes generates refs to it.
// extern "C" char * memset(char * ptr, int val, unsigned int cb) { while (cb) { *ptr++ = (char)val; } return ptr; }
#ifdef TINY_WINAPI_DECLARATIONS_ONLY
#else
   // when /Oi is used, memset is an intrinsic.
   extern "C" void * __cdecl memset(char* p, int value, size_t cb) { while (cb) { p[--cb] = (char)value; } return p; };
#endif
   void ZeroMemory(void* p, size_t cb) { memset((char*)p, 0, cb); }

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

#ifndef NO_UTF8
/*
template <class c>
int UnicodeToUTF8_brute(c chi, BYTE cho[8]) {
   if (chi == 0) { cho[0] = 0xC0; cho[1] = 0x80; return 2; }
   if (chi < 0x80) { cho[0] = (BYTE)chi; return 1; }
   if (chi < 0x800) { cho[0] = 0xC0 + chi>>6; cho[1] = 0x80 + chi&0x3F; return 2; }
   if (chi < 0x1000) { cho[0] = 0xE0 + chi>>12; cho[1] = 0x80 + (chi>>6)&0x3F; cho[2] = 0x80 + chi&0x3F; return 3; }
   if (chi < 0x20000) { cho[0] = 0xF0 + chi>>18; cho[1] = 0x80 + (chi>>12)>&0x3F; cho[2] = 0x80 + (chi>>6)&0x3f; chi[3] = 0x80 + chi&0x3F; return 4; }
   if (chi < 0x400000) { cho[0] = 0xF0 + chi>>24; cho[1] = 0x80 + (chi>>18)>&0x3F; cho[2] = 0x80 + (chi>>12)&0x3f; cho[2] = 0x80 + (chi>>6)&0x3f; chi[4] = 0x80 + chi&0x3F; return 5; }
}
*/

// for UTF8, every code point that needs more than 1 char will have 0x80 bit set
// extension bytes are always binary 10xxxxxx
// lead bytes are always binary 11xxxxxx with number of leading 1's indicating how many extension bytes there will be
template <class c>
int UTF8fromUnicode(char cho[8], c chi) {
   if ( ! chi) { cho[0] = 0xC0; cho[1] = 0x80; return 2; } // special case for \0 so null's roundtrip.
   if ( ! (chi & ~0x7F)) { cho[0] = (char)(BYTE)chi; return 1; }
   BYTE b[8]; BYTE * pb = &b[7];
   UINT u = chi;
   UINT am = 0x3f;
   while (u & ~am) { *pb-- = (BYTE)(u & 0x3f); u >>= 6; am >>= 1; }
   int ix = 0;
   cho[ix++] = (char)(BYTE)(u | ~am);
   while (pb <= &b[7]) { cho[ix++] = (char)(0x80 | *pb++); }
   return ix;
}

// returns how many utf8 characters are needed for a given unicode character
template <class c>
int SizeofUTF8fromUnicode(c chi) {
   if ( ! chi) { return 2; } // special case for \0 so null's roundtrip.
   if ( ! (chi & ~0x7F)) { return 1; }
   UINT u = chi, am = 0x3f;
   int count = 1;
   while (u & ~am) { u >>= 6; am >>= 1; ++count; }
   return count;
}

// convert a utf8 character to unicode
template <class c>
int UnicodeFromUTF8(c & cho, const char* chi) {
   if ( ! (chi[0] & ~0x7f)) { cho = (c)(unsigned char)chi[0]; return 1; }

   // calculate number of expected follow-on bytes
   int count = 0;
   unsigned int mask = chi[0];
   while ((mask & 0xC0) == 0xc0) { mask<<=1; ++count; }

   unsigned int ch = 0;
   int ii = 0;
   for (ii = 0; ii < count; ++ii) {
      unsigned int ch1 = (unsigned int)chi[ii+1];
      if ((ch1 & 0xC0) != 0x80) break; // not a legal follow on byte!!
      ch |= (ch1 & 0x3F)<<(ii*6);
   }

   if (ii == 0) {
      // if there was no valid follow on byte, then treat the first byte not
      // as a utf8 lead byte, but as a ISO-8859-1 encoded byte (i.e. just convert it to 0x00nn unicode)
      ch = (unsigned char)chi[0];
   } else {
      // ch has the follow on bytes, but not yet the bits from the lead byte, so we insert them now.
      ch <<= (6-count) | ((unsigned int)(unsigned char)chi[0] & (0x3F>>count));
   }
   cho = (c)ch;
   return ii+1;
}

// given the lead byte for a utf8 character, figure out how many bytes it has
// the pattern is this (in binary)
// 0xxxxxxx - 1 byte ascii
// 10xxxxxx - following byte
// 110xxxxx - lead byte for 2 char sequence
// 1110xxxx - lead byte for 3 char sequence (see the pattern?)
//
int inline UTF8AdvanceSize(char c) {
   if ( ! (c & 0x80)) return 1; // simple ASCII,
   if ((c & 0xC0) == 0x80) return -1; // not a lead byte. must back up!!
   c <<= 2; // shift off the obligitory 0xC that marks a lead byte
   int ix=2;
   while (c & 0x80) { ++ix; c <<= 1; } // now for each 0x80 we find as we upshift, it's one more byte
   return ix;
}

int inline UTF8StringCbSize(const wchar_t * psz) {
   int cb = 0;
   while (*psz) { cb += SizeofUTF8fromUnicode(*psz); ++psz; }
   return cb;
}

#endif NO_UTF8

template <class c, class t>
int CopyText(c* out, int cc, const t* in, int ct)
{
   int ii;
   for (ii = 0; ii < ct; ++ii) {
      if (ii >= cc) return ii;
      out[ii] = (c)in[ii];
   }
   return ii;
}
template <class c, class t>
int TextConversionSize(c* out, const t* in, int ct) { if (ct < 0) return Length(in)+1; return ct; }

#ifndef NO_UTF8
// specialization for for UCS-2 to UTF8
template <> int CopyText(char* out, int cc, const wchar_t* in, int ct)
{
   int ii,jo = 0;
   for (ii = 0; ii < ct && jo < cc-8; ++ii) {
      jo += UTF8fromUnicode(&out[jo], in[ii]);
   }
   for ( ; ii < ct; ++ii) {
      int cch = SizeofUTF8fromUnicode(in[ii]);
      if (jo+cch >= cc) return ii;
      jo += UTF8fromUnicode(&out[jo], in[ii]);
   }
   return jo;
}
// specialization for UTF8 to UCS-2
template <> int CopyText(wchar_t* out, int cc, const char* in, int ct)
{
   int ii,ji = 0;
   for (ii = 0; ji < ct && ii < cc; ++ii) {
      ji += UnicodeFromUTF8(out[ii], &in[ji]);
   }
   return ii;
}

template <> int TextConversionSize(char* out, const wchar_t* in, int ct) { 
   if (ct < 0) return UTF8StringCbSize(in) + 1;
   int cb = 0;
   for (int ii = 0; ii < ct; ++ii) { cb += SizeofUTF8fromUnicode(in[ii]); }
   return cb;
}
template <> int TextConversionSize(wchar_t* out, const char* in, int ct) {
   wchar_t chtemp;
   int ii = 0;
   if (ct < 0) {
      while (*in) { in += UnicodeFromUTF8(chtemp, in); ++ii; }
   } else {
      int ji = 0;
      while (ji < ct) { ji += UnicodeFromUTF8(chtemp, &in[ji]); ++ii; }
   }
   return ii;
}
#endif NO_UTF8

template <class t>
t* AllocCopy(const t* in, int ct)
{
   t* pt = (t*)Alloc(ct*sizeof(in[0]));
   if (pt) for (int ii = 0; ii < ct; ++ii) pt[ii] = in[ii];
   return pt;
}

template <class t>
t* AllocCopyCat(const t* in1, int ct1, const t*in2, int ct2)
{
   t* pt = (char*)Alloc(ct1+ct2*sizeof(in1[0]));
   if (pt) {
      int ii = ct1;
      if (in1) { for (int ii = 0; ii < ct1; ++ii) pt[ii] = in1[ii]; }
      if (in2) { for (int jj = 0; jj < ct2; ++jj) pt[ii+jj] = in2[jj]; }
   }
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

template <class c>
char* AllocCopyZALossy(const c* in, int cch=-1)
{
   if (cch < 0) { cch = Length(in); }
   char* psz = (char*)Alloc( (cch+1) * sizeof(char) );
   if (psz) {
      for (int ii = 0; ii < cch; ++ii) { psz[ii] = (char)in[ii]; }
      psz[cch] = 0;
   }
   return psz;
}

template <class t> t* Alloc(int count) { return (t*)LocalAlloc(0, count*sizeof(t)); }
template <class t> t* AllocZero(int count) { return (t*)LocalAlloc(LMEM_ZERO, count*sizeof(t)); }
template <class t> HLOCAL Free(t* pv) { return LocalFree((void*)pv); }

#pragma optimize("gs", on)
template <class c>
c* append_hex(c* psz, unsigned __int64 ui, int min_width = 1, char lead = '0')
{
   c temp[18+1];
   int  ii = 0;
   while (ui) {
      unsigned int dig = (unsigned int)(ui & 0xF);
      temp[ii] = (char)((dig < 10) ? (dig + '0') : (dig - 10 + 'A'));
      ui = ui >> 4;
      ++ii;
   }
   if ( ! ii && min_width) temp[ii++] = '0';
   for (int jj = ii; jj < min_width; ++jj) *psz++ = lead;
   while (ii > 0) *psz++ = temp[--ii];
   *psz = 0;
   return psz;
}
#pragma optimize("", on)

template <class c>
c* append_hex(c* psz, void* ui, int min_width = 1, char lead = '0') {
	return append_hex(psz, (unsigned __int64)(ULONG_PTR)ui, min_width, lead);
}

#ifdef TINY_WINAPI_DECLARATIONS_ONLY
unsigned __int64 divu10(unsigned __int64 ui, int * premainder);
unsigned __int64 nanos_to_microsec(unsigned __int64 nanos);
unsigned __int64 divu1000(unsigned __int64 ui, int * premainder);
#else
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
#pragma optimize("",on) // revert to command line optimization options

unsigned __int64 nanos_to_microsec(unsigned __int64 nanos) { return divu10(nanos, NULL); };
unsigned __int64 divu1000(unsigned __int64 ui, int * premainder) {
	int r0, rt;
	ui = divu10(ui, &rt);
	ui = divu10(ui, &r0); rt = rt*10 + r0;
	ui = divu10(ui, &r0); rt = rt*10 + r0;
	if (premainder) *premainder = rt;
	return ui;
}

#endif // TINY_WINAPI_DECLARATIONS_ONLY

template <class c>
c* append_num(c* psz, unsigned __int64 ui, bool negative=false, int min_width = 1, c lead = ' ')
{
   c temp[25+2];
   int  ii = 0;
   while (ui) {
      int digit;
      ui = divu10(ui, &digit);
      temp[ii] = digit + '0';
      ++ii;
      if (ii > 24) break;
   }
   if ( ! ii && min_width) temp[ii++] = '0';
   if (negative) temp[ii++] = '-';
   for (int jj = ii; jj < min_width; ++jj) { *psz++ = lead; }
   while (ii > 0) { *psz++ = temp[--ii]; }
   *psz = 0;
   return psz;
}

template <class c>
c* append_num(c* psz, __int64 ii, int min_width = 1, c lead = ' ')
{
   if (ii < 0) {
      if (ii == 0x8000000000000000ull) append_num(psz, (unsigned __int64)(ii), false, min_width, lead);
      return append_num(psz, (unsigned __int64)(-ii), true, min_width, lead);
   }
   return append_num(psz, (unsigned __int64)(ii), false, min_width, lead);
}

template <class c>
c* append_decimal(c* psz, unsigned __int64 ui, bool negative, int fract_width, int min_width = 1, c lead = ' ')
{
   c temp[25+2];
   int  ii = 0;
   while (ui) {
      int digit;
      ui = divu10(ui, &digit);
      temp[ii] = digit + '0';
      ++ii;
      if (ii > 24) break;
   }
   while (ii < 1+fract_width) temp[ii++] = '0';
   if (negative) { temp[ii++] = '-'; }
   for (int jj = ii; jj < min_width+fract_width; ++jj) { *psz++ = lead; }
   while (ii > fract_width) { *psz++ = temp[--ii]; }
   *psz++ = '.';
   while (ii > 0) { *psz++ = temp[--ii]; }
   *psz = 0;
   return psz;
}

template <class c>
c* append_decimal(c* psz, __int64 ii, int fract_width, int min_width = 1, c lead = ' ')
{
   if (ii < 0) {
      if (ii == 0x8000000000000000ull) append_decimal(psz, (unsigned __int64)(ii), false, fract_width, min_width, lead);
      return append_decimal(psz, (unsigned __int64)(-ii), true, fract_width, min_width, lead);
   }
   return append_decimal(psz, (unsigned __int64)(ii), false, fract_width, min_width, lead);
}

template <class c>
c* append_num(c* psz, unsigned int ui, bool negative=false, int min_width = 1, c lead = ' ')
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

template <class c>
c* append_num(c* psz, int ii, int min_width = 1, c lead = ' ')
{
   if (ii < 0) {
      // the signed value 0x80000000 can't be negated, but it doesn't need to be, we can just pass the negative flag
      if (ii == 0x80000000) return append_num(psz, (unsigned int)ii, true, min_width, lead);
      return append_num(psz, (unsigned int)(-ii), true, min_width, lead);
   }
   return append_num(psz, (unsigned int)(ii), false, min_width, lead);
}


template <class c, class w>
c* append_unsafe(c* psz, const w* pin) {
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

#ifndef NO_UTF8
template <> char* append(char* psz, const wchar_t* pin, char* pend) {
   while (*pin && psz < pend) {
      int cch = SizeofUTF8fromUnicode(*pin);
      if (cch == 1) { *psz++ = (char)*pin++; continue; }
      if (psz+cch >= pend) break;
      psz += UTF8fromUnicode(psz, *pin++);
   }
   *psz = 0;
   return psz;
}
#endif NO_UTF8

template <class c, class w>
c* append_aligned(c* psz, const w* pin, c* pend, int align, int max_chars) {
   int cch = Length(pin);
   // -1 is a special value that means no max, all other negative values are treated as max_char == 0
   if (max_chars < -1) cch = 0;
   else if (max_chars > 0) cch = MIN(cch, max_chars);
   int max_buf = (int)(pend - psz);
   if (align < 0) {
      // negative is left align
      align = MIN(-align, max_buf);
      cch = MIN(cch, max_buf);
      int ix;
      for (ix = 0; ix < cch; ++ix) { *psz++ = (c)pin[ix]; }
      for ( ; ix < align; ++ix) { *psz++ = (c)' '; }
   } else {
      // positive is right align
      align = MIN(align, max_buf);
      cch = MIN(cch, max_buf - align);
      int ix;
      for (ix = 0; ix < align - cch; ++ix) { *psz++ = (c)' '; }
      for (ix = 0; ix < cch; ++ix) { *psz++ = (c)pin[ix]; }
   }
   if (psz < pend) *psz = 0;
   return psz;
}
// TODO: utf8 specialization needed for append_aligned.

template <class c, class w>
c* append_banner_aligned(c* psz, w ch, c* pend, unsigned int cch, int align) {
   int max_buf = (int)(pend - psz);
   if (align < 0) {
      // negative is left align
      align = MIN(-align, max_buf);
      cch = MIN<int>(cch, max_buf);
      int ix;
      for (ix = 0; ix < cch; ++ix) { *psz++ = (c)ch; }
      for ( ; ix < align; ++ix) { *psz++ = (c)' '; }
   } else {
      // positive is right align
      align = MIN(align, max_buf);
      cch = MIN<int>(cch, max_buf - align);
      int ix;
      for (ix = 0; ix < align - cch; ++ix) { *psz++ = (c)' '; }
      for (ix = 0; ix < cch; ++ix) { *psz++ = (c)ch; }
   }
   if (psz < pend) *psz = 0;
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
const c* parse_uint(const c* pline, unsigned int * puint) {
	*puint = 0;
	for (c ch = *pline; ch >= '0' && ch <= '9'; ch = *++pline) {
		*puint = (*puint * 10) + (unsigned int)(ch - '0');
	}
	return pline;
}

template <class c>
bool str_equal(const c* a, const c* b) {
	while (*a && *b) {
		if (*a != *b) return false;
		++a, ++b;
	}
	return true;
}

template <class c>
bool str_equal_nocase(const c* a, const c* b) {
	while (*a && *b) {
		if (*a != *b) { 
			int cha = *a | 0x20;
			if (cha < 'a' || cha > 'z' || cha != (*b|0x20)) return false;
		}
		++a, ++b;
	}
	return true;
}

template <class c>
bool str_starts_with(const c* a, const c* pre) {
	while (*a) {
		if (*a != *pre) { return !(*pre); }
		++a, ++pre;
	}
	return true;
}

template <class c>
bool str_starts_with_nocase(const c* a, const c* pre) {
	while (*a) {
		if (*a != *pre) { 
			if ( ! *pre) return true;
			int cha = *a | 0x20;
			if (cha < 'a' || cha > 'z' || cha != (*pre|0x20)) return false;
		}
		++a, ++pre;
	}
	return true;
}

template <class c>
BOOL Print(HANDLE hf, const c* output, unsigned int cch) {
    unsigned int cbWrote = 0;
    return WriteFile(hf, (const char*)output, cch * sizeof(c), &cbWrote, 0);
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

#ifndef NO_BPRINT_BUFFER

template <class c>
class BprintBuffer {
public:
    UINT   cch;     // current end of data pointer relative to psz. 
    UINT   cchMax;  // current max     (shrinks when we call Write bug have no hOut in order to buffer)
    c*     psz;     // current pointer (advances when we call Write but have no hOut in order to buffer)
    c*     pAlloc;  // original allocation pointer, if != psz then we have unflused buffered data
    SIZE_T cbAlloc; // original allocation size (in bytes)
    SIZE_T cbUsage; // highwatermark of usage
    HANDLE hOut;    // write handle (if attached to a file)
    UINT   flags;

    enum {
       OWNS_BUFFER  = 0x0001,
       OWNS_HANDLE  = 0x0002,
       AUTO_FLUSH   = 0x0004,
      #ifndef NO_AUTO_GROWING_BPRINT_BUFFER
       AUTO_GROWING = 0x0008, // future
      #endif
    };

    BprintBuffer() : cch(0), cchMax(0), psz(NULL), pAlloc(NULL), cbAlloc(0), cbUsage(0), hOut(NULL), flags(0) {}
    BprintBuffer(c *ary, int cmax, HANDLE h=NULL, UINT fl=0) : cch(0), cchMax(cmax), psz(ary), pAlloc(ary), cbAlloc(cmax*sizeof(c)), cbUsage(0), hOut(h), flags(fl) { }
    ~BprintBuffer() {
        if (hOut && (flags & AUTO_FLUSH)) { EndLine(); }
        if (hOut && (flags & OWNS_HANDLE)) { CloseHandle(hOut); hOut = NULL; }
        if (pAlloc && (flags & OWNS_BUFFER)) { LocalFree(pAlloc); pAlloc = NULL; }
        psz = NULL;
        cch = 0;
        cchMax = cbAlloc / sizeof(c);
    }

    int Count() { return cch; }
    int Capacity() { return (int)(cbAlloc/sizeof(c)); }
    int BufferedSize() { return cch + (Capacity() - cchMax); }
    bool IsEmptyLine() { return cch==0; }
    bool IsEmptyBuffer() { return cch==0 && cchMax == Capacity(); }
    bool ClearLine() { bool was_empty = IsEmptyLine(); MaxUsage(); cch = 0; return ! was_empty; }
    bool ClearAll() { bool was_empty = IsEmptyBuffer(); MaxUsage(); cch = 0; cchMax = Capacity(); psz = pAlloc; return was_empty; }
    const c* ToString() { if (psz) { psz[cch] = 0; } return pAlloc; }
    int StringLength() { if (psz) psz[cch] = 0; return cch; }

    bool IncreaseCapacity(SIZE_T cchMinIncrease, c* & p1) {
         SIZE_T cchNew = cbAlloc/sizeof(c) + MAX(cbAlloc/sizeof(c), cchMinIncrease);
         c* pa = Alloc<c>(cchNew); if ( ! pa) { __debugbreak(); return false; }

         // if there is existing data, copy it the the new buffer
         if (pAlloc) {
            for (UINT ii = 0; ii < cbAlloc/sizeof(c); ++ii) { pa[ii] = pAlloc[ii]; }
            if (flags & OWNS_BUFFER) Free(pAlloc);
         }

         int cchStored = (int)(psz - pAlloc);

         // fixup the class
         p1 = pa + (p1 - pAlloc);
         pAlloc = pa;
         cbAlloc = cchNew * sizeof(c);
         psz = pa + cchStored;
         cchMax = cchNew - cchStored;
         flags |= OWNS_BUFFER;
         return true;
    }

    bool AutoGrowCapacity(SIZE_T cchMinIncrease, c* & p1) {
      #ifndef NO_AUTO_GROWING_BPRINT_BUFFER
       if (flags & AUTO_GROWING) return IncreaseCapacity(cchMinIncrease, p1);
      #endif
       return false;
    }

    SIZE_T MaxUsage() {
       SIZE_T cbCurrUse = BufferedSize() / sizeof(c);
       cbUsage = MAX(cbCurrUse, cbUsage);
       return cbUsage/sizeof(c);
    }

    template <class t> int append(const t* pin, int cchNew) {
       if (cch + cchNew > cchMax) {
          c* p = psz+cch;
          if ( ! AutoGrowCapacity(cchNew, p)) {
             __debugbreak();
             cchNew = cchMax - cch;
          }
       }
       if (cchNew > 0) {
          cch += CopyText(psz+cch, cchMax-cch, pin, cchNew);
       }
       return cchNew;
    }

    template <class t> int append(const t* pin) { return append(pin, Length(pin)); }
    template <class t> int append(const t* pin, const t* pinE) { return append(pin, (int)(pinE - pin)); }

    bool Write() {
       if (cch > 0) {
          if (cch > cchMax) { __debugbreak(); cch = cchMax; }
          if (hOut) {
             Print(hOut, psz, (UINT)cch);
          } else {
             // no output handle, just buffer for now.
             psz += cch;
             cchMax -= cch;
          }
          MaxUsage();
          cch = 0;
          return true;
       }
       return false;
    }

    bool WriteTo(HANDLE hOut) {
       int cchTot = cch + (cbAlloc/sizeof(c)) - cchMax;
       if (cchTot) {
          Print(hOut, pAlloc, (UINT)cchTot);
          ClearAll();
          return true;
       }
       return false;
    }

    bool EndLine(bool flush=true) {
       bool fAdded = false;
       if (cch > 0 && (int)psz[cch-1] != '\n') {
          psz[cch++] = (c)'\n';
          fAdded = true;
       }
       if (flush) Write();
       return fAdded;
    }

    template <class t>
    bool Write(const t* pin) {
       if (psz) append(pin);
       return Write();
    }

    // these add data to the buffer, then write the buffer
    template <class t> void print(const t* pin) { Write(pin); }
    template <class t> void printl(const t* pin) { append(pin); EndLine(true); }
    template <class t> void vprintl(const t* pfmt, int cvargs, const void* vargs) { vformat(pfmt, cvargs, vargs); EndLine(true); }

    // this adds string and insures a newline at the end, but does NOT write the buffer out
    template <class t> void appendl(const t* pin) { append(pin); EndLine(false); }

    // helper structure for vformat method
    // holds parse fields:  {index[,alignment][:type[quan]]}
    struct vformat_opt {
       char cch;        // width of {} including the braces (in characters),
       char index;      // index into the argv array
       char alignment;  // output field width & alignment
       char datasize;   // if non-zero indicates the data size in bytes
       char type;       // single digit type field, one of: 'x', 'p', 'd', 'z', 'w', 's'
       char quan;       // numeric quantity field after type field
       bool is_string_type() { return !type || (type|0x20)=='s' || (type|0x20)=='w'; }
    };

    // the .NET string.format format specifier is: {index[,alignment][:formatString]}
    // this class parses that and fills in a vformat_opt,
    // formatstring can only be 1 datasize char, 1 letter and 1 number, not the arbitrary strings that .NET allows
    template <class t>
    bool is_vformat(const t* pf, struct vformat_opt & opt) {
       unsigned int num;
       const t* p = parse_uint(pf+1, &num);
       if (p == pf+1) return false;
       if (num > 99) return false;

       // ok we tentatively have a vformat with an index argument
       opt.index = (char)num;
       opt.datasize = sizeof(t);
       opt.cch = opt.alignment = opt.type = opt.quan = 0;

       // check for optional ,alignment
       if (*p == ',') {
          int sign = 1;
          ++p; if (*p == '-') { sign = -1; ++p; }
          const t* p2 = parse_uint(p, &num);
          if (p2 == p) return false; // got a comma but no number, not a valid vformat
          opt.alignment = (char)(sign * (int)num);
          p = p2;
       }

       // check for optional :type, this does not follow the .net rules, but is a subset.
       // It must be a letter optionally followed but a number, before the letter
       // the letter 'l' can be used to indicate that the data is 'large', so 'ls' is a wide string
       // and 'ld' is a large (i.e. 64bit) integer.
       if (*p == ':') {
          int ch = p[1];
          if (ch == 'l') { opt.datasize = ch; ++p; ch = p[1]; } // long prefix indicates datasize
          if ((ch|0x20) < 'a' || (ch|0x20) > 'z') return false;
          opt.type = (char)ch; p += 2;
          if ((ch|0x20) == 'm') { if (*p != '}') { opt.quan = (char)*p++; } else { opt.quan = ' '; } } else
          if (*p >= '0' && *p <= '9') {
             unsigned int quan = 0;
             while (*p >= '0' && *p <= '9') { quan = quan*10 + (*p++ - '0'); }
             opt.quan = (char)quan;
             if ( ! quan && opt.is_string_type()) opt.quan = (char)-2;
          }
       }

       if (*p != '}') return false;

       // cook datasize based on the type argument and native bitness
       if (opt.datasize == 'l') {
          int tch = opt.type | 0x20;
          if (tch == 's' || tch == 'w' || tch == 'c') opt.datasize = 2; // for strings l means utf16 characters
          else if (tch == 'd' || tch == 'u' || tch == 'z' || tch == 'n' || tch == 'x' || tch == 'p') {
            #ifdef _M_X64
             opt.datasize = 0; // numeric/pointer fields are already 64 bits in size
            #else
             opt.datasize = 8; // numeric/pointer fields default to 4 byte
            #endif
          }
       } else if (opt.datasize == 2) {
          // 's' format and 'c' format are always 8 bit chars, 'ls' and 'lc' are always 16 bit chars.
          int tch = opt.type | 0x20;
          if (tch == 's' || tch == 'c') { opt.datasize = 1; }
       }

       opt.cch = (char)(int)(p - pf)+1;
       return true;
    }

    int  get_formatted_arg_size(struct vformat_opt & opt, int cvargs, const void* vargs) {
       int index = opt.index;
       if (index < 0 || index >= cvargs) { return 1; }
       const void *va = ((const void**)vargs)[index];
       int width = (opt.alignment < 0) ? -opt.alignment : opt.alignment;
       int cchArg;
       switch (opt.type|0x20) {
       case 'x':
       case 'p': cchArg = MAX(16, width); // 16 is worst case formatted hex.
          break;
       case 'u':
       case 'z':
       case 'd':
       case 'n': cchArg = MAX(22, width); // 21 is worst case formatted number. (18+sign+decimal)
          break;
       case 'c': cchArg = 8; // worst case for 'c' is 8 characters
          break;
       case 'm': cchArg = MIN((int)(ULONG_PTR)va, 1024); // format m for "margin" prints as many characters as the int value of the arg
          break;
       case 'w':
          if (va) {
             // TODO: account for align and quan here
             cchArg = TextConversionSize((c*)NULL, (const wchar_t*)va, -1);
          } else {
             cchArg = 4;
          }
          break;
       case 's':
       case 0: // string of same type as format string, opt.datasize will have been set to indicate
       default:
          if (va) {
              if (opt.datasize == 2) {
             // TODO: account for align and quan here
                 cchArg = TextConversionSize((c*)NULL, (const wchar_t*)va, -1);
              } else {
                 cchArg = TextConversionSize((c*)NULL, (const char*)va, -1);
              }
          } else {
              cchArg = 4;
          }
          break;
       }
       return cchArg;
    }

    c* append_formatted_arg(c* p, c* pmax, struct vformat_opt & opt, int cvargs, const void* vargs) {
       int index = opt.index;
       if (index < 0 || index >= cvargs) { *p++ = '?'; return p; }

       const void *va = ((const void**)vargs)[index];
      #ifdef _M_X64
       unsigned __int64 uval = (unsigned __int64)(ULONG_PTR)va;
                __int64 sval = (__int64)(LONG_PTR)va;
      #else
       const ULONG_PTR *pva = ((const ULONG_PTR*)vargs)+index;
       unsigned __int64 uval;
                __int64 sval;
       if (opt.datasize > 4) {
          uval = *(const unsigned __int64*)pva;
          sval = *(const __int64*)pva;
       } else {
          uval = *pva;
          sval = *(const LONG_PTR*)pva;
       }
      #endif

       switch (opt.type|0x20) {

       // these are the number and pointer formats
       case 'x':
          p = ::append_hex(p, uval, opt.alignment ? opt.alignment : 1);
          break;
       case 'p':
          p = ::append_hex(p, uval, opt.alignment ? opt.alignment : 8);
          break;

       case 'u':
          p = ::append_num(p, uval, false, opt.alignment ? opt.alignment : 1);
          break;
       case 'z':
          p = ::append_decimal(p, uval, false, opt.quan, opt.alignment ? opt.alignment : 1);
          break;

       case 'd':
          p = ::append_num(p, sval, opt.alignment ? opt.alignment : 1);
          break;
       case 'n':
          p = ::append_decimal(p, sval, opt.quan, opt.alignment ? opt.alignment : 1);
          break;

       case 'm':  // format m for "margin" prints as many characters as the int value of the arg
          p = ::append_banner_aligned(p, (c)(opt.quan ? opt.quan : ' '), pmax, MIN<UINT>(uval, 1024), opt.alignment ? opt.alignment : MIN<int>(uval, 1024));
          break;

       // these are the string formats
       case 'c':
          if (opt.datasize == 2) {
             int quan = opt.quan ? opt.quan : 1;
             if (quan > sizeof(__int64)/sizeof(wchar_t)) quan = sizeof(__int64)/sizeof(wchar_t);
             p = ::append(p, (const wchar_t*)&uval, p+quan < pmax ? p+quan : pmax);
          } else {
             int quan = opt.quan ? opt.quan : 1;
             if (quan > sizeof(__int64)/sizeof(char)) quan = sizeof(__int64)/sizeof(char);
             p = ::append(p, (const char*)&uval, p+quan < pmax ? p+quan : pmax);
          }
          break;

       case 's':
       case 0: // string of same type as format string, opt.datasize will have been set to indicate
       default:
          if (opt.datasize != 2) {
             const char* pch = va ? (const char*)va : "NULL";
             p = ::append_aligned(p, pch, pmax, opt.alignment, opt.quan);
             break;
          }
          // fall through to 'w' case when opt.datasize == 2
       case 'w': {
             const wchar_t* pch = va ? (const wchar_t*)va : L"NULL";
             p = ::append_aligned(p, pch, pmax, opt.alignment, opt.quan);
          }
          break;

       } // end switch

       return p;
    }

   #ifndef NO_AUTO_GROWING_BPRINT_BUFFER
    template <class t>
    int sizeof_vformat(const t* pfmt, int cvargs, const void* vargs) {
       const t* pf = pfmt;
       struct vformat_opt opt;
       int cchNew = 0;
       while (*pf) {
          int ch = *pf;
          switch (ch) {
          case '{':
             if (is_vformat(pf, opt)) {
                cchNew += get_formatted_arg_size(opt, cvargs, vargs);
                pf += opt.cch;
                continue;
             }
             // fall through
          default:
             // TODO: fix this to be utf8 correct
             ++cchNew;
          }
          ++pf;
       }
       return cchNew;
    }
   #endif

    template <class t>
    int vformat(const t* pfmt, int cvargs, const void* vargs) {
      #ifndef NO_AUTO_GROWING_BPRINT_BUFFER
       int cMaxResizes = 1;
      #endif
       c* p = psz+cch;
       c* pmax = psz+cchMax;
       const t* pf = pfmt;
       struct vformat_opt opt;
       while (*pf && p < pmax) {
          int ch = *pf;
          switch (ch) {
          case '{':
             if (is_vformat(pf, opt)) {
               #ifndef NO_AUTO_GROWING_BPRINT_BUFFER
                int cchArg = get_formatted_arg_size(opt, cvargs, vargs);
                if (p+cchArg >= pmax) {
                   ch = 0; // trigger the resizing code (or the quitting code)
                   break;
                }
               #endif
                p = append_formatted_arg(p, pmax, opt, cvargs, vargs);
                pf += opt.cch;
                continue;
             }
             // fall through
          default:
             // TODO: fix this to be utf8 correct
             *p++ = (c)ch;
            #ifndef NO_AUTO_GROWING_BPRINT_BUFFER
             if (p >= pmax) { ch = 0; } // trigger resizing (or quitting code)
            #endif
             break;
          }
         #ifndef NO_AUTO_GROWING_BPRINT_BUFFER
          if ( ! ch) { // ch is set to 0 when we run out of room. 
             if ( ! cMaxResizes) break; --cMaxResizes; // already resized once, so just quit.
             int cchGrow = sizeof_vformat(pfmt, cvargs, vargs);
             if ( ! AutoGrowCapacity(cchGrow, p)) { break; }
             pmax = psz+cchMax;
             continue; // we grew, now re-try formatting at the same character we left off.
          }
         #endif
          ++pf;
       }
       int cchNew = p - (psz+cch);
       cch += cchNew;
       return cchNew;
    }

    template <class t>
    int vformatl(const t* pfmt, int cvargs, const void* vargs) {
       int cch_before = cch;
       vformat(pfmt, cvargs, vargs);
       EndLine(false);
       return cch - cch_before;
    }

    template <class t>
    int vaformatf(const t* pfmt, va_list & va) {

       // scan the formatting string so that we know how to construct
       // the argv array
       struct vformat_opt opt[20];
       const t* pfopt[NUMELMS(opt)+1];
       int cOpt = 0;

      #ifdef _M_X64
      #else
       // on 32-bit platforms we have to keep track of which args are 64-bits in size
       // and adjust the indexs into the vargs array accordingly
       int extra[NUMELMS(opt)];
       for (int ii = 0; ii < (int)NUMELMS(extra); ++ii) { extra[ii] = 0; }
      #endif
       int highest_index = -1;

      #ifndef NO_AUTO_GROWING_BPRINT_BUFFER
       int cchUnformat = 0;
      #endif
       // scan the format string and find all of the {} substitution points
       const t* pf = pfmt;
       while (*pf) {
          if (*pf == '{' && is_vformat(pf, opt[cOpt])) {
             int index = opt[cOpt].index;
             if (highest_index < index) highest_index = index;
            #ifdef _M_X64
            #else
             if (index >= NUMELMS(extra)) { /*INLINE_BREAK;*/ return 0; }
             if (opt[cOpt].datasize > 4) { extra[index] = opt[cOpt].datasize/4; }
            #endif
             pfopt[cOpt] = pf;
             pf += opt[cOpt].cch;
             ++cOpt;
             if (cOpt >= NUMELMS(opt)) { __debugbreak(); return 0; }
             continue;
          }
         #ifndef NO_AUTO_GROWING_BPRINT_BUFFER
          ++cchUnformat;
         #endif
          ++pf;
       }
       pfopt[cOpt] = pf;

       // now we have enough information to call va_arg
       const void* vargs = va;
       int cvargs = highest_index+1;
      #ifdef _M_X64
      #else
       // on x86, we have to re-write the indexes to account for the size of int64 items
       // walk the extras array turing the individual counts into aggregate counts
       int total_extra = 0;
       for (int ii = 0, tot=0; ii < cvargs; ++ii) {
          int add = extra[ii]; extra[ii] += total_extra; total_extra += add;
       }
       // now walk the opt array, modifying the indexes to account for extra size items
       for (int ii = 0; ii < cOpt; ++ii) {
          opt[ii].index += extra[opt[ii].index];
       }
       cvargs += total_extra;
      #endif

       bool check_arg_size = false;
       int argsize[NUMELMS(opt)];
      #ifndef NO_AUTO_GROWING_BPRINT_BUFFER
       // get the sizes of all the formatted args so we can calculate the overall size needed
       int cchArgs = 0;
       for (int ii = 0; ii < cOpt; ++ii) {
          cchArgs += argsize[ii] = get_formatted_arg_size(opt[ii], cvargs, vargs);
       }
       if (cchArgs + cchUnformat > cchMax - cch) {
          c* p = psz+cch;
          if ( ! AutoGrowCapacity(cchArgs + cchUnformat, p)) {
             __debugbreak();
             check_arg_size = true;
          }
       }
      #endif

       c* p = psz+cch;
       c* pmax = psz+cchMax;
       pf = pfmt;
       int ixOpt = 0;
       while (*pf) {
          if (pf == pfopt[ixOpt]) {
             if (check_arg_size) { if (p + argsize[ixOpt] > pmax) break; }
             p = append_formatted_arg(p, pmax, opt[ixOpt], cvargs, vargs);
             pf += opt[ixOpt].cch;
             ++ixOpt;
          } else {
             p += CopyText(p, (int)(pmax - p), pf, (int)(pfopt[ixOpt] - pf));
             pf = pfopt[ixOpt];
          }
       }

       int cchNew = p - (psz+cch);
       cch += cchNew;
       return cchNew;
    }

    template <class t>
    int formatf(const t* pfmt, ...) {
       va_list va;
       va_start(va, pfmt);
       int ret = vaformatf(pfmt, va);
       va_end(va);
       return ret;
    }

    template <class t>
    int formatfl(const t* pfmt, ...) {
       va_list va;
       va_start(va, pfmt);
       int ret = vaformatf(pfmt, va);
       va_end(va);
       EndLine(false);
       return ret;
    }

    template <class t>
    int printf(const t* pfmt, ...) {
       va_list va;
       va_start(va, pfmt);
       int ret = vaformatf(pfmt, va);
       va_end(va);
       Write();
       return ret;
    }

    template <class t>
    int printfl(const t* pfmt, ...) {
       va_list va;
       va_start(va, pfmt);
       int ret = vaformatf(pfmt, va);
       va_end(va);
       EndLine(true);
       return ret;
    }

    template <class t>
    int hex_dump(BYTE* pb, UINT cb, UINT flags, const t* prefix) {
        UINT cchOld = cch;
        const UINT width = 16;
        const bool initial_prefix = flags&1;
        for (UINT ix = 0; ix < cb; ++ix) {
            bool do_prefix = (initial_prefix && (ix==0)) || (ix>0) && (0 == (ix % width));
            if (do_prefix) { EndLine(false); if (prefix) append(prefix); }
            char hexval[5]; ::append_hex(hexval, pb[ix], 2); hexval[2] = ' '; hexval[3] = hexval[4] = 0;
            if ((ix & 3) == 3) hexval[3] = ' ';
            append(hexval);
            if ((ix % width) == (width-1)) {
                append("  ");
                for (UINT jj = ix+1-width, kk=0; kk < width; ++kk) {
                    char ch = (char)pb[jj+kk]; if (ch < ' ' || ch > '~') ch = '.'; append(&ch, 1);
                }
            }
        }
        return cch - cchOld;
    }
};

#endif NO_BPRINT_BUFFER
