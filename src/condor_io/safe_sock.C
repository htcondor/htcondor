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

/* Note: this code needs to have error handling overhauled */

#define _POSIX_SOURCE

#include "condor_common.h"
#include "condor_constants.h"
#include "condor_io.h"
#include "condor_debug.h"
#include "internet.h"


SafeSock::SafeSock(					/* listen on port		*/
	int		port
	)
	: Sock()
{
	listen(port);
}


SafeSock::SafeSock(					/* listen on serv		*/
	char	*serv
	)
	: Sock()
{
	listen(serv);
}


SafeSock::SafeSock(
	char	*host,
	int		port,
	int		timeout_val
	)
	: Sock()
{
	timeout(timeout_val);
	connect(host, port);
}



SafeSock::SafeSock(
	char	*host,
	char	*serv,
	int		timeout_val
	)
	: Sock()
{
	timeout(timeout_val);
	connect(host, serv);
}



SafeSock::~SafeSock()
{
	close();
}



int SafeSock::listen()
{
	if (!valid()) return FALSE;

	if (_state != sock_bound) return FALSE;
	if (::listen(_sock, 5) < 0) return FALSE;

	_state = sock_special;
	_special_state = safesock_listen;

	return TRUE;
}


int SafeSock::handle_incoming_packet()
{
	/* do not queue up more than one message at a time on safe sockets */
	/* but return 1, because old message can still be read.	           */
	if (rcv_msg.ready) return TRUE;

	if (!rcv_packet(_sock)) return FALSE;

	return TRUE;
}



int SafeSock::end_of_message()
{
	int ret_val = FALSE;

	switch(_coding){
		case stream_encode:
			if (!snd_msg.buf.empty()){
				return snd_packet(_sock);
			}
			break;

		case stream_decode:
			if ( rcv_msg.ready ) {
				if ( rcv_msg.buf.consumed() )
					ret_val = TRUE;
				rcv_msg.ready = FALSE;
				rcv_msg.buf.reset();
			}
			break;

		default:
			assert(0);
	}

	return ret_val;
}



int SafeSock::connect(
	char	*host,
	int		port
	)
{
	struct hostent	*hostp = NULL;
	unsigned long	inaddr = 0;

	if (!valid()) return FALSE;
	if (!host || port < 0) return FALSE;

		/* we bind here so that a sock may be	*/
		/* assigned to the stream if needed		*/
	if (_state == sock_virgin || _state == sock_assigned) bind();

	if (_state != sock_bound) return FALSE;
	
	memset(&_who, 0, sizeof(sockaddr_in));
	_who.sin_family = AF_INET;
	_who.sin_port = htons((u_short)port);

	/* try to get a decimal notation first 			*/

	inaddr = inet_addr(host);

	if( inaddr != (unsigned int)(-1) ) {
		memcpy((char *)&_who.sin_addr, &inaddr, sizeof(inaddr));
	} else {
			/* if dotted notation fails, try host database	*/
		hostp = gethostbyname(host);
		if( hostp == (struct hostent *)0 ) {
			return FALSE;
		} else {
			memcpy(&_who.sin_addr, hostp->h_addr, sizeof(hostp->h_addr));
		}
	}

	_state = sock_connect;
	return TRUE;
}

int SafeSock::put_bytes(
	const void	*dta,
	int			sz
	)
{
	int		tw;
	int		nw;

	if (!valid()) return -1;

	for(nw=0;;){

		if (snd_msg.buf.full()){
			return FALSE;
		}

/*
		if (snd_msg.buf.empty()){
			snd_msg.buf.seek(5);
		}
*/

		if ((tw = snd_msg.buf.put_max(&((char *)dta)[nw], sz-nw)) < 0)
			return -1;
		nw += tw;
		if (nw == sz) break;
	}

	return nw;
}


int SafeSock::get_bytes(
	void		*dta,
	int			max_sz
	)
{
	if (!valid()) return -1;

	while (!rcv_msg.ready) {
		if (!handle_incoming_packet()){
			return FALSE;
		}
	}

	return rcv_msg.buf.get_max(dta, max_sz);
}


int SafeSock::get_ptr(
	void		*&ptr,
	char		delim
	)
{
	int nr;
	int tr;

	if (!valid()) return -1;

	while (!rcv_msg.ready){
		if (!handle_incoming_packet()) return FALSE;
	}

	if ((tr = rcv_msg.buf.find(delim)) < 0) {
		return -1;
	}
	ptr = rcv_msg.buf.get_ptr();
	nr = rcv_msg.buf.seek(0);
	rcv_msg.buf.seek(nr+tr+1);
	return tr+1;
}


int SafeSock::peek(
	char		&c
	)
{
	if (!valid()) return -1;

	while (!rcv_msg.ready) {
		if (!handle_incoming_packet()) return FALSE;
	}

	return rcv_msg.buf.peek(c);
}

int SafeSock::rcv_packet(
	SOCKET _sock
	)
{
	char	tmp[65536];
	int		len, fromlen;

	fromlen = sizeof(struct sockaddr_in);

	if (_timeout > 0) {
		struct timeval	timer;
		fd_set			readfds;
		int				nfds=0, nfound;
		timer.tv_sec = _timeout;
		timer.tv_usec = 0;
#if !defined(WIN32) // nfds is ignored on WIN32
		nfds = _sock + 1;
#endif
		FD_ZERO( &readfds );
		FD_SET( _sock, &readfds );

		nfound = select( nfds, &readfds, 0, 0, &timer );

		switch(nfound) {
		case 0:
			return -1;
			break;
		case 1:
			break;
		default:
			dprintf( D_ALWAYS, "select returns %d, recv failed\n",
				nfound );
			return -1;
			break;
		}
	}

	if ((len = recvfrom(_sock, tmp, 65536, 0,
						(struct sockaddr *)&_who,&fromlen)) < 0) {
		return FALSE;
	}
	if (!rcv_msg.buf.put_max(tmp, len)) {
		dprintf(D_ALWAYS, "IO: Packet storing failed\n");
		return FALSE;
	}

	rcv_msg.ready = TRUE;

	return TRUE;
}


int SafeSock::snd_packet(
	int		_sock
	)
{
	int		ns;

	ns = snd_msg.buf.num_used();

	if (sendto(_sock, (const char *)snd_msg.buf.get_ptr(),
			   snd_msg.buf.num_used(), 0,
			   (const struct sockaddr *)&_who,
			   sizeof(struct sockaddr_in)) < 0) {
		return FALSE;
	}
	snd_msg.buf.reset();

	return TRUE;
}



int SafeSock::get_port()
{
	sockaddr_in	addr;
	int			addr_len;

	addr_len = sizeof(sockaddr_in);

	if (getsockname(_sock, (sockaddr *)&addr, &addr_len) < 0) return -1;

	return (int) ntohs(addr.sin_port);
}



int SafeSock::get_file_desc()
{
	return _sock;
}

#ifndef WIN32
	// interface no longer supported
int SafeSock::attach_to_file_desc(
	int		fd
	)
{
	if (_state != sock_virgin) return FALSE;

	_sock = fd;
	_state = sock_connect;
	return TRUE;
}
#endif

struct sockaddr_in *SafeSock::endpoint()
{
	return &_who;
}

char *SafeSock::endpoint_IP()
{
	int             i;
	static  char    buf[24];
	char    tmp_buf[10];
	char    *cur_byte;
	unsigned char   this_byte;

	buf[0] = '\0';
	cur_byte = (char *) &(_who.sin_addr);
	for (i = 0; i < sizeof(_who.sin_addr); i++) {
		this_byte = (unsigned char) *cur_byte;
		sprintf(tmp_buf, "%u.", this_byte);
		cur_byte++;
		strcat(buf, tmp_buf);
	}
	buf[strlen(buf) - 1] = '\0';
	return buf;
}

int SafeSock::endpoint_port()
{
	return (int) ntohs(_who.sin_port);
}

char * SafeSock::serialize(char *buf)
{
	char sinful_string[28];
	char *ptmp;

	if ( buf == NULL ) {
		// here we want to save our state into a buffer

		// first, get the state from our parent class
		char * parent_state = Sock::do_serialize();
		// now concatenate our state
		char * outbuf = new char[50];
		sprintf(outbuf,"*%d*%s",_special_state,sin_to_string(&_who));
		strcat(parent_state,outbuf);
		delete []outbuf;
		return( parent_state );
	}

	// here we want to restore our state from the incoming buffer

	// first, let our parent class restore its state
	ptmp = Sock::do_serialize(buf);
	assert( ptmp );
	sscanf(ptmp,"%d*%s",&_special_state,sinful_string);
	string_to_sin(sinful_string, &_who);

	return NULL;
}
