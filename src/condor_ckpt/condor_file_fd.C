
#include "condor_file_fd.h"

CondorFileFD::CondorFileFD() : CondorFileLocal()
{
}

CondorFileFD::~CondorFileFD()
{
}

int CondorFileFD::open(const char *url_in, int flags, int mode)
{
	strcpy( url, url_in );
	sscanf( url, "fd:%d", &fd );

	switch( flags & O_ACCMODE ) {
		case O_RDONLY:
			readable = 1;
			writeable = 0;
			break;
		case O_WRONLY:
			readable = 0;
			writeable = 1;
			break;
		case O_RDWR:
			readable = 1;
			writeable = 1;
			break;
		default:
			return -1;
	}

	size = -1;

	return fd;
}

int CondorFileFD::close()
{
	return 0;
}
