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

#ifndef RELI_SOCK_H
#define RELI_SOCK_H

#include "buffers.h"
#include "sock.h"
#include "condor_adtypes.h"
#include "condor_io.h"
#include "../condor_daemon_core.V6/condor_ipverify.h"

/*
**	R E L I A B L E    S O C K
*/

class Authentication;
class Condor_MD_MAC;
/** The ReliSock class implements the Sock interface with TCP. */

// Define a 'filesize_t' type and FILESIZE_T_FORMAT printf format string
#if defined HAS_INT64_T
  typedef int64_t filesize_t;
# define FILESIZE_T_FORMAT "%" PRId64

#elif defined HAS___INT64_T
  typedef __int64 filesize_t;
# define FILESIZE_T_FORMAT "%" PRId64

#else
  typedef long filesize_t;
# define FILESIZE_T_FORMAT "%l"
#endif

class ReliSock : public Sock {
	friend class Authentication;

//	PUBLIC INTERFACE TO RELIABLE SOCKS
//
public:

	friend class DaemonCore;

	/*
	**	Methods
	*/

	// Virtual socket services
	//
    ///
	virtual int handle_incoming_packet();

    ///
	virtual int end_of_message();

    /** Connect to a host on a port
        @param s can be a hostname or sinful string
        @param port the port to connect to, ignorred if s contains port
    */
	virtual int connect(char *s, int port=0, 
							bool do_not_block = false);


    ///
	ReliSock();

	/// Copy ctor
	ReliSock(const ReliSock &);

    ///
	~ReliSock();
    ///
	void init();				/* shared initialization method */

    ///
	int listen();
    ///
	inline int listen(int p) { if (!bind(p)) return FALSE; return listen(); }
    ///
	inline int listen(char *s) { if (!bind(s)) return FALSE; return listen(); }

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
	int put_bytes_raw( char *buffer, int length );
    ///
	int get_bytes_raw( char *buffer, int length );
    ///
	int put_bytes_nobuffer(char *buf, int length, int send_size=1);
    ///
	int get_bytes_nobuffer(char *buffer, int max_length, int receive_size=1);

    /// returns -1 on failure, 0 for ok
	int get_file( filesize_t *size, const char *destination, bool flush_buffers=false);
    /// returns -1 on failure, 0 for ok
	int get_file( filesize_t *size, int fd, bool flush_buffers=false);
    /// returns -1 on failure, 0 for ok
	int put_file( filesize_t *size, const char *source);
    /// returns -1 on failure, 0 for ok
	int put_file( filesize_t *size, int fd );
    ///
	float get_bytes_sent() { return _bytes_sent; }
    ///
	float get_bytes_recvd() { return _bytes_recvd; }
    ///
	void reset_bytes_sent() { _bytes_sent = 0; }
    ///
	void reset_bytes_recvd() { _bytes_recvd = 0; }

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
	void setOwner( const char * );
    ///
	int authenticate( const char* methods, CondorError* errstack );
    ///
	int authenticate( KeyInfo *& key, const char* methods, CondorError* errstack );
    ///
    virtual const char * getFullyQualifiedUser() const;
    ///
	const char *getOwner();
    ///
	const char *getDomain();
    ///
    const char * getHostAddress();
    ///
	int isAuthenticated();
    ///
	void unAuthenticate();
	///
	
	int encrypt(bool);
	///
	bool is_encrypt();
	///
	int hdr_encrypt();
	///
	bool is_hdr_encrypt();
	///
	int isClient() { return is_client; };

    const char * isIncomingDataMD5ed();


//	PROTECTED INTERFACE TO RELIABLE SOCKS
//
protected:

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

	/*
	**	Data structures
	*/

	class RcvMsg {
		
                CONDOR_MD_MODE  mode_;
                Condor_MD_MAC * mdChecker_;
		ReliSock      * p_sock; //preserve parent pointer to use for condor_read/write
		
	public:
		RcvMsg();
                ~RcvMsg();
		int rcv_packet(SOCKET, int);
		void init_parent(ReliSock *tmp){ p_sock = tmp; } 

		ChainBuf	buf;
		int			ready;
                bool init_MD(CONDOR_MD_MODE mode, KeyInfo * key);
	} rcv_msg;

	class SndMsg {
                CONDOR_MD_MODE  mode_;
                Condor_MD_MAC * mdChecker_;
		ReliSock      * p_sock;
	public:
		SndMsg();
                ~SndMsg();
		Buf			buf;
		int snd_packet(int, int, int);

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

	Authentication * authob;
	int is_client;
	char *hostAddr;
    char * fqu_;
};

#endif
