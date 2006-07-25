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
#ifndef CONDOR_FILE_COMPRESS_H
#define CONDOR_FILE_COMPRESS_H

#include "condor_file.h"
#include "condor_compress.h"

/*
This object transparently converts any CondorFile into a gzipped file.
In general, only sequential access is supported.  Random-access reading
is allowed but is very slow because it is implemented by reading intervening
data.

Only the most basic flavor of compression, as described by the gzip documentation,
is supported.  This seems to interact just fine with normal invocations of
gzip, however, I would not be surprised were there more complicated formats
which this does not understand.
*/

#define COMPRESS_BUFFER_SIZE 32768

class CondorFileCompress : public CondorFile {
public:
	CondorFileCompress( CondorFile *original );
	virtual ~CondorFileCompress();

	virtual int open(const char *url, int flags, int mode);
	virtual int close();
	virtual int read(int offset, char *data, int length);
	virtual int write(int offset, char *data, int length);

	virtual int fcntl( int cmd, int arg );
	virtual int ioctl( int cmd, int arg );
	virtual int ftruncate( size_t length ); 
	virtual int fstat( struct stat* buf );
	virtual int fsync();
	virtual int flush();

	virtual int	is_readable();
	virtual int	is_writeable();
	virtual int	is_seekable();

	virtual int	get_size();
	virtual char	*get_url();

	virtual int get_unmapped_fd();
	virtual int is_file_local();

private:

	void	end_compression();

	int	write_data( char *data, int length, int flush );
	void	write_complete();
	int	write_header();
	int	write_int( int bytes, int value );

	int	read_consume( int length );
	int	read_data( char *data, int length );
	int	read_header();
	int	read_int( int bytes );
	void	read_string();

	// The URL of this file
	char url[_POSIX_PATH_MAX];

	// Compressed data buffer
	char buffer[COMPRESS_BUFFER_SIZE];

	// The last type of action performed
	enum {NONE,READ,WRITE} last_action;

	// The current compression state
	z_stream stream;

	// The file which is to be compressed
	CondorFile *original;

	// Current offset into the physical file
	int poffset;

	// Current offset into the virtual (compressed) file
	int voffset;

	// Accumulating checksum for writes
	int crc;
};

#endif /* CONDOR_FILE_COMPRESS_H */
