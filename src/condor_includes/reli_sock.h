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

#include <openssl/evp.h>

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
	friend class DockerProc;
	friend class OsProc;

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
	virtual int connect(char const *s, int port = 0,
							bool do_not_block = false,
							CondorError * errorStack = NULL);

	virtual int close();

	virtual int do_reverse_connect(char const *ccb_contact,bool nonblocking,CondorError * errorStack);

	virtual void cancel_reverse_connect();

	virtual int do_shared_port_local_connect( char const *shared_port_id, bool nonblocking,char const *sharedPortIP );

		/** Connect this socket to another socket (s).
			An implementation of socketpair() that works on windows as well
			as unix (by using the loopback interface).
			@param sock - the socket to connect this socket to
			@param use_standard_interface - if true, do not use loopback,
			                                use normal interface
			@returns true on success, false on failure.
		 */
	bool connect_socketpair( ReliSock & dest );
	bool connect_socketpair( ReliSock & dest, char const * asIfConnectingTo );

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
	int listen();
    /// FALSE means this is an incoming connection
	int listen(condor_protocol proto, int port);
	bool isListenSock() { return _state == sock_special && _special_state == relisock_listen; }

    ///
	ReliSock *accept();
    ///
	int accept(ReliSock &);
    ///
	int accept(ReliSock *);

    ///
	int put_line_raw( const char *buffer );
    ///
	int get_line_raw( char *buffer, int max_length );
    ///
	int put_bytes_raw( const char *buffer, int length );
    ///
	int get_bytes_raw( char *buffer, int length );
    ///
	int put_bytes_nobuffer(const char *buf, int length, int send_size=1);
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

	/// returns delegation_error on failure, delegation_ok on success,
	/// and delegation_continue if the delegation is incomplete.
	///
	/// The continuations are allowed only if state_ptr is non-NULL.
	/// When more data is available on the socket, the caller should call
	/// get_x509_delegation_finish.
	enum x509_delegation_result {delegation_ok, delegation_continue, delegation_error};
	x509_delegation_result get_x509_delegation( const char *destination, bool flush_buffers, void **state_ptr );

	/// The second half of get_x509_delegation.  If state_ptr was non-NULL to the original function call,
	/// one must pass the same pointer here.  The function takes ownership of the value and will call delete
	/// on it.
	x509_delegation_result get_x509_delegation_finish( const char *destination, bool flush_buffers, void *state_ptr );

	/// returns -1 on failure, 0 for ok
	// expiration_time: 0 if none; o.w. timestamp of delegated proxy expiration
	// result_expiration_time: if non-NULL will be set to actual expiration
	//                         time of delegated proxy; could be shorter than
	//                         requested time if source proxy expires sooner
	int put_x509_delegation( filesize_t *size, const char *source, time_t expiration_time, time_t *result_expiration_time );
    ///
	float get_bytes_sent() const { return _bytes_sent; }
    ///
	float get_bytes_recvd() const { return _bytes_recvd; }
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

		// returns a pointer to an internally managed
		// buffer with a human-readable string containing
		// tcp statistics from the TCP_INFO sockopt.  Don't free.
		// may return null
	char *get_statistics();

#ifndef WIN32
	// interface no longer supported 
	int attach_to_file_desc(int);
#endif

	/*
	**	Stream protocol
	*/

    ///
	virtual stream_type type() const;

	//	byte operations
	//
    ///
	virtual int put_bytes(const void *, int);
	int put_bytes_after_encryption(const void *, int);
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
	int isClient() const { return is_client; };

	// Normally, the side of the connection that called connect() is
	// the client.  The opposite is true for a reversed connection.
	// This matters for the authentication protocol.
	void isClient(bool flag) { is_client=flag; };

    const char * isIncomingDataHashed();

	int clear_backlog_flag() {bool state = m_has_backlog; m_has_backlog = false; return state;}
	int clear_read_block_flag() {bool state = m_read_would_block; m_read_would_block = false; return state;}

	bool is_closed() const {return rcv_msg.m_closed;}

	// serialize and deserialize
	const char * deserialize(const char *);	// restore state from buffer
	void serialize(std::string& outbuf) const;	// save state into buffer

		// Reset the message digests for header integrity.
	void resetHeaderMD();

//	PROTECTED INTERFACE TO RELIABLE SOCKS
//
protected:

	bool set_non_blocking(bool val) {bool state = m_non_blocking; m_non_blocking = val; return state;}
	bool is_non_blocking() const {return m_non_blocking;}

        virtual bool init_MD(CONDOR_MD_MODE mode, KeyInfo * key, const char * keyId);
        virtual bool set_encryption_id(const char * keyId);
	Condor_Crypt_Base *get_crypto() {return crypto_;}
	Condor_Crypto_State *get_crypto_state() {return crypto_state_;}

	/*
	**	Types
	*/

	enum relisock_state { relisock_none, relisock_listen };

	/*
	**	Methods
	*/

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
		size_t		m_len_t; // Network-encoded length of packet (used to reconstruct header).
		int		m_end; // The end status of the partial packet.
		Buf		*m_tmp;
	public:
		RcvMsg();
		~RcvMsg();
		void reset();
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
		void reset();
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
	char *statsBuf;

	classy_counted_ptr<class CCBClient> m_ccb_client; // for reverse connects

		// after connecting, request to be routed to this daemon
	char *m_target_shared_port_id;

	Authentication *m_authob;
	bool m_auth_in_progress;

	bool m_has_backlog;
	bool m_read_would_block;
	bool m_non_blocking;

	// Message digest covering communications prior to enabling encryption
	// When encryption is enabled, this digest is included in the authenticated
	// data in order to detect that the two sides didn't see the same handshake.
	// NOTE: We only check the first 1MB of sent / received data; after that,
	// if we haven't seen an encrypted packet we assume such a thing will never
	// happen.
#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)
	std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_destroy)> m_send_md_ctx;
	std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_destroy)> m_recv_md_ctx;
#else
	std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> m_send_md_ctx;
	std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> m_recv_md_ctx;
#endif
	std::vector<unsigned char> m_final_mds;
	bool m_final_send_header{false};
	bool m_final_recv_header{false};
	bool m_finished_send_header{false};
	bool m_finished_recv_header{false};
	void serializeMsgInfo(std::string& outbuf) const;
	const char * deserializeMsgInfo(const char * buf);

	virtual void setTargetSharedPortID( char const *id );
	virtual bool sendTargetSharedPortID();
	char const *getTargetSharedPortID() { return m_target_shared_port_id; }

private:
    ///
	void init();				/* shared initialization method */

	bool connect_socketpair_impl( ReliSock & dest, condor_protocol proto, bool isLoopback );
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
