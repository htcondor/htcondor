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

#include "setenv.h"

extern DLL_IMPORT_MAGIC char **environ;

int SetEnv( const char *key, char *value)
{
	assert(key);
	assert(value);
#ifdef WIN32
	if ( !SetEnvironmentVariable(key, value) ) {
		dprintf(D_ALWAYS,
				"SetEnvironmentVariable failed, errno=%d\n",
				GetLastError());
		return FALSE;
	}
#else
	// XXX: We should actually put all of this in a hash table, so that 
	// we don't leak memory.  This is just quick and dirty to get something
	// working.
	char *buf;
	buf = new char[strlen(key) + strlen(value) + 2];
	sprintf(buf, "%s=%s", key, value);
	if( putenv(buf) != 0 )
	{
		dprintf(D_ALWAYS, "putenv failed: %s (errno=%d)\n",
				strerror(errno), errno);
		return FALSE;
	}
#endif
	return TRUE;
}

int SetEnv( char *env_var ) 
{
		// this function used if you've already got a name=value type
		// of string, and want to put it into the environment.  env_var
		// must therefore contain an '='.
	if ( !env_var ) {
		dprintf (D_ALWAYS, "DaemonCore::SetEnv, env_var = NULL!\n" );
		return FALSE;
	}

		// Assume this type of string passes thru with no problem...
	if ( env_var[0] == 0 ) {
		return TRUE;
	}

	char *equalpos = NULL;

	if ( ! (equalpos = strchr( env_var, '=' )) ) {
		dprintf (D_ALWAYS, "SetEnv, env_var has no '='\n" );
		dprintf (D_ALWAYS, "env_var = \"%s\"\n", env_var );
		return FALSE; 
	}

#ifdef WIN32  // will this ever be used?  Hmmm....
		// hack up string and pass to other SetEnv version.
	int namelen = (int) equalpos - (int) env_var;
	int valuelen = strlen(env_var) - namelen - 1;

	char *name = new char[namelen+1];
	char *value = new char[valuelen+1];
	strncpy ( name, env_var, namelen );
	strncpy ( value, equalpos+1, valuelen );

	int retval = SetEnv ( name, value );

	delete [] name;
	delete [] value;
	return retval;
#else
		// XXX again, this is a memory-leaking quick hack; see above.
	char *buf = new char[strlen(env_var) +1];
	strcpy ( buf, env_var );
	if( putenv(buf) != 0 ) {
		dprintf(D_ALWAYS, "putenv failed: %s (errno=%d)\n",
				strerror(errno), errno);
		return FALSE;
	}
	return TRUE;
#endif
}

int UnsetEnv( const char *env_var ) 
{
	assert( env_var );

#ifdef WIN32
	if ( !SetEnvironmentVariable(env_var, NULL) ) {
		dprintf(D_ALWAYS,
				"SetEnvironmentVariable failed, errno=%d\n",
				GetLastError());
		return FALSE;
	}
#endif

		// XXX Again, this is a memory-leaking quick hack. If we tracked
		// variables we previously inserted, we could free them here.
	for ( int i = 0 ; environ[i] != NULL; i++ ) {
		if ( strncmp( environ[i], env_var, strlen(env_var) ) == 0 ) {
            for ( ; environ[i] != NULL; i++ ) {
                environ[i] = environ[i+1];
			}
		    break;
		}
	}

	return TRUE;
}
