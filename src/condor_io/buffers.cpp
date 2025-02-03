/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

#include "condor_common.h"
#include "condor_io.h"
#include "condor_debug.h"
#include "condor_md.h"
#include "condor_rw.h"

unsigned long num_created = 0;
unsigned long num_deleted = 0;



void
sanity_check()
{
	dprintf(D_ALWAYS, "IO: Buffer management:\n");
	dprintf(D_ALWAYS, "IO:     created: %lu\n", num_created);
	dprintf(D_ALWAYS, "IO:     deleted: %lu\n\n", num_deleted);
}


Buf::Buf(
	int	sz
	)
	: _dta{nullptr},
	_dta_sz{0},
	_dta_maxsz{sz},
	_dta_pt{0},
	_next{nullptr},
	p_sock{nullptr}
{
	num_created++;
}

Buf::Buf(Sock *sock, int sz) :
    Buf(sz)
{
	p_sock = sock;
}

Buf::~Buf()
{
	dealloc_buf();
	num_deleted++;
}

void
Buf::alloc_buf()
{
	if( !_dta ) {
		_dta = new char[_dta_maxsz];
	}
}

void
Buf::dealloc_buf()
{
	if (_dta) {
        delete [] _dta;
		_dta = NULL;
    }
}

void
Buf::grow_buf(int new_sz)
{
	if (new_sz < _dta_maxsz) return;
	char * new_dta = new char[new_sz];
	if (_dta) {
		memcpy(new_dta, _dta, _dta_sz);
		delete [] _dta;
	}
	_dta = new_dta;
	_dta_maxsz = new_sz;
}

int Buf::write(
	char const *peer_description,
	SOCKET	sockd,
	int		sz,
	time_t		timeout,
	bool	non_blocking
	)
{
	int	nw;
	alloc_buf();
	if (sz < 0 || sz > num_untouched()) {
		sz = num_untouched();
	}

	nw = condor_write(peer_description,sockd, &_dta[num_touched()], sz , timeout, 0, non_blocking);
	if (nw < 0) {
		dprintf( D_ALWAYS, "Buf::write(): condor_write() failed\n" );
		return -1;
	}

	_dta_pt += nw;

	return nw;
}


int Buf::flush(
	char const *peer_description,
	SOCKET	sockd,
	void	*hdr,
	int		sz,
	time_t		timeout,
	bool		non_blocking
	)
{
/* DEBUG SESSION
	int		dbg_fd;
*/

	alloc_buf();

	if (sz > max_size()) return -1;
	if (hdr && sz > 0){
		memcpy(_dta, hdr, sz);
	}


	rewind();

/* DEBUG SESSION
	if ((dbg_fd = safe_open_wrapper_follow("trace.snd", O_WRONLY|O_APPEND|O_CREAT, 0700)) < 0){
		dprintf(D_ALWAYS, "IO: Error opening trace file\n");
		exit(1);
	}
	if (write(dbg_fd, _dta, _dta_maxsz) != _dta_maxsz){
		dprintf(D_ALWAYS, "IO: ERROR LOGGING\n");
		return FALSE;
	}
	::close(dbg_fd);
*/


	sz = write(peer_description,sockd, -1, timeout, non_blocking);
	if (!non_blocking || consumed()) {
		reset();
	}

	return sz;
}


int Buf::read(
	char const *peer_description,
	SOCKET	sockd,
	int		sz,
	time_t		timeout,
	bool		non_blocking
	)
{
	int	nr;

	alloc_buf();

	if (sz < 0 || sz > num_free()){
		dprintf(D_ALWAYS, "IO: Buffer too small\n");
		return -1;
		/* sz = num_free(); */
	}

	nr = condor_read(peer_description,sockd,&_dta[num_used()],sz,timeout, 0, non_blocking);
	if (nr < 0) {
		dprintf( D_ALWAYS, "Buf::read(): condor_read() failed\n" );
		return nr;
	}

	_dta_sz += nr;

/* DEBUG SESSION
	if ((dbg_fd = safe_open_wrapper_follow("trace.rcv", O_WRONLY|O_APPEND|O_CREAT, 0700)) < 0){
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
	alloc_buf();
	if (sz > num_free()) sz = num_free();

	memcpy(&_dta[num_used()], dta, sz);

	_dta_sz += sz;
	return sz;
}


int Buf::put_force(
	const void *dta,
	int sz
	)
{
	int free = num_free();
	int needed = sz - free;
	if (needed > 0) {
		grow_buf(_dta_maxsz + needed);
	}
	memcpy(&_dta[num_used()], dta, sz);
	_dta_sz += sz;
	return sz;
}


int Buf::get_max(
	void		*dta,
	int			sz
	)
{
	alloc_buf();

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

	alloc_buf();

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

	alloc_buf();

	c = _dta[num_touched()];
	return TRUE;
}



int Buf::seek(
	int		pos
	)
{
	int	tmp;

	alloc_buf();

	tmp = _dta_pt;
	_dta_pt = (pos < 0) ? 0 : ((pos < _dta_maxsz) ? pos : _dta_maxsz-1);
	if (_dta_pt > _dta_sz) _dta_sz = _dta_pt;
	return tmp;
}

bool Buf::computeMD(char * checkSUM, Condor_MD_MAC * checker)
{
	alloc_buf();

    // normal cedar header is 5 bytes.  Add MAC_SIZE to that for this to work
    checker->addMD((unsigned char *) &(_dta[5+MAC_SIZE]), _dta_sz - (5+MAC_SIZE));
    unsigned char * md = checker->computeMD();

    if (md) {
        memcpy(checkSUM, md, MAC_SIZE);
        free(md);
        return true;
    }
    return false;
}

bool Buf::verifyMD(char * checkSUM, Condor_MD_MAC * checker)
{
	alloc_buf();

    checker->addMD((unsigned char *) &(_dta[0]), _dta_sz);

    return checker->verifyMD((unsigned char *) checkSUM);
}

void Buf::swap(Buf &other)
{
	char * tmp_dta = _dta;
	int tmp_dta_sz = _dta_sz;
	int tmp_dta_maxsz = _dta_maxsz;
	int tmp_dta_pt = _dta_pt;
		// NOTE: This will not work with a buf in ChainBuf
		// because we need to fixup anything pointing to us!
	Buf *tmp_next = _next;
	Sock *tmp_sock = p_sock;

	_dta = other._dta;
	_dta_sz = other._dta_sz;
	_dta_maxsz = other._dta_maxsz;
	_dta_pt = other._dta_pt;
	_next = other._next;
	p_sock = other.p_sock;

	other._dta = tmp_dta;
	other._dta_sz = tmp_dta_sz;
	other._dta_maxsz = tmp_dta_maxsz;
	other._dta_pt = tmp_dta_pt;
	other._next = tmp_next;
	other.p_sock = tmp_sock;
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


int ChainBuf::get(
	void	*dta,
	int		sz
	)
{
	int		last_incr;
	int		nr;

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

