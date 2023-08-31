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
#include "directory.WINDOWS.h "

#include "condor_attributes.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "condor_uid.h"
#include "directory.h"
#include "remote_close.WINDOWS.h"
#include "string_conversion.WINDOWS.h"
#include "security.WINDOWS.h"

#include <winioctl.h>

/***************************************************************
/* Macros                                                       
***************************************************************/

#define condor_countof(x) (sizeof(x)/sizeof(x[0]))

/***************************************************************
/* Constants                                                    
***************************************************************/

static PCWSTR all_pattern = L"*.*",
              dot         = L".",
              ddot        = L"..";

/***************************************************************
/* Types                                                        
***************************************************************/

/* In order to use the stack to store our reparese point 
information, we create a dummy information buffer that is
large enough to hold all the information we would ever 
receive. */
typedef struct {
    REPARSE_GUID_DATA_BUFFER data;
    WCHAR buffer [MAX_PATH*3];
} STACK_REPARSE_GUID_DATA_BUFFER;

/***************************************************************
/* Win32 Message Formating                                      
***************************************************************/

static void 
PrintFormatedErrorMessage ( DWORD last_error, DWORD flags = 0 ) {

    DWORD   length      = 0;
    LPVOID  buffer      = NULL;
    PSTR    message     = NULL;
    BOOL    ok          = FALSE;

    __try {

        /* remove the junk we don't support... this is just for basic
	   message handling */
        flags &= ~( FORMAT_MESSAGE_ARGUMENT_ARRAY 
            | FORMAT_MESSAGE_FROM_HMODULE 
            | FORMAT_MESSAGE_FROM_STRING );

        length = FormatMessage ( 
            flags 
            | FORMAT_MESSAGE_ALLOCATE_BUFFER
            | FORMAT_MESSAGE_FROM_SYSTEM, 
            NULL, 
            last_error, 
            MAKELANGID ( LANG_NEUTRAL, SUBLANG_DEFAULT ), 
            (PSTR) &buffer, 
            0, 
            NULL );

        if ( 0 == length ) {

            last_error = GetLastError ();

            dprintf (
                D_FULLDEBUG,
                "PrintFormatedErrorMessage: failed to retrieve error "
                "message. (last-error = %u)\n", 
                last_error );

            __leave;

        }

        message = (PSTR) buffer;
        message[length - 2] = '\0'; /* remove new-line */

        dprintf ( 
            D_FULLDEBUG, 
            "Reason: %s\n", 
            message );

        /* all went well */
        ok = TRUE;

    }
    __finally {

        /* propagate the last error */
        SetLastError ( ok ? ERROR_SUCCESS : last_error );

        if ( buffer ) {
            LocalFree ( buffer );
            buffer = NULL;
        }

    }

}

/***************************************************************
/* Directory helper functions                                  
***************************************************************/

static PWSTR 
CondorAppendDirectoryToPath (
    PCWSTR w_path, 
    PCWSTR w_directory ) {
    
    SSIZE_T length = 0;
    PWSTR w_new  = NULL;
    
    length = wcslen ( w_path ) 
           + wcslen ( w_directory ) + 1; /* +1 for \ */
    w_new = new WCHAR[length + 1];       /* +1 for \0 */    
    ASSERT ( w_new );
    
    wsprintfW ( 
        w_new, 
        L"%s\\%s", 
        w_path, 
        w_directory );

    return w_new;
}

/***************************************************************
/* File iteration helper functions                             
***************************************************************/

static HANDLE 
CondorFindFirstFileAs (
    priv_state          who,
    LPCWSTR             w_path,
    LPWIN32_FIND_DATAW  find_data ) {

    priv_state  priv        = PRIV_UNKNOWN;
    HANDLE      current     = INVALID_HANDLE_VALUE;
    DWORD       last_error  = ERROR_SUCCESS;
    BOOL        found       = FALSE;
    
    __try { 
        
        /* do the following as the specified privilege state */
        priv = set_priv ( who );

        /* try to find the given path */
        current = FindFirstFileW ( 
            w_path,
            find_data );

        /* simplify the test for success */
        found = ( INVALID_HANDLE_VALUE != current );

        if ( !found ) {

            /* get the last error */
            last_error = GetLastError ();

            dprintf ( 
                D_FULLDEBUG, 
                "CondorFindFirstFileAs: FindFirstFile('%S') when "
                "run as '%s' %s. (last-error = %u)\n", 
                w_path,
                priv_identifier ( who ),
                found ? "succeeded" : "failed", 
                found ? 0 : last_error );

            /* print a human parsable reason for the failure */
            PrintFormatedErrorMessage ( 
                last_error );

            __leave;

        }

    }
    __finally {

        /* propagate the last error */
        SetLastError ( found ? ERROR_SUCCESS : last_error );

        /* return to previous privilege level */
        set_priv ( priv );

    }
    
    return current;

}

static HANDLE 
CondorFindFirstFile (
    LPCWSTR            w_path,
    LPWIN32_FIND_DATAW find_data ) {

    priv_state  privs[]     = { PRIV_USER, PRIV_CONDOR };
    DWORD       size        = condor_countof ( privs ),
                i           = 0,
                last_error  = ERROR_SUCCESS;
    HANDLE      current     = INVALID_HANDLE_VALUE;
    BOOL        found       = FALSE,
                chowned     = FALSE;

    __try { 
        
        /* search for the file using acceding levels 
        of authority */
        for ( i = 0; !found && i < size; ++i ) {

            /* do the actual search */
            current = CondorFindFirstFileAs (
                privs[i],
                w_path,
                find_data );

            /* make a record of the last error */
            last_error = GetLastError ();

            /* simplify the test for success */
            found = ( INVALID_HANDLE_VALUE != current );

        }

    }
    __finally {

        /* propagate the last error */
        SetLastError ( found ? ERROR_SUCCESS : last_error );

    }

    return current;

}

static BOOL 
CondorFindNextFileAs (
    priv_state          who,
    HANDLE              current,
    LPWIN32_FIND_DATAW  find_data ) {

    priv_state  priv        = PRIV_UNKNOWN;
    DWORD       last_error  = ERROR_SUCCESS;
    BOOL        found       = FALSE,
                ok          = FALSE;
    
    __try { 
        
        /* do the following as the specified privilege state */
        priv = set_priv ( who );

        /* try to find the next file */
        found = FindNextFileW ( 
            current,
            find_data );

        if ( !found ) {

            /* get the last error */
            last_error = GetLastError ();

            dprintf ( 
                D_FULLDEBUG, 
                "CondorFindNextFileAs: FindNextFile() when run as "
                "'%s' %s. (last-error = %u)\n", 
                priv_identifier ( who ),
                found ? "succeeded" : "failed", 
                found ? 0 : last_error );

            /* print a human parsable reason for the failure */
            PrintFormatedErrorMessage ( 
                last_error );

            __leave;

        }

        /* we found it */
        ok = TRUE;

    }
    __finally {

        /* propagate the last error */
        SetLastError ( ok ? ERROR_SUCCESS : last_error );

        /* return to previous privilege level */
        set_priv ( priv );

    }
    
    return ok;

}

static BOOL 
CondorFindNextFile (
    HANDLE             current,
    LPWIN32_FIND_DATAW find_data ) {

    priv_state  privs[]     = { PRIV_USER, PRIV_CONDOR };
    DWORD       size        = condor_countof ( privs ),
                i           = 0,
                last_error  = ERROR_SUCCESS;
    BOOL        found       = FALSE;

    __try { 

        for ( i = 0; !found && i < size; ++i ) {
        
            /* do the actual search */
            found = CondorFindNextFileAs (
                privs[i],
                current,
                find_data );

            /* make a record of the last error */
            last_error = GetLastError ();

        }

    }
    __finally {

        /* propagate the last error */
        SetLastError ( found ? ERROR_SUCCESS : last_error );

    }

    return found;

}

/***************************************************************
/* Textbook recursive directory copy                            
/***************************************************************

/* return TRUE on a successful creation; otherwise, FALSE. */
static BOOL
CondorCreateDirectoryAs ( 
    priv_state  who,
    PCWSTR      w_source, 
    PCWSTR      w_destination ) {

    priv_state  priv                = PRIV_UNKNOWN;
    DWORD       last_error          = ERROR_SUCCESS;
    BOOL        directory_created   = FALSE,
                ok                  = FALSE;

    __try {

        /* do the following as the specified privilege state */
        priv = set_priv ( who );

        /* create the destination directory based on the settings 
        of the source directory */
        directory_created = CreateDirectoryExW ( 
            w_source, 
            w_destination, 
            NULL );

        /* bail if we were unable to create the directory */
        if ( !directory_created ) {

            /* get the user's last error */
            last_error = GetLastError ();

            dprintf ( 
                D_FULLDEBUG, 
                "CondorCreateDirectoryAs: Copying directory '%S' as "
                "%s %s. (last-error = %u)\n", 
                w_source,
                priv_identifier ( who ),
                directory_created ? "succeeded" : "failed", 
                directory_created ? 0 : last_error );

            /* print a human parsable reason for the failure */
            PrintFormatedErrorMessage ( 
                last_error );

            __leave;

        }

        /* good to go */
        ok = TRUE;

    }
    __finally {

        /* propagate the last error */
        SetLastError ( ok ? ERROR_SUCCESS : last_error );

        /* return to previous privilege level */
        set_priv ( priv );

    }

    return ok;

}

/* return TRUE on a successful creation; otherwise, FALSE. */
static BOOL
CondorCreateFileAs ( 
     priv_state  who,
     PCWSTR      w_source, 
     PCWSTR      w_destination ) {

     priv_state  priv           = PRIV_UNKNOWN;
     DWORD       last_error     = ERROR_SUCCESS;
     BOOL        file_created   = FALSE,
                 ok             = FALSE;

     __try {

         /* do the following as the specified privilege state */
         priv = set_priv ( who );

         /* create the destination directory based on the settings 
         of the source directory */
         file_created = CopyFileW ( 
             w_source, 
             w_destination, 
             TRUE );

         /* bail if we were unable to create the directory */
         if ( !file_created ) {

             /* get the user's last error */
             last_error = GetLastError ();

             dprintf ( 
                 D_FULLDEBUG, 
                 "CondorCreateFileAs: Copying file '%S' as "
                 "%s %s. (last-error = %u)\n", 
                 w_source,
                 priv_identifier ( who ),
                 file_created ? "succeeded" : "failed", 
                 file_created ? 0 : last_error );

             /* print a human parsable reason for the failure */
             PrintFormatedErrorMessage ( 
                 last_error );

             __leave;

         }

         /* good to go */
         ok = TRUE;

     }
     __finally {

         /* propagate the last error */
         SetLastError ( ok ? ERROR_SUCCESS : last_error );

         /* return to previous privilege level */
         set_priv ( priv );

     }

     return ok;

}

/* return TRUE on a successful copy; otherwise, FALSE. */
static BOOL
CondorTryCopyDirectory (
    PCWSTR w_source, 
    PCWSTR w_destination ) {

    DWORD       last_error          = ERROR_SUCCESS;
    BOOL        directory_created   = FALSE,
                handle_closed       = FALSE,
                ok                  = TRUE; /* assume success for early 
                                          exists on a successful copy */

    __try {

        /* first, we try to create the directory as the user */
        directory_created = CondorCreateDirectoryAs ( 
            PRIV_USER,
            w_source, 
            w_destination );

        last_error = GetLastError ();

        if ( directory_created ) {
            __leave;
        }

        /* next, for good measure we give it a try as Condor 
        (aka SYSTEM) which should, presumably, be able to create
        anything... we should hope. */
        directory_created = CondorCreateDirectoryAs ( 
            PRIV_CONDOR,
            w_source, 
            w_destination );

        last_error = GetLastError ();

        if ( directory_created ) {
            __leave;
        }

        // Don't spawn threads in other processes on the system to
        // close file handles.
        // TODO Finding and logging the processes that have our
        //   directory open and locked would be worthwhile.
#if 0
        /*******************************************************
        NOTE: For future implementations which allow for any
        user to load their profile, what follows bellow is 
        known as a BAD IDEA. If the user in question is logged
        in, running a personal Condor (or the machine is an
        submit and execute node, and the job is matched to it)
        then we may be closing off handles in that user's 
        interactive session. Not exactly what they would expect.
        *******************************************************/

        /* as a last ditch attempt, lets close off whom ever has an 
        open (locked) handle to our directory, as it should only 
        be us who would ever have a lock on it in the first place */
        handle_closed = RemoteFindAndCloseW ( 
            w_destination, 
            L"Directory" );

        last_error = GetLastError ();

        if ( !handle_closed ) {
            ok = FALSE;
            __leave;
        }

        /* once again, we try to copy the directory as the user */
        directory_created = CondorCreateDirectoryAs ( 
            PRIV_USER,
            w_source, 
            w_destination );

        last_error = GetLastError ();       

        if ( directory_created ) {
            __leave;
        }
#endif
         
        /* if we are here, then something went really wrong */
        ok = FALSE;

    }
    __finally {

        /* propagate the last error */
        SetLastError ( ok ? ERROR_SUCCESS : last_error );

    }

    return ok;

}

/* return TRUE on a successful copy; otherwise, FALSE. */
static BOOL
CondorTryCopyFile (
    PCWSTR w_source, 
    PCWSTR w_destination ) {

    DWORD       last_error          = ERROR_SUCCESS;
    BOOL        directory_created   = FALSE,
                handle_closed       = FALSE,
                ok                  = TRUE; /* assume success for early 
                                          exists on a successful copy */

    __try {

        /* first, we try to create the file as the user */
        directory_created = CondorCreateFileAs (
            PRIV_USER,
            w_source, 
            w_destination );

        last_error = GetLastError ();

        if ( directory_created ) {
            __leave;
        }

        /* next, for good measure we give it a try as Condor 
        (aka SYSTEM) which should, presumably, be able to create
        anything... we should hope. */
        directory_created = CondorCreateFileAs ( 
            PRIV_CONDOR,
            w_source, 
            w_destination );

        last_error = GetLastError ();

        if ( directory_created ) {
            __leave;
        }

        // Don't spawn threads in other processes on the system to
        // close file handles.
        // TODO Finding and logging the processes that have our
        //   directory open and locked would be worthwhile.
#if 0
        /*******************************************************
        NOTE: For future implementations which allow for any
        user to load their profile, what follows bellow is 
        known as a BAD IDEA. If the user in question is logged
        in, running a personal Condor (or the machine is an
        submit and execute node, and the job is matched to it)
        then we may be closing off handles in that user's 
        interactive session. Not exactly what they would expect.
        *******************************************************/

        /* as a last ditch attempt, lets close off whom ever has an 
        open (locked) handle to our file, as it should only be us who
        would ever have a lock on it in the first place */
        handle_closed = RemoteFindAndCloseW ( 
            w_destination, 
            L"Directory" );

        last_error = GetLastError ();

        if ( !handle_closed ) {
            ok = FALSE;
            __leave;
        }

        /* once again, we try to copy the file as the user */
        directory_created = CondorCreateFileAs ( 
            PRIV_USER,
            w_source, 
            w_destination );

        last_error = GetLastError ();       

        if ( directory_created ) {
            __leave;
        }
#endif
         
        /* if we are here, then something went really wrong */
        ok = FALSE;

    }
    __finally {

        /* propagate the last error */
        SetLastError ( ok ? ERROR_SUCCESS : last_error );

    }

    return ok;

}

/*
CondorTryReadReparesePoint (
    PCWSTR w_reparese ) {

    DWORD       last_error               = ERROR_SUCCESS;
    HANDLE      source                   = INVALID_HANDLE_VALUE;
    BOOL        io_success               = FALSE,
                deleted                  = FALSE,
                created                  = FALSE,
                ok                       = FALSE;

    __try {

    
    }
    __finally {

    }

    return ok;

}

CondorTryWriteReparesePoint () {

    DWORD       last_error               = ERROR_SUCCESS;
    HANDLE      source                   = INVALID_HANDLE_VALUE;
    BOOL        io_success               = FALSE,
                deleted                  = FALSE,
                created                  = FALSE,
                ok                       = FALSE;
    
    __try {
        
        
    }
    __finally {
        
    }
    
    return ok;
    
}
*/

/* return TRUE on a successful copy; otherwise, FALSE. */
static BOOL
CondorTryCopyReparsePoint (
    PCWSTR w_source, 
    PCWSTR w_destination ) {

    DWORD                          size        = 0,
                                   last_error  = ERROR_SUCCESS;
    HANDLE                         source      = INVALID_HANDLE_VALUE,
                                   destination = INVALID_HANDLE_VALUE;
    STACK_REPARSE_GUID_DATA_BUFFER fake;
    PREPARSE_GUID_DATA_BUFFER      buffer = (PREPARSE_GUID_DATA_BUFFER) &fake;
    BOOL                           io_success  = FALSE,
                                   deleted     = FALSE,
                                   created     = FALSE,
                                   ok          = FALSE;

    __try {

        /*
        FILE_FLAG_BACKUP_SEMANTICS 
        Indicates that the file is being opened or created for a 
        backup or restore operation. The system ensures that the 
        calling process overrides file security checks, provided 
        it has the necessary privileges. The relevant privileges 
        are SE_BACKUP_NAME and SE_RESTORE_NAME. 

        You can also set this flag to obtain a handle to a 
        directory. A directory handle can be passed to some 
        functions in place of a file handle.
        */
 
        /* open the reparse point for reading as if we were a 
        back-up application (which, in this situation, we 
        are... kinda) */
        source = CreateFileW (
            w_source,
            GENERIC_READ,
            0,
            NULL,
            OPEN_EXISTING,
            FILE_FLAG_OPEN_REPARSE_POINT
            | FILE_FLAG_BACKUP_SEMANTICS,
            NULL );

        if ( INVALID_HANDLE_VALUE == source ) {

            last_error = GetLastError ();

            dprintf ( 
                D_FULLDEBUG, 
                "CondorTryCopyReparsePoint: Unable to open source "
                "'%S'. (last-error = %u)\n", 
                w_source, 
                last_error );

            __leave;

        }

        /* read the reparse point's information */
        io_success = DeviceIoControl (
            source,
            FSCTL_GET_REPARSE_POINT,
            NULL,
            0,
            (LPVOID) buffer,
            size,
            &size,
            NULL );

        if ( !io_success ) {

            last_error = GetLastError ();

            dprintf ( 
                D_FULLDEBUG,
                "CondorTryCopyReparsePoint: Unable to get "
                "reparse point information."
                "(last-error = %u)\n",
                last_error );

            __leave;

        }

        /* create the directory which we'll make into a 
        reparse point. this part seems a little  */
        created = CreateDirectoryW (
            w_destination,
            NULL );

        if ( !created ) {
            
            last_error = GetLastError ();

            dprintf ( 
                D_FULLDEBUG, 
                "CondorTryCopyReparsePoint: Unable to create "
                "destination directory '%S'. (last-error = %u)\n", 
                w_destination, 
                last_error );

            __leave;

        }

        /* open the reparse point for reading as if we were a 
        back-up application (which, in this situation, we 
        are... kinda) */
        destination = CreateFileW (
            w_destination,
            GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            FILE_FLAG_OPEN_REPARSE_POINT
            | FILE_FLAG_BACKUP_SEMANTICS,
            NULL );

        if ( INVALID_HANDLE_VALUE == destination ) {

            last_error = GetLastError ();

            dprintf ( 
                D_FULLDEBUG, 
                "CondorTryCopyReparsePoint: Unable to open "
                "destination directory '%S'. (last-error = %u)\n", 
                w_destination, 
                last_error );

            __leave;

        }

        /* write out the reparse point's information */
        io_success = DeviceIoControl (
            destination,
            FSCTL_SET_REPARSE_POINT,
            (LPVOID) buffer,
            size,
            NULL,
            0,
            &size,
            NULL );

        if ( !io_success ) {

            last_error = GetLastError ();

            dprintf ( 
                D_FULLDEBUG, 
                "CondorTryCopyReparsePoint: Unable to set "
                "destination information for '%S'. "
                "(last-error = %u)\n", 
                w_destination, 
                last_error );

            __leave;

        }        

        /* all done */
        ok = TRUE;

    }
    __finally {

        /* propagate the last error */
        SetLastError ( ok ? ERROR_SUCCESS : last_error );

        if ( INVALID_HANDLE_VALUE != source ) {
            CloseHandle ( source );
        }
        if ( INVALID_HANDLE_VALUE != destination ) {
            CloseHandle ( destination );
        }

        if ( buffer ) {
            delete [] buffer;
            buffer = NULL;
        }

    }

    return ok;

}

static BOOL 
CondorCopyDirectoryImpl (
    PCWSTR w_source, 
    PCWSTR w_destination ) {
    
    PSTR                error_message        = NULL;
    PWSTR               w_directory_searcher = NULL,
                        w_new_source         = NULL,
                        w_new_destination    = NULL;                        
    DWORD               length               = 0,
                        directory_attributes = 0,
                        attributes           = 0,
                        last_error           = ERROR_SUCCESS;
    PSTR                file_name            = NULL;
    WIN32_FIND_DATAW    find_data;
    HANDLE              current              = INVALID_HANDLE_VALUE;
    BOOL                entry_created        = FALSE,
                        ok                   = FALSE;
    
    __try {

        /* we want find things *in* w_source no just find w_source, 
        so we append the *.* pattern (the function name may be a 
        little missleading, it simply concatonates the two strings
        separated by a back-slash.) */
        w_directory_searcher = CondorAppendDirectoryToPath (
            w_source, 
            all_pattern );
        ASSERT ( w_directory_searcher );
        
        current = CondorFindFirstFile ( 
            w_directory_searcher, 
            &find_data );

        if ( INVALID_HANDLE_VALUE == current ) {

            last_error = GetLastError ();

            if ( ERROR_NO_MORE_FILES == last_error ) {

                /* we're legitimately done in this directory */
                ok = TRUE;
                __leave;

            } 

            dprintf ( 
                D_FULLDEBUG, 
                "CondorCopyDirectoryImpl: Unable to search in "
                "'%S'. (last-error = %u)\n", 
                w_source, 
                last_error );

            dprintf ( 
                D_FULLDEBUG, 
                "Trace:\n" );

            __leave;

        }
        
        do {

            if ( !( MATCH == wcscmp ( find_data.cFileName, dot ) 
                 || MATCH == wcscmp ( find_data.cFileName, ddot ) ) ) {

                w_new_source = CondorAppendDirectoryToPath ( 
                    w_source, 
                    find_data.cFileName );
                ASSERT ( w_new_source );
                
                w_new_destination = CondorAppendDirectoryToPath ( 
                    w_destination, 
                    find_data.cFileName );
                ASSERT ( w_new_destination );

                /* reset to detect successful creation */
                entry_created = FALSE;

				/* remove any attributes that may inhibit 
                a deletion */
                attributes = find_data.dwFileAttributes 
					& ~( FILE_ATTRIBUTE_HIDDEN | 
					     FILE_ATTRIBUTE_READONLY | 
                         FILE_ATTRIBUTE_SYSTEM );
                
                SetFileAttributesW ( 
                    w_destination, 
                    attributes );
                
                if ( FILE_ATTRIBUTE_REPARSE_POINT 
                    & find_data.dwFileAttributes ) {

                    /* create the destination junKtion based on 
                    the source's settings */
                    entry_created = CondorTryCopyReparsePoint (
                        w_new_source,
                        w_new_destination );

                } else if ( FILE_ATTRIBUTE_DIRECTORY 
                    & find_data.dwFileAttributes ) {

                    /* create the destination directory based on 
                    the source's settings */
                    entry_created = CondorTryCopyDirectory (
                        w_new_source,
                        w_new_destination );
                    
                    if ( !entry_created ) {

                        last_error = GetLastError ();

                        dprintf ( 
                            D_FULLDEBUG, 
                            "CondorCopyDirectoryImpl: "
                            "Unable to copy the directory '%S'. "
                            "(last-error = %u)\n", 
                            find_data.cFileName, 
                            last_error );

                        __leave;

                    }
                        
                    /* recurse to the next level */
                    entry_created = CondorCopyDirectoryImpl ( 
                        w_new_source, 
                        w_new_destination );
                    
                } else {

                    /* it's just a file, so copy it */
                    entry_created = CondorTryCopyFile ( 
                        w_new_source, 
                        w_new_destination );

                }

                last_error = GetLastError ();
                
                dprintf ( 
                    D_FULLDEBUG, 
                    "CondorCopyDirectoryImpl: "
                    "Copying '%S' %s. (last-error = %u)\n", 
                    find_data.cFileName, 
                    entry_created ? "succeeded" : "failed", 
                    last_error );

				/* re-apply the file attributes that we removed */
				SetFileAttributesW ( 
                    w_destination, 
                    find_data.dwFileAttributes );

                /*******************************************************
                NOTE: For future implementations which allow for any
                user to load their profile, what follows bellow is 
                known as a BAD IDEA. This would not preserve all of the
                user's data.
                *******************************************************/
                
                /* only fail when a fs object cannot be created */
                if ( !entry_created ) {
                    __leave;
                }
                
                delete [] w_new_source;    
                w_new_source = NULL;
                delete [] w_new_destination;
                w_new_destination = NULL;

            } 

        } while ( CondorFindNextFile ( current, &find_data ) );

        /* all done */
        ok = TRUE;

    }
    __finally {

        /* propagate the last error */
        SetLastError ( ok ? ERROR_SUCCESS : last_error );
        
        if ( INVALID_HANDLE_VALUE != current ) {
            FindClose ( current );
        }

        if ( w_directory_searcher ) {
            delete [] w_directory_searcher;
        }
        if ( w_new_source ) {
            delete [] w_new_source;
        }
        if ( w_new_destination ) {
            delete [] w_new_destination;
        }

    }

    return ok;

}

/* returns TRUE if the destination is populated with the source file
hierarchy; otherwise, FALSE */
BOOL 
CondorCopyDirectory ( 
    PCSTR source, 
    PCSTR destination ) {
    
    priv_state  priv          = PRIV_UNKNOWN;
    PWSTR       w_source      = NULL,
                w_destination = NULL;
    DWORD       last_error    = ERROR_SUCCESS,
                i;
    HANDLE      have_access   = NULL;
    LPCTSTR     privelages[]  = { SE_BACKUP_NAME, SE_RESTORE_NAME };
    BOOL        opened        = TRUE,
                added         = FALSE,
                copied        = FALSE,
                removed       = FALSE,
                ok            = FALSE;
    
    __try {
        
        /* do the following as the user */
        priv = set_user_priv ();
        
        dprintf ( 
            D_FULLDEBUG, 
            "CondorCopyDirectory: Source: '%s'.\n", 
            source );
        
        dprintf ( 
            D_FULLDEBUG, 
            "CondorCopyDirectory: Destination: '%s'.\n", 
            destination );
        
        /* give the user a few extra privelages so it can handle
        reparse points, et. al. */
        for ( i = 0; i < condor_countof ( privelages ); ++i ) {

            added = ModifyPrivilege ( 
                privelages[i], 
                TRUE );
            
            if ( !added ) {
                
                last_error = GetLastError ();
                
                dprintf ( 
                    D_FULLDEBUG, 
                    "CondorRemoveDirectory: Failed to add "
                    "'%s' privilege.\n",
                    privelages[i] ); 
                
                __leave;
            
            }
                
        }

        /* create wide versions of the strings */
        w_source      = ProduceWFromA ( source );
        w_destination = ProduceWFromA ( destination );
        ASSERT ( w_source );
        ASSERT ( w_destination );

        /* check if the destination already exists first */
        have_access = CreateFile ( 
            destination, 
            GENERIC_WRITE, 
            0,                          /* NOT shared */
            NULL, 
            OPEN_EXISTING,              /* do not create it */
            FILE_FLAG_BACKUP_SEMANTICS, /* only take a peek */
            NULL );

        if ( INVALID_HANDLE_VALUE == have_access ) {

            last_error = GetLastError ();

            if ( ERROR_ACCESS_DENIED == last_error ) {

                /* We don't have access, lets just blow it away
                and copy over it */
                removed = CondorRemoveDirectory ( 
                    destination );

                last_error = GetLastError ();

                dprintf ( 
                    D_FULLDEBUG, 
                    "CondorCopyDirectory: Removing locked directory "
                    "'%s' %s. (last-error = %u)\n", 
                    destination,
                    removed ? "succeeded" : "failed", 
                    removed ? 0 : last_error );

                if ( !removed ) {
                    __leave;
                }

            } 
        
        }

        /* clean-up after ourselves if we get here; otherwise, it
        is taken care of once the function terminates. */
        CloseHandle ( have_access );

        /* create the destination directory based on 
        the source's settings */
        copied = CondorTryCopyDirectory (
            w_source,
            w_destination );

        if ( copied ) {
            
            /* do the actual work */
            copied = CondorCopyDirectoryImpl ( 
                w_source, 
                w_destination );

        }

        if ( !copied ) {

            last_error = GetLastError ();
            
            dprintf ( 
                D_FULLDEBUG, 
                "CondorCopyDirectory: Unable to copy from '%S' to "
                "'%S' (last-error = %u)\n", 
                w_source, 
                w_destination, 
                last_error );

            __leave;

        }

        /* we made it! */
        ok = TRUE;

    }
    __finally {

        /* revoke the privelages we gave the user */
        for ( i = 0; i < condor_countof ( privelages ); ++i ) {
            
            removed = ModifyPrivilege ( 
                privelages[i], 
                FALSE );
            
            if ( !removed ) {
                
                dprintf ( 
                    D_FULLDEBUG, 
                    "CondorCopyDirectory: Failed to remove "
                    "'%s' privilege. (last-error = %d)\n",
                    privelages[i],
                    GetLastError () ); 
                
            }
            
        }
        
        /* propagate the last error */
        SetLastError ( ok ? ERROR_SUCCESS : last_error );

        if ( w_source ) {
            delete [] w_source;
            w_source = NULL;
        }
        if ( w_destination ) {
            delete [] w_destination;
            w_destination = NULL;
        }

        /* return to previous privilege level */
        set_priv ( priv );

    }

    return ok;

}

/***************************************************************
/* Textbook recursive directory/file deletion                   
***************************************************************/

/***************************************************************
/* Directory part                                               
***************************************************************/

static BOOL
CondorRemoveDirectoryAs ( 
     priv_state  who,
     const char *name,
     PCWSTR      w_path ) {

     priv_state  priv       = PRIV_UNKNOWN;
     DWORD       last_error = ERROR_SUCCESS;
     BOOL        removed    = FALSE,
                 ok         = FALSE;

     __try {

         /* do the following as the specified privilege state */
         priv = set_priv ( who );

         /* delete the destination directory */
         removed = RemoveDirectoryW ( 
             w_path );

         /* bail if we were unable to create the directory */
         if ( !removed ) {

             /* get the user's last error */
             last_error = GetLastError ();

             dprintf ( 
                 D_FULLDEBUG, 
                 "CondorRemoveDirectoryAs: Removing directory '%S' as "
                 "%s %s. (last-error = %u)\n", 
                 w_path,
                 name,
                 removed ? "succeeded" : "failed", 
                 removed ? 0 : last_error );

             /* print a human parsable reason for the failure */
             PrintFormatedErrorMessage ( 
                 last_error );

             __leave;

         }

         /* good to go */
         ok = TRUE;

     }
     __finally {

         /* propagate the last error */
         SetLastError ( ok ? ERROR_SUCCESS : last_error );

         /* return to previous privilege level */
         set_priv ( priv );

     }

     return ok;

}

static BOOL
CondorRemoveFileAs ( 
     priv_state  who,
     const char *name,
     PCWSTR      w_path ) {

     priv_state  priv       = PRIV_UNKNOWN;
     DWORD       last_error = ERROR_SUCCESS;
     BOOL        removed    = FALSE,
                 ok         = FALSE;

     __try {

         /* do the following as the specified privilege state */
         priv = set_priv ( who );

         /* delete the file */
         removed = DeleteFileW ( 
             w_path );

         /* bail if we were unable to create the file */
         if ( !removed ) {

             /* get the user's last error */
             last_error = GetLastError ();

             dprintf ( 
                 D_FULLDEBUG, 
                 "CondorRemoveDirectoryAs: Removing file '%S' as "
                 "%s %s. (last-error = %u)\n", 
                 w_path,
                 name,
                 removed ? "succeeded" : "failed", 
                 removed ? 0 : last_error );

             /* print a human parsable reason for the failure */
             PrintFormatedErrorMessage ( 
                 last_error );

             __leave;

         }

         /* good to go */
         ok = TRUE;

     }
     __finally {

         /* propagate the last error */
         SetLastError ( ok ? ERROR_SUCCESS : last_error );

         /* return to previous privilege level */
         set_priv ( priv );

     }

     return ok;

}

/* return TRUE on a successful copy; otherwise, FALSE. */
static BOOL
CondorTryRemoveDirectory (
    PCWSTR w_path ) {

    DWORD       last_error          = ERROR_SUCCESS;
    BOOL        directory_removed   = FALSE,
                handle_closed       = FALSE,
                ok                  = TRUE; /* assume success for early 
                                          exists on a successful copy */

    __try {

        /* first, we try to remove the directory as the user */
        directory_removed = CondorRemoveDirectoryAs ( 
            PRIV_USER,
            "user",
            w_path );

        last_error = GetLastError ();

        if ( directory_removed ) {
            __leave;
        }

        /* next, for good measure we give it a try as Condor which 
        should, presumably, be able to delete anything. */
        directory_removed = CondorRemoveDirectoryAs ( 
            PRIV_CONDOR,
            "SYSTEM",
            w_path );

        last_error = GetLastError ();

        if ( directory_removed ) {
            __leave;
        }

        // Don't spawn threads in other processes on the system to
        // close file handles.
        // TODO Finding and logging the processes that have our
        //   directory open and locked would be worthwhile.
#if 0
        /*******************************************************
        NOTE: For future implementations which allow for any
        user to load their profile, what follows bellow is 
        known as a BAD IDEA. If the user in question is logged
        in, running a personal Condor (or the machine is a
        submit and execute node, and the job is matched to it)
        then we may be closing off handles in that user's 
        interactive session. Not exactly what they would expect.
        *******************************************************/

        /* as a last ditch attempt, lets close off whom ever has an 
        open (locked) handle to our directory, as it should only 
        be us who would ever have a lock on it in the first place */
        handle_closed = RemoteFindAndCloseW ( 
            w_path, 
            L"Directory" );

        last_error = GetLastError ();

        if ( !handle_closed ) {
            ok = FALSE;
            __leave;
        }

        /* once again, we try to copy the directory as the user */
        directory_removed = CondorRemoveDirectoryAs ( 
            PRIV_USER,
            "user",
            w_path );

        last_error = GetLastError ();       

        if ( directory_removed ) {
            __leave;
        }
#endif

        /* if we are here, then something went really wrong */
        ok = FALSE;

    }
    __finally {

        /* propagate the last error */
        SetLastError ( ok ? ERROR_SUCCESS : last_error );

    }

    return ok;

}

/* return TRUE on a successful copy; otherwise, FALSE. */
static BOOL
CondorTryRemoveFile (
    PCWSTR w_path ) {

    DWORD       last_error          = ERROR_SUCCESS;
    BOOL        directory_removed   = FALSE,
                handle_closed       = FALSE,
                ok                  = TRUE; /* assume success for early 
                                          exists on a successful copy */

    __try {

        /* first, we try to remove the file as the user */
        directory_removed = CondorRemoveFileAs ( 
            PRIV_USER,
            "user",
            w_path );

        last_error = GetLastError ();

        if ( directory_removed ) {
            __leave;
        }

        /* next, for good measure we give it a try as Condor which 
        should, presumably, be able to delete anything. */
        directory_removed = CondorRemoveFileAs ( 
            PRIV_CONDOR,
            "SYSTEM",
            w_path );

        last_error = GetLastError ();

        if ( directory_removed ) {
            __leave;
        }

        // Don't spawn threads in other processes on the system to
        // close file handles.
        // TODO Finding and logging the processes that have our
        //   directory open and locked would be worthwhile.
#if 0
        /*******************************************************
        NOTE: For future implementations which allow for any
        user to load their profile, what follows bellow is 
        known as a BAD IDEA. If the user in question is logged
        in, running a personal Condor (or the machine is a
        submit and execute node, and the job is matched to it)
        then we may be closing off handles in that user's 
        interactive session. Not exactly what they would expect.
        *******************************************************/

        /* as a last ditch attempt, lets close off whom ever has an 
        open (locked) handle to our directory, as it should only 
        be us who would ever have a lock on it in the first place */
        handle_closed = RemoteFindAndCloseW ( 
            w_path, 
            L"Directory" );

        last_error = GetLastError ();

        if ( !handle_closed ) {
            ok = FALSE;
            __leave;
        }

        /* once again, we try to copy the file as the user */
        directory_removed = CondorRemoveFileAs ( 
            PRIV_USER,
            "user",
            w_path );

        last_error = GetLastError ();       

        if ( directory_removed ) {
            __leave;
        }
#endif

        /* if we are here, then something went really wrong */
        ok = FALSE;

    }
    __finally {

        /* propagate the last error */
        SetLastError ( ok ? ERROR_SUCCESS : last_error );

    }

    return ok;

}

/* recursively delete a directory */
extern BOOL 
CondorRemoveDirectoryImpl ( 
    PCWSTR w_directory ) {

    PSTR                error_message        = NULL;
    PWSTR               w_directory_searcher = NULL,
                        w_current            = NULL;
    DWORD               length               = 0,
                        attributes           = 0,
                        last_error           = 0;
    PSTR                file_name            = NULL;
    WIN32_FIND_DATAW    find_data;
    HANDLE              current              = INVALID_HANDLE_VALUE;
    BOOL                entry_removed        = FALSE,
                        ok                   = FALSE;

    _try {

        /* we want find things *in* w_source no just find w_source, 
        so we append the *.* pattern (the function name may be a 
        little missleading, is simply concatonates the two strings
        separated by a back-slash.) */
        w_directory_searcher = CondorAppendDirectoryToPath (
            w_directory, 
            all_pattern );
        ASSERT ( w_directory_searcher );
            
        current = CondorFindFirstFile ( 
            w_directory_searcher, 
            &find_data );

        if ( INVALID_HANDLE_VALUE == current ) {

            last_error = GetLastError ();
            
            if ( ERROR_NO_MORE_FILES == last_error ) {
            
                /* we're done in this directory */
                ok = TRUE;
                __leave;
            
            } 
            
            dprintf ( 
                D_FULLDEBUG, 
                "CondorRemoveDirectoryImpl: Unable to search in "
                "'%S'. (last-error = %u)\n", 
                w_directory, 
                last_error );

            dprintf ( 
                D_FULLDEBUG, 
                "Trace:" );
            
            __leave;

        }    
        
        do {
            
            if ( !( MATCH == wcscmp ( find_data.cFileName, dot ) 
                 || MATCH == wcscmp ( find_data.cFileName, ddot ) ) ) {

                w_current = CondorAppendDirectoryToPath ( 
                    w_directory, 
                    find_data.cFileName );
                ASSERT ( w_current );

                /* remove any attributes that may inhibit a deletion */
                attributes = GetFileAttributesW ( 
                    w_current );
                
                attributes &= ~( 
                    FILE_ATTRIBUTE_HIDDEN | 
                    FILE_ATTRIBUTE_READONLY | 
                    FILE_ATTRIBUTE_SYSTEM );
                
                SetFileAttributesW ( 
                    w_current, 
                    attributes );

                /* reset to detect successful creation */
                entry_removed = FALSE;

                if ( FILE_ATTRIBUTE_DIRECTORY 
                   & find_data.dwFileAttributes ) {                    

                    /* recurse to the next level */
                    entry_removed = CondorRemoveDirectoryImpl ( 
                        w_current );
                    
                    if ( !entry_removed ) {

                        last_error = GetLastError ();

                        dprintf ( 
                            D_FULLDEBUG, 
                            "CondorRemoveDirectoryImpl: "
                            "Unable to delete the contents of "
                            "directory '%S'. (last-error = %u)\n", 
                            find_data.cFileName, 
                            last_error );

                        __leave;

                    }

                    /* now that we are done with deleting the contents
                    of the given directory, delete it too */
                    entry_removed = CondorTryRemoveDirectory ( 
                        w_current ); 
                    
                } else {

                    /* it's just a file, so delete it */
                    entry_removed = CondorTryRemoveFile ( 
                        w_current ); 
                
                } 

                last_error = GetLastError ();

                dprintf ( 
                    D_FULLDEBUG, 
                    "CondorRemoveDirectoryImpl: "
                    "Removing '%S' %s. (last-error = %u)\n", 
                    find_data.cFileName, 
                    entry_removed ? "succeeded" : "failed", 
                    last_error );

                if ( !entry_removed ) {
                    __leave;
                }
                
                delete [] w_current;    
                w_current = NULL;
            
            } 

        } while ( CondorFindNextFile ( current, &find_data ) );

        /* cool, we made it! */
        ok = TRUE;

    }
    __finally {

        /* propagate the last error */
        SetLastError ( ok ? ERROR_SUCCESS : last_error );

        if ( INVALID_HANDLE_VALUE != current ) {
            FindClose ( current );
        }
        
        if ( w_directory_searcher ) {
            delete [] w_directory_searcher;
        }
        if ( w_current ) {
            delete [] w_current;
        }

    }

    return ok;

}

/* returns TRUE if the directory has been removed; otherwise, FALSE */
BOOL 
CondorRemoveDirectory ( PCSTR directory ) {

    priv_state  priv         = PRIV_UNKNOWN;
    PWSTR       w_directory  = NULL;
    DWORD       last_error   = ERROR_SUCCESS,
                i;
    LPCTSTR     privelages[] = { SE_BACKUP_NAME, SE_RESTORE_NAME };
    BOOL        opened       = FALSE,
                added        = FALSE,
                removed      = FALSE,
                ok           = FALSE;
    
    __try {

        /* do the following as the user */
        priv = set_user_priv ();
        
        dprintf ( 
            D_FULLDEBUG, 
            "CondorRemoveDirectory: Directory: '%s'.\n", 
            directory );
        
        /* give the user a few extra privelages so it can handle
        reparse points, et. al. */
        for ( i = 0; i < condor_countof ( privelages ); ++i ) {

            added = ModifyPrivilege ( 
                privelages[i], 
                TRUE );
            
            if ( !added ) {
                
                last_error = GetLastError ();
                
                dprintf ( 
                    D_FULLDEBUG, 
                    "CondorRemoveDirectory: Failed to add "
                    "'%s' privilege.\n",
                    privelages[i] ); 
                
                __leave;
            
            }
                
        }

        /* create wide versions of the strings */
        w_directory = ProduceWFromA ( directory );

        dprintf ( 
            D_FULLDEBUG, 
            "CondorRemoveDirectory: Allocation of directory %s.",
            NULL != w_directory ? "succeeded" : "failed" );

        if ( !w_directory ) {           
            __leave;
        }

        dprintf ( 
            D_FULLDEBUG, 
            "CondorRemoveDirectory: Removing: %s\n", 
            directory );                    
        
        /* do the actual work */
        removed = CondorRemoveDirectoryImpl ( 
            w_directory );

        last_error = GetLastError ();
        
        dprintf ( 
            D_FULLDEBUG, 
            "CondorRemoveDirectory: Deleting the "
            "contents of '%s' %s. (last-error = %u)\n", 
            directory, 
            removed ? "succeeded" : "failed",
            last_error );

        if ( !removed ) {
            __leave;
        }

        /* finally, now that we are done with deleting the contents of 
        the given directory, delete it too */
        removed = CondorTryRemoveDirectory ( 
            w_directory ); 

        last_error = GetLastError ();

        dprintf ( 
            D_FULLDEBUG, 
            "CondorRemoveDirectory: Deleting the "
            "directory '%s' %s. (last-error = %u)\n",
            directory, 
            removed ? ERROR_SUCCESS : last_error );

        if ( !removed ) {
            __leave;
        }
        
        /* we made it, it's all ok */
        ok = TRUE;

    }
    __finally {

        /* revoke the privelages we gave the user */
        for ( i = 0; i < condor_countof ( privelages ); ++i ) {
            
            removed = ModifyPrivilege ( 
                privelages[i], 
                FALSE );
            
            if ( !removed ) {
                
                dprintf ( 
                    D_FULLDEBUG, 
                    "CondorRemoveDirectory: Failed to remove "
                    "'%s' privilege. (last-error = %d)\n",
                    privelages[i],
                    GetLastError () );
                
            }
            
        }
        
        /* propagate the last error */
        SetLastError ( ok ? ERROR_SUCCESS : last_error );

        if ( w_directory ) {
            delete [] w_directory;
        } 

        /* return to previous privilege level */
        set_priv ( priv );

    }
    
    return ok;

}


/* returns TRUE if all the directories allong the given path were 
created (or already existed); ortherwise, FALSE */
BOOL 
CreateSubDirectory ( PCSTR path, LPSECURITY_ATTRIBUTES sa ) {

    BOOL    ok          = FALSE;
    DWORD   last_error  = ERROR_SUCCESS;
    CHAR    *current    = NULL,
            directory[MAX_PATH];
            
    
    __try {

        if ( !path || !(*path) ) {

            dprintf ( 
                D_FULLDEBUG,
                "CreateSubDirectory: a NULL path was passed\n" );

            __leave;

        }

        /* if we can create the top level path, then we are done */        
        if ( CreateDirectory ( path, sa ) ) { 
            ok = TRUE;
            __leave;
        }

        last_error = GetLastError ();

        /* if it already existed, then, again, then we are done */
        if ( ERROR_ALREADY_EXISTS == last_error ) {
            ok = TRUE;
            __leave;     
        }

        /* if we've made it this far, it means we need to create
        each directory along the path.  To this effect we copy the
        original path to a temporary buffer and use that to create
        all the path names we need. */
        strncpy ( directory, path, MAX_PATH );

        /* skip leading drive or relative path */
        current = directory;
        if ( ':' == directory[1] ) {

            /* skip the drive letter */
            current += 3;

        } else if ( '\\' == directory[0] ) {

            /* skip the leading \ */
            current++;

        }

        /* start iterating through the subdirectories */
        while ( *current++ ) {

            while ( *current && '\\' != *current ) {
                current++;
            }
            
            if ( '\\' == *current ) {

                /* NULL terminate, to create a "new" directory */
                *current = '\0';
                
                /* attempt to create the next parent directory 
                in the path */
                if ( !CreateDirectory ( directory, NULL ) ) {
                    
                    last_error = GetLastError ();

                    if ( ERROR_ALREADY_EXISTS != last_error ) {
                        
                        dprintf (
                            D_FULLDEBUG,
                            "CreateSubDirectory: failed. "
                            "(last-error = %d)\n",
                            last_error );

                        __leave;

                    }
                }
                
                /* remove the NULL and continue to the next lower 
                level directory name (if there are any left) */
                *current++ = '\\';

            }

        }

        /* create the top level path, if we can, then we 
        are done */        
        if ( !CreateDirectory ( path, sa ) ) { 
            
            last_error = GetLastError ();

            /* if it already existed, then consider this a success */
            if ( ERROR_ALREADY_EXISTS == last_error ) {
                ok = TRUE;
                __leave;
            }

        }

        /* if we made it to the end, then we're all good */
        ok = TRUE;

    }
    __finally {

        /* propagate the error message */
        SetLastError ( ok ? ERROR_SUCCESS : last_error );

    }

    return ok;

}

BOOL
CreateUserDirectory ( HANDLE user_token, PCSTR directory ) {

    SECURITY_DESCRIPTOR         security_descriptor;
    SECURITY_ATTRIBUTES         security_attributes;
    PACL                        pacl            = NULL;
    SID_IDENTIFIER_AUTHORITY    nt_authority    = SECURITY_NT_AUTHORITY;
    PSID                        user_sid        = NULL,
                                system_sid      = NULL,
                                admin_sid       = NULL;
    DWORD                       last_error      = ERROR_SUCCESS,
                                size            = 0,
                                i               = 0;
    ACE_HEADER                  *ace_header     = NULL;
    BOOL                        got_sid         = FALSE,
                                initialized     = FALSE,
                                added           = FALSE,
                                got_ace         = FALSE,
                                sd_initialized  = FALSE,
                                sd_set          = FALSE,
                                created         = FALSE,
                                ok              = FALSE;
    __try {

        got_sid = LoadUserSid ( 
            user_token,
            &user_sid );

        if ( !got_sid ) {

            last_error = GetLastError ();

            dprintf (
                D_FULLDEBUG,
                "CreateUserDirectory: Failed to get user SID. "
                "(last-error = %d)\n",
                last_error  );

            __leave;

        }

        got_sid = AllocateAndInitializeSid (
            &nt_authority,
            1, 
            SECURITY_LOCAL_SYSTEM_RID,
            0, 0, 0, 0, 0, 0, 0,
            &system_sid );
        
        if ( !got_sid ) {
            
            last_error = GetLastError ();            
            
            dprintf (
                D_FULLDEBUG,
                "CreateUserDirectory: Failed to get system SID. "
                "(last-error = %d)\n",
                last_error  );
            
            __leave;
            
        }

        got_sid = AllocateAndInitializeSid (
            &nt_authority,
            2, 
            SECURITY_BUILTIN_DOMAIN_RID,
            DOMAIN_ALIAS_RID_ADMINS,
            0, 0, 0, 0, 0, 0,
            &admin_sid );
        
        if ( !got_sid ) {
            
            last_error = GetLastError ();            
            
            dprintf (
                D_FULLDEBUG,
                "CreateUserDirectory: Failed to get admin SID. "
                "(last-error = %d)\n",
                last_error  );
            
            __leave;
            
        }

        size = sizeof ( ACL )
             + ( 2 * GetLengthSid ( user_sid ) ) 
             + ( 2 * GetLengthSid ( system_sid ) ) 
             + ( 2 * GetLengthSid ( admin_sid ) )                  
             + ( 6 * ( sizeof ( ACCESS_ALLOWED_ACE ) 
                       - sizeof ( DWORD ) ) );

        pacl = (PACL) malloc(size);
        ASSERT( pacl );

        initialized = InitializeAcl (
            pacl, 
            size, 
            ACL_REVISION );

        if ( !initialized ) {

            last_error = GetLastError ();            
            
            dprintf (
                D_FULLDEBUG,
                "CreateUserDirectory: Failed to Initialize ACL. "
                "(last-error = %d)\n",
                last_error );
            
            __leave;

        }

        /* add non-inheritable ACEs */
        PSID sids[] = { user_sid, system_sid, admin_sid };
        UINT count = condor_countof ( sids );
        for ( i = 0; i < count; ++i ) {
            
            added = AddAccessAllowedAce (
                pacl,
                ACL_REVISION,
                FILE_ALL_ACCESS,
                sids[i] );
            
            if ( !added ) {
                
                last_error = GetLastError ();            
                
                dprintf (
                    D_FULLDEBUG,
                    "CreateUserDirectory: Failed to initialize "
                    "non-inheritable access-control list. "
                    "(last-error = %d)\n",
                    last_error );
                
                __leave;
                
            }
            
        }

        /* add the inheritable ACEs */
        for ( i = 0; i < count; ++i ) {
            
            added = AddAccessAllowedAceEx (
                pacl,
                ACL_REVISION,
                OBJECT_INHERIT_ACE 
                | CONTAINER_INHERIT_ACE 
                | INHERIT_ONLY_ACE,
                FILE_ALL_ACCESS,
                sids[i] );
            
            if ( !added ) {
                
                last_error = GetLastError ();            
                
                dprintf (
                    D_FULLDEBUG,
                    "CreateUserDirectory: Failed to initialize "
                    "inheritable access-control list. "
                    "(last-error = %d)\n",
                    last_error );
                
                __leave;
                
            }
            
        }

        /* construct the security descriptor */
        sd_initialized = InitializeSecurityDescriptor (
            &security_descriptor,
            SECURITY_DESCRIPTOR_REVISION );

        if ( !sd_initialized ) {
            
            last_error = GetLastError ();            
            
            dprintf (
                D_FULLDEBUG,
                "CreateUserDirectory: Failed to initialize security "
                "descriptor. (last-error = %d)\n",
                last_error );
            
            __leave;
            
        }
            
        sd_set = SetSecurityDescriptorDacl (
            &security_descriptor, 
            TRUE, 
            pacl, 
            FALSE );

        if ( !sd_set ) {
            
            last_error = GetLastError ();            
            
            dprintf (
                D_FULLDEBUG,
                "CreateUserDirectory: Failed to initialize security "
                "descriptor DACL. (last-error = %d)\n",
                last_error );
            
            __leave;
            
        }  
        
        /* add the security descriptor to the security attributes */
        security_attributes.nLength              = sizeof ( SECURITY_ATTRIBUTES );
        security_attributes.lpSecurityDescriptor = &security_descriptor;
        security_attributes.bInheritHandle       = FALSE;

        /* finally, make the user's directory */
        created = CreateSubDirectory ( 
            directory,
            &security_attributes );

        if ( !created ) {
            
            last_error = GetLastError ();            
            
            dprintf (
                D_FULLDEBUG,
                "CreateUserDirectory: Failed to create user's "
                "directory. (last-error = %d)\n",
                last_error );
            
            __leave;
            
        }            

        /* if we're here, we're golden */
        ok = TRUE;

    }
    __finally {

        if ( user_sid ) {
            UnloadUserSid ( user_sid );
        }
        if ( system_sid ) {
            FreeSid ( system_sid );
        }
        if ( admin_sid ) {
            FreeSid ( admin_sid );
        }

        if ( pacl ) {
            free (pacl);
        }

    }

    return ok;

}
