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

#ifndef RELI_SOCK_H
#define RELI_SOCK_H

#include "buffers.h"
#include "sock.h"
#include "condor_adtypes.h"

/*
**	R E L I A B L E    S O C K
*/

class Authentication;

/** The ReliSock class implements the Sock interface with TCP. */


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
	int put_bytes_nobuffer(char *buf, int length, int send_size=1);
    ///
	int get_bytes_nobuffer(char *buffer, int max_length,
							 int receive_size=1);
    /// returns -1 on failure, else number of bytes transferred
	int get_file(const char *destination, bool flush_buffers=false);
    /// returns -1 on failure, else number of bytes transferred
	int put_file(const char *source);
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
	int authenticate();
    ///
	const char *getOwner();
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
	int  wrap(char* input, int input_len,char*& output,int& output_len);
	///
	int  unwrap(char* input,int input_len,char*& output, int& output_len);
	///
	int isClient() { return is_client; };

//	PROTECTED INTERFACE TO RELIABLE SOCKS
//
protected:

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
		
		ReliSock *p_sock; //preserve parent pointer to use for condor_read/write
		
	public:
		
		RcvMsg() : ready(0) {}
		int rcv_packet(SOCKET, int);
		void init_parent(ReliSock *tmp){ p_sock = tmp; } 
		
		ChainBuf	buf;
		int			ready;
	} rcv_msg;

	class SndMsg {
		ReliSock* p_sock;
	public:
		
		Buf			buf;
		int snd_packet(int, int, int);

		//function to support the use of condor_read /write
		
		void init_parent(ReliSock *tmp){ 
			p_sock = tmp; 
			buf.init_parent(tmp);
		} 

	} snd_msg;

	relisock_state	_special_state;
	int	ignore_next_encode_eom;
	int	ignore_next_decode_eom;
	float _bytes_sent, _bytes_recvd;

	Authentication * authob;
	int is_client;
	char *hostAddr;
};

#endif
