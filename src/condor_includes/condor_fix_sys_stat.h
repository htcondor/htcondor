#ifndef CONDOR_SYS_STAT_H
#define CONDOR_SYS_STAT_H

#include <sys/stat.h>

/**********************************************************************
**  Include whatever we need to get statfs and friends.
**********************************************************************/
#if defined(OSF1)
#  include <sys/mount.h>
#elif defined(LINUX) || defined(HPUX9)
#  include <sys/vfs.h>
#else
#  include <sys/statfs.h>
#endif

#if defined(HPUX9)
#include <sys/ustat.h>
#endif

#endif CONDOR_SYS_STAT_H

