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


#ifndef SOCK_H
#define SOCK_H

#include "condor_common.h"
#include "stream.h"
#include "CondorError.h"
#include "condor_perms.h"
#include "condor_sockaddr.h"
#include "condor_crypt_aesgcm.h"

#include <unordered_set>

// retry failed connects for CONNECT_TIMEOUT seconds
#define CONNECT_TIMEOUT 10

// Some error codes.  These should go to condor_errno.h once it is there.
const int CEDAR_EWOULDBLOCK = 666;
const int CEDAR_ENOCCB = 667;

#if !defined(WIN32)
#  ifndef SOCKET
#    define SOCKET int
#  endif
#  ifndef INVALID_SOCKET
#    define INVALID_SOCKET -1
#  endif
#endif /* not WIN32 */


#ifdef WIN32
class SockInitializer {
	private:
		bool called_from_init;
	public:
		SockInitializer();
		~SockInitializer();
		void init();
};
#endif  /* of WIN32 */

namespace classad {
class ClassAd;
}

/*
**	B A S E    S O C K
*/

/** The Sock class defines an interface for connection establishment.
This interface is implemented with TCP in the ReliSock class and with
UDP in the SafeSock class.  ReliSock and SafeSock are the only Cedar
classes which should be instantiated; however, pointers to the Stream
base class are often used at points in the application where it is not
important if the underlying medium is TCP or UDP (for example, in a
daemon which handles the same commands over both TCP and UDP).
*/
class Sock : public Stream {

//	PUBLIC INTERFACE TO ALL SOCKS
//
public:

	friend class DaemonCore;
	friend class Daemon;
	friend class SecMan;
	friend class SecManStartCommand;
	friend class SharedPortListener;
	friend class SharedPortEndpoint;
	friend class DockerProc;
	friend class OsProc;

	/*
	**	Methods
	*/

	/// Virtual socket services
	virtual int handle_incoming_packet() = 0;

	/** Connect the socket to a remote peer.
		@param host Hostname of peer, either a DNS name or an IP address, or
		an IP address and port together in the format "&lt;IP:PORT&gt;", such as 
		<p> &lt;128.105.44.66:3354&gt;
		<b>Note:</b> The "&lt;" and "&gt;" are required when giving both the
		IP and Port.
		@param port The port to connect to.  If host is in the form <IP:PORT>,
		then the port parameter is ignored.
		@param do_not_block If false, then the connect call will block until
		completion or until timeout.  If true, then the connect call will
		return immediately with TRUE, FALSE, or CEDAR_EWOULDBLOCK.  If
		CEDAR_EWOULDBLOCK is returned, the user should call
		do_connect_finish() when select says this socket is writeable to
		find out if the CEDAR connection process has really completed.
		In Condor this is usually accomplished by calling DaemonCore's
		Register_Socket() method.
		@return TRUE if connection succeeded, FALSE on failure, or
		CEDAR_EWOULDBLOCK if parameter do_not_block is set to true and
		the call would block.
		@see do_connect_finish
	*/
	virtual int connect( char const *host, int port=0, bool do_not_block = false, CondorError * errorStack = NULL ) = 0;

	/** Close the socket.  Note that reusing the socket after closing it is not
		recommended; instead simply delete the Socket (it will be closed in the
		destructor), and reallocate a new one.
	*/
	virtual int close();

	/** Connect the socket to a remote peer.
		@param host Hostname of the peer, either a DNS name or IP address.
		@param service The name of a service that represents a port address,
		which can be passed to getportbyserv().
	*/
	inline int connect(char const *host, char *service, bool do_not_block = false) { 
		return connect(host,getportbyserv(service),do_not_block);
	}


	//
	// This set of functions replaces assign().
	//
	// assignSocket() specifies a socket FD for this socket to use.  It
	// must have been created with AF_INET or AF_INET6.  The socket's protocol
	// and type MUST match the Sock's protocol and type.
	//
	// assignInvalidSocket() should probably be called reinitialize(); it
	// calls socket() as appropriate for its existing type and protocol.  The
	// Sock MUST have an existing protocol.
	//
	// assignDomainSocket() specifies a (unix) domain socket FD for this
	// socket to use.  It must have been created with AF_UNX (AF_LOCAL).
	//

	//
	// These functions should probably be pruned and replace by a smaller
	// set of protected functions and a constructor which accepts a
	// socket FD; we could then use a placement constructor if necessary.
	//

	int assignSocket( SOCKET s );
	int assignSocket( condor_protocol proto, SOCKET s );
	int assignInvalidSocket();
	int assignInvalidSocket( condor_protocol proto );
	int assignDomainSocket( SOCKET s );
	//
	// Normally, we verify that the socket (fd) you're assigning to this
	// Sock (object) has the same address family (protocol).  However,
	// when using CCB, this restriction must be relaxed.  Consider an
	// IPv4-only schedd matches against a mixed-mode startd using a
	// mixed-mode CCB.  If the mixed-mode startd prefers its IPv6 address,
	// and the schedd is configured to respect that preference, it will
	// attempt to connect to the IPv6 address via CCB.  However, since the
	// schedd connected to CCB using IPv4, CCB will ask the startd to
	// also connect over IPv4, leading to a reverse connection over IPv4
	// for an IPv6 address.
	//
	int assignCCBSocket( SOCKET s );
#if defined(WIN32) && defined(_WINSOCK2API_)
	int assign(LPWSAPROTOCOL_INFO);		// to inherit sockets from other processes
#endif

	int bind(condor_protocol proto, bool outbound, int port, bool loopback, condor_sockaddr *bindTo = NULL);

    int setsockopt(int, int, const void*, int);

	/**  Set the size of the operating system buffers (in the IP stack) for
		 this socket.
		 @param desired_size The desired size of the buffer in bytes.  If 
		 desired_size is greater than the maximum allowed by the operating system,
		 the size will be set to the maximum allowed.
		 @param set_write_buf if false, then only the size of the receive buffer
		 is changed; if true, then both the send and receive buffers are resized
		 @return the actual/resulting size of the buffer in bytes
	*/
	int set_os_buffers(int desired_size, bool set_write_buf = false);

	/** Enable keepalive options for this socket as dictacted by config knob
		TCP_KEEPALIVE_INTERVAL.  Currently a no-op on anything but relisocks.
		@return false if any system call errors, true otherwise.
	*/
	bool set_keepalive();

	/** if any operation takes more than sec seconds, timeout
        call timeout(0) to set blocking mode (default)
        @param sec the number of seconds to wait before timing out
               Note that (for better or worse) this may be multiplied
               by a global timeout multiplier unless
               ignoreTimeoutMultiplier has been called.
        @return previous timeout (divided by timeout multiplier)
    */
	int timeout(int sec);

	/** This is just like timeout(), but it does not do any
	    adjustments.  The timeout multiplier is always ignored.
	*/
	int timeout_no_timeout_multiplier(int sec);
    
	/** Returns the timeout with timeout multiplier applied. */
	int get_timeout_raw() const;

	/** get the number of bytes available to read without blocking.
		@return number of bytes, or -1 on failure
	*/
	int bytes_available_to_read() const;

	/**	@return true if > 0 bytes ready to read without blocking
	*/
	bool readReady();

	/**	@return true if a complete message is ready to be read
	*/
	virtual bool msgReady() = 0;

        //------------------------------------------
        // Encryption support below
        //------------------------------------------
        bool set_crypto_key(bool enable, KeyInfo * key, const char * keyId=0);

        int ciphertext_size(int plaintext_size) {
            if (!crypto_) return plaintext_size;
            if (!crypto_state_) return plaintext_size;

            // for all methods other than AESGCM just return plaintext
            if (crypto_state_->m_keyInfo.getProtocol() != CONDOR_AESGCM) return plaintext_size;

            int cs = ((Condor_Crypt_AESGCM*)crypto_)->ciphertext_size_with_cs(plaintext_size, &(crypto_state_->m_stream_crypto_state));
            dprintf(D_NETWORK, "Sock::ciphertext_size: went from plaintext_size %i to ciphertext_size %i.\n", plaintext_size, cs);
            return cs;
        }

        //------------------------------------------
        // PURPOSE: set sock to use a particular encryption key
        // REQUIRE: KeyInfo -- a wrapper for keyData
        // RETURNS: true -- success; false -- failure
        //------------------------------------------

        bool zkm_wrap(const unsigned char* input, int input_len,
                  unsigned char*& output, int& outputlen);
        bool wrap(const unsigned char* input, int input_len,
                  unsigned char*& output, int& outputlen);
        //------------------------------------------
        // PURPOSE: encrypt some data
        // REQUIRE: Protocol, keydata. set_encryption_procotol
        //          must have been called and encryption_mode is on
        // RETURNS: TRUE -- success, FALSE -- failure
        //------------------------------------------

        bool zkm_unwrap(const unsigned char* input, int input_len,
                    unsigned char*& output, int& outputlen);
        bool unwrap(const unsigned char* input, int input_len,
                    unsigned char*& output, int& outputlen);
        //------------------------------------------
        // PURPOSE: decrypt some data
        // REQUIRE: Protocol, keydata. set_encryption_procotol
        //          must have been called and encryption_mode is on
        // RETURNS: TRUE -- success, FALSE -- failure
        //------------------------------------------

        //----------------------------------------------------------------------
        // MAC/MD related stuff
        //----------------------------------------------------------------------
        bool set_MD_mode(CONDOR_MD_MODE mode, KeyInfo * key = 0, const char * keyid = 0);    
        //virtual bool set_MD_off() = 0;
        //------------------------------------------
        // PURPOSE: set mode for MAC (on or off)
        // REQUIRE: mode -- see the enumeration defined above
        //          key  -- an optional key for the MAC. if null (by default)
        //                  all CEDAR does is send a Message Digest over
        //                  When key is specified, this is essentially a MAC
        // RETURNS: true -- success; false -- false
        //------------------------------------------

        bool isOutgoing_Hash_on() const { return (mdMode_ == MD_ALWAYS_ON); }
        //------------------------------------------
        // PURPOSE: whether MD is turned on or not
        // REQUIRE: None
        // RETURNS: true -- MD is on; 
        //          false -- MD is off
        //------------------------------------------

        virtual const char * isIncomingDataHashed() = 0;
        //------------------------------------------
        // PURPOSE: To check to see if incoming data
        //          has checksum/. NOTE! Currently,
        //          this method should be used with UDP only!
        // REQUIRE: None
        // RETURNS: NULL -- data does not contain hash
        //          key id -- if the data is checksumed
        //------------------------------------------

	/*
	**	Stream protocol
	*/

    /// peer's port and IP address in a struct sockaddr_in.
	condor_sockaddr peer_addr() const;

	/// peer's port number 
	int peer_port() const;

	/// peer's IP address, string verison (e.g. "128.105.101.17")
	const char* peer_ip_str() const;

	/// peer's IP address, integer version (e.g. 2154390801)

	/// is peer a local interface, aka did this connection originate from a local process?
	bool peer_is_local() const;

    /// my port and IP address in a class condor_sockaddr
	condor_sockaddr my_addr() const;
	condor_sockaddr my_addr_wildcard_okay() const;


	/// my IP address, string version (e.g. "128.105.101.17")
	virtual const char* my_ip_str() const;

	/// local port number
	int get_port() const;

    /// sinful address of mypoint() in the form of "<a.b.c.d:pppp>"
    char const * get_sinful() const;

	/// Sinful address for access from outside of our private network.
	/// This takes into account TCP_FORWARDING_HOST.
	char const *get_sinful_public() const;

	/// sinful address of peer in form of "<a.b.c.d:pppp>"
	char const * get_sinful_peer() const;

		// Address that was passed to connect().  This is useful in cases
		// such as CCB or shared port where our peer address is not the
		// address one would use to actually connect to the peer.
		// Returns NULL if connect was never called.
	char const *get_connect_addr() const;

	/// sinful address of peer, suitable for passing to dprintf() (never NULL)
	virtual char const *default_peer_description() const;

	/// local file descriptor (fd) of this socket
	int get_file_desc() const { return _sock; }

	/// is a non-blocking connect outstanding?
	bool is_connect_pending() const { return _state == sock_connect_pending || _state == sock_connect_pending_retry || _state == sock_reverse_connect_pending; }

	bool is_reverse_connect_pending() const { return _state == sock_reverse_connect_pending; }

	/// is the socket connected?
	bool is_connected() const { return _state == sock_connect; }

    /// 
	virtual ~Sock();

	/// Copy constructor -- this also dups the underlying socket
	Sock(const Sock &);

	void doNotEnforceMinimalCONNECT_TIMEOUT() ;		// Used by HA Daemon

	const char * getFullyQualifiedUser() const;

		/// Get user portion of fqu
	const char *getOwner() const;

		/// Get domain portion of fqu
	const char *getDomain() const;

	void setFullyQualifiedUser(char const *fqu);

	void setAuthenticationMethodUsed(char const *auth_method);
	const char *getAuthenticationMethodUsed() const;

	void setAuthenticationMethodsTried(char const *auth_methods);
	const char *getAuthenticationMethodsTried() const;

	void setAuthenticatedName(char const *auth_name);
	const char *getAuthenticatedName() const;

	bool hasAuthorizationBoundingSet() const;
	bool isAuthorizationInBoundingSet(const std::string &) const;

	void setCryptoMethodUsed(char const *crypto_method);
	const char* getCryptoMethodUsed() const;

	void setSessionID(const std::string &session_id) {_session = session_id;}
	const std::string &getSessionID() const {return _session;}

	void getPolicyAd(classad::ClassAd &ad) const;
	const classad::ClassAd *getPolicyAd() const {return _policy_ad;}
	void setPolicyAd(const classad::ClassAd &ad);

		/// True if socket has tried to authenticate or socket is
		/// using a security session that tried to authenticate.
		/// Authentication may or may not have succeeded and
		/// fqu may or may not be set.  (For example, we may
		/// have authenticated but the method was not mutual,
		/// so the other side knows who we are, but we do not
		/// know who the other side is.)
	bool triedAuthentication() const { return _tried_authentication; }

	void setTriedAuthentication(bool toggle) { _tried_authentication = toggle; }

		// True if the socket failed to authenticate with the remote
		// server but may succeed with a token request workflow.
	bool shouldTryTokenRequest() const { return _should_try_token_request; }
	void setShouldTryTokenRequest(bool val) { _should_try_token_request = val; }

		// Trust domain of the remote host (empty if unknown).
	void setTrustDomain(const std::string &trust_domain) { _trust_domain = trust_domain; }
	const std::string &getTrustDomain() const { return _trust_domain; }

		/// Returns true if the fully qualified user name is
		/// a non-anonymous user name (i.e. something not from
		/// the unmapped domain)
	bool isMappedFQU() const;

		/// Returns true if the fully qualified user name was
		/// authenticated
	bool isAuthenticated() const;

    ///
	virtual int authenticate(const char * auth_methods, CondorError* errstack, int timeout, bool non_blocking);
    ///
	// method_used should be freed by the caller when finished with it
	virtual int authenticate(KeyInfo *&ki, const char * auth_methods, CondorError* errstack, int timeout, bool non_blocking, char **method_used);

	// method_used should be freed by the caller when finished with it
	virtual int authenticate_continue(CondorError* errstack, bool non_blocking, char **method_used);

	/// if we are connecting, merges together Stream::get_deadline
	/// and connect_timeout_time()
	virtual time_t get_deadline() const;

	void invalidateSock();

	unsigned int getUniqueId() const { return m_uniqueId; }

#ifdef WIN32
	bool set_inheritable( bool flag );
#else
	// On unix, sockets are always inheritable
	bool set_inheritable(bool) { return true;}
#endif

//	PRIVATE INTERFACE TO ALL SOCKS
//
protected:
	


	/*
	**	Type definitions
	*/

	enum sock_state { sock_virgin, sock_assigned, sock_bound, sock_connect,
					  sock_writemsg, sock_readmsg, sock_special, 
					  sock_connect_pending, sock_connect_pending_retry,
					  sock_reverse_connect_pending };


	/*
	**	Methods
	*/

    ///
	Sock();

	int getportbyserv(char *);

    /**
        @param host the host to connect to, can be sinful form with port, in
        @param port the port to connect to, ignored if host contains a port
		@param non_blocking_flag if set to true, do not block
		@return FALSE on failure, TRUE if connected, CEDAR_EWOULDBLOCK if
			non_blocking_flag is set to true.  In current implementation,
		    will never return TRUE if non_blocking_flag was set, even
			if the connect operation succeeds immediately without blocking.
			When returning CEDAR_EWOULDBLOCK, Sock::is_connect_pending()
			will also be true.  In this case, the caller is expected
			to ensure that do_connect_finish() is called when the
			connect operation succeeds/fails or when connect_timeout_time()
			is reached.  For DaemonCore applications, this is handled
			by passing the socket to DaemonCore's Register_Socket() function.
    */
	int do_connect(char const *host, int port, bool non_blocking_flag = false, CondorError * errorStack = NULL);

	/**
	   Called internally by do_connect() to examine the address and
	   see if we should try a reversed connection (CCB) instead of a
	   normal forward connection.  If so, initiates the reversed
	   connection.  If not, returns CEDAR_ENOCCB.  Also checks to
	   see if we are connecting to a shared port (SharedPortServer)
       requiring further directions to route us to our final destination.
	*/
	int special_connect(char const *host,int port,bool nonblocking,CondorError * errorStack);

	virtual int do_reverse_connect(char const *ccb_contact,bool nonblocking,CondorError * errorStack) = 0;
	virtual void cancel_reverse_connect() = 0;
	virtual int do_shared_port_local_connect( char const *shared_port_id,bool nonblocking,char const *sharedPortIP ) = 0;

	void set_connect_addr(char const *addr);

	inline SOCKET get_socket (void) const { return _sock; }
	const char * deserialize(const char *);
	static void close_serialized_socket(char const *buf);
	void serialize(std::string& outbuf) const;
    const char * deserializeCryptoInfo(const char * buf);
    void serializeCryptoInfo(std::string& outbuf) const;
    const char * deserializeMdInfo(const char * buf);
    void serializeMdInfo(std::string& outbuf) const;
        
	virtual int encrypt(bool);
	///
	virtual int hdr_encrypt();
	///
	virtual bool is_hdr_encrypt();
    ///
	virtual bool is_encrypt();

    ///
	bool test_connection();
	/// get timeout time for pending connect operation;
	time_t connect_timeout_time() const;

    /// called whenever the bound or connected state changes
    void addr_changed();

	virtual void setTargetSharedPortID( char const *id ) = 0;
	virtual bool sendTargetSharedPortID() = 0;

	bool enter_connected_state(char const *op="CONNECT");

	virtual bool init_MD(CONDOR_MD_MODE mode, KeyInfo * key, const char * keyId) = 0;
	virtual bool set_encryption_id(const char * keyId) = 0;
	const KeyInfo& get_crypto_key() const;
	const KeyInfo& get_md_key() const;
	void resetCrypto();
	virtual bool canEncrypt() const;
	virtual bool mustEncrypt() const;

	/*
	**	Data structures
	*/

	SOCKET			_sock;
	sock_state		_state;
	int				_timeout;
	condor_sockaddr			_who;	// endpoint of "connection"
	char *			m_connect_addr;
	char *          _fqu;
	char *          _fqu_user_part;
	char *          _fqu_domain_part;
	char *          _auth_method;
	char *          _auth_methods;
	char *          _auth_name;
	char *          _crypto_method;
	std::string     _session;
	classad::ClassAd *_policy_ad;
	bool            _tried_authentication;
	bool            _should_try_token_request{false};
	std::string	_trust_domain;
	std::unordered_set<std::string> m_authz_bound;

	bool ignore_connect_timeout;	// Used by HA Daemon

	// Buffer to hold the string version of our own IP address. 
	mutable char _my_ip_buf[IP_STRING_BUF_SIZE];

	Condor_Crypt_Base    * crypto_;         // The actual crypto object
	Condor_Crypto_State  * crypto_state_;   // The object state
	CONDOR_MD_MODE      mdMode_;        // MAC mode
	KeyInfo           * mdKey_;

	static bool guess_address_string(char const* host, int port, condor_sockaddr& addr);

	unsigned int m_uniqueId;
	static unsigned int m_nextUniqueId;

	// Helper function: if host is a Sinful string with an addrs attribute,
	// return a Sinful string in addr with the primary address rewritten to
	// be the best address in addrs.
	static bool chooseAddrFromAddrs( const char * host, std::string & addr, condor_sockaddr* saddr = nullptr );

private:
	bool initialize_crypto(KeyInfo * key);
        //------------------------------------------
        // PURPOSE: initialize crypto
        // REQUIRE: KeyInfo
        // RETURNS: None
        //------------------------------------------

	int _condor_read(SOCKET fd, char *buf, int sz, int timeout);
	int _condor_write(SOCKET fd, const char *buf, int sz, int timeout);
	int bindWithin(condor_protocol proto, const int low, const int high);
	///
	// Buffer to hold the string version of our peer's IP address. 
	mutable char _peer_ip_buf[IP_STRING_BUF_SIZE];

	// Buffer to hold the sinful address of our peer
	mutable std::string _sinful_peer_buf;

	// Buffer to hold the sinful address of ourself
	mutable std::string _sinful_self_buf;

	// Buffer to hold the public sinful address of ourself
	mutable std::string _sinful_public_buf;

	// struct to hold state info for do_connect() method
	struct connect_state_struct {
			int retry_timeout_interval;   // amount of time to keep retrying
			bool connect_failed, failed_once;
			bool connect_refused;
			time_t first_try_start_time;
			time_t this_try_timeout_time; // timestamp of connect timeout
			time_t retry_timeout_time;    // timestamp when to give up retrying
			time_t retry_wait_timeout_time; // when to try connecting again
			int	old_timeout_value;
			bool non_blocking_flag;
			char *host;
			int port;
			char *connect_failure_reason;
	} connect_state;

	/**
	   This function is called by do_connect() to manage the whole process
	   of connecting the socket, including retries if necessary.  This is
	   also expected to be called by whoever manages the application's
	   select loop (e.g. DaemonCore) for non-blocking connections.
	   @return same as do_connect()
	 **/
	int do_connect_finish();

	/**
		@return
		   false
		     connect_state.connect_failed = true   # connection failed
		     connect_state.connect_failed = false  # connection pending
		   true                                    # connection has been made

		   In addition, if connect fails and connect_state.connect_refused
		   is true, this indicates that retry attempts are futile.
	**/
	bool do_connect_tryit();

	/**
	   @param reason Descriptive message indicating what went wrong with
	                 the connection attempt.
	 **/
	void setConnectFailureReason(char const *reason);

	/**
	   @param error The errno that was encountered when attempting to connect
	   @param syscall The system call that failed (e.g. "connect" or "select")
	 **/
	void setConnectFailureErrno(int error,char const *syscall);

	/**
	   This function logs a failed connection attempt, using the failure
	   description previously set by setConnectFailureReason or
	   setConnectFailureErrno.
	   @param timed_out True if we failed due to timeout.
	**/
	void reportConnectionFailure(bool timed_out) const;

	/**
	   This function puts the socket back in a state suitable for another
	   connection attempt.
	 **/
	void cancel_connect();

	void computeAuthorizationBoundingSet() const;
};

void dprintf ( int flags, const Sock & sock, const char *fmt, ... ) CHECK_PRINTF_FORMAT(3,4);

#endif /* SOCK_H */
