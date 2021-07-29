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

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
 * MyAsyncFileReader and associated helper classes.
 *
 * A class that allows you to read a file a line at a time without blocking. It uses aio_*
 * functions on Linux/Unix and native Windows API on Windows to do nonblocking file io.
 * These APIs expect to be working with FILES and may or may not work with pipes etc.
 *
 * The basic use pattern is
 *
 *   MyAsyncFileReader reader;
 *   if (reader.open() == 0) {
 *      // optionally call reader.queue_next_read() to queue the first I/O
 *      while(true) {
 *         if (reader.readline(...)) {
 *            // process the line
 *         } else if (reader.done_reading()) {
 *            // when readline returns false and done_reading() returns true we are
 *            // either at end-of-file or there was an error. 
 *            // check reader.error_code() for non-zero to see if there was an error.
 *            break;
 *         } else {
 *            // in practice you wouldn't use this class like this. instead you would
 *            // stash the reader class and come back later to try readline() again.
 *            do_other_work_for_a_while();
 *         }
 *      }
 *   }
 *
 *
 * Implementation details.

 * The class uses 1 or 2 buffers, nextbuf is queued for read, and it is swapped with buf
 * once the read is completed and buf has been consumed. The buffers are classes that
 * can keep track of whether they are empty, have valid data, or have been queued for
 * a read and have pending data.
 *
 * A single buffer is used when the file size is detected on open() to be small enough to fit
 * entirely in the default buffering. In that case only a single buffer is allocated and the
 * first completed read will result in all of the data becoming available. By default this
 * size is 128k or less (0x20000)
 *
 * Whenever get_data is called (readline calls it) it will first call check_for_read_completion()
 * before returning pointers to valid data. check_for_read_completion() checks for completed IO
 * and marks data as valid, possibly swapping the nextbuf and the buf if the nextbuf has completed
 * and buf is empty. check_for_read_completion() will also call queue_next_read() if it detects
 * that there are idle buffers after it has finished checking for completion.
 *
 * if readline finds a newline (or gets to EOF) in the valid data, it will copy the line into
 * the supplied buffer and call consume_data() to mark the data as 'free'. consume_data() will
 * in turn call queue_next_read() if it detects idle buffers.
 *
 * both queue_next_read() and check_for_read_completion() will close the file if they detect EOF
 * or (sometimes) on error conditions.
 *
 * Once an error condition has been set, error_code() will return non-zero, at this point
 * get_data() and readline() will ALWAYS return false and any buffered data is no longer accessible.
 * no new reads will be queued, and the file may or may not be closed. calling close() or destroying
 * the class will always close the file. 
 *
 *---------------------------------------------------------------------------------------------*/

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
	virtual bool readLine(std::string & str, bool append = false);
	virtual bool isEof();
	bool allDataIsAvailable(); // becomes true once EOF has been buffered.

protected:
	MyAsyncFileReader & aio;
	// disallow assignment and copy construction
	MyStringAioSource(const MyStringAioSource & that);
	MyStringAioSource& operator=(const MyStringAioSource & that);
};


// helper class for MyAsyncFileReader, used to hold a buffer for async io
// this class keeps track of allocation size, valid data, and pending data
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

	size_t capacity() const { return cballoc; }
	// buffers are idle when the have no data, and no pending reads
	bool idle() const { return (cbdata == 0) && (cbpending == 0); }
	bool pending() const { return cbpending != 0; }
	bool has_valid_data() const { return (cbdata > 0) && (cbpending == 0); }

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
			cb = (int)cbdata;
			return ((const char*)ptr) + offset;
		}
		return NULL;
	}
	size_t datasize() const { return ((ssize_t)cbdata >= 0) ? cbdata : 0; }

	int use_data(int cb) {
		ASSERT(cb >= 0);
		if ((ssize_t)cbdata >= 0) {
			cb = (int)MIN(cb, (ssize_t)cbdata);
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

// Class to hold an non-blocking file handle and read from it a line at a time.
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
	long long cbfile; // size of the file, determined after the file was open.
	long long ixpos; // next position to queue for read, updated when the read is queued, not when it completes.
	int    error;   // error code from reading, if any
	int    status; // last status return from aio_error, used for debugging.
	bool   whole_file; // true when the whole file will fit into the buffers (normally into a single buffer)
	bool   not_async; // do synchronous reads
	bool   got_eof; // set to true when eof was read
	int    total_reads;   // number of aio_read/ReadFile calls
	int    total_inprogress; // number of times aio_error/GetOverlappedResult returned EINPROGRESS

	// a MyStringSource compatable class that is intimately tied to this class.
	MyStringAioSource src;
	friend class MyStringAioSource;

	// during operation, the guts of these classes periodically swap so that
	// buf is always the one we return data from, and nextbuf is always queued for read.
	MyAsyncBuffer buf;  // the buffer we are returing lines from.
	MyAsyncBuffer nextbuf; // the buffer queued for read, or returning lines when a line straddles buf+nextbuf.

	static const int DEFAULT_BUFFER_SIZE = 0x10000; // default to 64k buffers
	static const int DEFAULT_BUFFER_ALIGMENT = 0x1000; // default to 4k buffer alignment

public:
	static const int NOT_INTIALIZED=0xd01e; // error_code() or status value are uninitialized
	static const int MAX_LINE_LENGTH_EXCEEDED=0xd00d; // error_code() value for line length exceeded buffer capacity
	static const int READ_QUEUED=0x1eee;   // current_status() value when a read has been queued, but completion has never been checked.
	MyAsyncFileReader()
		: fd(FILE_DESCR_NOT_SET)
		, cbfile(-1)
		, ixpos(0)
		, error(NOT_INTIALIZED)
		, status(NOT_INTIALIZED)
		, whole_file(false)
		, not_async(false)
		, got_eof(false)
		, total_reads(0)
		, total_inprogress(0)
		, src(*this) 
	{ memset(&ab, 0, sizeof(ab));}
	virtual ~MyAsyncFileReader();

	// prepare class for destruction or re-use, closes the file and
	// rewinds any input buffers, but does not free all of them.
	void clear();

	// set sync
	bool set_sync(bool sync) { bool ret = not_async; not_async = sync; return ret; }

	// open a file for non-blocking io
	// returns 0 if file is opened
	// returns -1 this class already has an open file
	// returns errno if file cannot be opened
	int open (const char * filename, bool buffer_whole_file=false);

	// close the file, the file may be closed automatically once EOF is read.
	// it will always be closed automatically when this class is destroyed.
	bool close();

	// returns true if the FILE* handle have been closed (or was never opened), false if not.
	bool is_closed() const { return (fd == FILE_DESCR_NOT_SET); }

	// returns true if we have read up to the end of file.
	// NOTE: will return true when all of the data has been buffered, even if readline has not yet returned it.
	// (effectively if get_data has not yet returned all of it, and consume_data has not yet used all of it.)
	bool eof_was_read() const { return !error && got_eof; }

	// returns true when the reader has either failed, or has buffered all of the data
	// note that this ALSO returns true before you open() the file !
	// Call this when readline/get_data returns false to determine if it is worthwhile to try again later.
	bool done_reading() const { return error || got_eof || is_closed(); }

	// once is_closed() returns true, this can be used to query the final state.
	int error_code() const { return error; }

	// returns "" if no error, strerror() or some other short string
	// if there was an error opening the file or reading output
	const char * error_str() const;

	// writes or appends the next line into the input str, and returns true if there was a line
	// returns false if there is not yet enough data to find the next newline in the file, or
	// if at the end of file or there was a read error and reading was aborted. 
	// call done_reading() to determine if it is worthwhile to retry later.
	// If this function returns true, the returned data is removed from this classes internal buffers
	// by calling the consume_data() method below.
	bool readline(MyString & str, bool append=false) { return src.readLine(str, append); }

	// returns raw pointers to available data without consuming it.
	// returns true if there is any data, false if there was an error or there is no data
	bool get_data(const char * & p1, int & cb1, const char * & p2, int & cb2);

	// consumes data and possibly queues new reads, used by readline
	int consume_data(int cb);

	// after we open the file, use this to queue the first io buffer if you want to begin reading right away.
	// It is called by readline()/consume_data() as necessary to queue new reads.
	int queue_next_read();

	// check to see if a pending io has completed, and queue the next one
	// if there is more data to read and a free buffer to read into
	// called by readline/get_data() to check to see if pending buffers are ready.
	int check_for_read_completion();

	// Classes that parse via MyString::readLine can use this as a source once is_closed() or eof_was_read() returns true.
	// if you call this before is_closed() is true, you must deal the fact that it's readLine method will return false when data is pending.
	MyStringAioSource& output() { return src; }

	// return the stats for the reader
	void get_stats(int & reads, int & in_progress) const { reads = total_reads; in_progress = total_inprogress; }
	// last status return from aio_error, use for debugging.
	int current_status() const { return status; }

private:
	void set_error_and_close(int err);

	// disallow assignment and copy construction
	MyAsyncFileReader(const MyAsyncFileReader & that);
	MyAsyncFileReader& operator=(const MyAsyncFileReader & that);
};


#endif // __MY_ASYNC_FREAD__
