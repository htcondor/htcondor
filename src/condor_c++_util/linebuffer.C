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

#include "condor_common.h"
#include "condor_debug.h"
#include "linebuffer.h"

// Constructor
LineBuffer::
LineBuffer( int maxsize )
{
	// TODO This is fubar but it works for now TODO
	//bufptr = buffer = (char *) malloc( maxsize + 1 );
	bufptr = buffer = (char *) malloc( maxsize + 1 );
	assert( buffer );
	bufsize = maxsize;
	bufcount = 0;
}

// Destructor
LineBuffer::
~LineBuffer( void )
{
	free( buffer );
}

// Buffer a block of data
int
LineBuffer::
Buffer( const char ** buf, int *nptr )
{
	const char	*bptr = *buf;
	int			nbytes = *nptr;

	// Loop through the whole input buffer
	while( nbytes-- ) {
		int status = Buffer( *bptr++ );
		if ( status ) {
			*buf = bptr;
			*nptr = nbytes;
			return status;
		}
	}
	*nptr = 0;
	return 0;
}

// Buffer a single byte
int
LineBuffer::
Buffer( char c )
{
	if (  ( '\n' == c ) || ( '\0' == c ) || ( bufcount >= bufsize ) ) {
		return DoOutput( false );
	} else {
		*bufptr = c;
		bufptr++;
		bufcount++;
		return 0;
	}
}

// Flush the input buffer
int
LineBuffer::
Flush( void )
{
	return DoOutput( false );
}

// Send out data
int
LineBuffer::
DoOutput( bool force ) {

	// If there is data, or we're in force, just do it..
	if ( ( bufcount ) || ( force )  ) {
		*bufptr = '\0';
		int status = Output( buffer, bufcount );
		bufptr = buffer;
		bufcount = 0;
		return status;
	}
	return 0;

}
