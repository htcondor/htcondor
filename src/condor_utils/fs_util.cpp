/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


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

#include "basename.h"
#include "fs_util.h"

// Historically, there was also a USE_STATVFS that only was used
// on Solaris.
#if defined HAVE_STATFS
# define USE_STATFS
#endif


#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif

#ifdef USE_STATFS
#ifdef LINUX
#include <sys/statfs.h>
#include <linux/magic.h>
#else
// statfs isn't POSIX; MacOS has the same function but in a different
// header file compared to Linux.
#include <sys/mount.h>
#define NFS_SUPER_MAGIC 0x6969	// From the man pages of statfs()
#endif
#endif

#include <string.h>

#if( defined USE_STATFS )
static int
detect_nfs_statfs( const char *path, bool *is_nfs )
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

	if ( ( status < 0 ) && ( errno == ENOENT )) {
			// file doesn't exist yet, so check parent directory.
			char *_dir_name;
			_dir_name = condor_dirname(path);
# ifdef FS_UTIL_TEST
			printf("checking %s instead...\n", _dir_name);
# endif
# if (STATFS_ARGS == 2)
			status = statfs( _dir_name, &buf );
# elif (STATFS_ARGS == 4)
			status = statfs( _dir_name, &buf, sizeof(buf), 0 );
# else
#  error "Unknown statfs() implemenation"
# endif
			free(_dir_name);
	}

	if ( status < 0 ) {
#	  if (!defined FS_UTIL_TEST )
		dprintf( D_ALWAYS, "statfs(%s) failed: %d/%s\n",
				 path, errno, strerror( errno ) );
		if (EOVERFLOW == errno) {
			dprintf(D_ALWAYS,
					"statfs overflow, if %s is a large volume make "
					"sure you have a 64 bit version of Condor\n", path);
		}
#	  endif
		return -1;
	}

	// 1st try: Look at the fstypename string
# if defined HAVE_STRUCT_STATFS_F_FSTYPENAME
	if ( !strncmp( buf.f_fstypename, "nfs", 3 ) ) {
		*is_nfs = true;
	} else {
		*is_nfs = false;
	}
#  ifdef FS_UTIL_TEST
	printf( "detect_nfs_statfs: f_fstypename = %s -> %s\n",
			buf.f_fstypename, *is_nfs ? "true" : "false" );
#  endif

	// #2: Look at the f_type value
# elif defined(UNIX)
	if ( buf.f_type == NFS_SUPER_MAGIC ) {
		*is_nfs = true;
	} else {
		*is_nfs = false;
	}
#  ifdef FS_UTIL_TEST
	printf( "detect_nfs_statfs: f_type = %x -> %s\n",
			buf.f_type, *is_nfs ? "true" : "false" );
#  endif

	// #3: Completely unknown
# else
#	error "Neither F_TYPE or F_FSTYP defined!"

# endif

	return 0;
}
#endif

int
fs_detect_nfs( const char *path,
			   bool *is_nfs )
{
#if defined USE_STATFS
	return detect_nfs_statfs( path, is_nfs );
#else
#  if !defined(WIN32)
#	warning "No valid fs type detection"
#  endif
	*is_nfs = false;
	return 0;
#endif

}

#ifdef FS_UTIL_TEST
int
main( int argc, const char *argv[] )
{
	const char *path = argv[1];
	bool	is_nfs;

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

