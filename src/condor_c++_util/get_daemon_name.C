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

 

//////////////////////////////////////////////////////////////////////
// Methods to manipulate and manage daemon names
//////////////////////////////////////////////////////////////////////

#include "condor_common.h"
#include "condor_config.h"
#include "condor_string.h"
#include "get_full_hostname.h"
#include "my_hostname.h"
#include "my_username.h"
#include "condor_uid.h"

extern "C" {

// Return the host portion of a daemon name string.  Either the name
// includes an "@" sign, in which case we return whatever is after it,
// or it doesn't, in which case we just return what we got passed.
const char*
get_host_part( const char* name )
{
	char* tmp;
	if (name == NULL) return NULL;
	tmp = strrchr( name, '@' );
	if( tmp ) {
		return ++tmp;
	} else {
		return name;
	}
}


// Return a pointer to a newly allocated string that contains the
// valid daemon name that corresponds with the given name.  Basically,
// see if there's an '@'.  If so, resolve everything after it as a
// hostname.  If not, resolve what we were passed as a hostname.
// The string is allocated with strnewp() (or it's equivalent), so you
// should deallocate it with delete [].
char*
get_daemon_name( const char* name )
{
	char *tmp, *fullname, *tmpname, *daemon_name = NULL;
	int size;

	dprintf( D_HOSTNAME, "Finding proper daemon name for \"%s\"\n",
			 name ); 

		// First, check for a '@' in the name. 
	tmpname = strdup( name );
	tmp = strrchr( tmpname, '@' );
	if( tmp ) {
			// There's a '@'.
		*tmp = '\0';
		tmp++;
		if( *tmp ) {
				// There was something after the @, try to resolve it
				// as a full hostname:
			dprintf( D_HOSTNAME, "Daemon name has data after the '@', "
					 "trying to resolve \"%s\"\n", tmp ); 
			fullname = get_full_hostname( tmp );
		} else {
			dprintf( D_HOSTNAME, "Daemon name has no data after the '@', "
					 "trying to use the local host\n" ); 
				// There was nothing after the @, use localhost:
			fullname = strnewp( my_full_hostname() );
		}
		if( fullname ) {
			size = strlen(tmpname) + strlen(fullname) + 2;
			daemon_name = new char[size];
			sprintf( daemon_name, "%s@%s", tmpname, fullname );
			delete [] fullname;
		} 
	} else {
			// There's no '@', just try to resolve the hostname.
		dprintf( D_HOSTNAME, "Daemon name contains no '@', treating as a "
				 "regular hostname\n" );
		daemon_name = get_full_hostname( tmpname );
	}
	free( tmpname );

		// If there was an error, this will still be NULL.
	if( daemon_name ) { 
		dprintf( D_HOSTNAME, "Returning daemon name: \"%s\"\n", daemon_name );
	} else {
		dprintf( D_HOSTNAME, "Failed to construct daemon name, "
				 "returning NULL\n" );
	}
	return daemon_name;
}


// Given some name, create a valid name for ourself with our full
// hostname.  If the name contains an '@', strip off everything after
// it and append my_full_hostname().  If there's no '@', try to
// resolve what we have and see if it's my_full_hostname.  If so, use
// it, otherwise, use name@my_full_hostname().  We return the answer
// in a string which should be deallocated w/ delete [].
char*
build_valid_daemon_name( char* name ) 
{
	char *tmp, *tmpname, *daemon_name = NULL;
	int size;

		// This flag determines if we want to just return a copy of
		// my_full_hostname(), or if we want to append
		// "@my_full_hostname" to the name we were given.  The name we
		// were given might include an '@', in which case, we trim off
		// everything after the '@'.
	bool just_host = false;

	if( name && *name ) {
		tmpname = strnewp( name );
		tmp = strrchr( tmpname, '@' );
		if( tmp ) {
				// name we were passed has an '@', ignore everything
				// after (and including) the '@'.  
			*tmp = '\0';
		} else {
				// no '@', see if what we have is our hostname
			if( (tmp = get_full_hostname(name)) ) {
				if( !strcmp(tmp, my_full_hostname()) ) {
						// Yup, so just the full hostname.
					just_host = true;
				}					
				delete [] tmp;
			}
		}
	} else {
			// Passed NULL for the name.
		just_host = true;
	}

	if( just_host ) {
		daemon_name = strnewp( my_full_hostname() );
	} else {
		size = strlen(tmpname) + strlen(my_full_hostname()) + 2; 
		daemon_name = new char[size];
		sprintf( daemon_name, "%s@%s", tmpname, my_full_hostname() ); 
		delete [] tmpname;
	}
	return daemon_name;
}


/* 
   Return a string on the heap (must be deallocated with delete()) that 
   contains the default daemon name for the calling process.  If we're
   root (additionally, on UNIX, if we're condor), we default to the
   full hostname.  Otherwise, we default to username@full.hostname.
*/
char*
default_daemon_name( void )
{
	if( is_root() ) {
		return strnewp( my_full_hostname() );
	}
#ifndef WIN32
	if( getuid() == get_real_condor_uid() ) {
		return strnewp( my_full_hostname() );
	}
#endif /* ! LOSE32 */
	char* name = my_username();
	if( ! name ) {
		return NULL;
	}
	char* host = my_full_hostname();
	if( ! host ) {
		free( name );
		return NULL;
	}
	int size = strlen(name) + strlen(host) + 2;
	char* ans = new char[size];
	if( ! ans ) {
		free( name );
		return NULL;
	}
	sprintf( ans, "%s@%s", name, host );
	free(name);
	return ans;
}


} /* extern "C" */
