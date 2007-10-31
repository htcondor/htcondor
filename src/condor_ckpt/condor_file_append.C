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

#include "condor_common.h"
#include "condor_file_append.h"
#include "condor_debug.h"

CondorFileAppend::CondorFileAppend( CondorFile *o )
{
	original = o;
	url = NULL;
}

CondorFileAppend::~CondorFileAppend()
{
	delete original;
	free( url );
}

int CondorFileAppend::open( const char *u, int flags, int mode )
{
	char *junk = (char *)malloc(strlen(u)+1);
	char *sub_url = (char *)malloc(strlen(u)+1);

	free( url );
	url = strdup(u);

	/* linux glibc 2.1 and presumeably 2.0 had a bug where the range didn't 
		work with 8bit numbers */
	#if defined(LINUX) && (defined(GLIBC20) || defined(GLIBC21))
	sscanf(url,"%[^:]:%[\x1-\x7F]",junk,sub_url);
	#else
	sscanf(url,"%[^:]:%[\x1-\xFF]",junk,sub_url);
	#endif
	free( junk );

	int result = original->open(sub_url,flags|O_APPEND,mode);
	free( sub_url );
	return result;
}

int CondorFileAppend::close()
{
	return original->close();
}

int CondorFileAppend::read( int offset, char *data, int length )
{
	return original->read(offset,data,length);
}

int CondorFileAppend::write( int offset, char *data, int length )
{
	return original->write(offset,data,length);
}

int CondorFileAppend::fcntl( int cmd, int arg )
{
	return original->fcntl(cmd,arg);
}

int CondorFileAppend::ioctl( int cmd, long arg )
{
	return original->ioctl(cmd,arg);
}

int CondorFileAppend::ftruncate( size_t length )
{
	return original->ftruncate(length);
}

int CondorFileAppend::fsync()
{
	return original->fsync();
}

int CondorFileAppend::flush()
{
	return original->flush();
}

int CondorFileAppend::fstat( struct stat *buf )
{
	return original->fstat(buf);
}

int CondorFileAppend::is_readable()
{
	return original->is_readable();
}

int CondorFileAppend::is_writeable()
{
	return original->is_writeable();
}

int CondorFileAppend::is_seekable()
{
	return original->is_seekable();
}

int CondorFileAppend::get_size()
{
	return original->get_size();
}

char const *CondorFileAppend::get_url()
{
	return url ? url : "";
}

int CondorFileAppend::get_unmapped_fd()
{
	return original->get_unmapped_fd();
}

int CondorFileAppend::is_file_local()
{
	return original->is_file_local();
}

