#ifndef CONDOR_SYS_STAT_H
#define CONDOR_SYS_STAT_H

#ifdef IRIX53
#define _save_XOPEN4UX _XOPEN4UX
#undef _XOPEN4UX
#define _XOPEN4UX 1
#endif


#include <sys/stat.h>


#ifdef IRIX53
#undef _XOPEN4UX
#define _XOPEN4UX _save_XOPEN4UX
#undef _save_XOPEN4UX
#endif

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

