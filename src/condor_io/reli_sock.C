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



#define _POSIX_SOURCE

#include "condor_common.h"
#include "condor_constants.h"
#include "condor_io.h"
#include "condor_debug.h"
#include "internet.h"


ReliSock::ReliSock(					/* listen on port		*/
	int		port
	)
	: Sock()
{
	if (!listen(port))
		dprintf(D_ALWAYS, "failed to listen on port %d!\n", port);
}


ReliSock::ReliSock(					/* listen on serv		*/
	char	*serv
	)
	: Sock()
{
	if (!listen(serv))
		dprintf(D_ALWAYS, "failed to listen on serv %s!\n", serv);
}


ReliSock::ReliSock(
	char	*host,
	int		port,
	int		timeout_val
	)
	: Sock()
{
	timeout(timeout_val);
	if (!connect(host, port))
		dprintf(D_ALWAYS, "failed to connect to %s:%d!\n", host, port);
}



ReliSock::ReliSock(
	char	*host,
	char	*serv,
	int		timeout_val
	)
	: Sock()
{
	timeout(timeout_val);
	if (!Sock::connect(host, serv))
		dprintf(D_ALWAYS, "failed to connect to %s:%s!\n", host, serv);
}



ReliSock::~ReliSock()
{
	close();
}



int ReliSock::listen()
{
	if (!valid()) return FALSE;

	if (_state != sock_bound) return FALSE;
	if (::listen(_sock, 5) < 0) return FALSE;

	_state = sock_special;
	_special_state = relisock_listen;

	return TRUE;
}



int ReliSock::accept(
	ReliSock	&c
	)
{
	int			c_sock;
	sockaddr_in	addr;
	int			addr_sz;

	if (!valid()) return FALSE;
	if (_state != sock_special || _special_state != relisock_listen ||
													c._state != sock_virgin)
		return FALSE;

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
			return FALSE;
			break;
		case 1:
			break;
		default:
			dprintf( D_ALWAYS, "select returns %d, connect failed\n",
				nfound );
			return FALSE;
			break;
		}
	}

	addr_sz = sizeof(addr);
	if ((c_sock = ::accept(_sock, (sockaddr *)&addr, &addr_sz)) < 0)
		return FALSE;

	c._sock = c_sock;
	c._state = sock_connect;
	c.decode();

	return TRUE;
}



int ReliSock::accept(
	ReliSock	*c
	)
{
	if (!c) return FALSE;

	return accept(*c);
}



ReliSock *ReliSock::accept()
{
	ReliSock	*c_rs;
	int			c_sock;

	if (!(c_rs = new ReliSock())) return (ReliSock *)0;

	if ((c_sock = accept(*c_rs)) < 0) {
		delete c_rs;
		return (ReliSock *)0;
	}

	return c_rs;
}


int ReliSock::handle_incoming_packet()
{
	/* if socket is listening, and packet is there, it is ready for accept */
	if (_state == sock_special && _special_state == relisock_listen)
		return TRUE;

	/* do not queue up more than one message at a time on reliable sockets */
	/* but return 1, because old message can still be read.	               */
	if (rcv_msg.ready) return TRUE;

	if (!rcv_msg.rcv_packet(_sock, _timeout)) return FALSE;

	return TRUE;
}



int ReliSock::end_of_message()
{
	int ret_val = FALSE;

	switch(_coding){
		case stream_encode:
			if (!snd_msg.buf.empty()){
				return snd_msg.snd_packet(_sock, TRUE, _timeout);
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



int ReliSock::connect(
	char	*host,
	int		port
	)
{
	return do_connect(host, port);
}



int ReliSock::put_bytes(
	const void	*dta,
	int			sz
	)
{
	int		tw;
	int		nw;

	if (!valid()) return -1;

	for(nw=0;;){

		if (snd_msg.buf.full()){
			if (!snd_msg.snd_packet(_sock, FALSE, _timeout)) return FALSE;
		}
		if (snd_msg.buf.empty()){
			snd_msg.buf.seek(5);
		}

		if ((tw = snd_msg.buf.put_max(&((char *)dta)[nw], sz-nw)) < 0)
			return -1;
		nw += tw;
		if (nw == sz) break;
	}

	return nw;
}


int ReliSock::get_bytes(
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

	return rcv_msg.buf.get(dta, max_sz);
}


int ReliSock::get_ptr(
	void		*&ptr,
	char		delim
	)
{
	if (!valid()) return -1;

	while (!rcv_msg.ready){
		if (!handle_incoming_packet()) return FALSE;
	}

	return rcv_msg.buf.get_tmp(ptr, delim);
}


int ReliSock::peek(
	char		&c
	)
{
	if (!valid()) return -1;

	while (!rcv_msg.ready) {
		if (!handle_incoming_packet()) return FALSE;
	}

	return rcv_msg.buf.peek(c);
}

int ReliSock::RcvMsg::rcv_packet(
	SOCKET _sock,
	int _timeout
	)
{
	Buf		*tmp;
	char	hdr[5];
	int		end;
	int		len, len_t;
	int		tmp_len;

    len = 0;
    while (len < 5) {
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
        tmp_len = recv(_sock, hdr+len, 5-len, 0);
        if (tmp_len <= 0)
            return FALSE;
        len += tmp_len;
    }
	end = (int) ((char *)hdr)[0];
	memcpy(&len_t,  &hdr[1], 4);
	len = (int) ntohl(len_t);

	if (!(tmp = new Buf)){
		dprintf(D_ALWAYS, "IO: Out of memory\n");
		return FALSE;
	}
	if (len > tmp->max_size()){
		delete tmp;
		dprintf(D_ALWAYS, "IO: Incoming packet is too big\n");
		return FALSE;
	}
	if ((tmp_len = tmp->read(_sock, len, _timeout)) != len){
		delete tmp;
		dprintf(D_ALWAYS, "IO: Packet read failed: read %d of %d\n",
				tmp_len, len);
		return FALSE;
	}
	if (!buf.put(tmp)) {
		delete tmp;
		dprintf(D_ALWAYS, "IO: Packet storing failed\n");
		return FALSE;
	}

	if (end) ready = TRUE;

	return TRUE;
}


int ReliSock::SndMsg::snd_packet(
	int		_sock,
	int		end,
	int		_timeout
	)
{
	char	hdr[5];
	int		len;
	int		ns;


	hdr[0] = (char) end;
	ns = buf.num_used()-5;
	len = (int) htonl(ns);

	memcpy(&hdr[1], &len, 4);
	if (buf.flush(_sock, hdr, 5, _timeout) != (ns+5)){
		return FALSE;
	}


	return TRUE;
}

struct sockaddr_in * ReliSock::endpoint()
{
	int addr_len;

	addr_len = sizeof(sockaddr_in);

	if (getpeername(_sock, (struct sockaddr *) &_who, &addr_len) < 0) 
		return NULL;	// socket not connected

	return &_who;
}
		
int ReliSock::get_port()
{
	sockaddr_in	addr;
	int			addr_len;

	addr_len = sizeof(sockaddr_in);

	if (getsockname(_sock, (sockaddr *)&addr, &addr_len) < 0) return -1;

	return (int) ntohs(addr.sin_port);
}



int ReliSock::get_file_desc()
{
	return _sock;
}

#ifndef WIN32
	// interface no longer supported
int ReliSock::attach_to_file_desc(
	int		fd
	)
{
	if (_state != sock_virgin) return FALSE;

	_sock = fd;
	_state = sock_connect;
	return TRUE;
}
#endif

char * ReliSock::serialize(char *buf)
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
