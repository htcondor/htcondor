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

#ifdef FS_UTIL_TEST
# include <stdio.h>
# include <stdlib.h>
# include <errno.h>
#else
# include "condor_common.h"
# include "condor_debug.h"
#endif

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "condor_constants.h"
#include "fs_util.h"

#if ( defined HAVE_STATVFS ) && ( defined HAVE_STRUCT_STATVFS_F_BASETYPE )
# define USE_STATVFS
#elif defined HAVE_STATFS
# define USE_STATFS
#endif


#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif

#ifdef USE_STATFS
# ifdef HAVE_SYS_MOUNT_H
#  include <sys/mount.h>
# endif
# ifdef HAVE_USTAT_H
#  include <ustat.h>
# endif
# ifdef HAVE_SYS_STATFS_H
#  include <sys/statfs.h>
# endif
# ifdef HAVE_LINUX_NFSD_CONST_H
#  include <linux/nfsd/const.h>
#  ifndef NFS_SUPER_MAGIC
#   undef USE_STATFS
#  endif
# endif
#endif

#ifdef USE_STATVFS
# ifdef HAVE_SYS_STATVFS_H
#  include <sys/statvfs.h>
# endif
# ifdef HAVE_SYS_VFS_H
#  include <sys/vfs.h>
# endif
#endif

#include <string.h>

#if ( defined USE_STATVFS )
static int
detect_nfs_statvfs( const char *path, BOOLEAN *is_nfs )
{
	int status;
	struct statvfs	buf;

	status = statvfs( path, &buf );
	if ( status < 0 ) {
#    ifndef FS_UTIL_TEST
		dprintf( D_ALWAYS, "statvfs() failed: %d/%s\n",
				 errno, strerror( errno ) );
#    endif
		return -1;
	}
	if ( !strncmp( buf.f_basetype, "nfs", 3 ) ) {
		*is_nfs = TRUE;
	} else {
		*is_nfs = FALSE;
	}
# ifdef FS_UTIL_TEST
	printf( "detect_nfs_statvfs: f_basetype = %s -> %s\n",
			buf.f_basetype, *is_nfs ? "true" : "false" );
# endif
	return 0;
}

#elif( defined USE_STATFS )
static int
detect_nfs_statfs( const char *path, BOOLEAN *is_nfs )
{
	int status;
	struct statfs	buf;

# if (STATFS_ARGS == 2)
	status = statfs( path, &buf );
# elif (STATFS_ARGS == 4)
	status = statfs( path, &buf, sizeof(buf), 0 );
# else
#  error "Unknown statfs() implemenation"
# endif
	if ( status < 0 ) {
#	  if (!defined FS_UTIL_TEST )
		dprintf( D_ALWAYS, "statfs() failed: %d/%s\n",
				 errno, strerror( errno ) );
#	  endif
		return -1;
	}

	// 1st try: Look at the fstypename string
# if defined HAVE_STRUCT_STATFS_F_FSTYPENAME
	if ( !strncmp( buf.f_fstypename, "nfs", 3 ) ) {
		*is_nfs = TRUE;
	} else {
		*is_nfs = FALSE;
	}
#  ifdef FS_UTIL_TEST
	printf( "detect_nfs_statfs: f_fstypename = %s -> %s\n",
			buf.f_fstypename, *is_nfs ? "true" : "false" );
#  endif

	// #2: Look at the f_type value
# elif defined HAVE_STRUCT_STATFS_F_TYPE
	if ( buf.f_type == NFS_SUPER_MAGIC ) {
		*is_nfs = TRUE;
	} else {
		*is_nfs = FALSE;
	}
#  ifdef FS_UTIL_TEST
	printf( "detect_nfs_statfs: f_type = %x -> %s\n",
			buf.f_type, *is_nfs ? "true" : "false" );
#  endif

	// #3: This is untested: look at fstype
# elif defined HAVE_STRUCT_STATFS_F_FSTYP
#   error "Don't know how to evaluate f_fstyp yet"

	// #3: Completely unknown
# else
#	error "Neither F_TYPE or F_FSTYP defined!"

# endif

	return 0;
}
#endif

int
fs_detect_nfs( const char *path,
			   BOOLEAN *is_nfs )
{
#if defined USE_STATFS
	return detect_nfs_statfs( path, is_nfs );
#elif defined USE_STATVFS
	return detect_nfs_statvfs( path, is_nfs );
#else
#  if !defined(WIN32)
#	warning "No valid fs type detection"
#  endif
	*is_nfs = FALSE;
	return 0;
#endif

}

#ifdef FS_UTIL_TEST
int
main( int argc, const char *argv[] )
{
	const char *path = argv[1];
	BOOLEAN	is_nfs;

	if ( argc != 2 ) {
		fprintf( stderr, "usage: fs_util_test path\n" );
		exit( 1 );
	}
	if ( fs_detect_nfs( path, &is_nfs ) < 0 ) {
		fprintf( stderr, "fs_detect_nfs(%s) failed\n", path );
		exit( 1 );
	}
	printf( "%s %s on NFS\n", path, is_nfs ? "is" : "is not" );

	return 0;
}
#endif
