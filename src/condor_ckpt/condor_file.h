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

#ifndef CONDOR_FILE_H
#define CONDOR_FILE_H

#include "condor_common.h"

/**
CondorFile is a completely virtual class which defines that operations
that a storage medium must implement.  Actual methods for accessing
a file (local, remote, ioserver, etc.) are built by extending
CondorFile.
<p>
<pre>
                           CondorFile-----------------\
                          /     |    \                 \
                        /       |      \                \
                      /         V        \               \
        CondorFileBasic CondorFileAgent CondorFileBuffer CondorFileCompress
         |         |
         V         V
CondorFileRemote  CondorFileLocal
                 /                \
                /                  \
        CondorFileSpecial     CondorFileFD

</pre>
<p>
The basic file operations - open, close, read, and write - are
expected to be implemented for any sort of file, whether it be local,
remote, or some exotic storage method such as SRB.  These are
implemented in a descendant of CondorFile.  This allows FileTab->read()
to translate into whatever code is appropriate for the file.
<p>
Exotic operations such as fstatfs() are only supported by the old
method -- find the fd with _condor_file_table_map(), and attempt
the syscall.  If an exotic operation needs to do more than simply
get the fd of the file in question, then the operation needs to
be routed through CondorFileTable.
<p>
*/

class CondorFile {
public:
	CondorFile();
	virtual ~CondorFile();

	virtual int open( const char *url, int flags, int mode )=0;
	virtual int close()=0;
	virtual int read( int offset, char *data, int length )=0;
	virtual int write( int offset, char *data, int length )=0;

	virtual int fcntl( int cmd, int arg )=0;
	virtual int ioctl( int cmd, long arg )=0;
	virtual int ftruncate( size_t length )=0; 
	virtual int fsync()=0;
	virtual int flush()=0;
	virtual int fstat( struct stat *buf )=0;

	virtual int	is_readable()=0;
	virtual int	is_writeable()=0;
	virtual int	is_seekable()=0;

	virtual int	get_size()=0;
	virtual char const	*get_url()=0;

	virtual int get_unmapped_fd()=0;
	virtual int is_file_local()=0;

};

#endif
