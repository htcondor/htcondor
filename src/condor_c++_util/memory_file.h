/*
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
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
