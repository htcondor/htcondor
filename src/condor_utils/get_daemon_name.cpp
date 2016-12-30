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

//////////////////////////////////////////////////////////////////////
// Methods to manipulate and manage daemon names
//////////////////////////////////////////////////////////////////////

#include "condor_common.h"
#include "condor_config.h"
#include "condor_string.h"
#include "my_hostname.h"
#include "my_username.h"
#include "condor_uid.h"
#include "ipv6_hostname.h"

// Return the host portion of a daemon name string.  Either the name
// includes an "@" sign, in which case we return whatever is after it,
// or it doesn't, in which case we just return what we got passed.
const char*
get_host_part( const char* name )
{
	char const *tmp;
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
// see if there's an '@'.  If so, leave the name alone. If not,
// resolve what we were passed as a hostname.  The string is allocated
// with strnewp() (or it's equivalent), so you should deallocate it
// with delete [].
char*
get_daemon_name( const char* name )
{
	char * daemon_name = NULL;
	dprintf( D_HOSTNAME, "Finding proper daemon name for \"%s\"\n", name );

	const char * hasAt = strrchr( name, '@' );
	if( hasAt ) {
		dprintf( D_HOSTNAME, "Daemon name has an '@', we'll leave it alone\n" );
		daemon_name = strnewp( name );
	} else {
		dprintf( D_HOSTNAME, "Daemon name contains no '@', treating as a "
				 "regular hostname\n" );
		MyString fqdn = get_fqdn_from_hostname( name );
		daemon_name = strnewp( fqdn.Value() );
	}

	if( daemon_name ) {
		dprintf( D_HOSTNAME, "Returning daemon name: \"%s\"\n", daemon_name );
	} else {
		dprintf( D_HOSTNAME, "Failed to construct daemon name, returning NULL\n" );
	}
	return daemon_name;
}


// Given some name, create a valid name for ourself with our full
// hostname.  If the name contains an '@', leave it alone.  If there's
// no '@', try to resolve what we have and see if it's
// get_local_fqdn().Value.  If so, use it, otherwise, use
// name@get_local_fqdn().Value().  We return the answer in a string which
// should be deallocated w/ delete [].
char*
build_valid_daemon_name( const char* name )
{
	char * daemon_name = NULL;
	if( name && *name ) {
		const char * hasAt = strrchr( name, '@' );
		if( hasAt ) {
			daemon_name = strnewp( name );
		} else {
			MyString fqdn = get_fqdn_from_hostname( name );
			if( fqdn.Length() > 0 && strcasecmp( get_local_fqdn().Value(), fqdn.Value() ) == 0 ) {
				daemon_name = strnewp( get_local_fqdn().Value() );
			} else {
				int size = strlen(name) + get_local_fqdn().length() + 2;
				daemon_name = new char[size];
				sprintf( daemon_name, "%s@%s", name, get_local_fqdn().Value() );
			}
		}
	} else {
		daemon_name = strnewp( get_local_fqdn().Value() );
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
		return strnewp( get_local_fqdn().Value() );
	}
#ifndef WIN32
	if( getuid() == get_real_condor_uid() ) {
		return strnewp( get_local_fqdn().Value() );
	}
#endif /* ! LOSE32 */
	char* name = my_username();
	if( ! name ) {
		return NULL;
	}
	if( get_local_fqdn().length() == 0) {
		free( name );
		return NULL;
	}
	int size = strlen(name) + get_local_fqdn().length() + 2;
	char* ans = new char[size];
	if( ! ans ) {
		free( name );
		return NULL;
	}
	sprintf( ans, "%s@%s", name, get_local_fqdn().Value() );
	free(name);
	return ans;
}

