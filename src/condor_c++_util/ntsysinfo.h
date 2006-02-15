/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
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
	BOOL GetProcessName (pid_t pid, char *dest, int sz);
#if 0
	int GetTIDs (pid_t pid, ExtArray<DWORD> & tids, 
		ExtArray<DWORD> & status);
#endif
	int GetTIDs (pid_t pid, ExtArray<DWORD> & tids);
	HANDLE OpenThread (DWORD tid, DWORD accessflag = THREAD_ALL_ACCESS);
	pid_t FindThreadProcess (DWORD find_tid);
	DWORD GetHandleCount (pid_t pid);
	DWORD GetParentPID (pid_t pid);
	int GetProcessEntry(pid_t pid, PROCESSENTRY32 &pe32 );
	void CloseThread (HANDLE hthread);
	bool IsWin2korXP() { return IsWin2k; }
	bool GetProcessBirthday(pid_t pid, FILETIME *ft);
	int ComparePidAge(pid_t pid1, pid_t pid2 );
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

