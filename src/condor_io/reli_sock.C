/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

 



#define _POSIX_SOURCE

#include "condor_common.h"
#include "condor_constants.h"
#include "condor_io.h"
#include "condor_adtypes.h"
#include "condor_debug.h"
#include "internet.h"
#include "condor_rw.h"

#if defined(GSS_AUTHENTICATION)
gss_cred_id_t ReliSock::credential_handle = GSS_C_NO_CREDENTIAL;
#endif defined(GSS_AUTHENTICATION)

static char _FileName_[] = __FILE__;
extern char *mySubSystem;


/**************************************************************/

ReliSock::ReliSock() : Sock(), ignore_next_encode_eom(FALSE),
		ignore_next_decode_eom(FALSE) 
{
	auth_setup( CAUTH_NONE );
}


ReliSock::ReliSock(					/* listen on port		*/
	int		port
	)
	: Sock(), ignore_next_encode_eom(FALSE), ignore_next_decode_eom(FALSE)
{
	if (!listen(port)) {
		dprintf(D_ALWAYS, "failed to listen on port %d!\n", port);
	}
	else {
		auth_setup( CAUTH_SERVER );
	}
}


ReliSock::ReliSock(					/* listen on serv		*/
	char	*serv
	)
	: Sock(), ignore_next_encode_eom(FALSE), ignore_next_decode_eom(FALSE)
{
	if (!listen(serv)) {
		dprintf(D_ALWAYS, "failed to listen on serv %s!\n", serv);
	}
	else {
		auth_setup( CAUTH_SERVER );
	}
}


ReliSock::ReliSock(
	char	*host,
	int		port,
	int		timeout_val
	)
	: Sock(), ignore_next_encode_eom(FALSE), ignore_next_decode_eom(FALSE)
{
	timeout(timeout_val);
	if (!connect(host, port)) {
		dprintf(D_ALWAYS, "failed to connect to %s:%d!\n", host, port);
	}
	else {
		auth_setup( CAUTH_CLIENT );
	}
}



ReliSock::ReliSock(
	char	*host,
	char	*serv,
	int		timeout_val
	)
	: Sock(), ignore_next_encode_eom(FALSE), ignore_next_decode_eom(FALSE)
{
	timeout(timeout_val);
	if (!Sock::connect(host, serv)) {
		dprintf(D_ALWAYS, "failed to connect to %s:%s!\n", host, serv);
	}
	else {
		auth_setup( CAUTH_CLIENT );
	}
}



ReliSock::~ReliSock()
{
	close();
//	if ( GSSClientname ) {
//		free( GSSClientname );
//	}
}


int 
ReliSock::put_bytes_nobuffer(char *buf, int length, int send_size)
{
	int i, result;
	int pagesize = 65536;  // Optimize large writes to be page sized.
	char * cur = buf;

	// Tell peer how big the transfer is going to be, if requested.
	// Note: send_size param is 1 (true) by default.
	this->encode();
	if ( send_size ) {
		ASSERT( this->code(length) != FALSE );
		ASSERT( this->end_of_message() != FALSE );
	}

	// First drain outgoing buffers
	if ( !prepare_for_nobuffering(stream_encode) ) {
		// error flushing buffers; error message already printed
		return -1;
	}

	// Optimize transfer by writing in pagesized chunks.
	for(i = 0; i < length;)
	{
		// If there is less then a page left.
		if( (length - i) < pagesize ) {
			result = condor_write(_sock, cur, (length - i), _timeout);
			if( result < 0 ) {
				dprintf(D_ALWAYS, 
					"ReliSock::put_bytes_nobuffer: Send failed.\n");
				return -1;
			}
			cur += (length - i);
			i += (length - i);
		} else {  
			// Send another page...
			result = condor_write(_sock, cur, pagesize, _timeout);
			if( result < 0 ) {
				dprintf(D_ALWAYS, 
					"ReliSock::put_bytes_nobuffer: Send failed.\n");
				return -1;
			}
			cur += pagesize;
			i += pagesize;
		}
	}
	return i;
}

int 
ReliSock::get_bytes_nobuffer(char *buffer, int max_length, int receive_size)
{
	int result;
	int length;

	ASSERT(buffer != NULL);
	ASSERT(max_length > 0);

	// Find out how big the file is going to be, if requested.
	// No receive_size means read max_length bytes.
	this->decode();
	if ( receive_size ) {
		ASSERT( this->code(length) != FALSE );
		ASSERT( this->end_of_message() != FALSE );
	} else {
		length = max_length;
	}

	// First drain incoming buffers
	if ( !prepare_for_nobuffering(stream_decode) ) {
		// error draining buffers; error message already printed
		return -1;
	}


	if( length > max_length ) {
		dprintf(D_ALWAYS, 
			"ReliSock::get_bytes_nobuffer: data too large for buffer.\n");
		return -1;
	}

	result = condor_read(_sock, buffer, length, _timeout);
	
	if( result < 0 ) {
		dprintf(D_ALWAYS, 
			"ReliSock::get_bytes_nobuffer: Failed to receive file.\n");
		return -1;
	} else {
		return result;
	}
}

int
ReliSock::get_file(const char *destination)
{
	int total=0, nbytes, written, fd;
	char buf[65536];
	unsigned int filesize;

	if ( !get(filesize) || !end_of_message() ) {
		dprintf(D_ALWAYS, 
			"Failed to receive filesize in ReliSock::get_file\n");
		return FALSE;
	}
	dprintf(D_FULLDEBUG,"get_file(): filesize=%d\n",filesize);

#if defined(WIN32)
	if ((fd = ::open(destination, O_WRONLY | O_CREAT | _O_BINARY, 0644)) < 0)
#else
	if ((fd = ::open(destination, O_WRONLY | O_CREAT, 0644)) < 0)
#endif
	{
		dprintf(D_ALWAYS, 
			"get_file(): Failed to open file %s, errno = %d.\n", destination, errno);
		return FALSE;
	}

	while (total < (int)filesize &&
			(nbytes = get_bytes_nobuffer(buf, MIN(sizeof(buf),filesize-total),0)) > 0) {
		dprintf(D_FULLDEBUG, "read %d bytes\n", nbytes);
		if ((written = ::write(fd, buf, nbytes)) < nbytes) {
			dprintf(D_ALWAYS, "failed to write %d bytes in ReliSock::get_file "
					"(only wrote %d, errno=%d)\n", nbytes, written, errno);
			::close(fd);
			unlink(destination);
			return FALSE;
		}
		dprintf(D_FULLDEBUG, "wrote %d bytes\n", written);
		total += written;
	}
	dprintf(D_FULLDEBUG, "done with transfer, errno = %d\n", errno);
	dprintf(D_FULLDEBUG, "wrote %d bytes to file %s\n",
			total, destination);

	if (::close(fd) < 0) {
		dprintf(D_ALWAYS, "close failed in ReliSock::get_file\n");
		return FALSE;
	}

	if ( total < (int)filesize ) {
		dprintf(D_ALWAYS,"get_file(): ERROR: received %d bytes, expected %d!\n",
			total, filesize);
		unlink(destination);
		return FALSE;
	}

	return total;
}

int
ReliSock::put_file(const char *source)
{
	int total=0, nbytes, nrd, fd;
	char buf[65536];
	struct stat filestat;
	unsigned int filesize;

#if defined(WIN32)
	if ((fd = ::open(source, O_RDONLY | _O_BINARY | _O_SEQUENTIAL, 0)) < 0)
#else
	if ((fd = ::open(source, O_RDONLY, 0)) < 0)
#endif
	{
		dprintf(D_ALWAYS, "Failed to open file %s, errno = %d.\n", source, errno);
		return FALSE;
	}

	if (::fstat(fd, &filestat) < 0) {
		dprintf(D_ALWAYS, "fstat of %s failed\n", source);
		::close(fd);
		return FALSE;
	}

	filesize = filestat.st_size;
	if ( !put(filesize) || !end_of_message() ) {
		dprintf(D_ALWAYS, "Failed to send filesize in ReliSock::put_file\n");
		::close(fd);
		return FALSE;
	}

	while (total < filestat.st_size &&
		(nrd = ::read(fd, buf, sizeof(buf))) > 0) {
		dprintf(D_FULLDEBUG, "read %d bytes\n", nrd);
		if ((nbytes = put_bytes_nobuffer(buf, nrd, 0)) < nrd) {
			dprintf(D_ALWAYS, "failed to put %d bytes in ReliSock::put_file "
				"(only wrote %d)\n", nrd, nbytes);
			::close(fd);
			return FALSE;
		}
		// dprintf(D_FULLDEBUG, "wrote %d bytes\n", nbytes);
		total += nbytes;
	}
	dprintf(D_FULLDEBUG, "done with transfer, errno = %d\n", errno);
	dprintf(D_FULLDEBUG, "sent file %s (%d bytes)\n", source, total);

	if (::close(fd) < 0) {
		dprintf(D_ALWAYS, "close failed in ReliSock::put_file, errno = %d\n", errno);
		return FALSE;
	}

	if (total < (int)filesize) {
		dprintf(D_ALWAYS,"put_file(): ERROR, only sent %d bytes out of %d\n",
			total, filesize);
		return FALSE;
	}

	return total;
}


int 
ReliSock::listen()
{
	if (!valid()) return FALSE;

	if (_state != sock_bound) return FALSE;
	if (::listen(_sock, 5) < 0) return FALSE;

	dprintf( D_NETWORK, "%s LISTEN %s\n", mySubSystem, sock_to_string(_sock) );

	_state = sock_special;
	_special_state = relisock_listen;

	return TRUE;
}

int 
ReliSock::accept( ReliSock	&c )
{
	int			c_sock;
	sockaddr_in	addr;
	int			addr_sz;


	if (!valid()) return FALSE;
	if (_state != sock_special || _special_state != relisock_listen ||
													c._state != sock_virgin)
	{
		return FALSE;
	}

	if (_timeout > 0) {
		struct timeval	timer;
		fd_set			readfds;
		int				nfds=0, nfound;
		timer.tv_sec = _timeout;
		timer.tv_usec = 0;
#if !defined(WIN32) // nfds is ignored on WIN32
		nfds = _sock + 1;
#endif
		FD_ZERO( &readfds );
		FD_SET( _sock, &readfds );

		nfound = select( nfds, &readfds, 0, 0, &timer );

		switch(nfound) {
		case 0:
			return FALSE;
			break;
		case 1:
			break;
		default:
			dprintf( D_ALWAYS, "select returns %d, connect failed\n",
				nfound );
			return FALSE;
			break;
		}
	}

	addr_sz = sizeof(addr);
	if ((c_sock = ::accept(_sock, (sockaddr *)&addr, &addr_sz)) < 0)
		return FALSE;

	c._sock = c_sock;
	c._state = sock_connect;
	c.decode();
	if (  c._timeout == 0 ) {
		int on = 1;
		c.setsockopt(SOL_SOCKET, SO_KEEPALIVE, (char*)&on, sizeof(on));
	}

	c.set_conn_type( CAUTH_SERVER );

	dprintf( D_NETWORK, "%s ACCEPT %s ", mySubSystem, sock_to_string(_sock) );
	dprintf( D_NETWORK|D_NOHEADER, "%s\n", sin_to_string(c.endpoint()) );
	
	return TRUE;
}


int 
ReliSock::accept( ReliSock	*c)
{
	if (!c) 
	{
		return FALSE;
	}

	return accept(*c);
}



ReliSock *
ReliSock::accept()
{
	ReliSock	*c_rs;
	int			c_sock;


	if (!(c_rs = new ReliSock())) {
		return (ReliSock *)0;
	}

	if ((c_sock = accept(*c_rs)) < 0) {
		delete c_rs;
		return (ReliSock *)0;
	}

	c_rs->set_conn_type( CAUTH_SERVER );
	return c_rs;
}

int 
ReliSock::handle_incoming_packet()
{
	/* if socket is listening, and packet is there, it is ready for accept */
	if (_state == sock_special && _special_state == relisock_listen) {
		return TRUE;
	}

	/* do not queue up more than one message at a time on reliable sockets */
	/* but return 1, because old message can still be read.						*/
	if (rcv_msg.ready) {
		return TRUE;
	}

	if (!rcv_msg.rcv_packet(_sock, _timeout)) {
		return FALSE;
	}

	return TRUE;
}



int 
ReliSock::end_of_message()
{
	int ret_val = FALSE;

	switch(_coding){
		case stream_encode:
			if ( ignore_next_encode_eom == TRUE ) {
				ignore_next_encode_eom = FALSE;
				return TRUE;
			}
			if (!snd_msg.buf.empty()) {
				return snd_msg.snd_packet(_sock, TRUE, _timeout);
			}
			break;

		case stream_decode:
			if ( ignore_next_decode_eom == TRUE ) {
				ignore_next_decode_eom = FALSE;
				return TRUE;
			}
			if ( rcv_msg.ready ) {
				if ( rcv_msg.buf.consumed() )
					ret_val = TRUE;
				rcv_msg.ready = FALSE;
				rcv_msg.buf.reset();
			}
			break;

		default:
			assert(0);
	}

	return ret_val;
}



int 
ReliSock::connect( char	*host, int		port)
{
	conn_type = CAUTH_CLIENT;
	return do_connect(host, port);
}



int 
ReliSock::put_bytes(const void *dta, int sz)
{
	int		tw;
	int		nw;

	if (!valid()) return -1;

	ignore_next_encode_eom = FALSE;

	for(nw=0;;) {
		
		if (snd_msg.buf.full()) {
			if (!snd_msg.snd_packet(_sock, FALSE, _timeout)) {
				return FALSE;
			}
		}
		
		if (snd_msg.buf.empty()) {
			snd_msg.buf.seek(5);
		}
		
		if ((tw = snd_msg.buf.put_max(&((char *)dta)[nw], sz-nw)) < 0) {
			return -1;
		}
		
		nw += tw;
		if (nw == sz) {
			break;
		}
	}
	return nw;
}


int 
ReliSock::get_bytes(void *dta, int max_sz)
{
	if (!valid()) {
		return -1;
	}

	ignore_next_decode_eom = FALSE;

	while (!rcv_msg.ready) {
		if (!handle_incoming_packet()){
			return FALSE;
		}
	}
	
	return rcv_msg.buf.get(dta, max_sz);
}


int ReliSock::get_ptr( void *&ptr, char delim)
{
	if (!valid()) {
		return -1;
	}

	while (!rcv_msg.ready){
		if (!handle_incoming_packet()) {
			return FALSE;
		}
	}

	return rcv_msg.buf.get_tmp(ptr, delim);
}


int ReliSock::peek( char &c)
{
	if (!valid()) {
		return -1;
	}

	while (!rcv_msg.ready) {
		if (!handle_incoming_packet()) {
			return FALSE;
		}
	}

	return rcv_msg.buf.peek(c);
}

int ReliSock::RcvMsg::rcv_packet( SOCKET _sock, int _timeout)
{
	Buf		*tmp;
	char	hdr[5];
	int		end;
	int		len, len_t;
	int		tmp_len;

	len = 0;
	while (len < 5) {
		if (_timeout > 0) {
			struct timeval	timer;
			fd_set			readfds;
			int				nfds=0, nfound;
			timer.tv_sec = _timeout;
			timer.tv_usec = 0;
#if !defined(WIN32) // nfds is ignored on WIN32
			nfds = _sock + 1;
#endif
			FD_ZERO( &readfds );
			FD_SET( _sock, &readfds );
				
			nfound = select( nfds, &readfds, 0, 0, &timer );
				
			switch(nfound) {
			case 0:
				return FALSE;
				break;
			case 1:
				break;
			default:
				dprintf( D_ALWAYS, "select returns %d, recv failed\n", nfound );
				return FALSE;
				break;
			}
		}
		tmp_len = recv(_sock, hdr+len, 5-len, 0);
		if (tmp_len <= 0) {
			return FALSE;
		}
		len += tmp_len;
	}
	end = (int) ((char *)hdr)[0];
	memcpy(&len_t,  &hdr[1], 4);
	len = (int) ntohl(len_t);
		
	if (!(tmp = new Buf)){
		dprintf(D_ALWAYS, "IO: Out of memory\n");
		return FALSE;
	}
	if (len > tmp->max_size()){
		delete tmp;
		dprintf(D_ALWAYS, "IO: Incoming packet is too big\n");
		return FALSE;
	}
	if ((tmp_len = tmp->read(_sock, len, _timeout)) != len){
		delete tmp;
		dprintf(D_ALWAYS, "IO: Packet read failed: read %d of %d\n",
				tmp_len, len);
		return FALSE;
	}
	if (!buf.put(tmp)) {
		delete tmp;
		dprintf(D_ALWAYS, "IO: Packet storing failed\n");
		return FALSE;
	}
		
	if (end) {
		ready = TRUE;
	}
	return TRUE;
}


int ReliSock::SndMsg::snd_packet( int _sock, int end, int _timeout )
{
	char	hdr[5];
	int		len;
	int		ns;

	hdr[0] = (char) end;
	ns = buf.num_used()-5;
	len = (int) htonl(ns);

	memcpy(&hdr[1], &len, 4);
	if (buf.flush(_sock, hdr, 5, _timeout) != (ns+5)){
		return FALSE;
	}

	return TRUE;
}

struct sockaddr_in * 
ReliSock::endpoint()
{
	int addr_len;

	addr_len = sizeof(sockaddr_in);

	if (getpeername(_sock, (struct sockaddr *) &_who, &addr_len) < 0) {
		return NULL;	// socket not connected
	}

	return( &_who );
}
		
int 
ReliSock::get_port()
{
	sockaddr_in	addr;
	int addr_len = sizeof(sockaddr_in);

	if (getsockname(_sock, (sockaddr *)&addr, &addr_len) < 0) {
		return -1;
	}

	return( (int) ntohs(addr.sin_port) );
}


int 
ReliSock::get_file_desc()
{
	return _sock;
}

#ifndef WIN32
	// interface no longer supported
int 
ReliSock::attach_to_file_desc( int fd )
{
	if (_state != sock_virgin) {
		return FALSE;
	}

	_sock = fd;
	_state = sock_connect;
	return TRUE;
}
#endif

Stream::stream_type 
ReliSock::type() 
{ 
	return Stream::reli_sock; 
}

char * 
ReliSock::serialize(char *buf)
{
	char sinful_string[28];
	char *ptmp;

	if ( buf == NULL ) {
		// here we want to save our state into a buffer

		// first, get the state from our parent class
		char * parent_state = Sock::do_serialize();
		// now concatenate our state
		char * outbuf = new char[50];
		sprintf(outbuf,"*%d*%s",_special_state,sin_to_string(&_who));
		strcat(parent_state,outbuf);
		delete []outbuf;
		return( parent_state );
	}

	// here we want to restore our state from the incoming buffer

	// first, let our parent class restore its state
	ptmp = Sock::do_serialize(buf);
	assert( ptmp );
	sscanf(ptmp,"%d*%s",&_special_state,sinful_string);
	string_to_sin(sinful_string, &_who);

	return NULL;
}

int ReliSock::prepare_for_nobuffering(stream_coding direction)
{
	int ret_val = TRUE;

	if ( direction == stream_unknown ) {
		direction = _coding;
	}

	switch(direction){
		case stream_encode:
			if ( ignore_next_encode_eom == TRUE ) {
				// optimization: if we already prepared for nobuffering,
				// just return true.
				return TRUE;
			}
			if (!snd_msg.buf.empty()) {
				ret_val = snd_msg.snd_packet(_sock, TRUE, _timeout);
			}
			if ( ret_val ) {
				ignore_next_encode_eom = TRUE;
			}
			break;

		case stream_decode:
			if ( ignore_next_decode_eom == TRUE ) {
				// optimization: if we already prepared for nobuffering,
				// just return true.
				return TRUE;
			}
			if ( rcv_msg.ready ) {
				if ( !rcv_msg.buf.consumed() )
					ret_val = FALSE;
				rcv_msg.ready = FALSE;
				rcv_msg.buf.reset();
			}
			if ( ret_val ) {
				ignore_next_decode_eom = TRUE;
			}
			break;

		default:
			assert(0);
	}

	return ret_val;
}

/**************************************************************/

int
ReliSock::handshake()
{
	int shouldUseMethod = 0;

#if 0
	//only tell other side about GSS if config and self Authenticated
#if defined(GSS_AUTHENTICATION)
	if( authenticate_self_gss() ) {
		canUseFlags |= CAUTH_GSS;
	}
	else {
		canUseFlags &= ~CAUTH_GSS;
	}
#else
	canUseFlags &= ~CAUTH_GSS;
#endif
#endif

	int clientCanUse = 0;

	switch( conn_type ) {
	 case CAUTH_CLIENT:
		dprintf(D_FULLDEBUG,"authentication handshake, client can use: %d\n",
				canUseFlags );
		encode();
		code( canUseFlags );
		end_of_message();
		decode();
		code( shouldUseMethod );
		end_of_message();
//fprintf(stderr,"authentication handshake, server says use: %d\n",shouldUseMethod );
		break;
	 case CAUTH_SERVER:
		decode();
		code( clientCanUse );
		end_of_message();
		shouldUseMethod = selectAuthenticationType( clientCanUse );
		encode();
		code( shouldUseMethod );
		end_of_message();
		break;
	 default:
		dprintf(D_ALWAYS,"invalid conn_type state in ReliSock::handshake\n" );
		return CAUTH_NONE;
	}
	return( shouldUseMethod );
}

int
ReliSock::authenticate()
{
#ifndef WIN32
	char tmp[128];
	//default to no authentication accomplished at all
	sprintf( tmp, "%s = \"%s\"", USERAUTH_ADTYPE, "none" );

#if 0
	if ( stateClassad ) {
		delete stateClassad;
		stateClassad = NULL;
	}

	stateClassad = new ClassAd();
	stateClassad->SetMyTypeName( USERAUTH_ADTYPE );
#endif

#if 0
	stateClassad.SetMyTypeName( USERAUTH_ADTYPE );
#endif

	int firm = handshake();
	dprintf(D_FULLDEBUG,"authenticate, handshake returned: %d\n", firm );

	switch ( firm ) {
#if defined(GSS_AUTHENTICATION)
	 case CAUTH_GSS:
		authenticate_self_gss();
		if( authenticate_gss() ) {
			sprintf( tmp, "%s = \"%s\"", USERAUTH_ADTYPE, "GSS" );
			auth_status = CAUTH_GSS;
		}
		break;
#endif
	 case CAUTH_FILESYSTEM:
//fprintf(stderr,"filesystem!\n" );
		if ( authenticate_filesystem() ) {
			sprintf( tmp, "%s = \"%s\"", USERAUTH_ADTYPE, "FILESYSTEM" );
			auth_status = CAUTH_FILESYSTEM;
		}
		break;
	 default:
		dprintf(D_FULLDEBUG,"ReliSock::authenticate-- bad handshake\n" );
	}

	//if authenticated, have server send claimsToBe back over
	if ( auth_status != CAUTH_NONE ) {
		int size = 0;
		switch ( conn_type ) {
		 case CAUTH_SERVER:
			encode();
#if 0
			size = strlen( claimToBe );
			code( size );
//			code( claimToBe, size );
#else
//fprintf(stderr,"about to send claimToBe\n" );
//fprintf(stderr,"claimToBe: %s, length: %d\n", claimToBe, strlen(claimToBe ) );
			code( claimToBe );
#endif
			end_of_message();
			break;
		 case CAUTH_CLIENT:
			decode();
#if 0
			code( size );
			code( claimToBe, size );
#else
//fprintf(stderr,"about to receive claimToBe\n" );
			code( claimToBe );
//fprintf(stderr,"claimToBe: %s, length: %d\n", claimToBe, strlen(claimToBe ) );
#endif
			end_of_message();
			break;
		 default:
			dprintf(D_ALWAYS,"ReliSock::authenticate-- not CLIENT or SERVER?\n" );
		}
	}

	//if none of the methods succeeded, we fall thru to default "none" from above
#if 0
	stateClassad.Insert( tmp );
#endif
	//TRUE means success, FALSE is failure (CAUTH_NONE)
	int retval = ( auth_status != CAUTH_NONE );
	dprintf( D_FULLDEBUG, "ReliSock::auth returning %d (TRUE on success)\n",
			retval );
	return ( retval );
#else
	return FALSE;
#endif
}

void 
ReliSock::setFile( char *file ) {
	if ( file ) {
		strcpy( rendevousFile, file );
	}
}

char *
ReliSock::getFile() {
	//should I return copy here?
	return( rendevousFile );
}

void
ReliSock::setOwner( char *owner ) {
	if ( owner ) {
		//check for overflow attempts
		sprintf( claimToBe, "%.*s", MAX_USERNAMELEN, owner );
	}
	else {
		*claimToBe = NULL;
	}
}

char *
ReliSock::getOwner() {
	if ( !this ) {
		return( NULL );
	}

	//should I return copy here?
	return( claimToBe );
}

void 
ReliSock::setOwnerUid( int uid ) {
	ownerUid = uid;

}
int 
ReliSock::getOwnerUid() {
	return( ownerUid );
}

int
ReliSock::authenticate_filesystem()
{

#ifndef WIN32
	if ( conn_type != CAUTH_CLIENT && conn_type != CAUTH_SERVER ) {
		return 0;
	}

	char *new_file = NULL;
	int fd = -1;
	char *owner = NULL;

	if ( conn_type == CAUTH_CLIENT ) {
		//send owner, get filename
		decode();
		code( new_file );
		if ( new_file ) {
			setFile( new_file );
			fd = open(new_file, O_RDONLY | O_CREAT | O_TRUNC, 0666);
			::close(fd);
			free( new_file );
		}
		end_of_message();
		encode();
		owner = getOwner();
		code( owner );
		code( fd );
		end_of_message();
		decode();
		int rval = -1;
		code( rval );
		end_of_message();
		return( rval == 0 );
	}

		//else CAUTH_SERVER
	//create temp file name and send it over
	new_file = tempnam("/tmp", "qmgr_");
	setFile( new_file );
	// man page says string returned by tempnam has been malloc()'d
	encode();
	code( new_file );
	end_of_message();
	decode();
	code( owner );
	code( fd );
	end_of_message();
	encode();

	setOwner( owner );

	int retval = 0;
	if ( fd > -1 ) {
		//check file to ensure that claimant is owner
		struct stat stat_buf;
		if ( lstat( new_file, &stat_buf ) < 0 ) {
			//failed, but keep connection open, set owner to nobody
			retval = 0;  //if not there, say OK, but set owner to nobody
			setOwner( "nobody" );
		}
		unlink( new_file );

init_user_ids( getOwner() );

		// Authentication should fail if a) owner match fails, or b) the
		// file is either a hard or soft link (no links allowed because they
		// could spoof the owner match).  -Todd 3/98
//		if ( (stat_buf.st_uid != getOwnerUid() ) ||
		if ( (stat_buf.st_uid != get_user_uid() ) ||
			(stat_buf.st_nlink > 1) ||	 // check for hard link
			(S_ISLNK(stat_buf.st_mode)) ) 
		{
			setOwner( "nobody" );
			retval = -1;
		}
	}
	else {
		retval = -1;
		dprintf( D_ALWAYS, "invalid state in authenticate_filesystem\n" );
	}
	code( retval );
	end_of_message();
	free( new_file );
	return( retval == 0 );
#else
	return -1;
#endif
}

#if defined(GSS_AUTHENTICATION)

int 
ReliSock::lookup_user_gss( char *client_name ) 
{
	/* return -1 for error */
	char filename[MAXPATHLEN];
	char command[MAXPATHLEN+32];

	sprintf( filename, "%s/index.txt", getenv( "X509_CERT_DIR" ) );

	FILE *index;
	if ( !(index = fopen(  filename, "r" ) ) ) {
		dprintf( D_ALWAYS, "unable to open index file %s, errno %d\n", 
				filename, errno );
		return -1;
	}

	//find entry
	int found = 0;
	char line[81];
	while ( !found && fgets( line, 80, index ) ) {
		//Valid user entries have 'V' as first byte in their cert db entry
		if ( line[0] == 'V' &&  strstr( line, client_name ) ) {
			found = 1;
		}
	}
	fclose( index );
	if ( !found ) {
		dprintf( D_ALWAYS, "unable to find V entry for %s in %s\n", 
				filename, client_name );
		return( -1 );
	}

	return 0;
}

//cannot make this an AuthSock method, since gss_assist method expects
//three parms, methods have hidden "this" parm first. Couldn't figure out
//a way around this, so made AuthSock have a member of type AuthComms
//to pass in to this method to manage buffer space.  //mju
int 
authsock_get(void *arg, void **bufp, size_t *sizep)
{
	/* globus code which calls this function expects 0/-1 return vals */

	//authsock must "hold onto" GSS state, pass in struct with comms stuff
	GSSComms *comms = (GSSComms *) arg;
	ReliSock *sock = comms->sock;
	size_t stat;

	sock->decode();

	//read size of data to read
	stat = sock->code( *((int *)sizep) );

	*bufp = malloc( *((int *)sizep) );
	if ( !*bufp ) {
		dprintf( D_ALWAYS, "malloc failure authsock_get\n" );
		stat = FALSE;
	}

	//if successfully read size and malloced, read data
	if ( stat )
		sock->code_bytes( *bufp, *((int *)sizep) );

	sock->end_of_message();

	//check to ensure comms were successful
	if ( !stat ) {
		dprintf( D_ALWAYS, "authsock_get (read from socket) failure\n" );
		return -1;
	}
	return 0;
}

int 
authsock_put(void *arg,  void *buf, size_t size)
{
	//param is just a AS*
	ReliSock *sock = (ReliSock *) arg;
	int stat;

	sock->encode();

	//send size of data to send
	stat = sock->code( (int &)size );
	//if successful, send the data
	if ( stat ) {
		if ( !(stat = sock->code_bytes( buf, ((int) size )) ) ) {
			dprintf( D_ALWAYS, "failure sending data (%d bytes) over sock\n",size);
		}
	}
	else {
		dprintf( D_ALWAYS, "failure sending size (%d) over sock\n", size );
	}

	sock->end_of_message();

	//ensure data send was successful
	if ( !stat ) {
		dprintf( D_ALWAYS, "authsock_put (write to socket) failure\n" );
		return -1;
	}
	return 0;
}

int 
ReliSock::authenticate_self_gss()
{
	OM_uint32 major_status;
	OM_uint32 minor_status;

	if ( credential_handle != GSS_C_NO_CREDENTIAL ) { // user already auth'd 
		dprintf( D_FULLDEBUG, "This process has a valid certificate & key\n" );
		return TRUE;
	}

	// ensure all env vars are in place, acquire cred will fail otherwise 
	if ( !( getenv( "X509_USER_CERT" ) && getenv( "X509_USER_KEY" ) 
			&& getenv( "X509_CERT_DIR" ) ) ) 
	{
		//don't log error, since this can be called before env vars are set!
		dprintf(D_FULLDEBUG,"X509 env vars not set yet (might not be error)\n");
		return FALSE;
	}

	//use gss-assist to verify user (not connection)
	//this method will prompt for password if private key is encrypted!
	int time = timeout(60 * 5);  //allow user 5 min to type passwd

	major_status = globus_gss_assist_acquire_cred(&minor_status,
		GSS_C_BOTH, &credential_handle);

	timeout(time); //put it back to what it was before

	if (major_status != GSS_S_COMPLETE)
	{
		dprintf( D_ALWAYS, "gss-api failure initializing user credentials, "
				"stats: 0x%x\n", major_status );
		credential_handle = GSS_C_NO_CREDENTIAL; 
		return FALSE;
	}

	dprintf( D_FULLDEBUG, "This process has a valid certificate & key\n" );
	return TRUE;
}

int 
ReliSock::authenticate_client_gss()
{
	OM_uint32						  major_status = 0;
	OM_uint32						  minor_status = 0;
	int								  token_status = 0;
	OM_uint32						  ret_flags = 0;
	gss_ctx_id_t					  context_handle = GSS_C_NO_CONTEXT;

	if ( !authenticate_self_gss() ) {
		dprintf( D_ALWAYS, 
			"failure authenticating client from auth_connection_client\n" );
		return FALSE;
	}
	 
	char *gateKeeper = getenv( "CONDOR_GATEKEEPER" );

	if ( !gateKeeper ) {
		dprintf( D_ALWAYS, "env var CONDOR_GATEKEEPER not set" );
		return FALSE;
	}

	authComms.sock = this;
	authComms.buffer = NULL;
	authComms.size = 0;

	major_status = globus_gss_assist_init_sec_context(&minor_status,
		  credential_handle, &context_handle,
		  gateKeeper,
		  GSS_C_DELEG_FLAG|GSS_C_MUTUAL_FLAG,
		  &ret_flags, &token_status,
		  authsock_get, (void *) &authComms,
		  authsock_put, (void *) this 
	);

	if (major_status != GSS_S_COMPLETE)
	{
		dprintf( D_ALWAYS, "failed auth connection client, gss status: 0x%x\n",
								major_status );
		delete gateKeeper;
		gateKeeper = NULL;
		return FALSE;
	}

	/* 
	 * once connection is Authenticated, don't need sec_context any more
	 * ???might need sec_context for PROXIES???
	 */
	gss_delete_sec_context( &minor_status, &context_handle, GSS_C_NO_BUFFER );

	auth_status = CAUTH_GSS;

	dprintf(D_FULLDEBUG,"valid GSS connection established to %s\n", gateKeeper);
	delete gateKeeper;
	gateKeeper = NULL;
	return TRUE;
}

//made major mods: took out authsock param and made everything [implied] this//
int 
ReliSock::authenticate_server_gss()
{
	int rc;
	OM_uint32 major_status = 0;
	OM_uint32 minor_status = 0;
	int		 token_status = 0;
	OM_uint32 ret_flags = 0;
	gss_ctx_id_t context_handle = GSS_C_NO_CONTEXT;

	if ( !authenticate_self_gss() ) {
		dprintf( D_ALWAYS, 
			"failure authenticating self in auth_connection_server\n" );
		return FALSE;
	}
//	if ( GSSClientname ) {
//		free( GSSClientname );
//		GSSClientname = NULL;
//	}
	 
	authComms.sock = this;
	authComms.buffer = NULL;
	authComms.size = 0;

	major_status = globus_gss_assist_accept_sec_context(&minor_status,
				 &context_handle, credential_handle,
				 &GSSClientname,
				 &ret_flags, NULL, /* don't need user_to_user */
				 &token_status,
				 authsock_get, (void *) &authComms,
				 authsock_put, (void *) this
	);


	if ( (major_status != GSS_S_COMPLETE) ||
			( ( lookup_user_gss( GSSClientname ) ) < 0 ) ) 
	{
		if (major_status != GSS_S_COMPLETE) {
			dprintf(D_ALWAYS, "server: GSS failure authenticating %s 0x%x\n",
					"client, status: ", major_status );
		}
		else {
			dprintf( D_ALWAYS, "server: user lookup failure.\n" );
		}
		return FALSE;
	}

	if ( !nameGssToLocal() ) {
		return( FALSE );
	}

	/* 
	 * once connection is Authenticated, don't need sec_context any more
	 * ???might need sec_context for PROXIES???
	 */
	gss_delete_sec_context( &minor_status, &context_handle, GSS_C_NO_BUFFER );

	auth_status = CAUTH_GSS;

	dprintf(D_FULLDEBUG,"valid GSS connection established to %s\n", 
			GSSClientname);
	return TRUE;
}

int
ReliSock::nameGssToLocal() {
	//this might need to change with SSLK5 stuff

	//just extract username from /CN=<username>@<domain,etc>
	if ( !GSSClientname ) {
		return 0;
	}

	char *tmp;
	tmp = strchr( GSSClientname, '=' );

	if ( tmp ) {
		tmp++;
		sprintf( claimToBe, "%*.*s", strcspn( tmp, "@" ),
				strcspn( tmp, "@" ), tmp );
	}
	else {
		setOwner( GSSClientname );
	}
	return 1;
}

int
ReliSock::authenticate_gss() 
{
	int status = 1;

	//don't just return TRUE if isAuthenticated() == TRUE, since 
	//we should BALANCE calls of Authenticate() on client/server side
	//just like end_of_message() calls must balance!

	if ( !authenticate_self_gss() ) {
		dprintf( D_ALWAYS, "authenticate: user creds not established\n" );
		status = 0;
	}

	if ( status ) {
		//temporarily change timeout to 5 minutes so the user can type passwd
		//MUST do this even on server side, since client side might call
		//authenticate_self_gss() while server is waiting on authenticate!!
		int time = timeout(60 * 5); 

		switch ( conn_type ) {
			case CAUTH_SERVER : 
				dprintf(D_FULLDEBUG,"about to authenticate client from server\n" );
				status = authenticate_server_gss();
init_user_ids( getOwner() );
				break;
			case CAUTH_CLIENT : 
				dprintf(D_FULLDEBUG,"about to authenticate server from client\n" );
				status = authenticate_client_gss();
				break;
			default : 
				dprintf(D_ALWAYS,"authenticate:should have connected/accepted\n");
		}
		timeout(time); //put it back to what it was before
	}

	return( status );
}

#endif defined(GSS_AUTHENTICATION)

int
ReliSock::selectAuthenticationType( int clientCanUse ) 
{
	dprintf( D_FULLDEBUG, "auth handshake: clientCanUse: %d\n", clientCanUse );
	int retval = 0;

	if ( clientCanUse & canUseFlags & CAUTH_FILESYSTEM ) {
		retval = CAUTH_FILESYSTEM;
	}
	else if ( clientCanUse & canUseFlags & CAUTH_GSS ) {
		retval = CAUTH_GSS;
	}
	dprintf(D_FULLDEBUG,"auth handshake: selected method: %d\n", retval );

	return retval;
}

int
ReliSock::isAuthenticated()
{
	return( auth_status != CAUTH_NONE );
}

void
ReliSock::canTryFilesystem()
{
	canUseFlags |= CAUTH_FILESYSTEM;
}

void
ReliSock::canTryGSS()
{
	canUseFlags |= CAUTH_GSS;
}

void 
ReliSock::set_conn_type( authentication_state sock_type ) 
{
	conn_type = sock_type; 
}

void 
ReliSock::setAuthType( authentication_state auth_type ) 
{
	auth_status = auth_type; 
}

#if 0
ClassAd* 
ReliSock::getStateAd()
{
	return( new ClassAd( stateClassad ) );
}
#endif

void 
ReliSock::auth_setup( authentication_state type ) {
	conn_type = type;

	//this is so we don't reset state when a copy or accept happens...
	static int start = 0;
	if (start) {
		return;
	}
	start = 1;

	//credential_handle is static and set at top of this file//
	auth_status = CAUTH_ANY;
	canUseFlags = CAUTH_NONE;
	GSSClientname = NULL;
//	stateClassad = NULL;
	authComms.buffer = NULL;
	authComms.size = 0;
	*rendevousFile = '\0';
	*claimToBe = '\0';
	ownerUid = -1;
}
