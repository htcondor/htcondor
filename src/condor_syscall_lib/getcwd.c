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
extern int REMOTE_CONDOR_getwd(char **path_name);


/* Definitions of the getwd() and getcwd() that the user job will
   probably call */

char *
getwd( char *path )
{
	if( LocalSysCalls() ) {
		return GETCWD( path, _POSIX_PATH_MAX );
	} else {
		char *p = NULL;
		REMOTE_CONDOR_getwd( &p );
			/* We do not know how big the supplied buffer is.  If
			   we assume it is only _POSIX_PATH_MAX, this could cause
			   failure when, in fact, more space is needed and more
			   space has been provided.  Therefore, the correct
			   behavior is to assume the caller has provided enough space.
			   Here goes nothing ...
			*/
		strcpy( path, p );
		free( p );
		return path;
	}
}

char *
getcwd( char *path, size_t size )
{
	char *tmpbuf = NULL;

	if( LocalSysCalls() ) {
		return GETCWD( path, size );
	} else {
		if ( size == 0 ) {
#ifdef EINVAL
			errno = EINVAL;
#endif
			return NULL;
		}

		REMOTE_CONDOR_getwd( &tmpbuf );
		if ( tmpbuf == NULL || tmpbuf[0] == '\0' ) {
			// our remote syscall failed somehow
			free( tmpbuf );
			return NULL;
		}
		if ( (strlen(tmpbuf) + 1) > size ) {
			// the path will not fit into the user's buffer
#ifdef ERANGE
			errno = ERANGE;
#endif
			free( tmpbuf );
			return NULL;
		}

    	if ( path == NULL ) {
			path = tmpbuf;
			return path;
		}

		// finally, copy from our tmpbuf into users buffer
		strcpy(path,tmpbuf);
		free( tmpbuf );

		return path;
	}
}


/* We want to remap these underscore versions so we don't miss them. */
REMAP_ONE( getwd, _getwd, char *, char * )
REMAP_ONE( getwd, __getwd, char *, char * )
REMAP_TWO( getcwd, _getcwd, char *, char *, size_t )
REMAP_TWO( getcwd, __getcwd, char *, char *, size_t )

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
		  
