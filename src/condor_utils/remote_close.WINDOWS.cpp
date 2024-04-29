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

#include "condor_common.h"
#include "condor_attributes.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "condor_uid.h"
#include "perm.h"

#include "remote_close.WINDOWS.h "
#include "string_conversion.WINDOWS.h"
#include "system_info.WINDOWS.h"

/* 
The two function  that follow, CloseRemoteHandle() and RemoteCloseA(), 
are derived from the following CodeGuru articles:
    + www.codeguru.com/Cpp/W-P/files/fileio/article.php/c1287/
    + www.codeguru.com/Cpp/W-P/system/processesmodules/article.php/c2827
*/

DWORD 
CloseRemoteHandle ( DWORD pid, HANDLE handle ) {
    
    HANDLE                  process_handle  = NULL,
                            thread_handle   = NULL;
    HMODULE                 kernel32_module = NULL;                            
    LPTHREAD_START_ROUTINE  close_handle_fn = NULL;
    DWORD                   last_error      = 0;
    
    __try {
        
        /* open the remote process */
        process_handle = OpenProcess ( 
            PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION 
            | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, pid );
        
        if ( NULL == process_handle ) {
            last_error = GetLastError ();
            dprintf ( D_FULLDEBUG, "CloseRemoteHandle: OpenProcess "
                "failed (last-error = %u)", last_error );            
            __leave;
        }

        /* load kernel32.dll */
        kernel32_module = LoadLibrary ( "kernel32.dll" );
        ASSERT ( kernel32_module ); /* should never assert :) */

        /* get a pointer to the close handle function */
        close_handle_fn = (LPTHREAD_START_ROUTINE) GetProcAddress ( 
            kernel32_module, "CloseHandle" ); 
        ASSERT ( close_handle_fn ); /* again, this will not fail */

        /* create a thread in the remote process */
        thread_handle = CreateRemoteThread ( process_handle, NULL, 0, 
            close_handle_fn, kernel32_module, 0, &last_error );
        
        if ( NULL == thread_handle ) { 
            /* something is wrong with the privileges, or the process 
            doesn't like us */
            last_error = GetLastError ();
            dprintf ( 
                D_FULLDEBUG, 
                "CloseRemoteHandle: CreateRemoteThread() failed "
                "(last-error = %u)", 
                last_error );            
            __leave;
        }

        /* wait while the handle is closed... as for the time out, 
        well: careful years research has show that 20 units of 
        anything seems to be the right magic number (so why not a 
        multiple of it, too) */
        switch ( WaitForSingleObject ( thread_handle, 2000 ) ) {
        case WAIT_OBJECT_0:
            /* killed the little bugger */
            last_error = 0;
            break;        
        default:
            /* something went wrong. maybe further research into
            the 20 unit heuristic might shed some light on this 
            problem */
            last_error = GetLastError ();
            dprintf ( 
                D_FULLDEBUG, 
                "CloseRemoteHandle: our remote CloseHandle() call "
                "failed (last-error = %u)", 
                last_error );            
            __leave;
            break;
        }

    }
    __finally {

        /* close the remote thread handle */
        if ( thread_handle ) {
            CloseHandle( thread_handle );
        }
        
        /* free up kernel32.dll */
        if ( kernel32_module ) {
            FreeLibrary ( kernel32_module );
        }
        
        /* close the process handle */
        if ( process_handle ) {
            CloseHandle ( process_handle );
        }

    }

    return last_error;

}

/* 
Taken straight from the web-site code listed above, so I take no 
responsibility for it... I only made it use our data structures
rather than MFC ones 

-Ben 2007-09-20
*/

/*
Filter can be any of the following:
 
 "Directory", "SymbolicLink", "Token", "Process", "Thread", 
 "Unknown7", "Event", "EventPair", "Mutant", "Unknown11", 
 "Semaphore", "Timer","Profile", "WindowStation", "Desktop", 
 "Section", "Key", "Port", "WaitablePort", "Unknown21", 
 "Unknown22", "Unknown23", "Unknown24", "IoCompletion", "File" 

 We presently make use of only two of these filters: "Directory" 
 and "File". This may change in future versions, when it may be 
 appropriate to filter for a "SymbolicLink", or otherwise.
*/
DWORD RemoteFindAndCloseA ( PCSTR filename, PCSTR filter )  {

    dprintf ( 
        D_FULLDEBUG, 
        "RemoteFindAndCloseA: name = %s, filter = %s\n", 
        filename, 
        filter );

    DWORD last_error = 0;
	std::string deviceFileName;
	std::string name;
	std::string processName;
    SystemHandleInformation hi;
    SystemProcessInformation pi;
    SystemProcessInformation::SYSTEM_PROCESS_INFORMATION* pPi;
    
    //Convert it to device file name
    if ( !SystemInfoUtils::GetDeviceFileName( filename, deviceFileName ) )
    {
        dprintf( D_FULLDEBUG, "RemoteCloseA: GetDeviceFileName() failed.\n" );
        return last_error;
    }

    //Query every file handle (system wide)
    if ( !hi.SetFilter( filter, TRUE ) )
    {
        dprintf( D_FULLDEBUG, "RemoteCloseA: SystemHandleInformation::SetFilter() failed.\n" );
        return last_error;
    }

    if ( !pi.Refresh() )
    {
        dprintf( D_FULLDEBUG, "RemoteCloseA: SystemProcessInformation::Refresh() failed.\n" );
        return last_error;
    }
    
    //Iterate through the found file handles
	for (auto &h: hi.m_HandleInfos) {

        auto spi_it = pi.m_ProcessInfos.find(h->ProcessID);
        if (spi_it == pi.m_ProcessInfos.end())
            continue;

        pPi = spi_it->second;
        
        if ( pPi == NULL )
            continue;
        
        //Get the process name
        SystemInfoUtils::Unicode2String ( &pPi->usName, processName );

        //what's the file name for this given handle?
        hi.GetName( (HANDLE)h->HandleNumber, name, h->ProcessID );
        
        //This is what we want to delete, so close the handle
        if ( deviceFileName == name ) {

            dprintf ( 
                D_FULLDEBUG, 
                "RemoteCloseA: Closing handle in process '%s' (%u)", 
                processName.c_str (), 
                h->ProcessID );

            last_error = CloseRemoteHandle ( 
                h->ProcessID, 
                (HANDLE) h->HandleNumber );

        }
    }

    return last_error;
    
}

DWORD RemoteFindAndCloseW ( PCWSTR w_filename, PCWSTR w_filter ) {

    PSTR filename = ProduceAFromW ( w_filename ),
         filter   = ProduceAFromW ( w_filter );
    ASSERT ( filename && filter );
    
    DWORD last_error = RemoteFindAndCloseA ( filename, filter );
    
    delete [] filename;
    delete [] filter;
    
    return last_error;

}
    
BOOL RemoteDeleteW ( PCWSTR w_filename, PCWSTR w_filter ) {

    PSTR  filename = NULL,
          filter   = NULL;
    DWORD closed   = ERROR_SUCCESS;
    BOOL  deleted  = FALSE,
          ok       = FALSE;

    __try {

        filename = ProduceAFromW ( w_filename );
        filter   = ProduceAFromW ( w_filter );
        
        dprintf ( 
            D_FULLDEBUG, 
            "RemoteDeleteW: Allocation of file name %s; "
            "Allocation of filter %s."
            "(last-error = %u)\n",
            NULL != filename ? "succeeded" : "failed",
            NULL != filter ? "succeeded" : "failed" );

        if ( !filename || !filter ) {           
            __leave;
        }
        
        /* close any remote handle to this file, returning the value
        of GetLastError(), rather than a simple boolean. */
        closed = RemoteFindAndCloseA ( filename, filter );

        dprintf ( 
            D_FULLDEBUG, 
            "RemoteDeleteW: Closing %s %s. (last-error = %u)\n",
            filename,
            ERROR_SUCCESS == closed ? "succeeded" : "failed",
            closed );

        if ( ERROR_SUCCESS != closed ) {
            __leave;
        } 
        
        deleted = DeleteFile ( filename );

        dprintf ( 
            D_FULLDEBUG, 
            "RemoteDeleteW: Deleting %s %s. "
            "(last-error = %u)\n",
            filename,
            deleted ? "succeeded" : "failed",
            deleted ? 0 : GetLastError () );

        if ( !deleted ) {
            __leave;
        }

        /* if we made it here, then we did our job */
        ok = TRUE;

    }
    __finally {

        if ( filename ) {
            delete [] filename;
        }
        if ( filter ) {
            delete [] filter;
        }

    }
    
    return ok;

}
