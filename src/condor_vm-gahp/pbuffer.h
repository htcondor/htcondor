/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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

#ifndef P_BUFFER_H
#define P_BUFFER_H

#include "MyString.h"

/**
 *
 * This class encapsulates a DaemonCore pipe and provides buffering.
 * E.g. it allows you to hold a result read out of the pipe until
 * a full line is read. The getNextLine() and Write() methods are
 * designed to be used in read pipe and write pipe handlers,
 * respectively. Note that an object of this type is to be used
 * either strictly for reading (using getNextLine()) or strictly
 * for writing (using Write()).
 *
 **/

const int P_BUFFER_READAHEAD_SIZE = 2048;

class PBuffer {
public:
	PBuffer ();
	PBuffer (int pipe_end);

	MyString * GetNextLine();
	int Write (const char * toWrite = NULL);

	int getPipeEnd() { return pipe_end; }
	void setPipeEnd(const int _pipe_end) { pipe_end = _pipe_end; }
	const char * getBuffer() { return buffer.Value(); }

	int IsEmpty();
	bool IsError() { return error; }
	bool IsEOF() { return eof; }

protected:
	int pipe_end;
	MyString buffer;

	bool error;
	bool eof;

	bool last_char_was_escape;

	char readahead_buffer[P_BUFFER_READAHEAD_SIZE];
	int readahead_length;
	int readahead_index;
};

#endif
