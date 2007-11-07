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

#ifndef CONDOR_FILE_BUFFER_H
#define CONDOR_FILE_BUFFER_H

#include "condor_file.h"

class CondorChunk;

/**
This class buffers reads and writes by keeping variable-sized
buffers of data.  The maximum amount of data to store and
the maximum block size to use are given in the constructor:
<pre>
CondorFile *original = new CondorFileRemote;
CondorFile *faster = new CondorFileBuffer(original,10000,1000);
</pre>
However, they values may also be overridden in the URL like so:
<pre>
faster->open("buffer:(1000,100)local:/tmp/file");
</pre>
*/

class CondorFileBuffer : public CondorFile {
public:
	CondorFileBuffer( CondorFile *original, int buffer_size, int buffer_block_size );
	virtual ~CondorFileBuffer();

	virtual int open(const char *url, int flags, int mode);
	virtual int close();
	virtual int read(int offset, char *data, int length);
	virtual int write(int offset, char *data, int length);

	virtual int fcntl( int cmd, int arg );
	virtual int ioctl( int cmd, long arg );
	virtual int ftruncate( size_t length ); 
	virtual int fsync();
	virtual int flush();
	virtual int fstat( struct stat *buf );

	virtual int	is_readable();
	virtual int	is_writeable();
	virtual int	is_seekable();

	virtual int	get_size();
	virtual char const	*get_url();

	virtual int get_unmapped_fd();
	virtual int is_file_local();

private:
	void trim();
	void flush( int deallocate );
	void evict( CondorChunk *c );
	void clean( CondorChunk *c );

	// The url of this file
	char *url;

	// The raw file being buffered
	CondorFile *original;

	// A linked list of data chunks
	CondorChunk *head;

	// The integer time used for LRU.
	int time;

	// The logical size of the buffered file
	int size;

	// The maximum amount of data to buffer
	int buffer_size;

	// The largest chunk to buffer
	int buffer_block_size;
};

#endif
