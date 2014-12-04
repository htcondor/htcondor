#include "condor_common.h"
#include "condor_fsync.h"
#include "condor_classad.h"
#include "generic_stats.h"

bool condor_fsync_on = true;

typedef _condor_auto_accum_runtime< stats_entry_probe<double> > condor_auto_runtime;
stats_entry_probe<double> condor_fsync_runtime; // this holds the count and runtime of fsync calls.

int condor_fsync(int fd, const char* /*path*/)
{
	if(!condor_fsync_on)
		return 0;

	condor_auto_runtime rt(condor_fsync_runtime);
	return fsync(fd);
}

int condor_fdatasync(int fd, const char* /*path*/)
{
	if (!condor_fsync_on)
	{
		return 0;
	}

	condor_auto_runtime rt(condor_fsync_runtime);
#ifdef HAVE_FDATASYNC
	return fdatasync(fd);
#else
	return fsync(fd);
#endif
}

