/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#define _POSIX_SOURCE

#include "condor_common.h"
#include "condor_constants.h"
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
	if (_dta) {
        delete [] _dta;
    }
	num_deleted++;
}



int Buf::write(
	SOCKET	sockd,
	int		sz,
	int		timeout
	)
{
	int	nw;
	if (sz < 0 || sz > num_untouched()) {
        sz = num_untouched();
    }

    nw = condor_write(sockd, &_dta[num_touched()], sz , timeout);
	if (nw < 0){
		dprintf( D_ALWAYS, "Buf::write(): condor_write() failed\n" );
		return -1;
	}

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

	if (sz < 0 || sz > num_free()){
		dprintf(D_ALWAYS, "IO: Buffer too small\n");
		return -1;
		/* sz = num_free(); */
	}

    nr = condor_read(sockd,&_dta[num_used()],sz,timeout);	
	if (nr == -1) {
		dprintf( D_ALWAYS, "Buf::read(): condor_read() failed\n" );
		return -1;
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

bool Buf::computeMD(char * checkSUM, Condor_MD_MAC * checker)
{
    // I absolutely hate this! 21
    checker->addMD((unsigned char *) &(_dta[21]), _dta_sz - 21);
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
    checker->addMD((unsigned char *) &(_dta[0]), _dta_sz);

    return checker->verifyMD((unsigned char *) checkSUM);
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

