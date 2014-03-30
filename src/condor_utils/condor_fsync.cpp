#include "condor_common.h"
#include "condor_fsync.h"

bool condor_fsync_on = true;

int condor_fsync(int fd, const char* /*path*/)
{
	if(!condor_fsync_on)
		return 0;

	return fsync(fd);
}

int condor_fdatasync(int fd, const char* /*path*/)
{
	if (!condor_fsync_on)
	{
		return 0;
	}

#ifdef HAVE_FDATASYNC
	return fdatasync(fd);
#else
	return fsync(fd);
#endif
}

