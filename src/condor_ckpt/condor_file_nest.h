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
#ifndef CONDOR_FILE_NEST_H
#define CONDOR_FILE_NEST_H

#ifdef ENABLE_NEST

#include "condor_common.h"
#include "condor_file.h"
#include "nest_speak.h"

class CondorFileNest : public CondorFile {
public:
	CondorFileNest();
	virtual ~CondorFileNest();

	virtual int open( const char *url, int flags, int mode );
	virtual int close();
	virtual int read( int offset, char *data, int length );
	virtual int write( int offset, char *data, int length );

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
	NestConnection con;
	NestReplyStatus status;

	char	url[_POSIX_PATH_MAX];
	char	method[_POSIX_PATH_MAX];
	char	server[_POSIX_PATH_MAX];
	char	path[_POSIX_PATH_MAX];
	int	port;
};

#endif /* ENABLE_NEST */

#endif
