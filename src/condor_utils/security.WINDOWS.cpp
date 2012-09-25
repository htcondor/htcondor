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
/* File Security
***************************************************************/

/* return TRUE on a successful copy; otherwise, FALSE. */
BOOL
GetFileSecurityAttributes (
    PCWSTR w_source, 
    PSECURITY_ATTRIBUTES *security_attributes ) {

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

        if ( !ok ) {
            *security_attributes = NULL;
            delete [] buffer;
            buffer = NULL;
        }

        *security_attributes = destination_attributes;

    }

	return ok;

}

/***************************************************************
 * Token Security
 ***************************************************************/

BOOL 
ModifyPrivilege ( LPCTSTR privilege, BOOL enable ) { 

    
    DWORD               last_error          = ERROR_SUCCESS;
    TOKEN_PRIVILEGES    token_privileges;
    LUID                luid;
    HANDLE              process_token       = INVALID_HANDLE_VALUE;
    BOOL                have_pricess_token  = FALSE,
                        found_privelage     = FALSE,
                        privelages_adjusted = FALSE,
                        ok                  = FALSE;

    __try {
    
        /* we open the current process' ptoken */ 
        have_pricess_token = OpenProcessToken (
            GetCurrentProcess (), 
            TOKEN_READ
            | TOKEN_ADJUST_PRIVILEGES, 
            &process_token );
    
        if ( !have_pricess_token ) { 
        
            last_error = GetLastError ();

            dprintf ( 
                D_FULLDEBUG, 
                "ModifyPrivilege: Failed to retrieve "
                "the process' ptoken. (last-error = %d)\n", 
                last_error ); 
        
            __leave;
        
        }

        /* lookup the local ID for the required privilege */    
        found_privelage = LookupPrivilegeValue (
            NULL, 
            privilege, 
            &luid );

        if ( !found_privelage ) { 
        
            last_error = GetLastError ();
        
            dprintf ( D_FULLDEBUG, "ModifyTokenPrivilege: Failed to "
                "retrieve privilege name. (last-error = %d)\n", 
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
            process_token, 
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

		if ( INVALID_HANDLE_VALUE != process_token ) {
			CloseHandle ( process_token );
		}
    
    }

    return ok; 
    
}

/***************************************************************
 * User Security
 ***************************************************************/

BOOL 
LoadUserSid ( HANDLE token, PSID *sid ) {

    DWORD       last_error  = ERROR_SUCCESS;
    PTOKEN_USER ptoken      = NULL;
    DWORD       size        = 0; /* uneducated guess */
    BOOL        got_token   = FALSE,
                copied      = FALSE,
                ok          = FALSE;
    
    __try {

        /* point the return buffer to nowhere */
        *sid = NULL;

        /* determine how much memory we need */
        GetTokenInformation (
            token,
            TokenUser, 
            NULL,
            0,
            &size );

        if ( size <= 0 ) {
            
            last_error = GetLastError ();
            
            dprintf ( 
                D_FULLDEBUG, 
                "GetUserSid: failed determine how much memory "
                "is required to store TOKEN_USER. "
                "(last-error = %d)\n",
                last_error );
            
            __leave;

        }

        /* try to allocate the buffer for the sid */
        ptoken = (PTOKEN_USER) new BYTE[size];
        
        if ( !ptoken ) {
            
            last_error = GetLastError ();
            
            dprintf ( 
                D_FULLDEBUG, 
                "GetUserSid: failed to allocate memory for "
                "TOKEN_USER. (last-error = %d)\n",
                last_error );
            
            __leave;
            
        }
        
        /* now try and actually get the infromation */
        got_token = GetTokenInformation (
            token,
            TokenUser, 
            ptoken,
            size,
            &size );

        /* make sure we have a copy of the user information */
        if ( !got_token ) {

            last_error = GetLastError ();

            dprintf ( 
                D_FULLDEBUG, 
                "GetUserSid: failed to get user's token information. "
                "(last-error = %d)\n",
                last_error );
            
            __leave;

        }

        /* allocate room to return the token's SID */
        size -= sizeof ( DWORD );
        *sid = (PSID) new BYTE[size];

        if ( !*sid ) {
            
            last_error = GetLastError ();

            dprintf ( 
                D_FULLDEBUG, 
                "GetUserSid: failed to allocate memory for "
                "the SID to be returned. (last-error = %d)\n",
                last_error );

            
            __leave;
            
        }

        /* make a copy of the token's SID */
        copied = CopySid ( 
            size,
            *sid,
            ptoken->User.Sid );

        if ( !copied ) {
            
            last_error = GetLastError ();
            
            dprintf ( 
                D_FULLDEBUG, 
                "GetUserSid: failed to copy the user's SID. "
                "(last-error = %d)\n",
                last_error );
            
            __leave;
            
        }

        /* if we are here, then it's all good */
        ok = TRUE;

    }
    __finally {

        /* propagate the error message */
        SetLastError ( ok ? ERROR_SUCCESS : last_error );

        if ( ptoken ) {
            delete [] ptoken;
        }
        if ( !ok && *sid ) {
            delete [] *sid;
        }

    }

    return ok;

}

void 
UnloadUserSid ( PSID sid ) {
    delete [] sid;
}
