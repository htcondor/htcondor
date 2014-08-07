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


#ifndef RELI_SOCK_H
#define RELI_SOCK_H

#include "buffers.h"
#include "sock.h"
#include "condor_adtypes.h"
#include "condor_system.h"
#include "condor_ipverify.h"
#include "condor_md.h"

#include <memory>

/*
**	R E L I A B L E    S O C K
*/

// These functions exchange arbitrary data blocks over a ReliSock.
// They are for use with the Globus GSI gss-assist library.
int relisock_gsi_get(void *arg, void **bufp, size_t *sizep);
int relisock_gsi_put(void *arg,  void *buf, size_t size);

class Authentication;
class Condor_MD_MAC;
/** The ReliSock class implements the Sock interface with TCP. */

#define GET_FILE_OPEN_FAILED -2
#define PUT_FILE_OPEN_FAILED -2
#define GET_FILE_WRITE_FAILED -3
#define GET_FILE_PLUGIN_FAILED -4
#define PUT_FILE_PLUGIN_FAILED -4
#define PUT_FILE_MAX_BYTES_EXCEEDED -5
#define GET_FILE_MAX_BYTES_EXCEEDED -5

class BlockingModeGuard;

class ReliSock : public Sock {
	friend class Authentication;
	friend class BlockingModeGuard;

//	PUBLIC INTERFACE TO RELIABLE SOCKS
//
public:

	friend class DaemonCore;
	friend class SharedPortEndpoint;

	/*
	**	Methods
	*/

	// Virtual socket services
	//
    ///
	virtual int handle_incoming_packet();

    ///
	virtual int end_of_message();
	int end_of_message_internal();

		// If in non-blocking mode and EOM_nb returns 2,
		// then this must be called until it finishes
		// successfully (1) or fails (0); if this returns
		// 2, then try again in the future.
	int end_of_message_nonblocking();
	int finish_end_of_message();

	virtual bool peek_end_of_message();

	/**	@return true if a complete message is ready to be read
	*/
	virtual bool msgReady();

    /** Connect to a host on a port
        @param s can be a hostname or sinful string
        @param port the port to connect to, ignorred if s contains port
    */
	virtual int connect(char const *s, int port=0, 
							bool do_not_block = false);


	virtual int do_reverse_connect(char const *ccb_contact,bool nonblocking);

	virtual void cancel_reverse_connect();

	virtual int do_shared_port_local_connect( char const *shared_port_id, bool nonblocking );

		/** Connect this socket to another socket (s).
			An implementation of socketpair() that works on windows as well
			as unix (by using the loopback interface).
			@param sock - the socket to connect this socket to
			@param use_standard_interface - if true, do not use loopback,
			                                use normal interface
			@returns true on success, false on failure.
		 */
	bool connect_socketpair(ReliSock &dest,bool use_standard_interface=false);

    ///
	ReliSock();

	/// Copy ctor
	ReliSock(const ReliSock &);

	/// Create a copy of this stream (e.g. dups underlying socket).
	/// Caller should delete the returned stream when finished with it.
	Stream *CloneStream();

    ///
	~ReliSock();
    ///
	void init();				/* shared initialization method */

    ///
	int listen();
    /// FALSE means this is an incoming connection
	int listen(condor_protocol proto, int port);
    /// FALSE means this is an incoming connection
	int listen(char *s);
	bool isListenSock() { return _state == sock_special && _special_state == relisock_listen; }

    ///
	ReliSock *accept();
    ///
	int accept(ReliSock &);
    ///
	int accept(ReliSock *);

    ///
	int put_line_raw( char *buffer );
    ///
	int get_line_raw( char *buffer, int max_length );
    ///
	int put_bytes_raw( const char *buffer, int length );
    ///
	int get_bytes_raw( char *buffer, int length );
    ///
	int put_bytes_nobuffer(char *buf, int length, int send_size=1);
    ///
	int get_bytes_nobuffer(char *buffer, int max_length, int receive_size=1);

    /// returns <0 on failure, 0 for ok
	//  failure codes: GET_FILE_OPEN_FAILED  (errno contains specific error)
	//                 GET_FILE_WRITE_FAILED (errno contains specific error)
	//                 -1                    (all other errors)
	int get_file_with_permissions(filesize_t *size, const char *desination,
								  bool flush_buffers=false, filesize_t max_bytes=-1,
								  class DCTransferQueue *xfer_q=NULL);
    /// returns <0 on failure, 0 for ok
	//  failure codes: GET_FILE_OPEN_FAILED  (errno contains specific error)
	//                 -1                    (all other errors)
	int get_file( filesize_t *size, const char *destination,
				  bool flush_buffers=false, bool append=false, filesize_t max_bytes=-1, class DCTransferQueue *xfer_q=NULL);
    /// returns -1 on failure, 0 for ok
	int get_file( filesize_t *size, int fd,
				  bool flush_buffers=false, bool append=false, filesize_t max_bytes=-1, class DCTransferQueue *xfer_q=NULL);
    /// returns <0 on failure, 0 for ok
	//  See put_file() for the meaning of specific return codes.
	int put_file_with_permissions( filesize_t *size, const char *source, filesize_t max_bytes=-1, class DCTransferQueue *xfer_q=NULL);
	// xfer_q (if not NULL) is used to report i/o stats
    /// returns <0 on failure, 0 for ok
	//  failure codes: PUT_FILE_OPEN_FAILED  (errno contains specific error)
	//                 PUT_FILE_MAX_BYTES_EXCEEDED
	//                 -1                    (all other errors)
	// In the case of PUT_FILE_OPEN_FAILED, the caller may assume that
	// we can continue talking to the receiver, as though a file had
	// been successfully sent.  In most cases, the next logical thing
	// to do is to tell the receiver about the failure.
	int put_file( filesize_t *size, const char *source, filesize_t offset=0, filesize_t max_bytes=-1, class DCTransferQueue *xfer_q=NULL );
    /// returns -1 on failure, 0 for ok
	int put_file( filesize_t *size, int fd, filesize_t offset=0, filesize_t max_bytes=-1, class DCTransferQueue *xfer_q=NULL );

	// This is used internally to recover sanity on the stream after
	// failing to open a file.  The remote side will see this as a zero-sized file.
	// returns -1 on failure, 0 for ok
	int put_empty_file( filesize_t *size );

	/// returns -1 on failure, 0 for ok
	int get_x509_delegation( filesize_t *size, const char *destination,
							 bool flush_buffers=false );
	/// returns -1 on failure, 0 for ok
	// expiration_time: 0 if none; o.w. timestamp of delegated proxy expiration
	// result_expiration_time: if non-NULL will be set to actual expiration
	//                         time of delegated proxy; could be shorter than
	//                         requested time if source proxy expires sooner
	int put_x509_delegation( filesize_t *size, const char *source, time_t expiration_time, time_t *result_expiration_time );
    ///
	float get_bytes_sent() { return _bytes_sent; }
    ///
	float get_bytes_recvd() { return _bytes_recvd; }
    ///
	void reset_bytes_sent() { _bytes_sent = 0; }
    ///
	void reset_bytes_recvd() { _bytes_recvd = 0; }

	/// Used by CCBClient to put this socket in a state that behaves
	/// like a socket waiting for a non-blocking connection when it
	/// is actually waiting for a connection _to_ us _from_ the
	/// desired endpoint.
	void enter_reverse_connecting_state();
	void exit_reverse_connecting_state(ReliSock *sock);

#ifndef WIN32
	// interface no longer supported 
	int attach_to_file_desc(int);
#endif

	/*
	**	Stream protocol
	*/

    ///
	virtual stream_type type();

	//	byte operations
	//
    ///
	virtual int put_bytes(const void *, int);
    ///
	virtual int get_bytes(void *, int);
    ///
	virtual int get_ptr(void *&, char);
    ///
	virtual int peek(char &);

    ///
	int authenticate( const char* methods, CondorError* errstack, int auth_timeout, bool non_blocking );
    ///
	int authenticate( KeyInfo *& key, const char* methods, CondorError* errstack, int auth_timeout, bool non_blocking, char **method_used );
    ///
	int authenticate_continue( CondorError* errstack, bool non_blocking, char **method_used );
    ///
	int isClient() { return is_client; };

	// Normally, the side of the connection that called connect() is
	// the client.  The opposite is true for a reversed connection.
	// This matters for the authentication protocol.
	void isClient(bool flag) { is_client=flag; };

    const char * isIncomingDataMD5ed();

	int clear_backlog_flag() {bool state = m_has_backlog; m_has_backlog = false; return state;}
	int clear_read_block_flag() {bool state = m_read_would_block; m_read_would_block = false; return state;}

	bool is_closed() {return rcv_msg.m_closed;}
//	PROTECTED INTERFACE TO RELIABLE SOCKS
//
protected:

	bool set_non_blocking(bool val) {bool state = m_non_blocking; m_non_blocking = val; return state;}
	bool is_non_blocking() const {return m_non_blocking;}

        virtual bool init_MD(CONDOR_MD_MODE mode, KeyInfo * key, const char * keyId);
        virtual bool set_encryption_id(const char * keyId);

	/*
	**	Types
	*/

	enum relisock_state { relisock_none, relisock_listen };

	/*
	**	Methods
	*/
	char * serialize(char *);	// restore state from buffer
	char * serialize() const;	// save state into buffer

	int prepare_for_nobuffering( stream_coding = stream_unknown);
	int perform_authenticate( bool with_key, KeyInfo *& key, 
							  const char* methods, CondorError* errstack,
							  int auth_timeout, bool non_blocking, char **method_used );


	/*
	**	Data structures
	*/

	class RcvMsg {
		
		char m_partial_cksum[MAC_SIZE];
                CONDOR_MD_MODE  mode_;
                Condor_MD_MAC * mdChecker_;
		ReliSock      * p_sock; //preserve parent pointer to use for condor_read/write
		bool		m_partial_packet; // A partial packet is stored.
		size_t		m_remaining_read_length; // Length remaining on a partial packet
		int		m_end; // The end status of the partial packet.
		Buf		*m_tmp;
	public:
		RcvMsg();
                ~RcvMsg();
		int rcv_packet(char const *peer_description, SOCKET, int);
		void init_parent(ReliSock *tmp){ p_sock = tmp; } 

		ChainBuf	buf;
		int			ready;
		bool m_closed;
		bool init_MD(CONDOR_MD_MODE mode, KeyInfo * key);
	} rcv_msg;

	class SndMsg {
                CONDOR_MD_MODE  mode_;
                Condor_MD_MAC * mdChecker_;
		ReliSock      * p_sock;
		Buf		*m_out_buf;
		void stash_packet();

	public:
		SndMsg();
                ~SndMsg();
		Buf			buf;
		int snd_packet(char const *peer_description, int, int, int);

			// If there is a packet not flushed to the network, try to
			// send it again.
		int finish_packet(const char *peer_description, int sock, int timeout);

		//function to support the use of condor_read /write
		
		void init_parent(ReliSock *tmp){ 
			p_sock = tmp; 
			buf.init_parent(tmp);
		}

        bool init_MD(CONDOR_MD_MODE mode, KeyInfo * key);


	} snd_msg;

	relisock_state	_special_state;
	int	ignore_next_encode_eom;
	int	ignore_next_decode_eom;
	float _bytes_sent, _bytes_recvd;

	int is_client;
	char *hostAddr;
	classy_counted_ptr<class CCBClient> m_ccb_client; // for reverse connects

		// after connecting, request to be routed to this daemon
	char *m_target_shared_port_id;

	Authentication *m_authob;
	bool m_auth_in_progress;

	bool m_has_backlog;
	bool m_read_would_block;
	bool m_non_blocking;

	virtual void setTargetSharedPortID( char const *id );
	virtual bool sendTargetSharedPortID();
	char const *getTargetSharedPortID() { return m_target_shared_port_id; }
};

class BlockingModeGuard {

public:
	BlockingModeGuard(ReliSock *parent, bool non_blocking)
	 : m_parent(parent), m_mode(m_parent->set_non_blocking(non_blocking))
	{
	}

	~BlockingModeGuard()
	{
		m_parent->set_non_blocking(m_mode);
	}

private:
	ReliSock *m_parent;
	bool m_mode;
};

#endif
