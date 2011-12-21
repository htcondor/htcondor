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


#ifndef STD_UNIV_SOCK_H
#define STD_UNIV_SOCK_H

#include "condor_common.h"
#include "condor_socket_types.h"
#include "stream.h"
#include "CondorError.h"
#include "std_univ_buffers.h"
#include "condor_sockaddr.h"

#if !defined(WIN32)
#  ifndef SOCKET
#    define SOCKET int
#  endif
#  ifndef INVALID_SOCKET
#    define INVALID_SOCKET -1
#  endif
#endif /* not WIN32 */

class StdUnivSock : public Stream {

public:

	StdUnivSock();
	virtual ~StdUnivSock();

	int attach_to_file_desc(int);

	int end_of_message();

	virtual bool peek_end_of_message();

	virtual int put_bytes(const void *, int);

	virtual int get_bytes(void *, int);

	virtual int get_ptr(void *&, char);

	virtual int peek(char &);

	float get_bytes_sent() { return _bytes_sent; }

	float get_bytes_recvd() { return _bytes_recvd; }

	/** if any operation takes more than sec seconds, timeout
        call timeout(0) to set blocking mode (default)
        @param sec the number of seconds to wait before timing out
        @return previous timeout
    */
	int timeout(int sec);

	/// peer's IP address, string verison (e.g. "128.105.101.17")
	const char* peer_ip_str();

	/// local socket address
	condor_sockaddr my_addr();

 private:

	void init();				/* shared initialization method */

	int close();

	int handle_incoming_packet();

    /// called whenever the bound or connected state changes
    void addr_changed();

	/// sinful address of peer in form of "<a.b.c.d:pppp>"
	char * get_sinful_peer();

	/// sinful address of peer, suitable for passing to dprintf() (never NULL)
	virtual char const *default_peer_description();

	virtual bool canEncrypt();

    int setsockopt(int, int, const char*, int); 

	class RcvMsg {

		StdUnivSock      * p_sock; //preserve parent pointer to use for condor_read/write

	public:
		RcvMsg();
                ~RcvMsg();
		int rcv_packet(char const *peer_description, SOCKET, int);
		void init_parent(StdUnivSock *tmp){ p_sock = tmp; } 

		StdUnivChainBuf	buf;
		int			ready;
	} rcv_msg;

	class SndMsg {
		StdUnivSock      * p_sock;
	public:
		SndMsg();
                ~SndMsg();
		StdUnivBuf			buf;
		int snd_packet(char const *peer_description, int, int, int);

		//function to support the use of condor_read /write
		
		void init_parent(StdUnivSock *tmp){ 
			p_sock = tmp; 
			buf.init_parent(tmp);
		}

	} snd_msg;

	int	ignore_next_encode_eom;
	int	ignore_next_decode_eom;
	float _bytes_sent, _bytes_recvd;

	SOCKET			_sock;
	int				_timeout;
	condor_sockaddr			 _who;	// endpoint of "connection"

	char _peer_ip_buf[IP_STRING_BUF_SIZE];	

	char _sinful_peer_buf[SINFUL_STRING_BUF_SIZE];

 private:

	/*
	 * unimplemented stubs to make inheritance from Stream possible
	 */

	int bytes_available_to_read();
	bool peer_is_local();
	char * serialize(char *);
	char * serialize() const;
	Stream *CloneStream();
	stream_type type();
	const char* my_ip_str();
};

#endif /* SOCK_H */
