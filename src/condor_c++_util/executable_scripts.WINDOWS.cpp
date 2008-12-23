/***************************************************************
*
* Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
* University of Wisconsin-Madison, WI.
* 
* Licensed under the Apache License, Version 2.0 (the "License"); you
* may not use this script except in compliance with the License.  You may
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

/***************************************************************
* Headers
***************************************************************/

#include "condor_common.h"      /* pre-compiled header */
#include "condor_config.h"      /* for param */
#include "executable_scripts.h" /* our header file */

/***************************************************************
* Macros
***************************************************************/

#define count_of(x) (sizeof(x)/sizeof(x[0]))

/***************************************************************
* Functions
***************************************************************/

/** Finds the executable and parameter list associated with the
    provided extension. */
BOOL 
GetExecutableAndArgumentTemplateByExtention (
	LPCSTR extension, 
	LPSTR  executable,
	LPSTR  arguments ) {

	DWORD			last_error			= ERROR_SUCCESS;
	LONG			result,
					length;
    CHAR			value[MAX_PATH+1],
					subkey[MAX_PATH+1],
                    verbkey[MAX_PATH+1],
                    uppercase_extension[MAX_PATH],
                    default_verb[]      = "Open";
    LPSTR           verb                = NULL;
	HANDLE			found				= INVALID_HANDLE_VALUE;
	WIN32_FIND_DATA find_data;
	LPSTR			tmp, 
					exe_start			= NULL,
					arg_start			= NULL;					
	BOOL			quoted				= FALSE,
					expanded			= FALSE,
					ok					= FALSE;

	__try {

        /* make sure the output buffers not contain any misleading 
        information, if we are to fail */
        executable[0] = '\0';
        if ( arguments ) {
            arguments[0] = '\0';
        }

		/* using the extension, find the script type (or program ID) */
		length = MAX_PATH;
		result = RegQueryValue (
			HKEY_CLASSES_ROOT,
			extension,
			(LPSTR) value, 
			&length );

		if ( ERROR_SUCCESS != result ) {

			last_error = result;

			dprintf (
                D_ALWAYS,
				"GetExecutableAndArgumentsByExtention: failed to find "
				"extension *%s in the registry (last-error = %d).\n",
				extension, 
				last_error );

			__leave;

		}

        /* Because some interpreters have /n/ verb handlers , there is
        no intelligent way we can choose which one to use.  The verb
        'Open' is the most common, but we let the user define one if
        they want. */
        strcpy ( 
            uppercase_extension, 
            extension + 1 );

        sprintf ( 
            verbkey, 
            "OPEN_VERB_FOR_%s_FILES", 
            strupr ( uppercase_extension ) );

        verb = param ( verbkey );

        sprintf ( 
            subkey, 
            "%s\\Shell\\%s\\Command", 
            value, 
            verb ? verb : default_verb );

        if ( verb ) {
            free ( verb );
        }

        length = MAX_PATH;
	    result = RegQueryValue (
		    HKEY_CLASSES_ROOT,
		    subkey,
		    (LPSTR) value, 
		    &length );

        if ( ERROR_SUCCESS != result ) {

			last_error = result;

			dprintf (
                D_ALWAYS,
				"GetExecutableAndArgumentsByExtention: failed to "
				"find a suitable handler for files with extension "
				"*%s (last-error = %d).\n",
				extension, last_error );

			__leave;

		}

        /* we now have the command-line.  First, let's trim all the
		leading whitespace. */
		tmp = (LPSTR) value;

		/* strip leading spaces */
        while ( *tmp && iswspace ( *tmp ) ) {
            tmp++;
        }

        /* we are now at the start of the executable's path */
		exe_start = tmp;

        /* if the executable is surrounded by quotes, then we want to 
		ignore all whitespace until we see another quote--after that
		we will stop. */
		quoted = ( '"' == *exe_start );

		/* step through the executable's path until we reach either
		the closing quote, a whitespace character or NULL. */
		if ( quoted ) {
			 /* skip first quote because FindFirstFile() does not
			 work if there are quotes surrounding a script name. */
			exe_start = ++tmp;
			while ( *tmp && ( '"' != *tmp ) ) {
				tmp++;
			}
        } else {
            while ( *tmp && !iswspace ( *tmp ) ) {
                tmp++;
            }
        }

        /* null terminate at the closing quote, the first 
		whitespace character or at the end of the buffer. */
		*tmp = '\0';

        /* validate the executable's existence */
		found = FindFirstFile ( 
			exe_start ,
			&find_data );

		if ( INVALID_HANDLE_VALUE == found ) {

			last_error = GetLastError ();

			dprintf (
                D_ALWAYS,
				"GetExecutableAndArgumentsByExtention: failed to "
                "locate the executable, %s, to handle files "
                "with extension *%s (last-error = %d).\n",
				exe_start, extension, last_error );

			__leave;

		}

		/* clean-up after the script search */
		FindClose ( found );
		
		/* if we have not already consumed the entire executable, 
        then copy the rest as the arguments to the executable ( skip
		the null termination we added above first ) */
		if ( arguments && tmp - exe_start < length - 1 ) {
			
			/* skip the null character */
			tmp++;
			
			/* strip leading spaces */
            while ( *tmp && iswspace ( *tmp ) ) {
                tmp++;
            }

			/* we are now at the start of the executable's arguments */
			arg_start = tmp;
			
			/* make a copy of the arguments */
            strcpy ( arguments, arg_start );

		}
		
		/* finally, copy the executable path */
		strcpy ( executable, exe_start );

		/* if we made it here, then we're all going to be ok */
		ok = TRUE;

	}
	__finally {

		/* propagate the last error to our caller */
		SetLastError ( last_error );

	};
	
	return ok;

}
