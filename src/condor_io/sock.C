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
#include "condor_constants.h"
#include "condor_io.h"
#include "sock.h"
#include "condor_network.h"
#include "internet.h"
#include "my_hostname.h"
#include "condor_debug.h"
#include "condor_socket_types.h"
#include "get_port_range.h"

#if !defined(WIN32)
#define closesocket close
#endif

// initialize static data members
int Sock::timeout_multiplier = 0;

Sock::Sock() : Stream() {
	_sock = INVALID_SOCKET;
	_state = sock_virgin;
	_timeout = 0;
	connect_state.host = NULL;
	memset(&_who, 0, sizeof(struct sockaddr_in));
	memset(&_endpoint_ip_buf, 0, _ENDPOINT_BUF_SIZE);
}

Sock::Sock(const Sock & orig) : Stream() {

	// initialize everything in the new sock
	_sock = INVALID_SOCKET;
	_state = sock_virgin;
	_timeout = 0;
	connect_state.host = NULL;
	memset( &_who, 0, sizeof( struct sockaddr_in ) );
	memset(	&_endpoint_ip_buf, 0, _ENDPOINT_BUF_SIZE );

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
}

Sock::~Sock()
{
	if ( connect_state.host ) free(connect_state.host);
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
	// on WinNT, sockets are created as inheritable by default.  we
	// want to create the socket as non-inheritable by default.  so 
	// we duplicate the socket as non-inheritable and then close
	// the default inheritable socket.  Note on Win95, it is the opposite:
	// i.e. on Win95 sockets are created non-inheritable by default.
	// note: on UNIX, set_inheritable just always return TRUE.
	if ( !set_inheritable(FALSE) ) return FALSE;
	
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
	// Use hash function with pid to get the starting point
    struct timeval curTime;
#ifndef WIN32
    (void) gettimeofday(&curTime, NULL);
#else
	// Win32 does not have gettimeofday, sigh.
	curTime.tv_usec = ::GetTickCount();
#endif

	// int pid = (int) getpid();
	int range = high_port - low_port + 1;
	// this line must be changed to use the hash function of condor
	int start_trial = low_port + (curTime.tv_usec * 73/*some prime number*/ % range);

	int this_trial = start_trial;
	do {
		sockaddr_in		sin;

		memset(&sin, 0, sizeof(sockaddr_in));
		sin.sin_family = AF_INET;
		sin.sin_addr.s_addr = htonl(my_ip_addr());
		sin.sin_port = htons((u_short)this_trial++);

		if ( ::bind(_sock, (sockaddr *)&sin, sizeof(sockaddr_in)) == 0 ) { // success
			dprintf(D_NETWORK, "Sock::bindWithin - bound to %d...\n", this_trial-1);
			return TRUE;
		} else {
#ifdef WIN32
			int error = WSAGetLastError();
			dprintf(D_NETWORK, 
				"Sock::bindWithin - failed to bind: WSAError = %d\n", error );
#else
			dprintf(D_NETWORK, "Sock::bindWithin - failed to bind: %s\n", strerror(errno));
#endif
		}

		if ( this_trial > high_port )
			this_trial = low_port;
	} while(this_trial != start_trial);

	dprintf(D_ALWAYS, "Sock::bindWithin - failed to bind any port within (%d ~ %d)\n",
	        low_port, high_port);

	return FALSE;
}


int Sock::bind(int port)
{
	sockaddr_in		sin;

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
	int lowPort, highPort;
	if ( port == 0 && get_port_range(&lowPort, &highPort) == TRUE ) {
			// Bind in a specific port range.
		if ( bindWithin(lowPort, highPort) != TRUE ) {
			return FALSE;
		}
	} else {
			// Bind to a dynamic port.
		memset(&sin, 0, sizeof(sockaddr_in));
		sin.sin_family = AF_INET;
		sin.sin_addr.s_addr = htonl(my_ip_addr());
		sin.sin_port = htons((u_short)port);

		if (::bind(_sock, (sockaddr *)&sin, sizeof(sockaddr_in)) < 0) {
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
	if (_state == sock_virgin || _state == sock_assigned) bind();

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
		if ((hostp = gethostbyname(host)) == (hostent *)0) return FALSE;
		memcpy(&_who.sin_addr, hostp->h_addr, hostp->h_length);
	}

	if (_timeout < CONNECT_TIMEOUT) {
		connect_state.timeout_interval = CONNECT_TIMEOUT;
	} else {
		connect_state.timeout_interval = _timeout;
	}
	connect_state.timeout_time = time(NULL) + connect_state.timeout_interval;
	connect_state.connect_failed = false;
	connect_state.failed_once = false;
	connect_state.non_blocking_flag = non_blocking_flag;
	if ( connect_state.host ) free( connect_state.host );
	connect_state.host = strdup(host);
	connect_state.port = port;

	do {
		connect_state.connect_failed = false;

			// If non-blocking, we must be certain the code in the timeout()
			// method which sets up the socket to be non-blocking with the 
			// OS has happened.  So call timeout() now, and save the old
			// value so we can set it back.
		if ( non_blocking_flag ) {
			connect_state.old_timeout_value = timeout(1);
			if ( connect_state.old_timeout_value < 0 ) {
				// failed to set socket to non-blocking
				return FALSE;
			}
		}

		if ( do_connect_tryit() ) {
			return TRUE;
		}

		if ( non_blocking_flag && !connect_state.connect_failed) {
			_state = sock_connect_pending;
			if( DebugFlags & D_NETWORK ) {
				char* dst = strdup( sin_to_string(&_who) );
				dprintf(D_NETWORK,"non-blocking CONNECT started fd=%d dst=%s\n",
				         _sock, dst );
				free( dst );
			}
			return CEDAR_EWOULDBLOCK; 
		}

			// Note, if timeout is 0, do_connect_tryit() is either
			// going to block until connect succeeds, or it will fail
			// miserably, i.e., errno will *NOT* be E_INPROGRESS

		if (_timeout > 0 && !connect_state.connect_failed) {
			struct timeval	timer;
			fd_set			writefds;
			int				nfds=0, nfound;
			timer.tv_sec = _timeout;
			timer.tv_usec = 0;
#if !defined(WIN32) // nfds is ignored on WIN32
			nfds = _sock + 1;
#endif
			FD_ZERO( &writefds );
			FD_SET( _sock, &writefds );

			nfound = ::select( nfds, 0, &writefds, &writefds, &timer );

				// select() might return 1 or 2, depending on the
				// platform and if select() is implemented in such a
				// way that if our socket is set in *both* the write
				// and the execpt sets (does select ever do that?  we
				// don't know...) -Derek, Todd and Pete K. 1/19/01
			if( nfound > 0 ) {
				if ( do_connect_finish() ) {
					return TRUE;
				}
			} else {
				if (!connect_state.failed_once) {
					dprintf( D_ALWAYS, "select returns %d, connect failed\n",
							 nfound );
					dprintf( D_ALWAYS, "Will keep trying for %d seconds...\n",
							 connect_state.timeout_interval );
					connect_state.failed_once = true;
				}	
			}
		}

		// we don't want to busyloop on connect, so sleep for a second
		// before we try again
		sleep(1);

	} while (time(NULL) < connect_state.timeout_time);

	dprintf( D_ALWAYS, "Connect failed for %d seconds; returning FALSE\n",
			 connect_state.timeout_interval );
	return FALSE;
}

bool Sock::do_connect_finish()
{
	if (test_connection()) {
		_state = sock_connect;
		if( DebugFlags & D_NETWORK ) {
			char* src = strdup(	sock_to_string(_sock) );
			char* dst = strdup( sin_to_string(&_who) );
			dprintf( D_NETWORK, "CONNECT src=%s fd=%d dst=%s\n",
					 src, _sock, dst );
			free( src );
			free( dst );
		}
		if ( connect_state.non_blocking_flag ) {
			timeout(connect_state.old_timeout_value);			
		}
		return true;
	}
	
	if (!connect_state.failed_once) {
		dprintf( D_ALWAYS, 
				 "getpeername failed so connect must have failed\n");
		connect_state.failed_once = true;
	}

	if ( connect_state.non_blocking_flag ) {

		if (time(NULL) < connect_state.timeout_time)
		{
				// we don't want to busyloop on connect, so sleep for a second
				// before we try again
			sleep(1);
			if ( do_connect_tryit() )
				return true;
		} else {
			// we've tried to connect until timeout_time without success.
			// so we need to giveup.  to do this in non-blocking mode, 
			// we must return true so our caller knows we are finished.  the
			// caller will know we failed to connect because we're setting
			// the sock _state apropriately.
			_state = sock_bound;	// just bound, *not* sock_connect.
			return true;
		}
	}

	return false;
}

bool Sock::do_connect_tryit()
{
	if (::connect(_sock, (sockaddr *)&_who, sizeof(sockaddr_in)) == 0) {
		if ( connect_state.non_blocking_flag ) {
			//Pretend that we haven't connected yet so that there is
			//only one code path for all non-blocking connects.
			//Otherwise, blindly calling DaemonCore::Register_Socket()
			//after initiating a non-blocking connect will fail if
			//connect() happens to complete immediately.  Why does
			//that fail?  Because DaemonCore will see that the socket
			//is in a connected state and will therefore select() on
			//reads instead of writes.

			return false;
		}
		_state = sock_connect;
		if( DebugFlags & D_NETWORK ) {
			char* src = strdup(	sock_to_string(_sock) );
			char* dst = strdup( sin_to_string(&_who) );
			dprintf( D_NETWORK, "CONNECT src=%s fd=%d dst=%s\n",
					 src, _sock, dst );
			free( src );
			free( dst );
		}
		return true;
	}

#if defined(WIN32)
	int lasterr = WSAGetLastError();
	if (lasterr != WSAEINPROGRESS && lasterr != WSAEWOULDBLOCK) {
		if (!connect_state.failed_once) {
			dprintf( D_ALWAYS, "Can't connect to %s:%d, errno = %d\n",
					 connect_state.host, connect_state.port, lasterr );
			dprintf( D_ALWAYS, "Will keep trying for %d seconds...\n",
					 connect_state.timeout_interval );
			connect_state.failed_once = true;
		}
		connect_state.connect_failed = true;
	}
#else

		// errno can only be EINPROGRESS if timeout is > 0.
		// -Derek, Todd, Pete K. 1/19/01
	if (errno != EINPROGRESS) {
		if (!connect_state.failed_once) {
			dprintf( D_ALWAYS, "Can't connect to %s:%d, errno = %d\n",
					 connect_state.host, connect_state.port, errno );
			dprintf( D_ALWAYS, "Will keep trying for %d seconds...\n",
					 connect_state.timeout_interval );
			connect_state.failed_once = true;
		}
		connect_state.connect_failed = true;

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
			return false;
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
				return false;
			}
			::closesocket(_sock);
			_sock = old_sock;
		}

		// finally, bind the socket
		bind();
	}
#endif /* end of unix code */

	return false;
}

time_t Sock::connect_timeout_time()
{
	return _state == sock_connect_pending ? connect_state.timeout_time : 0;
}

bool Sock::test_connection()
{
	// test the connection -- on OSF1, select returns 1 even if
	// the connect fails!

	struct sockaddr_in test_addr;
	memset((char *) &test_addr, 0, sizeof(test_addr));
	test_addr.sin_family = AF_INET;

	SOCKET_LENGTH_TYPE nbytes;
	
	nbytes = sizeof(test_addr);
	if (getpeername(_sock, (struct sockaddr *) &test_addr, &nbytes) < 0) {
		sleep(1);	// try once more -- sometimes it fails the first time
		if (getpeername(_sock, (struct sockaddr *) &test_addr, &nbytes) < 0) {
			sleep(1);	// try once more -- sometimes it fails the second time
			if (getpeername(_sock, (struct sockaddr *) &test_addr, &nbytes)<0) {
				return false;
			}
		}
	}
	return true;
}


int Sock::close()
{
	if (_state == sock_virgin) return FALSE;

	if (type() == Stream::reli_sock) {
		dprintf( D_NETWORK, "CLOSE %s fd=%d\n", 
						sock_to_string(_sock), _sock );
	}

	if (::closesocket(_sock) < 0) return FALSE;

	_sock = INVALID_SOCKET;
	_state = sock_virgin;
    if (connect_state.host) {
        free(connect_state.host);
    }
	connect_state.host = NULL;
	memset(&_who, 0, sizeof( struct sockaddr_in ) );
	memset(&_endpoint_ip_buf, 0, _ENDPOINT_BUF_SIZE );
	
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


char *
Sock::endpoint_ip_str()
{
		// We need to recompute this each time because _who might have changed.
	memset(&_endpoint_ip_buf, 0, _ENDPOINT_BUF_SIZE );
	strcpy( _endpoint_ip_buf, inet_ntoa(_who.sin_addr) );
	return &(_endpoint_ip_buf[0]);
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

	success = select( table_size, &set, 0, 0, &zero );

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

int Sock :: isAuthenticated()
{
	return -1;
}

void Sock :: unAuthenticate()
{}
	
bool Sock :: is_encrypt()
{
    return FALSE;
}


