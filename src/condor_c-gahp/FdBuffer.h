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

#ifndef FD_BUFFER_H
#define FD_BUFFER_H

#include "condor_common.h"
#include "../condor_c++_util/MyString.h"


/**
 *
 * This class encapsulates a fd and provides buffering,
 * i.e. it allows you to hold a result read out of the pipe until
 * a full line is read.
 *
 **/
class FdBuffer;

class FdBuffer {
public:
	FdBuffer ();
	FdBuffer (int fd);

	MyString * GetNextLine();

	static int Select (FdBuffer** wait_on, int count, int timeout, int * result);

	int getFd() { return fd; }
	void setFd(const int _fd) { fd = _fd; }
	const char * getBuffer() { return buffer.Value(); }

	int IsError() { return error; }

protected:
	int fd;
	MyString buffer;

	int newlineCount;
	int error;

	MyString * ReadNextLineFromBuffer();
	int HasNextLineInBuffer();

};
#endif
