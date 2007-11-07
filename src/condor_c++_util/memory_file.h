/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
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
