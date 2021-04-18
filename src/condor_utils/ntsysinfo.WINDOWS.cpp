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
bool CSysinfo::IsWin2k = false;
DWORD* CSysinfo::memptr = NULL;
DWORD CSysinfo::memptr_size = 0x10000;

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

		memptr = (DWORD*)VirtualAlloc (NULL, 
						memptr_size, 
						MEM_COMMIT,
						PAGE_READWRITE); 


		// Figure out if we are running on Win2k or above
		OSVERSIONINFO info;
		info.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
		MSC_SUPPRESS_WARNING_FOREVER(4996) // 'GetVersionExA': was declared deprecated
		if (GetVersionEx(&info) > 0) {
			if ( info.dwPlatformId ==  VER_PLATFORM_WIN32_NT  &&
				 info.dwMajorVersion >= 5 ) 
			{
				IsWin2k = true;
			}
		}

		// If this is Win2k, map in some additional functions
		if ( IsWin2k ) {
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
		if ( memptr ) {
			VirtualFree (memptr, 0, MEM_RELEASE);
			memptr = NULL;
		}
	}
}

int CSysinfo::GetPIDs (std::vector<pid_t> & dest)
{
#if 0
	// This code is deprecated in favor of using supported functions below
	pid_t curpid = 0;
	DWORD *startblock = memptr;
	dest.clear();
	Refresh();	
	while (startblock)
	{
		curpid = *(startblock + 17);
		dest.push_back(curpid);
		startblock = NextBlock (startblock);
		if ( startblock == (DWORD*)1 ) {
			startblock = memptr;
			dest.clear();
		}
	}
	return (int)dest.size();
#endif
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

BOOL CSysinfo::GetProcessName (pid_t pid, char *dest, int sz)
{
	// this code is deprecated. It was causing ACCESS_VIOLATIONS
	// on Windows XP.
#if 0
	DWORD *block;
	Refresh();
	block = FindBlock (pid);
	if (!block)
	{
		dest[0] = '\0';
		return FALSE;
	}
	MakeAnsiString ((WORD*)(*(block+15)), dest);
#endif
	HANDLE Hnd;

	if( ! (Hnd = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid))) {
		return FALSE;
	}

	if ( ! GetModuleBaseName(Hnd, NULL, dest, sz) ) {
		return FALSE;
	}
	
	return TRUE;
}

DWORD CSysinfo::GetHandleCount (pid_t pid)
{
	DWORD *block;
	Refresh();
	block = FindBlock (pid);
	if (!block)
		return 0;
	return block[19];
}

// substitute HEX values into output messages and print the resulting string via OutputDebugString.  
// pmsg must be of the form "blah blah 00000000 blah blah 00000000\n" with one section
// of zeros for each HEX. 
//
// This is strictly Windows-specific debugging code. and I didn't want to create any
// external dependancies for it.
//
static void Debug_Subst_Hex_ODS(const TCHAR * pmsg, const DWORD * pHex, int cHex)
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
      for (DWORD dw = pHex[ix]; dw && psz >= sz; dw = dw >> 4) 
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

DWORD CSysinfo::GetParentPID (pid_t pid)
{
#if 0
	/* this is broken on some versions of XP, so 
	   it is deprecated. See new implementation below. 
	 */
	DWORD *block;
	Refresh();
	block = FindBlock (pid);
	if (!block)
		return 0;
	return block[18];
#else
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
               DWORD pids[] = {pbi.InheritedFromUniqueProcessId, pid};
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
#endif
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

#if 0
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

#if 0
int CSysinfo::GetTIDs (pid_t pid, std::vector<DWORD> & tids, 
					   std::vector<DWORD> & tstatus)
#endif
int CSysinfo::GetTIDs (pid_t pid, std::vector<DWORD> & tids)
{
	DWORD s = 0;

	tids.clear();

	if ( !IsWin2k ) {
		/*** Window NT 4.0 Specific Code -- this does not work on Win2k! ***/
		DWORD *block;
		Refresh();
		block = FindBlock (pid);
		if (!block)
			return 0;
		for (s=0; s < *(block+1); s++)
		{
			tids.push_back( *(block+43+s*16) );
			// tstatus.push_back( *(block+48+s*16) + (*(block+47+s*16)<<8) );
		}
		return (int)s;
	}
			
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


//////////////////////// Helper Funcs ////////////////////////////

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
	NtQuerySystemInformation (5, memptr, memptr_size ,0);	
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

