
#include "condor_common.h"
#include "condor_file_append.h"
#include "condor_debug.h"

CondorFileAppend::CondorFileAppend( CondorFile *o )
{
	original = o;
	url[0] = 0;
}

CondorFileAppend::~CondorFileAppend()
{
	delete original;
}

int CondorFileAppend::cfile_open( const char *u, int flags, int mode )
{
	char junk[_POSIX_PATH_MAX];
	char sub_url[_POSIX_PATH_MAX];

	strcpy(url,u);

	/* linux glibc 2.1 and presumeably 2.0 had a bug where the range didn't 
		work with 8bit numbers */
	#if defined(LINUX) && (defined(GLIBC20) || defined(GLIBC21))
	sscanf(url,"%[^:]:%[\x1-\x7F]",junk,sub_url);
	#else
	sscanf(url,"%[^:]:%[\x1-\xFF]",junk,sub_url);
	#endif

	return original->cfile_open(sub_url,flags|O_APPEND,mode);
}

int CondorFileAppend::cfile_close()
{
	return original->cfile_close();
}

int CondorFileAppend::cfile_read( int offset, char *data, int length )
{
	return original->cfile_read(offset,data,length);
}

int CondorFileAppend::cfile_write( int offset, char *data, int length )
{
	return original->cfile_write(offset,data,length);
}

int CondorFileAppend::cfile_fcntl( int cmd, int arg )
{
	return original->cfile_fcntl(cmd,arg);
}

int CondorFileAppend::cfile_ioctl( int cmd, int arg )
{
	return original->cfile_ioctl(cmd,arg);
}

int CondorFileAppend::cfile_ftruncate( size_t length )
{
	return original->cfile_ftruncate(length);
}

int CondorFileAppend::cfile_fsync()
{
	return original->cfile_fsync();
}

int CondorFileAppend::cfile_flush()
{
	return original->cfile_flush();
}

int CondorFileAppend::cfile_fstat( struct stat *buf )
{
	return original->cfile_fstat(buf);
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

char *CondorFileAppend::get_url()
{
	return url;
}

int CondorFileAppend::get_unmapped_fd()
{
	return original->get_unmapped_fd();
}

int CondorFileAppend::is_file_local()
{
	return original->is_file_local();
}

