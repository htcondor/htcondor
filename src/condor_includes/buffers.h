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
#ifndef BUFFERS_H
#define BUFFERS_H

#define CONDOR_IO_BUF_SIZE 4096

void sanity_check();

#if !defined(WIN32)
#  ifndef SOCKET
#    define SOCKET int
#  endif
#endif /* not WIN32 */

class Buf {
public:
	Buf(int sz=CONDOR_IO_BUF_SIZE);
	~Buf();

	inline void reset() { _dta_pt = _dta_sz = 0; }
	inline void rewind() { _dta_pt = 0; }


	inline int max_size() const { return _dta_maxsz; }
	inline int num_untouched() const { return _dta_sz - _dta_pt; }
	inline int num_touched() const { return _dta_pt; }
	inline int num_used() const { return _dta_sz; }
	inline int num_free() const { return _dta_maxsz - _dta_sz; }

	inline int empty() const { return _dta_sz == 0; }
	inline int full() const { return _dta_sz == _dta_maxsz; }
	inline int consumed() const { return _dta_pt == _dta_sz; }


	int write(SOCKET dataSock,
	          SOCKET mngSock,
			  int sz,
			  int timeout,
              int threshold,
			  bool &congested,
			  bool &ready);
	int read(SOCKET sockd, int sz=-1, int timeout=0);

	int flush(SOCKET sockd, SOCKET mngSock,
	          void *hdr, int sz, int timeout,
			  int threshold, bool &congested, bool &ready);

	int put_max(const void *, int);
	int get_max(void *, int);

	int find(char);
	int peek(char &);
	int seek(int);

	inline Buf *next() const { return _next; }
	inline void set_next(Buf *b) { _next = b; }
	inline void *get_ptr() const { return &_dta[num_touched()]; }

private:

	char	*_dta;
	int		_dta_sz;
	int		_dta_maxsz;
	int		_dta_pt;
	Buf		*_next;
};




class ChainBuf {
public:
	ChainBuf() : _head(0), _tail(0), _curr(0),_tmp(0) {}
	~ChainBuf() { reset(); }


	inline int consumed() { return !_tail || (_tail && _tail->consumed()); }

	int put(Buf *);
	void reset();

	int get(void *, int);
	int get_tmp(void *&, char);

	int peek(char &);

private:
	Buf			*_head;
	Buf			*_tail;
	Buf			*_curr;
	char		*_tmp;
};




#endif
