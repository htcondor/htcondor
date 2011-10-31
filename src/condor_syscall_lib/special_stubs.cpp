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


/* 
   This file contains special stubs that are needed to make certain
   things work within a user job that can't use their normal
   functionality.  
*/

#include "condor_common.h"
#include "std_univ_io.h"
#include "condor_sys.h"
#include "condor_debug.h"
#include "exit.h"
#include "shared_utils.h"
#include "get_port_range.h"
#include "condor_netdb.h"
#include "simplelist.h"
#include "condor_sockaddr.h"
#include <algorithm>

extern	StdUnivSock* syscall_sock;

condor_sockaddr
get_local_ipaddr() {
	return syscall_sock->my_addr();	
}

extern "C" {

extern	int		DebugFlags;
int		_condor_DebugFD = 0;

extern int SetSyscalls(int);
extern int SYSCALL(int, ...);


/*
	In the 6.3 series, REMOTE_syscall had been removed. Well, it turns out
	that 6.4 was supposed to be backwards compatible with 6.2. :( This means
	that some 6.2 starters are going to look for the symbol REMOTE_syscall in a
	6.3+(including 6.4) standard universe executable in order to determine if
	it had been linked with condor correctly. This function must do nothing
	except exist for a while until we get rid of it for good and in a
	non-backwards compatible way.
*/
void REMOTE_syscall(void) {} ;


/*
  _condor_dprintf_va is the real meat of dprintf().  We have different
  concerns inside the user job.  All we need to care about in here is
  making sure we have the right syscall mode.  Otherwise, we don't
  need to do anything complex with priv states, locking, etc.
*/
void
_condor_dprintf_va( int flags, const char* fmt, va_list args )
{
	int scm;
	int no_fd = FALSE;

		/* See if this is one of the messages we are logging */
	if( !(flags&DebugFlags) ) {
		return;
	}

		/* If dprintf() isn't initialized, don't seg fault */
	if( ! _condor_DebugFD ) {
		_condor_DebugFD = 2; /* stderr */
		no_fd = TRUE;
	}

		/*
		  When talking to the debug fd, you are talking to an fd that
		  is not visible to the user.  It is not entered in the
		  virtual file table, and, hence, you should be in unmapped
		  mode.
		*/
	scm = SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );

		/* Actually print the message */
	_condor_vfprintf_va( _condor_DebugFD, fmt, args );
	
		/* Restore our syscall mode */
	(void) SetSyscalls( scm );

	if( no_fd ) {
		_condor_DebugFD = 0;
	}
}


int get_port_range(int is_outgoing, int *low_port, int *high_port)
{
	char *low = NULL, *high = NULL;

	// is_outgoing is ignored here.  all connections are assumed to be outgoing
	// since cedar should not be opening any listen sockets in the userjob.

	if ( (low = getenv("_condor_LOWPORT")) == NULL ) {
        dprintf(D_NETWORK, "_condor_LOWPORT undefined\n");
		return FALSE;
    }
	if ( (high = getenv("_condor_HIGHPORT")) == NULL ) {
        dprintf(D_ALWAYS, "_condor_LOWPORT is defined but _condor_HIGHPORT undefined!\n");
		return FALSE;
	}

	*low_port = atoi(low);
	*high_port = atoi(high);

	if(*low_port < 1024 || *high_port < 1024 || *low_port > *high_port) {
		dprintf(D_ALWAYS, "get_port_range - invalid LOWPORT(%d) \
		                   and/or HIGHPORT(%d)\n",
		                   *low_port, *high_port);
		return FALSE;
	}

	return TRUE;
}


int
_condor_bind_all_interfaces( void )
{
	char *tmp = NULL;
	int bind_all = FALSE;

	if( (tmp = getenv("_condor_BIND_ALL_INTERFACES")) == NULL ) {
        dprintf(D_NETWORK, "_condor_BIND_ALL_INTERFACES undefined\n");
		return TRUE;
    }
	
	switch( tmp[0] ) {
	case 'T':
	case 't':
		bind_all = TRUE;
		break;
	default:
		bind_all = FALSE;
		break;
	}

	return bind_all;
}


struct hostent *
condor_gethostbyname( const char *name )
{
	return gethostbyname( name );
}

struct hostent *
condor_gethostbyaddr( const char *addr, SOCKET_LENGTH_TYPE len, int type )

{
	return gethostbyaddr( addr, len, type );
}

int
condor_gethostname( char *name, size_t namelen )
{
	return gethostname( name, namelen );
}


#if !defined(WIN32)
/*
  Can't open something because we are out of fd's.  Try to let
  somebody know what happened.  In the user job, we don't want to
  close a bunch of fds, since we'll loose our debug socket, which is
  always open.  In fact, we shouldn't have to close anything to be
  able to dprintf().  So, just dprintf(), flush the fd for good
  measure, and call Suicide() (so we don't leave the job queue).  
*/
void
_condor_fd_panic( int line, const char* file )
{
	dprintf( D_ALWAYS,
			 "**** PANIC -- OUT OF FILE DESCRIPTORS at line %d in %s\n", 
			 line, file );
	Suicide();
}
#endif /* ! LOOSE32 */

#if HAVE_EXT_GCB
int
GCB_local_bind(int fd, struct sockaddr *my_addr, socklen_t addrlen)
{
	return bind(fd, my_addr, addrlen);
}
#endif /* HAVE_EXT_GCB */

/* For MyString */
int vprintf_length(const char *format, va_list args) { return 0; }


} /* extern "C" */


/*
  We need our own definition of my_ip_string(), which is used by the
  utilities in internet.c (which in turn are used by CEDAR).  This
  version, instead of looking in a config file for magic parameters,
  looks at the existing syscall_sock and grabs the IP address off of
  there.
*/
char*
my_ip_string()
{
	static char ipbuf[INET6_ADDRSTRLEN] = {0,};
	syscall_sock->my_addr().to_ip_string(ipbuf, sizeof(ipbuf));
	return ipbuf;
}

std::vector<condor_sockaddr> resolve_hostname(const char* hostname) {
	std::vector<condor_sockaddr> ret;
	addrinfo* addrs = 0;
	int result;

	result = getaddrinfo(hostname, NULL, NULL, &addrs);
	if (result != 0 || !addrs) {
		return ret;
	}

	addrinfo* iter = addrs;
	while (iter) {
		ret.push_back(condor_sockaddr(iter->ai_addr));
		iter = iter->ai_next;
	}
	freeaddrinfo(addrs);


		// in order to completely eliminate duplicates, sort() should be 
		// called before unique().

		// however, we do not need strict uniqueness here.
		// also, most of duplication comes from protocol variety.
		// e.g. same address with different socket type such as 
		// SOCK_STREAM, SOCK_DGRAM
	std::vector<condor_sockaddr>::iterator it;
	it = std::unique(ret.begin(), ret.end());
	ret.resize(it - ret.begin());
	return ret;
}

std::vector<condor_sockaddr> resolve_hostname(const MyString& hostname) {
	return resolve_hostname(hostname.Value());
}
