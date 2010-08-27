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

#ifndef STD_UNIV_BUFFERS_H
#define STD_UNIV_BUFFERS_H

#define STD_UNIV_IO_BUF_SIZE 4096

void sanity_check();

#if !defined(WIN32)
#  ifndef SOCKET
#    define SOCKET int
#  endif
#endif /* not WIN32 */

class StdUnivSock;

class StdUnivBuf {
	
public:
	StdUnivBuf(int sz=STD_UNIV_IO_BUF_SIZE);
	StdUnivBuf(StdUnivSock* tmp, int sz=STD_UNIV_IO_BUF_SIZE);
 
	~StdUnivBuf();

	inline void reset() { _dta_pt = _dta_sz = 0; }
	inline void rewind() { _dta_pt = 0; }

	void alloc_buf();
	void dealloc_buf();

	inline int max_size() const { return _dta_maxsz; }
	inline int num_untouched() const { return _dta_sz - _dta_pt; }
	inline int num_touched() const { return _dta_pt; }
	inline int num_used() const { return _dta_sz; }
	inline int num_free() const { return _dta_maxsz - _dta_sz; }

	inline int empty() const { return _dta_sz == 0; }
	inline int full() const { return _dta_sz == _dta_maxsz; }
	inline int consumed() const { return _dta_pt == _dta_sz; }


	int write(char const *peer_description,SOCKET sockd, int sz=-1, int timeout=0);
	int read(char const *peer_description,SOCKET sockd, int sz=-1, int timeout=0);

	int flush(char const *peer_description,SOCKET sockd, void * hdr=0, int sz=0, int timeout=0);

	int put_max(const void *, int);
	int get_max(void *, int);

	int find(char);
	int peek(char &);
	int seek(int);

	inline StdUnivBuf *next() const { return _next; }
	inline void set_next(StdUnivBuf *b) { _next = b; }
	inline void *get_ptr() const { return &_dta[num_touched()]; }
	
	///initialize socket handle.
	void init_parent(StdUnivSock* tmp) { p_sock = tmp;}

private:

	char	*_dta;
	int		_dta_sz;
	int		_dta_maxsz;
	int		_dta_pt;
	StdUnivBuf		*_next;
	StdUnivSock	*p_sock; //serves as a handle to invoke condor_read/write for 
					 //any read/ write to sockets.
};




class StdUnivChainBuf {
public:
	StdUnivChainBuf() : _head(0), _tail(0), _curr(0),_tmp(0) {}
	~StdUnivChainBuf() { reset(); }


	inline int consumed() { return !_tail || (_tail && _tail->consumed()); }

	int put(StdUnivBuf *);
	void reset();

	int get(void *, int);
	int get_tmp(void *&, char);

	int peek(char &);

private:
	StdUnivBuf			*_head;
	StdUnivBuf			*_tail;
	StdUnivBuf			*_curr;
	char		*_tmp;
};




#endif
