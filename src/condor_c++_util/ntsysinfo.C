/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

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
#include "ntsysinfo.h"

static char *_FileName_ = __FILE__;		/* Used by EXCEPT (see except.h)     */

// Initialize static members in the class
int CSysinfo::reference_count = 0;
HINSTANCE CSysinfo::hNtDll = NULL;
CSysinfo::FPNtQuerySystemInformation CSysinfo::NtQuerySystemInformation = NULL;
CSysinfo::FPNtOpenThread CSysinfo::NtOpenThread = NULL;
CSysinfo::FPNtClose CSysinfo::NtClose = NULL;
DWORD* CSysinfo::memptr = NULL;

CSysinfo::CSysinfo()
{
	if ( reference_count == 0 ) {

		hNtDll = LoadLibrary("ntdll.dll");

		if ( !hNtDll ) {
			EXCEPT("cannot load ntdll.dll library");
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

		memptr = (DWORD*)VirtualAlloc ((void*)0x100000, 
						0x10000, 
						MEM_COMMIT,
						PAGE_READWRITE); 
	}
	reference_count++;
}

CSysinfo::~CSysinfo()
{
	reference_count--;
	if ( reference_count == 0 ) {
		FreeLibrary(hNtDll);
		VirtualFree (memptr, 0, MEM_RELEASE);
	}
}

int CSysinfo::GetPIDs (ExtArray<pid_t> & dest)
{
	int s=0;
	DWORD *startblock = memptr;
	Refresh();	
	while (startblock)
	{
		dest[s++] = *(startblock + 17);
		startblock = NextBlock (startblock);
	}
	return (s);
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

BOOL CSysinfo::GetProcessName (pid_t pid, char *dest)
{
	DWORD *block;
	Refresh();
	block = FindBlock (pid);
	if (!block)
	{
		dest[0] = '\0';
		return FALSE;
	}
	MakeAnsiString ((WORD*)(*(block+15)), dest);
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

DWORD CSysinfo::GetParentPID (pid_t pid)
{
	DWORD *block;
	Refresh();
	block = FindBlock (pid);
	if (!block)
		return 0;
	return block[18];
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
int CSysinfo::GetTIDs (pid_t pid, ExtArray<DWORD> & tids, 
					   ExtArray<DWORD> & tstatus)
#endif
int CSysinfo::GetTIDs (pid_t pid, ExtArray<DWORD> & tids)
{
	DWORD *block;
	DWORD s;
	Refresh();
	block = FindBlock (pid);
	if (!block)
		return 0;
	for (s=0; s < *(block+1); s++)
	{
		tids[s] = *(block+43+s*16);	
		// tstatus[s] = *(block+48+s*16) + (*(block+47+s*16)<<8);
	}
	return (int)s;
}

pid_t CSysinfo::FindThreadProcess (DWORD find_tid)
{
	ExtArray<pid_t> pids(256);
	ExtArray<DWORD> tids;
	int num_pids;
	int num_tids;
	
	num_pids = GetPIDs (pids);
	for (int s=0; s<num_pids; s++)
	{
		num_tids = GetTIDs (pids[s], tids);
		for (int l=0; l<num_tids; l++)
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

__inline DWORD* CSysinfo::NextBlock (DWORD* oldblock)
{
	DWORD offset = *oldblock;
	if (offset)
		return ((DWORD*)((DWORD)oldblock + offset));
	else
		return NULL;
}

__inline void CSysinfo::Refresh (void)
{
	NtQuerySystemInformation (5, memptr, 0x10000 ,0);	
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
	}
	return NULL;
}

