#ifndef _CONDOR_GETMNT_H
#define _CONDOR_GETMNT_H

#if defined(ULTRIX42) || defined(ULTRIX43)
#	include <sys/mount.h>
#endif

#if !defined(OSF1) && !defined(ULTRIX42) && !defined(ULTRIX43) && !defined(AIX32)
#	include <mntent.h>
#endif

#if defined(AIX32)
#	include <sys/mntctl.h>
#	include <sys/vmount.h>
#	include <sys/sysmacros.h>
#endif

#if !defined(NMOUNT)
#define NMOUNT 256
#endif

#if !defined(ULTRIX42) && !defined(ULTRIX43)
struct fs_data_req {
	dev_t	dev;
	char	*devname;
	char	*path;
};
struct fs_data {
	struct fs_data_req fd_req;
};
#define NOSTAT_MANY 0
#endif

#if NMOUNT < 256
#undef  NMOUNT
#define NMOUNT  256
#endif

#endif /* _CONDOR_GETMNT_H */
