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
#include "authentication.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "internet.h"
#include "condor_rw.h"
#include "condor_socket_types.h"
#include "condor_md.h"
#include "selector.h"
#include "ccb_client.h"
#include "condor_sockfunc.h"

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
	m_has_backlog = false;
	m_read_would_block = false;
	m_non_blocking = false;
	ignore_next_encode_eom = FALSE;
	ignore_next_decode_eom = FALSE;
	_bytes_sent = 0.0;
	_bytes_recvd = 0.0;
	_special_state = relisock_none;
	is_client = 0;
	hostAddr = NULL;
	snd_msg.buf.reset();                                                    
	rcv_msg.buf.reset();   
	rcv_msg.init_parent(this);
	snd_msg.init_parent(this);
	m_target_shared_port_id = NULL;
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

Stream *
ReliSock::CloneStream()
{
	return new ReliSock(*this);
}

ReliSock::~ReliSock()
{
	close();
	if ( hostAddr ) {
		free( hostAddr );
		hostAddr = NULL;
	}
	if( m_target_shared_port_id ) {
		free( m_target_shared_port_id );
		m_target_shared_port_id = NULL;
	}
}


int 
ReliSock::listen()
{
	if (_state != sock_bound) {
        dprintf(D_ALWAYS, "Failed to listen on TCP socket, because it is not bound to a port.\n");
        return FALSE;
    }

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

            char const *self_address = get_sinful();
            if( !self_address ) {
                self_address = "<bad address>";
            }
#ifdef WIN32
			int error = WSAGetLastError();
			dprintf( D_ALWAYS, "Failed to listen on TCP socket %s: WSAError = %d\n", self_address, error );
#else
			dprintf(D_ALWAYS, "Failed to listen on TCP socket %s: (errno = %d) %s\n", self_address, errno, strerror(errno));
#endif

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

	if (_state != sock_special || _special_state != relisock_listen ||
													c._state != sock_virgin)
	{
		return FALSE;
	}

	if (_timeout > 0) {
		Selector		selector;
		selector.set_timeout( _timeout );
		selector.add_fd( _sock, Selector::IO_READ );

		selector.execute();

		if( selector.timed_out() ) {
			return FALSE;
		} else if ( !selector.has_ready() ) {
			dprintf( D_ALWAYS, "select returns %d, connect failed\n",
				selector.select_retval() );
			return FALSE;
		}
	}

#ifndef WIN32 /* Unix */
	errno = 0;
#endif
	if ((c_sock = condor_accept(_sock, c._who)) < 0) {
#ifndef WIN32 /* Unix */
		if ( errno == EMFILE ) {
			_condor_fd_panic ( __LINE__, __FILE__ ); /* This calls dprintf_exit! */
		}
#endif
		return FALSE;

	}

	c.assign(c_sock);
	c.enter_connected_state("ACCEPT");
	c.decode();

	c.set_keepalive();

		/* Set no delay to disable Nagle, since we buffer all our
		   relisock output and it degrades performance of our
		   various chatty protocols. -Todd T, 9/05
		*/
	int on = 1;
	c.setsockopt(IPPROTO_TCP, TCP_NODELAY, (char*)&on, sizeof(on));

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

bool ReliSock :: set_encryption_id(const char * /* keyId */)
{
    return false; // TCP does not need this yet
}

bool ReliSock::init_MD(CONDOR_MD_MODE mode, KeyInfo * key, const char * /* keyId */)
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

	if ((c_sock = accept(*c_rs)) == FALSE) {
		delete c_rs;
		return (ReliSock *)0;
	}

	return c_rs;
}

int 
ReliSock::connect( char	const *host, int port, bool non_blocking_flag )
{
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
	result = put_bytes_raw("\n", 1);
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
ReliSock::put_bytes_raw( const char *buffer, int length )
{
	return condor_write(peer_description(),_sock,buffer,length,_timeout);
}

int 
ReliSock::get_bytes_raw( char *buffer, int length )
{
	return condor_read(peer_description(),_sock,buffer,length,_timeout);
}

int 
ReliSock::put_bytes_nobuffer( char *buffer, int length, int send_size )
{
	int i, result, l_out;
	int pagesize = 65536;  // Optimize large writes to be page sized.
	char * cur;
	unsigned char * buf = NULL;
        
	// First, encrypt the data if necessary
	if (get_encryption()) {
		if (!wrap((unsigned char *) buffer, length,  buf , l_out)) { 
			dprintf(D_SECURITY, "Encryption failed\n");
			goto error;
		}
		cur = (char *)buf;
	}
	else {
		cur = buffer;
	}

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
			result = condor_write(peer_description(), _sock, cur, (length - i), _timeout);
			if( result < 0 ) {
                                goto error;
			}
			cur += (length - i);
			i += (length - i);
		} else {  
			// Send another page...
			result = condor_write(peer_description(), _sock, cur, pagesize, _timeout);
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

	result = condor_read(peer_description(), _sock, buffer, length, _timeout);

	
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
ReliSock::handle_incoming_packet()
{
	/* if socket is listening, and packet is there, it is ready for accept */
	if (_state == sock_special && _special_state == relisock_listen) {
		return TRUE;
	}

		// since we are trying to read from the socket, we can assume
		// that it is no longer ok for there to be no message at all.
	allow_empty_message_flag = FALSE;

	/* do not queue up more than one message at a time on reliable sockets */
	/* but return 1, because old message can still be read.						*/
	if (rcv_msg.ready) {
		return TRUE;
	}

	return rcv_msg.rcv_packet(peer_description(), _sock, _timeout);
}

int
ReliSock::finish_end_of_message()
{
	BlockingModeGuard guard(this, true);
	int retval = snd_msg.finish_packet(peer_description(), _sock, _timeout);
	if (retval == 2) m_has_backlog = true;
	return retval;
}

	// Ret values:
	// - TRUE: successfully sent EOM; if set to ignore next EOM, we did nothing.
	// - FALSE: failure occurred when sending EOM.
	// - 2: When encoding mode and non-blocking is enabled, this indicates that
	//   a subset of the data successfully went to the wire but finish_end_of_message
	//   needs to be called.
	// If allow_empty_message is set and there are no bytes to send, return TRUE.
	// Otherwise, an error occurs if there is no unbuffer bytes.
int
ReliSock::end_of_message_nonblocking()
{
	BlockingModeGuard guard(this, true);
	return end_of_message_internal();
}

int
ReliSock::end_of_message()
{
	BlockingModeGuard guard(this, false);
	return end_of_message_internal();
}

int 
ReliSock::end_of_message_internal()
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
				int retval = snd_msg.snd_packet(peer_description(), _sock, TRUE, _timeout);
				if (retval == 2 || retval == 3) {
					m_has_backlog = true;
				}
				return retval ? true : false;
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
				if ( rcv_msg.buf.consumed() ) {
					ret_val = TRUE;
				}
				else {
					char const *ip = get_sinful_peer();
					dprintf(D_FULLDEBUG,"Failed to read end of message from %s.\n",ip ? ip : "(null)");
				}
				rcv_msg.ready = FALSE;
				rcv_msg.buf.reset();
			}
			else if ( allow_empty_message_flag ) {
				allow_empty_message_flag = FALSE;
				return TRUE;
			}
			allow_empty_message_flag = FALSE;
			break;

		default:
			ASSERT(0);
	}

	return ret_val;
}

bool
ReliSock::peek_end_of_message()
{
	if ( rcv_msg.ready ) {
		if ( rcv_msg.buf.consumed() ) {
			return true;
		}
	}
	return false;
}

const char * ReliSock :: isIncomingDataMD5ed()
{
    return NULL;    // For now
}

int 
ReliSock::put_bytes(const void *data, int sz)
{
	int		tw=0, header_size = isOutgoing_MD5_on() ? MAX_HEADER_SIZE:NORMAL_HEADER_SIZE;
	int		nw, l_out;
        unsigned char * dta = NULL;

        // Check to see if we need to encrypt
        // Okay, this is a bug! H.W. 9/25/2001
        if (get_encryption()) {
            if (!wrap((unsigned char *)const_cast<void*>(data), sz, dta , l_out)) { 
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
            if((dta = (unsigned char *) malloc(sz)) != 0)
		memcpy(dta, data, sz);
        }

	ignore_next_encode_eom = FALSE;

	for(nw=0;;) {
		
		if (snd_msg.buf.full()) {
			int retval = snd_msg.snd_packet(peer_description(), _sock, FALSE, _timeout);
			// This would block and the user asked us to work non-buffered - force the
			// buffer to grow to hold the data for now.
			if (retval == 3) {
				nw = snd_msg.buf.put_force(&((char *)dta)[nw], sz-nw);
				m_has_backlog = true;
				nw += tw;
				break;
			} else if (!retval) {
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
		
		if (dta && (tw = snd_msg.buf.put_max(&((char *)dta)[nw], sz-nw)) < 0) {
			free(dta);
		dta = NULL;
			return -1;
		}
		
		nw += tw;
		if (nw >= sz) {
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

	m_read_would_block = false;
	while (!rcv_msg.ready) {
		int retval = handle_incoming_packet();
		if (retval == 2) {
			m_read_would_block = true;
			return false;
		} else if (!retval) {
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
	p_sock(0),
	m_partial_packet(false),
	m_remaining_read_length(0),
	m_end(0),
	m_tmp(NULL),
	ready(0)
{
}

ReliSock::RcvMsg::~RcvMsg()
{
    delete mdChecker_;
}

int ReliSock::RcvMsg::rcv_packet( char const *peer_description, SOCKET _sock, int _timeout)
{
	Buf		*tmp;
	char	        hdr[MAX_HEADER_SIZE];
	int		len, len_t, header_size;
	int		tmp_len;
	int		retval;

	// We read the partial packet in a previous read; try to finish it and
	// then skip down to packet verification.
	if (m_partial_packet) {
		m_partial_packet = false;
		len = m_remaining_read_length;
		goto read_packet;
	}

	header_size = (mode_ != MD_OFF) ? MAX_HEADER_SIZE : NORMAL_HEADER_SIZE;

	retval = condor_read(peer_description,_sock,hdr,header_size,_timeout, p_sock->is_non_blocking());
        if ( retval == -3 ) {   // -3 means that the read would have blocked; 0 bytes were read
		return 2;
        }
	// Block on short reads for the header.  Since the header is very short (typically, 5 bytes),
	// we don't care to gracefully handle the case where it has been fragmented over multiple
	// TCP packets.
	if ( (retval > 0) && (retval != header_size) ) {
		retval = condor_read(peer_description, _sock, hdr+retval, header_size-retval, _timeout);
	}

	if ( retval < 0 && 
		 retval != -2 ) // -2 means peer just closed the socket
	{
		dprintf(D_ALWAYS,"IO: Failed to read packet header\n");
		return FALSE;
	}
	if ( retval == -2 ) {	// -2 means peer just closed the socket
		dprintf(D_FULLDEBUG,"IO: EOF reading packet header\n");
		return FALSE;
	}

	m_end = (int) ((char *)hdr)[0];
	memcpy(&len_t,  &hdr[1], 4);
	len = (int) ntohl(len_t);

	if (m_end < 0 || m_end > 10) {
		dprintf(D_ALWAYS,"IO: Incoming packet header unrecognized\n");
		return FALSE;
	}
        
	if (!(m_tmp = new Buf)){
		dprintf(D_ALWAYS, "IO: Out of memory\n");
		return FALSE;
	}
	if (len > m_tmp->max_size()){
		delete tmp;
		dprintf(D_ALWAYS, "IO: Incoming packet is too big\n");
		return FALSE;
	}
	if (len <= 0)
	{
		delete m_tmp;
		m_tmp = NULL;
		dprintf(D_ALWAYS, 
			"IO: Incoming packet improperly sized (len=%d,end=%d)\n",
			len, m_end);
		return FALSE;
	}

read_packet:
	tmp_len = m_tmp->read(peer_description, _sock, len, _timeout, p_sock->is_non_blocking());
	if (tmp_len != len) {
		if (p_sock->is_non_blocking() && (tmp_len == -3 || tmp_len >= 0)) {
			m_partial_packet = true;
			if (tmp_len >= 0) {
				m_remaining_read_length = len - tmp_len;
			} else {
				m_remaining_read_length = len;
			}
			return 2;
		} else {
			delete m_tmp;
			m_tmp = NULL;
			dprintf(D_ALWAYS, "IO: Packet read failed: read %d of %d\n",
					tmp_len, len);
			return FALSE;
		}
	}

        // Now, check MD
        if (mode_ != MD_OFF) {
            if (!tmp->verifyMD(&hdr[5], mdChecker_)) {
                delete tmp;
		m_tmp = NULL;
                dprintf(D_ALWAYS, "IO: Message Digest/MAC verification failed!\n");
                return FALSE;  // or something other than this
            }
        }
        
	if (!buf.put(tmp)) {
		delete tmp;
		m_tmp = NULL;
		dprintf(D_ALWAYS, "IO: Packet storing failed\n");
		return FALSE;
	}
		
	if (m_end) {
		ready = TRUE;
	}
	return TRUE;
}


ReliSock::SndMsg::SndMsg() : 
    mode_(MD_OFF), 
    mdChecker_(0),
	p_sock(0),
	m_out_buf(NULL)
{
}

ReliSock::SndMsg::~SndMsg() 
{
    delete mdChecker_;
}

int ReliSock::SndMsg::finish_packet(const char *peer_description, int sock, int timeout)
{
	if (m_out_buf == NULL) {
		return true;
	}
	dprintf(D_NETWORK, "Finishing packet with non-blocking %d.\n", p_sock->is_non_blocking());
	int retval = true;
	int result = m_out_buf->write(peer_description, sock, -1, timeout, p_sock->is_non_blocking());
	if (result < 0) {
		retval = false;
	} else if (!m_out_buf->consumed()) {
		if (p_sock->is_non_blocking()) {
			return 2;
		} else {
			retval = false;
		}
	}
	delete m_out_buf;
	m_out_buf = NULL;
	return retval;
}

void ReliSock::SndMsg::stash_packet()
{
	m_out_buf = new Buf();
	m_out_buf->swap(buf);
	buf.reset();
}

	// Send the current buffer in a single CEDAR packet.
	// Return codes:
	//   - TRUE: successful send.
	//   - FALSE: failed
	//   - 2: Write would have blocked and we are in non-blocking mode.
	//
	// Note: If we are in non-blocking mode, we may just form the buffer but only
	// partially send it to the wire.  If this happens, we return 2.  In such a case,
	// we leave the SndMsg buffer in a valid state -- if the caller cannot buffer the
	// data itself, it may use the 'put_force' method to grow the underlying buffer.
int ReliSock::SndMsg::snd_packet( char const *peer_description, int _sock, int end, int _timeout )
{
		// First, see if we have an incomplete packet.
	int retval = finish_packet(peer_description, _sock, _timeout);
	if (retval == 2) {
		return 3;
	} else if (!retval) {
		return false;
	}
		// 

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

	int result = buf.flush(peer_description, _sock, hdr, header_size, _timeout, p_sock->is_non_blocking());
	if (result < 0) {
		return false;
	} else if (result != ns+header_size) {
		if (p_sock->is_non_blocking()) {
			stash_packet();
			return 2;
		} else {
			return false;
		}
	}
        
	if( end ) {
		buf.dealloc_buf(); // save space, now that we are done sending
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
	sprintf(outbuf,"%d*%s*",_special_state,_who.to_sinful().Value());
	strcat(parent_state,outbuf);

    // Serialize crypto stuff
	char * crypto = serializeCryptoInfo();
    strcat(parent_state, crypto);
    strcat(parent_state, "*");

    // serialize MD info
    char * md = serializeMdInfo();
    strcat(parent_state, md);
    strcat(parent_state, "*");

	delete []outbuf;
    delete []crypto;
    delete []md;
	return( parent_state );
}

char * 
ReliSock::serialize(char *buf)
{
	char * sinful_string = NULL;
	char fqu[256];
	char *ptmp, * ptr = NULL;
	int len = 0;

    ASSERT(buf);

	// first, let our parent class restore its state
    ptmp = Sock::serialize(buf);
    ASSERT( ptmp );
    int itmp;
    int citems = sscanf(ptmp,"%d*",&itmp);
	if (citems == 1)
       _special_state = relisock_state(itmp);
    // skip through this
    ptmp = strchr(ptmp, '*');
    if(ptmp) ptmp++;
    // Now, see if we are 6.3 or 6.2
    if (ptmp && (ptr = strchr(ptmp, '*')) != NULL) {
        // we are 6.3
		sinful_string = new char [1 + ptr - ptmp];
        memcpy(sinful_string, ptmp, ptr - ptmp);
		sinful_string[ptr - ptmp] = 0;

        ptmp = ++ptr;
        // The next part is for crypto
        ptmp = serializeCryptoInfo(ptmp);
        // Followed by Md
        ptmp = serializeMdInfo(ptmp);

        citems = sscanf(ptmp, "%d*", &len);

        if (1 == citems && len > 0) {
            ptmp = strchr(ptmp, '*');
            ptmp++;
            memcpy(fqu, ptmp, len);
            if ((fqu[0] != ' ') && (fqu[0] != '\0')) {
                    // We are cozy
				setFullyQualifiedUser(fqu);
            }
        }
    }
    else if(ptmp) {
        // we are 6.2, this is the end of it.
		size_t sinful_len = strlen(ptmp);
		sinful_string = new char [1 + sinful_len];
        citems = sscanf(ptmp,"%s",sinful_string);
		if (1 != citems) sinful_string[0] = 0;
		sinful_string[sinful_len] = 0;
    }

	_who.from_sinful(sinful_string);
	delete [] sinful_string;
    
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
				bool is_non_blocking = m_non_blocking;
				m_non_blocking = false;
				ret_val = snd_msg.snd_packet(peer_description(), _sock, TRUE, _timeout);
				m_non_blocking = is_non_blocking;
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
								   const char* methods, CondorError* errstack,
								   int auth_timeout, char **method_used)
{
	int in_encode_mode;
	int result;

	if( method_used ) {
		*method_used = NULL;
	}

    if (!triedAuthentication()) {
		Authentication authob(this);
		setTriedAuthentication(true);
			// store if we are in encode or decode mode
		in_encode_mode = is_encode();

			// actually perform the authentication
		if ( with_key ) {
			result = authob.authenticate( hostAddr, key, methods, errstack, auth_timeout );
		} else {
			result = authob.authenticate( hostAddr, methods, errstack, auth_timeout );
		}
			// restore stream mode (either encode or decode)
		if ( in_encode_mode && is_decode() ) {
			encode();
		} else {
			if ( !in_encode_mode && is_encode() ) { 
				decode();
			}
		}

		setFullyQualifiedUser(authob.getFullyQualifiedUser());

		if( authob.getMethodUsed() ) {
			setAuthenticationMethodUsed(authob.getMethodUsed());
			if( method_used ) {
				*method_used = strdup(authob.getMethodUsed());
			}
		}
		if ( authob.getFQAuthenticatedName() ) {
			setAuthenticatedName( authob.getFQAuthenticatedName() );
		}
		return result;
    }
    else {
        return 1;
    }
}

int ReliSock::authenticate(KeyInfo *& key, const char* methods, CondorError* errstack, int auth_timeout, char **method_used)
{
	return perform_authenticate(true,key,methods,errstack,auth_timeout,method_used);
}

int 
ReliSock::authenticate(const char* methods, CondorError* errstack,int auth_timeout ) 
{
	KeyInfo *key = NULL;
	return perform_authenticate(false,key,methods,errstack,auth_timeout,NULL);
}

bool
ReliSock::connect_socketpair(ReliSock &sock,bool use_standard_interface)
{
	ReliSock tmp_srv;

	if( use_standard_interface ) {
		if( !bind(false) ) {
			dprintf(D_ALWAYS, "connect_socketpair: failed in bind()\n");
			return false;
		}
	}
	else if( !bind_to_loopback(false) ) {
		dprintf(D_ALWAYS, "connect_socketpair: failed in bind_to_loopback()\n");
		return false;
	}

	if( use_standard_interface ) {
		if( !tmp_srv.bind(false) ) {
			dprintf(D_ALWAYS, "connect_socketpair: failed in tmp_srv.bind()\n");
			return false;
		}
	}
	else if( !tmp_srv.bind_to_loopback(false) ) {
		dprintf(D_ALWAYS, "connect_socketpair: failed in tmp_srv.bind_to_loopback()\n");
		return false;
	}

	if( !tmp_srv.listen() ) {
		dprintf(D_ALWAYS, "connect_socketpair: failed in tmp_srv.listen()\n");
		return false;
	}

	if( !connect(tmp_srv.my_ip_str(),tmp_srv.get_port()) ) {
		dprintf(D_ALWAYS, "connect_socketpair: failed in tmp_srv.get_port()\n");
		return false;
	}

	if( !tmp_srv.accept( sock ) ) {
		dprintf(D_ALWAYS, "connect_socketpair: failed in tmp_srv.accept()\n");
		return false;
	}

	return true;
}

void
ReliSock::enter_reverse_connecting_state()
{
	if( _state == sock_assigned ) {
		// no need for a socket to be allocated while we are waiting
		// because this socket will be assigned to a new socket
		// once we accept a connection from the listen socket
		this->close();
	}
	ASSERT( _state == sock_virgin );
	_state = sock_reverse_connect_pending;
}

void
ReliSock::exit_reverse_connecting_state(ReliSock *sock)
{
	ASSERT( _state == sock_reverse_connect_pending );
	_state = sock_virgin;

	if( sock ) {
		int assign_rc = assign(sock->get_file_desc());
		ASSERT( assign_rc );
		isClient(true);
		if( sock->_state == sock_connect ) {
			enter_connected_state("REVERSE CONNECT");
		}
		else {
			_state = sock->_state;
		}
		sock->_sock = INVALID_SOCKET;
		sock->close();
	}
	m_ccb_client = NULL;
}

void
ReliSock::setTargetSharedPortID( char const *id )
{
	if( m_target_shared_port_id ) {
		free( m_target_shared_port_id );
		m_target_shared_port_id  = NULL;
	}
	if( id ) {
		m_target_shared_port_id = strdup( id );
	}
}

bool
ReliSock::msgReady() {
	return rcv_msg.ready;
}
