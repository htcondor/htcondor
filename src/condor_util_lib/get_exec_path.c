/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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
#include "condor_debug.h" 

/*

  This file defines getExecPath(), which tries to return the full path
  to the file that the current process is executing.  On platforms
  where this information cannot be reliably determined, it returns
  NULL.  On platforms where it is possible to collect this
  information, the full path is stored in a string allocated with
  malloc() that must be free()'ed when the caller is done with it.
*/


/*
  Each platform that supports this functionality uses a completely
  different method for finding it out, so they are implemented in
  seperate helper functions...
*/

#if defined( LINUX )
char*
linux_getExecPath()
{
	int rval;
	char* full_path;
	char path_buf[_POSIX_PATH_MAX];
	rval = readlink( "/proc/self/exe", path_buf, _POSIX_PATH_MAX );
	if( rval < 0 ) {
		dprintf( D_ALWAYS,"getExecPath: "
				 "readlink(\"/proc/self/exe\") failed: errno %d (%s)\n",
				 errno, strerror(errno) );
		return NULL;
	}
	if( rval == _POSIX_PATH_MAX ) {
		dprintf( D_ALWAYS,"getExecPath: "
				 "unable to find full path from /proc/self/exe\n" );
		return NULL;
	}
		/* Oddly, readlink doesn't terminate the string for you, so
		   we've got to handle that ourselves... */
	path_buf[rval] = '\0';
	full_path = strdup( path_buf );
	return full_path;
}
#endif /* defined(LINUX) */


#if defined( Solaris )
char*
solaris_getExecPath()
{
	const char* name;
	char* cwd = NULL;
	char* full_path = NULL;
	int size;

	name = getexecname();
	if( name[0] == '/' ) {
		return strdup(name);
	}
		/*
		  if we're still here, we didn't get the full path, and the
		  man pages say we can prepend the output of getcwd() to find
		  the real value...
		*/ 
	cwd = getcwd( NULL, _POSIX_PATH_MAX );
	size = strlen(cwd) + strlen(name) + 2;
	full_path = malloc(size);
	snprintf( full_path, size, "%s/%s", cwd, name );
	free( cwd );
	cwd = NULL;
	return full_path;
}
#endif /* defined(Solaris) */


#if defined( WIN32 )
char*
win32_getExecPath()
{
		/* There's a way to do this.  For now, just return NULL */
	return NULL;
}
#endif /* defined(WIN32) */


/*
  Now, the public method that just invokes the right platform-specific
  helper (if there is one)
*/
char*
getExecPath( void )
{
#if defined( LINUX )
	return linux_getExecPath();
#elif defined( Solaris )
	return solaris_getExecPath();
#elif defined( WIN32 )
	return win32_getExecPath();
#else
	return NULL;
#endif
}

