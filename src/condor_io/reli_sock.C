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



virtual int ReliSock::close()
{
	if (_state == sock_virgin) return FALSE;

	if (::close(_sock) < 0) return FALSE;

	_state = sock_virgin;
	return TRUE;
}



virtual int ReliSock::handle_incoming_packet()
{
	Buf		*tmp;
	char	hdr[5];
	int		end;
	int		len;
	int		len_t;
	int		i;

	/* if socket is listening, and packet is there, it is ready for accept */
	if (_state == sock_special && _special_state == relisock_listen)
		return 1;

	/* do not queue up more than one message at a time on reliable sockets */
	/* but return 1, because old message can still be read.	               */
	if (rcv_msg.ready) return 1;

	if (read(_sock, hdr, 5) != 5){
		return -1;
	}
	end = (int) ((char *)hdr)[0];
	memcpy(&len_t,  &hdr[1], 4);
	len = (int) ntohl(len_t);

	if (!(tmp = new Buf)) return -1;
	if (len > tmp->max_size()){ delete tmp; return -1; }

	if ((i = tmp->read_frm_fd(_sock, len)) != len){
		fprintf(stderr, "Read %d, was supp to read %d\n", i, len);
	}

	if (!rcv_msg.buf.put(tmp)) { delete tmp; return -1; }

	if (end) rcv_msg.ready = 1;

	return rcv_msg.ready;
}



virtual int ReliSock::end_of_message()
{
	char	hdr[5];
	int		tmp;
	int		end;
	int		len_t;
	int		len;

	switch(_coding){
		case stream_encode:

			hdr[0] = 1;
			tmp = htonl( snd_msg.buf.num_used()-5 );
			memcpy(&hdr[1], &tmp, 4);
			if ((len = snd_msg.buf.flush_to_fd(_sock, hdr, 5)) < 0){
				break;
			}
			end = (int) *( (char *) &hdr[0] );
			memcpy(&len_t,  &hdr[1], 4);
			len = (int) ntohl(len_t);
			return TRUE;

		case stream_decode:
			if (!rcv_msg.ready) break;
			if (rcv_msg.buf.consumed()){
				rcv_msg.ready = 0;
				rcv_msg.buf.reset();
				return TRUE;
			}
			break;

		default:
			assert(0);
	}

	return FALSE;
}



virtual int ReliSock::connect(
	char	*host,
	int		port
	)
{
	return do_connect(host, port);
}



virtual int ReliSock::put_bytes(
	const void	*dta,
	int			sz
	)
{
	int		tw;
	int		nw;
	char	hdr[5];
	int		tmp;
	int		end;
	int		len_t;
	int		len;

	if (!valid()) return -1;

	for(nw=0;;){

		if (snd_msg.buf.full()){
			hdr[0] = 0;
			tmp = htonl( snd_msg.buf.num_used()-5 );
			memcpy(&hdr[1], &tmp, 4);

			if ((len = snd_msg.buf.flush_to_fd(_sock, hdr, 5)) < 0){
				return FALSE;
			}
			end = (int) *( (char *) &hdr[0] );
			memcpy(&len_t,  &hdr[1], 4);
			len = (int) ntohl(len_t);
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


virtual int ReliSock::get_bytes(
	void		*dta,
	int			max_sz
	)
{
	if (!valid()) return -1;

	while (!rcv_msg.ready) handle_incoming_packet();

	return rcv_msg.buf.get(dta, max_sz);
}


virtual int ReliSock::get_ptr(
	void		*&ptr,
	char		delim
	)
{
	if (!valid()) return -1;

	while (!rcv_msg.ready) handle_incoming_packet();

	return rcv_msg.buf.get_tmp(ptr, delim);
}


virtual int ReliSock::peek(
	char		&c
	)
{
	if (!valid()) return -1;

	while (!rcv_msg.ready) handle_incoming_packet();

	return rcv_msg.buf.peek(c);
}
