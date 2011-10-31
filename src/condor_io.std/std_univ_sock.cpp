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
#include "std_univ_io.h"
#include "condor_uid.h"
#include "std_univ_sock.h"
#include "condor_network.h"
#include "internet.h"
#include "condor_debug.h"
#include "condor_netdb.h"
#include "condor_rw.h"

#if !defined(WIN32)
#define closesocket close
#endif

#define NORMAL_HEADER_SIZE 5
#define MAX_HEADER_SIZE NORMAL_HEADER_SIZE


int StdUnivSock::setsockopt(int level, int optname, const char* optval, int optlen)
{
	ASSERT (_sock != INVALID_SOCKET);

	if(::setsockopt(_sock, level, optname, optval, optlen) < 0)
	{
		return FALSE;
	}
	return TRUE; 
}

int StdUnivSock::close()
{
	if (type() == Stream::reli_sock && (DebugFlags & D_NETWORK)) {
		dprintf( D_NETWORK, "CLOSE %s fd=%d\n", 
						sock_to_string(_sock), _sock );
	}

	if ( _sock != INVALID_SOCKET ) {
		if (::closesocket(_sock) < 0) return FALSE;
	}

	_sock = INVALID_SOCKET;
	_who.clear();
    addr_changed();
	
	return TRUE;
}


/* NOTE: on timeout() we return the previous timeout value, or a -1 on an error.
 * Once more: we do _not_ return FALSE on Error like most other CEDAR functions;
 * we return a -1 !! 
 */
int
StdUnivSock::timeout(int sec)
{
	int t = _timeout;

	_timeout = sec;

	if (_sock == INVALID_SOCKET) {
		// Rather than forcing creation of the socket here, we just
		// return success.  All paths that create the socket also
		// set the timeout.
		return t;
	}

	if (_timeout == 0) {
		// Put socket into blocking mode
		int fcntl_flags;
		if ( (fcntl_flags=fcntl(_sock, F_GETFL)) < 0 )
			return -1;
		fcntl_flags &= ~O_NONBLOCK;	// reset blocking mode
		if ( fcntl(_sock,F_SETFL,fcntl_flags) == -1 )
			return -1;
	} else {
		// Put socket into non-blocking mode.
		// However, we never want to put a UDP socket into non-blocking mode.		
		if ( type() != safe_sock ) {	// only if socket is not UDP...
			int fcntl_flags;
			if ( (fcntl_flags=fcntl(_sock, F_GETFL)) < 0 )
				return -1;
			fcntl_flags |= O_NONBLOCK;	// set nonblocking mode
			if ( fcntl(_sock,F_SETFL,fcntl_flags) == -1 )
				return -1;
		}
	}

	return t;
}

void
StdUnivSock::addr_changed()
{
    // these are all regenerated whenever they are needed, so when
    // either the peer's address or our address change, zap them all
    _peer_ip_buf[0] = '\0';
    _sinful_peer_buf[0] = '\0';
}

const char *
StdUnivSock::peer_ip_str()
{
    if( _peer_ip_buf[0] ) {
        return _peer_ip_buf;
    }
	MyString ip_string = _who.to_ip_string();
    strncpy( _peer_ip_buf, ip_string.Value(), IP_STRING_BUF_SIZE );
    _peer_ip_buf[IP_STRING_BUF_SIZE-1] = '\0';
	return _peer_ip_buf;
}

condor_sockaddr
StdUnivSock::my_addr()
{
	sockaddr_storage addr;
	SOCKET_LENGTH_TYPE addr_len;
	addr_len = sizeof(addr);
	if (getsockname(_sock, (sockaddr*)&addr, &addr_len) < 0)
		return condor_sockaddr::null;
	return condor_sockaddr((const sockaddr*)&addr);
}

char *
StdUnivSock::get_sinful_peer()
{       
    if( _sinful_peer_buf[0] ) {
        return _sinful_peer_buf;
    }
	MyString sinful_string = _who.to_sinful();
	ASSERT(sinful_string.Length() < sizeof(_sinful_peer_buf));
	strcpy(_sinful_peer_buf, sinful_string.Value());
	return _sinful_peer_buf;
}

char const *
StdUnivSock::default_peer_description()
{
	char const *retval = get_sinful_peer();
	if( !retval ) {
		return "(unconnected socket)";
	}
	return retval;
}

bool
StdUnivSock::canEncrypt()
{
	return false;
}

/* 
   NOTE: All StdUnivSock constructors initialize with this, so you can
   put any shared initialization code here.  -Derek Wright 3/12/99
*/
void
StdUnivSock::init()
{
	ignore_next_encode_eom = FALSE;
	ignore_next_decode_eom = FALSE;
	_bytes_sent = 0.0;
	_bytes_recvd = 0.0;
	snd_msg.buf.reset();                                                    
	rcv_msg.buf.reset();   
	rcv_msg.init_parent(this);
	snd_msg.init_parent(this);

	_sock = INVALID_SOCKET;
	_timeout = 0;
	_who.clear();
    addr_changed();
}


StdUnivSock::StdUnivSock()
	: Stream()
{
	init();
}

StdUnivSock::~StdUnivSock()
{
	close();
}

int 
StdUnivSock::handle_incoming_packet()
{
	/* do not queue up more than one message at a time on reliable sockets */
	/* but return 1, because old message can still be read.						*/
	if (rcv_msg.ready) {
		return TRUE;
	}

	if (!rcv_msg.rcv_packet(peer_description(), _sock, _timeout)) {
		return FALSE;
	}

	return TRUE;
}



int 
StdUnivSock::end_of_message()
{
	int ret_val = FALSE;

	switch(_coding){
		case stream_encode:
			if ( ignore_next_encode_eom == TRUE ) {
				ignore_next_encode_eom = FALSE;
				return TRUE;
			}
			if (!snd_msg.buf.empty()) {
				return snd_msg.snd_packet(peer_description(), _sock, TRUE, _timeout);
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
				else if( !allow_empty_message_flag ) {
					char const *ip = get_sinful_peer();
					dprintf(D_FULLDEBUG,"Failed to read end of message from %s.\n",ip ? ip : "(null)");
				}
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


bool 
StdUnivSock::peek_end_of_message()
{
	EXCEPT("not implemented");
	return false;
}

int 
StdUnivSock::put_bytes(const void *data, int sz)
{
	int		tw, header_size = NORMAL_HEADER_SIZE;
	int		nw;
	unsigned char * dta = NULL;

	if((dta = (unsigned char *) malloc(sz)) != 0)
		memcpy(dta, data, sz);
	ignore_next_encode_eom = FALSE;
	for(nw=0;;) {
		if (snd_msg.buf.full()) {
			if (!snd_msg.snd_packet(peer_description(), _sock, FALSE, _timeout)) {
				if (dta != NULL) {
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
			if (dta != NULL) {
				free(dta);
				dta = NULL;
			}
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

	if (dta != NULL) {
		free(dta);
		dta = NULL;
	}
	return nw;
}


int 
StdUnivSock::get_bytes(void *dta, int max_sz)
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


int StdUnivSock::get_ptr( void *&ptr, char delim)
{
	while (!rcv_msg.ready){
		if (!handle_incoming_packet()) {
			return FALSE;
		}
	}

	return rcv_msg.buf.get_tmp(ptr, delim);
}


int StdUnivSock::peek( char &c)
{
	while (!rcv_msg.ready) {
		if (!handle_incoming_packet()) {
			return FALSE;
		}
	}

	return rcv_msg.buf.peek(c);
}

StdUnivSock::RcvMsg :: RcvMsg() : p_sock(0),ready(0) {}

StdUnivSock::RcvMsg::~RcvMsg() {}

int StdUnivSock::RcvMsg::rcv_packet( char const *peer_description, SOCKET _sock, int _timeout)
{
	StdUnivBuf		*tmp;
	char	        hdr[MAX_HEADER_SIZE];
	int		end;
	int		len, len_t, header_size;
	int		tmp_len;

	header_size = NORMAL_HEADER_SIZE;

	int retval = condor_read(peer_description,_sock,hdr,header_size,_timeout);
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
	end = (int) ((char *)hdr)[0];
	memcpy(&len_t,  &hdr[1], 4);
	len = (int) ntohl(len_t);

	if (end < 0 || end > 10) {
		dprintf(D_ALWAYS,"IO: Incoming packet header unrecognized\n");
		return FALSE;
	}
        
	if (!(tmp = new StdUnivBuf)){
		dprintf(D_ALWAYS, "IO: Out of memory\n");
		return FALSE;
	}
	if (len > tmp->max_size()){
		delete tmp;
		dprintf(D_ALWAYS, "IO: Incoming packet is too big\n");
		return FALSE;
	}
	if (len <= 0)
	{
		delete tmp;
		dprintf(D_ALWAYS, 
			"IO: Incoming packet improperly sized (len=%d,end=%d)\n",
			len,end);
		return FALSE;
	}
	if ((tmp_len = tmp->read(peer_description, _sock, len, _timeout)) != len){
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


StdUnivSock::SndMsg::SndMsg() : 
	p_sock(0)
{
}

StdUnivSock::SndMsg::~SndMsg() 
{
}

int StdUnivSock::SndMsg::snd_packet( char const *peer_description, int _sock, int end, int _timeout )
{
	char	        hdr[MAX_HEADER_SIZE];
	int		len, header_size;
	int		ns;

    header_size = NORMAL_HEADER_SIZE;
	hdr[0] = (char) end;
	ns = buf.num_used() - header_size;
	len = (int) htonl(ns);

	memcpy(&hdr[1], &len, 4);

        if (buf.flush(peer_description, _sock, hdr, header_size, _timeout) != (ns+header_size)){
            return FALSE;
        }
        
	if( end ) {
		buf.dealloc_buf(); // save space, now that we are done sending
	}
	return TRUE;
}

#ifndef WIN32
	// interface no longer supported
int 
StdUnivSock::attach_to_file_desc( int fd )
{
	ASSERT (_sock == INVALID_SOCKET);

	_sock = fd;
	timeout(0);	// make certain in blocking mode
	return TRUE;
}
#endif

Stream::stream_type 
StdUnivSock::type() 
{ 
	return Stream::reli_sock; 
}


bool
StdUnivSock::peer_is_local()
{
	EXCEPT("not implemented");
	return false;
}

char *
StdUnivSock::serialize(char *)
{
	EXCEPT("not implemented");
	return NULL;
}
char *
StdUnivSock::serialize() const
{
	EXCEPT("not implemented");
	return NULL;
}

Stream *
StdUnivSock::CloneStream()
{
	EXCEPT("not implemented");
	return NULL;
}

int
StdUnivSock::bytes_available_to_read()
{
	EXCEPT("not implemented");
	return -1;
}

const char*
StdUnivSock::my_ip_str()
{
	EXCEPT("not implemented");
	return NULL;
}

