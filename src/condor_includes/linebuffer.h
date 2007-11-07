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

#ifndef _LINEBUFFER_H
#define _LINEBUFFER_H

// Simple line buffering class
class LineBuffer
{
public:
	LineBuffer( int maxsize = 128 );
	virtual ~LineBuffer( void );
	int Buffer( const char ** buf, int *nbytes );
	int Buffer( char c );
	int Flush( void );
	virtual int Output( const char *buf, int len ) = 0;

private:
	int DoOutput( bool force );

	char	*buffer;			// Start of actual input buffer
	char	*bufptr;			// Current buffer pointer
	int		bufsize;			// Physical size of buffer
	int		bufcount;			// Count of bytes in the buffer
};

#endif /* _LINEBUFFER_H */
