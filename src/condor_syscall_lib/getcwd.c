/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#include "condor_common.h"
#include "condor_syscall_mode.h"
#include "syscall_numbers.h"
#include "syscall_macros.h"

/* Prototypes, just so no one is confused */
char* getcwd( char*, size_t );
char* _getcwd( char*, size_t );
char* __getcwd( char*, size_t );
char* GETCWD( char*, size_t );
char* _GETCWD( char*, size_t );
char* __GETCWD( char*, size_t );
char* getwd( char* );
char* _getwd( char* );
char* __getwd( char* );

/* remote system call externs */
extern int REMOTE_CONDOR_getwd(char *path_name);


/* Definitions of the getwd() and getcwd() that the user job will
   probably call */

char *
getwd( char *path )
{
	if( LocalSysCalls() ) {
		return GETCWD( path, _POSIX_PATH_MAX );
	} else {
		REMOTE_CONDOR_getwd( path );
		return path;
	}
}

char *
getcwd( char *path, size_t size )
{
	char tmpbuf[_POSIX_PATH_MAX];

	if( LocalSysCalls() ) {
		return GETCWD( path, size );
	} else {
		if ( size == 0 ) {
#ifdef EINVAL
			errno = EINVAL;
#endif
			return NULL;
		}
		tmpbuf[0] = '\0';
		REMOTE_CONDOR_getwd( tmpbuf );
		if ( tmpbuf[0] == '\0' ) {
			// our remote syscall failed somehow
			return NULL;
		}
		if ( (strlen(tmpbuf) + 1) > size ) {
			// the path will not fit into the user's buffer
#ifdef ERANGE
			errno = ERANGE;
#endif
			return NULL;
		}

    	if ( path == NULL ) {
			if ( (path=(char *)malloc(size)) == NULL ) {
				// Out of memory
				return NULL;
			}
		}

		// finally, copy from our tmpbuf into users buffer
		strcpy(path,tmpbuf);

		return path;
	}
}


/* We want to remap these underscore versions so we don't miss them. */
REMAP_ONE( getwd, _getwd, char *, char * )
REMAP_ONE( getwd, __getwd, char *, char * )
#if !defined(IRIX)
REMAP_TWO( getcwd, _getcwd, char *, char *, size_t )
REMAP_TWO( getcwd, __getcwd, char *, char *, size_t )
#else

/* 
   On IRIX, we can't just do the direct remapping, b/c the getcwd() in
   libc.so really calls _getcwd(), which would cause in an infinite
   loop.  So, we have our own definitions of _getcwd() and __getcwd()
   that work just like getcwd(), but they call _GETCWD() and
   __GETCWD() instead.  These are defined below to use dlsym() on the
   underscore versions of the functions from libc.so.

   However, getcwd() does seem to work at all if we trap __getcwd()
   and do our magic with it.  It's unclear why, unless we have the
   source to the IRIX C library.  So, for now, we'll just ignore this
   and hopefully someday we'll get to the bottom of it.

   -Derek Wright 7/10/98
*/

char *
_getcwd( char *path, size_t size )
{
	if( LocalSysCalls() ) {
		return (char *)_GETCWD( path, size );
	} else {
		// in the remote case, we can just invoke getcwd above
		return getcwd(path,size);
	}
}

#endif /* IRIX */


/*
  If we're not extracting and ToUpper'ing to get GETCWD(), we're
  going to have DL_EXTRACT defined so we know to use dlopen() and
  dlsym() to call the versions from the dynamic C library. 
*/
#if defined( DL_EXTRACT )
#include <dlfcn.h>
char *
GETCWD( char* path, size_t size )
{
	void *handle;
	char * (*fptr)(char *, size_t);
	if ((handle = dlopen("libc.so", RTLD_LAZY)) == NULL) {
		return (char *)-1;
	}
	
	if ((fptr = (char * (*)(char *, size_t))dlsym(handle, "getcwd")) == NULL) {
		return (char *)-1;
	}

	return (*fptr)(path, size);
}	

char *
_GETCWD( char* path, size_t size )
{
	void *handle;
	char * (*fptr)(char *, size_t);
	if ((handle = dlopen("libc.so", RTLD_LAZY)) == NULL) {
		return (char *)-1;
	}
	
	if ((fptr = (char * (*)(char *, size_t))dlsym(handle, "_getcwd")) == NULL) {
		return (char *)-1;
	}

	return (*fptr)(path, size);
}	

#endif /* DL_EXTRACT */
		  
