#ifndef BUFFERS_H
#define BUFFERS_H

#define CONDOR_IO_BUF_SIZE 4096

void sanity_check();

#if !defined(WIN32)
typedef int SOCKET;
#endif

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


	int write(SOCKET sockd, int sz=-1, int timeout=0);
	int read(SOCKET sockd, int sz=-1, int timeout=0);

	int flush(SOCKET sockd, void * hdr=0, int sz=0, int timeout=0);

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
	~ChainBuf() { if (_tmp) delete [] _tmp; }


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
