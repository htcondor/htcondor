/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
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

#include "condor_common.h"
#include "condor_debug.h"
#include "linebuffer.h"

// Constructor
LineBuffer::
LineBuffer( int maxsize )
{
	// TODO This is fucked up but it works for now TODO
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
	while( nbytes-- )
	{
		int status = Buffer( *bptr++ );
		if ( status ) {
			*buf = bptr;
			*nptr = nbytes;
			return status;
		}
	}
	return 0;
}

// Buffer a single byte
int
LineBuffer::
Buffer( char c )
{
	*bufptr = c;
	bufcount++;
	if (  ( '\n' == c ) || ( '\0' == c ) || ( bufcount >= bufsize ) )
	{
		*bufptr = '\0';
		int status = Output( buffer, bufcount );
		bufptr = buffer;
		bufcount = 0;
		if ( status ) {
			return status;
		}
	} else {
		bufptr++;
	}
	return 0;
}

// Flush the input buffer
int
LineBuffer::
Flush( void )
{
	if ( bufcount )
	{
		*bufptr = '\0';
		int status = Output( buffer, bufcount );
		bufptr = buffer;
		bufcount = 0;
		return status;
	}
	return 0;
}
