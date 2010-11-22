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


	/* Before including condor_common.h, we must set definitions so that
	 * open() and fopen() calls are not redefined to be an error.  This
	 * should be the only file in Condor that is allowed to call open() or
	 * fopen() directly.  
	 * Note: since we are making pre-proccesor calls before including 
	 * condor_common.h, on Win32 we must tell the build system to avoid
	 * using pre-compiled headers for this file.
	 */
#define _CONDOR_ALLOW_OPEN 1
#define _CONDOR_ALLOW_FOPEN 1
#define _FILE_OFFSET_BITS 64
#include "condor_common.h"
#include "condor_open.h"
#include "condor_debug.h"



/***********
 Replacement functions for open.  These functions differ in the following ways:
 1) file creation is always done safely.  basically, the idea is to always 
    use the O_EXCL flag when creating a file to prevent many types of 
	common security attacks.
 2) passing O_CREAT and O_EXCL to the safe_create* functions is optional
    (should not normally be passed)
 3) passing O_CREAT and O_EXCL to the safe_open_no_create is an error.
 4) file creation permissions have a default of 0644.  umask still applies.
    (should not pass unless something else is needed)
 5) all other flags are passed to open
***********/

int safe_open_wrapper(const char *fn, int flags, mode_t mode)
{

	if ( flags & O_CREAT ) {

			// O_CREAT specified, pick function based on other flags
			
		if ( flags & O_EXCL ) {
			return safe_create_fail_if_exists(fn, flags, mode);
		}

#if 0    // FOR V6.8 COMMENT OUT THIS BLOCK - PUT IT BACK FOR V6.9
		if ( flags & O_TRUNC ) {
			// Strictly speaking semantic-wise, O_TRUNC should
			// not replace the file -- it should just clear out
			// the existing file and keep the inode the same.
			// However, in this default case, 
			// we prefer to replace the file if we can (instead
			// of just truncating the file), since this way
			// we create the file ourselves and therefore know
			// it will be created with O_EXCL and with a reasonable
			// default mode.
			return safe_create_replace_if_exists(fn, flags, mode);
		}
#endif

		return safe_create_keep_if_exists(fn, flags, mode);

	} else {
		
			// O_CREAT not specified
		return safe_open_no_create(fn,flags);
	
	}

}

// create file, error if exists, don't follow sym link
int safe_create_fail_if_exists(const char *fn, int flags, mode_t mode)
{
    int f = open(fn, flags | O_CREAT | O_EXCL, mode);

    return f;
}


// create file, replace file if exists
int safe_create_replace_if_exists(const char *fn, int flags, mode_t mode)
{
    int r = unlink(fn);

    if (r != 0)  {
        if (errno != ENOENT)  {
            // unexpected unlink error, just continue and try open
        }
    }

    int f = open(fn, flags | O_CREAT | O_EXCL, mode);

    return f;
}


// create file if it doesn't exist, keep inode if it does
int safe_create_keep_if_exists(const char *fn, int flags, mode_t mode)
{
	int f = -1;
	int saved_errno = errno;
	int num_tries = 0;

		/* check for invalid argument values */
	if (!fn)  {
		errno = EINVAL;
		return -1;
	}

		/* Remove O_CREATE and O_EXCL from the flags, the safe_open_no_create()
		 * requires them to not be included and safe_creat_fail_if_exists() adds
		 * them implicitly.
		 */
	flags &= ~O_CREAT & ~O_EXCL;

		/* Loop alternating between creating the file (and failing if it exists)
		 * and opening an existing file.  Return an error if any error occurs other
		 * than an indication that the other function should work.
		 */
	while (f == -1)  {
			/* If this is the second or subsequent attempt, then someone is
			 * manipulating the file system object referred to by fn.  Call the user
			 * defined callback if registered, and fail if it returns a non-zero value.
			 */
		if (++num_tries > 1)  {
				/* the default error is EAGAIN */
			errno = EAGAIN;

				/* check if we tried too many times */
			if (num_tries > SAFE_OPEN_RETRY_MAX)  {
					/* let the user decide what to do */
				return -1;
			}
		}

		f = safe_create_fail_if_exists(fn, flags, mode);

			/* check for error */
		if (f == -1)  {
			if (errno != EEXIST)  {
				return -1;
			}

				/* previous function said the file exists, so this should work */
			f = safe_open_no_create(fn, flags);
			if (f == -1 && errno != ENOENT)  {
				return -1;
			}

				/* At this point, safe_open_no_create either worked in which case
				 * we are done, or it failed saying the file does not exist in which
				 * case we'll take another spin in the loop.
				 */
		}
	}

		/* no error, restore errno incase we had recoverable failures */
	errno = saved_errno;

	return f;
}


// open existing file
int safe_open_no_create(const char *fn, int flags)
{
    // fail if O_CREAT or O_EXCL are included.
    if (flags & (O_CREAT | O_EXCL))  {
		errno = EINVAL;
		return -1;
    }

    int f = open(fn, flags);

    return f;
}


//
// Internal helper function declarations
//
static int stdio_mode_to_open_flag(const char* flags, int* mode, int createFile);
static FILE* safe_fdopen(int fd, const char* flags);


/*********************
 Replacement functions for fopen.  These functions differ in the following ways:
 1) file creation is always done safely and the semantics are determined by
    which of the 4 functions is used.
 2) the default file permission is 0644 instead of 0666.  umask still applies.
 3) an optional permissions argument is allowed
 4) in the case of "a", the file is opened with O_APPEND
*********************/

FILE* safe_fopen_wrapper(const char *fn, const char *flags, mode_t mode)
{
	int r, open_flags;

	if (!fn || !flags) {
		return NULL;
	}

	if ( flags[0] == 'r' ) {
		r = stdio_mode_to_open_flag(flags, &open_flags, 0);
	} else {
		r = stdio_mode_to_open_flag(flags, &open_flags, 1);
		if(strcmp(fn, NULL_FILE) == 0) {
			/* Truncating /dev/null (or NUL:) is nonsensical
			 * and at least one platform (Solaris 8) won't
			 * let you. 
			 */
			open_flags &= ~O_TRUNC;
		}
		open_flags |= O_CREAT;
	}

	if ( r != 0 ) {
		return NULL;
	}

	int f = safe_open_wrapper(fn, open_flags, mode);

	return safe_fdopen(f, flags);

}

// create file, fail if it exists, don't follow symlink
FILE* safe_fcreate_fail_if_exists(const char *fn, const char* flags, mode_t mode)
{
    int open_flags;
    int r = stdio_mode_to_open_flag(flags, &open_flags, 1);

    if (r != 0)  {
		return NULL;
    }

    int f = safe_create_fail_if_exists(fn, open_flags, mode);

    return safe_fdopen(f, flags);
}


// create file, replace file if exists
FILE* safe_fcreate_replace_if_exists(const char *fn, const char* flags, mode_t mode)
{
    int open_flags;
    int r = stdio_mode_to_open_flag(flags, &open_flags, 1);

    if (r != 0)  {
		return NULL;
    }

    int f = safe_create_replace_if_exists(fn, open_flags, mode);

    return safe_fdopen(f, flags);
}


// create file if it doesn't exist, keep inode if it does
FILE* safe_fcreate_keep_if_exists(const char *fn, const char* flags, mode_t mode)
{
    int open_flags;
    int r = stdio_mode_to_open_flag(flags, &open_flags, 1);

    if (r != 0)  {
		return NULL;
    }

    int f = safe_create_keep_if_exists(fn, open_flags, mode);

    return safe_fdopen(f, flags);
}


// open existing file
FILE* safe_fopen_no_create(const char* fn, const char* flags)
{
    int open_flags;
    int r = stdio_mode_to_open_flag(flags, &open_flags, 0);

    if (r != 0)  {
		return NULL;
    }

    int f = safe_open_no_create(fn, open_flags);

    return safe_fdopen(f, flags);
}


//
// Helper functions for us in safe_fcreate* and safe_fopen functions
//

// create the flags needed by open from mode string passed to fopen
// fail if the create_file is true and mode is "r" or "rb", or if the
// mode does not start with one of the 15 valid modes:
//   r rb r+ r+b rb+  w wb w+ w+b wb+  a ab a+ a+b ab+
int stdio_mode_to_open_flag(const char* flags, int* mode, int create_file)
{
    int         plus_flag = 0;
    char        mode_char;

    *mode = 0;

	if ( !flags ) {
		errno = EINVAL;
		return -1;
	}

	mode_char = flags[0];

    if (mode_char == 'r' || mode_char == 'w' || mode_char == 'a')  {
		if (flags[1] == 'b')  {
			plus_flag = flags[2] == '+';
		}  else  {
			plus_flag = flags[1] == '+';
		}
    }  else  {
		// invalid flags
		errno = EINVAL;
		return -1;
    }

    // it is an error if "r" is passed to one of the creation routines
    if (create_file && mode_char == 'r')  {
		errno = EINVAL;
		return -1;
    }

    if (plus_flag)  {
		*mode |= O_RDWR;
    }  else if (mode_char == 'r')  {
		*mode |= O_RDONLY;
    }  else   {	// mode_char == 'w' or 'a'
		*mode |= O_WRONLY;
    }

    if (mode_char == 'a')  {
		*mode |= O_APPEND;
    }  else if (mode_char == 'w')  {
		*mode |= O_TRUNC;
    }

    // deal with windows extra flags: btSRTD

#ifdef WIN32
#ifdef _O_BINARY
	if (strchr(flags,'b'))  {
		*mode |= _O_BINARY;
	}
#endif

#ifdef _O_TEXT
	if (strchr(flags,'t'))  {
		*mode |= _O_TEXT;
	}
#endif

#ifdef _O_SEQUENTIAL
	if (strchr(flags, 'S'))  {
		*mode |= _O_SEQUENTIAL;
	}
#endif

#ifdef _O_RANDOM
	if (strchr(flags, 'R'))  {
		*mode |= _O_RANDOM;
	}
#endif

#ifdef _O_SHORT_LIVED
	if (strchr(flags, 'T'))  {
		*mode |= _O_SHORT_LIVED;
	}
#endif

#ifdef _O_TEMPORARY
	if (strchr(flags, 'D'))  {
		*mode |= _O_TEMPORARY;
	}
#endif
#endif

    return 0;
}


// turn the file descriptor into a FILE*.  return null if fd == -1.
// close the file descriptor if the fdopen fails.
FILE* safe_fdopen(int fd, const char* flags)
{
		// Don't call fdopen() if the fd is -1. This preserves the errno
		// from the failed open().
    if (fd == -1)  {
		return NULL;
    }

    FILE* f = fdopen(fd, flags);
    if (f == NULL)  {
		(void)close(fd);
    }

    return f;
}


