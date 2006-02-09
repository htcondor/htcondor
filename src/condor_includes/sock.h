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

#ifndef SOCK_H
#define SOCK_H

#include "condor_common.h"
#include "stream.h"
#include "CondorError.h"

// retry failed connects for CONNECT_TIMEOUT seconds
#define CONNECT_TIMEOUT 10

const int _ENDPOINT_BUF_SIZE = 16;

// Some error codes.  These should go to condor_errno.h once it is there.
const int CEDAR_EWOULDBLOCK = 666;

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

/*
We want to define a callback function to be invoked when certain actions happen upon a stream.  CedarHandler is the type of a callback function.   The following notation is a little strange.  It reads: Define a new type called "CedarHandler" to be "a function returning void with single argument pointer to Stream"
*/

typedef void (CedarHandler) (Stream *s);

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
	virtual int connect(char *host, int port=0, bool do_not_block = false) = 0;

	/** Connect the socket to a remote peer.
		@param host Hostname of the peer, either a DNS name or IP address.
		@param service The name of a service that represents a port address,
		which can be passed to getportbyserv().
	*/
	inline int connect(char *host, char *service, bool do_not_block = false) { 
		return connect(host,getportbyserv(service),do_not_block); 
	}


	/** Install this function as the asynchronous handler.  When a handler is installed, it is invoked whenever data arrives on the socket.  Setting the handler to zero disables asynchronous notification.  */

	int set_async_handler( CedarHandler *handler );

	
	//	Socket services
	//

	int assign(SOCKET =INVALID_SOCKET);
#if defined(WIN32) && defined(_WINSOCK2API_)
	int assign(LPWSAPROTOCOL_INFO);		// to inherit sockets from other processes
#endif
	int bind(int, int =0);
    int setsockopt(int, int, const char*, int); 

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

	static int set_timeout_multiplier(int secs);
	
	inline int bind(int is_outgoing, char *s) { return bind(is_outgoing, getportbyserv(s)); }
	int close();
	/** if any operation takes more than sec seconds, timeout
        call timeout(0) to set blocking mode (default)
        @param sec the number of seconds to wait before timing out
        @return previous timeout
    */
	int timeout(int sec);
    
	/*
	**	Stream protocol
	*/

    /// peer's port and IP address in a struct sockaddr_in.
	struct sockaddr_in *endpoint();

	/// peer's port number
	int endpoint_port();

	/// peer's IP address, string verison (e.g. "128.105.101.17")
	char* endpoint_ip_str();

	/// peer's IP address, integer version (e.g. 2154390801)
	unsigned int endpoint_ip_int();

    /// my port and IP address in a struct sockaddr_in
    /// @args: the address is returned via 'sin'
    /// @ret: 0 if succeed, -1 if failed
    int mypoint(struct sockaddr_in *sin);

	/// local port number
	int get_port();

	/// local ip address integer
	unsigned int get_ip_int();

    /// sinful address in the form of "<a.b.c.d:pppp>"
    char * Sock::get_sinful();

	/// local file descriptor (fd) of this socket
	int get_file_desc();

	/// is a non-blocking connect outstanding?
	bool is_connect_pending() { return _state == sock_connect_pending; }

	/// is the socket connected?
	bool is_connected() { return _state == sock_connect; }

    /// 
	virtual ~Sock();

	/// Copy constructor -- this also dups the underlying socket
	Sock(const Sock &);

	void doNotEnforceMinimalCONNECT_TIMEOUT() ;		// Used by HA Daemon

//	PRIVATE INTERFACE TO ALL SOCKS
//
protected:
	


	/*
	**	Type definitions
	*/

	enum sock_state { sock_virgin, sock_assigned, sock_bound, sock_connect,
						sock_writemsg, sock_readmsg, sock_special, 
						sock_connect_pending };


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
			non_blocking_flag is set to true and connection was not immediate.
    */
	int do_connect(char *host, int port, bool non_blocking_flag = false);

	bool do_connect_finish();


	inline SOCKET get_socket (void) { return _sock; }
	char * serialize(char *);
	char * serialize() const;
    char * serializeCryptoInfo(char * buf);
    char * serializeCryptoInfo() const;
    char * serializeMdInfo(char * buf);
    char * serializeMdInfo() const;
        
	virtual void setFullyQualifiedUser(char * u);
	///
	virtual const char * getFullyQualifiedUser();
	///
	virtual int encrypt(bool);
	///
	virtual int hdr_encrypt();
	///
	virtual bool is_hdr_encrypt();
    ///
	virtual int authenticate(const char * auth_methods, CondorError* errstack);
    ///
	virtual int authenticate(KeyInfo *&ki, const char * auth_methods, CondorError* errstack);
    ///
	virtual int isAuthenticated() const;
    ///
	virtual void unAuthenticate();
    ///
	virtual bool is_encrypt();
#ifdef WIN32
	int set_inheritable( int flag );
#else
	// On unix, sockets are always inheritable
	inline int set_inheritable( int ) { return TRUE; }
#endif

    ///
	bool test_connection();
	///
	time_t connect_timeout_time();
	///
	int move_descriptor_up();

	/*
	**	Data structures
	*/

	SOCKET			_sock;
	sock_state		_state;
	int				_timeout;
	struct sockaddr_in _who;	// endpoint of "connection"

	static int timeout_multiplier;

	bool ignore_connect_timeout;	// Used by HA Daemon
private:
	int _condor_read(SOCKET fd, char *buf, int sz, int timeout);
	int _condor_write(SOCKET fd, char *buf, int sz, int timeout);
	int bindWithin(const int low, const int high);
	///
	// Buffer to hold the string version of our endpoint's IP address. 
	char _endpoint_ip_buf[_ENDPOINT_BUF_SIZE];	

	// struct to hold state info for do_connect() method
	struct connect_state_struct {
			int timeout_interval;
			bool connect_failed, failed_once;
			bool connect_refused;
			time_t timeout_time;
			int	old_timeout_value;
			bool non_blocking_flag;
			char *host;
			int port;
	} connect_state;

	bool do_connect_tryit();

};

#endif /* SOCK_H */
