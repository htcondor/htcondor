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

// Written by Zoltan Csizmadia, zoltan_csizmadia@yahoo.com
// For companies(Austin,TX): If you would like to get my resume, send an email.
//
// The source is free, but if you want to use it, mention my name and e-mail address
//
//////////////////////////////////////////////////////////////////////////////////////
//
// SystemInfo.h, v1.1

#ifndef SYSTEMINFO_H_INCLUDED
#define SYSTEMINFO_H_INCLUDED

/*
#ifndef WINNT
#error You need Windows NT to use this source code. Define WINNT!
#endif
*/

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#pragma warning( disable : 4786 )
#pragma warning( disable : 4200 )

// #include <afxtempl.h>
#include <map>

// #include <winternl.h>
#include <ntsecapi.h>


//////////////////////////////////////////////////////////////////////////////////////
//
// Typedefs
//
//////////////////////////////////////////////////////////////////////////////////////

/*

  Already in the PSDK
  -Ben 2007-09-20

typedef struct _UNICODE_STRING
{
	WORD  Length;
	WORD  MaximumLength;
	PWSTR Buffer;
} UNICODE_STRING;
*/

//////////////////////////////////////////////////////////////////////////////////////
//
// SystemInfoUtils
//
//////////////////////////////////////////////////////////////////////////////////////

// Helper functions

class SystemInfoUtils
{
public:

	//////////////////////////////////////////////////////////////////////////////////
	// String conversion functions

	// From wide char string to std::string
	static void LPCWSTR2String( LPCWSTR strW, std::string& str );
	// From unicode string to MyString
	static void Unicode2String( UNICODE_STRING* strU, std::string& str );

	//////////////////////////////////////////////////////////////////////////////////
	// File name conversion functions

	static BOOL GetDeviceFileName( LPCTSTR, std::string& );
	static BOOL GetFsFileName( LPCTSTR, std::string& );

	//////////////////////////////////////////////////////////////////////////////////
	// Information functions
	static DWORD GetNTMajorVersion();
	static BOOL Is64BitWindows();
	
};

//////////////////////////////////////////////////////////////////////////////////////
//
// INtDll
//
//////////////////////////////////////////////////////////////////////////////////////

class INtDll
{
public:
	typedef DWORD (WINAPI *PNtQueryObject)( HANDLE, DWORD, VOID*, DWORD, VOID* );
	typedef DWORD (WINAPI *PNtQuerySystemInformation)( DWORD, VOID*, DWORD, ULONG* );
	typedef DWORD (WINAPI *PNtQueryInformationThread)(HANDLE, ULONG, PVOID,	DWORD, DWORD* );
	typedef DWORD (WINAPI *PNtQueryInformationFile)(HANDLE, PVOID,	PVOID, DWORD, DWORD );
	typedef DWORD (WINAPI *PNtQueryInformationProcess)(HANDLE, DWORD, PVOID, DWORD, PVOID );
	
public:
	static PNtQuerySystemInformation	NtQuerySystemInformation;
	static PNtQueryObject				NtQueryObject;
	static PNtQueryInformationThread	NtQueryInformationThread;
	static PNtQueryInformationFile		NtQueryInformationFile;
	static PNtQueryInformationProcess	NtQueryInformationProcess;

	static BOOL							NtDllStatus;

	static DWORD						dwNTMajorVersion;

protected:
	static BOOL Init();
};

//////////////////////////////////////////////////////////////////////////////////////
//
// SystemProcessInformation
//
//////////////////////////////////////////////////////////////////////////////////////

class SystemProcessInformation : public INtDll
{
public:
	typedef LARGE_INTEGER   QWORD;

	typedef struct _PROCESS_BASIC_INFORMATION {
			DWORD ExitStatus;
			PVOID PebBaseAddress;
			DWORD AffinityMask;
			DWORD BasePriority;
			DWORD UniqueProcessId;
			DWORD InheritedFromUniqueProcessId;
		} PROCESS_BASIC_INFORMATION;

	typedef struct _VM_COUNTERS
		{
		DWORD PeakVirtualSize;
		DWORD VirtualSize;
		DWORD PageFaultCount;
		DWORD PeakWorkingSetSize;
		DWORD WorkingSetSize;
		DWORD QuotaPeakPagedPoolUsage;
		DWORD QuotaPagedPoolUsage;
		DWORD QuotaPeakNonPagedPoolUsage;
		DWORD QuotaNonPagedPoolUsage;
		DWORD PagefileUsage;
		DWORD PeakPagefileUsage;
		} VM_COUNTERS;

	typedef struct _SYSTEM_THREAD
		{
		DWORD        u1;
		DWORD        u2;
		DWORD        u3;
		DWORD        u4;
		DWORD        ProcessId;
		DWORD        ThreadId;
		DWORD        dPriority;
		DWORD        dBasePriority;
		DWORD        dContextSwitches;
		DWORD        dThreadState;      // 2=running, 5=waiting
		DWORD        WaitReason;
		DWORD        u5;
		DWORD        u6;
		DWORD        u7;
		DWORD        u8;
		DWORD        u9;
		} SYSTEM_THREAD;

	typedef struct _SYSTEM_PROCESS_INFORMATION
		{
		DWORD          dNext;
		DWORD          dThreadCount;
		DWORD          dReserved01;
		DWORD          dReserved02;
		DWORD          dReserved03;
		DWORD          dReserved04;
		DWORD          dReserved05;
		DWORD          dReserved06;
		QWORD          qCreateTime;
		QWORD          qUserTime;
		QWORD          qKernelTime;
		UNICODE_STRING usName;
		DWORD	       BasePriority;
		DWORD          dUniqueProcessId;
		DWORD          dInheritedFromUniqueProcessId;
		DWORD          dHandleCount;
		DWORD          dReserved07;
		DWORD          dReserved08;
		VM_COUNTERS    VmCounters;
		DWORD          dCommitCharge;
		SYSTEM_THREAD  Threads[1];
		} SYSTEM_PROCESS_INFORMATION;

	enum { BufferSize = 0x10000 };

public:
	SystemProcessInformation( BOOL bRefresh = FALSE );
	virtual ~SystemProcessInformation();

	BOOL Refresh();

public:
	// CMap< DWORD, DWORD&, SYSTEM_PROCESS_INFORMATION*, SYSTEM_PROCESS_INFORMATION*> m_ProcessInfos;
	std::map< DWORD, SYSTEM_PROCESS_INFORMATION* > m_ProcessInfos;
	SYSTEM_PROCESS_INFORMATION* m_pCurrentProcessInfo;

protected:
	UCHAR*						m_pBuffer;
};


//////////////////////////////////////////////////////////////////////////////////////
//
// SystemHandleInformation
//
//////////////////////////////////////////////////////////////////////////////////////

class SystemHandleInformation : public INtDll
{
public:
	enum {
		OB_TYPE_UNKNOWN = 0,
		OB_TYPE_TYPE = 1,
		OB_TYPE_DIRECTORY,
		OB_TYPE_SYMBOLIC_LINK,
		OB_TYPE_TOKEN,
		OB_TYPE_PROCESS,
		OB_TYPE_THREAD,
		OB_TYPE_UNKNOWN_7,
		OB_TYPE_EVENT,
		OB_TYPE_EVENT_PAIR,
		OB_TYPE_MUTANT,
		OB_TYPE_UNKNOWN_11,
		OB_TYPE_SEMAPHORE,
		OB_TYPE_TIMER,
		OB_TYPE_PROFILE,
		OB_TYPE_WINDOW_STATION,
		OB_TYPE_DESKTOP,
		OB_TYPE_SECTION,
		OB_TYPE_KEY,
		OB_TYPE_PORT,
		OB_TYPE_WAITABLE_PORT,
		OB_TYPE_UNKNOWN_21,
		OB_TYPE_UNKNOWN_22,
		OB_TYPE_UNKNOWN_23,
		OB_TYPE_UNKNOWN_24,
		//OB_TYPE_CONTROLLER,
		//OB_TYPE_DEVICE,
		//OB_TYPE_DRIVER,
		OB_TYPE_IO_COMPLETION,
		OB_TYPE_FILE                        
	} SystemHandleType;

public:
	typedef struct _SYSTEM_HANDLE
	{
		DWORD	ProcessID;
		WORD	HandleType;
		WORD	HandleNumber;
		DWORD	KernelAddress;
		DWORD	Flags;
	} SYSTEM_HANDLE;

	typedef struct _SYSTEM_HANDLE_INFORMATION
	{
		DWORD			Count;
		SYSTEM_HANDLE	Handles[1];
	} SYSTEM_HANDLE_INFORMATION;

protected:
	typedef struct _GetFileNameThreadParam
	{
		HANDLE		hFile;
		std::string*	pName;
		ULONG		rc;
	} GetFileNameThreadParam;

public:
	SystemHandleInformation( DWORD pID = (DWORD)-1, BOOL bRefresh = FALSE, LPCTSTR lpTypeFilter = NULL );
	~SystemHandleInformation();

	BOOL SetFilter( LPCTSTR lpTypeFilter, BOOL bRefresh = TRUE );
	const std::string& GetFilter();
	
	BOOL Refresh();

public:
	//Information functions
	static BOOL GetType( HANDLE, WORD&, DWORD processId = GetCurrentProcessId() );
	static BOOL GetTypeToken( HANDLE, std::string&, DWORD processId = GetCurrentProcessId() );
	static BOOL GetTypeFromTypeToken( LPCTSTR typeToken, WORD& type );
	static BOOL GetNameByType( HANDLE, WORD, std::string& str, DWORD processId = GetCurrentProcessId());
	static BOOL GetName( HANDLE, std::string&, DWORD processId = GetCurrentProcessId() );

	//Thread related functions
	static BOOL GetThreadId( HANDLE, DWORD&, DWORD processId = GetCurrentProcessId() );

	//Process related functions
	static BOOL GetProcessId( HANDLE, DWORD&, DWORD processId = GetCurrentProcessId() );
	static BOOL GetProcessPath( HANDLE h, std::string& strPath, DWORD processId = GetCurrentProcessId());

	//File related functions
	static BOOL GetFileName( HANDLE, std::string&, DWORD processId = GetCurrentProcessId() );

public:
	//For remote handle support
	static HANDLE OpenProcess( DWORD processId );
	static HANDLE DuplicateHandle( HANDLE hProcess, HANDLE hRemote );

protected:
	static void GetFileNameThread( PVOID /* GetFileNameThreadParam* */ );
	BOOL IsSupportedHandle( SYSTEM_HANDLE& handle );

public:
	std::vector<SYSTEM_HANDLE *> m_HandleInfos;
	DWORD	m_processId;

protected:
	std::string	m_strTypeFilter;
};

#endif
