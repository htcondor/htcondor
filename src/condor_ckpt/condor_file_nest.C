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

#ifdef ENABLE_NEST

#include "condor_file_nest.h"
#include "condor_error.h"
#include "filename_tools.h"
#include "condor_syscall_mode.h"
#include "condor_debug.h"

int nest_status_to_errno( NestReplyStatus status )
{
	switch(status) {
		case NEST_SUCCESS:
			return 0;
		case NEST_LOCAL_FILE_NOT_FOUND:
		case NEST_REMOTE_FILE_NOT_FOUND:
			return ENOENT;
		case NEST_INSUFFICIENT_SPACE:
			return ENOSPC;
		case NEST_NO_CONNECTION:
			return EPIPE;
		default:
			return EINVAL;
	}
}

CondorFileNest::CondorFileNest()
{
	url[0] = 0;
	server[0] = 0;
	path[0] = 0;
	_condor_ckpt_disable();
}

CondorFileNest::~CondorFileNest()
{
	_condor_ckpt_enable();
}

int CondorFileNest::open( const char *u, int flags, int mode )
{
	strcpy(url,u);
	filename_url_parse( url, method, server, &port, path );

	int scm = SetSyscalls(SYS_LOCAL|SYS_UNMAPPED);
	status = OpenNestConnection( con, server );
	SetSyscalls(scm);

	/* strip off the leading / */
	strcpy( path, &path[1] );

	if(status==NEST_SUCCESS) {
		return 0;
	} else {
		_condor_error_fatal("Couldn't connect to NeST server '%s': %s\n",server,NestErrorString(status));
		errno = nest_status_to_errno(status);
		return -1;
	}
}

int CondorFileNest::close()
{
	int scm = SetSyscalls(SYS_LOCAL|SYS_UNMAPPED);
	CloseNestConnection( con );
	SetSyscalls(scm);
}

int CondorFileNest::read( int offset, char *data, int length )
{
	int file_size;

	fprintf(stderr,"CondorFileNest::read(%d,%x,%d)\n",offset,data,length);

	file_size = get_size();

	if( offset>=file_size ) return 0;

	if( (offset+length)>=file_size ) {
		length = file_size-offset;
	}

	int scm = SetSyscalls(SYS_LOCAL|SYS_UNMAPPED);
	status = ReadNestBytes( con, path, offset, data, length );
	SetSyscalls(scm);

	if(status==NEST_SUCCESS) {
		return length;
	} else {
		errno = nest_status_to_errno(status);
		return -1;
	}
}

int CondorFileNest::write( int offset, char *data, int length )
{
	fprintf(stderr,"CondorFileNest::write(%d,%x,%d)\n",offset,data,length);

	int scm = SetSyscalls(SYS_LOCAL|SYS_UNMAPPED);
	status = WriteNestBytes( con, path, offset, data, length, 1 );
	SetSyscalls(scm);

	if(status==NEST_SUCCESS) {
		return length;
	} else {
		errno = nest_status_to_errno(status);
		return -1;
	}
}

int CondorFileNest::fcntl( int cmd, int arg )
{
	errno = EINVAL;
	return -1;
}

int CondorFileNest::ioctl( int cmd, int arg )
{
	errno = EINVAL;
	return -1;
}

int CondorFileNest::ftruncate( size_t length )
{
	errno = EINVAL;
	return -1;
}

int CondorFileNest::fsync()
{
	return 0;
}

int CondorFileNest::flush()
{
	return 0;
}

int CondorFileNest::fstat( struct stat *buf )
{
	errno = EINVAL;
	return -1;
}

int CondorFileNest::is_readable()
{
	return 1;
}

int CondorFileNest::is_writeable()
{
	return 1;
}

int CondorFileNest::is_seekable()
{
	return 1;
}

int CondorFileNest::get_size()
{
	long size;

	int scm = SetSyscalls(SYS_LOCAL|SYS_UNMAPPED);
	status = NestFilesize( size, path, con );
	SetSyscalls(scm);

	if(status==NEST_SUCCESS) {
		return size;
	} else {
		errno = nest_status_to_errno(status);
		return -1;
	}
}

char *CondorFileNest::get_url()
{
	return url;
}

int CondorFileNest::get_unmapped_fd()
{
	return -1;
}

int CondorFileNest::is_file_local()
{
	return 0;
}

#endif /* ENABLE_NEST */
