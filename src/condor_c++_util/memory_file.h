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

#ifndef MEMORY_FILE_H
#define MEMORY_FILE_H

/*
memory_file simulates a file stored in memory.
The primary use of this class is to have a second implementation
of read/write/seek, against which to test the real read/write/seek,
*/

/* The bottom is defined for test suites that use this code in themselves */
#ifndef NO_CONDOR_COMMON
#include "condor_common.h"
#endif

class memory_file {
public:
	memory_file();
	~memory_file();

	/* Seek to a new file pointer */
	/* Return the new pointer */

	off_t	seek( off_t offset, int whence );

	/* Write data at the current file pointer */
	/* Returns the number of bytes read */

	ssize_t	write( char *data, size_t length );

	/* Read data from the current file pointer */
	/* Returns the number of bytes read */

	ssize_t	read( char *data, size_t length );

	/* Compare the contents of this object against */
	/* A UNIX file.  Return the number of bytes that */
	/* are different */

	int compare( char *filename );

private:

	/* Expand to this many bytes, if necessary */

	void ensure( int size );

	char *buffer;		// Pointer to the file data
	off_t pointer;		// Current seek pointer
	off_t filesize;		// Size of the simulated file
	off_t bufsize;		// Current size of the buffer
				// bufsize >= filesize
};

/*
Count the number of discrepancies between two buffers.
If any are found, display them and return the number of errors.
As a convenience for splitting buffers, the offset
argument is added to the "position" field in the output.
*/

extern int count_errors( char *b1, char *b2, int length, int offset );

#endif
