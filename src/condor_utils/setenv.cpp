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

#include "setenv.h"

#if defined(DARWIN)
#include <crt_externs.h>
#else
extern DLL_IMPORT_MAGIC char **environ;
#endif

char **GetEnviron()
{
#if defined(DARWIN)
	return *_NSGetEnviron();
#else
	return environ;
#endif
}

int SetEnv( const char *key, const char *value)
{
	assert(key);
	assert(value);
#ifdef WIN32
	if ( !SetEnvironmentVariable(key, value) ) {
		dprintf(D_ALWAYS,
			"SetEnv(%s, %s): SetEnvironmentVariable failed, "
			"errno=%d\n", key, value, GetLastError());
		return FALSE;
	}
#else
	if( setenv(key, value, 1) != 0 )
	{
		dprintf(D_ERROR, "setenv failed: %s (errno=%d)\n",
				strerror(errno), errno);
		return FALSE;
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

	char const *equalpos = NULL;

	if ( ! (equalpos = strchr( env_var, '=' )) ) {
		dprintf (D_ALWAYS, "SetEnv, env_var has no '='\n" );
		dprintf (D_ALWAYS, "env_var = \"%s\"\n", env_var );
		return FALSE; 
	}

		// hack up string and pass to other SetEnv version.
	size_t namelen = equalpos - env_var;
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
			"UnsetEnv(%s): SetEnvironmentVariable failed, errno=%d\n",
			env_var, GetLastError());
		return FALSE;
	}
#else
	unsetenv(env_var);
#endif

	return TRUE;
}

const char *GetEnv( const char *env_var )
{
	assert( env_var );

#ifdef WIN32
	static std::string result;
	return GetEnv( env_var, result );
#else
	return getenv( env_var );
#endif
}

const char *GetEnv( const char *env_var, std::string &result )
{
	assert( env_var );

#ifdef WIN32
	DWORD rc;
	int value_size = 0;
	char *value = NULL;

	value_size = 1024;
	value = (char *)malloc( value_size );

	rc = GetEnvironmentVariable( env_var, value, value_size );
	if ( rc > value_size - 1 ) {
			// environment variable value is too large,
			// reallocate our string and try again
		free( value );
		value_size = rc;
		value = (char *)malloc( value_size );
		rc = GetEnvironmentVariable( env_var, value, value_size );
	}
	if ( rc > value_size - 1 ) {
		dprintf( D_ALWAYS, "GetEnv(): environment variable still too large: %d\n", rc );
		free( value );
		return NULL;
	} else if ( rc == 0 ) {
		DWORD error = GetLastError();
		if ( error != ERROR_ENVVAR_NOT_FOUND ) {
			dprintf( D_ALWAYS,
					 "GetEnv(): GetEnvironmentVariable() failed, error=%d\n",
					 error );
		}
		free( value );
		return NULL;
	} else {
		result = value;
		free( value );
	}
#else
	const char* val = getenv(env_var);
	result = val ? val : "";
#endif
	return result.c_str();
}
