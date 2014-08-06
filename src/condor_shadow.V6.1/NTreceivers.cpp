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


#include "condor_common.h"
#include "condor_classad.h"
#include "condor_debug.h"
#include "condor_io.h"
#include "condor_constants.h"
#include "pseudo_ops.h"
#include "condor_sys.h"
#include "baseshadow.h"
#include "remoteresource.h"
#include "directory.h"

#if defined(Solaris)
#include <sys/statvfs.h>
#endif

extern ReliSock *syscall_sock;
extern BaseShadow *Shadow;
extern RemoteResource *thisRemoteResource;


static bool read_access(const char * filename ) {
	return thisRemoteResource->allowRemoteReadFileAccess( filename );
}

static bool write_access(const char * filename ) {
	return thisRemoteResource->allowRemoteWriteFileAccess( filename );
}

static int stat_string( char *line, struct stat *info )
{
#ifdef WIN32
	return 0;
#else
	return sprintf(line,"%lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld\n",
		(long long) info->st_dev,
		(long long) info->st_ino,
		(long long) info->st_mode,
		(long long) info->st_nlink,
		(long long) info->st_uid,
		(long long) info->st_gid,
		(long long) info->st_rdev,
		(long long) info->st_size,
		(long long) info->st_blksize,
		(long long) info->st_blocks,
		(long long) info->st_atime,
		(long long) info->st_mtime,
		(long long) info->st_ctime
	);
#endif
}

#if defined(Solaris)
static int statfs_string( char *line, struct statvfs *info )
#else
static int statfs_string( char *line, struct statfs *info )
#endif
{
#ifdef WIN32
	return 0;
#else
	return sprintf(line,"%lld %lld %lld %lld %lld %lld %lld\n",
#  if defined(Solaris)
		(long long) info->f_fsid,
		(long long) info->f_frsize,
#  else
		(long long) info->f_type,
		(long long) info->f_bsize,
#  endif
		(long long) info->f_blocks,
		(long long) info->f_bfree,
		(long long) info->f_bavail,
		(long long) info->f_files,
		(long long) info->f_ffree
	);
#endif
}

static const char * shadow_syscall_name(int condor_sysnum)
{
	switch(condor_sysnum) {
        case CONDOR_register_job_info: return "register_job_info";
        case CONDOR_register_starter_info: return "register_starter_info";
        case CONDOR_get_job_info: return "get_job_info";
        case CONDOR_get_user_info: return "get_user_info";
        case CONDOR_job_exit: return "job_exit";
        case CONDOR_job_termination: return "job_termination";
        case CONDOR_begin_execution: return "begin_execution";
        case CONDOR_open: return "open";
        case CONDOR_close: return "close";
        case CONDOR_read: return "read";
        case CONDOR_write: return "write";
        case CONDOR_lseek: return "lseek";
        case CONDOR_lseek64: return "lseek64";
        case CONDOR_llseek: return "llseek";
        case CONDOR_unlink: return "unlink";
        case CONDOR_rename: return "rename";
        case CONDOR_register_mpi_master_info: return "register_mpi_master_info";
        case CONDOR_mkdir: return "mkdir";
        case CONDOR_rmdir: return "rmdir";
        case CONDOR_fsync: return "fsync";
        case CONDOR_get_file_info_new: return "get_file_info_new";
        case CONDOR_ulog: return "ulog";
        case CONDOR_phase: return "phase";
        case CONDOR_get_job_attr: return "get_job_attr";
        case CONDOR_set_job_attr: return "set_job_attr";
        case CONDOR_constrain: return "constrain";
        case CONDOR_get_sec_session_info: return "get_sec_session_info";
#ifdef WIN32
#else
        case CONDOR_pread: return "pread";
        case CONDOR_pwrite: return "pwrite";
        case CONDOR_sread: return "sread";
        case CONDOR_swrite: return "swrite";
        case CONDOR_rmall: return "rmall";
#endif
        case CONDOR_getfile: return "getfile";
        case CONDOR_putfile: return "putfile";
        case CONDOR_getlongdir: return "getlongdir";
        case CONDOR_getdir: return "getdir";
        case CONDOR_whoami: return "whoami";
        case CONDOR_whoareyou: return "whoareyou";
        case CONDOR_fstat: return "fstat";
        case CONDOR_fstatfs: return "fstatfs";
        case CONDOR_fchown: return "fchown";
        case CONDOR_fchmod: return "fchmod";
        case CONDOR_ftruncate: return "ftruncate";
        case CONDOR_link: return "link";
        case CONDOR_symlink: return "symlink";
        case CONDOR_readlink: return "readlink";
        case CONDOR_stat: return "stat";
        case CONDOR_lstat: return "lstat";
        case CONDOR_statfs: return "statfs";
        case CONDOR_access: return "access";
        case CONDOR_chmod: return "chmod";
		case CONDOR_chown: return "chown";
        case CONDOR_lchown: return "lchown";
        case CONDOR_truncate: return "truncate";
        case CONDOR_utime: return "utime";
	}
	return "unknown";
}

int
do_REMOTE_syscall()
{
	int condor_sysnum;
	int	rval = -1, result = -1, fd = -1, mode = -1, uid = -1, gid = -1;
	int length = -1;
	condor_errno_t terrno = (condor_errno_t)0;
	char *path = NULL, *buffer = NULL;
	void *buf = NULL;

	syscall_sock->decode();

	dprintf(D_SYSCALLS, "About to decode condor_sysnum\n");

	rval = syscall_sock->code(condor_sysnum);
	if (!rval) {
		MyString err_msg;
		err_msg = "Can no longer talk to condor_starter ";
		err_msg += syscall_sock->get_sinful_peer();

            // the socket is closed, there's no way to recover
            // from this.  so, we have to cancel the socket
            // handler in daemoncore and delete the relisock.
		thisRemoteResource->closeClaimSock();

            /* It is possible that we are failing to read the
            syscall number because the starter went away
            because we *asked* it to go away. Don't be shocked
            and surprised if the startd/starter actually did
            what we asked when we deactivated the claim */
       if ( thisRemoteResource->wasClaimDeactivated() ) {
           return -1;
       }

		if( Shadow->supportsReconnect() ) {
				// instead of having to EXCEPT, we can now try to
				// reconnect.  happy day! :)
			dprintf( D_ALWAYS, "%s\n", err_msg.Value() );

			const char* txt = "Socket between submit and execute hosts "
				"closed unexpectedly";
			Shadow->logDisconnectedEvent( txt ); 

			if (!Shadow->shouldAttemptReconnect(thisRemoteResource)) {
					dprintf(D_ALWAYS, "This job cannot reconnect to starter, so job exiting\n");
					Shadow->gracefulShutDown();
					EXCEPT( "%s", err_msg.Value() );
			}
				// tell the shadow to start trying to reconnect
			Shadow->reconnect();
				// we need to return 0 so that our caller doesn't
				// think the job exited and doesn't do anything to the
				// syscall socket.
			return 0;
		} else {
				// The remote starter doesn't support it, so give up
				// like we always used to.
			EXCEPT( "%s", err_msg.Value() );
		}
	}

	dprintf(D_SYSCALLS,
		"Got request for syscall %s (%d)\n",
		shadow_syscall_name(condor_sysnum), condor_sysnum);

	switch( condor_sysnum ) {

	case CONDOR_register_starter_info:
	{
		ClassAd ad;
		result = ( getClassAd(syscall_sock, ad) );
		ASSERT( result );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );

		errno = 0;
		rval = pseudo_register_starter_info( &ad );
		terrno = (condor_errno_t)errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();
		result = ( syscall_sock->code(rval) );
		ASSERT( result );
		if( rval < 0 ) {
			result = ( syscall_sock->code( terrno ) );
			ASSERT( result );
		}
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		return 0;
	}

	case CONDOR_register_job_info:
	{
		ClassAd ad;
		result = ( getClassAd(syscall_sock, ad) );
		ASSERT( result );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );

		errno = 0;
		rval = pseudo_register_job_info( &ad );
		terrno = (condor_errno_t)errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();
		result = ( syscall_sock->code(rval) );
		ASSERT( result );
		if( rval < 0 ) {
			result = ( syscall_sock->code( terrno ) );
			ASSERT( result );
		}
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		return 0;
	}

	case CONDOR_get_job_info:
	{
		ClassAd *ad = NULL;
		bool delete_ad;

		result = ( syscall_sock->end_of_message() );
		ASSERT( result );

		errno = 0;
		rval = pseudo_get_job_info(ad, delete_ad);
		terrno = (condor_errno_t)errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();
		result = ( syscall_sock->code(rval) );
		ASSERT( result );
		if( rval < 0 ) {
			result = ( syscall_sock->code( terrno ) );
			ASSERT( result );
		} else {
			result = ( putClassAd(syscall_sock, *ad) );
			ASSERT( result );
		}
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		if ( delete_ad ) {
			delete ad;
		}
		return 0;
	}


	case CONDOR_get_user_info:
	{
		ClassAd *ad = NULL;

		result = ( syscall_sock->end_of_message() );
		ASSERT( result );

		errno = 0;
		rval = pseudo_get_user_info(ad);
		terrno = (condor_errno_t)errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();
		result = ( syscall_sock->code(rval) );
		ASSERT( result );
		if( rval < 0 ) {
			result = ( syscall_sock->code( terrno ) );
			ASSERT( result );
		} else {
			result = ( putClassAd(syscall_sock, *ad) );
			ASSERT( result );
		}
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		return 0;
	}


	case CONDOR_job_exit:
	{
		int status=0;
		int reason=0;
		ClassAd ad;

		result = ( syscall_sock->code(status) );
		ASSERT( result );
		result = ( syscall_sock->code(reason) );
		ASSERT( result );
		result = ( getClassAd(syscall_sock, ad) );
		ASSERT( result );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );

		errno = 0;
		rval = pseudo_job_exit(status, reason, &ad);
		terrno = (condor_errno_t)errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();
		result = ( syscall_sock->code(rval) );
		ASSERT( result );
		if( rval < 0 ) {
			result = ( syscall_sock->code( terrno ) );
			ASSERT( result );
		}
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		return -1;
	}

	case CONDOR_job_termination:
	{
		ClassAd ad;
		result = ( getClassAd(syscall_sock, ad) );
		ASSERT( result );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );

		errno = 0;
		rval = pseudo_job_termination( &ad );
		terrno = (condor_errno_t)errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();
		result = ( syscall_sock->code(rval) );
		ASSERT( result );
		if( rval < 0 ) {
			result = ( syscall_sock->code( terrno ) );
			ASSERT( result );
		}
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		return 0;
	}

	case CONDOR_begin_execution:
	{
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );

		errno = 0;
		rval = pseudo_begin_execution();
		terrno = (condor_errno_t)errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();
		result = ( syscall_sock->code(rval) );
		ASSERT( result );
		if( rval < 0 ) {
			result = ( syscall_sock->code( terrno ) );
			ASSERT( result );
		}
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		return 0;
	}

	case CONDOR_open:
	  {
		open_flags_t flags;
		int   lastarg;

		result = ( syscall_sock->code(flags) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  flags = %d\n", flags );
		result = ( syscall_sock->code(lastarg) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  lastarg = %d\n", lastarg );
		path = NULL;
		result = ( syscall_sock->code(path) );
		dprintf( D_SYSCALLS, "  path = %s\n", path );
		ASSERT( result );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );


		bool access_ok;
		// O_RDONLY, O_WRONLY, and O_RDWR are an enum, not flags. O_RDONLY==0
		if ( (flags & (O_RDONLY  | O_WRONLY | O_RDWR)) == O_RDONLY ) {
			access_ok = read_access(path);
		} else {
			access_ok = write_access(path);
		}

		errno = 0;
		if ( access_ok ) {
			rval = safe_open_wrapper_follow( path , flags , lastarg);
		} else {
			rval = -1;
			errno = EACCES;
		}
		terrno = (condor_errno_t)errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );
		
		syscall_sock->encode();
		result = ( syscall_sock->code(rval) );
		ASSERT( result );
		if( rval < 0 ) {
			result = ( syscall_sock->code( terrno ) );
			ASSERT( result );
		}

		free( (char *)path );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		return 0;
	}

	case CONDOR_close:
	  {
		result = ( syscall_sock->code(fd) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  fd = %d\n", fd );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );

		errno = 0;
		rval = close( fd);
		terrno = (condor_errno_t)errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();
		result = ( syscall_sock->code(rval) );
		ASSERT( result );
		if( rval < 0 ) {
			result = ( syscall_sock->code( terrno ) );
			ASSERT( result );
		}
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		return 0;
	}
	case CONDOR_read:
	  {
		size_t   len;

		result = ( syscall_sock->code(fd) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  fd = %d\n", fd );
		result = ( syscall_sock->code(len) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  len = %ld\n", (long)len );
		buf = (void *)malloc( (unsigned)len );
		ASSERT( buf );
		memset( buf, 0, (unsigned)len );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );

		errno = 0;
		rval = read( fd , buf , len);
		terrno = (condor_errno_t)errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();
		result = ( syscall_sock->code(rval) );
		ASSERT( result );
		if( rval < 0 ) {
			result = ( syscall_sock->code( terrno ) );
			ASSERT( result );
		}
		if( rval >= 0 ) {
			result = ( syscall_sock->code_bytes_bool(buf, rval) );
			ASSERT( result );
		}
		free( buf );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		return 0;
	}

	case CONDOR_write:
	  {
		size_t   len;

		result = ( syscall_sock->code(fd) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  fd = %d\n", fd );
		result = ( syscall_sock->code(len) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  len = %ld\n", (long)len );
		buf = (void *)malloc( (unsigned)len );
		ASSERT( buf );
		memset( buf, 0, (unsigned)len );
		result = ( syscall_sock->code_bytes_bool(buf, len) );
		ASSERT( result );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );

		errno = 0;
		rval = write( fd , buf , len);
		terrno = (condor_errno_t)errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();
		result = ( syscall_sock->code(rval) );
		ASSERT( result );
		if( rval < 0 ) {
			result = ( syscall_sock->code( terrno ) );
			ASSERT( result );
		}
		free( buf );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		return 0;
	}

	case CONDOR_lseek:
	case CONDOR_lseek64:
	case CONDOR_llseek:
	  {
		off_t   offset;
		int   whence;

		result = ( syscall_sock->code(fd) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  fd = %d\n", fd );
		result = ( syscall_sock->code(offset) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  offset = %ld\n", (long)offset );
		result = ( syscall_sock->code(whence) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  whence = %d\n", whence );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );

		errno = 0;
		rval = lseek( fd , offset , whence);
		terrno = (condor_errno_t)errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();
		result = ( syscall_sock->code(rval) );
		ASSERT( result );
		if( rval < 0 ) {
			result = ( syscall_sock->code( terrno ) );
			ASSERT( result );
		}
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		return 0;
	}

	case CONDOR_unlink:
	  {
		path = NULL;
		result = ( syscall_sock->code(path) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  path = %s\n", path );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );

		if ( write_access(path) ) {
			errno = 0;
			rval = unlink( path);
		} else {
			// no permission to write to this file
			rval = -1;
			errno = EACCES;
		}
		terrno = (condor_errno_t)errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();
		result = ( syscall_sock->code(rval) );
		ASSERT( result );
		if( rval < 0 ) {
			result = ( syscall_sock->code( terrno ) );
			ASSERT( result );
		}
		free( (char *)path );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		return 0;
	}

	case CONDOR_rename:
	  {
		char *  from;
		char *  to;

		to = NULL;
		from = NULL;
		result = ( syscall_sock->code(from) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  from = %s\n", from );
		result = ( syscall_sock->code(to) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  to = %s\n", to );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );

		if ( write_access(from) && write_access(to) ) {
			errno = 0;
			rval = rename( from , to);
		} else {
			rval = -1;
			errno = EACCES;
		}

		terrno = (condor_errno_t)errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();
		result = ( syscall_sock->code(rval) );
		ASSERT( result );
		if( rval < 0 ) {
			result = ( syscall_sock->code( terrno ) );
			ASSERT( result );
		}
		free( (char *)to );
		free( (char *)from );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		return 0;
	}

	case CONDOR_register_mpi_master_info:
	{
		ClassAd ad;
		result = ( getClassAd(syscall_sock, ad) );
		ASSERT( result );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );

		errno = 0;
		rval = pseudo_register_mpi_master_info( &ad );
		terrno = (condor_errno_t)errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();
		result = ( syscall_sock->code(rval) );
		ASSERT( result );
		if( rval < 0 ) {
			result = ( syscall_sock->code( terrno ) );
			ASSERT( result );
		}
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		return 0;
	}

	case CONDOR_mkdir:
	  {
		path = NULL;
		result = ( syscall_sock->code(path) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  path = %s\n", path );
		result = ( syscall_sock->code(mode) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  mode = %d\n", mode );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );

		if ( write_access(path) ) {
			errno = 0;
			rval = mkdir(path,mode);
		} else {
			rval = -1;
			errno = EACCES;
		}
		terrno = (condor_errno_t)errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();
		result = ( syscall_sock->code(rval) );
		ASSERT( result );
		if( rval < 0 ) {
			result = ( syscall_sock->code( terrno ) );
			ASSERT( result );
		}
		free( (char *)path );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		return 0;
	}

	case CONDOR_rmdir:
	  {
		path = NULL;
		result = ( syscall_sock->code(path) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  path = %s\n", path );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );

		if ( write_access(path) ) {
			errno = 0;
			rval = rmdir( path);
		} else {
			rval = -1;
			errno = EACCES;
		}
		terrno = (condor_errno_t)errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();
		result = ( syscall_sock->code(rval) );
		ASSERT( result );
		if( rval < 0 ) {
			result = ( syscall_sock->code( terrno ) );
			ASSERT( result );
		}
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		return 0;
	}

	case CONDOR_fsync:
	  {
		result = ( syscall_sock->code(fd) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  fs = %d\n", fd );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );

		errno = 0;
		rval = fsync(fd);
		terrno = (condor_errno_t)errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();
		result = ( syscall_sock->code(rval) );
		ASSERT( result );
		if( rval < 0 ) {
			result = ( syscall_sock->code( terrno ) );
			ASSERT( result );
		}
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		return 0;
	}

	case CONDOR_get_file_info_new:
	  {
		char *  logical_name;
		char *  actual_url;
 
		actual_url = NULL;
		logical_name = NULL;
		ASSERT( syscall_sock->code(logical_name) );
		ASSERT( syscall_sock->end_of_message() );;
 
		errno = (condor_errno_t)0;
		rval = pseudo_get_file_info_new( logical_name , actual_url );
		terrno = (condor_errno_t)errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, (int)terrno );
 
		syscall_sock->encode();
		ASSERT( syscall_sock->code(rval) );
		if( rval < 0 ) {
			ASSERT( syscall_sock->code(terrno) );
		}
		if( rval >= 0 ) {
			ASSERT( syscall_sock->code(actual_url) );
		}
		free( (char *)actual_url );
		free( (char *)logical_name );
		ASSERT( syscall_sock->end_of_message() );;
		return 0;
	}

	case CONDOR_ulog:
	{
		ClassAd ad;

		result = ( getClassAd(syscall_sock, ad) );
		ASSERT( result );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );

		rval = pseudo_ulog(&ad);
		dprintf( D_SYSCALLS, "\trval = %d\n", rval );

		//NOTE: caller does not expect a response.

		return 0;
	}

	case CONDOR_phase:
	{
		char *phase = 0;
		result = ( syscall_sock->code(phase) );
		ASSERT( result );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );

		rval = pseudo_phase(phase);
		dprintf( D_SYSCALLS, "\trval = %d\n", rval );

		//NOTE: caller does not expect a response.

		return 0;
	}

	case CONDOR_get_job_attr:
	  {
		char *  attrname = 0;

		ASSERT( syscall_sock->code(attrname) );
		ASSERT( syscall_sock->end_of_message() );;

		errno = (condor_errno_t)0;
		MyString expr;
		if ( thisRemoteResource->allowRemoteReadAttributeAccess(attrname) ) {
			rval = pseudo_get_job_attr( attrname , expr);
			terrno = (condor_errno_t)errno;
		} else {
			rval = -1;
			terrno = (condor_errno_t)EACCES;
		}
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, (int)terrno );

		syscall_sock->encode();
		ASSERT( syscall_sock->code(rval) );
		if( rval < 0 ) {
			ASSERT( syscall_sock->code(terrno) );
		}
		if( rval >= 0 ) {
			ASSERT( syscall_sock->put(expr.Value()) );
		}
		free( (char *)attrname );
		ASSERT( syscall_sock->end_of_message() );;
		return 0;
	}

	case CONDOR_set_job_attr:
	  {
		char *  attrname = 0;
		char *  expr = 0;

		ASSERT( syscall_sock->code(expr) );
		ASSERT( syscall_sock->code(attrname) );
		ASSERT( syscall_sock->end_of_message() );;

		errno = (condor_errno_t)0;
		if ( thisRemoteResource->allowRemoteWriteAttributeAccess(attrname) ) {
			rval = pseudo_set_job_attr( attrname , expr , true );
			terrno = (condor_errno_t)errno;
		} else {
			rval = -1;
			terrno = (condor_errno_t)EACCES;
		}
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, (int)terrno );

		syscall_sock->encode();
		ASSERT( syscall_sock->code(rval) );
		if( rval < 0 ) {
			ASSERT( syscall_sock->code(terrno) );
		}
		free( (char *)expr );
		free( (char *)attrname );
		ASSERT( syscall_sock->end_of_message() );;
		return 0;
	}

	case CONDOR_constrain:
	  {
		char *  expr = 0;

		ASSERT( syscall_sock->code(expr) );
		ASSERT( syscall_sock->end_of_message() );;

		errno = (condor_errno_t)0;
		if ( thisRemoteResource->allowRemoteWriteAttributeAccess(ATTR_REQUIREMENTS) ) {
			rval = pseudo_constrain( expr);
			terrno = (condor_errno_t)errno;
		} else {
			rval = -1;
			terrno = (condor_errno_t)EACCES;
		}
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, (int)terrno );

		syscall_sock->encode();
		ASSERT( syscall_sock->code(rval) );
		if( rval < 0 ) {
			ASSERT( syscall_sock->code(terrno) );
		}
		free( (char *)expr );
		ASSERT( syscall_sock->end_of_message() );;
		return 0;
	}
	case CONDOR_get_sec_session_info:
	{
		MyString starter_reconnect_session_info;
		MyString starter_filetrans_session_info;
		MyString reconnect_session_id;
		MyString reconnect_session_info;
		MyString reconnect_session_key;
		MyString filetrans_session_id;
		MyString filetrans_session_info;
		MyString filetrans_session_key;
		bool socket_default_crypto = syscall_sock->get_encryption();
		if( !socket_default_crypto ) {
				// always encrypt; we are exchanging super secret session keys
			syscall_sock->set_crypto_mode(true);
		}
		ASSERT( syscall_sock->code(starter_reconnect_session_info) );
		ASSERT( syscall_sock->code(starter_filetrans_session_info) );
		ASSERT( syscall_sock->end_of_message() );

		errno = (condor_errno_t)0;
		rval = pseudo_get_sec_session_info(
			starter_reconnect_session_info.Value(),
			reconnect_session_id,
			reconnect_session_info,
			reconnect_session_key,
			starter_filetrans_session_info.Value(),
			filetrans_session_id,
			filetrans_session_info,
			filetrans_session_key );
		terrno = (condor_errno_t)errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, (int)terrno );

		syscall_sock->encode();
		ASSERT( syscall_sock->code(rval) );
		if( rval < 0 ) {
			ASSERT( syscall_sock->code(terrno) );
		}
		else {
			ASSERT( syscall_sock->code(reconnect_session_id) );
			ASSERT( syscall_sock->code(reconnect_session_info) );
			ASSERT( syscall_sock->code(reconnect_session_key) );

			ASSERT( syscall_sock->code(filetrans_session_id) );
			ASSERT( syscall_sock->code(filetrans_session_info) );
			ASSERT( syscall_sock->code(filetrans_session_key) );
		}

		ASSERT( syscall_sock->end_of_message() );

		if( !socket_default_crypto ) {
			syscall_sock->set_crypto_mode( false );  // restore default
		}
		return 0;
	}
#ifdef WIN32
#else
	case CONDOR_pread:
	  {
		size_t len, offset;

		result = ( syscall_sock->code(fd) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  fd = %d\n", fd );
		result = ( syscall_sock->code(len) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  len = %ld\n", (long)len );
		result = ( syscall_sock->code(offset) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  offset = %ld\n", (long)offset );
		buf = (void *)malloc( (unsigned)len );
		memset( buf, 0, (unsigned)len );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );

		errno = 0;
		rval = pread( fd , buf , len, offset );
		terrno = (condor_errno_t)errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();
		result = ( syscall_sock->code(rval) );
		ASSERT( result );
		if( rval < 0 ) {
			result = ( syscall_sock->code( terrno ) );
			ASSERT( result );
		}
		else {
			result = ( syscall_sock->code_bytes_bool(buf, rval) );
			ASSERT( result );
		}
		free( buf );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		return 0;
	}
	case CONDOR_pwrite:
	  {
		size_t   len, offset;

		result = ( syscall_sock->code(fd) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  fd = %d\n", fd );
		result = ( syscall_sock->code(len) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  len = %ld\n", (long)len );
		result = ( syscall_sock->code(offset) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  offset = %ld\n", (long)offset);
		buf = malloc( (unsigned)len );
		memset( buf, 0, (unsigned)len );
		result = ( syscall_sock->code_bytes_bool(buf, len) );
		ASSERT( result );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );

		errno = 0;
		rval = pwrite( fd , buf , len, offset);
		terrno = (condor_errno_t)errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();
		result = ( syscall_sock->code(rval) );
		ASSERT( result );
		if( rval < 0 ) {
			result = ( syscall_sock->code( terrno ) );
			ASSERT( result );
		}
		free( buf );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		return 0;
	}
	case CONDOR_sread:
	  {
		size_t   len, offset, stride_length, stride_skip;

		result = ( syscall_sock->code(fd) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  fd = %d\n", fd );
		result = ( syscall_sock->code(len) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  len = %ld\n", (long)len );
		result = ( syscall_sock->code(offset) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  offset = %ld\n", (long)offset );
		result = ( syscall_sock->code(stride_length) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  stride_length = %ld\n", (long)stride_length);
		result = ( syscall_sock->code(stride_skip) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  stride_skip = %ld\n", (long)stride_skip);
		buf = (void *)malloc( (unsigned)len );
		memset( buf, 0, (unsigned)len );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );

		errno = EINVAL;
		rval = -1;
		unsigned int total = 0;
		buffer = (char*)buf;

		while(total < len && stride_length > 0) {
			// For last read (make sure we only read total of 'len' bytes)
			if(len - total < stride_length) {
				stride_length = len - total;
			}
			rval = pread( fd, (void*)&buffer[total], stride_length, offset );
			if(rval >= 0) {
				total += rval;
				offset += stride_skip;
			}
			else {
				break;
			}
		}

		syscall_sock->encode();
		if( rval < 0 ) {
			result = ( syscall_sock->code(rval) );
			ASSERT( result );
			terrno = (condor_errno_t)errno;
			dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );
			result = ( syscall_sock->code( terrno ) );
			ASSERT( result );
		}
		else {
			dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", total, terrno );
			result = ( syscall_sock->code(total) );
			ASSERT( result );
			dprintf( D_ALWAYS, "buffer: %s\n", buffer);
			result = ( syscall_sock->code_bytes_bool(buf, total) );
			ASSERT( result );
		}
		free( buf );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		return 0;
	}
	case CONDOR_swrite:
	  {
		size_t   len, offset, stride_length, stride_skip;

		result = ( syscall_sock->code(fd) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  fd = %d\n", fd );
		result = ( syscall_sock->code(len) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  len = %ld\n", (long)len );
		result = ( syscall_sock->code(offset) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  offset = %ld\n", (long)offset);
		result = ( syscall_sock->code(stride_length) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  stride_length = %ld\n", (long)stride_length);
		result = ( syscall_sock->code(stride_skip) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  stride_skip = %ld\n", (long)stride_skip);
		buf = (void *)malloc( (unsigned)len );
		memset( buf, 0, (unsigned)len );
		result = ( syscall_sock->code_bytes_bool(buf, len) );
		ASSERT( result );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );

		errno = EINVAL;
		rval = -1;
		unsigned int total = 0;
		buffer = (char*)buf;

		while(total < len && stride_length > 0) {
			// For last write (make sure we only write 'len' bytes)
			if(len - total < stride_length) {
				stride_length = len - total;
			}
			rval = pwrite( fd, (void*)&buffer[total], stride_length, offset);
			if(rval >= 0) {
				total += rval;
				offset += stride_skip;
			}
			else {
				break;
			}
		}
		
		syscall_sock->encode();
		if( rval < 0 ) {
			terrno = (condor_errno_t)errno;
			dprintf( D_SYSCALLS, "\trval = %d, errno = %d (%s)\n", rval, terrno, strerror(errno));
			result = ( syscall_sock->code(rval) );
			ASSERT( result );
			result = ( syscall_sock->code( terrno ) );
			ASSERT( result );
		}
		else {
			dprintf( D_SYSCALLS, "\trval = %d, errno = %d (%s)\n", total, terrno, strerror(errno));
			result = ( syscall_sock->code(total) );
			ASSERT( result );
		}
		free( buf );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		return 0;
	}
	case CONDOR_rmall:
	{
		result = ( syscall_sock->code(path) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  path = %s\n", path );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );

		errno = 0;
		if ( write_access(path) ) {
			// Try to rmdir 
			rval = rmdir(path);
			
			// If rmdir failed, try again after removing everthing in directory
			if(rval == -1) {
				Directory dir(path);
				if(dir.Remove_Entire_Directory()) {
					rval = rmdir(path);
				}
			}
		} else {
			rval = -1;
			errno = EACCES;
		}
		terrno = (condor_errno_t)errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();
		result = ( syscall_sock->code(rval) );
		ASSERT( result );
		if( rval < 0 ) {
			result = ( syscall_sock->code( terrno ) );
			ASSERT( result );
		}
		free( (char *)path );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		return 0;
	}
#endif // ! WIN32
case CONDOR_getfile:
	{
		result = ( syscall_sock->code(path) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  path = %s\n", path );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		
		errno = 0;
		fd = safe_open_wrapper_follow( path, O_RDONLY | _O_BINARY );
		if(fd >= 0) {
			struct stat info;
			stat(path, &info);
			length = info.st_size;
			buf = (void *)malloc( (unsigned)length );
			ASSERT( buf );
			memset( buf, 0, (unsigned)length );

			errno = 0;
			rval = read( fd , buf , length);
		} else {
			rval = fd;
		}
		terrno = (condor_errno_t)errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();
		result = ( syscall_sock->code(rval) );
		ASSERT( result );
		if( rval < 0 ) {
			result = ( syscall_sock->code( terrno ) );
			ASSERT( result );
		}
		else {	
			result = ( syscall_sock->code_bytes_bool(buf, rval) );
			ASSERT( result );
		}
		free( (char *)path );
		free( buf );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		return 0;
	}
case CONDOR_putfile:
	{
		result = ( syscall_sock->code(path) );
		ASSERT( result );
		dprintf(D_SYSCALLS, "  path: %s\n", path);
		result = ( syscall_sock->code(mode) );
		ASSERT( result );
		dprintf(D_SYSCALLS, "  mode: %d\n", mode);
		result = ( syscall_sock->code(length) );
		ASSERT( result );
		dprintf(D_SYSCALLS, "  length: %d\n", length);
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		
		errno = 0;
		fd = safe_open_wrapper_follow(path, O_CREAT | O_WRONLY | O_TRUNC | _O_BINARY, mode);
		terrno = (condor_errno_t)errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );
		
		// Need to send reply after file creation
		syscall_sock->encode();
		result = ( syscall_sock->code(fd) );
		ASSERT( result );
		if( fd < 0 ) {
			result = ( syscall_sock->code( terrno ) );
			ASSERT( result );
		}
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		free((char*)path);

        if (length <= 0) return 0;
		
		int num = -1;
		if(fd >= 0) {
			syscall_sock->decode();
			buffer = (char*)malloc( (unsigned)length );
			ASSERT( buffer );
			memset( buffer, 0, (unsigned)length );
			result = ( syscall_sock->code_bytes_bool(buffer, length) );
			ASSERT( result );
			result = ( syscall_sock->end_of_message() );
			ASSERT( result );
			num = write(fd, buffer, length);
		}
		else {
			dprintf(D_SYSCALLS, "Unable to put file %s\n", path);
		}
		close(fd);
		
		syscall_sock->encode();
		result = ( syscall_sock->code(num) );
		ASSERT( result );
		if( num < 0 ) {
			result = ( syscall_sock->code( terrno ) );
			ASSERT( result );
		}
		free((char*)buffer);
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		return 0;
	}
case CONDOR_getlongdir:
	{
		result = ( syscall_sock->code(path) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  path = %s\n", path );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );

		errno = 0;
		rval = -1;
		MyString msg, check;
		const char *next;
		Directory directory(path);
		struct stat stat_buf;
		char line[1024];
		
		// Get directory's contents
		while((next = directory.Next())) {
			dprintf(D_ALWAYS, "next: %s\n", next);
			msg.formatstr_cat("%s\n", next);
			check.formatstr("%s%c%s", path, DIR_DELIM_CHAR, next);
			rval = stat(check.Value(), &stat_buf);
			terrno = (condor_errno_t)errno;
			if(rval == -1) {
				break;
			}
			if(stat_string(line, &stat_buf) < 0) {
				rval = -1;
				break;
			}
			msg.formatstr_cat("%s", line);
		}
		terrno = (condor_errno_t)errno;
		if(msg.Length() > 0) {
			msg.formatstr_cat("\n");	// Needed to signify end of data
			rval = msg.Length();
		}
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();
		result = ( syscall_sock->code(rval) );
		ASSERT( result );
		if( rval < 0 ) {
			result = ( syscall_sock->code( terrno ) );
			ASSERT( result );
		}
		else {
			result = ( syscall_sock->put(msg.Value()) );
			ASSERT( result );
		}
		free((char*)path);
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		return 0;
	}
case CONDOR_getdir:
	{
		result = ( syscall_sock->code(path) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  path = %s\n", path );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );

		errno = 0;
		rval = -1;
		MyString msg;
		const char *next;
		Directory directory(path);

		// Get directory's contents
		while((next = directory.Next())) {
			msg.formatstr_cat("%s", next);
			msg.formatstr_cat("\n");
		}
		terrno = (condor_errno_t)errno;
		if(msg.Length() > 0) {
			msg.formatstr_cat("\n");	// Needed to signify end of data
			rval = msg.Length();
		}
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();
		result = ( syscall_sock->code(rval) );
		ASSERT( result );
		if( rval < 0 ) {
			result = ( syscall_sock->code( terrno ) );
			ASSERT( result );
		}
		else {
			result = ( syscall_sock->put(msg.Value()) );
			ASSERT( result );
		}
		free((char*)path);
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		return 0;
	}
// Return something more useful?
	case CONDOR_whoami:
	{
		result = ( syscall_sock->code(length) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  length = %d\n", length );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		
		errno = 0;
		buffer = (char*)malloc( (unsigned)length );
		ASSERT( buffer );
		int size = 6;
		if(length < size) {
			rval = -1;
			terrno = (condor_errno_t) ENOSPC;
		}
		else {
			rval = sprintf(buffer, "CONDOR");
			terrno = (condor_errno_t) errno;
		}
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );
		
		syscall_sock->encode();
		result = ( syscall_sock->code(rval) );
		ASSERT( result );
		if( rval != size) {
			result = ( syscall_sock->code( terrno ) );
			ASSERT( result );
		}
		else {
			result = ( syscall_sock->code_bytes_bool(buffer, rval));
			ASSERT( result );
		}
		free((char*)buffer);
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		return 0;
	}
// Return something more useful?
	case CONDOR_whoareyou:
	{
		char *host = NULL;

		result = ( syscall_sock->code(host) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  host = %s\n", host );
		result = ( syscall_sock->code(length) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  length = %d\n", length );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );

		errno = 0;
		buffer = (char*)malloc( (unsigned)length );
		ASSERT( buffer );
		int size = 7;
		if(length < size) {
			rval = -1;
			terrno = (condor_errno_t) ENOSPC;
		}
		else {
			rval = sprintf(buffer, "UNKNOWN");
			terrno = (condor_errno_t) errno;
		}
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );
		
		syscall_sock->encode();
		result = ( syscall_sock->code(rval) );
		ASSERT( result );
		if( rval != size) {
			result = ( syscall_sock->code( terrno ) );
			ASSERT( result );
		}
		else {
			result = ( syscall_sock->code_bytes_bool(buffer, rval));
			ASSERT( result );
		}
		free((char*)buffer);
		free((char*)host);
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		return 0;
	}
#ifdef WIN32
#else
	case CONDOR_fstatfs:
	{
		result = ( syscall_sock->code(fd) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  fd = %d\n", fd );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );

		errno = 0;
#if defined(Solaris)
		struct statvfs statfs_buf;
		rval = fstatvfs(fd, &statfs_buf);
#else
		struct statfs statfs_buf;
		rval = fstatfs(fd, &statfs_buf);
#endif
		terrno = (condor_errno_t)errno;
		char line[1024];
		if(rval == 0) {
			if(statfs_string(line, &statfs_buf) < 0) {
				rval = -1;
				terrno = (condor_errno_t)errno;
			}
		}
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();
		result = ( syscall_sock->code(rval) );
		ASSERT( result );
		if( rval < 0 ) {
			result = ( syscall_sock->code( terrno ) );
			ASSERT( result );
		}
		else {
			result = ( syscall_sock->code_bytes_bool(line, 1024) );
			ASSERT( result );
		}
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		return 0;	
	}
	case CONDOR_fchown:
	{
		result = ( syscall_sock->code(fd) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  fd = %d\n", fd );
		result = ( syscall_sock->code(uid) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  uid = %d\n", uid );
		result = ( syscall_sock->code(gid) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  gid = %d\n", gid );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		
		errno = 0;
		rval = fchown(fd, uid, gid);
		terrno = (condor_errno_t)errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );
		
		syscall_sock->encode();
		result = ( syscall_sock->code(rval) );
		ASSERT( result );
		if( rval < 0 ) {
			result = ( syscall_sock->code( terrno ) );
			ASSERT( result );
		}
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		return 0;
	}
	case CONDOR_fchmod:
	{
		result = ( syscall_sock->code(fd) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  fd = %d\n", fd );
		result = ( syscall_sock->code(mode) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  mode = %d\n", mode );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		
		errno = 0;
		rval = fchmod(fd, (mode_t)mode);
		terrno = (condor_errno_t)errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );
		
		syscall_sock->encode();
		result = ( syscall_sock->code(rval) );
		ASSERT( result );
		if(rval < 0) {
			result = ( syscall_sock->code( terrno ) );
			ASSERT( result );
		}
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		return 0;
	}
	case CONDOR_ftruncate:
	{
		result = ( syscall_sock->code(fd) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  fd = %d\n", fd );
		result = ( syscall_sock->code(length) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  length = %d\n", length );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		
		errno = 0;
		rval = ftruncate(fd, length);
		terrno = (condor_errno_t)errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );
		
		syscall_sock->encode();
		result = ( syscall_sock->code(rval) );
		ASSERT( result );
		if(rval < 0) {
			result = ( syscall_sock->code( terrno ) );
			ASSERT( result );
		}
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		return 0;
	}
	
	
	
	
	
	
	case CONDOR_link:
	{
		char *newpath = NULL;

		result = ( syscall_sock->code(path) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  path = %s\n", path );
		result = ( syscall_sock->code(newpath) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  newpath = %s\n", newpath );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		
		errno = 0;
		rval = link(path, newpath);
		terrno = (condor_errno_t)errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );
		
		syscall_sock->encode();
		result = ( syscall_sock->code(rval) );
		ASSERT( result );
		if(rval < 0) {
			result = ( syscall_sock->code( terrno ) );
			ASSERT( result );
		}
		free((char*)path);
		free((char*)newpath);
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		return 0;
	}
	case CONDOR_symlink:
	{
		char *newpath = NULL;

		result = ( syscall_sock->code(path) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  path = %s\n", path );
		result = ( syscall_sock->code(newpath) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  newpath = %s\n", newpath );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		
		errno = 0;
		rval = symlink(path, newpath);
		terrno = (condor_errno_t)errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );
		
		syscall_sock->encode();
		result = ( syscall_sock->code(rval) );
		ASSERT( result );
		if(rval < 0) {
			result = ( syscall_sock->code( terrno ) );
			ASSERT( result );
		}
		free((char*)path);
		free((char*)newpath);
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		return 0;
	}
	case CONDOR_readlink:
	{
		result = ( syscall_sock->code(path) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  path = %s\n", path );
		result = ( syscall_sock->code(length) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  length = %d\n", length );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );

		char *lbuffer = (char*)malloc(length);
		errno = 0;
		rval = readlink(path, lbuffer, length);
		terrno = (condor_errno_t)errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );
		syscall_sock->encode();
		result = ( syscall_sock->code(rval) );
		ASSERT( result );
		if( rval < 0 ) {
			result = ( syscall_sock->code( terrno ) );
			ASSERT( result );
		}
		else {
			result = ( syscall_sock->code_bytes_bool(lbuffer, rval));
			ASSERT( result );
		}
		free(lbuffer);
		free(path);
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		return 0;
	}
	case CONDOR_lstat:
	{
		result = ( syscall_sock->code(path) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  path = %s\n", path );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );

		errno = 0;
		struct stat stat_buf;
		rval = lstat(path, &stat_buf);
		terrno = (condor_errno_t)errno;
		char line[1024];
		if(rval == 0) {
			if(stat_string(line, &stat_buf) < 0) {
				rval = -1;
				terrno = (condor_errno_t)errno;
			}
		}
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );
		
		syscall_sock->encode();
		result = ( syscall_sock->code(rval) );
		ASSERT( result );
		if( rval < 0 ) {
			result = ( syscall_sock->code( terrno ) );
			ASSERT( result );
		}
		else {
			result = ( syscall_sock->code_bytes_bool(line, 1024) );
			ASSERT( result );
		}
		free( (char*)path );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		return 0;
	}
	case CONDOR_statfs:
	{
		result = ( syscall_sock->code(path) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  path = %s\n", path );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		
		errno = 0;
#if defined(Solaris)
		struct statvfs statfs_buf;
		rval = statvfs(path, &statfs_buf);
#else
		struct statfs statfs_buf;
		rval = statfs(path, &statfs_buf);
#endif
		terrno = (condor_errno_t)errno;
		char line[1024];
		if(rval == 0) {
			if(statfs_string(line, &statfs_buf) < 0) {
				rval = -1;
				terrno = (condor_errno_t)errno;
			}
		}
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );
		
		syscall_sock->encode();
		result = ( syscall_sock->code(rval) );
		ASSERT( result );
		if( rval < 0 ) {
			result = ( syscall_sock->code( terrno ) );
			ASSERT( result );
		}
		else {
			result = ( syscall_sock->code_bytes_bool(line, 1024) );
			ASSERT( result );
		}
		free( (char*)path );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		return 0;
	}
	case CONDOR_chown:
	{
		result = ( syscall_sock->code(path) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  path = %s\n", path );
		result = ( syscall_sock->code(uid) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  uid = %d\n", uid );
		result = ( syscall_sock->code(gid) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  gid = %d\n", gid );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		
		errno = 0;
		rval = chown(path, uid, gid);
		terrno = (condor_errno_t)errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );
	
		syscall_sock->encode();
		result = ( syscall_sock->code(rval) );
		ASSERT( result );
		if( rval < 0 ) {
			result = ( syscall_sock->code( terrno ) );
			ASSERT( result );
		}
		free( (char*)path );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		return 0;
	}
	case CONDOR_lchown:
	{
		result = ( syscall_sock->code(path) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  path = %s\n", path );
		result = ( syscall_sock->code(uid) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  uid = %d\n", uid );
		result = ( syscall_sock->code(gid) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  gid = %d\n", gid );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		
		errno = 0;
		rval = lchown(path, uid, gid);
		terrno = (condor_errno_t)errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );
	
		syscall_sock->encode();
		result = ( syscall_sock->code(rval) );
		ASSERT( result );
		if( rval < 0 ) {
			result = ( syscall_sock->code( terrno ) );
			ASSERT( result );
		}
		free( (char*)path );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		return 0;
	}
	case CONDOR_truncate:
	{
		
		result = ( syscall_sock->code(path) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  path = %s\n", path );
		result = ( syscall_sock->code(length) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  length = %d\n", length );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		
		errno = 0;
		// fd needs to be open for writing!
		if ( write_access(path) ) {
			errno = 0;
			rval = truncate(path, length);
		} else {
			rval = -1;
			errno = EACCES;
		}
		terrno = (condor_errno_t)errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();
		result = ( syscall_sock->code(rval) );
		ASSERT( result );
		if(rval < 0) {
			result = ( syscall_sock->code( terrno ) );
			ASSERT( result );
		}
		free( (char*)path );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		return 0;
	}
#endif // ! WIN32

	case CONDOR_fstat:
	{
		result = ( syscall_sock->code(fd) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  fd = %d\n", fd );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );

		errno = 0;
		struct stat stat_buf;
		rval = fstat(fd, &stat_buf);
		terrno = (condor_errno_t)errno;
		char line[1024];
		if(rval == 0) {
			if(stat_string(line, &stat_buf) < 0) {
				rval = -1;
				terrno = (condor_errno_t)errno;
			}
		}
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();
		result = ( syscall_sock->code(rval) );
		ASSERT( result );
		if( rval < 0 ) {
			result = ( syscall_sock->code( terrno ) );
			ASSERT( result );
		}
		else {
			result = ( syscall_sock->code_bytes_bool(line, 1024) );
			ASSERT( result );
		}
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		return 0;	
	}
	case CONDOR_stat:
	{
		result = ( syscall_sock->code(path) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  path = %s\n", path );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );

		errno = 0;
		struct stat stat_buf;
		rval = stat(path, &stat_buf);
		terrno = (condor_errno_t)errno;
		char line[1024];
		if(rval == 0) {
			if(stat_string(line, &stat_buf) < 0) {
				rval = -1;
				terrno = (condor_errno_t)errno;
			}
		}
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();
		result = ( syscall_sock->code(rval) );
		ASSERT( result );
		if( rval < 0 ) {
			result = ( syscall_sock->code( terrno ) );
			ASSERT( result );
		}
		else {
			result = ( syscall_sock->code_bytes_bool(line, 1024) );
			ASSERT( result );
		}
		free( (char*)path );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		return 0;
	}
	case CONDOR_access:
	{
		int flags = -1;

		result = ( syscall_sock->code(path) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  path = %s\n", path );
		result = ( syscall_sock->code(flags) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  flags = %d\n", flags );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		
		errno = 0;
		rval = access(path, flags);
		terrno = (condor_errno_t)errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();
		result = ( syscall_sock->code(rval) );
		ASSERT( result );
		if(rval < 0 ) {
			result = ( syscall_sock->code( terrno ) );
			ASSERT( result );
		}
		free( (char*)path );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		return 0;
	}
	case CONDOR_chmod:
	{
		result = ( syscall_sock->code(path) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  path = %s\n", path );
		result = ( syscall_sock->code(mode) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  mode = %d\n", mode );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		
		errno = 0;
		rval = chmod(path, mode);
		terrno = (condor_errno_t)errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );
		
		syscall_sock->encode();
		result = ( syscall_sock->code(rval) );
		ASSERT( result );
		if(rval < 0) {
			result = ( syscall_sock->code( terrno ) );
			ASSERT( result );
		}
		free( (char*)path );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		return 0;
	}
	case CONDOR_utime:
	{
		time_t actime = -1, modtime = -1;

		result = ( syscall_sock->code(path) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  path = %s\n", path );
		result = ( syscall_sock->code(actime) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  actime = %ld\n", actime );
		result = ( syscall_sock->code(modtime) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  modtime = %ld\n", modtime );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		
		struct utimbuf ut;
		ut.actime = actime;
		ut.modtime = modtime;
		
		errno = 0;
		rval = utime(path, &ut);
		terrno = (condor_errno_t)errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();
		result = ( syscall_sock->code(rval) );
		ASSERT( result );
		if(rval < 0) {
			result = ( syscall_sock->code( terrno ) );
			ASSERT( result );
		}
		free( (char*)path );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		return 0;
	}
	default:
	{
		dprintf(D_ALWAYS, "ERROR: unknown syscall %d received\n", condor_sysnum );
			// If we return failure, the shadow will shutdown, so
			// pretend everything's cool...
		return 0;
		
	}

	}	/* End of switch on system call number */

	return -1;

}	/* End of do_REMOTE_syscall() procedure */
