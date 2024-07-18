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


#include "condor_common.h"
#include "condor_debug.h"

#include "my_async_fread.h"

#define ALIGN_SIZE(len,align) ((len) + (align-1)) & ~(align-1);

// Hack for Wubuntu (and others?)
// when aio_error returns ENOSYS, just do the read synchronously.
// #define USE_SYNC_IO_WHEN_AIO_UNSUPPORTED 1

#if defined WIN32 && ! defined USE_WIN32_ASYNC_IO

// simulate async read
int aio_read(struct aiocb * ab)
{
	ab->rval = 0; ab->cbread = 0; ab->check = 0;
	return 0;
}

int aio_cancel(int fd, struct aiocb * ab)
{
	return 0;
}

// simulate async status check
int aio_error(struct aiocb * ab)
{
	if ( ! ab->check) {
		ab->check += 1;
		return EINPROGRESS;
	}
	if ( ! ab->cbread) {
		ab->cbread = read(ab->aio_fildes, (void*)ab->aio_buf, ab->aio_nbytes);
		ab->rval = (ab->cbread < 0) ? errno : 0;
	}
	return ab->rval;
}

// simulate async 
ssize_t aio_return(struct aiocb * ab)
{
	if (ab->rval) return ab->rval;
	return ab->cbread;
}

#endif

MyAsyncFileReader::~MyAsyncFileReader()
{
	clear();
}

void MyAsyncFileReader::clear()
{
	close();
	error = NOT_INTIALIZED;

	buf.free();
	nextbuf.free();
}

int MyAsyncFileReader::open (const char * filename, bool buffer_whole_file /*=false*/)
{
	if (error != NOT_INTIALIZED) return error;
	ASSERT(fd == FILE_DESCR_NOT_SET);
	error = 0;

#ifdef USE_WIN32_ASYNC_IO
	DWORD ovFlag = not_async ? 0 : FILE_FLAG_OVERLAPPED;
	fd = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, ovFlag | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (fd == INVALID_HANDLE_VALUE) {
		error = GetLastError();
		fd = FILE_DESCR_NOT_SET;
	} else {
		LARGE_INTEGER li;
		if (GetFileSizeEx(fd, &li)) {
			cbfile = li.QuadPart;
			ixpos = 0;
			got_eof = false;
		} else {
			error = ENOENT;
			close();
		}
	}
#else
	memset(&ab, 0, sizeof(ab));

#ifndef O_SEQUENTIAL
  #define O_SEQUENTIAL 0
#endif

	fd = ::safe_open_no_create(filename, O_RDONLY | O_SEQUENTIAL);
	if (fd == FILE_DESCR_NOT_SET) {
		error = errno;
	} else {

		struct stat st;
		if (fstat(fd, &st) < 0) {
			error = errno;
			close();
		} else {
			cbfile = st.st_size;
			ixpos = 0;
			got_eof = false;
		}

		ab.aio_fildes = fd;
	}
#endif

	// if we opened the file and got its size, allocate buffers
	if (fd != FILE_DESCR_NOT_SET) {
		const int buffer_size = DEFAULT_BUFFER_SIZE; 
		if (buffer_whole_file || (cbfile <= buffer_size*2)) {
			if (cbfile == 0) {
				// file has 0 size, just pretend we read the data already.
				nextbuf.reset(DEFAULT_BUFFER_ALIGMENT);
			} else {
				// don't bother to make ping/pong buffers if we will be buffering the whole file.
				// we still need the buffer to be big enough and aligned to the requirements of unbuffered io.
				int cbbuf = ALIGN_SIZE(cbfile, DEFAULT_BUFFER_ALIGMENT);
				nextbuf.reset(cbbuf);
				whole_file = true;
			}
		} else {
			nextbuf.reset(buffer_size);
			buf.reset(buffer_size);
		}
		size_t dummy;
		ASSERT(nextbuf.getbuf(dummy) != NULL);
	}

	return (fd != FILE_DESCR_NOT_SET) ? 0 : -1;
}

bool MyAsyncFileReader::close() {
	if (fd == FILE_DESCR_NOT_SET)
		return false;

#ifdef USE_WIN32_ASYNC_IO
	CloseHandle(fd);
#else
	::close(fd);
#endif
	fd = FILE_DESCR_NOT_SET;
	return true;
}


// queue the next read, if there is no more data to be read, this closes the file.
//
int MyAsyncFileReader::queue_next_read()
{
	if (error) return error;

	// if the nextbuf has a pending read or still has unconsumed data, do nothing.
	if ( ! nextbuf.idle()) return 0;

	if (this->got_eof) {
		// we expect to have closed the file before this if we got EOF
		// but if we get here and the file is not yet closed, close it now.
		close();
		return 0;
	}

	ab.aio_buf = nextbuf.getbuf(ab.aio_nbytes);
	if ( ! ab.aio_buf) {
		// if the nextbuf has no buffer pointer, this is a special case where the entire file fits in a buffer.
		// we will just skip queueing the empty nextbuf in that case.
		got_eof = true;
		close();
		return 0;
	}
	ab.aio_offset = this->ixpos;

	ASSERT(fd != FILE_DESCR_NOT_SET);
	++total_reads;

	this->ixpos += ab.aio_nbytes;
	nextbuf.set_pending_data(ab.aio_nbytes);
#ifdef USE_WIN32_ASYNC_IO
	DWORD cbRead = 0;
	LARGE_INTEGER li; li.QuadPart = ab.aio_offset;
	ab.ovl.Offset = li.LowPart;
	ab.ovl.OffsetHigh = li.HighPart;
	ab.ovl.hEvent = NULL;
	bool read_ok;
	if (not_async) {
		read_ok = ReadFile(fd, ab.aio_buf, (DWORD)ab.aio_nbytes, &cbRead, NULL);
	} else {
		read_ok = ReadFile(fd, ab.aio_buf, (DWORD)ab.aio_nbytes, &cbRead, &ab.ovl);
	}
	if (read_ok) {
		// read completed synchronously, so 'unqueue' the read
		// and also handle end-of-file detection.
		nextbuf.set_valid_data(0, cbRead);
		if (0 == cbRead) this->got_eof = true;
		this->status = 0;
		// not queued anymore
		ab.aio_buf = NULL;
		ab.aio_nbytes = 0;

		// if the primary read buffer is empty, promote this new data to the primary buffer
		if (buf.idle()) {
			buf.swap(nextbuf);
		}
	} else {
		DWORD dw = GetLastError();
		if (dw == ERROR_IO_PENDING) {
			// read was queued.
			this->status = READ_QUEUED;
		} else if (dw == ERROR_HANDLE_EOF) {
			// read hit the end of file
			nextbuf.set_valid_data(0, 0);
			this->got_eof = true;
			this->status = 0;
		} else {
			// some other error
			error = dw ? (int)dw : -1;
			this->status = error;
		}

		// close the file on failure or end-of-file
		if (this->got_eof || error) {
			close();
		}
	}
#else
	if (aio_read(&ab) < 0) {
		error = errno ? errno : -1;
		// not queued anymore
		ab.aio_buf = NULL;
		ab.aio_nbytes = 0;
		this->status = error;
		// close the file on failure.
		close();
	} else {
		this->status = READ_QUEUED;
	}
#endif
	return error;
}

// check for completion if a queue async read, and if it has completed
// and we are 
//
int MyAsyncFileReader::check_for_read_completion()
{
	if (error) return error;

	if (ab.aio_buf) {
		ASSERT(fd != FILE_DESCR_NOT_SET);

		bool complete = false;
		ssize_t cbread = -1;

		// check for completed read
	#ifdef USE_WIN32_ASYNC_IO
		DWORD dwRead;
		if (GetOverlappedResult(fd, &ab.ovl, &dwRead, FALSE)) {
			// operation complete
			cbread = dwRead;
			complete = true;
			this->status = 0;
		} else {
			DWORD dw = GetLastError();
			if (dw == ERROR_IO_INCOMPLETE) {
				++total_inprogress; // not done yet.
				this->status = EINPROGRESS;
			} else if (dw == ERROR_HANDLE_EOF) {
				// io complete, hit end of file.
				cbread = 0;
				got_eof = true;
				complete = true;
				this->status = 0;
			} else {
				// other errors
				error = dw;
				this->status = dw;
			}
		}
	#else
		ASSERT(fd == ab.aio_fildes);
		int rval = aio_error(&ab);
		this->status = rval;

		#ifdef USE_SYNC_IO_WHEN_AIO_UNSUPPORTED
		if (rval == ENOSYS) {
			// if the aio_read function was not implemented, just do synchronous reads now.
			lseek(ab.aio_fildes, ab.aio_offset, SEEK_SET);
			cbread = read(ab.aio_fildes, ab.aio_buf, ab.aio_nbytes);
			if ((ssize_t)cbread < 0) {
				rval = errno;
			} else {
				rval = 0;
				complete = true;  // prevent the call to aio_return below, since we "know" that it will fail
				got_eof = (0 == cbread);
			}
		}
		#endif

		//fprintf(stderr, "aio_error for fd=%d returned %d %s\n", fd, rval, strerror(rval));
		if (rval == EINPROGRESS) {
			++total_inprogress; // not done yet.
		} else {
			if (rval) {
				// io failed or was cancelled.
				error = rval;
				//fprintf(stderr, "check_for_read_completion for fd=%d setting error to %d %s\n", fd, error, strerror(error));
			} else if ( ! complete) {
				// io completed, mark the data as valid and clear the pending state in the nextbuf
				cbread = aio_return(&ab);
				complete = true;
				got_eof = (0 == cbread);
			}
		}
	#endif

		if (error) {
			// there was an error, and the ab buffer is no longer queued, so clear out it's pointer and size.
			ab.aio_buf = 0;
			ab.aio_nbytes = 0;
		} else if (complete) {
			size_t cballoc;
			ASSERT (nextbuf.getbuf(cballoc) == ab.aio_buf && (ssize_t)cballoc >= cbread)
			nextbuf.set_valid_data(0, cbread);

			// the ab buffer is no longer queued, so clear out its pointer and size.
			ab.aio_buf = 0;
			ab.aio_nbytes = 0;

			// if the primary read buffer is empty, promote this new data to the primary buffer
			if (buf.idle()) {
				buf.swap(nextbuf);
			}
		}

		// if we no longer have a queued request, close the file on io error or when we get end-of-file.
		if ( ! ab.aio_buf && (got_eof || error)) {
			close();
		}
	}

	// if we have not failed, have not yet closed the file, have no pending reads
	// try and queue a new read.
	if ( ! error && ! ab.aio_buf && fd != FILE_DESCR_NOT_SET) {
		queue_next_read();
	}

	// return the current error/success state.
	return error;
}

void MyAsyncFileReader::set_error_and_close(int err)
{
	ASSERT(err);
	error = err;
	if (fd != FILE_DESCR_NOT_SET) {
#ifdef USE_WIN32_ASYNC_IO
		CancelIo(fd);
#else
		if (ab.aio_fildes) aio_cancel(fd, NULL);
#endif
		memset(&ab, 0, sizeof(ab));
		close();
	}
}


// returns pointers to the components of the next line and true if the next line is available
// otherwise it returns false.  when this returns false 
bool MyAsyncFileReader::get_data(const char * & p1, int & cb1, const char * & p2, int & cb2)
{
	if (error) return false;

	// before we return available data, check for any completed reads
	check_for_read_completion();
	if (error) {
		set_error_and_close(error);
		return false;
	}

	p1 = p2 = NULL;
	cb1 = cb2 = 0;

	// return data pointers for the buffers that have valid data.
	//
	if (buf.has_valid_data()) {
		p1 = buf.data(cb1);
		if (nextbuf.has_valid_data()) {
			p2 = nextbuf.data(cb2);
		}
		return true;
	}
	return false;
}

// consumes data and possibly queues new reads
int MyAsyncFileReader::consume_data(int cb)
{
	ASSERT( ! buf.pending());
	int cbused = buf.use_data(cb);

	// if we consumed all of the data in the primary buffer, mark it is truly empty
	// and try and swap it with read-ahead buffer.
	if (buf.idle()) {
		buf.set_valid_data(0, 0); // note that 'valid' data buffers never have 0 bytes of data, this really marks the buffer as empty.

		// We can only swap it if the read-ahead buffer has no pending reads
		// or in the special case where there is only a single buffer.
		if (nextbuf.has_valid_data() || ! nextbuf.capacity()) {
			buf.swap(nextbuf);
			int remain = cb - cbused;
			int cbused2 = buf.use_data(remain);
			cbused += cbused2;
		}
	}

	// if the read-ahead buffer is now idle, try and queue the next read.
	if (nextbuf.idle() && !error && fd != FILE_DESCR_NOT_SET) {
		queue_next_read();
	}

	return cbused;
}

bool MyStringAioSource::allDataIsAvailable()
{
	return aio.eof_was_read();
}

bool MyStringAioSource::readLine(std::string & str, bool append /* = false*/)
{
	const char * p1;
	const char * p2;
	int c1, c2;
	if (aio.get_data(p1, c1, p2, c2) && p1) {
		if ( ! p2) c2 = 0;

		int len = 0;
		for (int ix = 0; ix < c1; ++ix) { if (p1[ix] == '\n') { len = ix+1; break; } }
		if ( ! len && p2) {
			for (int ix = 0; ix < c2; ++ix) { if (p2[ix] == '\n') { len = c1+ix+1; break; } }
		}

		// no newline found, are we at the end of the file?
		if ( ! len) {
			if (aio.eof_was_read()) {
				// if we read the end of file, then the file must not have ended with a \n
				// so just treat all of the remaining data as a line.
				len = c1 + c2;
			} else {
				if (p1 && p2) {
					aio.set_error_and_close(MyAsyncFileReader::MAX_LINE_LENGTH_EXCEEDED);
				}
				return false;
			}
		}

		if (append) {
			str.append(p1, MIN(c1, len));
		} else {
			str.assign(p1, MIN(c1, len));
		}
		if (p2 && (len > c1)) {
			str.append(p2, len-c1);
		}
		aio.consume_data(len);
		return true;
	}
	return false;
}

bool MyStringAioSource::isEof()
{
	const char * p1;
	const char * p2;
	int c1, c2;
	if (aio.get_data(p1, c1, p2, c2)) {
		return false;
	}
	return aio.eof_was_read();
}

