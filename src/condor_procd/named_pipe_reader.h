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

#ifndef _NAMED_PIPE_READER_H
#define _NAMED_PIPE_READER_H

class NamedPipeReader {

public:
	// create a new FIFO for reading with the given
	// "address" (which is really a node in the
	// filesystem)
	//
	NamedPipeReader(const char*);
	
	// clean up open FDs, file system droppings, and
	// dynamically allocated memory
	//
	~NamedPipeReader();

	// change the owner of the named pipe file system node
	//
	void change_owner(uid_t);

	// read data off the pipe
	//
	void read_data(void*, int);

	// return true if a the named pipe becomes ready
	// for reading within the given timeout period
	//
	bool poll(int);

private:
	// the filesystem name for our FIFO
	//
	char* m_addr;

	// the ownership uid for the pipe
	//
	uid_t m_uid;

	// an O_RDONLY file descriptor for our FIFO
	//
	int m_pipe;

	// a O_WRONLY file descriptor for the FIFO; this
	// never gets used but we keep it open so that EOF
	// is never read from m_pipe
	//
	int m_dummy_pipe;
};

#endif
