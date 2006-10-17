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
#include "condor_constants.h"
#include "condor_io.h"
#include "condor_uid.h"
#include "sock.h"
#include "condor_network.h"
#include "internet.h"
#include "my_hostname.h"
#include "condor_debug.h"
#include "condor_socket_types.h"
#include "get_port_range.h"
#include "condor_netdb.h"
#include "daemon_core_sock_adapter.h"

#if !defined(WIN32)
#define closesocket close
#endif

// initialize static data members
int Sock::timeout_multiplier = 0;

DaemonCoreSockAdapterClass daemonCoreSockAdapter;

Sock::Sock() : Stream() {
	_sock = INVALID_SOCKET;
	_state = sock_virgin;
	_timeout = 0;
	ignore_connect_timeout = FALSE;		// Used by the HA Daemon
	connect_state.host = NULL;
	connect_state.connect_failure_reason = NULL;
	memset(&_who, 0, sizeof(struct sockaddr_in));
	memset(&_endpoint_ip_buf, 0, IP_STRING_BUF_SIZE);
}

Sock::Sock(const Sock & orig) : Stream() {

	// initialize everything in the new sock
	_sock = INVALID_SOCKET;
	_state = sock_virgin;
	_timeout = 0;
	connect_state.host = NULL;
	connect_state.connect_failure_reason = NULL;
	memset( &_who, 0, sizeof( struct sockaddr_in ) );
	memset(	&_endpoint_ip_buf, 0, IP_STRING_BUF_SIZE );

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
	if ( connect_state.host ) free(connect_state.host);
	if ( connect_state.connect_failure_reason) {
		free(connect_state.connect_failure_reason);
	}
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
	char		*my_prot;

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

	_sock = WSASocket(FROM_PROTOCOL_INFO, FROM_PROTOCOL_INFO, 
					  FROM_PROTOCOL_INFO, pProtoInfo, 0, 0);

	if ( _sock == INVALID_SOCKET )
		return FALSE;

	_state = sock_assigned;
	return TRUE;
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
	 * calls to fopen() will start to fail on Solaris.  In Condor, this 
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
	int		my_type;

	if (_state != sock_virgin) return FALSE;

	if (sockd != INVALID_SOCKET){
		_sock = sockd;		/* Could we check for correct protocol ? */
		_state = sock_assigned;
		return TRUE;
	}

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
	if ((_sock = socket(AF_INET, my_type, 0)) == INVALID_SOCKET) {
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
		timeout( _timeout );

	return TRUE;
}


int Sock::bindWithin(const int low_port, const int high_port)
{
	bool bind_all = (bool)_condor_bind_all_interfaces();

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
		sockaddr_in		sin;
		priv_state old_priv;
		int bind_return_val;

		memset(&sin, 0, sizeof(sockaddr_in));
		sin.sin_family = AF_INET;
		if( bind_all ) {
			sin.sin_addr.s_addr = INADDR_ANY;
		} else {
			sin.sin_addr.s_addr = htonl(my_ip_addr());
		}
		sin.sin_port = htons((u_short)this_trial++);

#ifndef WIN32
		if (this_trial <= 1024) {
			// use root priv for the call to bind to allow privileged ports
			old_priv = PRIV_UNKNOWN;
			old_priv = set_root_priv();
		}
#endif
		bind_return_val = ::bind(_sock, (sockaddr *)&sin, sizeof(sockaddr_in));
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


int Sock::bind(int is_outgoing, int port)
{
	sockaddr_in		sin;
	priv_state old_priv;
	int bind_return_value;

	// Following lines are added because some functions in condor call
	// this method without checking the port numbers returned from
	// such as 'getportbyserv'
	if (port < 0) return FALSE;

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
	// this is why there is an is_outgoing flag passed in.  when this is true,
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
	if ( port == 0 && get_port_range(is_outgoing, &lowPort, &highPort) == TRUE ) {
			// Bind in a specific port range.
		if ( bindWithin(lowPort, highPort) != TRUE ) {
			return FALSE;
		}
	} else {
			// Bind to a dynamic port.
		memset(&sin, 0, sizeof(sockaddr_in));
		sin.sin_family = AF_INET;
		bool bind_all = (bool)_condor_bind_all_interfaces();
		if( bind_all ) {
			sin.sin_addr.s_addr = INADDR_ANY;
		} else {
			sin.sin_addr.s_addr = htonl(my_ip_addr());
		}
		sin.sin_port = htons((u_short)port);

#ifndef WIN32
		if(port < 1024) {
			// use root priv for the call to bind to allow privileged ports
			old_priv = PRIV_UNKNOWN;
			old_priv = set_root_priv();
		}
#endif
		bind_return_value = ::bind(_sock, (sockaddr *)&sin, sizeof(sockaddr_in));
#ifndef WIN32
		if(port < 1024) {
			set_priv (old_priv);
		}
#endif
		if ( bind_return_value < 0) {
	#ifdef WIN32
			int error = WSAGetLastError();
			dprintf( D_ALWAYS, "bind failed: WSAError = %d\n", error );
	#else
			dprintf(D_NETWORK, "bind failed errno = %d\n", errno);
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

int Sock::set_os_buffers(int desired_size, bool set_write_buf)
{
	int current_size = 0;
	int previous_size = 0;
	int attempt_size = 0;
	int command;
	int ret_val;
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
			(char*)&current_size,&temp);
	dprintf(D_FULLDEBUG,"Current Socket bufsize=%dk\n",
		current_size / 1024);
	current_size = 0;

	/* 
		We want to set the socket buffer size to be as close
		to the desired size as possible.  Unfortunatly, there is no
		contant defined which states the maximum size possible.  So
		we keep raising it up 2k at a time until (a) we got up to the
		desired value, or (b) it is not increasing anymore.  We ignore
		errors (-1) values from setsockopt since on some platforms this 
		could signal a value which is to low...
	*/
	 
	do {
		attempt_size += 2000;
		if ( attempt_size > desired_size ) {
			attempt_size = desired_size;
		}
		ret_val = setsockopt(SOL_SOCKET,command,
			(char*)&attempt_size,sizeof(int));
		if ( ret_val < 0 )
			continue;
		previous_size = current_size;
		temp = sizeof(int);
		::getsockopt(_sock,SOL_SOCKET,command,
			(char*)&current_size,&temp);
	} while ( ((ret_val < 0) || (previous_size < current_size))
				&& (attempt_size < desired_size) );

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


int Sock::do_connect(
	char	*host,
	int		port,
	bool	non_blocking_flag
	)
{
	hostent		*hostp;
	unsigned long	inaddr;

	if (!host || port < 0) return FALSE;

		/* we bind here so that a sock may be	*/
		/* assigned to the stream if needed		*/
		/* TRUE means this is an outgoing connection */
	if (_state == sock_virgin || _state == sock_assigned) bind( TRUE );

	if (_state != sock_bound) return FALSE;

	memset(&_who, 0, sizeof(sockaddr_in));
	_who.sin_family = AF_INET;
	_who.sin_port = htons((u_short)port);

	/* might be in <x.x.x.x:x> notation				*/
	if (host[0] == '<') {
		string_to_sin(host, &_who);
	}
	/* try to get a decimal notation 	 			*/
	else if ((inaddr = inet_addr(host)) != (unsigned int)-1){
		memcpy((char *)&_who.sin_addr, &inaddr, sizeof(inaddr));
	}
	/* if dotted notation fails, try host database	*/
	else{
		if ((hostp = condor_gethostbyname(host)) == (hostent *)0) return FALSE;
		memcpy(&_who.sin_addr, hostp->h_addr, hostp->h_length);
	}

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
				_state = sock_connect;
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

				if( DebugFlags & D_NETWORK ) {
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
			struct timeval	timer;
			fd_set			writefds;
			fd_set			exceptfds;
			int				nfds=0, nfound;
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
			timer.tv_sec = timeleft;
			timer.tv_usec = 0;
#if !defined(WIN32) // nfds is ignored on WIN32
			nfds = _sock + 1;
#endif
			FD_ZERO( &writefds );
			FD_SET( _sock, &writefds );
			FD_ZERO( &exceptfds );
			FD_SET( _sock, &exceptfds );

			nfound = ::select( nfds, 0, &writefds, &exceptfds, &timer );

			if( nfound == 0 ) {
				if( !connect_state.non_blocking_flag ) {
					cancel_connect();
				}
				break; // select timed out
			}
			if( nfound < 0 ) {
#if !defined(WIN32)
				if(errno == EINTR) {
					continue; // select() was interrupted by a signal
				}
#endif
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
			else if( FD_ISSET(_sock,&exceptfds) ) {
					// In some cases, test_connection() lies, so we
					// have to rely on select() to detect the problem.
				_state = sock_bound;
				connect_state.connect_failed = true;
				setConnectFailureReason("select() detected failure");
				cancel_connect();
				break; // done with select() loop
			}
			else {
				_state = sock_connect;
				if( DebugFlags & D_NETWORK ) {
					dprintf( D_NETWORK, "CONNECT src=%s fd=%d dst=%s\n",
							 get_sinful(), _sock, get_sinful_peer() );
				}
				if ( connect_state.old_timeout_value != _timeout ) {
						// Restore old timeout
					timeout(connect_state.old_timeout_value);			
				}
				return true;
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

		if( connect_state.non_blocking_flag ) {
				// To prevent a retry busy loop, set a retry timeout
				// of one second and return.  We will be called again
				// when this timeout expires (e.g. by DaemonCore)
			_state = sock_connect_pending_retry;
			connect_state.retry_wait_timeout_time = time(NULL)+1;

			if( DebugFlags & D_NETWORK ) {
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
	snprintf( errmsg, sizeof(errmsg), "%.15s errno = %d%.30s",
	         syscall,
	         error,
	         errdesc );
	setConnectFailureReason( errmsg );
#else
	char errmsg[150];
	if(error == ECONNREFUSED) {
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


	if (::connect(_sock, (sockaddr *)&_who, sizeof(sockaddr_in)) == 0) {
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

		_state = sock_connect;
		if( DebugFlags & D_NETWORK ) {
			dprintf( D_NETWORK, "CONNECT src=%s fd=%d dst=%s\n",
					 get_sinful(), _sock, get_sinful_peer() );
		}
		return true;
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
#if defined(WIN32)
	_state = sock_bound;
#else
		// Here we need to close the underlying socket and re-create
		// it.  Why?  Because v2.2.14 of the Linux Kernel, which is 
		// used in RedHat 4.2, has a bug which will cause the machine
		// to lock up if you do repeated calls to connect() on the same
		// socket after a call to connect has failed.  The workaround
		// is if the connect() fails, close the socket.  We do this
		// procedure on all Unix platforms because we have noticed
		// strange behavior on Solaris as well when we re-use a 
		// socket after a failed connect.  -Todd 8/00
		
		// stash away the descriptor so we can compare later..
	int old_sock = _sock;

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

		// make certain our descriptor number has not changed,
		// because parts of Condor may have stashed the old
		// socket descriptor into data structures.  So if it has
		// changed, use dup2() to set it the same as before.
	if ( _sock != old_sock ) {
		if ( dup2(_sock,old_sock) < 0 ) {
			dprintf(D_ALWAYS,
				"dup2 failed after a failed connect! errno=%d\n", 
				errno);
			connect_state.connect_refused = true; // better give up
			return;
		}
		::closesocket(_sock);
		_sock = old_sock;
	}

	// finally, bind the socket
	/* TRUE means this is an outgoing connection */
	if( !bind( TRUE ) ) {
		connect_state.connect_refused = true; // better give up
	}
#endif
	if ( connect_state.old_timeout_value != _timeout ) {
			// Restore old timeout
		timeout(connect_state.old_timeout_value);			
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
    if (::getsockopt(_sock, SOL_SOCKET, SO_ERROR, (char*)&error, &len) < 0) {
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
	if (_state == sock_virgin) return FALSE;

	if (type() == Stream::reli_sock) {
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
	memset(&_who, 0, sizeof( struct sockaddr_in ) );
	memset(&_endpoint_ip_buf, 0, IP_STRING_BUF_SIZE );
	
	return TRUE;
}


#if !defined(WIN32)
#define ioctlsocket ioctl
#endif

int
Sock::set_timeout_multiplier(int secs)
{
   int old_val = timeout_multiplier;
   timeout_multiplier = secs;
   return old_val;
}

/* NOTE: on timeout() we return the previous timeout value, or a -1 on an error.
 * Once more: we do _not_ return FALSE on Error like most other CEDAR functions;
 * we return a -1 !! 
 */
int Sock::timeout(int sec)
{
	int t = _timeout;

	if (sec && (timeout_multiplier > 0)) {
		sec *= timeout_multiplier;
	}

	_timeout = sec;

	/* if stream not assigned to a sock, do it now	*/
	if (_state == sock_virgin) assign();
	if ( (_state != sock_assigned) &&  
				(_state != sock_connect) &&
				(_state != sock_bound) )  {
		return -1;
	}

	if (_timeout == 0) {
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

	return t;
}

char * Sock::serializeCryptoInfo() const
{
    const unsigned char * kserial = NULL;
    int len = 0;

    if (get_encryption()) {
        kserial = get_crypto_key().getKeyData();
        len = get_crypto_key().getKeyLength();
    }

	// here we want to save our state into a buffer
	char * outbuf = NULL;
    if (len > 0) {
        int buflen = len*2+32;
        outbuf = new char[buflen];
        sprintf(outbuf,"%d*%d*", len*2, (int)get_crypto_key().getProtocol());

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

    sscanf(ptmp, "%d*", &encoded_len);
    if ( encoded_len > 0 ) {
        len = encoded_len/2;
        kserial = (unsigned char *) malloc(len);

        // skip the *
        ptmp = strchr(ptmp, '*');
		ASSERT( ptmp );
        ptmp++;

        // Reading protocol
        sscanf(ptmp, "%d*", &protocol);
        ptmp = strchr(ptmp, '*');
		ASSERT( ptmp );
        ptmp++;

        // Now, convert from Hex back to binary
        unsigned char * ptr = kserial;
        unsigned int hex;
        for(int i = 0; i < len; i++) {
            sscanf(ptmp, "%2X", &hex);
            *ptr = (unsigned char)hex;
			ptmp += 2;  // since we just consumed 2 bytes of hex
			ptr++;      // since we just stored a single byte of binary
        }        

        // Initialize crypto info
        KeyInfo k((unsigned char *)kserial, len, (Protocol)protocol);
        set_crypto_key(true, &k, 0);
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

    sscanf(ptmp, "%d*", &encoded_len);
    if ( encoded_len > 0 ) {
        len = encoded_len/2;
        kmd = (unsigned char *) malloc(len);

        // skip the *
        ptmp = strchr(ptmp, '*');
		ASSERT( ptmp );
        ptmp++;

        // Now, convert from Hex back to binary
        unsigned char * ptr = kmd;
        unsigned int hex;
        for(int i = 0; i < len; i++) {
            sscanf(ptmp, "%2X", &hex);
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
	char * outbuf = new char[500];
    if (outbuf) {
        memset(outbuf, 0, 500);
        sprintf(outbuf,"%u*%d*%d",_sock,_state,_timeout);
    }
    else {
        dprintf(D_ALWAYS, "Out of memory!\n");
    }
	return( outbuf );
}

char * Sock::serialize(char *buf)
{
	char *ptmp;
	int i;
	SOCKET passed_sock;

	ASSERT(buf);

	// here we want to restore our state from the incoming buffer
	sscanf(buf,"%u*%d*%d*",&passed_sock,(int*)&_state,&_timeout);

	// replace _sock with the one from the buffer _only_ if _sock
	// is currently invalid.  if it is not invalid, it has already
	// been initialized (probably via the Sock copy constructor) and
	// therefore we should _not mess with it_.
	if ( _sock == INVALID_SOCKET ) {
		_sock = passed_sock;
	}

	// call the timeout method to make certain socket state set via
	// setsockopt() and/or ioctl() is restored.
	timeout(_timeout);

	// set our return value to a pointer beyond the 3 state values...
	ptmp = buf;
	for (i=0;i<3;i++) {
		if ( ptmp ) {
			ptmp = strchr(ptmp,'*');
			ptmp++;
		}
	}

	return ptmp;
}


struct sockaddr_in *
Sock::endpoint()
{
	return &_who;
}


int
Sock::endpoint_port()
{
	return (int) ntohs( _who.sin_port );
}


unsigned int
Sock::endpoint_ip_int()
{
	return (unsigned int) ntohl( _who.sin_addr.s_addr );
}


const char *
Sock::endpoint_ip_str()
{
		// We need to recompute this each time because _who might have changed.
	memset(&_endpoint_ip_buf, 0, IP_STRING_BUF_SIZE );
	strcpy( _endpoint_ip_buf, inet_ntoa(_who.sin_addr) );
	return &(_endpoint_ip_buf[0]);
}


// my port and IP address in a struct sockaddr_in
// @args: the address is returned via 'sin'
// @ret: 0 if succeed, -1 if failed
int
Sock::mypoint( struct sockaddr_in *sin )
{
    struct sockaddr_in *tmp = getSockAddr(_sock);
    if (tmp == NULL) return -1;

    memcpy(sin, tmp, sizeof(struct sockaddr_in));
    return 0;
}

const char *
Sock::sender_ip_str()
{
		// We need to recompute this each in case we have reconnected via a different interface
	struct sockaddr_in sin;
	if(mypoint(&sin) == -1) {
		return NULL;
	}
	memset(&_sender_ip_buf, 0, IP_STRING_BUF_SIZE );
	strcpy( _sender_ip_buf, inet_ntoa(sin.sin_addr) );
	return &(_sender_ip_buf[0]);
}

char *
Sock::get_sinful()
{       
    struct sockaddr_in *tmp = getSockAddr(_sock);
    if (tmp == NULL) return NULL;
    char const *s = sin_to_string(tmp);
	if(!s) {
		return NULL;
	}
	ASSERT(strlen(s) < sizeof(_sinful_self_buf));
	strcpy(_sinful_self_buf,s);
	return _sinful_self_buf;
}

char *
Sock::get_sinful_peer()
{       
    char const *s = sin_to_string(&_who);
	if(!s) {
		return NULL;
	}
	ASSERT(strlen(s) < sizeof(_sinful_peer_buf));
	strcpy(_sinful_peer_buf,s);
	return _sinful_peer_buf;
}


int 
Sock::get_file_desc()
{
	return _sock;
}


int
Sock::get_port()
{
	sockaddr_in	addr;

	SOCKET_LENGTH_TYPE addr_len;
	
	addr_len = sizeof(sockaddr_in);

	if (getsockname(_sock, (sockaddr *)&addr, &addr_len) < 0) return -1;
	return (int) ntohs(addr.sin_port);
}


unsigned int 
Sock::get_ip_int()
{
	sockaddr_in	addr;
	SOCKET_LENGTH_TYPE addr_len;

	addr_len = sizeof(sockaddr_in);

	if (getsockname(_sock, (sockaddr *)&addr, &addr_len) < 0) return 0;
	return (unsigned int) ntohl(addr.sin_addr.s_addr);
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

static void async_handler( int s )
{
	int i;
	fd_set set;
	int success;
	struct timeval zero;

	zero.tv_sec = 0;
	zero.tv_usec = 0;

	FD_ZERO( &set );

	for( i=0; i<table_size; i++ ) {
		if( handler_table[i] ) {
			FD_SET( i, &set );
		}
	}

	success = ::select( table_size, &set, 0, 0, &zero );

	if( success>0 ) {
		for( i=0; i<table_size; i++ ) {
			if( FD_ISSET( i, &set ) ) {
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


void Sock :: setFullyQualifiedUser(char *)
{
	return;
}

const char * Sock :: getFullyQualifiedUser()
{
	return NULL;
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

int Sock :: authenticate(KeyInfo *&, const char * methods, CondorError* errstack)
{
	return -1;
}

int Sock :: authenticate(const char * methods, CondorError* errstack)
{
	/*
	errstack->push("AUTHENTICATE", AUTHENTICATE_ERR_NOT_BUILT,
			"Failure: This version of condor was not compiled with authentication enabled");
	*/
	return -1;
}

int Sock :: isAuthenticated() const
{
	return -1;
}

void Sock :: unAuthenticate()
{}
	
bool Sock :: is_encrypt()
{
    return FALSE;
}


