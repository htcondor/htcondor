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
#include "classad_hashtable.h"

#include "setenv.h"

extern DLL_IMPORT_MAGIC char **environ;

// Under unix, we maintain a hash-table of all environment variables that
// have been inserted using SetEnv() below. If they are overwritten by
// another call to SetEnv() or removed by UnsetEnv(), we delete the
// allocated memory. Windows does its own memory management for environment
// variables, so this hash-table is unnecessary there.

#define HASH_TABLE_SIZE		50

#ifndef WIN32

template class HashTable<HashKey, char *>;
template class HashBucket<HashKey, char *>;

HashTable <HashKey, char *> EnvVars( HASH_TABLE_SIZE, hashFunction );

#endif

int SetEnv( const char *key, const char *value)
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
	char *buf;
	buf = new char[strlen(key) + strlen(value) + 2];
	sprintf(buf, "%s=%s", key, value);
	if( putenv(buf) != 0 )
	{
		dprintf(D_ALWAYS, "putenv failed: %s (errno=%d)\n",
				strerror(errno), errno);
		return FALSE;
	}

	char *hashed_var;
	if ( EnvVars.lookup( HashKey( key ), hashed_var ) == 0 ) {
			// found old one
			// remove old one
		EnvVars.remove( HashKey( key ) );
			// delete old one
		delete [] hashed_var;
			// insert new one
		EnvVars.insert( HashKey( key ), buf );
	} else {
			// no old one
			// add new one
		EnvVars.insert( HashKey( key ), buf );
	}
#endif
	return TRUE;
}

int SetEnv( const char *env_var ) 
{
		// this function used if you've already got a name=value type
		// of string, and want to put it into the environment.  env_var
		// must therefore contain an '='.
	if ( !env_var ) {
		dprintf (D_ALWAYS, "SetEnv, env_var = NULL!\n" );
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

		// hack up string and pass to other SetEnv version.
	int namelen = (int) equalpos - (int) env_var;
	int valuelen = strlen(env_var) - namelen - 1;

	char *name = new char[namelen+1];
	char *value = new char[valuelen+1];
	strncpy ( name, env_var, namelen );
	strncpy ( value, equalpos+1, valuelen );
	name[namelen] = '\0';
	value[valuelen] = '\0';

	int retval = SetEnv ( name, value );

	delete [] name;
	delete [] value;
	return retval;
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

	char *hashed_var;
	if ( EnvVars.lookup( HashKey( env_var ), hashed_var ) == 0 ) {
			// found it
			// remove it
		EnvVars.remove( HashKey( env_var ) );
			// delete it
		delete [] hashed_var;
	}

	return TRUE;
}

const char *GetEnv( const char *env_var )
{
	assert( env_var );

#ifdef WIN32
	DWORD rc;
	static char *value = NULL;
	if ( value == NULL ) {
		value = (char *)malloc( 1024 );
	}
	rc = GetEnvironmentVariable( env_var, value, sizeof(value) );
	if ( rc > sizeof(value) - 1 ) {
			// environment variable value is too large,
			// reallocate our string and try again
		free( value );
		value = (char *)malloc( rc );
		rc = GetEnvironmentVariable( env_var, value, sizeof(value) );
	}
	if ( rc > sizeof(value) - 1 ) {
		dprintf( D_ALWAYS, "GetEnv(): environment variable still too large: %d\n", rc );
		return NULL;
	} else if ( rc == 0 ) {
		DWORD error = GetLastError();
		if ( error != ERROR_ENVVAR_NOT_FOUND ) {
			dprintf( D_ALWAYS,
					 "GetEnv(): GetEnvironmentVariable() failed, error=%d\n",
					 error );
		}
		return NULL;
	} else {
		return value;
	}
#else
	return getenv( env_var );
#endif
}
