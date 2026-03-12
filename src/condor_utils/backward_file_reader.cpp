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

static int64_t ftell_64b(FILE* f) {
#ifdef WIN32
	return _ftelli64(f);
#else
	return (int64_t)ftello(f);
#endif
}

static int fseek_64b(FILE* f, int64_t offset, int origin) {
#ifdef WIN32
	return _fseeki64(f, offset, origin);
#else
	return fseeko(f, (off_t)offset, origin);
#endif
}

BackwardFileReader::BWReaderBuffer::BWReaderBuffer(int cb/*=0*/, char * input /* = NULL*/)
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

void BackwardFileReader::BWReaderBuffer::setsize(int cb)
{
	cbData = cb; 
	ASSERT(cbData <= cbAlloc);
}

bool BackwardFileReader::BWReaderBuffer::reserve(int cb)
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

int BackwardFileReader::BWReaderBuffer::fread_at(FILE * file, int64_t offset, int cb)
{
	if ( ! reserve(((cb + 16) & ~15) + 16))
		return 0;

	int ret = fseek_64b(file, offset, SEEK_SET);
	if (ret < 0) {
		error = ferror(file);
		return 0;
	} else {
		error = 0;
	}

	ret = (int)fread(data, 1, cb, file);
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
		int64_t end_offset = ftell_64b(file);
		int extra = (int)(end_offset - (offset + ret));
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


BackwardFileReader::BackwardFileReader(const std::string& filename, int open_flags)
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

	// stash of temporary strings, for when we for when we need
	// to hold multiple buffers before we find a \n
	std::deque<std::string> stash;

	// can we get a previous line out of our existing buffer?
	// then do that.
	if (PrevLineFromBuf(str, stash))
		return true;

	// no line in the buffer? then return false
	if (AtBOF())
		return false;

	const int64_t cbBack = BUFSIZ;
	while (true) {
		int64_t off = cbPos > cbBack ? cbPos - cbBack : 0;
		int cbToRead = (int)(cbPos - off);

		// we want to read in cbBack chunks at cbBack aligment, of course
		// this only makes sense to do if cbBack is a power of 2. 
		// also, in order to get EOF to register, we have to read past the end
		// so we may want the first read (from the end, of course) to be a bit 
		// larger than cbBack so that we read at least cbBack but also end up
		// on cbBack alignment. 
		if (cbFile == cbPos) {
			// test to see if cbBack is a power of 2, if it is, then set our
			// seek to align on cbBack.
			if (!(cbBack & (cbBack-1))) {
				// seek to the last even multiple of cbBack before the end of the file.
				off = (cbFile - 1) & ~(cbBack-1);
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
		if (PrevLineFromBuf(str, stash) || AtBOF())
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
		fseek_64b(file, 0, SEEK_END);
		cbFile = cbPos = ftell_64b(file);
		error = 0;
		buf.SetTextMode( ! strchr(open_options,'b'));
	}
	return error == 0;
}

// When we have to scan muiltiple buffers to find a newline, we will have a set of buffers (the stash)
// that we need to assemble into the final result. The current out will be the last part.
// The stash will be in the middle in reverse order, and the first_bit should be at the beginning of the output.
//
void BackwardFileReader::insert_stash(std::string & out, std::deque<std::string> & stash, const char * first_bit)
{
	if (stash.empty()) {
		// no stash, just insert the first bit at the start of the output buffer
		out.insert(0, first_bit);
	} else if (*first_bit == 0 && stash.size() == 1) {
		// no first bit and a stash of one, just insert the stash at the start of the output buffer
		out.insert(0, stash.back());
		stash.pop_back();
	} else {
		// we have multiple pieces to assemble into the output, so we build that into a
		// temporary string, and the move that into the output.
		//
		// create a tmp string that is big enough to hold the stash,
		// the current output value, and the first_bit
		size_t cch = strlen(first_bit) + out.size();
		for (const auto & str : stash) { cch += str.size(); }
		std::string tmp; tmp.reserve(cch+1);

		// build up the tmp with the first bit first, the stash from back to front,
		// and the current output value last
		tmp.assign(first_bit);
		while ( ! stash.empty()) {
			tmp.append(stash.back());
			stash.pop_back();
		}
		tmp.append(out);
		// finally move the result to the output
		out = std::move(tmp);
	}
}

// prefixes or part of a line into str, and updates internal
// variables to keep track of what parts of the buffer have been returned.
bool BackwardFileReader::PrevLineFromBuf(std::string & str, std::deque<std::string> & stash)
{
	// if we have no buffered data, then there is nothing to do
	int cb = buf.size();
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
#if 1
			insert_stash(str, stash, "");
#endif
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
#if 1
			insert_stash(str, stash, &buf[cb+1]);
#else
			str.insert(0, &buf[cb+1]);
#endif
			buf[cb] = 0;
			buf.setsize(cb);
			return true;
		}
	}

	// we hit the start of the buffer without finding another newline,
	// so add that text to the output, but only return true if we are also at the start
	// of the file.
#if 1
	if (0 == cbPos) {
		insert_stash(str, stash, &buf[0]);
	} else if ( ! str.empty()) {
		stash.emplace_back(&buf[0]);
	} else {
		str = &buf[0];
	}
#else
	str.insert(0, &buf[0]);
#endif
	buf[0] = 0;
	buf.clear();

	return (0 == cbPos);
}
