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

#include "condor_common.h"
#include "condor_constants.h"
#include "condor_io.h"
#include "authentication.h"
#include "condor_debug.h"
#include "internet.h"
#include "condor_rw.h"
#include "condor_socket_types.h"

#ifdef WIN32
#include <mswsock.h>	// For TransmitFile()
#endif

/**************************************************************/

/* 
   NOTE: All ReliSock constructors initialize with this, so you can
   put any shared initialization code here.  -Derek Wright 3/12/99
*/
void
ReliSock::init()
{
	ignore_next_encode_eom = FALSE;
	ignore_next_decode_eom = FALSE;
	_bytes_sent = 0.0;
	_bytes_recvd = 0.0;
	_special_state = relisock_none;
	is_client = 0;
	authob = NULL;
	hostAddr = NULL;
	rcv_msg.init_parent(this);
	snd_msg.init_parent(this);
}


ReliSock::ReliSock()
	: Sock()
{
	init();
}

ReliSock::ReliSock(const ReliSock & orig) : Sock(orig)
{
	init();
	// now copy all cedar state info via the serialize() method
	char *buf = NULL;
	buf = orig.serialize();	// get state from orig sock
	assert(buf);
	serialize(buf);			// put the state into the new sock
	delete [] buf;
}

ReliSock::~ReliSock()
{
	close();
	if ( authob ) {
		delete authob;
		authob = NULL;
	}
	if ( hostAddr ) {
		free( hostAddr );
		hostAddr = NULL;
	}
}


int 
ReliSock::listen()
{
	if (_state != sock_bound) return FALSE;

	// many modern OS's now support a >5 backlog, so we ask for 200,
	// but since we don't know how they behave when you ask for too
	// many, if 200 doesn't work we try 5 before returning failure
	if( ::listen( _sock, 200 ) < 0 ) {
		if( ::listen( _sock, 5 ) < 0 ) {
			return FALSE;
		}
	}

	dprintf( D_NETWORK, "LISTEN %s fd=%d\n", sock_to_string(_sock),
			 _sock );

	_state = sock_special;
	_special_state = relisock_listen;

	return TRUE;
}


int 
ReliSock::accept( ReliSock	&c )
{
	int c_sock;
	SOCKET_LENGTH_TYPE addr_sz;

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

	addr_sz = sizeof(c._who);
#ifndef WIN32 /* Unix */
	errno = 0;
#endif
	if ((c_sock = ::accept(_sock, (sockaddr *)&c._who, &addr_sz)) < 0) {
#ifndef WIN32 /* Unix */
		if ( errno == EMFILE ) {
			_condor_fd_panic ( __LINE__, __FILE__ ); /* This calls dprintf_exit! */
		}
#endif
		return FALSE;

	}

	c._sock = c_sock;
	c._state = sock_connect;
	c.decode();

	int on = 1;
	c.setsockopt(SOL_SOCKET, SO_KEEPALIVE, (char*)&on, sizeof(on));

	if( DebugFlags & D_NETWORK ) {
		char* src = strdup(	sock_to_string(_sock) );
		char* dst = strdup( sin_to_string(c.endpoint()) );
		dprintf( D_NETWORK, "ACCEPT src=%s fd=%d dst=%s\n",
				 src, c._sock, dst );
		free( src );
		free( dst );
	}
	
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
	int c_sock;

	if (!(c_rs = new ReliSock())) {
		return (ReliSock *)0;
	}

	if ((c_sock = accept(*c_rs)) < 0) {
		delete c_rs;
		return (ReliSock *)0;
	}

	return c_rs;
}

int 
ReliSock::connect( char	*host, int port, bool non_blocking_flag )
{
	is_client = 1;
	if( ! host ) {
		return FALSE;
	}
	hostAddr = strdup( host );
	return do_connect( host, port, non_blocking_flag );
}

int 
ReliSock::put_bytes_nobuffer( char *buf, int length, int send_size )
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
	if (i > 0) {
		_bytes_sent += i;
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
		_bytes_recvd += result;
		return result;
	}
}

int
ReliSock::get_file(const char *destination, bool flush_buffers)
{
	int nbytes, written, fd;
	char buf[65536];
	unsigned int filesize;
	unsigned int eom_num;
	unsigned int total = 0;

	if ( !get(filesize) || !end_of_message() ) {
		dprintf(D_ALWAYS, 
			"Failed to receive filesize in ReliSock::get_file\n");
		return -1;
	}
	// dprintf(D_FULLDEBUG,"get_file(): filesize=%d\n",filesize);

#if defined(WIN32)
	if ((fd = ::open(destination, O_WRONLY | O_CREAT | O_TRUNC | 
		_O_BINARY | _O_SEQUENTIAL, 0644)) < 0)
#else /* Unix */
	errno = 0;
	if ((fd = ::open(destination, O_WRONLY | O_CREAT | O_TRUNC, 0644)) < 0)
#endif
	{
#ifndef WIN32 /* Unix */
		if ( errno == EMFILE ) {
			_condor_fd_panic( __LINE__, __FILE__ ); /* This calls dprintf_exit! */
		}
#endif
		dprintf(D_ALWAYS, "get_file(): Failed to open file %s, errno = %d.\n",
				destination, errno);
		return -1;
	}

	while ((filesize == -1 || total < (int)filesize) &&
			(nbytes = get_bytes_nobuffer(buf, MIN(sizeof(buf),filesize-total),0)) > 0) {
		// dprintf(D_FULLDEBUG, "read %d bytes\n", nbytes);
		if ((written = ::write(fd, buf, nbytes)) < nbytes) {
			dprintf(D_ALWAYS, "failed to write %d bytes in ReliSock::get_file "
					"(only wrote %d, errno=%d)\n", nbytes, written, errno);
			::close(fd);
			unlink(destination);
			return -1;
		}
		// dprintf(D_FULLDEBUG, "wrote %d bytes\n", written);
		total += written;
	}

	if ( filesize == 0 ) {
		get(eom_num);
		if ( eom_num != 666 ) {
			dprintf(D_ALWAYS,"get_file: Zero-length file check failed!\n");
			::close(fd);
			unlink(destination);
			return -1;
		}			
	}

	if (flush_buffers) {
		fsync(fd);
	}
	
	dprintf(D_FULLDEBUG, "wrote %d bytes to file %s\n",
			total, destination);

	if (::close(fd) < 0) {
		dprintf(D_ALWAYS, "close failed in ReliSock::get_file\n");
		return -1;
	}

	if ( total < (int)filesize && filesize != -1 ) {
		dprintf(D_ALWAYS,"get_file(): ERROR: received %d bytes, expected %d!\n",
			total, filesize);
		unlink(destination);
		return -1;
	}

	return total;
}

int
ReliSock::put_file(const char *source)
{
	int fd;
	struct stat filestat;
	unsigned int filesize;
	unsigned int eom_num = 666;
	unsigned int total = 0;

#if defined(WIN32)
	if ((fd = ::open(source, O_RDONLY | _O_BINARY | _O_SEQUENTIAL, 0)) < 0)
#else
	char buf[65536];
	int nbytes, nrd;
	if ((fd = ::open(source, O_RDONLY, 0)) < 0)
#endif
	{
		dprintf(D_ALWAYS, "Failed to open file %s, errno = %d.\n", source, errno);
		return -1;
	}

	if (::fstat(fd, &filestat) < 0) {
		dprintf(D_ALWAYS, "fstat of %s failed\n", source);
		::close(fd);
		return -1;
	}

	filesize = filestat.st_size;
	if ( !put(filesize) || !end_of_message() ) {
		dprintf(D_ALWAYS, "Failed to send filesize in ReliSock::put_file\n");
		::close(fd);
		return -1;
	}
	
	if ( filesize > 0 ) {

#if defined(WIN32)
	// First drain outgoing buffers
	if ( !prepare_for_nobuffering(stream_encode) ) {
		dprintf(D_ALWAYS,"Relisock::put_file() failed to drain buffers!\n");
		::close(fd);
		return -1;
	}

	// Now transmit file using special optimized Winsock call
	// Only transfer if filesize > 0.
	if ( (filesize > 0) && (TransmitFile(_sock,(HANDLE)_get_osfhandle(fd),
			filesize,0,NULL,NULL,0) == FALSE) ) {
		dprintf(D_ALWAYS,"put_file: TransmitFile() failed, errno=%d\n",
			GetLastError() );
		::close(fd);
		return -1;
	} else {
		total = filesize;
	}
#else
	// On Unix, send file using put_bytes_nobuffer()
	while (total < filestat.st_size &&
		(nrd = ::read(fd, buf, sizeof(buf))) > 0) {
		// dprintf(D_FULLDEBUG, "read %d bytes\n", nrd);
		if ((nbytes = put_bytes_nobuffer(buf, nrd, 0)) < nrd) {
			dprintf(D_ALWAYS, "failed to put %d bytes in ReliSock::put_file "
				"(only wrote %d)\n", nrd, nbytes);
			::close(fd);
			return -1;
		}
		// dprintf(D_FULLDEBUG, "wrote %d bytes\n", nbytes);
		total += nbytes;
	}
	dprintf(D_FULLDEBUG, "done with transfer (errno = %d)\n", errno);
#endif
	
	} // end of if filesize > 0

	if ( filesize == 0 ) {
		put(eom_num);
	}

	dprintf(D_FULLDEBUG, "sent file %s (%d bytes)\n", source, total);

	if (::close(fd) < 0) {
		dprintf(D_ALWAYS, "close failed in ReliSock::put_file, errno = %d\n", errno);
		return -1;
	}

	if (total < (int)filesize) {
		dprintf(D_ALWAYS,"put_file(): ERROR, only sent %d bytes out of %d\n",
			total, filesize);
		return -1;
	}

	return total;
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
ReliSock::put_bytes(const void *dta, int sz)
{
	int		tw;
	int		nw;

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
	if (nw > 0) {
		_bytes_sent += nw;
	}
	return nw;
}


int 
ReliSock::get_bytes(void *dta, int max_sz)
{
	int		bytes;

	ignore_next_decode_eom = FALSE;

	while (!rcv_msg.ready) {
		if (!handle_incoming_packet()){
			return FALSE;
		}
	}

	bytes = rcv_msg.buf.get(dta, max_sz);
	if (bytes > 0) {
		_bytes_recvd += bytes;
	}
	return bytes;
}


int ReliSock::get_ptr( void *&ptr, char delim)
{
	while (!rcv_msg.ready){
		if (!handle_incoming_packet()) {
			return FALSE;
		}
	}

	return rcv_msg.buf.get_tmp(ptr, delim);
}


int ReliSock::peek( char &c)
{
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
	timeout(0);	// make certain in blocking mode
	return TRUE;
}
#endif

Stream::stream_type 
ReliSock::type() 
{ 
	return Stream::reli_sock; 
}

char * 
ReliSock::serialize() const
{
	// here we want to save our state into a buffer

	// first, get the state from our parent class
	char * parent_state = Sock::serialize();
	// now concatenate our state
	char * outbuf = new char[50];
	sprintf(outbuf,"*%d*%s",_special_state,sin_to_string(&_who));
	strcat(parent_state,outbuf);
	delete []outbuf;
	return( parent_state );
}

char * 
ReliSock::serialize(char *buf)
{
	char sinful_string[28];
	char *ptmp;
	
	assert(buf);

	// here we want to restore our state from the incoming buffer

	// first, let our parent class restore its state
	ptmp = Sock::serialize(buf);
	assert( ptmp );
	sscanf(ptmp,"%d*%s",&_special_state,sinful_string);
	string_to_sin(sinful_string, &_who);

	return NULL;
}

int 
ReliSock::prepare_for_nobuffering(stream_coding direction)
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

int 
ReliSock::authenticate() {
	if ( !authob ) {
		authob = new Authentication( this );
	}
	if ( authob ) {
		return( authob->authenticate( hostAddr ) );
	}
	return( 0 );
}

void
ReliSock::setOwner( const char *newOwner ) {
	if ( authob ) {
		authob->setOwner( newOwner );
	}
}

const char *
ReliSock::getOwner() {
	if ( authob ) {
		return( authob->getOwner() );
	}
	return NULL;
}

int
ReliSock::isAuthenticated()
{
	if ( !authob ) {
		dprintf(D_FULLDEBUG, "authentication not called prev, auth'ing TRUE\n" );
		return 1;
	}
	return( authob->isAuthenticated() );
	return 0;
}

void
ReliSock::unAuthenticate()
{
	if ( authob ) {
		authob->unAuthenticate();
	}
}

int ReliSock :: encrypt(bool flag)
{
	if ( authob ){
		return(authob -> encrypt( flag ));
	}
	return -1;
}


int ReliSock :: hdr_encrypt()
{
	if ( authob ){
		return(authob -> hdr_encrypt());
	}
	return -1;
}

bool ReliSock :: is_hdr_encrypt()
{
	if ( authob ){
		return(authob -> is_hdr_encrypt());
	}
	return FALSE;
}


bool ReliSock :: is_encrypt()
{
	if ( authob ){
		return(authob -> is_encrypt());
	}
	return FALSE;
}

int  ReliSock :: wrap(char* d_in,int l_in,char*& d_out,int& l_out)
{
	if ( authob ){
		return(authob -> wrap(d_in,l_in,d_out,l_out));
	}
	return FALSE;
}

int ReliSock :: unwrap(char* d_in,int l_in,char*& d_out , int& l_out)
{
	if ( authob ){
		return(authob -> unwrap(d_in,l_in,d_out,l_out));
	}
	return FALSE;
}


