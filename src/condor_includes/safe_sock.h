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
** Author:  Jim Basney
**
*/ 


#ifndef SAFE_SOCK_H
#define SAFE_SOCK_H

#if defined(WIN32)
#include <winsock.h>
#else
#include <netinet/in.h>
#endif

#include "buffers.h"
#include "sock.h"
#include "condor_constants.h"

/*
**	S A F E    S O C K
*/

class SafeSock : public Sock {

//	PUBLIC INTERFACE TO SAFE SOCKS
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
	inline int connect(char *h, char *s) { return connect(h,getportbyserv(s));}


	// Safe socket services
	//

	SafeSock() : Sock() {}		/* virgin safe_sock		*/
	SafeSock(int);				/* listen on port		*/
	SafeSock(char *);			/* listen on serv 		*/
	SafeSock(char *, int, int timeout_val=0);		/* connect to host/port	*/
	SafeSock(char *, char *, int timeout_val=0);	/* connect to host/serv	*/
	~SafeSock();

	int listen();
	inline int listen(int p) { if (!bind(p)) return FALSE; return listen(); }
	inline int listen(char *s) { if (!bind(s)) return FALSE; return listen(); }

	int get_port();
	struct sockaddr_in *endpoint();
	char *endpoint_IP();
	int endpoint_port();

#if 0 // interface no longer supported
	int attach_to_file_desc(int);
#endif
	
	/*
	**	Stream protocol
	*/

	virtual stream_type type() { return Stream::safe_sock; }

	//	byte operations
	//
	virtual int put_bytes(const void *, int);
	virtual int get_bytes(void *, int);
	virtual int get_ptr(void *&, char);
	virtual int peek(char &);


//	PRIVATE INTERFACE TO SAFE SOCKS
//
protected:

	/*
	**	Types
	*/

	enum safesock_state { safesock_listen };

	/*
	**	Methods
	*/
	char * serialize(char *);
	inline char * serialize() { return(serialize(NULL)); }
	int get_file_desc();

	/*
	**	Data structures
	*/

	int rcv_packet(SOCKET);
	class RcvMsg {
	public:
		RcvMsg() : buf(65536), ready(0) {}

		Buf			buf;
		int			ready;
	} rcv_msg;

	int snd_packet(int);
	class SndMsg {
	public:
		SndMsg() : buf(65536) {}

		Buf			buf;
	} snd_msg;

	safesock_state	_special_state;
	struct sockaddr_in _who;	// endpoint of "connection"
};



#endif
