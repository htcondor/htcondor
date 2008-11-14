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
