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

 

/* Note: this code needs to have error handling overhauled */

#include "condor_common.h"
#include "condor_constants.h"
#include "condor_io.h"
#include "condor_debug.h"
#include "internet.h"

/* 
   NOTE: All SafeSock constructors initialize with this, so you can
   put any shared initialization code here.  -Derek Wright 3/12/99
*/
void
SafeSock::init()
{
	_special_state = safesock_none;
}


SafeSock::SafeSock() 				/* virgin safesock	*/
	: Sock()
{
	init();
}

SafeSock::SafeSock(const SafeSock & orig) 
	: Sock(orig)
{
	init();
	// now copy all cedar state info via the serialize() method
	char *buf = NULL;
	buf = orig.serialize();	// get state from orig sock
	assert(buf);
	serialize(buf);			// put the state into the new sock
	delete [] buf;
}

SafeSock::~SafeSock()
{
	close();
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

	if (!host || port < 0) return FALSE;

		/* we bind here so that a sock may be	*/
		/* assigned to the stream if needed		*/
	if (_state == sock_virgin || _state == sock_assigned) bind();

	if (_state != sock_bound) return FALSE;
	
	memset(&_who, 0, sizeof(sockaddr_in));
	_who.sin_family = AF_INET;
	_who.sin_port = htons((u_short)port);

	/* might be in <x.x.x.x:x> notation, i.e. sinfull string */
	if (host[0] == '<') {
		string_to_sin(host, &_who);
	}
	/* try to get a decimal notation first 			*/
	else if( (inaddr=inet_addr(host)) != (unsigned int)(-1) ) {
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
			return FALSE;
			break;
		case 1:
			break;
		default:
			dprintf( D_ALWAYS, "select returns %d, recv failed\n",
				nfound );
			return FALSE;
			break;
		}
	}

	if ((len = recvfrom(_sock, tmp, 65536, 0,
						(struct sockaddr *)&_who,&fromlen)) < 0) {
		return FALSE;
	}
	dprintf( D_NETWORK, "RECV %s ", sock_to_string(_sock) );
	dprintf( D_NETWORK|D_NOHEADER, "%s\n", sin_to_string(&_who) );
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
	dprintf( D_NETWORK, "SEND %s ", sock_to_string(_sock) );
	dprintf( D_NETWORK|D_NOHEADER, "%s\n", sin_to_string(&_who) );
	snd_msg.buf.reset();

	return TRUE;
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

char * SafeSock::serialize() const
{
	// here we want to save our state into a buffer

	// first, get the state from our parent class
	char * parent_state = Sock::serialize();
	// now concatenate our state
	char * outbuf = new char[50];
	sprintf(outbuf,"*%d*%s",_special_state,sin_to_string(&_who));
	strcat(parent_state,outbuf);
	delete []outbuf;
	return( parent_state );
}

char * SafeSock::serialize(char *buf)
{
	char sinful_string[28];
	char *ptmp;

	assert(buf);

	// here we want to restore our state from the incoming buffer

	// first, let our parent class restore its state
	ptmp = Sock::serialize(buf);
	assert( ptmp );
	sscanf(ptmp,"%d*%s",&_special_state,sinful_string);
	string_to_sin(sinful_string, &_who);

	return NULL;
}
