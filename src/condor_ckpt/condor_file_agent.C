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

#include "condor_file_agent.h"
#include "condor_file_local.h"
#include "condor_debug.h"
#include "condor_error.h"
#include "condor_syscall_mode.h"

#define KB 1024
#define TRANSFER_BLOCK_SIZE (32*KB)

static char buffer[TRANSFER_BLOCK_SIZE];

CondorFileAgent::CondorFileAgent( CondorFile *file )
{
	original = file;
}

CondorFileAgent::~CondorFileAgent()
{
	delete original;
}

int CondorFileAgent::open( const char *url_in, int flags, int mode )
{
	int pos=0, chunk=0, result=0;
	int local_flags;
	char junk[_POSIX_PATH_MAX];
	char sub_url[_POSIX_PATH_MAX];

	strcpy( url, url_in );

	/* linux glibc 2.1 and presumeably 2.0 had a bug where the range didn't 
		work with 8bit numbers */
	#if defined(LINUX) && (defined(GLIBC20) || defined(GLIBC21))
	sscanf( url, "%[^:]:%[\x1-\x7F]",junk,sub_url );
	#else
	sscanf( url, "%[^:]:%[\x1-\xFF]",junk,sub_url );
	#endif


	// First, fudge the flags.  Even if the file is opened
	// write-only, we must open it read/write to get the 
	// original data.

	// We need not worry especially about O_CREAT and O_TRUNC
	// because these will be applied at the original open,
	// and their effects will be preserved through pull_data().
	
	if( flags&O_WRONLY ) {
		flags = flags&~O_WRONLY;
		flags = flags|O_RDWR;
	}

	// The local copy is created anew, opened read/write.

	local_flags = O_RDWR|O_CREAT|O_TRUNC;

	// O_APPEND is a little tricky.  In this case, it is applied
	// to both the original and the copy, but the data must not
	// be pulled.

	if( flags&O_APPEND ){
		local_flags = local_flags|O_APPEND;
	}

	// Open the original file.

	result = original->open(sub_url,flags,mode);
	if(result<0) return -1;

	// Choose where to store the file.
	// Notice that tmpnam() may do funny syscalls to make sure that
	// the name is not in use.  So, it must be done in local mode.
	// Eventually, this should be in a Condor spool directory.

	int scm = SetSyscalls(SYS_LOCAL|SYS_UNMAPPED);
	tmpnam(local_filename);
	SetSyscalls(scm);

	sprintf(local_url,"local:%s",local_filename);

	// Open the local copy, with a private mode.

	local_copy = new CondorFileLocal;
	result = local_copy->open(local_url,local_flags,0700);
	if(result<0) _condor_error_retry("Couldn't create temporary file '%s'!",local_url);

	// Now, delete the file right away.
	// If we get into trouble and can't clean up properly,
	// the file system will free it for us.

	scm = SetSyscalls(SYS_LOCAL|SYS_UNMAPPED);
	unlink( local_filename );
	SetSyscalls(scm);

	// If the file has not been opened for append,
	// then yank all of the data in.

	dprintf(D_ALWAYS,"CondorFileAgent: Copying %s into %s\n",original->get_url(),local_copy->get_url());

	if( !(flags&O_APPEND) ){
		do {
			chunk = original->read(pos,buffer,TRANSFER_BLOCK_SIZE);
			if(chunk<0) _condor_error_retry("CondorFileAgent: Couldn't read from '%s'",original->get_url());

			result = local_copy->write(pos,buffer,chunk);
			if(result<0) _condor_error_retry("CondorFileAgent: Couldn't write to '%s'",local_url);
		
			pos += chunk;
		} while(chunk==TRANSFER_BLOCK_SIZE);		
	}

	// Return success!

	return 1;
}

int CondorFileAgent::close()
{
	int pos=0, chunk=0, result=0;

	// If the original file was opened for writing, then
	// push the local copy back out.  if the original file
	// was opened append-only, then the data is simply appended
	// by the operating system at the other end.

	if( original->is_writeable() ) {

		dprintf(D_ALWAYS,"CondorFileAgent: Copying %s back into %s\n",local_copy->get_url(),original->get_url());

		original->ftruncate(local_copy->get_size());

		do {
			chunk = local_copy->read(pos,buffer,TRANSFER_BLOCK_SIZE);
			if(chunk<0) _condor_error_retry("CondorFileAgent: Couldn't read from '%s'",local_url);
	
			result = original->write(pos,buffer,chunk);
			if(result<0) _condor_error_retry("CondorFileAgent: Couldn't write to '%s'",original->get_url());
				
			pos += chunk;
		} while(chunk==TRANSFER_BLOCK_SIZE);
	}

	// Close the original file.  This must happen before the
	// local copy is closed, so that any failure can be reported
	// before the local copy is lost.

	result = original->close();
	if(result<0) return result;

	// Finally, close and delete the local copy.
	// There is nothing reasonable we can do if the local close fails.

	result = local_copy->close();
	delete local_copy;

	return 0;
}

int CondorFileAgent::read( int offset, char *data, int length )
{
	if( is_readable() ) {
		return local_copy->read( offset, data, length );
	} else {
		errno = EINVAL;
		return -1;
	}
}

int CondorFileAgent::write( int offset, char *data, int length )
{
	if( is_writeable() ) {
		return local_copy->write( offset,data, length );
	} else {
		errno = EINVAL;
		return -1;
	}
}

int CondorFileAgent::fcntl( int cmd, int arg )
{
	return local_copy->fcntl(cmd,arg);
}

int CondorFileAgent::ioctl( int cmd, int arg )
{
	return local_copy->ioctl(cmd,arg);
}

int CondorFileAgent::ftruncate( size_t length )
{
	return local_copy->ftruncate( length );
}

int CondorFileAgent::fsync()
{
	return local_copy->fsync();
}

int CondorFileAgent::flush()
{
	return local_copy->flush();
}

int CondorFileAgent::fstat(struct stat *buf)
{
	struct stat local;
	int ret, ret2;

	ret = original->fstat(buf);
	if (ret != 0){
		return ret;
	}

	ret2 = local_copy->fstat(&local);
	if (ret2 != 0){
		return ret2;
	}

	buf->st_size = local.st_size;
	buf->st_atime = local.st_atime;
	buf->st_mtime = local.st_mtime;

	return ret2;
}

int CondorFileAgent::is_readable()
{
	return original->is_readable();
}

int CondorFileAgent::is_writeable()
{
	return original->is_writeable();
}

int CondorFileAgent::is_seekable()
{
	return 1;
}

int CondorFileAgent::get_size()
{
	return local_copy->get_size();
}

char * CondorFileAgent::get_url()
{
	return url;
}

int CondorFileAgent::get_unmapped_fd()
{
	return local_copy->get_unmapped_fd();
}

int CondorFileAgent::is_file_local()
{
	return 1;
}









