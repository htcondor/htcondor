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


#ifndef SOCK_H
#define SOCK_H

#include "stream.h"

/*
**	B A S E    S O C K
*/

class Sock : public Stream {

//	PUBLIC INTERFACE TO ALL SOCKS
//
public:

	/*
	**	Methods
	*/

	// Virtual socket services
	//

	virtual int handle_incoming_packet() { assert(0); return 0; }
	virtual int end_of_message() { assert(0); return 0; }
	inline int eom() { return end_of_message(); }
	virtual int close() { assert(0); return 0; }
	virtual int connect(char *, int) { assert(0); return 0; }
	inline int connect(char *h, char *s) { return connect(h,getportbyserv(s)); }



	//	Socket services
	//

	int assign(int =-1);
	int bind(int =0);
    int setsockopt(int, int, const char*, int); 
	inline int bind(char *s) { return bind(getportbyserv(s)); }
	inline int get_socket (void) { return _sock; }


	/*
	**	Stream protocol
	*/

	virtual ~Sock() {}


//	PRIVATE INTERFACE TO ALL SOCKS
//
protected:

	/*
	**	Type definitions
	*/

	enum sock_state { sock_virgin, sock_assigned, sock_bound, sock_connect,
						sock_writemsg, sock_readmsg, sock_special };


	/*
	**	Methods
	*/

	Sock() : Stream(),  _sock(-1), _state(sock_virgin) {}

	int getportbyserv(char *);
	int do_connect(char *, int);


	/*
	**	Data structures
	*/

	int				_sock;
	sock_state		_state;
};



#endif
