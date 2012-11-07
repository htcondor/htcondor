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

#include "condor_common.h"				/* pre-compiled header */
#include "condor_config.h"				/* for param */
#include "executable_scripts.WINDOWS.h" /* our header file */

/***************************************************************
* Macros
***************************************************************/

#define count_of(x) (sizeof(x)/sizeof(x[0]))

/***************************************************************
* Forward declarations
***************************************************************/

#if 0
void
ExpandArgumentTemplate (
	LPCSTR	from,
	LPCSTR	file,
	LPCSTR	params,
	LPSTR	to,
	DWORD	length );
#endif

/***************************************************************
* Functions
***************************************************************/

/** The code bellow should probably be changed to use:
	RegOpenUserClassesRoot() when impersonating a user, and
	HKLM\Software\Classes instead of HKEY_CLASSES_ROOT.

	For reasons explained here:
	http://blogs.msdn.com/larryosterman/archive/2004/10/20/245172.aspx
	*/

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
					key[MAX_PATH+1],
                    name[MAX_PATH+1],
                    uppercase[MAX_PATH+1],
                    default_verb[]      = "Open";
    LPSTR           verb                = NULL;
	HANDLE			found				= INVALID_HANDLE_VALUE;
	WIN32_FIND_DATA find_data;
	LPSTR			tmp, 
					start				= NULL;					
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

        /* Because some file types have /n/ verb handlers, there is
        no intelligent way we can choose which one to use.  The verb
        'Open' is the most common, but we allow the user define one
		if they want. */
        strcpy (
            uppercase, 
            extension + 1 );

        sprintf ( 
            name, 
            "OPEN_VERB_FOR_%s_FILES", 
            strupr ( uppercase ) );

        verb = param ( name );

        sprintf ( 
            key, 
            "%s\\Shell\\%s\\Command", 
            value, 
            verb ? verb : default_verb );

        if ( verb ) {
            free ( verb );
        }

        length = MAX_PATH;
	    result = RegQueryValue (
		    HKEY_CLASSES_ROOT,
		    key,
		    (LPSTR) value, 
		    &length );

        if ( ERROR_SUCCESS != result ) {

			last_error = result;

			dprintf (
                D_ALWAYS,
				"GetExecutableAndArgumentsByExtention: failed to "
				"find a suitable handler for files with extension "
				"'*%s'. (last-error = %d)\n",
				extension, 
				last_error );

			__leave;

		}

        /* we now have the command-line.  First, let's trim all the
		leading whitespace. */
		tmp = (LPSTR) value;

		/* strip leading spaces */
        while ( *tmp && isspace ( *tmp ) ) {
            tmp++;
        }

        /* we are now at the start of the executable's path */
		start = tmp;

        /* if the executable is surrounded by quotes, then we want to 
		ignore all whitespace until we see another quote--after that
		we will stop. */
		quoted = ( '"' == *start );

		/* step through the executable's path until we reach either
		the closing quote, a whitespace character or NULL. */
		if ( quoted ) {
			
			/* skip first quote because FindFirstFile() does not
			work if there are quotes surrounding a script name. */
			start = ++tmp;
			while ( *tmp && ( '"' != *tmp ) ) {
				tmp++;
			}			

        } else {

			start = tmp;
            while ( *tmp && !isspace ( *tmp ) ) {
                tmp++;
            }

        }

		/* null terminate at the closing quote, the first 
		whitespace character or at the end of the buffer. */
		*tmp = '\0';

        /* validate the executable's existence */
		found = FindFirstFile ( 
			start,
			&find_data );

		if ( INVALID_HANDLE_VALUE == found ) {

			last_error = GetLastError ();

			dprintf (
                D_ALWAYS,
				"GetExecutableAndArgumentsByExtention: failed to "
                "locate the executable, %s, to handle files "
                "with extension '*%s'. (last-error = %d)\n",
				start, 
				extension, 
				last_error );

			__leave;

		}

		/* clean-up after the script search */
		FindClose ( found );
		
		/* finally, copy the executable path */
		strcpy ( executable, start );

		/* if we have not already consumed the entire buffer, then
		copy the rest as the arguments to the executable */
		start = ++tmp;
		if ( arguments && value - start < length - 1 ) {
			
			/* strip leading space until we are at the start of the
			executable's arguments */
            while ( *start && isspace ( *start ) ) {
                start++;
            }

			/* make a copy of the arguments */
            strcpy ( arguments, start );

		}
		
		/* if we made it here, then we're all going to be ok */
		ok = TRUE;

	}
	__finally {

		/* propagate the last error to our caller */
		SetLastError ( ok ? ERROR_SUCCESS : last_error );

	};
	
	return ok;

}

#if 0
void
ExpandArgumentTemplate (
	LPCSTR			from,
	LPCSTR			file,
	LPCSTR			params,
	LPSTR			to,
	DWORD			length ) {

	int		i;
	LPCSTR	tmp;
	BOOL	quoted = FALSE;

	for (; *from && --length; ++from ) {

		if ( '%' == *from ) {
			
			/* Skip over the '%' character */
			++from;

			/* If we see %0, then our file is actually an executable
			of some form, so we can expand the path in situ */
			if ( '0' == *from ) {
				strcpy ( to, file );
				to += strlen ( to );
				continue;
			}

			/* If the argument is not an index, then it may be an
			unexpanded environment variable, so we'll just copy it
			all to the output, for future expansion.  This could
			also be %*, in which case we also want to skip over it,
			in that we simply copy the remaining command line over 
			it (until the first whitespace is seen). */
			if ( *from < '1' || *from > '9' ) {
				*to++ = *from;
				continue;
			}

			/* If %0 means 'this is an executable' %1 means 'this is
			a file operated on.  In some cases this could be a script,
			or in others it may be an automated documents type, etc. */
			if ( '1' == *from ) {
				tmp = file;
				if ( '"' == *file ) {
					quoted = TRUE;
					tmp++;
				}
			}

			/* The only option that remains is that we are looking at
			a numeric index of the argument to the script or executable.
			Expand this blindly, until we reach a whitespace of the end
			of the input buffer. */
			else {
				i = ( *from - '2' );
				tmp = params;
				for (;;) {					
					while ( iswspace ( *tmp ) ) {
						tmp++;
					}
					if ( !*tmp || !i ) {
						break;
					}
					i--;
					while ( iswspace ( *tmp ) ) {
						tmp++;
					}
					if ( !tmp ) {
						continue;
					}
				}
			}

			/* Finally, perform the action determined by the above
			code.  If we saw a quote, then iterate until we see the
			next one; otherwise, iterate until we see the first bit
			of whitespace. */
			while ( *tmp ) {
				if ( iswspace ( *tmp ) && !quoted ) {
					break;
				}
				*to++ = *tmp++;
				if ( '"' == *tmp && quoted ) {
					break;
				}
			}

			/* Reset quoted flag */
			quoted = FALSE;

		} else {

			/* If we see no sign of a variable, then simply copy the
			command-line string to the output buffer */
			*to++ = *from;

		}
	}

	/* Terminate the expanded command-line */
	*to = '\0';

}
#endif
