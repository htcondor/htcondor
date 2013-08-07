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
#include "condor_constants.h"
#include "condor_io.h"
#include "condor_uid.h"
#include "sock.h"
#include "condor_network.h"
#include "internet.h"
#include "ipv6_hostname.h"
#include "condor_debug.h"
#include "condor_socket_types.h"
#include "get_port_range.h"
#include "condor_netdb.h"
#include "daemon_core_sock_adapter.h"
#include "selector.h"
#include "authentication.h"
#include "condor_sockfunc.h"
#include "condor_ipv6.h"
#include "condor_config.h"

#ifdef HAVE_EXT_OPENSSL
#include "condor_crypt_blowfish.h"
#include "condor_crypt_3des.h"
#include "condor_md.h"                // Message authentication stuff
#endif

#if !defined(WIN32)
#define closesocket close
#endif

void dprintf ( int flags, Sock & sock, const char *fmt, ... )
{
    va_list args;
    va_start( args, fmt );
    _condor_dprintf_va( flags, (DPF_IDENT)sock.getUniqueId(), fmt, args );
    va_end( args );
}

DaemonCoreSockAdapterClass daemonCoreSockAdapter;

unsigned int Sock::m_nextUniqueId = 1;

Sock::Sock() : Stream() {
	_sock = INVALID_SOCKET;
	_state = sock_virgin;
	_timeout = 0;
	_fqu = NULL;
	_fqu_user_part = NULL;
	_fqu_domain_part = NULL;
	_auth_method = NULL;
	_auth_methods = NULL;
	_auth_name = NULL;
	_crypto_method = NULL;
	_tried_authentication = false;
	ignore_connect_timeout = FALSE;		// Used by the HA Daemon
	connect_state.connect_failed = false;
	connect_state.this_try_timeout_time = 0;
	connect_state.retry_timeout_time = 0;
	connect_state.retry_wait_timeout_time = 0;
	connect_state.failed_once = false;
	connect_state.connect_refused = false;
	connect_state.old_timeout_value = 0;
	connect_state.non_blocking_flag = false;
	connect_state.host = NULL;
	connect_state.port = 0;
	connect_state.connect_failure_reason = NULL;
	_who.clear();
	m_uniqueId = m_nextUniqueId++;

    crypto_ = NULL;
    mdMode_ = MD_OFF;
    mdKey_ = 0;

	m_connect_addr = NULL;
    addr_changed();
}

Sock::Sock(const Sock & orig) : Stream() {

	// initialize everything in the new sock
	_sock = INVALID_SOCKET;
	_state = sock_virgin;
	_timeout = 0;
	_fqu = NULL;
	_fqu_user_part = NULL;
	_fqu_domain_part = NULL;
	_auth_method = NULL;
	_auth_methods = NULL;
	_auth_name = NULL;
	_crypto_method = NULL;
	_tried_authentication = false;
	ignore_timeout_multiplier = orig.ignore_timeout_multiplier;
	connect_state.connect_failed = false;
	connect_state.failed_once = false;
	connect_state.connect_refused = false;
	connect_state.this_try_timeout_time = 0;
	connect_state.retry_timeout_time = 0;
	connect_state.retry_wait_timeout_time = 0;
	connect_state.old_timeout_value = 0;
	connect_state.non_blocking_flag = false;
	connect_state.host = NULL;
	connect_state.port = 0;
	connect_state.connect_failure_reason = NULL;
	_who.clear();
	// TODO Do we want a new unique ID here?
	m_uniqueId = m_nextUniqueId++;

    crypto_ = NULL;
    mdMode_ = MD_OFF;
    mdKey_ = 0;

	m_connect_addr = NULL;
    addr_changed();

	// now duplicate the underlying network socket
#ifdef WIN32
	// Win32
	SOCKET DuplicateSock = INVALID_SOCKET;
	WSAPROTOCOL_INFO sockstate;

	dprintf(D_FULLDEBUG,"About to sock duplicate, old sock=%X new sock=%X state=%d\n",
		orig._sock,_sock,_state);

	if (WSADuplicateSocket(orig._sock,GetCurrentProcessId(),&sockstate) == 0)
	{
		// success on WSADuplicateSocket, now open it
		DuplicateSock = WSASocket(FROM_PROTOCOL_INFO, FROM_PROTOCOL_INFO,
			FROM_PROTOCOL_INFO, &sockstate, 0, 0);
	}
	if ( DuplicateSock == INVALID_SOCKET ) {
		EXCEPT("ERROR failed to duplicate socket, err=%d",
				WSAGetLastError());
	}
	// if made it here, successful duplication
	_sock = DuplicateSock;
	dprintf(D_FULLDEBUG,"Socket duplicated, old sock=%X new sock=%X state=%d\n",
		orig._sock,_sock,_state);
#else
	// Unix
	_sock = dup(orig._sock);
	if ( _sock < 0 ) {
		// dup failed, we're screwed
		EXCEPT("ERROR: dup() failed in Sock copy ctor");
	}
#endif
	ignore_connect_timeout = orig.ignore_connect_timeout;	// Used by HAD
}

Sock::~Sock()
{
    delete crypto_;
	crypto_ = NULL;
    delete mdKey_;
	mdKey_ = NULL;

	if ( connect_state.host ) free(connect_state.host);
	if ( connect_state.connect_failure_reason) {
		free(connect_state.connect_failure_reason);
	}
	if (_auth_method) {
		free(_auth_method);
		_auth_method = NULL;
	}
	if (_auth_methods) {
		free(_auth_methods);
		_auth_methods = NULL;
	}
	free(_auth_name);
	if (_crypto_method) {
		free(_crypto_method);
		_crypto_method = NULL;
	}
	if (_fqu) {
		free(_fqu);
		_fqu = NULL;
	}
	if (_fqu_user_part) {
		free(_fqu_user_part);
		_fqu_user_part = NULL;
	}
	if (_fqu_domain_part) {
		free(_fqu_domain_part);
		_fqu_domain_part = NULL;
	}
	free( m_connect_addr );
	m_connect_addr = NULL;
}

#if defined(WIN32)

#if !defined(SKIP_AUTHENTICATION)
#include "authentication.h"
HINSTANCE _condor_hSecDll = NULL;
#endif

	// This class has a global ctor/dtor, and loads in 
	// WINSOCK.DLL and, if security support is compiled in, SECURITY.DLL.
static bool _condor_SockInitializerCalled = false;
void SockInitializer::init() 
{
	called_from_init = true;
}

SockInitializer::SockInitializer() 
{
	WORD wVersionRequested = MAKEWORD( 2, 0 );
	WSADATA wsaData;
	int err;

	called_from_init = false;	// must set this before returning

	if ( _condor_SockInitializerCalled ) {
		// we've already been here
		return;
	}

	err = WSAStartup( wVersionRequested, &wsaData );
	if ( err < 0 ) {
		fprintf( stderr, "Can't find usable WinSock DLL!\n" );	
		exit(1);
	}

	if ( LOBYTE( wsaData.wVersion ) != 2 || HIBYTE( wsaData.wVersion ) != 0 ) {
		fprintf( stderr, "Warning: using WinSock version %d.%d, requested 1.1\n",
			LOBYTE( wsaData.wVersion ), HIBYTE( wsaData.wVersion ) );
	}

#if !defined(SKIP_AUTHENTICATION)
	if ( (_condor_hSecDll = LoadLibrary( "security.dll" )) == NULL ) {
		fprintf(stderr,"Can't find SECURITY.DLL!\n");
		exit(1);
	}
#endif
	_condor_SockInitializerCalled = true;
}	// end of SockInitializer() ctor

SockInitializer::~SockInitializer() 
{
	if ( called_from_init ) {
		return;
	}
	if (WSACleanup() < 0) {
		fprintf(stderr, "WSACleanup() failed, errno = %d\n", 
				WSAGetLastError());
	}
#if !defined(SKIP_AUTHENTICATION)
	if ( _condor_hSecDll ) {
		FreeLibrary(_condor_hSecDll);			
	}
#endif
}	// end of ~SockInitializer() dtor

static SockInitializer _SockInitializer;

#endif	// of ifdef WIN32

/*
**	Methods shared by all Socks
*/


int Sock::getportbyserv(
	char	*s
	)
{
	servent		*sp;
	const char	*my_prot=0;

	if (!s) return -1;

	switch(type()){
		case safe_sock:
			my_prot = "udp";
			break;
		case reli_sock:
			my_prot = "tcp";
			break;
		default:
			ASSERT(0);
	}

	if (!(sp = getservbyname(s, my_prot))) return -1;

	return ntohs(sp->s_port);
}

#if defined(WIN32) && defined(_WINSOCK2API_)
int Sock::assign(
	LPWSAPROTOCOL_INFO pProtoInfo
	)
{
	if (_state != sock_virgin) return FALSE;

	// verify the win32 socket type we are about to inherit matches
	// our CEDAR socket type
	switch(type()){
		case safe_sock:
			ASSERT( pProtoInfo->iSocketType == SOCK_DGRAM );
			break;
		case reli_sock:
			ASSERT( pProtoInfo->iSocketType == SOCK_STREAM );
			break;
		default:
			ASSERT(0);
	}

	int socket_fd = WSASocket(FROM_PROTOCOL_INFO, FROM_PROTOCOL_INFO, 
					  FROM_PROTOCOL_INFO, pProtoInfo, 0, 0);

	return assign( socket_fd );
}
#endif

#ifdef WIN32
int Sock::set_inheritable( int flag )
{
	// on unix, all sockets are always inheritable by a child process.
	// but on Win32, each individual socket has a flag that says if it can
	// or cannot be inherited by a child.  this method effectively sets
	// that flag (by duplicating the socket).  
	// pass flag as "TRUE" to make this socket inheritable, "FALSE" to make
	// it private to this process.
	// Returns TRUE on sucess, FALSE on failure.

	SOCKET DuplicateSock;

	if ( (flag != TRUE) && (flag != FALSE) )
		return FALSE;	// flag must be either TRUE or FALSE...

	if (!DuplicateHandle(GetCurrentProcess(),
        (HANDLE)_sock,
        GetCurrentProcess(),
        (HANDLE*)&DuplicateSock,
        0,
        flag, // inheritable flag
        DUPLICATE_SAME_ACCESS)) {
			// failed to duplicate
			dprintf(D_ALWAYS,"ERROR: DuplicateHandle() failed "
			                 "in Sock:set_inheritable(%d), error=%d\n"
				  ,flag,GetLastError());
			closesocket(DuplicateSock);
			return FALSE;
	}
	// if made it here, successful duplication; replace original
	closesocket(_sock);
	_sock = DuplicateSock;

	return TRUE;
}
#endif	// of WIN32

int Sock::move_descriptor_up()
{
	/* This function must be called IMMEDIATELY after a call to
	 * socket() or accept().  It gives CEDAR an opportunity to 
	 * move the descriptor if needed on this platform
	 */

#ifdef Solaris
	/* On Solaris, the silly stdio library will fail if the underlying
	 * file descriptor is > 255.  Thus if we have lots of sockets open,
	 * calls to safe_fopen_wrapper() will start to fail on Solaris.  In Condor, this 
	 * usually means dprintf() will EXCEPT.  So to avoid this, we reserve
	 * sockets between 101 and 255 to be ONLY for files/pipes, and NOT
	 * for network sockets.  We acheive this by moving the underlying
	 * descriptor above 255 if it falls into the reserved range. We don't
	 * bother moving descriptors until they are > 100 --- this prevents us
	 * from doing anything in the common case that the process has less
	 * than 100 active descriptors.  We also do not do anything if the
	 * file descriptor table is too small; the application should limit
	 * network socket usage to some sensible percentage of the file
	 * descriptor limit anyway.
	 */
	SOCKET new_fd = -1;
	if ( _sock > 100 && _sock < 256 && getdtablesize() > 256 ) {
		new_fd = fcntl(_sock, F_DUPFD, 256);
		if ( new_fd >= 256 ) {
			::closesocket(_sock);
			_sock = new_fd;
		} else {
			// the fcntl must have failed
			dprintf(D_NETWORK, "Sock::move_descriptor_up failed: %s\n", 
								strerror(errno));
			return FALSE;
		}
	}
#endif

	return TRUE;
}

int Sock::assign(SOCKET sockd)
{
	int		my_type = SOCK_DGRAM;

	if (_state != sock_virgin) return FALSE;

	if (sockd != INVALID_SOCKET){
		_sock = sockd;		/* Could we check for correct protocol ? */
		/* should we check that sockd matches to IPv6 mode? */
		_state = sock_assigned;

		_who.clear();
		condor_getpeername(_sock, _who);

		if ( _timeout > 0 ) {
			timeout_no_timeout_multiplier( _timeout );
		}
		return TRUE;
	}

	int af_type;
	if (_condor_is_ipv6_mode())
		af_type = AF_INET6;
	else
		af_type = AF_INET;

	switch(type()){
		case safe_sock:
			my_type = SOCK_DGRAM;
			break;
		case reli_sock:
			my_type = SOCK_STREAM;
			break;
		default:
			ASSERT(0);
	}

#ifndef WIN32 /* Unix */
	errno = 0;
#endif
	if ((_sock = socket(af_type, my_type, 0)) == INVALID_SOCKET) {
#ifndef WIN32 /* Unix... */
		if ( errno == EMFILE ) {
			_condor_fd_panic( __LINE__, __FILE__ ); /* Calls dprintf_exit! */
		}
#endif
		return FALSE;
	}

	// move the underlying descriptor if we need to on this platform
	if ( !move_descriptor_up() ) {
		::closesocket(_sock);
		_sock = INVALID_SOCKET;
		return FALSE;
	}

	// on WinNT, sockets are created as inheritable by default.  we
	// want to create the socket as non-inheritable by default.  so 
	// we duplicate the socket as non-inheritable and then close
	// the default inheritable socket.  Note on Win95, it is the opposite:
	// i.e. on Win95 sockets are created non-inheritable by default.
	// note: on UNIX, set_inheritable just always return TRUE.
	if ( !set_inheritable(FALSE) ) {
		::closesocket(_sock);
		_sock = INVALID_SOCKET;
		return FALSE;
	}
	
	_state = sock_assigned;

	// If we called timeout() previously on this object, then called close() on the
	// socket, we are now left with _timeout set to some positive value __BUT__ the
	// socket itself has never been set to non-blocking mode with some fcntl or whatever.
	// SO, we check here for this situation and rectify by calling timeout() again. -Todd 10/97.
	if ( _timeout > 0 )
		timeout_no_timeout_multiplier( _timeout );

    addr_changed();
	return TRUE;
}


int
Sock::bindWithin(const int low_port, const int high_port, bool outbound)
{
	bool bind_all = (bool)_condor_bind_all_interfaces();
	bool ipv6_mode = _condor_is_ipv6_mode();

	// Use hash function with pid to get the starting point
    struct timeval curTime;
#ifndef WIN32
    (void) gettimeofday(&curTime, NULL);
#else
	// Win32 does not have gettimeofday, sigh.
	curTime.tv_usec = ::GetTickCount() % 1000000;
#endif

	// int pid = (int) getpid();
	int range = high_port - low_port + 1;
	// this line must be changed to use the hash function of condor
	int start_trial = low_port + (curTime.tv_usec * 73/*some prime number*/ % range);

	int this_trial = start_trial;
	do {
		condor_sockaddr			addr;
		int bind_return_val;

		addr.clear();
		if( bind_all ) {
			if (ipv6_mode)
				addr.set_ipv6();
			else
				addr.set_ipv4();
			addr.set_addr_any();
		} else {
			addr = get_local_ipaddr();
			// what if the socket type does not match?
			// e.g. addr is ipv6 but ipv6 mode is not turned on?
			if (addr.is_ipv4() && ipv6_mode)
				addr.convert_to_ipv6();
		}
		addr.set_port((unsigned short)this_trial++);

#ifndef WIN32
		priv_state old_priv;
		if (this_trial <= 1024) {
			// use root priv for the call to bind to allow privileged ports
			old_priv = PRIV_UNKNOWN;
			old_priv = set_root_priv();
		}
#endif

		bind_return_val = _bind_helper(_sock, addr, outbound, false);

        addr_changed();

#ifndef WIN32
		if (this_trial <= 1024) {
			set_priv (old_priv);
		}
#endif

		if (  bind_return_val == 0 ) { // success
			dprintf(D_NETWORK, "Sock::bindWithin - bound to %d...\n", this_trial-1);
			return TRUE;
		} else {
#ifdef WIN32
			int error = WSAGetLastError();
			dprintf(D_NETWORK, 
				"Sock::bindWithin - failed to bind to port %d: WSAError = %d\n", this_trial-1, error );
#else
			dprintf(D_NETWORK, "Sock::bindWithin - failed to bind to port %d: %s\n", this_trial-1, strerror(errno));
#endif
		}

		if ( this_trial > high_port )
			this_trial = low_port;
	} while(this_trial != start_trial);

	dprintf(D_ALWAYS, "Sock::bindWithin - failed to bind any port within (%d ~ %d)\n",
	        low_port, high_port);

	return FALSE;
}


int Sock::bind(bool outbound, int port, bool loopback)
{
	condor_sockaddr addr;
	int bind_return_value;

	// Following lines are added because some functions in condor call
	// this method without checking the port numbers returned from
	// such as 'getportbyserv'
	if (port < 0) {
        dprintf(D_ALWAYS, "Sock::bind - invalid port %d\n", port);
        return FALSE;
    }

	// if stream not assigned to a sock, do it now	*/
	if (_state == sock_virgin) assign();

	if (_state != sock_assigned) {
		dprintf(D_ALWAYS, "Sock::bind - _state is not correct\n");
		return FALSE;
	}

	// If 'port' equals 0 and if we have 'LOWPORT' and 'HIGHPORT' defined
	// in the config file for security, we will bind this Sock to one of
	// the port within the range defined by these variables rather than
	// an arbitrary free port. /* 07/27/2000 - sschang */
	//
	// zmiller on 2006-02-09 says:
	//
	// however,
	//
	// now that we have the ability to bind to privileged ports (below 1024)
	// for the daemons, we need a separate port range for the client tools
	// (which do not run as root) to bind to.  if none is specified, we bind
	// to any non-privileged port.  lots of firewalls still allow arbitrary
	// ports for outgoing connections, and this will work for that setup.
	// if the firewall doesn't allow it, the connect will just fail anyways.
	//
	// this is part of why there is an outbound flag.  when this is true,
	// we know to check OUT_LOWPORT and OUT_HIGHPORT first, and then fall back
	// to LOWPORT and HIGHPORT.
	//
	// likewise, in the interest of consistency, the server side will now
	// check first for IN_LOWPORT and IN_HIGHPORT, then fallback to
	// LOWPORT and HIGHPORT, and then to an arbitrary port.
	//
	// errors in configuration (like LOWPORT being greater than HIGHPORT)
	// still return FALSE whenever they are encountered which will cause
	// condor to attempt to bind to a dynamic port instead. an error is
	// printed to D_ALWAYS in get_port_range.

	int lowPort, highPort;
	if ( port == 0 && !loopback && get_port_range((int)outbound, &lowPort, &highPort) == TRUE ) {
			// Bind in a specific port range.
		if ( bindWithin(lowPort, highPort, outbound) != TRUE ) {
			return FALSE;
		}
	} else {
			// Bind to a dynamic port.

		if (_condor_is_ipv6_mode())
			addr.set_ipv6();
		else
			addr.set_ipv4();
		if( loopback ) {
			addr.set_loopback();
		} else if( (bool)_condor_bind_all_interfaces() ) {
			addr.set_addr_any();
		} else {
			addr = get_local_ipaddr();
			if (addr.is_ipv4() && _condor_is_ipv6_mode())
				addr.convert_to_ipv6();
		}
		addr.set_port((unsigned short)port);

#ifndef WIN32
		priv_state old_priv;
		if(port > 0 && port < 1024) {
			// use root priv for the call to bind to allow privileged ports
			old_priv = PRIV_UNKNOWN;
			old_priv = set_root_priv();
		}
#endif

		bind_return_value = _bind_helper(_sock, addr, outbound, loopback);

        addr_changed();

#ifndef WIN32
        int bind_errno = errno;
		if(port > 0 && port < 1024) {
			set_priv (old_priv);
		}
#endif
		if ( bind_return_value < 0) {
	#ifdef WIN32
			int error = WSAGetLastError();
			dprintf( D_ALWAYS, "Sock::bind failed: WSAError = %d\n", error );
	#else
			dprintf(D_ALWAYS, "Sock::bind failed: errno = %d %s\n", bind_errno, strerror(bind_errno));
	#endif
			return FALSE;
		}
	}

	_state = sock_bound;

	// Make certain SO_LINGER is Off.  This will result in the default
	// of closesocket returning immediately and the system attempts to 
	// send any unsent data.
	// Also set KEEPALIVE so we know if the socket disappears.
	if ( type() == Stream::reli_sock ) {
		struct linger linger = {0,0};
		int on = 1;
		setsockopt(SOL_SOCKET, SO_LINGER, (char*)&linger, sizeof(linger));
		setsockopt(SOL_SOCKET, SO_KEEPALIVE, (char*)&on, sizeof(on));
               /* Set no delay to disable Nagle, since we buffer all our
			      relisock output and it degrades performance of our
			      various chatty protocols. -Todd T, 9/05
			   */
		setsockopt(IPPROTO_TCP, TCP_NODELAY, (char*)&on, sizeof(on));
	}

	return TRUE;
}

bool Sock::bind_to_loopback(bool outbound,int port)
{
	return bind(outbound,port,true) == TRUE;
}

int Sock::set_os_buffers(int desired_size, bool set_write_buf)
{
	int current_size = 0;
	int previous_size = 0;
	int attempt_size = 0;
	int command;
	SOCKET_LENGTH_TYPE temp;

	if (_state == sock_virgin) assign();
	
	if ( set_write_buf ) {
		command = SO_SNDBUF;
	} else {
		command = SO_RCVBUF;
	}

	// Log the current size since Todd is curious.  :^)
	temp = sizeof(int);
	::getsockopt(_sock,SOL_SOCKET,command,
			(char*)&current_size,(socklen_t*)&temp);
	dprintf(D_FULLDEBUG,"Current Socket bufsize=%dk\n",
		current_size / 1024);
	current_size = 0;

	/* 
		We want to set the socket buffer size to be as close
		to the desired size as possible.  Unfortunatly, there is no
		contant defined which states the maximum size possible.  So
		we keep raising it up 1k at a time until (a) we got up to the
		desired value, or (b) it is not increasing anymore.  We ignore
		the return value from setsockopt since on some platforms this 
		could signal a value which is too low...
	*/
	 
	do {
		attempt_size += 1024;
		if ( attempt_size > desired_size ) {
			attempt_size = desired_size;
		}
		(void) setsockopt( SOL_SOCKET, command,
						   (char*)&attempt_size, sizeof(int) );

		previous_size = current_size;
		temp = sizeof(int);
		::getsockopt( _sock, SOL_SOCKET, command,
 					  (char*)&current_size, (socklen_t*)&temp );

	} while ( ( previous_size < current_size ) &&
			  ( attempt_size < desired_size  ) );

	return current_size;
}


int Sock::setsockopt(int level, int optname, const char* optval, int optlen)
{
	/* if stream not assigned to a sock, do it now	*/
	if (_state == sock_virgin) assign();

	if(::setsockopt(_sock, level, optname, optval, optlen) < 0)
	{
		return FALSE;
	}
	return TRUE; 
}

bool Sock::guess_address_string(char const* host, int port, condor_sockaddr& addr) {
	dprintf(D_HOSTNAME, "Guess address string for host = %s, port = %d\n",
			host, port);
	/* might be in <x.x.x.x:x> notation				*/
	if (host[0] == '<') {
		addr.from_sinful(host);
		dprintf(D_HOSTNAME, "it was sinful string. ip = %s, port = %d\n",
				addr.to_ip_string().Value(), addr.get_port());
	}
	/* try to get a decimal notation 	 			*/
	else if ( addr.from_ip_string(host) ) {
			// nothing to do here
		addr.set_port(port);
	}
	/* if dotted notation fails, try host database	*/
	else{
		std::vector<condor_sockaddr> addrs;
		addrs = resolve_hostname(host);
		if (addrs.empty())
			return false;
		addr = addrs.front();
		addr.set_port(port);
	}
	return true;
}

int Sock::do_connect(
	char const	*host,
	int		port,
	bool	non_blocking_flag
	)
{
	if (!host || port < 0) return FALSE;

	_who.clear();
	if (!guess_address_string(host, port, _who)) {
		return FALSE;
	}

	if (_condor_is_ipv6_mode() && _who.is_ipv4())
		_who.convert_to_ipv6();

		// current code handles sinful string and just hostname differently.
		// however, why don't we just use sinful string at all?
	if (host[0] == '<') {
		set_connect_addr(host);
	}
	else { // otherwise, just use ip string.
		set_connect_addr(_who.to_ip_string().Value());
	}
    addr_changed();

	// now that we have set _who (useful for getting informative
	// peer_description), see if we should do a reverse connect
	// instead of a forward connect.  Also see if we are connecting
	// to a shared port (SharedPortServer) that needs further information
	// to route us to the final destination.

	int retval=special_connect(host,port,non_blocking_flag);
	if( retval != CEDAR_ENOCCB ) {
		return retval;
	}

		/* we bind here so that a sock may be	*/
		/* assigned to the stream if needed		*/
		/* TRUE means this is an outgoing connection */
	if (_state == sock_virgin || _state == sock_assigned) {
		bind(true);
	}

	if (_state != sock_bound) return FALSE;

	if (_timeout < CONNECT_TIMEOUT) {
			// NOTE: if _timeout == 0 (no connect() timeout), we still
			// have a non-zero retry timeout, so we will not keep
			// retrying indefinitely
		connect_state.retry_timeout_interval = CONNECT_TIMEOUT;
	} else {
		connect_state.retry_timeout_interval = _timeout;
	}

	// Used by HAD
	// if doNotEnforceMinimalCONNECT_TIMEOUT() was previously called
	// than we don't enforce a minimal amount of CONNECT_TIMEOUT seconds
	// for connect_state.retry_timeout_interval
	if(ignore_connect_timeout == TRUE){
		connect_state.retry_timeout_interval = _timeout;
	}

	connect_state.first_try_start_time = time(NULL);
	connect_state.retry_timeout_time = time(NULL) + connect_state.retry_timeout_interval;
	connect_state.this_try_timeout_time = time(NULL)+_timeout;
	if(_timeout == 0) {
			// Do not timeout on calls to connect()
			// For non-blocking connect, do not timeout waiting for
			// connection to complete (e.g. DaemonCore select() loop
			// will not time out on this attempt).
		connect_state.this_try_timeout_time = 0;
	}
	connect_state.connect_failed = false;
	connect_state.failed_once = false;
	connect_state.connect_refused = false;
	connect_state.non_blocking_flag = non_blocking_flag;
	if ( connect_state.host ) free( connect_state.host );
	connect_state.host = strdup(host);
	connect_state.port = port;
	connect_state.old_timeout_value = _timeout;
	setConnectFailureReason(NULL);

	return do_connect_finish();
}

int
Sock::do_connect_finish()
{
		// NOTE: in all cases where we exit this function with
		// CEDAR_EWOULDBLOCK, we _must_ be in a state for which
		// is_connect_pending() is true.  In all other cases,
		// is_connect_pending() must be false when we return.

	while (1) {
			// There are three possible states we may be in at this point:
			// sock_bound                 - attempt to connect
			// sock_connect_pending       - connect attempt has finished
			//                              see if it succeeded or not
			// sock_connect_pending_retry - done waiting; now try again

		if( _state == sock_connect_pending_retry ) {
			_state = sock_bound;
		}

		if( _state == sock_bound ) {
			if ( do_connect_tryit() ) {
				return TRUE;
			}

			if( !connect_state.connect_failed ) {
				_state = sock_connect_pending;
			}

			if ( connect_state.non_blocking_flag &&
			     _state == sock_connect_pending )
			{
					// We expect to be called back later (e.g. by DaemonCore)
					// when the connection attempt succeeds/fails/times out

				if( IsDebugLevel( D_NETWORK ) ) {
					dprintf( D_NETWORK,
					         "non-blocking CONNECT started fd=%d dst=%s\n",
					         _sock, get_sinful_peer() );
				}
				return CEDAR_EWOULDBLOCK; 
			}
		}

			// Note, if timeout is 0, do_connect_tryit() is either
			// going to block until connect succeeds, or it will fail
			// miserably and we will no longer be in
			// sock_connect_pending state.  In all other cases, we check
			// the status of the pending connection attempt using
			// select() below.

		while ( _state == sock_connect_pending ) {
			Selector		selector;
			int				timeleft = connect_state.this_try_timeout_time - time(NULL);
			if( connect_state.non_blocking_flag ) {
				timeleft = 0;
			}
			else if( timeleft < 0 ) {
				timeleft = 0;
			}
			else if( timeleft > _timeout ) {
					// clock must have shifted
				timeleft = _timeout;
			}
			selector.reset();
			selector.set_timeout( timeleft );
			selector.add_fd( _sock, Selector::IO_WRITE );
			selector.add_fd( _sock, Selector::IO_EXCEPT );

			selector.execute();

			if( selector.timed_out() ) {
				if( !connect_state.non_blocking_flag ) {
					cancel_connect();
				}
				break; // select timed out
			}
			if( selector.signalled() ) {
				continue;
			}
			if( selector.failed() ) {
#if defined(WIN32)
				setConnectFailureErrno(WSAGetLastError(),"select");
#else
				setConnectFailureErrno(errno,"select");
#endif
				connect_state.connect_failed = true;
				connect_state.connect_refused = true; // better give up
				cancel_connect();
				break;
			}

				// Always call test_connection() first, because that
				// is the preferred way to test for errors, since it
				// reports an errno.

			if( !test_connection() ) {
					// failure reason is set by test_connection()
				_state = sock_bound;
				connect_state.connect_failed = true;
				cancel_connect();
				break; // done with select() loop
			}
			else if( selector.fd_ready(_sock,Selector::IO_EXCEPT) ) {
					// In some cases, test_connection() lies, so we
					// have to rely on select() to detect the problem.
				_state = sock_bound;
				connect_state.connect_failed = true;
				setConnectFailureReason("select() detected failure");
				cancel_connect();
				break; // done with select() loop
			}
			else {
				if ( connect_state.old_timeout_value != _timeout ) {
						// Restore old timeout
					timeout_no_timeout_multiplier(connect_state.old_timeout_value);			
				}
				return enter_connected_state();
			}
		}

		bool timed_out = connect_state.retry_timeout_time &&
		                 time(NULL) >= connect_state.retry_timeout_time;

		if( timed_out || connect_state.connect_refused) {
			if(_state != sock_bound) {
				cancel_connect();
			}
				// Always report failure, since this was the final attempt
			reportConnectionFailure(timed_out);
			return FALSE;
		}

		if( connect_state.connect_failed ) {
				// Report first failed attempt.
			if(!connect_state.failed_once) {
				connect_state.failed_once = true;

				reportConnectionFailure(timed_out);
			}
		}

		if( connect_state.non_blocking_flag &&
		    _state == sock_connect_pending )
		{
				// select() must have timed out, but the connection
				// attempt has not timed out, or we wouldn't get here,
				// so just keep waiting.
			return CEDAR_EWOULDBLOCK;
		}

		if( connect_state.non_blocking_flag ) {
				// To prevent a retry busy loop, set a retry timeout
				// of one second and return.  We will be called again
				// when this timeout expires (e.g. by DaemonCore)
			if(_state != sock_bound) {
				cancel_connect();
			}
			_state = sock_connect_pending_retry;
			connect_state.retry_wait_timeout_time = time(NULL)+1;

			if( IsDebugLevel( D_NETWORK ) ) {
				dprintf(D_NETWORK,
				        "non-blocking CONNECT  waiting for next "
				        "attempt fd=%d dst=%s\n",
				         _sock, get_sinful_peer() );
			}

			return CEDAR_EWOULDBLOCK;
		}

			// prevent busy loop in blocking-connect retries
		sleep(1);
	}

		// We _never_ get here
	EXCEPT("Impossible: Sock::do_connect_finish() broke out of while(1)");
	return FALSE;
}

bool
Sock::enter_connected_state(char const *op)
{
	_state = sock_connect;
	if( IsDebugLevel( D_NETWORK ) ) {
		dprintf( D_NETWORK, "%s bound to %s fd=%d peer=%s\n",
				 op, get_sinful(), _sock, get_sinful_peer() );
	}
		// if we are connecting to a shared port, send the id of
		// the daemon we want to be routed to
	if( !sendTargetSharedPortID() ) {
		connect_state.connect_refused = true;
		setConnectFailureReason("Failed to send shared port id.");
		return false;
	}
	return true;
}

void
Sock::setConnectFailureReason(char const *reason)
{
	if(connect_state.connect_failure_reason) {
		free(connect_state.connect_failure_reason);
		connect_state.connect_failure_reason = NULL;
	}
	if(reason) {
		connect_state.connect_failure_reason = strdup(reason);
	}
}

void
Sock::reportConnectionFailure(bool timed_out)
{
	char const *reason = connect_state.connect_failure_reason;
	char timeout_reason_buf[100];
	if((!reason || !*reason) && timed_out) {
		sprintf(timeout_reason_buf,
		        "timed out after %d seconds",
		        connect_state.retry_timeout_interval);
		reason = timeout_reason_buf;
	}
	if(!reason) {
		reason = "";
	}

	char will_keep_trying[100];
	will_keep_trying[0] = '\0';
	if(!connect_state.connect_refused && !timed_out) {
		snprintf(will_keep_trying, sizeof(will_keep_trying),
		        "  Will keep trying for %ld total seconds (%ld to go).\n",
		        (long)connect_state.retry_timeout_interval,
				(long)(connect_state.retry_timeout_time - time(NULL)));
	}

	char const *hostname = connect_state.host;
	if(!hostname) {
		hostname = "";
	}
	if(hostname[0] == '<') {
			// Suppress hostname if it is just a sinful string, because
			// the sinful address is explicitly printed as well.
		hostname = "";
	}
	dprintf( D_ALWAYS, 
	         "attempt to connect to %s%s%s failed%s%s.%s\n",
	         hostname,
			 hostname[0] ? " " : "",
	         get_sinful_peer(),
			 reason[0] ? ": " : "",
	         reason,
	         will_keep_trying);
}

void
Sock::setConnectFailureErrno(int error,char const *syscall)
{
#if defined(WIN32)
	char errmsg[150];
	char const *errdesc = "";
	if(error == WSAECONNREFUSED) {
		connect_state.connect_refused = true;
		errdesc = " connection refused";
	}
	else if(error == WSAEHOSTDOWN) {
		connect_state.connect_refused = true;
		errdesc = " host down";
	}
	else if(error == WSAEHOSTUNREACH) {
		connect_state.connect_refused = true;
		errdesc = " no route to host";
	}
	snprintf( errmsg, sizeof(errmsg), "%.15s errno = %d%.30s",
	         syscall,
	         error,
	         errdesc );
	setConnectFailureReason( errmsg );
#else
	char errmsg[150];
	if(error == ECONNREFUSED || error == EHOSTDOWN || error == EHOSTUNREACH) {
		connect_state.connect_refused = true;
	}
	snprintf( errmsg, sizeof(errmsg), "%.80s (%.15s errno = %d)",
	         strerror(error),
	         syscall,
	         error );
	setConnectFailureReason( errmsg );
#endif
}

bool Sock::do_connect_tryit()
{
		// See this function in the class definition for important notes
		// about the return states of this function.

	connect_state.connect_failed = false;
	connect_state.connect_refused = false;

		// If non-blocking, we must be certain the code in the
		// timeout() method which sets up the socket to be
		// non-blocking with the OS has happened, even if _timeout is
		// 0.  The timeout value we specify in this case is
		// irrelevant, as long as it is non-zero, because the
		// real timeout is enforced by connect_timeout_time(),
		// which is called by whoever is in charge of calling
		// connect_finish() (e.g. DaemonCore).

	if ( connect_state.non_blocking_flag ) {
		if ( timeout(1) < 0 ) {
				// failed to set socket to non-blocking
			connect_state.connect_refused = true; // better give up
			setConnectFailureReason("Failed to set timeout.");
			return false;
		}
	}


	if (condor_connect(_sock, _who) == 0) {
		if ( connect_state.non_blocking_flag ) {
			// Pretend that we haven't connected yet so that there is
			// only one code path for all non-blocking connects.
			// Otherwise, blindly calling DaemonCore::Register_Socket()
			// after initiating a non-blocking connect will fail if
			// connect() happens to complete immediately.  Why does
			// that fail?  Because DaemonCore will see that the socket
			// is in a connected state and will therefore select() on
			// reads instead of writes.

			return false;
		}

		return enter_connected_state();
	}

#if defined(WIN32)
	int lasterr = WSAGetLastError();
	if (lasterr != WSAEINPROGRESS && lasterr != WSAEWOULDBLOCK) {
		connect_state.connect_failed = true;
		setConnectFailureErrno(lasterr,"connect");

		cancel_connect();
	}
#else

		// errno can only be EINPROGRESS if timeout is > 0.
		// -Derek, Todd, Pete K. 1/19/01
	if (errno != EINPROGRESS) {
		connect_state.connect_failed = true;
		setConnectFailureErrno(errno,"connect");

			// Force close and re-creation of underlying socket.
			// See cancel_connect() for details on why.

		cancel_connect();
	}
#endif /* end of unix code */

	return false;
}

void
Sock::cancel_connect()
{
		// In some cases, we may be cancelling a non-blocking connect
		// attempt that has timed out.  In others, we may be cleaning
		// up after a failed connect attempt.  Even in the latter case
		// we need to close the underlying socket and re-create
		// it.  Why?  Because v2.2.14 of the Linux Kernel, which is 
		// used in RedHat 4.2, has a bug which will cause the machine
		// to lock up if you do repeated calls to connect() on the same
		// socket after a call to connect has failed.  The workaround
		// is if the connect() fails, close the socket.  We do this
		// procedure on all Unix platforms because we have noticed
		// strange behavior on Solaris as well when we re-use a 
		// socket after a failed connect.  -Todd 8/00
		
		// now close the underlying socket.  do not call Sock::close()
		// here, because we do not want all the CEDAR socket state
		// (like the _who data member) cleared.
	::closesocket(_sock);
	_sock = INVALID_SOCKET;
	_state = sock_virgin;
		
		// now create a new socket
	if (assign() == FALSE) {
		dprintf(D_ALWAYS,
			"assign() failed after a failed connect!\n");
		connect_state.connect_refused = true; // better give up
		return;
	}

	// finally, bind the socket
	/* TRUE means this is an outgoing connection */
	if( !bind(true) ) {
		connect_state.connect_refused = true; // better give up
	}

	if ( connect_state.old_timeout_value != _timeout ) {
			// Restore old timeout
		timeout_no_timeout_multiplier(connect_state.old_timeout_value);
	}
}

time_t Sock::connect_timeout_time()
{
		// This is called by DaemonCore or whoever is in charge of
		// calling connect_retry() when the connection attempt times
		// out or is ready to retry.

	if( _state == sock_connect_pending_retry ) {
		return connect_state.retry_wait_timeout_time;
	}
		// we are not waiting to retry, so return the connect() timeout time
	return connect_state.this_try_timeout_time;
}

time_t
Sock::get_deadline()
{
	time_t deadline = Stream::get_deadline();
	if( is_connect_pending() ) {
		time_t connect_deadline = connect_timeout_time();
		if( connect_deadline && !is_reverse_connect_pending() ) {
			if( deadline && deadline < connect_deadline ) {
				return deadline;
			}
			return connect_deadline;
		}
	}
	return deadline;
}

// Added for the HA Daemon
void Sock::doNotEnforceMinimalCONNECT_TIMEOUT()
{
	ignore_connect_timeout = TRUE;
}

bool Sock::test_connection()
{
    // Since a better way to check if a nonblocking connection has
    // succeed or not is to use getsockopt, I changed this routine
    // that way. --Sonny 7/16/2003

	int error;
    SOCKET_LENGTH_TYPE len = sizeof(error);
    if (::getsockopt(_sock, SOL_SOCKET, SO_ERROR, (char*)&error, (socklen_t*)&len) < 0) {
		connect_state.connect_failed = true;
#if defined(WIN32)
		setConnectFailureErrno(WSAGetLastError(),"getsockopt");
#else
		setConnectFailureErrno(errno,"getsockopt");
#endif
        dprintf(D_ALWAYS, "Sock::test_connection - getsockopt failed\n");
        return false;
    }
    // return result
    if (error) {
		connect_state.connect_failed = true;
		setConnectFailureErrno(error,"connect");
        return false;
    } else {
        return true;
    }
}

int Sock::close()
{
	if( _state == sock_reverse_connect_pending ) {
		cancel_reverse_connect();
	}

	if (_state == sock_virgin) return FALSE;

	if (type() == Stream::reli_sock && IsDebugLevel(D_NETWORK)) {
		dprintf( D_NETWORK, "CLOSE %s fd=%d\n", 
						sock_to_string(_sock), _sock );
	}

	if ( _sock != INVALID_SOCKET ) {
		if (::closesocket(_sock) < 0) return FALSE;
	}

	_sock = INVALID_SOCKET;
	_state = sock_virgin;
    if (connect_state.host) {
        free(connect_state.host);
    }
	connect_state.host = NULL;
	_who.clear();
    addr_changed();
	
	return TRUE;
}


#if !defined(WIN32)
#define ioctlsocket ioctl
#endif

int
Sock::bytes_available_to_read()
{
	/*	Does this platform have FIONREAD? 
		I think every platform support this, at least for network sockets.
		So for now, just fail the build if it looks bad.
		If the build does ever happen to fail here,
		we should improve this code to use autoconf to detect if FIONREAD 
		is available, and if it is not, implement this method by doing a non-blocking
		MSG_PEEK recv() call.  
	*/
#ifndef FIONREAD
#error FIONREAD is not defined!  Fix me by seeing code comment.
#endif

		/* if stream not assigned to a sock, do it now	*/
	if (_state == sock_virgin) assign();
	if ( (_state != sock_assigned) &&  
				(_state != sock_connect) &&
				(_state != sock_bound) )  {
		return -1;
	}

		/* sigh.  on win32, FIONREAD wants an ulong*, and on Unix an int*. */
#ifdef WIN32
	unsigned long num_bytes;
#else
	int num_bytes;
#endif

	if (ioctlsocket(_sock, FIONREAD, &num_bytes) < 0) {
			return -1;
	}

		/* Make certain our cast is safe to do */
	if ( num_bytes > INT_MAX ) {
		return -1;
	}
	
	int ret_val = (int) num_bytes;	// explicit cast to prevent warnings

	return ret_val;
}

bool
Sock::readReady() {
	Selector selector;

	if ( (_state != sock_assigned) &&  
		 (_state != sock_connect) &&
		 (_state != sock_bound) )  {
		return false;
	}

	if( msgReady() ) {
		return true;
	}

	selector.add_fd( _sock, Selector::IO_READ );
	selector.set_timeout( 0 );
	selector.execute();

	return selector.has_ready();
}

int
Sock::get_timeout_raw()
{
	return _timeout;
}

/* NOTE: on timeout() we return the previous timeout value, or a -1 on an error.
 * Once more: we do _not_ return FALSE on Error like most other CEDAR functions;
 * we return a -1 !! 
 */
int
Sock::timeout_no_timeout_multiplier(int sec)
{
	int t = _timeout;

	_timeout = sec;

	if (_state == sock_virgin) {
		// Rather than forcing creation of the socket here, we just
		// return success.  All paths that create the socket also
		// set the timeout.
		return t;
	}
	if ( (_state != sock_assigned) &&  
				(_state != sock_connect) &&
				(_state != sock_bound) )  {
		return -1;
	}

	if (_timeout == 0) {
		// Put socket into blocking mode
#ifdef WIN32
		unsigned long mode = 0;	// reset blocking mode
		if (ioctlsocket(_sock, FIONBIO, &mode) < 0)
			return -1;
#else
		int fcntl_flags;
		if ( (fcntl_flags=fcntl(_sock, F_GETFL)) < 0 )
			return -1;
		fcntl_flags &= ~O_NONBLOCK;	// reset blocking mode
		if ( fcntl(_sock,F_SETFL,fcntl_flags) == -1 )
			return -1;
#endif
	} else {
		// Put socket into non-blocking mode.
		// However, we never want to put a UDP socket into non-blocking mode.		
		if ( type() != safe_sock ) {	// only if socket is not UDP...
#ifdef WIN32
			unsigned long mode = 1;	// nonblocking mode
			if (ioctlsocket(_sock, FIONBIO, &mode) < 0)
				return -1;
#else
			int fcntl_flags;
			if ( (fcntl_flags=fcntl(_sock, F_GETFL)) < 0 )
				return -1;
			fcntl_flags |= O_NONBLOCK;	// set nonblocking mode
			if ( fcntl(_sock,F_SETFL,fcntl_flags) == -1 )
				return -1;
#endif
		}
	}

	return t;
}

int
Sock::timeout(int sec)
{
	bool adjusted = false;
	if ((timeout_multiplier > 0) && !ignore_timeout_multiplier) {
		sec *= timeout_multiplier;
		adjusted = true;
	}

	int t = timeout_no_timeout_multiplier( sec );

		// Adjust return value so caller can call timeout() with that value
		// to restore timeout to what it used to be.
	if( (t > 0) && adjusted ) {
		t /= timeout_multiplier;
		if( t == 0 ) {
				// Just in case t is not a multiple of timeout multiplier,
				// make sure it does not get adjusted to the special value 0.
			t = 1;
		}
	}

	return t;
}

char * Sock::serializeCryptoInfo() const
{
    const unsigned char * kserial = NULL;
    int len = 0;

    if (crypto_) {
        kserial = get_crypto_key().getKeyData();
        len = get_crypto_key().getKeyLength();
    }

	// here we want to save our state into a buffer
	char * outbuf = NULL;
    if (len > 0) {
        int buflen = len*2+32;
        outbuf = new char[buflen];
        sprintf(outbuf,"%d*%d*%d*", len*2, (int)get_crypto_key().getProtocol(),
				(int)get_encryption());

        // Hex encode the binary key
        char * ptr = outbuf + strlen(outbuf);
        for (int i=0; i < len; i++, kserial++, ptr+=2) {
            sprintf(ptr, "%02X", *kserial);
        }
    }
    else {
        outbuf = new char[2];
        memset(outbuf, 0, 2);
        sprintf(outbuf,"%d",0);
    }
	return( outbuf );
}

char * Sock::serializeMdInfo() const
{
    const unsigned char * kmd = NULL;
    int len = 0;

    if (isOutgoing_MD5_on()) {
        kmd = get_md_key().getKeyData();
        len = get_md_key().getKeyLength();
    }

	// here we want to save our state into a buffer
	char * outbuf = NULL;
    if (len > 0) {
        int buflen = len*2+32;
        outbuf = new char[buflen];
        sprintf(outbuf,"%d*", len*2);

        // Hex encode the binary key
        char * ptr = outbuf + strlen(outbuf);
        for (int i=0; i < len; i++, kmd++, ptr+=2) {
            sprintf(ptr, "%02X", *kmd);
        }
    }
    else {
        outbuf = new char[2];
        memset(outbuf, 0, 2);
        sprintf(outbuf,"%d",0);
    }
	return( outbuf );
}

char * Sock::serializeCryptoInfo(char * buf)
{
	unsigned char * kserial = NULL;
    char * ptmp = buf;
    int    len = 0, encoded_len = 0;
    int protocol = CONDOR_NO_PROTOCOL;

    // kserial may be a problem since reli_sock also has stuff after
    // it. As a result, kserial may contains not just the key, but
    // other junk from reli_sock as well. Hence the code below. Hao
    ASSERT(ptmp);

    int citems = sscanf(ptmp, "%d*", &encoded_len);
    if ( citems == 1 && encoded_len > 0 ) {
        len = encoded_len/2;
        kserial = (unsigned char *) malloc(len);
        ASSERT ( kserial )

        // skip the *
        ptmp = strchr(ptmp, '*');
        ASSERT( ptmp );
        ptmp++;

        // Reading protocol
        citems = sscanf(ptmp, "%d*", &protocol);
        ptmp = strchr(ptmp, '*');
        ASSERT( ptmp && citems == 1 );
        ptmp++;

        // read the encryption mode
        int encryption_mode = 0;
        citems = sscanf(ptmp, "%d*", &encryption_mode);
        ptmp = strchr(ptmp, '*');
        ASSERT( ptmp && citems == 1 );
        ptmp++;

        // Now, convert from Hex back to binary
        unsigned char * ptr = kserial;
        unsigned int hex;
        for(int i = 0; i < len; i++) {
            citems = sscanf(ptmp, "%2X", &hex);
			if (citems != 1) break;
            *ptr = (unsigned char)hex;
			ptmp += 2;  // since we just consumed 2 bytes of hex
			ptr++;      // since we just stored a single byte of binary
        }        

        // Initialize crypto info
        KeyInfo k((unsigned char *)kserial, len, (Protocol)protocol);
        set_crypto_key(encryption_mode==1, &k, 0);
        free(kserial);
		ASSERT( *ptmp == '*' );
        // Now, skip over this one
        ptmp++;
    }
    else {
		ptmp = strchr(ptmp, '*');
		ASSERT( ptmp );
		ptmp++;
    }
	return ptmp;
}

char * Sock::serializeMdInfo(char * buf)
{
	unsigned char * kmd = NULL;
    char * ptmp = buf;
    int    len = 0, encoded_len = 0;

    // kmd may be a problem since reli_sock also has stuff after
    // it. As a result, kmd may contains not just the key, but
    // other junk from reli_sock as well. Hence the code below. Hao
    ASSERT(ptmp);

    int citems = sscanf(ptmp, "%d*", &encoded_len);
    if ( 1 == citems && encoded_len > 0 ) {
        len = encoded_len/2;
        kmd = (unsigned char *) malloc(len);
        ASSERT( kmd );

        // skip the *
        ptmp = strchr(ptmp, '*');
        ASSERT( ptmp );
        ptmp++;

        // Now, convert from Hex back to binary
        unsigned char * ptr = kmd;
        unsigned int hex;
        for(int i = 0; i < len; i++) {
            citems = sscanf(ptmp, "%2X", &hex);
            if (citems != 1) break;
            *ptr = (unsigned char)hex;
			ptmp += 2;  // since we just consumed 2 bytes of hex
			ptr++;      // since we just stored a single byte of binary
        }        

        // Initialize crypto info
        KeyInfo k((unsigned char *)kmd, len);
        set_MD_mode(MD_ALWAYS_ON, &k, 0);
        free(kmd);
		ASSERT( *ptmp == '*' );
        // Now, skip over this one
        ptmp++;
    }
    else {
		ptmp = strchr(ptmp, '*');
		ASSERT( ptmp );
		ptmp++;
    }
	return ptmp;
}

char * Sock::serialize() const
{
	// here we want to save our state into a buffer
	size_t fqu_len = _fqu ? strlen(_fqu) : 0;

	size_t verstring_len = 0;
	char * verstring = NULL;
	CondorVersionInfo const *peer_version = get_peer_version();
	if( peer_version ) {
		verstring = peer_version->get_version_string();
		if( verstring ) {
			verstring_len = strlen(verstring);
				// daemoncore does not like spaces in our serialized string
			char *s;
			while( (s=strchr(verstring,' ')) ) {
				*s = '_';
			}
		}
	}

	char * outbuf = new char[500];
    if (outbuf) {
        memset(outbuf, 0, 500);
        sprintf(outbuf,"%u*%d*%d*%d*%lu*%lu*%s*%s*",_sock,_state,_timeout,triedAuthentication(),(unsigned long)fqu_len,(unsigned long)verstring_len,_fqu ? _fqu : "",verstring ? verstring : "");
    }
    else {
        dprintf(D_ALWAYS, "Out of memory!\n");
    }
	free( verstring );
	return( outbuf );
}

void
Sock::close_serialized_socket(char const *buf)
{
		// grab the fd from the serialized string and close it
	SOCKET passed_sock;
	int i;
	i = sscanf(buf,"%u*",&passed_sock);
	ASSERT( i == 1 );
	::close(passed_sock);
}

char * Sock::serialize(char *buf)
{
	int i;
	SOCKET passed_sock;
	size_t fqulen = 0;
	size_t verstring_len = 0;
	int pos;
	int tried_authentication = 0;

	ASSERT(buf);

	// here we want to restore our state from the incoming buffer
	i = sscanf(buf,"%u*%d*%d*%d*%lu*%lu*%n",&passed_sock,(int*)&_state,&_timeout,&tried_authentication,(unsigned long *)&fqulen,(unsigned long *)&verstring_len,&pos);
	if (i!=6) {
		EXCEPT("Failed to parse serialized socket information (%d,%d): '%s'\n",i,pos,buf);
	}
	buf += pos;

	setTriedAuthentication(tried_authentication);

	char *fqubuf = (char *)malloc(fqulen+1);
	ASSERT(fqubuf);
	memset(fqubuf,0,fqulen+1);
	strncpy(fqubuf,buf,fqulen);
	setFullyQualifiedUser(fqubuf);
	free(fqubuf);
	buf += fqulen;
	if( *buf != '*' ) {
		EXCEPT("Failed to parse serialized socket fqu (%lu): '%s'\n",(unsigned long)fqulen,buf);
	}
	buf++;

	char *verstring = (char *)malloc(verstring_len+1);
	ASSERT(verstring);
	memset(verstring,0,verstring_len+1);
	strncpy(verstring,buf,verstring_len);
	verstring[verstring_len] = 0;
	if( verstring_len ) {
			// daemoncore does not like spaces in our serialized string
		char *s;
		while( (s=strchr(verstring,'_')) ) {
			*s = ' ';
		}
		CondorVersionInfo peer_version(verstring);
		set_peer_version( &peer_version );
	}
	free( verstring );
	buf += verstring_len;
	if( *buf != '*' ) {
		EXCEPT("Failed to parse serialized peer version string (%lu): '%s'\n",(unsigned long)verstring_len,buf);
	}
	buf++;

	// replace _sock with the one from the buffer _only_ if _sock
	// is currently invalid.  if it is not invalid, it has already
	// been initialized (probably via the Sock copy constructor) and
	// therefore we should _not mess with it_.
	// On unix, if the inherited fd is larger than our fd limit, then
	// dup() it to a lower fd. Otherwise, our Selector class won't
	// handle it. This can happen if our parent has a larger fd limit
	// than us.
	if ( _sock == INVALID_SOCKET ) {
#if !defined(WIN32)
		if ( passed_sock < Selector::fd_select_size() ) {
			_sock = passed_sock;
		} else {
			_sock = dup( passed_sock );
			if ( _sock < 0 ) {
				EXCEPT( "Sock::serialize(): Dup'ing of high fd %d failed, "
						"errno=%d (%s)", passed_sock, errno,
						strerror( errno ) );
			} else if ( _sock >= Selector::fd_select_size() ) {
				EXCEPT( "Sock::serialize(): Dup'ing of high fd %d resulted "
						"in new high fd %d", passed_sock, _sock );
			}
			::close( passed_sock );
		}
#else
		_sock = passed_sock;
#endif
	}

	// call the timeout method to make certain socket state set via
	// setsockopt() and/or ioctl() is restored.
	timeout_no_timeout_multiplier(_timeout);

	return buf;
}

void
Sock::addr_changed()
{
    // these are all regenerated whenever they are needed, so when
    // either the peer's address or our address change, zap them all
    _my_ip_buf[0] = '\0';
    _peer_ip_buf[0] = '\0';
    _sinful_self_buf.clear();
    _sinful_public_buf.clear();
    _sinful_peer_buf[0] = '\0';
}

condor_sockaddr
Sock::peer_addr()
{
	return _who;
}


int
Sock::peer_port()
{
		//return (int) ntohs( _who.sin_port );
	return (int)(_who.get_port());
}


// [OBSOLETE]
/*
unsigned int
Sock::peer_ip_int()
{
	return (unsigned int) ntohl( _who.sin_addr.s_addr );
}
*/


const char *
Sock::peer_ip_str()
{
	if (!_peer_ip_buf[0]) {
		MyString peer_ip = _who.to_ip_string();
		strcpy(_peer_ip_buf, peer_ip.Value());
	}
	return _peer_ip_buf;
		/*
    if( _peer_ip_buf[0] ) {
        return _peer_ip_buf;
    }
    strncpy( _peer_ip_buf, inet_ntoa(_who.sin_addr), IP_STRING_BUF_SIZE );
    _peer_ip_buf[IP_STRING_BUF_SIZE-1] = '\0';
	return _peer_ip_buf;
		*/
}

// is peer a local interface, aka did this connection originate from a local process?
// return true if peer address corresponds to an interface local to this machine,
// or false if not or if an error.
bool 
Sock::peer_is_local()
{
		// peer_is_local is called rarely and by few call sites.
		// making hashtable for both ipv4 and ipv6 addresses does seem to
		// be worth implementation.

	if (!peer_addr().is_valid())
		return false;

	bool result;
	condor_sockaddr addr = peer_addr();
		// ... but use any old ephemeral port.
	addr.set_port(0);
	int sock = ::socket(addr.get_aftype(), SOCK_DGRAM, IPPROTO_UDP);
		// invoke OS bind, not cedar bind - cedar bind does not allow us
		// to specify the local address.
	if (condor_bind(sock, addr) < 0) {
		// failed to bind.  assume we failed  because the peer address is
		// not local.
		result = false;
	} else {
		// bind worked, assume address has a local interface.
		result = true;
	}
	// must not forget to close the socket we just created!
	::closesocket(sock);
	return result;
	
		/*

	// Keep a static cache of results, since determining if the peer is local
	// is somewhat expensive. Flush the cache every 20 min to deal w/
	// interfaces on the machine being activated/deactivated during runtime.
	static HashTable<int,bool>* isLocalTable = NULL;
	static time_t cache_ttl = 0;
	
	bool result;
	int peer_int = peer_ip_int();

	// if there is no peer, bail out now.
	if ( peer_int == 0 ) {
		return false;
	}

	// allocate hashtable dynamically first time we are called, so
	// we don't bloat private address space for daemons that never
	// invoke this method.
	if ( !isLocalTable ) {
		isLocalTable = new HashTable<int,bool>(hashFuncInt);
	}

	// check the time to live (ttl) on our cached data
	time_t now = time(NULL);
	if ( now >= cache_ttl ) {
		// ttl has expired; flush the cache and reset ttl
		isLocalTable->clear();
		cache_ttl = now + (20 * 60);	// push ttl 20 min into the future
	}

	// see if our cache already has the answer
	if (isLocalTable->lookup(peer_int,result) == 0) {
		// found the answer is in our cache table, just return what we found.
		return result;
	} 

	// if we made it here, we don't have a cached answer, so 
	// we run an experiment to see if the peer address relates
	// to one of our interfaces.  the experiment is simply try 
	// and bind a socket to the peer address on an ephemeral port - 
	// if it works, we must have a local interface w/ that address,
	// if it fails, assume we don't. 
	// note: this algorithm may be imperfect if the host is serving
	// as a NAT gateway.

	int sock = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if ( sock < 0 ) {
		dprintf(D_ALWAYS,"Sock::peer_is_local(): ERROR failed to create socket\n");
        return false;
    }
		// Bind to the *same address* as our peer ....
		//struct sockaddr_in mySockAddr = *( peer_addr() );
	sockaddr_in mySockAddr = peer_addr().to_sin();
		// ... but use any old ephemeral port.
    mySockAddr.sin_port = htons( 0 );
		// invoke OS bind, not cedar bind - cedar bind does not allow us
		// to specify the local address.
	if ( ::bind(sock, (SOCKET_ADDR_CONST_BIND SOCKET_ADDR_TYPE) &mySockAddr, 
		        sizeof(mySockAddr)) < 0 ) 
	{
		// failed to bind.  assume we failed  because the peer address is
		// not local.
		result = false;        
	} else {
		// bind worked, assume address has a local interface.
		result = true;
	}
	// must not forget to close the socket we just created!
	::closesocket(sock);
	
	// Stash our result in the cache.
	isLocalTable->insert(peer_int,result);

	// return the result to the caller
	return result;
		*/
}

condor_sockaddr
Sock::my_addr() 
{
	condor_sockaddr addr;
	condor_getsockname_ex(_sock, addr);
	return addr;
}

const char *
Sock::my_ip_str()
{
	if (!_my_ip_buf[0]) {
		MyString ip_str = my_addr().to_ip_string();
		strcpy(_my_ip_buf, ip_str.Value());
	}
	return _my_ip_buf;
}

char const *
Sock::get_sinful()
{
    if( _sinful_self_buf.empty() ) {
		condor_sockaddr addr;
		int ret = condor_getsockname_ex(_sock, addr);
		if (ret == 0) {
			_sinful_self_buf = addr.to_sinful();

			std::string alias;
			if( param(alias,"HOST_ALIAS") ) {
				Sinful s(_sinful_self_buf.c_str());
				s.setAlias(alias.c_str());
				_sinful_self_buf = s.getSinful();
			}

		}
	}
	return _sinful_self_buf.c_str();
}

char *
Sock::get_sinful_peer()
{       
	if ( !_sinful_peer_buf[0] ) {
		MyString sinful_peer = _who.to_sinful();
		strcpy(_sinful_peer_buf, sinful_peer.Value());
	}
	return _sinful_peer_buf;
}

char const *
Sock::default_peer_description()
{
	char const *retval = get_sinful_peer();
	if( !retval ) {
		return "(unconnected socket)";
	}
	return retval;
}

int
Sock::get_port()
{
	condor_sockaddr addr;
	if (condor_getsockname(_sock, addr) < 0)
		return -1;
	return addr.get_port();
}

#if !defined(WIN32)

/*
These arrays form the asynchronous handler table.  The number of entries is stored in "table_size".  Each entry in "handler_table" points to the asynchronous handler registered for each fd.  Each entry in "stream_table" gives the Stream object associated with each fd.  When a SIGIO comes in, we will consult this table to find the correct handler.
*/

static CedarHandler ** handler_table  = 0;
static Stream ** stream_table = 0;
static int table_size = 0;

/*
This function is invoked whenever a SIGIO arrives.  When this happens, we don't even know what fd is begging for attention, so we must use select() (or poll()) to figure this out.  With the fd in hand, we can look up the appropriate Stream and Handler in the table above.  Each Stream that is active will have its handler invoked.
*/

static void async_handler( int )
{
	Selector selector;
	int i;

	selector.set_timeout( 0 );

	for( i=0; i<table_size; i++ ) {
		if( handler_table[i] ) {
			selector.add_fd( i, Selector::IO_READ );
		}
	}

	selector.execute();

	if( selector.has_ready() ) {
		for( i=0; i<table_size; i++ ) {
			if( selector.fd_ready( i, Selector::IO_READ ) ) {
				handler_table[i](stream_table[i]);
			}
		}
	}
}

/*
Set this fd up for asynchronous operation.  There are many ways of accomplishing this.  
Some systems require multiple calls, some systems only support a few of these calls, 
and some support multiple, but only require one.  
On top of that, many calls with defined constants are known to fail.  
So, we will throw all of our knives at once and ignore return values.
*/

static void make_fd_async( int fd )
{
	int bits;
	int pid = getpid();

	/* Make the owner of this fd be this process */

	#if defined(FIOSSAIOOWN)
		ioctl( fd, FIOSSAIOOWN, &pid );
	#endif

	#if defined(F_SETOWN)
		fcntl( fd, F_SETOWN, pid);
	#endif

	/* make the fd asynchronous -- signal when ready */

	#if defined(O_ASYNC)
		bits = fcntl( fd, F_GETFL, 0 );
		fcntl( fd, F_SETFL, bits | O_ASYNC );
	#endif

	#if defined(FASYNC)
		bits = fcntl( fd, F_GETFL, 0 );
		fcntl( fd, F_SETFL, bits | FASYNC );
	#endif

	#if defined(FIOASYNC) && !defined(linux)
		{
		/* In some versions of linux, FIOASYNC results in
		   _synchronous_ I/O.  Bug!  Fortunately, FASYNC
		   is defined in these cases. */
			int on = 1;
			ioctl( fd, FIOASYNC, &on );
		}
    #endif

	#if defined(FIOSSAIOSTAT)
		{
			int on = 1; 
			ioctl( fd, FIOSSAIOSTAT, &on );
		}
	#endif
}

/* change this fd back to synchronous mode */

static void  make_fd_sync( int fd )
{
	int bits;

	bits = fcntl( fd, F_GETFL, 0 );

	#if defined(O_ASYNC)
		bits = bits & ~O_ASYNC;
	#endif

	#if defined(FASYNC)
		bits = bits & ~FASYNC;
	#endif

	fcntl( fd, F_SETFL, bits );
}

/*
This function adds a new entry to the handler table and marks the fd as 
asynchronous.  If the table does not exist yet, it is allocated and the 
async_handler is installed as the handler for SIGIO.  
If "handler" is null, then async notification is disabled for that fd.
*/

static int install_async_handler( int fd, CedarHandler *handler, Stream *stream )
{
	int i;
	struct sigaction act;

	if( !handler_table ) {
		table_size = sysconf(_SC_OPEN_MAX);
		if(table_size<=0) return 0;

		handler_table = (CedarHandler **) malloc( sizeof(handler) * table_size );
		if(!handler_table) return 0;

		stream_table = (Stream **) malloc( sizeof(stream) * table_size );
		if(!stream_table) return 0;

		for( i=0; i<table_size; i++ ) {
			handler_table[i] = 0;
			stream_table[i] = 0;
		}

		act.sa_handler = async_handler;
		sigfillset(&act.sa_mask);
		act.sa_flags = 0;

		sigaction( SIGIO, &act, 0 );
	}

	handler_table[fd] = handler;
	stream_table[fd] = stream;

	if(handler) {
		make_fd_async(fd);
	} else {
		make_fd_sync(fd);
	}

	return 1;
}

/*
Install the given handler for this stream.
*/

int Sock::set_async_handler( CedarHandler *handler )
{
	return install_async_handler( _sock, handler, this );
}

#endif  /* of ifndef WIN32 for the async support */


void Sock :: setAuthenticationMethodUsed(char const *auth_method)
{
	if( _auth_method ) {
		free (_auth_method);
	}
	_auth_method = strdup(auth_method);
}

const char* Sock :: getAuthenticationMethodUsed() {
	return _auth_method;
}

void Sock :: setAuthenticationMethodsTried(char const *auth_methods)
{
	free(_auth_methods);
	_auth_methods = strdup(auth_methods);
}

const char* Sock :: getAuthenticationMethodsTried() {
	return _auth_methods;
}

void Sock :: setAuthenticatedName(char const *auth_name)
{
	free(_auth_name);
	_auth_name = strdup(auth_name);
}

const char* Sock :: getAuthenticatedName() {
	return _auth_name;
}

void Sock :: setCryptoMethodUsed(char const *crypto_method)
{
	if( _crypto_method ) {
		free (_crypto_method);
	}
	_crypto_method = strdup(crypto_method);
}

const char* Sock :: getCryptoMethodUsed() {
	return _crypto_method;
}



void Sock :: setFullyQualifiedUser(char const *fqu)
{
	if( fqu == _fqu ) { // special case
		return;
	}
	if( fqu && fqu[0] == '\0' ) {
			// treat empty string identically to NULL to avoid subtlties
		fqu = NULL;
	}
	if( _fqu ) {
		free( _fqu );
		_fqu = NULL;
	}
	if (_fqu_user_part) {
		free(_fqu_user_part);
		_fqu_user_part = NULL;
	}
	if (_fqu_domain_part) {
		free(_fqu_domain_part);
		_fqu_domain_part = NULL;
	}
	if( fqu ) {
		_fqu = strdup(fqu);
		Authentication::split_canonical_name(_fqu,&_fqu_user_part,&_fqu_domain_part);
	}
}

int Sock :: encrypt(bool)
{
	return -1;
}

int Sock :: hdr_encrypt()
{
	return -1;
}

bool Sock :: is_hdr_encrypt(){
	return FALSE;
}

int Sock :: authenticate(KeyInfo *&, const char * /* methods */, CondorError* /* errstack */, int /*timeout*/, char ** /*method_used*/)
{
	return -1;
}

int Sock :: authenticate(const char * /* methods */, CondorError* /* errstack */, int /*timeout*/)
{
	/*
	errstack->push("AUTHENTICATE", AUTHENTICATE_ERR_NOT_BUILT,
			"Failure: This version of condor was not compiled with authentication enabled");
	*/
	return -1;
}

bool Sock :: is_encrypt()
{
    return FALSE;
}


int
Sock::_bind_helper(int fd, const condor_sockaddr& addr, bool outbound, bool loopback)
{
	int rval;

	if (outbound) {} // To remove unused variable warning
	if (loopback) {} // To remove unused variable warning
		//rval = ::bind(fd, (SOCKET_ADDR_CONST_BIND SOCKET_ADDR_TYPE)addr, len);
	rval = condor_bind(fd, addr);
	return rval;
}

void
Sock::set_connect_addr(char const *addr)
{
	free( m_connect_addr );
	m_connect_addr = NULL;
	if( addr ) {
		m_connect_addr = strdup(addr);
	}
}

char const *
Sock::get_connect_addr()
{
	return m_connect_addr;
}

const char *
Sock::getFullyQualifiedUser() const {
	return _fqu ? _fqu : UNAUTHENTICATED_FQU;
}

const char *
Sock::getOwner() const {
	return _fqu_user_part ? _fqu_user_part : UNAUTHENTICATED_USER;
}

		/// Get domain portion of fqu
const char *
Sock::getDomain() const {
	return _fqu_domain_part ? _fqu_domain_part : UNMAPPED_DOMAIN;
}


bool
Sock::isMappedFQU() const
{
	if( !_fqu_domain_part ) {
		return false;
	}
	return strcmp(_fqu_domain_part,UNMAPPED_DOMAIN) != 0;
}

bool
Sock::isAuthenticated() const
{
	if( !_fqu ) {
		return false;
	}
	return strcmp(_fqu,UNAUTHENTICATED_FQU) != 0;
}

bool 
Sock::wrap(unsigned char* d_in,int l_in, 
                    unsigned char*& d_out,int& l_out)
{    
    bool coded = false;
#ifdef HAVE_EXT_OPENSSL
    if (get_encryption()) {
        coded = crypto_->encrypt(d_in, l_in, d_out, l_out);
    }
#endif
    return coded;
}

bool 
Sock::unwrap(unsigned char* d_in,int l_in,
                      unsigned char*& d_out, int& l_out)
{
    bool coded = false;
#ifdef HAVE_EXT_OPENSSL
    if (get_encryption()) {
        coded = crypto_->decrypt(d_in, l_in, d_out, l_out);
    }
#endif
    return coded;
}

void Sock::resetCrypto()
{
#ifdef HAVE_EXT_OPENSSL
  if (crypto_) {
    crypto_->resetState();
  }
#endif
}

bool 
Sock::initialize_crypto(KeyInfo * key) 
{
    delete crypto_;
    crypto_ = 0;
	crypto_mode_ = false;

    // Will try to do a throw/catch later on
    if (key) {
        switch (key->getProtocol()) 
        {
#ifdef HAVE_EXT_OPENSSL
        case CONDOR_BLOWFISH :
			setCryptoMethodUsed("BLOWFISH");
            crypto_ = new Condor_Crypt_Blowfish(*key);
            break;
        case CONDOR_3DES:
			setCryptoMethodUsed("3DES");
            crypto_ = new Condor_Crypt_3des(*key);
            break;
#endif
        default:
            break;
        }
    }

    return (crypto_ != 0);
}

bool Sock::set_MD_mode(CONDOR_MD_MODE mode, KeyInfo * key, const char * keyId)
{
    mdMode_ = mode;
    delete mdKey_;
    mdKey_ = 0;
    if (key) {
      mdKey_  = new KeyInfo(*key);
    }

    return init_MD(mode, mdKey_, keyId);
}

const KeyInfo& Sock :: get_crypto_key() const
{
#ifdef HAVE_EXT_OPENSSL
    if (crypto_) {
        return crypto_->get_key();
    }
#endif
    ASSERT(0);	// This does not return...
	return  crypto_->get_key();  // just to make compiler happy...
}

const KeyInfo& Sock :: get_md_key() const
{
#ifdef HAVE_EXT_OPENSSL
    if (mdKey_) {
        return *mdKey_;
    }
#endif
    ASSERT(0);
    return *mdKey_;
}


bool 
Sock::set_crypto_key(bool enable, KeyInfo * key, const char * keyId)
{
    bool inited = true;
#ifdef HAVE_EXT_OPENSSL

    if (key != 0) {
        inited = initialize_crypto(key);
    }
    else {
        // We are turning encryption off
        if (crypto_) {
            delete crypto_;
            crypto_ = 0;
			crypto_mode_ = false;
        }
        ASSERT(keyId == 0);
        ASSERT(enable == false);
        inited = true;
    }

    // More check should be done here. what if keyId is NULL?
    if (inited) {
		if( enable ) {
				// We do not set the encryption id if the default crypto
				// mode is off, because setting the encryption id causes
				// the UDP packet header to contain the encryption id,
				// which causes a pre 7.1.3 receiver to think that encryption
				// is turned on by default, even if that is not what was
				// previously negotiated.
			set_encryption_id(keyId);
		}
		set_crypto_mode(enable);
    }

#endif /* HAVE_EXT_OPENSSL */

    return inited;
}

bool
Sock::canEncrypt()
{
	return crypto_ != NULL;
}

void
Sock::invalidateSock()
{
	_sock = INVALID_SOCKET; 
}
