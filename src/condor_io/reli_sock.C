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




ReliSock::ReliSock(					/* listen on port		*/
	int		port
	)
	: Sock()
{
	listen(port);
}


ReliSock::ReliSock(					/* listen on serv		*/
	char	*serv
	)
	: Sock()
{
	listen(serv);
}


ReliSock::ReliSock(
	char	*host,
	int		port
	)
	: Sock()
{
	connect(host, port);
}



ReliSock::ReliSock(
	char	*host,
	char	*serv
	)
	: Sock()
{
	Sock::connect(host, serv);
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
	if (!valid()) return FALSE;
	if (!c) return FALSE;

	return accept(*c);
}



ReliSock *ReliSock::accept()
{
	ReliSock	*c_rs;
	int			c_sock;
	sockaddr	addr;
	int			addr_sz;

	if (!valid()) return (ReliSock *)0;
	if (_state != sock_special || _special_state != relisock_listen)
		return (ReliSock *)0;

	if (!(c_rs = new ReliSock())) return (ReliSock *)0;

	if ((c_sock = ::accept(_sock, (sockaddr *)&addr, &addr_sz)) < 0)
		return (ReliSock *)0;

	c_rs->_sock = c_sock;
	c_rs->_state = sock_connect;
	c_rs->decode();

	return c_rs;
}



int ReliSock::close()
{
	if (_state == sock_virgin) return FALSE;

	if (::close(_sock) < 0) return FALSE;

	_state = sock_virgin;
	return TRUE;
}



int ReliSock::handle_incoming_packet()
{
	/* if socket is listening, and packet is there, it is ready for accept */
	if (_state == sock_special && _special_state == relisock_listen)
		return TRUE;

	/* do not queue up more than one message at a time on reliable sockets */
	/* but return 1, because old message can still be read.	               */
	if (rcv_msg.ready) return TRUE;

	if (!rcv_msg.rcv_packet(_sock)) return FALSE;

	return TRUE;
}



int ReliSock::end_of_message()
{
	switch(_coding){
		case stream_encode:
			if (!snd_msg.buf.empty()){
				return snd_msg.snd_packet(_sock, TRUE);
			}
			break;

		case stream_decode:
			if (rcv_msg.ready && rcv_msg.buf.consumed()){
				rcv_msg.ready = FALSE;
				rcv_msg.buf.reset();
				return TRUE;
			}
			break;

		default:
			assert(0);
	}

	return FALSE;
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
			if (!snd_msg.snd_packet(_sock, FALSE)) return FALSE;
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
			fprintf(stderr, "HANDLE_INCOMING FAILED\n");
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
	int	_sock
	)
{
	Buf		*tmp;
	char	hdr[5];
	int		end;
	int		len, len_t;
	int		tmp_len;

	if (read(_sock, hdr, 5) != 5){
		fprintf(stderr, "READ FAILES\n");
		return FALSE;
	}
	end = (int) ((char *)hdr)[0];
	memcpy(&len_t,  &hdr[1], 4);
	len = (int) ntohl(len_t);

	if (!(tmp = new Buf)){
		fprintf(stderr, "Out of memory\n");
		return FALSE;
	}
	if (len > tmp->max_size()){
		delete tmp;
		fprintf(stderr, "Incoming packet is too big\n");
		return FALSE;
	}
	if ((tmp_len = tmp->read_frm_fd(_sock, len)) != len){
		delete tmp;
		fprintf(stderr, "Packet read failed: read %d of %d\n", tmp_len, len);
		return FALSE;
	}
	if (!buf.put(tmp)) {
		delete tmp;
		fprintf(stderr, "Packet storing failed\n");
		return FALSE;
	}

	if (end) ready = TRUE;

	return TRUE;
}


int ReliSock::SndMsg::snd_packet(
	int		_sock,
	int		end
	)
{
	char	hdr[5];
	int		len;
	int		ns;


	hdr[0] = (char) end;
	ns = buf.num_used()-5;
	len = (int) htonl(ns);

	memcpy(&hdr[1], &len, 4);
	if (buf.flush_to_fd(_sock, hdr, 5) != (ns+5)){
		return FALSE;
	}


	return TRUE;
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


int ReliSock::attach_to_file_desc(
	int		fd
	)
{
	if (_state != sock_virgin) return FALSE;

	_sock = fd;
	_state = sock_connect;
	return TRUE;
}
