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
#include "condor_debug.h"

#include "security.WINDOWS.h"

/***************************************************************
/* Macros                                                       
***************************************************************/

#define condor_roundup(x) (sizeof(DWORD)*(((x)+sizeof(DWORD)-1)/sizeof(DWORD)))

/***************************************************************
/* Generic security functions
***************************************************************/

BOOL 
ModifyTokenPrivilege ( HANDLE token, LPCTSTR privilege, BOOL enable ) { 

    
    DWORD               last_error          = ERROR_SUCCESS;
    TOKEN_PRIVILEGES    token_privileges;
    LUID                luid;
    HANDLE              process_token       = NULL;
    BOOL                found_privelage     = FALSE,
                        privelages_adjusted = FALSE,
                        ok                  = FALSE;

    __try {
    
        /* we open the current process' token */ 
        /*
        have_pricess_token = OpenProcessToken (
            GetCurrentProcess (), 
            TOKEN_ADJUST_PRIVILEGES 
            | TOKEN_QUERY, 
            &process_token );
    
        if ( !have_pricess_token ) { 
        
            last_error = GetLastError ();

            dprintf ( D_FULLDEBUG, "ModifyPrivilege: Failed to retrieve "
                "the process' token. (last-error = %d)\n", 
                last_error ); 
        
            __leave;
        
        } 
        */

        /* lookup the local ID for the required privilege */    
        found_privelage = LookupPrivilegeValue (
            NULL, 
            privilege, 
            &luid );

        if ( !found_privelage ) { 
        
            last_error = GetLastError ();
        
            dprintf ( D_FULLDEBUG, "ModifyPrivilege: Failed to retrieve "
                "privilege name. (last-error = %d)\n", 
                last_error ); 
        
            __leave;
        
        } 
    
        /* initialize the new privileges struct */    
        token_privileges.PrivilegeCount           = 1;     
        token_privileges.Privileges[0].Luid       = luid;     
        token_privileges.Privileges[0].Attributes = enable 
            ? SE_PRIVILEGE_ENABLED : 0; 
    
        /* overwrite the current privileges */
        privelages_adjusted = AdjustTokenPrivileges (
            token, 
            FALSE, 
            &token_privileges, 
            0, 
            NULL, 
            NULL );

        if ( !privelages_adjusted ) { 
        
            last_error = GetLastError ();
        
            dprintf ( D_FULLDEBUG, "ModifyPrivilege: Failed to adjust "
                "privilege. (last-error = %d)\n", 
                last_error ); 
        
            __leave;
        
        } 
    
        /* if we got here, then it's all good */
        ok = TRUE;
    
    }
    __finally {

        /* propagate the last error */
        SetLastError ( ok ? ERROR_SUCCESS : last_error );

        if ( NULL != process_token ) {
            CloseHandle ( process_token ); 
        }
    
    }

    return ok; 
    
}

/***************************************************************
/* File related security functions
***************************************************************/

/* return TRUE on a successful copy; otherwise, FALSE. */
static BOOL
CondorGetSecurityInformation (
    PCWSTR w_source, 
    SECURITY_ATTRIBUTES *security_attributes ) {

    PSECURITY_ATTRIBUTES destination_attributes  = NULL;
    PSECURITY_DESCRIPTOR source_descriptor       = NULL;
    PBYTE                buffer                  = NULL;
    DWORD	             size                    = 0,
                         extra                   = condor_roundup ( sizeof ( PSECURITY_DESCRIPTOR ) ),
                         last_error	             = ERROR_SUCCESS;
	BOOL	             have_security_info      = FALSE,
                         ok                      = FALSE;

	__try {

        /* first, determine the required buffer size */
       have_security_info = GetFileSecurityW ( 
           w_source,
           SACL_SECURITY_INFORMATION 
           | OWNER_SECURITY_INFORMATION
           | GROUP_SECURITY_INFORMATION
           | DACL_SECURITY_INFORMATION,
           NULL,
           0,
           &size );

       if ( !have_security_info ) {

           last_error = GetLastError ();

           __leave;

       }

       /* try and allocated the required buffer size, plus some 
	  wiggle room */
       buffer = new BYTE[size + extra];
       
       if ( !buffer ) {
    
           last_error = GetLastError ();
           
           __leave;

       }
       
       /* translate the buffer into the structure we want */
       destination_attributes = (PSECURITY_ATTRIBUTES) buffer;
       
       /* to simplify deletion, we've allocated everything as 
	  one big block: placing the descriptor at the end of 
	  the attributes structure. this means we only need to 
	  call delete [] once on the returned pointer */
       source_descriptor = (PSECURITY_DESCRIPTOR) 
           ( ( (ULONG) buffer ) + extra );
       destination_attributes->lpSecurityDescriptor = 
           source_descriptor;

       /* now, with the buffers allocated, get the actual 
	  information we are interested in */
       have_security_info = GetFileSecurityW ( 
           w_source,
           SACL_SECURITY_INFORMATION 
           | OWNER_SECURITY_INFORMATION
           | GROUP_SECURITY_INFORMATION
           | DACL_SECURITY_INFORMATION,
           source_descriptor,
           size,
           &size );
       
       if ( !have_security_info ) {
           
           last_error = GetLastError ();
           
           __leave;
           
       }

       /* fill out the remaining parts of the security 
	  attributes structure */
       destination_attributes->nLength = 
           sizeof ( PSECURITY_ATTRIBUTES );
       destination_attributes->bInheritHandle = FALSE;

        /* we made it! */
       ok = TRUE;

}
__finally {

        /* propagate the last error */
        SetLastError ( ok ? ERROR_SUCCESS : last_error );

        if ( ok ) {
            security_attributes = destination_attributes;
        } else {
            security_attributes = NULL;
            delete [] buffer;
            buffer = NULL;
        }

	}

	return ok;

}
