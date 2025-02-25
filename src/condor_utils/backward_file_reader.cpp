/***************************************************************
 *
 * Copyright (C) 1990-2012, Condor Team, Computer Sciences Department,
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
#include "backward_file_reader.h"


BackwardFileReader::BWReaderBuffer::BWReaderBuffer(size_t cb/*=0*/, char * input /* = NULL*/)
	: data(input)
	, cbData(cb)
	, cbAlloc(cb)
	, at_eof(false)
	, text_mode(false)
	, error(0)
{
	if (input) {
		cbAlloc = cbData = cb;
	} else if (cb > 0) {
		data = (char*)malloc(cb);
		if (data) memset(data, 17, cb);
		cbData = 0;
	}
}

void BackwardFileReader::BWReaderBuffer::setsize(size_t cb)
{
	cbData = cb; 
	ASSERT(cbData <= cbAlloc);
}

bool BackwardFileReader::BWReaderBuffer::reserve(size_t cb)
{
	if (data && cbAlloc >= cb)
		return true;

	void * pv = realloc(data, cb);
	if (pv) {
		data = (char*)pv;
		cbAlloc = cb;
		return true;
	}
	return false;
}

size_t BackwardFileReader::BWReaderBuffer::fread_at(FILE * file, off_t offset, size_t cb)
{
	if ( ! reserve(((cb + 16) & ~15) + 16))
		return 0;

	if (fseek(file, offset, SEEK_SET) < 0) {
		error = ferror(file);
		return 0;
	} else {
		error = 0;
	}

	size_t ret = fread(data, 1, cb, file);
	cbData = ret;

	if (ret <= 0) {
		error = ferror(file);
		return 0;
	} else {
		error = 0;
	}

	// on windows in text mode we can consume more than we read because of \r
	// but since we are scanning backward this can cause us to re-read
	// the same bytes more than once. So lop off the end of the buffer so
	// so we only get back the unique bytes
	at_eof = feof(file);
	if (text_mode && ! at_eof) {
		off_t end_offset = ftell(file);
		size_t extra = (size_t)(end_offset - (offset + ret));
		ret -= extra;
	}

	if (ret < cbAlloc) {
		data[ret] =  0; // force null terminate.
	} else {
		// this should NOT happen
		EXCEPT("BWReadBuffer is unexpectedly too small!");
	}

	return ret;
}


BackwardFileReader::BackwardFileReader(std::string filename, int open_flags)
	: error(0), file(NULL), cbFile(0), cbPos(0) 
{
#ifdef WIN32
	open_flags |= O_BINARY;
#endif
	int fd = safe_open_wrapper_follow(filename.c_str(), open_flags);
	if (fd < 0)
		error = errno;
	else if ( ! OpenFile(fd, "rb"))
		close(fd);
}

BackwardFileReader::BackwardFileReader(int fd, const char * open_options)
	: error(0), file(NULL), cbFile(0), cbPos(0) 
{
	OpenFile(fd, open_options);
}
/*
BackwardFileReader::~BackwardFileReader()
{
	if (file) fclose(file);
	file = NULL;
}
*/

bool BackwardFileReader::PrevLine(std::string & str)
{
	str.clear();

	// can we get a previous line out of our existing buffer?
	// then do that.
	if (PrevLineFromBuf(str))
		return true;

	// no line in the buffer? then return false
	if (AtBOF())
		return false;

	const off_t cbBack = 512;
	while (true) {
		off_t off = cbPos > cbBack ? cbPos - cbBack : 0;
		size_t cbToRead = (size_t)(cbPos - off);

		// we want to read in cbBack chunks at cbBack aligment, of course
		// this only makes sense to do if cbBack is a power of 2. 
		// also, in order to get EOF to register, we have to read a little 
		// so we may want the first read (from the end, of course) to be a bit 
		// larger than cbBack so that we read at least cbBack but also end up
		// on cbBack alignment. 
		if (cbFile == cbPos && cbToRead > cbBack) {
			// test to see if cbBack is a power of 2, if it is, then set our
			// seek to align on cbBack.
			if (!(cbBack & (cbBack-1))) {
				// seek to an even multiple of cbBack at least cbBack from the end of the file.
				off = (cbFile - cbBack) & ~(cbBack-1);
				cbToRead = cbFile - off;
			}
			cbToRead += 16;
		}

		if ( ! buf.fread_at(file, off, cbToRead)) {
			if (buf.LastError()) {
				error = buf.LastError();
				return false;
			}
		}

		cbPos = off;

		// try again to get some data from the buffer
		if (PrevLineFromBuf(str) || AtBOF())
			return true;
	}
}

bool BackwardFileReader::OpenFile(int fd, const char * open_options)
{
	file = fdopen(fd, open_options);
	if ( ! file) {
		error = errno;
	} else {
		// seek to the end of the file.
		fseek(file, 0, SEEK_END);
		cbFile = cbPos = ftell(file);
		error = 0;
		buf.SetTextMode( ! strchr(open_options,'b'));
	}
	return error == 0;
}

// prefixes or part of a line into str, and updates internal
// variables to keep track of what parts of the buffer have been returned.
bool BackwardFileReader::PrevLineFromBuf(std::string & str)
{
	// if we have no buffered data, then there is nothing to do
	size_t cb = buf.size();
	if (cb <= 0)
		return false;

	// if buffer ends in a newline, convert it to a \0
	if (buf[cb-1] == '\n') {
		buf[--cb] = 0;
		// if the input string is not empty, then the previous
		// buffer ended _exactly_ at a newline boundary, so return
		// the string rather than concatinating it to the newline.
		if ( ! str.empty()) {
			if (buf[cb-1] == '\r')
				buf[--cb] = 0;
			buf.setsize(cb);
			return true;
		}
	}
	// because of windows style \r\n, we also tolerate a \r at the end of the line
	if (buf[cb-1] == '\r') {
		buf[--cb] = 0;
	}

	// now we walk backward through the buffer until we encounter another newline
	// returning all of the characters that we found.
	while (cb > 0) {
		if (buf[--cb] == '\n') {
			str.insert(0, &buf[cb+1]);
			buf[cb] = 0;
			buf.setsize(cb);
			return true;
		}
	}

	// we hit the start of the buffer without finding another newline,
	// so return that text, but only return true if we are also at the start
	// of the file.
	str.insert(0, &buf[0]);
	buf[0] = 0;
	buf.clear();

	return (0 == cbPos);
}
