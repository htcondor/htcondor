#define _POSIX_SOURCE

#include "condor_common.h"
#include "condor_constants.h"
#include "condor_io.h"
#include "condor_debug.h"


unsigned long num_created = 0;
unsigned long num_deleted = 0;



void
sanity_check()
{
	dprintf(D_ALWAYS, "IO: Buffer management:\n");
	dprintf(D_ALWAYS, "IO:     created: %d\n", num_created);
	dprintf(D_ALWAYS, "IO:     deleted: %d\n\n", num_deleted);
}


Buf::Buf(
	int	sz
	)
{
	_dta = new char[sz];
	_dta_maxsz = sz;
	_dta_sz = 0;
	_dta_pt = 0;
	_next = NULL;
	num_created++;
}


Buf::~Buf()
{
	if (_dta) delete [] _dta;
	num_deleted++;
}



int Buf::write(
	SOCKET	sockd,
	int		sz,
	int		timeout
	)
{
	int	nw;

	if (sz < 0 || sz > num_untouched()) sz = num_untouched();

	if (timeout > 0) {
		struct timeval	timer;
		fd_set			writefds;
		int				nfds=0, nfound;
		timer.tv_sec = timeout;
		timer.tv_usec = 0;
#if !defined(WIN32) // nfds is ignored on WIN32
		nfds = sockd + 1;
#endif
		FD_ZERO( &writefds );
		FD_SET( sockd, &writefds );

		nfound = select( nfds, 0, &writefds, 0, &timer );

		switch(nfound) {
		case 0:
			return -1;
			break;
		case 1:
			break;
		default:
			dprintf( D_ALWAYS, "select returns %d, send failed\n",
				nfound );
			return -1;
			break;
		}
	}

	nw = send(sockd, &_dta[num_touched()], sz, 0);
	if (nw <= 0) return -1;

	_dta_pt += nw;
	return nw;
}


int Buf::flush(
	SOCKET	sockd,
	void	*hdr,
	int		sz,
	int		timeout
	)
{
/* DEBUG SESSION
	int		dbg_fd;
*/

	if (sz > max_size()) return -1;
	if (hdr && sz > 0){
		memcpy(_dta, hdr, sz);
	}


	rewind();

/* DEBUG SESSION
	if ((dbg_fd = open("trace.snd", O_WRONLY|O_APPEND|O_CREAT, 0700)) < 0){
		dprintf(D_ALWAYS, "IO: Error opening trace file\n");
		exit(1);
	}
	if (write(dbg_fd, _dta, _dta_maxsz) != _dta_maxsz){
		dprintf(D_ALWAYS, "IO: ERROR LOGGING\n");
		return FALSE;
	}
	::close(dbg_fd);
*/


	sz = write(sockd, -1, timeout);
	reset();

	return sz;
}


int Buf::read(
	SOCKET	sockd,
	int		sz,
	int		timeout
	)
{
	int	nr;
	int nro;

	if (sz < 0 || sz > num_free()){
		dprintf(D_ALWAYS, "IO: Buffer too small\n");
		return -1;
		/* sz = num_free(); */
	}

	for(nr=0;nr <sz;){

		if (timeout > 0) {
			struct timeval	timer;
			fd_set			readfds;
			int				nfds=0, nfound;
			timer.tv_sec = timeout;
			timer.tv_usec = 0;
#if !defined(WIN32) // nfds is ignored on WIN32
			nfds = sockd + 1;
#endif
			FD_ZERO( &readfds );
			FD_SET( sockd, &readfds );

			nfound = select( nfds, &readfds, 0, 0, &timer );

			switch(nfound) {
			case 0:
				dprintf( D_ALWAYS, "select timed out in Buf::read()\n" );
				return -1;
			case 1:
				break;
			default:
				dprintf( D_ALWAYS, "select returns %d, recv failed\n",
					nfound );
				return -1;
			}
		}

		nro = recv(sockd, &_dta[num_used()+nr], sz-nr, 0);

		if (nro <= 0) {
			dprintf( D_ALWAYS, "recv returned %d, errno = %d\n", 
					 nro, errno );
			return -1;
		}

		nr += nro;
	}

	_dta_sz += nr;


/* DEBUG SESSION
	if ((dbg_fd = open("trace.rcv", O_WRONLY|O_APPEND|O_CREAT, 0700)) < 0){
		dprintf(D_ALWAYS, "IO: Error opening trace file\n");
		exit(1);
	}
	if (write(dbg_fd, _dta, _dta_maxsz) != _dta_maxsz){
		dprintf(D_ALWAYS, "IO: ERROR LOGGING\n");
		return FALSE;
	}
	::close(dbg_fd);
*/

	return nr;
}



int Buf::put_max(
	const void	*dta,
	int			sz
	)
{
	if (sz > num_free()) sz = num_free();

	memcpy(&_dta[num_used()], dta, sz);

	_dta_sz += sz;
	return sz;
}


int Buf::get_max(
	void		*dta,
	int			sz
	)
{
	if (sz > num_untouched()) sz = num_untouched();

	memcpy(dta, &_dta[num_touched()], sz);

	_dta_pt += sz;
	return sz;
}


int Buf::find(
	char	delim
	)
{
	char	*tmp;

	if (!(tmp = (char *)memchr(&_dta[num_touched()], delim, num_untouched()))){
		return -1;
	}

	return (tmp - &_dta[num_touched()]);
}


int Buf::peek(
	char		&c
	)
{
	if (empty() || consumed()) return FALSE;

	c = _dta[num_touched()];
	return TRUE;
}



int Buf::seek(
	int		pos
	)
{
	int	tmp;

	tmp = _dta_pt;
	_dta_pt = (pos < 0) ? 0 : ((pos < _dta_maxsz) ? pos : _dta_maxsz-1);
	if (_dta_pt > _dta_sz) _dta_sz = _dta_pt;
	return tmp;
}



void ChainBuf::reset()
{
	Buf	*trav;
	Buf	*trav_n;

	if (_tmp) { delete [] _tmp; _tmp = (char *)0; }

	for(trav=_head; trav; trav=trav_n){
		trav_n = trav->next();
		delete trav;
	}

	_head = _tail = _curr = (Buf *)0;
}


int dbg_count = 0;

int ChainBuf::get(
	void	*dta,
	int		sz
	)
{
	int		last_incr;
	int		nr;

	if (dbg_count++ > 307){
		dbg_count--;
	}


	for(nr=0; _curr; _curr=_curr->next()){
		last_incr = _curr->get_max(&((char *)dta)[nr], sz-nr);
		nr += last_incr;
		if (nr == sz) break;
	}

	return nr;
}



int ChainBuf::put(
	Buf		*dta
	)
{
	if (_tmp) { delete [] _tmp; _tmp = (char *)0; }

	if (!_tail){
		_head = _tail = _curr = dta;
		dta->set_next((Buf *)0);
	}
	else{
		_tail->set_next(dta);
		_tail = dta;
		_tail->set_next((Buf *)0);
	}

	return TRUE;
}


int ChainBuf::get_tmp(
	void	*&ptr,
	char	delim
	)
{
	int	nr;
	int	tr;
	Buf	*trav;

	if (_tmp) { delete [] _tmp; _tmp = (char *)0; }

	if (!_curr) return -1;

	/* case 1: in one buffer */
	if ((tr = _curr->find(delim)) >= 0){
		ptr = _curr->get_ptr();
		nr = _curr->seek(0);
		_curr->seek(nr+tr+1);
		return tr+1;
	}

	/* case 2: string is in >1 buffers. */

	nr = _curr->num_untouched();
	if (!_curr->next()) return -1;

	for(trav = _curr->next(); trav; trav = trav->next()){
		if ((tr = trav->find(delim)) < 0){
			nr += trav->num_untouched();
		}
		else{
			nr += tr;
			if (!(_tmp = new char[nr+1])) return -1;
			get(_tmp, nr+1);
			ptr = _tmp;
			return nr+1;
		}
	}

	return -1;
}


int ChainBuf::peek(
	char	&c
	)
{
	if (_tmp) { delete [] _tmp; _tmp = (char *)0; }
	if (!_curr) return FALSE;


	if (!_curr->peek(c)){
		if (!(_curr = _curr->next())) return FALSE;
		return _curr->peek(c);
	}

	return TRUE;
}

