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

#ifndef NTSYSINFO_H
#define NTSYSINFO_H

#include <vector>
#include <tlhelp32.h>	// If trying to build on NT4, just comment this out

class CSysinfo
{
	// some typdefs for function pointers into NTDLL.DLL
	typedef DWORD (WINAPI *FPNtQueryInformationProcess)(HANDLE, DWORD, PVOID, DWORD, DWORD* );
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
	int GetPIDs (std::vector<pid_t> & dest);
	BOOL GetProcessName (pid_t pid, char *dest, int sz);
	char* GetProcessEnvironment(pid_t pid, size_t size_max, DWORD & error);
	DWORD CopyProcessMemory(pid_t pid, void* address, size_t cb, char * out);
#ifdef USE_NTQUERY_SYS_INFO
	DWORD NumThreads (pid_t pid);
	DWORD GetHandleCount (pid_t pid);
	void Explore(DWORD pid);
#endif
	int GetTIDs (pid_t pid, std::vector<DWORD> & tids);
	HANDLE OpenThread (DWORD tid, DWORD accessflag = THREAD_ALL_ACCESS);
	pid_t FindThreadProcess (DWORD find_tid);
	pid_t GetParentPID (pid_t pid);
	int GetProcessEntry(pid_t pid, PROCESSENTRY32 &pe32 );
	void CloseThread (HANDLE hthread);
	bool GetProcessBirthday(pid_t pid, FILETIME *ft);
	int ComparePidAge(pid_t pid1, pid_t pid2 );

	protected:
#ifdef USE_NTQUERY_SYS_INFO
	static DWORD *memptr;
	static DWORD memptr_size;
	void Refresh (void);
	void MakeAnsiString (WORD *unistring, char *ansistring);
	DWORD* NextBlock (DWORD* oldblock);
	DWORD* FindBlock (DWORD pid);
#endif

	static HINSTANCE hNtDll;
	static HINSTANCE hKernel32Dll;
    static FPNtQueryInformationProcess NtQueryInformationProcess;
	static FPNtQuerySystemInformation NtQuerySystemInformation;
	static FPNtOpenThread NtOpenThread;
	static FPNtClose NtClose;
	static FPCreateToolhelp32Snapshot CreateToolhelp32Snapshot;
	static FPThread32First Thread32First;
	static FPThread32Next Thread32Next;


	private:
	static int reference_count;

};

#endif // NTSYSINFO_H

