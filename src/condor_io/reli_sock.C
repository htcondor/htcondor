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
#include "authentication.h"
#include "condor_debug.h"
#include "internet.h"
#include "condor_rw.h"
#include "condor_socket_types.h"
#include "condor_md.h"
#include "stat_wrapper.h"

#ifdef WIN32
#include <mswsock.h>	// For TransmitFile()
#endif

#define NORMAL_HEADER_SIZE 5
#define MAX_HEADER_SIZE MAC_SIZE + NORMAL_HEADER_SIZE
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
    fqu_ = NULL;
	snd_msg.buf.reset();                                                    
	rcv_msg.buf.reset();   
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
	ASSERT(buf);
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

    if (fqu_) {
        free( fqu_ );
        fqu_ = NULL;
    }
}


int 
ReliSock::listen()
{
	if (_state != sock_bound) return FALSE;

	// many modern OS's now support a >5 backlog, so we ask for 500,
	// but since we don't know how they behave when you ask for too
	// many, if 200 doesn't work we try progressively smaller numbers.
	// you may ask, why not just use SOMAXCONN ?  unfortunately,
	// it is not correct on several platforms such as Solaris, which
	// accepts an unlimited number of socks but sets SOMAXCONN to 5.
	if( ::listen( _sock, 500 ) < 0 ) {
		if( ::listen( _sock, 300 ) < 0 ) 
		if( ::listen( _sock, 200 ) < 0 ) 
		if( ::listen( _sock, 100 ) < 0 ) 
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

bool ReliSock :: set_encryption_id(const char * keyId)
{
    return false; // TCP does not need this yet
}

bool ReliSock::init_MD(CONDOR_MD_MODE mode, KeyInfo * key, const char * keyId)
{
    return (snd_msg.init_MD(mode, key) && rcv_msg.init_MD(mode, key));
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
	if (authob) {                                                           
		delete authob;                                                  
		authob = NULL;
	}  	                                                                     

	if (hostAddr != NULL)
	{
		free(hostAddr);
		hostAddr = NULL;
	}
 
	init();     
	is_client = 1;
	if( ! host ) {
		return FALSE;
	}
	hostAddr = strdup( host );
	return do_connect( host, port, non_blocking_flag );
}

int 
ReliSock::put_line_raw( char *buffer )
{
	int result;
	int length = strlen(buffer);
	result = put_bytes_raw(buffer,length);
	if(result!=length) return -1;
	result = put_bytes_raw("\n",1);
	if(result!=1) return -1;
	return length;
}

int
ReliSock::get_line_raw( char *buffer, int length )
{
	int total=0;
	int actual;

	while( length>0 ) {
		actual = get_bytes_raw(buffer,1);
		if(actual<=0) break;
		if(*buffer=='\n') break;

		buffer++;
		length--;
		total++;
	}
	
	*buffer = 0;
	return total;
}

int 
ReliSock::put_bytes_raw( char *buffer, int length )
{
	return condor_write(_sock,buffer,length,_timeout);
}

int 
ReliSock::get_bytes_raw( char *buffer, int length )
{
	return condor_read(_sock,buffer,length,_timeout);
}

int 
ReliSock::put_bytes_nobuffer( char *buffer, int length, int send_size )
{
	int i, result, l_out;
	int pagesize = 65536;  // Optimize large writes to be page sized.
	unsigned char * cur;
	unsigned char * buf = NULL;
        
	// First, encrypt the data if necessary
	if (get_encryption()) {
		if (!wrap((unsigned char *) buffer, length,  buf , l_out)) { 
			dprintf(D_SECURITY, "Encryption failed\n");
			goto error;
		}
	}
	else {
		buf = (unsigned char *) malloc(length);
		memcpy(buf, buffer, length);
	}

	cur = buf;

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
            goto error;
	}

	// Optimize transfer by writing in pagesized chunks.
	for(i = 0; i < length;)
	{
		// If there is less then a page left.
		if( (length - i) < pagesize ) {
			result = condor_write(_sock, (char *)cur, (length - i), _timeout);
			if( result < 0 ) {
                                goto error;
			}
			cur += (length - i);
			i += (length - i);
		} else {  
			// Send another page...
			result = condor_write(_sock, (char *)cur, pagesize, _timeout);
			if( result < 0 ) {
                            goto error;
			}
			cur += pagesize;
			i += pagesize;
		}
	}
	if (i > 0) {
		_bytes_sent += i;
	}
        
        free(buf);

	return i;
 error:
        dprintf(D_ALWAYS, "ReliSock::put_bytes_nobuffer: Send failed.\n");

        free(buf);

        return -1;
}

int 
ReliSock::get_bytes_nobuffer(char *buffer, int max_length, int receive_size)
{
	int result;
	int length;
    unsigned char * buf = NULL;

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
            goto error;
	}


	if( length > max_length ) {
		dprintf(D_ALWAYS, 
			"ReliSock::get_bytes_nobuffer: data too large for buffer.\n");
                goto error;
	}

	result = condor_read(_sock, buffer, length, _timeout);

	
	if( result < 0 ) {
		dprintf(D_ALWAYS, 
			"ReliSock::get_bytes_nobuffer: Failed to receive file.\n");
                goto error;
	} 
	else {
		// See if it needs to be decrypted
		if (get_encryption()) {
			unwrap((unsigned char *) buffer, result, buf, length);  // I am reusing length
			memcpy(buffer, buf, result);
			free(buf);
		}
		_bytes_recvd += result;
		return result;
	}
 error:
        return -1;
}

int
ReliSock::get_file( filesize_t *size, const char *destination, bool flush_buffers)
{
	int fd;
	int result;

	// Open the file
	errno = 0;
	fd = ::open( destination,
				 O_WRONLY | O_CREAT | O_TRUNC |
				 _O_BINARY | _O_SEQUENTIAL | O_LARGEFILE,
				 0600 );

	// Handle open failure; it's bad....
	if ( fd < 0 )
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

	dprintf(D_FULLDEBUG,"get_file(): going to write to filename %s\n",
		destination);

	result = get_file( size, fd,flush_buffers);

	if(::close(fd)!=0) {
		dprintf(D_ALWAYS, "close failed in ReliSock::get_file\n");
		return -1;
	}

	if(result<0) unlink(destination);
	
	return result;
}

int
ReliSock::get_file( filesize_t *size, int fd, bool flush_buffers )
{
	char buf[65536];
	filesize_t filesize;
	unsigned int eom_num;
	filesize_t total = 0;

	// Read the filesize from the other end of the wire
	if ( !get(filesize) || !end_of_message() ) {
		dprintf(D_ALWAYS, 
			"Failed to receive filesize in ReliSock::get_file\n");
		return -1;
	}


	// Log what's going on
	dprintf(D_FULLDEBUG,
			"get_file: Receiving " FILESIZE_T_FORMAT " bytes\n", filesize );

		/*
		  the code used to check for filesize == -1 here, but that's
		  totally wrong.  we're storing the size as an unsigned int,
		  so we can't check it against a negative value.  furthermore,
		  ReliSock::put_file() never puts a -1 on the wire for the
		  size.  that's legacy code from the pseudo_put_file_stream()
		  RSC in the syscall library.  this code isn't like that.
		*/

	// Now, read it all in & save it
	while( total < filesize ) {
		int	iosize = (int) MIN( (filesize_t) sizeof(buf),
									filesize - total );
		int	nbytes = get_bytes_nobuffer( buf, iosize, 0 );
		if ( nbytes <= 0 ) {
			break;
		}

		int rval;
		int written;
		for( written=0; written<nbytes; ) {
			rval = ::write( fd, &buf[written], (nbytes-written) );
			if( rval < 0 ) {
				dprintf( D_ALWAYS,
						 "ReliSock::get_file: write() returned %d: %s "
						 "(errno=%d)\n", rval, strerror(errno), errno );
				return -1;
			} else if( rval == 0 ) {
					/*
					  write() shouldn't really return 0 at all.
					  apparently it can do so if we asked it to write
					  0 bytes (which we're not going to do) or if the
					  file is closed (which we're also not going to
					  do).  so, for now, if we see it, we want to just
					  break out of this loop.  in the future, we might
					  do more fancy stuff to handle this case, but
					  we're probably never going to see this anyway.
					*/
				dprintf( D_ALWAYS, "ReliSock::get_file: write() returned 0: "
						 "wrote %d out of %d bytes (errno=%d %s)\n",
						 written, nbytes, errno, strerror(errno) );
				break;
			} else {
				written += rval;
			}
		}
		total += written;
	}

	if ( filesize == 0 ) {
		get(eom_num);
		if ( eom_num != 666 ) {
			dprintf(D_ALWAYS,"get_file: Zero-length file check failed!\n");
			return -1;
		}			
	}

	if (flush_buffers) {
		fsync(fd);
	}

	dprintf(D_FULLDEBUG,
			"get_file: wrote " FILESIZE_T_FORMAT " bytes to file\n", total );

	if ( total < filesize ) {
		dprintf( D_ALWAYS,
				 "get_file(): ERROR: received %d bytes, expected %d!\n",
				 total, filesize);
		return -1;
	}

	*size = total;
	return 0;
}

int
ReliSock::put_file( filesize_t *size, const char *source)
{
	int fd;
	int result;

	// Open the file, handle failure
	fd = ::open(source, O_RDONLY | O_LARGEFILE | _O_BINARY | _O_SEQUENTIAL, 0);
	if ( fd < 0 )
	{
		dprintf(D_ALWAYS,
				"ReliSock: put_file: Failed to open file %s, errno = %d.\n",
				source, errno);
		return -1;
	}

	dprintf(D_FULLDEBUG,"put_file: going to send from filename %s\n",
		source);

	result = put_file( size, fd);

	if (::close(fd) < 0) {
		dprintf(D_ALWAYS, "ReliSock: put_file: close failed, errno = %d\n", errno);
		return -1;
	}

	return result;
}

int
ReliSock::put_file( filesize_t *size, int fd )
{
	filesize_t	filesize;
	filesize_t	total = 0;
	unsigned int eom_num = 666;


	StatWrapper	filestat( fd );
	if ( filestat.GetStatus() ) {
		int		staterr = filestat.GetErrno( );
		dprintf(D_ALWAYS, "ReliSock: put_file: StatBuf failed: %d %s\n",
				staterr, strerror( staterr ) );
		return -1;
	}
	filesize = filestat.GetStatBuf( )->st_size;
	dprintf( D_FULLDEBUG, "put_file: Found file size %lld\n", filesize );

	// Send the file size to the reciever
	if ( !put(filesize) || !end_of_message() ) {
		dprintf(D_ALWAYS, "ReliSock: put_file: Failed to send filesize.\n");
		return -1;
	}

	// Log what's going on
	dprintf(D_FULLDEBUG,
			"put_file: senting " FILESIZE_T_FORMAT " bytes\n", filesize);

	// If the file has a non-zero size, send it
	if ( filesize > 0 ) {

#if defined(WIN32)
		// On Win32, if we don't need encryption, use the super-efficient Win32
		// TransmitFile system call. Also, TransmitFile does not support
		// file sizes over 2GB, so we avoid that case as well.
		if ( !get_encryption() && filesize < INT_MAX ) {

			// First drain outgoing buffers
			if ( !prepare_for_nobuffering(stream_encode) ) {
				dprintf(D_ALWAYS,
						"ReliSock: put_file: failed to drain buffers!\n");
				return -1;
			}

			// Now transmit file using special optimized Winsock call
			if ( TransmitFile(_sock,(HANDLE)_get_osfhandle(fd),
							  filesize,0,NULL,NULL,0) == FALSE ) {
				dprintf(D_ALWAYS,
						"ReliSock: put_file: TransmitFile() failed, errno=%d\n",
						GetLastError() );
				return -1;
			} else {
				// Note that it's been sent, so that we don't try to below
				total = filesize;
			}
		}
#endif

		char buf[65536];
		int nbytes, nrd;

		// On Unix, always send the file using put_bytes_nobuffer().
		// Note that on Win32, we use this method as well if encryption 
		// is required.
		while (total < filesize &&
			   (nrd = ::read(fd, buf, sizeof(buf))) > 0) {
			if ((nbytes = put_bytes_nobuffer(buf, nrd, 0)) < nrd) {
					// put_bytes_nobuffer() does the appropriate
					// looping for us already, the only way this could
					// return less than we asked for is if it returned
					// -1 on failure.
				ASSERT( nbytes == -1 );
				dprintf( D_ALWAYS, "ReliSock::put_file: failed to put %d "
						 "bytes (put_bytes_nobuffer() returned %d)\n",
						 nrd, nbytes );
				return -1;
			}
			total += nbytes;
		}
	
	} // end of if filesize > 0

	if ( filesize == 0 ) {
		put(eom_num);
	}

	dprintf(D_FULLDEBUG,
			"ReliSock: put_file: sent " FILESIZE_T_FORMAT " bytes\n", total);

	if (total < filesize) {
		dprintf(D_ALWAYS,"ReliSock: put_file: only sent %d bytes out of %d\n",
			total, filesize);
		return -1;
	}

	*size = filesize;
	return 0;
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

    resetCrypto();
	switch(_coding){
		case stream_encode:
			if ( ignore_next_encode_eom == TRUE ) {
				ignore_next_encode_eom = FALSE;
				return TRUE;
			}
			if (!snd_msg.buf.empty()) {
				return snd_msg.snd_packet(_sock, TRUE, _timeout);
			}
			if ( allow_empty_message_flag ) {
				allow_empty_message_flag = FALSE;
				return TRUE;
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
			if ( allow_empty_message_flag ) {
				allow_empty_message_flag = FALSE;
				return TRUE;
			}
			break;

		default:
			ASSERT(0);
	}

	return ret_val;
}


const char * ReliSock :: isIncomingDataMD5ed()
{
    return NULL;    // For now
}

int 
ReliSock::put_bytes(const void *data, int sz)
{
	int		tw, header_size = isOutgoing_MD5_on() ? MAX_HEADER_SIZE:NORMAL_HEADER_SIZE;
	int		nw, l_out;
        unsigned char * dta = NULL;

        // Check to see if we need to encrypt
        // Okay, this is a bug! H.W. 9/25/2001
        if (get_encryption()) {
            if (!wrap((unsigned char *)data, sz, dta , l_out)) { 
                dprintf(D_SECURITY, "Encryption failed\n");
				if (dta != NULL)
				{
					free(dta);
					dta = NULL;
				}
                return -1;  // encryption failed!
            }
        }
        else {
            dta = (unsigned char *) malloc(sz);
            memcpy(dta, data, sz);
        }

	ignore_next_encode_eom = FALSE;

	for(nw=0;;) {
		
		if (snd_msg.buf.full()) {
			if (!snd_msg.snd_packet(_sock, FALSE, _timeout)) {
				if (dta != NULL)
				{
					free(dta);
					dta = NULL;
				}
				return FALSE;
			}
		}
		
		if (snd_msg.buf.empty()) {
			snd_msg.buf.seek(header_size);
		}
		
		if ((tw = snd_msg.buf.put_max(&((char *)dta)[nw], sz-nw)) < 0) {
					if (dta != NULL)
					{
                    	free(dta);
						dta = NULL;
					}
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

	if (dta != NULL)
	{
    	free(dta);
		dta = NULL;
	}

	return nw;
}


int 
ReliSock::get_bytes(void *dta, int max_sz)
{
	int		bytes, length;
    unsigned char * data = 0;

	ignore_next_decode_eom = FALSE;

	while (!rcv_msg.ready) {
		if (!handle_incoming_packet()){
			return FALSE;
		}
	}

	bytes = rcv_msg.buf.get(dta, max_sz);

	if (bytes > 0) {
            if (get_encryption()) {
                unwrap((unsigned char *) dta, bytes, data, length);
                memcpy(dta, data, bytes);
                free(data);
            }
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

bool ReliSock::RcvMsg::init_MD(CONDOR_MD_MODE mode, KeyInfo * key)
{
    if (!buf.consumed()) {
        return false;
    }

    mode_ = mode;
    delete mdChecker_;
	mdChecker_ = 0;

    if (key) {
        mdChecker_ = new Condor_MD_MAC(key);
    }

    return true;
}

ReliSock::RcvMsg :: RcvMsg() : 
    mode_(MD_OFF),
    mdChecker_(0), 
    ready(0) 
{
}

ReliSock::RcvMsg::~RcvMsg()
{
    delete mdChecker_;
}

int ReliSock::RcvMsg::rcv_packet( SOCKET _sock, int _timeout)
{
	Buf		*tmp;
	char	        hdr[MAX_HEADER_SIZE];
	int		end;
	int		len, len_t, header_size;
	int		tmp_len;

	header_size = (mode_ != MD_OFF) ? MAX_HEADER_SIZE : NORMAL_HEADER_SIZE;

	if ( condor_read(_sock,hdr,header_size,_timeout) < 0 ) {
			// condor_read() already does dprintfs into the log
			// about what went wrong...
		return FALSE;
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

        // Now, check MD
        if (mode_ != MD_OFF) {
            if (!tmp->verifyMD(&hdr[5], mdChecker_)) {
                delete tmp;
                dprintf(D_ALWAYS, "IO: Message Digest/MAC verification failed!\n");
                return FALSE;  // or something other than this
            }
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


ReliSock::SndMsg::SndMsg() : 
    mode_(MD_OFF), 
    mdChecker_(0) 
{
}

ReliSock::SndMsg::~SndMsg() 
{
    delete mdChecker_;
}

int ReliSock::SndMsg::snd_packet( int _sock, int end, int _timeout )
{
	char	        hdr[MAX_HEADER_SIZE];
	int		len, header_size;
	int		ns;

    header_size = (mode_ != MD_OFF) ? MAX_HEADER_SIZE : NORMAL_HEADER_SIZE;
	hdr[0] = (char) end;
	ns = buf.num_used() - header_size;
	len = (int) htonl(ns);

	memcpy(&hdr[1], &len, 4);

        if (mode_ != MD_OFF) {
            if (!buf.computeMD(&hdr[5], mdChecker_)) {
                dprintf(D_ALWAYS, "IO: Failed to compute Message Digest/MAC\n");
                return FALSE;
            }
        }

        if (buf.flush(_sock, hdr, header_size, _timeout) != (ns+header_size)){
            return FALSE;
        }
        
	return TRUE;
}

bool ReliSock::SndMsg::init_MD(CONDOR_MD_MODE mode, KeyInfo * key)
{
    if (!buf.empty()) {
        return false;
    }

    mode_ = mode;
    delete mdChecker_;
	mdChecker_ = 0;

    if (key) {
        mdChecker_ = new Condor_MD_MAC(key);
    }

    return true;
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
    memset(outbuf, 0, 50);
	sprintf(outbuf,"*%d*%s*",_special_state,sin_to_string(&_who));
	strcat(parent_state,outbuf);

    // Serialize crypto stuff
	char * crypto = serializeCryptoInfo();
    strcat(parent_state, crypto);
    strcat(parent_state, "*");

    // serialize MD info
    char * md = serializeMdInfo();
    strcat(parent_state, md);
    strcat(parent_state, "*");

    const char * tmp = getFullyQualifiedUser();
    char buf[5];
    int len = 0;
    if (tmp) {
        len = strlen(tmp);
        sprintf(buf, "%d*", len);
        strcat(parent_state, buf);
        strcat(parent_state, tmp);
        strcat(parent_state, "*");
    }
    else {
        sprintf(buf, "%d*", 0);
        strcat(parent_state, buf);
    }

	delete []outbuf;
    delete []crypto;
    delete []md;
	return( parent_state );
}

char * 
ReliSock::serialize(char *buf)
{
	char sinful_string[28], fqu[256];
	char *ptmp, * ptr = NULL;
	int len = 0;

    ASSERT(buf);
    memset(fqu, 0, 256);
    memset(sinful_string, 0 , 28);

	// here we want to restore our state from the incoming buffer
	if (fqu_ != NULL) {
		free(fqu_);
	}
    fqu_ = NULL;

	// first, let our parent class restore its state
    ptmp = Sock::serialize(buf);
    ASSERT( ptmp );

    sscanf(ptmp,"%d*",(int*)&_special_state);
    // skip through this
    ptmp = strchr(ptmp, '*');
    ptmp++;
    // Now, see if we are 6.3 or 6.2
    if ((ptr = strchr(ptmp, '*')) != NULL) {
        // we are 6.3
        memcpy(sinful_string, ptmp, ptr - ptmp);

        ptmp = ++ptr;
        // The next part is for crypto
        ptmp = serializeCryptoInfo(ptmp);
        // Followed by Md
        ptmp = serializeMdInfo(ptmp);

        sscanf(ptmp, "%d*", &len);
        
        if (len > 0) {
            ptmp = strchr(ptmp, '*');
            ptmp++;
            memcpy(fqu, ptmp, len);
            if ((fqu[0] != ' ') && (fqu[0] != '\0')) {
                if (authob && (authob->getFullyQualifiedUser() != NULL)) {
                    // odd situation!
                    dprintf(D_SECURITY, "WARNING!!!! Trying to serialize a socket for user %s but the socket is identified with another user: %s", fqu, authob->getFullyQualifiedUser());
                }
                else {
                    // We are cozy
                    fqu_ = strdup(fqu);
                }
            }
        }
    }
    else {
        // we are 6.2, this is the end of it.
        sscanf(ptmp,"%s",sinful_string);
    }
    
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
			ASSERT(0);
	}

	return ret_val;
}

int ReliSock::perform_authenticate(bool with_key, KeyInfo *& key, 
								   const char* methods, CondorError* errstack)
{
	int in_encode_mode;
	int result;

    if (!isAuthenticated()) {
        if ( !authob ) {
            authob = new Authentication( this );
        }
        if ( authob ) {
				// store if we are in encode or decode mode
			in_encode_mode = is_encode();

				// actually perform the authentication
			if ( with_key ) {
				result = authob->authenticate( hostAddr, key, methods, errstack );
			} else {
				result = authob->authenticate( hostAddr, methods, errstack );
			}
				// restore stream mode (either encode or decode)
			if ( in_encode_mode && is_decode() ) {
				encode();
			} else {
				if ( !in_encode_mode && is_encode() ) { 
					decode();
				}
			}

			return result;
        }
        return( 0 );  
    }
    else {
        return 1;
    }
}

int ReliSock::authenticate(KeyInfo *& key, const char* methods, CondorError* errstack)
{
	return perform_authenticate(true,key,methods,errstack);
}

int 
ReliSock::authenticate(const char* methods, CondorError* errstack ) 
{
	KeyInfo *key = NULL;
	return perform_authenticate(false,key,methods,errstack);
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

const char *
ReliSock::getFullyQualifiedUser() const {
	if ( authob ) {
		return( authob->getFullyQualifiedUser() );
	}
    else if (fqu_) {
        return fqu_;
    }
	return NULL;
}

const char * ReliSock::getDomain()
{
    if (authob) {
        return ( authob->getDomain() );
    }
    return NULL;
}

const char * ReliSock::getHostAddress() {
    if (authob) {
        return ( authob->getRemoteAddress() );
    }
    return NULL;
}

int
ReliSock::isAuthenticated()
{
	if ( !authob ) {
		return 0;
	}
	return( authob->isAuthenticated() );
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

