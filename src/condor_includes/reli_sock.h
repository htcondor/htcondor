/* 
** Copyright 1993 by Miron Livny, Mike Litzkow, and Emmanuel Ackaouy.
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the copyright holders not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the 
** copyright holders make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE COPYRIGHT HOLDERS DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Author:  Emmanuel Ackaouy
**
*/ 


#ifndef RELI_SOCK_H
#define RELI_SOCK_H

#include "buffers.h"
#include "sock.h"

/*
**	R E L I A B L E    S O C K
*/

class ReliSock : public Sock {

//	PUBLIC INTERFACE TO RELIABLE SOCKS
//
public:

	friend class DaemonCore;

	/*
	**	Methods
	*/

	// Virtual socket services
	//
	virtual int handle_incoming_packet();
	virtual int end_of_message();
	virtual int connect(char *, int);


	// Reliable socket services
	//

	ReliSock() : Sock() {}		/* virgin reli_sock		*/
	ReliSock(int);				/* listen on port		*/
	ReliSock(char *);			/* listen on serv 		*/
	ReliSock(char *, int, int timeout_val=0);		/* connect to host/port	*/
	ReliSock(char *, char *, int timeout_val=0);	/* connect to host/serv	*/
	~ReliSock();

	int listen();
	inline int listen(int p) { if (!bind(p)) return FALSE; return listen(); }
	inline int listen(char *s) { if (!bind(s)) return FALSE; return listen(); }

	ReliSock *accept();
	int accept(ReliSock &);
	int accept(ReliSock *);

	int get_port();
	struct sockaddr_in *endpoint();

	int get_file_desc();

#ifndef WIN32
	// interface no longer supported 
	int attach_to_file_desc(int);
#endif

	/*
	**	Stream protocol
	*/

	virtual stream_type type() { return Stream::reli_sock; }

	//	byte operations
	//
	virtual int put_bytes(const void *, int);
	virtual int get_bytes(void *, int);
	virtual int get_ptr(void *&, char);
	virtual int peek(char &);


	
//	PRIVATE INTERFACE TO RELIABLE SOCKS
//
protected:

	/*
	**	Types
	*/

	enum relisock_state { relisock_listen };

	/*
	**	Methods
	*/
	virtual char * serialize(char *);
	inline char * serialize() { return(serialize(NULL)); }

	/*
	**	Data structures
	*/

	class RcvMsg {
	public:
		RcvMsg() : ready(0) {}
		int rcv_packet(SOCKET, int);

		ChainBuf	buf;
		int			ready;
	} rcv_msg;

	class SndMsg {
	public:
		int snd_packet(int, int, int);

		Buf			buf;
	} snd_msg;

	relisock_state	_special_state;
	struct sockaddr_in _who;  // updated when endpoint() called
};



#endif
