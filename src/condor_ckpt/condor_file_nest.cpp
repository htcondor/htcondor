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
	url = NULL;
	server = NULL;
	method = NULL;
	path = NULL;
	_condor_ckpt_disable();
}

CondorFileNest::~CondorFileNest()
{
	_condor_ckpt_enable();
	free( url );
	free( server );
	free( method );
	free( path );
}

int CondorFileNest::open( const char *u, int flags, int mode )
{
	free( url );
	url = strdup(u);

	free( method );
	free( server );
	free( path );
	method = server = path = NULL;
	filename_url_parse_malloc( url, &method, &server, &port, &path );

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

int CondorFileNest::ioctl( int cmd, long arg )
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

char const *CondorFileNest::get_url()
{
	return url ? url : "";
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
