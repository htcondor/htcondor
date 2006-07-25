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
	virtual int ioctl( int cmd, int arg );
	virtual int ftruncate( size_t length ); 
	virtual int fsync();
	virtual int flush();
	virtual int fstat( struct stat *buf );

	virtual int	is_readable();
	virtual int	is_writeable();
	virtual int	is_seekable();

	virtual int	get_size();
	virtual char	*get_url();

	virtual int get_unmapped_fd();
	virtual int is_file_local();

private:
	void trim();
	void flush( int deallocate );
	void evict( CondorChunk *c );
	void clean( CondorChunk *c );

	// The url of this file
	char url[_POSIX_PATH_MAX];

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
