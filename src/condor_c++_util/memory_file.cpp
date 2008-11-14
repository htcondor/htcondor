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


#define _CONDOR_ALLOW_OPEN 1   // because this is used in the test suite

/* the below is defined in test suite programs that use these files. */
#ifndef NO_CONDOR_COMMON
#include "condor_common.h"
#endif

#include "memory_file.h"
#include "condor_fix_iostream.h"

static const int DEFAULT_BUFFER_SIZE=1024;
static const int COMPARE_BUFFER_SIZE=10000;   

memory_file::memory_file()
{
	buffer = new char[DEFAULT_BUFFER_SIZE];
	bufsize = DEFAULT_BUFFER_SIZE;
	memset(buffer, 0, bufsize);
	pointer = filesize = 0;
}

memory_file::~memory_file()
{
	if( buffer ) delete [] buffer;
}

/*
Compare this memory_file against a real file.
Return the number of errors found.
*/

int memory_file::compare( char *filename )
{
	int errors=0;
	off_t position=0, chunksize=0;
	char cbuffer[COMPARE_BUFFER_SIZE];

	int fd = open(filename,O_RDONLY);
	if( fd==-1 ) {
		cerr << "Couldn't open " << filename << endl;
		return 100;
	}

	while(1) {
		chunksize = ::read(fd,cbuffer,COMPARE_BUFFER_SIZE);
		if(chunksize<=0) break;

		errors += count_errors( cbuffer, &buffer[position], chunksize, position );
		position += chunksize;

		if( errors>10 ) {
			cout << "Too many errors, stopping.\n";
			break;
		}

	}

	if(position!=filesize) {
		cout << "SIZE ERROR:\nFile was " << position
		     << " bytes, but mem was " << filesize
		     << " bytes.\n";
		errors++;
	}

	::close(fd);

	return errors;
}

/*
Move the current seek pointer to a new position, as lseek(2).
Return the new pointer, or -1 in case of an error.
*/

off_t memory_file::seek( off_t offset, int whence )
{
	off_t newpointer;

	switch(whence) {
		case SEEK_SET:
		newpointer = offset;
		break;
		case SEEK_CUR:
		newpointer = pointer+offset;
		break;
		case SEEK_END:
		newpointer = filesize+offset;
		break;
	}

	if( newpointer<0 ) {
		return -1;
	} else {
		pointer = newpointer;
	}

	return pointer;
}

/*
Read from the simulated file, as read(2).
Returns the number of bytes read, or an error.
*/

ssize_t memory_file::read( char *data, size_t length )
{

	if( (data==0) || (pointer<0) ) return -1;
	if( pointer>=filesize ) return 0;

	if(length==0) return 0;

	if((pointer+(off_t)length)>filesize) length = filesize-pointer;
	memcpy(data,&buffer[pointer],length);

	pointer += length;
	return length;
}

/*
Write to the simulated file, as write(2).
Returns the number of bytes written, or an error.
*/

ssize_t memory_file::write( char *data, size_t length )
{
	if( (data==0) || (pointer<0) ) return -1;

	if(length==0) return 0;

	ensure(pointer+length);

	memcpy(&buffer[pointer],data,length);
	pointer+=length;
	if( pointer>filesize ) filesize=pointer;

	return length;
}

/*
Expand the buffer so that it can hold up to needed.
To minimize memory allocations, the buffer size is
increased by powers of two.
*/

void memory_file::ensure( int needed )
{
	if( needed>bufsize ) {
		int newsize = bufsize;
		while(newsize<needed) newsize*=2;

		char *newbuffer = new char[newsize];
		memcpy(newbuffer,buffer,bufsize);
		memset(&newbuffer[bufsize], 0, newsize-bufsize);
		delete [] buffer;
		buffer = newbuffer;
		bufsize = newsize;
	}
}

/*
Count the number of discrepancies between two buffers and
return that number.  Display any errors along the way.
*/

int count_errors( char *b1, char *b2, int length, int offset )
{
	int errors=0;

	for( int i=0; i<length; i++ ) {
		if( b1[i]!=b2[i] ) {
			if(!errors) cout << "FOUND ERROR:\npos\ta\tb\n";
			errors++;
			cout << (i+offset) << '\t' << (int)b1[i] << '\t' << (int)b2[i] << endl;
			if( errors>50 ) {
				cout << "Too many errors, stopping." << endl;
				return 50;
			}
		}
	}

	return errors;
}

