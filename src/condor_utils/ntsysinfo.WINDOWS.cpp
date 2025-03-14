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


/*************** 
 * This class gets various system information on NT which is 
 * _not_ supported via WIN32.  We do this via undocumented calls
 * in NTDLL.DLL.  This all seems to work fine on NT 4.0, but who
 * knows, this whole class could stop working with any future NT
 * release.  It'd be real nice if Microsoft provided a _documented_
 * and officially supported way of doing these functions via WIN32.
 * Some, such as opening a handle to a thread, are promised to become
 * officially supported with NT 5.   - Todd Tannenbaum, 10/98
****************/


#include "condor_common.h"
#include "ntsysinfo.WINDOWS.h"
#include <psapi.h>
//#include <winternl.h>

// Initialize static members in the class
int CSysinfo::reference_count = 0;
HINSTANCE CSysinfo::hNtDll = NULL;
HINSTANCE CSysinfo::hKernel32Dll = NULL;
CSysinfo::FPNtQueryInformationProcess CSysinfo::NtQueryInformationProcess = NULL;
CSysinfo::FPNtQuerySystemInformation CSysinfo::NtQuerySystemInformation = NULL;
CSysinfo::FPNtOpenThread CSysinfo::NtOpenThread = NULL;
CSysinfo::FPNtClose CSysinfo::NtClose = NULL;
CSysinfo::FPCreateToolhelp32Snapshot CSysinfo::CreateToolhelp32Snapshot = NULL;
CSysinfo::FPThread32First CSysinfo::Thread32First = NULL;
CSysinfo::FPThread32Next CSysinfo::Thread32Next = NULL;
#ifdef USE_NTQUERY_SYS_INFO
DWORD* CSysinfo::memptr = NULL;
DWORD CSysinfo::memptr_size = 0x10000;
#endif

CSysinfo::CSysinfo()
{
	if ( reference_count == 0 ) {

		hNtDll = LoadLibrary("ntdll.dll");

		if ( !hNtDll ) {
			EXCEPT("cannot load ntdll.dll library");
		}

		NtQueryInformationProcess = (FPNtQueryInformationProcess) 
			GetProcAddress(hNtDll,"NtQueryInformationProcess");
		if ( !NtQueryInformationProcess ) {
			EXCEPT("cannot get address for NtQueryInformationProcess");
		}
		NtQuerySystemInformation = (FPNtQuerySystemInformation) 
			GetProcAddress(hNtDll,"NtQuerySystemInformation");
		if ( !NtQuerySystemInformation ) {
			EXCEPT("cannot get address for NtQuerySystemInformation");
		}
		NtOpenThread = (FPNtOpenThread) 
			GetProcAddress(hNtDll,"NtOpenThread");
		if ( !NtOpenThread ) {
			EXCEPT("cannot get address for NtOpenThread");
		}
		NtClose = (FPNtClose) 
			GetProcAddress(hNtDll,"NtClose");
		if ( !NtClose ) {
			EXCEPT("cannot get address for NtClose");
		}

	#ifdef USE_NTQUERY_SYS_INFO
		memptr = (DWORD*)VirtualAlloc (NULL, 
						memptr_size, 
						MEM_COMMIT,
						PAGE_READWRITE); 
	#endif

		// If this is Win2k or later, map in some additional functions
		{
			hKernel32Dll = LoadLibrary("kernel32.dll");
			if ( !hKernel32Dll ) {
				EXCEPT("cannot load kernel32.dll library");
			}

			CreateToolhelp32Snapshot = (FPCreateToolhelp32Snapshot) 
				GetProcAddress(hKernel32Dll,"CreateToolhelp32Snapshot");
			if ( !CreateToolhelp32Snapshot ) {
				EXCEPT("cannot get address for CreateToolhelp32Snapshot");
			}
			Thread32First = (FPThread32First) 
				GetProcAddress(hKernel32Dll,"Thread32First");
			if ( !Thread32First ) {
				EXCEPT("cannot get address for Thread32First");
			}
			Thread32Next = (FPThread32Next) 
				GetProcAddress(hKernel32Dll,"Thread32Next");
			if ( !Thread32Next ) {
				EXCEPT("cannot get address for Thread32Next");
			}
		}

	}
	reference_count++;
}

CSysinfo::~CSysinfo()
{
	reference_count--;
	if ( reference_count == 0 ) {
		FreeLibrary(hNtDll);
	#ifdef USE_NTQUERY_SYS_INFO
		if ( memptr ) {
			VirtualFree (memptr, 0, MEM_RELEASE);
			memptr = NULL;
		}
	#endif
	}
}

int CSysinfo::GetPIDs (std::vector<pid_t> & dest)
{
	HANDLE hProcessSnap;
	PROCESSENTRY32 pe32;

	dest.clear();

	// Take a snapshot of all processes in the system.
	hProcessSnap = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );
	
	if( hProcessSnap == INVALID_HANDLE_VALUE )
	{
		return 0;
	}
	
	// Set the size of the structure before using it.
	pe32.dwSize = sizeof( PROCESSENTRY32 );
	
	// Retrieve information about the first process,
	// and exit if unsuccessful
	if( !Process32First( hProcessSnap, &pe32 ) )
	{
		CloseHandle( hProcessSnap ); // Must clean up the snapshot object!
		return 0;
	}
	
	// Now walk the snapshot of processes, and
	// stick each one in our vector to form our Pid list.
	do {
		dest.push_back( pe32.th32ProcessID );
	} while( Process32Next( hProcessSnap, &pe32 ) );

  // Don't forget to clean up the snapshot object!
  CloseHandle( hProcessSnap );
  
  return (int)dest.size(); // return the number of PIDs we got
}

#ifdef USE_NTQUERY_SYS_INFO
DWORD CSysinfo::NumThreads (pid_t pid)
{
	DWORD *block;
	Refresh();
	block = FindBlock (pid);
	if (block)
		return *(block+1);
	else
		return 0;
}
#endif

BOOL CSysinfo::GetProcessName (pid_t pid, char *dest, int sz)
{
	HANDLE Hnd;

	if( ! (Hnd = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid))) {
		return FALSE;
	}

	if ( ! GetModuleBaseName(Hnd, NULL, dest, sz) ) {
		return FALSE;
	}
	
	return TRUE;
}

#ifdef USE_NTQUERY_SYS_INFO
DWORD CSysinfo::GetHandleCount (pid_t pid)
{
	DWORD *block;
	Refresh();
	block = FindBlock (pid);
	if (!block)
		return 0;
	return block[19];
}
#endif

// substitute HEX values into output messages and print the resulting string via OutputDebugString.  
// pmsg must be of the form "blah blah 00000000 blah blah 00000000\n" with one section
// of zeros for each HEX. 
//
// This is strictly Windows-specific debugging code. and I didn't want to create any
// external dependancies for it.
//
template <class T>
static void Debug_Subst_Hex_ODS(const TCHAR * pmsg, const T * pHex, int cHex)
{
   TCHAR sz[200];
   UINT  cch = lstrlen(pmsg)+1;
   CopyMemory(sz, pmsg, cch*sizeof(TCHAR));
   sz[(sizeof(sz)/sizeof(TCHAR))-1] = 0;

   TCHAR * psz = sz + cch;

   for (int ix = cHex-1; ix >= 0; --ix)
      {
      while (psz > sz && *psz != '0') 
         --psz; 
      for (auto dw = pHex[ix]; dw && psz >= sz; dw = dw >> 4) 
         { 
         TCHAR ch = (dw & 0xF); 
         *psz = (TCHAR)((ch < 10) ? (ch + '0') : (ch -10 + 'A')); 
         --psz;
         }
      while (psz > sz && *psz != ' ') 
         --psz; 
      }
   OutputDebugString(sz);
}

// for NtQueryInformationProcess
typedef struct _PROCESS_BASIC_INFORMATION {
	ULONG_PTR ExitStatus;
	PVOID     PebBaseAddress;
	ULONG_PTR AffinityMask;
	ULONG_PTR BasePriority;
	ULONG_PTR UniqueProcessId;
	ULONG_PTR InheritedFromUniqueProcessId;
} PROCESS_BASIC_INFORMATION;

// PebBaseAddress points to this on x64 windows (from the online Windows API docs)
struct _PEB {
	BYTE Reserved1[2];
	BYTE BeingDebugged;
	BYTE Reserved2[21];
	LPCVOID LoaderData;             // actually PEB_LDR_DATA*
	LPCVOID ProcessParameters;      // actually RTL_USER_PROCESS_PARAMETERS*
	BYTE Reserved3[520];
	LPCVOID PostProcessInitRoutine; // actually PS_POST_PROCESS_INIT_ROUTINE*
	BYTE Reserved4[136];
	ULONG SessionId;              // terminal services sessionId
};

struct _PEB_LDR_DATA {
	BYTE       Reserved1[8];
	PVOID      Reserved2[3];
	LIST_ENTRY InMemoryOrderModuleList;
};

struct _UNICODE_STRING {
	USHORT     Length;
	USHORT     MaximumLength;
	PWSTR      Buffer;
};

typedef struct _RTL_USER_PROCESS_PARAMETERS {
	BYTE           Reserved1[16]; // ULONG AllocationSize, Size, Flags, DebugFlags
	PVOID          Reserved2[10]; // HANDLE CondoleHandle, flags, hStdInput, hStdOutput, hStdError; also CurrentDirectory and DllPath
	struct _UNICODE_STRING ImagePathName;
	struct _UNICODE_STRING CommandLine;
	PWSTR          Environment;         // from here down info is from ReactOS
	ULONG          WindowSize[9];
	struct _UNICODE_STRING WindowTitle;
	struct _UNICODE_STRING Desktop;
	struct _UNICODE_STRING ShellInfo;
	struct _UNICODE_STRING RumtimeInfo;
} RTL_USER_PROCESS_PARAMETERS, *PRTL_USER_PROCESS_PARAMETERS;

pid_t CSysinfo::GetParentPID (pid_t pid)
{
    if (pid == 0 || pid == 4) // Can't open pid=0 (Idle) or pid=4 (System) processes
       return 0;

	if (NtQueryInformationProcess) {
		PROCESS_BASIC_INFORMATION pbi;
        const DWORD ProcessBasicInformation = 0;

        DWORD status = 1; // assume failure (success == 0)
        DWORD cbNeeded = 0;
        if (pid != GetCurrentProcessId()) {

			HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, TRUE, pid);
			if (hProcess) {
				status = NtQueryInformationProcess(
					hProcess, 
					ProcessBasicInformation,
					&pbi, sizeof(pbi), &cbNeeded);

				CloseHandle (hProcess);
            } else {
               Debug_Subst_Hex_ODS("ntsysinfo.WINDOWS can't open PID 00000000\n", &pid, 1);
            }

        } else {
		    status = NtQueryInformationProcess(
               GetCurrentProcess(), 
               ProcessBasicInformation,
               &pbi, sizeof(pbi), &cbNeeded);
        }
        if (NOERROR == status) {
            static bool fLogit = false;
            if (fLogit) {
               pid_t pids[] = {(pid_t)pbi.InheritedFromUniqueProcessId, pid};
               Debug_Subst_Hex_ODS("Parent PID is 00000000 for PID 00000000\n", pids, 2);
            }
            return pbi.InheritedFromUniqueProcessId;
        } else {
            Debug_Subst_Hex_ODS("ntsysinfo.WINDOWS can't Query PPID for PID 00000000\n", &pid, 1);
        }
        return 0;

	} else {
    	/* this is really slow, about 25ms on a quad core Amd64 */
		PROCESSENTRY32 pe;
		if (GetProcessEntry(pid, pe)) {
			return pe.th32ParentProcessID;
		}
		return (0);
	}
}

#if 0
// hook this into daemon core startup to test /proc/pid/environ reader code on windows.
static void spill_environment_to_log(char * procdir)
{
	// helper hack to test /proc/pid/environ reading on windows
	// spill the startup environment to a file so that condor_who can read it
	char* penv = GetEnvironmentStringsA();
	if (penv) {
		char * pend = penv;
		while (*pend) { pend += strlen(pend)+1; }
		pend += 1;
		std::string filename = procdir;
		filename += "/" + std::to_string(getpid()) + "_environ";
		int fd = safe_open_wrapper_follow(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC | _O_BINARY, 0600);
		if (fd >= 0) {
			full_write(fd, penv, pend - penv);
			close(fd);
		}
		FreeEnvironmentStringsA(penv);
	}
}
#endif

char * CSysinfo::GetProcessEnvironment(pid_t pid, size_t size_max, DWORD & error)
{
	if (pid == 0 || pid == 4) { // Can't open pid=0 (Idle) or pid=4 (System) processes
		error = E_INVALIDARG;
		return nullptr;
	}

	error = S_OK; // assume success
	char * out = nullptr;
	if (pid == GetCurrentProcessId()) {
		LPCH envptr = GetEnvironmentStringsA();
		if (envptr && *envptr) {
			const char * endp = envptr;
			while (*endp) { endp += strlen(endp) + 1; }
			char * out = (char*)malloc(endp - envptr + 1);
			if (out) memcpy(out, envptr, endp - envptr+1);
			FreeEnvironmentStringsA(envptr);
		} else {
			error = GetLastError();
		}
		return out;
	}

	if (NtQueryInformationProcess) {
		PROCESS_BASIC_INFORMATION pbi;
		const DWORD ProcessBasicInformation = 0;
		struct _PEB peb = {0};

		DWORD status = 1; // assume failure (success == 0)
		DWORD cbNeeded = 0;

		HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
		if (hProcess) {
			status = NtQueryInformationProcess(hProcess,
					ProcessBasicInformation,
					&pbi, sizeof(pbi), &cbNeeded);
			if (NOERROR == status) {
				SIZE_T cbReturn=0;
				ReadProcessMemory(hProcess, (LPCVOID)pbi.PebBaseAddress, &peb, sizeof(peb), &cbReturn);
				if (peb.ProcessParameters) {
					// read the ProcessParameters
					RTL_USER_PROCESS_PARAMETERS upp = {0};
					ReadProcessMemory(hProcess, peb.ProcessParameters, &upp, sizeof(upp), &cbReturn);
					if (upp.Environment) {
						int cbRawEnv = sizeof(WCHAR)*size_max;
						WCHAR* RawEnv = (WCHAR*)malloc(cbRawEnv+2);
						memset(RawEnv, 0, cbRawEnv+2);
						ReadProcessMemory(hProcess, upp.Environment, RawEnv, cbRawEnv, &cbReturn);
						// scan the unicode environment to find the terminating \0
						WCHAR * wendp = RawEnv;
						while (*wendp) { wendp += lstrlenW(wendp)+1; }
						int cchEnv = (wendp - RawEnv);
						if (cchEnv) {
							int cbEnv = WideCharToMultiByte(CP_UTF8, 0, RawEnv, cchEnv+1, nullptr, 0, nullptr, nullptr);
							out = (char*)malloc(cbEnv+2);
							if (out) {
								memset(out, 0, cbEnv+2);
								WideCharToMultiByte(CP_UTF8, 0, RawEnv, cchEnv+1, out, cbEnv, nullptr, nullptr);
							}
						}
					}
				}
				if ( ! out) error = GetLastError();
			} else {
				error = status;
			}
			CloseHandle(hProcess);
		}
	} else {
		error = E_UNEXPECTED;
	}
	return out;
}

DWORD CSysinfo::CopyProcessMemory(pid_t pid, void* address, size_t cb, char * out)
{
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
	if (hProcess) {
		SIZE_T cbReturn=0;
		DWORD  dwRet = S_OK;
		if ( ! ReadProcessMemory(hProcess, address, out, cb, &cbReturn)) {
			dwRet = GetLastError();
		}
		CloseHandle(hProcess);
		return dwRet;
	} else {
		return GetLastError();
	}
}


int
CSysinfo::GetProcessEntry(pid_t pid, PROCESSENTRY32 &pe32 ) {
	
	HANDLE hProcessSnap;
	int result;

	result = FALSE;

	// Take a snapshot of all processes in the system.
	hProcessSnap = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );
	
	if( hProcessSnap == INVALID_HANDLE_VALUE )
	{
		return FALSE;
	}
	
	// Set the size of the structure before using it.
	pe32.dwSize = sizeof( PROCESSENTRY32 );
	
	// Retrieve information about the first process,
	// and exit if unsuccessful
	if( !Process32First( hProcessSnap, &pe32 ) )
	{
		CloseHandle( hProcessSnap ); // Must clean up the snapshot object!
		return FALSE;
	}
	
	// Now walk the snapshot of processes, and
	// when we find our pid, stop.
	do {
		if ( pe32.th32ProcessID == pid ) {
			// Found it! Woohoo!
			result = TRUE;
			break;
		}
	} while( Process32Next( hProcessSnap, &pe32 ) );

  // Don't forget to clean up the snapshot object!
  CloseHandle( hProcessSnap );
  
  return result;
}

#ifdef USE_NTQUERY_SYS_INFO
void CSysinfo::Explore(pid_t pid)
{
	DWORD *block;
	int i;

	Refresh();
	block = FindBlock (pid);
	if (!block)
		return;

	for (i=0;i<43;i++) {
		printf("Offset %d - %d \n",i,block[i]);
	}
}
#endif

int CSysinfo::GetTIDs (pid_t pid, std::vector<DWORD> & tids)
{
	DWORD s = 0;

	tids.clear();

			
	/******** Win2k Specific Code -- use spiffy new Toolhelp32 calls ***/

	HANDLE        hThreadSnap = NULL; 
    THREADENTRY32 te32        = {0}; 
 
    // Take a snapshot of all threads currently in the system. 

    hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0); 
    if (hThreadSnap == (HANDLE)-1) 
	{
		// dprintf(D_DAEMONCORE,"CreateToolhelp32Snapshot failed\n");
        return 0;
	}
 
    // Fill in the size of the structure before using it. 

    te32.dwSize = sizeof(THREADENTRY32); 
 
    // Walk the thread snapshot to find all threads of the process. 
    // If the thread belongs to the process, add its information 
    // to the list.
    if (Thread32First(hThreadSnap, &te32)) 
    { 
        do 
        { 
            if (te32.th32OwnerProcessID == pid) 
            { 
				tids.push_back( te32.th32ThreadID );
				s++;
            } 
        } 
        while (Thread32Next(hThreadSnap, &te32)); 
    } 
 
    // Do not forget to clean up the snapshot object. 
	CloseHandle (hThreadSnap); 
	
	return (int)s;		
}

pid_t CSysinfo::FindThreadProcess (DWORD find_tid)
{
	std::vector<pid_t> pids;
	std::vector<DWORD> tids;
	int num_pids;
	int num_tids;
	
	num_pids = GetPIDs (pids);
	for (size_t s=0; s<pids.size(); s++)
	{
		num_tids = GetTIDs (pids[s], tids);
		for (size_t l=0; l<tids.size(); l++)
		{
			if (find_tid == tids[l])
				return pids[s];
		}
	}
	return 0xffffffff;
}

HANDLE CSysinfo::OpenThread (DWORD tid, DWORD accessflag)
{
	HANDLE hThread = NULL;
	DWORD struct1[] = {0x18, 0, 0, 0, 0, 0}; //OBJECT_ATTRIBUTES
	DWORD struct2[] = {0,tid}; //CLIENT_ID
	NtOpenThread (&hThread, 
			   accessflag, 
			   struct1, 
			   struct2);

	return hThread;
}

void CSysinfo::CloseThread (HANDLE hthread)
{
	NtClose (hthread);
}


#ifdef USE_NTQUERY_SYS_INFO

//////////////////////// Helper Funcs ////////////////////////////

/*
typedef struct _SYSTEM_PROCESS_INFORMATION {
	ULONG NextEntryOffset;
	ULONG NumberOfThreads;
	BYTE Reserved1[48];
	UNICODE_STRING ImageName;
	KPRIORITY BasePriority;
	HANDLE UniqueProcessId;
	PVOID Reserved2;
	ULONG HandleCount;
	ULONG SessionId;
	PVOID Reserved3;
	SIZE_T PeakVirtualSize;
	SIZE_T VirtualSize;
	ULONG Reserved4;
	SIZE_T PeakWorkingSetSize;
	SIZE_T WorkingSetSize;
	PVOID Reserved5;
	SIZE_T QuotaPagedPoolUsage;
	PVOID Reserved6;
	SIZE_T QuotaNonPagedPoolUsage;
	SIZE_T PagefileUsage;
	SIZE_T PeakPagefileUsage;
	SIZE_T PrivatePageCount;
	LARGE_INTEGER Reserved7[6];
} SYSTEM_PROCESS_INFORMATION, *PSYSTEM_PROCESS_INFORMATION;
*/

DWORD* CSysinfo::NextBlock (DWORD* oldblock)
{
	DWORD offset = *oldblock;
	DWORD *p_nextblock;
	DWORD next_offset;
	bool need_resize = false;
	DWORD offset_into_buf;
	static DWORD largest_offset = 3000;

	if ( offset > largest_offset )
		largest_offset = offset;

	p_nextblock = (DWORD*)((DWORD)oldblock + offset);
	offset_into_buf = (DWORD)p_nextblock - (DWORD)memptr + 1;

	if (!offset) {
		if ( (offset_into_buf + largest_offset) > memptr_size)
			need_resize = true;
		else
			return NULL;
	} else {
		// Make certain the entire next block fits into our buffer.
		// First make certain all 4 bytes of the offset are in our buffer.
		if ( (offset_into_buf + sizeof(next_offset)) > memptr_size)
		{
			need_resize = true;
		} else {
			next_offset = *p_nextblock;

			if ( next_offset > largest_offset)
				largest_offset = next_offset;

			if ( (offset_into_buf + next_offset) > memptr_size) {
				need_resize = true;
			}	
		}
	}

	if ( need_resize ) {
		VirtualFree (memptr, 0, MEM_RELEASE);
		memptr_size += 0x10000;		// grow our buffer another 64kbytes
		memptr = (DWORD*)VirtualAlloc (NULL, 
						memptr_size, 
						MEM_COMMIT,
						PAGE_READWRITE); 
		Refresh();
		// return special return value which tells our caller
		// to start over because we had to make our buffer larger.
		return ( (DWORD*)1 );
	}

	return (p_nextblock);
}

__inline void CSysinfo::Refresh (void)
{
	const DWORD SystemProcessInformation = 5;
	NtQuerySystemInformation (SystemProcessInformation, memptr, memptr_size ,0);	
}

__inline void CSysinfo::MakeAnsiString (WORD *unistring, char *ansistring)
{
	int s=0;
	if (unistring)
		while (unistring[s])
			ansistring[s] = (char)unistring[s++];
	ansistring[s] = '\0';
}

__inline DWORD* CSysinfo::FindBlock (DWORD pid)
{
	DWORD *startblock = memptr;
	while (startblock)
	{
		if (*(startblock+17) == pid)
			return startblock;
		startblock = NextBlock (startblock);
		if ( startblock == (DWORD*)1 )
			startblock = memptr;
	}
	return NULL;
}
#endif

// This function is sortof duplicated in ProcAPI::GetProcInfo(),
// but if all you want to know is some pid's bday, it's much
// faster to call this function than to iterate over all Pids 
// in the system and dig through the registry.
bool
CSysinfo::GetProcessBirthday(pid_t pid, FILETIME* ft) {

	HANDLE pidHnd;
	FILETIME crtime, extime, kerntime, usertime;
	BOOL result;

	ZeroMemory(&crtime, sizeof(FILETIME));
	ZeroMemory(&extime, sizeof(FILETIME));
	ZeroMemory(&kerntime, sizeof(FILETIME));
	ZeroMemory(&usertime, sizeof(FILETIME));

	pidHnd = OpenProcess( PROCESS_QUERY_INFORMATION, 
			FALSE,
			pid );

	if ( pidHnd == NULL ) {
		dprintf(D_ALWAYS, "CSysinfo::GetProcessBirthday() - OpenProcess() "
			   "failed with err=%d\n", GetLastError());	
		return false;
	}


	result = GetProcessTimes(pidHnd, &crtime, &extime, &kerntime, &usertime);

	if ( result == 0 ) {
		dprintf(D_ALWAYS, "CSysinfo::GetProcessBirthday() - GetProcessTimes() "
			   "failed with err=%d\n", GetLastError());	
		CloseHandle(pidHnd);
		return false;
	}

	ft->dwLowDateTime = crtime.dwLowDateTime;
	ft->dwHighDateTime = crtime.dwHighDateTime; 

	CloseHandle(pidHnd);
	return true;
}

// Return values: 
// 1 First pid is younger than second pid. 
//  0 First pid is equal to second pid. 
// -1 First pid is older than second pid. 
//
int
CSysinfo::ComparePidAge(pid_t pid1, pid_t pid2 ) {

	FILETIME ft1, ft2;

	ZeroMemory(&ft1, sizeof(FILETIME));
	ZeroMemory(&ft2, sizeof(FILETIME));

	if (! GetProcessBirthday(pid1, &ft1)) {
		dprintf(D_ALWAYS, "Should never happen: ComparePidAge(%d) failed\n",
				pid1);
		return 0;
	}

	if (! GetProcessBirthday(pid2, &ft2)) {
		dprintf(D_ALWAYS, "Should never happen: ComparePidAge(%d) failed\n",
				pid2);
		return 0;
	}

	return CompareFileTime(&ft1, &ft2);
}

