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
#ifndef NTSYSINFO_H
#define NTSYSINFO_H

#include "extArray.h"
#include <tlhelp32.h>	// If trying to build on NT4, just comment this out

class CSysinfo
{
	// some typdefs for function pointers into NTDLL.DLL
	typedef DWORD (__stdcall *FPNtQuerySystemInformation)(DWORD, DWORD*, DWORD, DWORD);
	typedef DWORD (__stdcall *FPNtOpenThread)(void*, DWORD, void*, void*);
	typedef DWORD (__stdcall *FPNtClose)(HANDLE);

	// some typdefs for function pointers into KERNEL32.DLL on Win2K and up
	typedef HANDLE (WINAPI *FPCreateToolhelp32Snapshot)(DWORD, DWORD);
	typedef BOOL (WINAPI *FPThread32First)(HANDLE, LPTHREADENTRY32);
	typedef BOOL (WINAPI *FPThread32Next)(HANDLE, LPTHREADENTRY32);

	public:
	CSysinfo();
	~CSysinfo();
	int GetPIDs (ExtArray<pid_t> & dest);
	DWORD NumThreads (pid_t pid);
	BOOL GetProcessName (pid_t pid, char *dest);
#if 0
	int GetTIDs (pid_t pid, ExtArray<DWORD> & tids, 
		ExtArray<DWORD> & status);
#endif
	int GetTIDs (pid_t pid, ExtArray<DWORD> & tids);
	HANDLE OpenThread (DWORD tid, DWORD accessflag = THREAD_ALL_ACCESS);
	pid_t FindThreadProcess (DWORD find_tid);
	DWORD GetHandleCount (pid_t pid);
	DWORD GetParentPID (pid_t pid);
	void CloseThread (HANDLE hthread);
	bool IsWin2korXP() { return IsWin2k; }
#if 0
	void Explore(DWORD pid);
#endif

	protected:
	static DWORD *memptr;
	static DWORD memptr_size;
	void Refresh (void);
	void MakeAnsiString (WORD *unistring, char *ansistring);
	DWORD* NextBlock (DWORD* oldblock);
	DWORD* FindBlock (DWORD pid);

	static HINSTANCE hNtDll;
	static HINSTANCE hKernel32Dll;
	static FPNtQuerySystemInformation NtQuerySystemInformation;
	static FPNtOpenThread NtOpenThread;
	static FPNtClose NtClose;
	static FPCreateToolhelp32Snapshot CreateToolhelp32Snapshot;
	static FPThread32First Thread32First;
	static FPThread32Next Thread32Next;

	static bool IsWin2k;

	private:
	static int reference_count;

};

#endif

