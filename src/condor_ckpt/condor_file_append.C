
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

int CondorFileAppend::open( const char *u, int flags, int mode )
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

	return original->open(sub_url,flags|O_APPEND,mode);
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

int CondorFileAppend::ioctl( int cmd, int arg )
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

