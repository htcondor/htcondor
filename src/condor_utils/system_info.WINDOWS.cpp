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
// SystemInfo.cpp v1.1
//
// History:
// 
// Date      Version     Description
// --------------------------------------------------------------------------------
// 10/16/00	 1.0	     Initial version
// 11/09/00  1.1         NT4 doesn't like if we bother the System process fix :)
//                       SystemInfoUtils::GetDeviceFileName() fix (subst drives added)
//                       NT Major version added to INtDLL class
//
//////////////////////////////////////////////////////////////////////////////////////

#include "condor_common.h"
#include "condor_debug.h"
#include <process.h>
#include "system_info.WINDOWS.h"

/*
#ifndef WINNT
#error You need Windows NT to use this source code. Define WINNT!
#endif
*/

typedef struct _BASIC_THREAD_INFORMATION {
	DWORD u1;
	DWORD u2;
	DWORD u3;
	DWORD ThreadId;
	DWORD u5;
	DWORD u6;
	DWORD u7;
} BASIC_THREAD_INFORMATION;

//////////////////////////////////////////////////////////////////////////////////////
//
// SystemInfoUtils
//
//////////////////////////////////////////////////////////////////////////////////////

// From wide char string to MyString
void SystemInfoUtils::LPCWSTR2MyString( LPCWSTR strW, MyString& str )
{
#ifdef UNICODE
	// if it is already UNICODE, no problem
	str = strW;
#else
	str = "";

	TCHAR* actChar = (TCHAR*)strW;

	if ( actChar == '\0' )
		return;

	size_t len = wcslen(strW) + 1;
	TCHAR* pBuffer = new TCHAR[ len ];
	TCHAR* pNewStr = pBuffer;

	while ( len-- )
	{
		*(pNewStr++) = *actChar;
		actChar += 2;
	}

	str = pBuffer;

	delete [] pBuffer;
#endif
}

// From wide char string to unicode
void SystemInfoUtils::Unicode2MyString( UNICODE_STRING* strU, MyString& str )
{
	if ( *(DWORD*)strU != 0 )
		LPCWSTR2MyString( (LPCWSTR)strU->Buffer, str );
	else
		str = "";
}

// From device file name to DOS filename
BOOL SystemInfoUtils::GetFsFileName( LPCTSTR lpDeviceFileName, MyString& fsFileName )
{
	BOOL rc = FALSE;

	TCHAR lpDeviceName[0x1000];
	TCHAR lpDrive[3] = "A:";

	// Iterating through the drive letters
	for ( TCHAR actDrive = 'A'; actDrive <= 'Z'; actDrive++ )
	{
		lpDrive[0] = actDrive;

		// Query the device for the drive letter
		if ( QueryDosDevice( lpDrive, lpDeviceName, 0x1000 ) != 0 )
		{
			// Network drive?
			if ( strnicmp( "\\Device\\LanmanRedirector\\", lpDeviceName, 25 ) == 0 )
			{
				//Mapped network drive 

				char cDriveLetter;
				DWORD dwParam;

				TCHAR lpSharedName[0x1000];

				if ( sscanf(  lpDeviceName, 
								"\\Device\\LanmanRedirector\\;%c:%d\\%s", 
								&cDriveLetter, 
								&dwParam, 
								lpSharedName ) != 3 )
						continue;

				strcpy( lpDeviceName, "\\Device\\LanmanRedirector\\" );
				strcat( lpDeviceName, lpSharedName );
			}
			
			// Is this the drive letter we are looking for?
			if ( strnicmp( lpDeviceName, lpDeviceFileName, strlen( lpDeviceName ) ) == 0 )
			{
				fsFileName = lpDrive;
				fsFileName += (LPCTSTR)( lpDeviceFileName + strlen( lpDeviceName ) );

				rc = TRUE;

				break;
			}
		}
	}

	return rc;
}

// From DOS file name to device file name
BOOL SystemInfoUtils::GetDeviceFileName( LPCTSTR lpFsFileName, MyString& deviceFileName )
{
	BOOL rc = FALSE;
	TCHAR lpDrive[3];

	// Get the drive letter 
	// unfortunetaly it works only with DOS file names
	strncpy( lpDrive, lpFsFileName, 2 );
	lpDrive[2] = '\0';

	TCHAR lpDeviceName[0x1000];

	// Query the device for the drive letter
	if ( QueryDosDevice( lpDrive, lpDeviceName, 0x1000 ) != 0 )
	{
		// Subst drive?
		if ( strnicmp( "\\??\\", lpDeviceName, 4 ) == 0 )
		{
			deviceFileName = lpDeviceName + 4;
			deviceFileName += lpFsFileName + 2;

			return TRUE;
		}
		else
		// Network drive?
		if ( strnicmp( "\\Device\\LanmanRedirector\\", lpDeviceName, 25 ) == 0 )
		{
			//Mapped network drive 

			char cDriveLetter;
			DWORD dwParam;

			TCHAR lpSharedName[0x1000];

			if ( sscanf(  lpDeviceName, 
							"\\Device\\LanmanRedirector\\;%c:%d\\%s", 
							&cDriveLetter, 
							&dwParam, 
							lpSharedName ) != 3 )
					return FALSE;

			strcpy( lpDeviceName, "\\Device\\LanmanRedirector\\" );
			strcat( lpDeviceName, lpSharedName );
		}
		strcat( lpDeviceName, lpFsFileName + 2 );
		deviceFileName = lpDeviceName;

		rc = TRUE;
	}

	return rc;
}

//Get NT version
DWORD SystemInfoUtils::GetNTMajorVersion()
{
   OSVERSIONINFOEX osvi;
   BOOL bOsVersionInfoEx;
   
   // Try calling GetVersionEx using the OSVERSIONINFOEX structure,
   // which is supported on Windows 2000.
   //
   // If that fails, try using the OSVERSIONINFO structure.

   ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
   osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

   MSC_SUPPRESS_WARNING_FOREVER(4996) // 'GetVersionExA': was declared deprecated
   bOsVersionInfoEx = GetVersionEx ((OSVERSIONINFO *) &osvi);

   if( bOsVersionInfoEx == 0 )
   {
      // If OSVERSIONINFOEX doesn't work, try OSVERSIONINFO.

      osvi.dwOSVersionInfoSize = sizeof (OSVERSIONINFO);

      MSC_SUPPRESS_WARNING_FOREVER(4996) // 'GetVersionExA': was declared deprecated
      if (! GetVersionEx ( (OSVERSIONINFO *) &osvi) ) 
         return FALSE;
   }

   return osvi.dwMajorVersion;
}

// Determine if we're running on 64-bit windows.
BOOL SystemInfoUtils::Is64BitWindows()
{
#if defined(_WIN64)
 return TRUE;  // 64-bit programs run only on Win64
#elif defined(_WIN32)
 // 32-bit programs run on both 32-bit and 64-bit Windows
 // so must sniff
 BOOL f64 = FALSE;
 return IsWow64Process(GetCurrentProcess(), &f64) && f64;
#else
 return FALSE; // Win64 does not support Win16
#endif
}

//////////////////////////////////////////////////////////////////////////////////////
//
// INtDll
//
//////////////////////////////////////////////////////////////////////////////////////
INtDll::PNtQuerySystemInformation INtDll::NtQuerySystemInformation = NULL;
INtDll::PNtQueryObject INtDll::NtQueryObject = NULL;
INtDll::PNtQueryInformationThread	INtDll::NtQueryInformationThread = NULL;
INtDll::PNtQueryInformationFile	INtDll::NtQueryInformationFile = NULL;
INtDll::PNtQueryInformationProcess INtDll::NtQueryInformationProcess = NULL;
DWORD INtDll::dwNTMajorVersion = SystemInfoUtils::GetNTMajorVersion();

BOOL INtDll::NtDllStatus = INtDll::Init();

BOOL INtDll::Init()
{
	// Get the NtDll function pointers
	HMODULE hmod = GetModuleHandle( "ntdll.dll" );
	if (hmod)
	{
		NtQuerySystemInformation = (PNtQuerySystemInformation)
					GetProcAddress( hmod, "NtQuerySystemInformation" );

		NtQueryObject = (PNtQueryObject)
					GetProcAddress(	hmod, "NtQueryObject" );

		NtQueryInformationThread = (PNtQueryInformationThread)
					GetProcAddress(	hmod, "NtQueryInformationThread" );

		NtQueryInformationFile = (PNtQueryInformationFile)
					GetProcAddress(	hmod, "NtQueryInformationFile" );

		NtQueryInformationProcess = (PNtQueryInformationProcess)
					GetProcAddress(	hmod, "NtQueryInformationProcess" );
	}

	return  NtQuerySystemInformation	!= NULL &&
			NtQueryObject				!= NULL &&
			NtQueryInformationThread	!= NULL &&
			NtQueryInformationFile		!= NULL &&
			NtQueryInformationProcess	!= NULL;
}

//////////////////////////////////////////////////////////////////////////////////////
//
// SystemProcessInformation
//
//////////////////////////////////////////////////////////////////////////////////////

size_t hashFunction ( const DWORD &key ) {
	// just use the process id
	return key;
}

SystemProcessInformation::SystemProcessInformation( BOOL bRefresh ) 
: m_ProcessInfos (hashFunction)
{
	m_pBuffer = (UCHAR*)VirtualAlloc (NULL,
						BufferSize, 
						MEM_COMMIT,
						PAGE_READWRITE);

	if ( bRefresh )
		Refresh();
}

SystemProcessInformation::~SystemProcessInformation()
{
	SYSTEM_PROCESS_INFORMATION* ptmp; 
	m_ProcessInfos.startIterations ();
	while ( m_ProcessInfos.iterate ( ptmp ) ) {
		VirtualFree ( ptmp, 0, MEM_RELEASE );
	}
	VirtualFree( m_pBuffer, 0, MEM_RELEASE );
}

BOOL SystemProcessInformation::Refresh()
{
	//m_ProcessInfos.RemoveAll();
	SYSTEM_PROCESS_INFORMATION* ptmp; 
	m_ProcessInfos.startIterations ();
	while ( m_ProcessInfos.iterate ( ptmp ) ) {
		VirtualFree ( ptmp, 0, MEM_RELEASE );
	}
	m_ProcessInfos.clear ();
	m_pCurrentProcessInfo = NULL;

	if ( !NtDllStatus || m_pBuffer == NULL )
		return FALSE;
	
	// query the process information	
	if ( INtDll::NtQuerySystemInformation( 5, m_pBuffer, BufferSize, NULL ) != 0 )
		return FALSE;

	DWORD currentProcessID = GetCurrentProcessId(); //Current Process ID
	

	SYSTEM_PROCESS_INFORMATION* pSysProcess = (SYSTEM_PROCESS_INFORMATION*)m_pBuffer;
	DWORD						remainingBufferSize = BufferSize;
	do 
	{
		DWORD blockSize = ( 0 == pSysProcess->dNext 
			? remainingBufferSize : pSysProcess->dNext ) * sizeof ( BYTE );
		remainingBufferSize -= blockSize;
		SYSTEM_PROCESS_INFORMATION* pNewSysProcess = 
			(SYSTEM_PROCESS_INFORMATION*)VirtualAlloc ( NULL, blockSize, 
			MEM_COMMIT, PAGE_READWRITE );
		ASSERT ( pNewSysProcess );
		memcpy ( pNewSysProcess, pSysProcess, blockSize );		
				
		// fill the process information map
		//m_ProcessInfos.SetAt( pSysProcess->dUniqueProcessId, pSysProcess );
		m_ProcessInfos.insert ( pNewSysProcess->dUniqueProcessId, pNewSysProcess );

		/*
		dprintf ( D_FULLDEBUG, "process map: %d -> 0x%x\n", 
			pNewSysProcess->dUniqueProcessId, 
			pNewSysProcess );
		*/

		// we found this process
		if ( pSysProcess->dUniqueProcessId == currentProcessID )
			m_pCurrentProcessInfo = pSysProcess;

		// get the next process information block
		if ( pSysProcess->dNext != 0 )
			pSysProcess = (SYSTEM_PROCESS_INFORMATION*)((UCHAR*)pSysProcess + pSysProcess->dNext);
		else
			pSysProcess = NULL;

	} while ( pSysProcess != NULL );

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////////////////
//
// SystemHandleInformation
//
//////////////////////////////////////////////////////////////////////////////////////

SystemHandleInformation::SystemHandleInformation( DWORD pID, BOOL bRefresh, LPCTSTR lpTypeFilter )
{
	m_processId = pID;
	
	// Set the filter
	SetFilter( lpTypeFilter, bRefresh );
}

SystemHandleInformation::~SystemHandleInformation()
{
	while ( !m_HandleInfos.IsEmpty () ) {
		VirtualFree ( m_HandleInfos.Current (), 0, MEM_RELEASE );
		m_HandleInfos.DeleteCurrent ();
	}
}

BOOL SystemHandleInformation::SetFilter( LPCTSTR lpTypeFilter, BOOL bRefresh )
{
	// Set the filter ( default = all objects )
	m_strTypeFilter = lpTypeFilter == NULL ? "" : lpTypeFilter;

	return bRefresh ? Refresh() : TRUE;
}

const MyString& SystemHandleInformation::GetFilter()
{
	return m_strTypeFilter;
}

BOOL SystemHandleInformation::IsSupportedHandle( SYSTEM_HANDLE& handle )
{
	//Here you can filter the handles you don't want in the Handle list

	// Windows 2000 supports everything :)
	if ( dwNTMajorVersion >= 5 )
		return TRUE;

	//NT4 System process doesn't like if we bother his internal security :)
	if ( handle.ProcessID == 2 && handle.HandleType == 16 )
		return FALSE;

	return TRUE;
}

BOOL SystemHandleInformation::Refresh()
{
	DWORD size = 0x2000;
	DWORD needed = 0;
	DWORD i = 0;
	BOOL  ret = TRUE;
	MyString strType;

	// m_HandleInfos.RemoveAll();
	while ( !m_HandleInfos.IsEmpty () ) {
		VirtualFree ( m_HandleInfos.Current (), 0, MEM_RELEASE );
		m_HandleInfos.DeleteCurrent ();
	}

	if ( !INtDll::NtDllStatus )
		return FALSE;

	// Allocate the memory for the buffer
	SYSTEM_HANDLE_INFORMATION* pSysHandleInformation = (SYSTEM_HANDLE_INFORMATION*)
				VirtualAlloc( NULL, size, MEM_COMMIT, PAGE_READWRITE );

	if ( pSysHandleInformation == NULL )
		return FALSE;

	// Query the needed buffer size for the objects ( system wide )
	if ( INtDll::NtQuerySystemInformation( 16, pSysHandleInformation, size, &needed ) != 0 )
	{
		if ( needed == 0 )
		{
			ret = FALSE;
			goto cleanup;
		}

		// The size was not enough
		VirtualFree( pSysHandleInformation, 0, MEM_RELEASE );

		pSysHandleInformation = (SYSTEM_HANDLE_INFORMATION*)
				VirtualAlloc( NULL, size = needed + 256, MEM_COMMIT, PAGE_READWRITE );
	}
	
	if ( pSysHandleInformation == NULL )
		return FALSE;

	// Query the objects ( system wide )
	if ( INtDll::NtQuerySystemInformation( 16, pSysHandleInformation, size, NULL ) != 0 )
	{
		ret = FALSE;
		goto cleanup;
	}
	
	// Iterating through the objects
	for ( i = 0; i < pSysHandleInformation->Count; i++ )
	{
		if ( !IsSupportedHandle( pSysHandleInformation->Handles[i] ) )
			continue;
		
		// ProcessId filtering check
		if ( pSysHandleInformation->Handles[i].ProcessID == m_processId || m_processId == (DWORD)-1 )
		{
			BOOL bAdd = FALSE;
			
			if ( m_strTypeFilter == "" )
				bAdd = TRUE;
			else
			{
				// Type filtering
				GetTypeToken( (HANDLE)pSysHandleInformation->Handles[i].HandleNumber, strType, pSysHandleInformation->Handles[i].ProcessID  );

				bAdd = strType == m_strTypeFilter;
			}

			// That's it. We found one.
			if ( bAdd )
			{	
				pSysHandleInformation->Handles[i].HandleType = (WORD)(pSysHandleInformation->Handles[i].HandleType % 256);
				
				SYSTEM_HANDLE* ptmp = (SYSTEM_HANDLE*)VirtualAlloc ( NULL, sizeof ( SYSTEM_HANDLE ), MEM_COMMIT, PAGE_READWRITE );
				ASSERT(ptmp);
				memcpy ( ptmp, &pSysHandleInformation->Handles[i], sizeof ( SYSTEM_HANDLE ) );
				m_HandleInfos.Append ( ptmp );

			}
		}
	}

cleanup:
	
	if ( pSysHandleInformation != NULL )
		VirtualFree( pSysHandleInformation, 0, MEM_RELEASE );

	return ret;
}

HANDLE SystemHandleInformation::OpenProcess( DWORD processId )
{
	// Open the process for handle duplication
	return ::OpenProcess( PROCESS_DUP_HANDLE, TRUE, processId );
}

HANDLE SystemHandleInformation::DuplicateHandle( HANDLE hProcess, HANDLE hRemote )
{
	HANDLE hDup = NULL;

	// Duplicate the remote handle for our process
	::DuplicateHandle( hProcess, hRemote,	GetCurrentProcess(), &hDup,	0, FALSE, DUPLICATE_SAME_ACCESS );

	return hDup;
}

//Information functions
BOOL SystemHandleInformation::GetTypeToken( HANDLE h, MyString& str, DWORD processId )
{
	ULONG size = 0x2000;
	UCHAR* lpBuffer = NULL;
	BOOL ret = FALSE;

	HANDLE handle;
	HANDLE hRemoteProcess = NULL;
	BOOL remote = processId != GetCurrentProcessId();
	
	if ( !NtDllStatus )
		return FALSE;

	if ( remote )
	{
		// Open the remote process
		hRemoteProcess = OpenProcess( processId );
		
		if ( hRemoteProcess == NULL )
			return FALSE;

		// Duplicate the handle
		handle = DuplicateHandle( hRemoteProcess, h );
	}
	else
		handle = h;

	// Query the info size
	INtDll::NtQueryObject( handle, 2, NULL, 0, &size );

	lpBuffer = new UCHAR[size];

	// Query the info size ( type )
	if ( INtDll::NtQueryObject( handle, 2, lpBuffer, size, NULL ) == 0 )
	{
		str = "";
		SystemInfoUtils::LPCWSTR2MyString( (LPCWSTR)(lpBuffer+0x60), str );

		ret = TRUE;
	}

	if ( remote )
	{
		if ( hRemoteProcess != NULL )
			CloseHandle( hRemoteProcess );

		if ( handle != NULL )
			CloseHandle( handle );
	}
	
	if ( lpBuffer != NULL )
		delete [] lpBuffer;

	return ret;
}

BOOL SystemHandleInformation::GetType( HANDLE h, WORD& type, DWORD processId )
{
	MyString strType;

	type = OB_TYPE_UNKNOWN;

	if ( !GetTypeToken( h, strType, processId ) )
		return FALSE;

	return GetTypeFromTypeToken( strType.Value (), type );
}

BOOL SystemHandleInformation::GetTypeFromTypeToken( LPCTSTR typeToken, WORD& type )
{
	const WORD count = 27;
	MyString constStrTypes[count] = { 
		"", "", "Directory", "SymbolicLink", "Token",
		"Process", "Thread", "Unknown7", "Event", "EventPair", "Mutant", 
		"Unknown11", "Semaphore", "Timer", "Profile", "WindowStation",
		"Desktop", "Section", "Key", "Port", "WaitablePort", 
		"Unknown21", "Unknown22", "Unknown23", "Unknown24",
		"IoCompletion", "File" };

	type = OB_TYPE_UNKNOWN;

	for ( WORD i = 1; i < count; i++ )
		if ( constStrTypes[i] == typeToken )
		{
			type = i;
			return TRUE;
		}
		
	return FALSE;
}

BOOL SystemHandleInformation::GetName( HANDLE handle, MyString& str, DWORD processId )
{
	WORD type = 0;

	if ( !GetType( handle, type, processId  ) )
		return FALSE;

	return GetNameByType( handle, type, str, processId );
}

BOOL SystemHandleInformation::GetNameByType( HANDLE h, WORD type, MyString& str, DWORD processId )
{
	ULONG size = 0x2000;
	UCHAR* lpBuffer = NULL;
	BOOL ret = FALSE;

	HANDLE handle;
	HANDLE hRemoteProcess = NULL;
	BOOL remote = processId != GetCurrentProcessId();
	DWORD dwId = 0;
	
	if ( !NtDllStatus )
		return FALSE;

	if ( remote )
	{
		hRemoteProcess = OpenProcess( processId );
		
		if ( hRemoteProcess == NULL )
			return FALSE;

		handle = DuplicateHandle( hRemoteProcess, h );
	}
	else
		handle = h;

	// let's be happy, handle is in our process space, so query the infos :)
	switch( type )
	{
	case OB_TYPE_PROCESS:
		GetProcessId( handle, dwId );
		
		//str.Format( "PID: 0x%X", dwId );
		str.formatstr( "PID: 0x%X", dwId );
			
		ret = TRUE;
		goto cleanup;
		break;

	case OB_TYPE_THREAD:
		GetThreadId( handle, dwId );

		str.formatstr( "TID: 0x%X" , dwId );
				
		ret = TRUE;
		goto cleanup;
		break;

	case OB_TYPE_FILE:
		ret = GetFileName( handle, str );

		// access denied :(
		if ( ret && str == "" )
			goto cleanup;
		break;

	};

	INtDll::NtQueryObject ( handle, 1, NULL, 0, &size );

	// let's try to use the default
	if ( size == 0 )
		size = 0x2000;

	lpBuffer = new UCHAR[size];

	if ( INtDll::NtQueryObject( handle, 1, lpBuffer, size, NULL ) == 0 )
	{
		SystemInfoUtils::Unicode2MyString( (UNICODE_STRING*)lpBuffer, str );
		ret = TRUE;
	}
	
cleanup:

	if ( remote )
	{
		if ( hRemoteProcess != NULL )
			CloseHandle( hRemoteProcess );

		if ( handle != NULL )
			CloseHandle( handle );
	}

	if ( lpBuffer != NULL )
		delete [] lpBuffer;
	
	return ret;
}

//Thread related functions
BOOL SystemHandleInformation::GetThreadId( HANDLE h, DWORD& threadID, DWORD processId )
{
	BASIC_THREAD_INFORMATION ti;
	HANDLE handle;
	HANDLE hRemoteProcess = NULL;
	BOOL remote = processId != GetCurrentProcessId();
	
	if ( !NtDllStatus )
		return FALSE;

	if ( remote )
	{
		// Open process
		hRemoteProcess = OpenProcess( processId );
		
		if ( hRemoteProcess == NULL )
			return FALSE;

		// Duplicate handle
		handle = DuplicateHandle( hRemoteProcess, h );
	}
	else
		handle = h;
	
	// Get the thread information
	if ( INtDll::NtQueryInformationThread( handle, 0, &ti, sizeof(ti), NULL ) == 0 )
		threadID = ti.ThreadId;

	if ( remote )
	{
		if ( hRemoteProcess != NULL )
			CloseHandle( hRemoteProcess );

		if ( handle != NULL )
			CloseHandle( handle );
	}

	return TRUE;
}

//Process related functions
BOOL SystemHandleInformation::GetProcessPath( HANDLE h, MyString& strPath, DWORD remoteProcessId )
{
	h; strPath; remoteProcessId;

	strPath.formatstr( "%d", remoteProcessId );

	return TRUE;
}

BOOL SystemHandleInformation::GetProcessId( HANDLE h, DWORD& processId, DWORD remoteProcessId )
{
	BOOL ret = FALSE;
	HANDLE handle;
	HANDLE hRemoteProcess = NULL;
	BOOL remote = remoteProcessId != GetCurrentProcessId();
	SystemProcessInformation::PROCESS_BASIC_INFORMATION pi;
	
	ZeroMemory( &pi, sizeof(pi) );
	processId = 0;
	
	if ( !NtDllStatus )
		return FALSE;

	if ( remote )
	{
		// Open process
		hRemoteProcess = OpenProcess( remoteProcessId );
		
		if ( hRemoteProcess == NULL )
			return FALSE;

		// Duplicate handle
		handle = DuplicateHandle( hRemoteProcess, h );
	}
	else
		handle = h;

	// Get the process information
	if ( INtDll::NtQueryInformationProcess( handle, 0, &pi, sizeof(pi), NULL) == 0 )
	{
		processId = pi.UniqueProcessId;
		ret = TRUE;
	}

	if ( remote )
	{
		if ( hRemoteProcess != NULL )
			CloseHandle( hRemoteProcess );

		if ( handle != NULL )
			CloseHandle( handle );
	}

	return ret;
}

//File related functions
void SystemHandleInformation::GetFileNameThread( PVOID pParam )
{
	// This thread function for getting the filename
	// if access denied, we hang up in this function, 
	// so if it times out we just kill this thread
	GetFileNameThreadParam* p = (GetFileNameThreadParam*)pParam;

	UCHAR lpBuffer[0x1000];
	DWORD iob[2];
	
	p->rc = INtDll::NtQueryInformationFile( p->hFile, iob, lpBuffer, sizeof(lpBuffer), 9 );

	if ( p->rc == 0 )
		*p->pName = (PCHAR) lpBuffer;
}

BOOL SystemHandleInformation::GetFileName( HANDLE h, MyString& str, DWORD processId )
{
	BOOL ret= FALSE;
	HANDLE hThread = NULL;
	GetFileNameThreadParam tp;
	HANDLE handle;
	HANDLE hRemoteProcess = NULL;
	BOOL remote = processId != GetCurrentProcessId();
	
	if ( !NtDllStatus )
		return FALSE;

	if ( remote )
	{
		// Open process
		hRemoteProcess = OpenProcess( processId );
		
		if ( hRemoteProcess == NULL )
			return FALSE;

		// Duplicate handle
		handle = DuplicateHandle( hRemoteProcess, h );
	}
	else
		handle = h;

	tp.hFile = handle;
	tp.pName = &str;
	tp.rc = 0;

	// tj:2012 is it really necessary to create a thread to call NtQueryInformationFile??"
	// Let's start the thread to get the file name
	hThread = (HANDLE)_beginthread( GetFileNameThread, 0, &tp );

	if ( hThread == NULL )
	{
		ret = FALSE;
		goto cleanup;
	}

	// Wait for finishing the thread
	if ( WaitForSingleObject( hThread, 100 ) == WAIT_TIMEOUT )
	{	
		// Access denied
		// Terminate the thread
		#pragma warning(suppress: 6258) // Using TerminateThread does not allow proper thread clean up
		TerminateThread( hThread, 0 );

		str = "";

		ret = TRUE;
	}
	else
		ret = ( tp.rc == 0 );

cleanup:

	if ( remote )
	{
		if ( hRemoteProcess != NULL )
			CloseHandle( hRemoteProcess );

		if ( handle != NULL )
			CloseHandle( handle );
	}
		
	return ret;
}
