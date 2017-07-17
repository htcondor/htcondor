/***************************************************************
 *
 * Copyright (C) 1990-2017, Condor Team, Computer Sciences Department,
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


#ifndef __MY_ASYNC_FREAD__
#define __MY_ASYNC_FREAD__

#include "MyString.h"
#ifdef WIN32
// if this is commented out, Windows will use fake aio_* routines that simulate async io.
#define USE_WIN32_ASYNC_IO 1
#else
#include <aio.h>
#endif

#ifdef USE_WIN32_ASYNC_IO
  #define FILE_DESCR_NOT_SET INVALID_HANDLE_VALUE
#else
  #define FILE_DESCR_NOT_SET -1
  #ifdef WIN32
	typedef struct aiocb {
		int    aio_fildes;
		off_t  aio_offset;
		volatile void * aio_buf;
		size_t aio_nbytes;
		//int    aio_reqprio;
		//int    aio_sigevent;
		//int    aio_lio_opcode;
		// these are used by the fake aio implementation on Win32.
		int cbread;
		int rval;
		int check;
	} aiocb;
	int aio_read(struct aiocb * ab);
	int aio_error(struct aiocb * ab);
	int aio_cancel(int fd, struct aiocb * ab);
	ssize_t aio_return(struct aiocb * ab);
  #endif
#endif

class MyAsyncFileReader;

class MyStringAioSource : public MyStringSource {
public:
	MyStringAioSource(MyAsyncFileReader& _aio) : aio(_aio) {}
	virtual ~MyStringAioSource() {};
	virtual bool readLine(MyString & str, bool append = false);
	virtual bool isEof();
	bool allDataIsAvailable(); // becomes true once EOF has been buffered.

protected:
	MyAsyncFileReader & aio;
	// disallow assignment and copy construction
	MyStringAioSource(const MyStringAioSource & that);
	MyStringAioSource& operator=(const MyStringAioSource & that);
};


class MyAsyncBuffer {
protected:
	void * ptr;
	size_t cballoc;
	size_t offset; // start of valid data
	size_t cbdata; // size of valid data
	size_t cbpending; // size of pending read

public:
	MyAsyncBuffer(int cb=0) : ptr(0), cballoc(cb), offset(0), cbdata(0), cbpending(0) {
		if (cb) { reset(cb); }
	}

	~MyAsyncBuffer() { free(); }

	void reset(size_t cb) {
		if (ptr && cb == cballoc)
			return;
		free();

		cballoc = cb;
		if (cballoc <= 0) return;

	#ifdef USE_WIN32_ASYNC_IO
		// use virtual alloc rather than malloc on windows to gurarantee the aligment that we need for unbuffered io.
		ptr = VirtualAlloc(NULL, cballoc, MEM_COMMIT, PAGE_READWRITE);
	#else
		ptr = malloc(cballoc);
	#endif
	}

	void free() {
		if (ptr) { 
		#ifdef USE_WIN32_ASYNC_IO
			// use virtual alloc rather than malloc on windows to gurarantee the aligment that we need for unbuffered io.
			VirtualFree(ptr, 0, MEM_RELEASE);
		#else
			::free(ptr);
		#endif
		}
		ptr = NULL;
		cbdata = offset = 0;
	}

	// clear resets the valid data, but does not free the buffer
	void clear() { set_valid_data(0,0); }

	// append writes valid data into the buffer (if there is room)
	// it returns true if all of the data was written, false and makes
	// no change to the buffer of all of the data does not fit.
	bool append(char * data, int cb) {
		if ( ! ptr || cballoc <= 0)
			return false;
		if (offset+cbdata+cb < cballoc) {
			memcpy((char*)ptr + offset+cbdata, data, cb);
			cbdata += cb;
			return true;
		}
		return false;
	}

	char * getbuf(size_t & cb) {
		if (ptr) { cb = cballoc; } else { cb = 0; }
		return (char*)ptr;
	}

	size_t capacity() { return cballoc; }
	// buffers are idle when the have no data, and no pending reads
	bool idle() { return (cbdata == 0) && (cbpending == 0); }
	bool pending() { return cbpending != 0; }
	bool has_valid_data() { return (cbdata > 0) && (cbpending == 0); }

	// set pending data size, we do this when the buffer is queued for async io to indicate
	// that we dont' yet have data, but that the buffer is not free for re-use yet either
	// we also call this to mark the buffer as idle. 
	bool set_pending_data(size_t size) {
		if (cbpending) return false;
		cbpending = size;
		return true;
	}

	// when async io completes or is cancelled, we set the size of the valid data
	// this also cancels the pending state for the buffer.
	bool set_valid_data(size_t off, size_t size) {
		if (((ssize_t)off >= 0) && (off < cballoc)) {
			offset = off;
			cbdata = MIN(size, cballoc - offset);
			cbpending = 0;
			return true;
		}
		return false;
	}

	// get the size and extent of valid data from the buffer
	const char * data(int & cb) {
		if ((ssize_t)cbdata >= 0) {
			cb = cbdata;
			return ((const char*)ptr) + offset;
		}
		return NULL;
	}
	size_t datasize() { return ((ssize_t)cbdata >= 0) ? cbdata : 0; }

	int use_data(int cb) {
		ASSERT(cb >= 0);
		if ((ssize_t)cbdata >= 0) {
			cb = MIN(cb, (ssize_t)cbdata);
			cbdata -= cb;
			offset += cb;
			return cb;
		}
		return 0;
	}

	// we disallow assignment and copy construction, but allow explicit swapping
	// but only of buffers that don't have reads pending.
	void swap(MyAsyncBuffer & that) {
		ASSERT((this->cbpending == 0) && (that.cbpending == 0));

		void * p1 = that.ptr;
		that.ptr = this->ptr;
		this->ptr = p1;

		size_t cb1 = that.cballoc;
		that.cballoc = this->cballoc;
		this->cballoc = cb1;

		size_t o1 = that.offset;
		that.offset = this->offset;
		this->offset = o1;

		size_t d1 = that.cbdata;
		that.cbdata = this->cbdata;
		this->cbdata = d1;
	}

private:
	// disallow assignment and copy construction
	MyAsyncBuffer(const MyAsyncBuffer & that);
	MyAsyncBuffer& operator=(const MyAsyncBuffer & that);
};

// Class to hold an non-blocking FILE handle and read from it a line at a time.
//
// Usage:
//
class MyAsyncFileReader {
protected:
#ifdef USE_WIN32_ASYNC_IO
	HANDLE fd;
	struct {
		void* aio_buf;
		size_t aio_nbytes;
		off_t  aio_offset;
		OVERLAPPED ovl;
	} ab;
#else
	int fd;
	aiocb ab;
#endif
	long long cbfile;
	long long ixposX; // next position to queue for read, updated when the read is queued, not when it completes.
	int    error;   // error code from reading
	int    status; // last status return from aio_error
	bool   got_eof; // set to true when eof was read
	int    total_reads;   // number of aio_read/ReadFile calls
	int    total_inprogress; // number of times aio_error/GetOverlappedResult returned EINPROGRESS
	MyStringAioSource src;
	friend class MyStringAioSource;

	MyAsyncBuffer buf;  // the first async read buffer, what we are currently returning lines from.
	MyAsyncBuffer nextbuf; // the next async buffer read buffer, usually the one that is queued.

	static const int DEFAULT_BUFFER_SIZE = 0x2000; // default to 64k buffers
	static const int DEFAULT_BUFFER_ALIGMENT = 0x1000; // default to 1k buffer alignment

public:
	static const int NOT_INTIALIZED=0xd01e; // error or status value are uninitialized
	static const int MAX_LINE_LENGTH_EXCEEDED=0xd00d; // error value for line length exceeded buffer capacity
	static const int READ_QUEUED=0x1eee;   // status value when a read has been queued, but completion has never been checked.
	MyAsyncFileReader() 
		: fd(FILE_DESCR_NOT_SET)
		, cbfile(-1)
		, ixposX(0)
		, error(NOT_INTIALIZED)
		, status(NOT_INTIALIZED)
		, got_eof(false)
		, total_reads(0)
		, total_inprogress(0)
		, src(*this) 
	{}
	virtual ~MyAsyncFileReader();

	// prepare class for destruction or re-use.
	// rewinds output buffer but does not free it.
	void clear();

	// open a file for non-blocking io and being reading from it into a buffer.
	// returns 0 if file is opened
	// returns -1 this class already has an open file
	// returns errno file cannot be opened
	int open (const char * filename);

	// close the file
	bool close();

	// returns true if the program and FILE* handle have been closed, false if not.
	bool is_closed() const { return (fd == FILE_DESCR_NOT_SET); }

	// returns true if we have read up to the end of file.
	// NOTE: will return true when all of the data has been buffered, even if
	// get_data has not yet returned all of it, and consume_data has not yet used all of it.
	// so you should only call this when get_data returns false.
	bool eof_was_read() const { return !error && got_eof; }

	// returns true when the reader has either failed, or has buffered all of the data
	// note that this ALSO returns true before you open() the file !
	bool done_reading() const { return error || got_eof || is_closed(); }

	// return the stats for the reader.
	void get_stats(int & reads, int & in_progress) { reads = total_reads; in_progress = total_inprogress; }

	// once is_closed() returns true, these can be used to query the final state.
	int error_code() const { return error; }
	int current_status() const { return status; }
	MyStringAioSource& output() { return src; } // if you call this before is_closed() is true, you must deal with pending data

	// returns "" if no error, strerror() or some other short string
	// if there was an error opening the file or reading output
	const char * error_str() const;

	// after we open the file, this queues the first io buffer
	int queue_next_read();

	// check to see if a pending io has completed, and queue the next one
	// if there is more data to read and a free buffer to read into
	int check_for_read_completion();

	// returns raw pointers to available data without consuming it.
	bool get_data(const char * & p1, int & cb1, const char * & p2, int & cb2);

	// consumes data and possibly queues new reads
	int consume_data(int cb);

	// writes or appends the next line into the input str, and returns true if there was a line
	// returns false if there is not yet enough data to find the next newline in the file, or
	// if at the end of file or there was a read error and reading was aborted. 
	// call done_reading() to determine if it is worthwhile to retry later.
	bool readline(MyString & str, bool append=false) { return src.readLine(str, append); }

private:
	void set_error_and_close(int err);

	// disallow assignment and copy construction
	MyAsyncFileReader(const MyAsyncFileReader & that);
	MyAsyncFileReader& operator=(const MyAsyncFileReader & that);
};


#endif // __MY_ASYNC_FREAD__
