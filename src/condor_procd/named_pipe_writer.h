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

#ifndef _NAMED_PIPE_WRITER_H
#define _NAMED_PIPE_WRITER_H

class NamedPipeWatchdog;

class NamedPipeWriter {

public:

	NamedPipeWriter() : m_initialized(false),
	                    m_pipe(-1),
	                    m_watchdog(NULL) { }

	// we use a plain old member function instead of the constructor
	// to do the real initialization work so that we can give our
	// caller an indication if something goes wrong
	//
	// init a new FIFO for writing with the given
	// "address" (which is really a FIFO node in the
	// filesystem)
	//
	bool initialize(const char*);
	
	// clean up
	//
	~NamedPipeWriter();

	// enable a watchdog on this writer
	//
	void set_watchdog(NamedPipeWatchdog*);

	// write to the pipe
	//
	bool write_data(void*, int);

private:

	// set true one we're successfully initialized
	//
	bool m_initialized;

	// a O_WRONLY file descriptor for our FIFO
	//
	int m_pipe;

	// an optional watchdog; if set this gives us
	// protection against hanging trying to talk to a
	// crashed process
	//
	NamedPipeWatchdog* m_watchdog;
};

#endif
