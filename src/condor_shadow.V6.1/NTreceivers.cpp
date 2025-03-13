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
#include "guidance.h"
#include "pseudo_ops.h"
#include "condor_sys.h"
#include "baseshadow.h"
#include "remoteresource.h"
#include "directory.h"
#include "secure_file.h"
#include "zkm_base64.h"
#include "directory_util.h"
#include "condor_holdcodes.h"


extern ReliSock *syscall_sock;
extern BaseShadow *Shadow;
extern RemoteResource *thisRemoteResource;


static bool read_access(const char * filename ) {
	return thisRemoteResource->allowRemoteReadFileAccess( filename );
}

static bool write_access(const char * filename ) {
	return thisRemoteResource->allowRemoteWriteFileAccess( filename );
}

static int stat_string( char *line, size_t sz, struct stat *info )
{
	return snprintf(line,sz,"%lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld\n",
		(long long) info->st_dev,
		(long long) info->st_ino,
		(long long) info->st_mode,
		(long long) info->st_nlink,
		(long long) info->st_uid,
		(long long) info->st_gid,
		(long long) info->st_rdev,
		(long long) info->st_size,
#ifdef WIN32
		(long long) 0, // use 0 as a placeholder
		(long long) 0, // use 0 as a placeholder
#else
		(long long) info->st_blksize,
		(long long) info->st_blocks,
#endif
		(long long) info->st_atime,
		(long long) info->st_mtime,
		(long long) info->st_ctime
	);
}

static int statfs_string( char *line, size_t sz, struct statfs *info )
{
#ifdef WIN32
	return 0;
#else
	return snprintf(line,sz,"%lld %lld %lld %lld %lld %lld %lld\n",
		(long long) info->f_type,
		(long long) info->f_bsize,
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
		case CONDOR_dprintf_stats: return "dprintf_stats";
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
        case CONDOR_getcreds: return "getcreds";
        case CONDOR_get_delegated_proxy: return "get_delegated_proxy";
        case CONDOR_event_notification: return "event_notification";
        case CONDOR_request_guidance: return "request_guidance";
	}
	return "unknown";
}

// If we fail to send a reply to the starter, assume the socket is borked.
// Close it and go into reconnect mode.
#define ON_ERROR_RETURN(x) if ((x) == 0) {dprintf(D_ERROR, "(%s:%d)  Can no longer talk to starter.\n", __FILE__, __LINE__); thisRemoteResource->disconnectClaimSock("Can no longer talk to condor_starter");return 0;}

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
            /* It is possible that we are failing to read the
            syscall number because the starter went away
            because we *asked* it to go away. Don't be shocked
            and surprised if the startd/starter actually did
            what we asked when we deactivated the claim.
            Or the starter went away by itself after telling us
            it's ready to do so (via a job_exit syscall). */
		if ( thisRemoteResource->wasClaimDeactivated() ||
		     thisRemoteResource->gotJobDone() ) {
			thisRemoteResource->closeClaimSock();
			return -1;
		}

		/* Tell the RemoteResource to close the socket and either
		 * enter reconnect mode or EXCEPT (if reconnect isn't possible).
		 */
		thisRemoteResource->disconnectClaimSock("Can no longer talk to condor_starter");

		return 0;
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
		ON_ERROR_RETURN( result );
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
		ON_ERROR_RETURN( result );
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
		ON_ERROR_RETURN( result );
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
		ON_ERROR_RETURN( result );
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
		if (!result) {
			// This is the last RPC the starter sends before it closes
			// up shop. It looks like closed the connection before we
			// could send a reply. Just close our end of the conneciton.
			thisRemoteResource->closeClaimSock();
		}
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
		ON_ERROR_RETURN( result );
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
		ON_ERROR_RETURN( result );
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
			rval = safe_open_wrapper_follow( path , flags | O_LARGEFILE , lastarg);
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
		ON_ERROR_RETURN( result );
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
		ON_ERROR_RETURN( result );
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
		ON_ERROR_RETURN( result );
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
		rval = full_write( fd , buf , len);
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
		ON_ERROR_RETURN( result );
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
		off_t new_position = lseek( fd , offset , whence);
		terrno = (condor_errno_t)errno;
		dprintf( D_SYSCALLS, "\trval = %ld, errno = %d\n", (long)new_position, terrno );

		syscall_sock->encode();
		result = ( syscall_sock->code(new_position) );
		ASSERT( result );
		if( new_position < 0 ) {
			result = ( syscall_sock->code( terrno ) );
			ASSERT( result );
		}
		result = ( syscall_sock->end_of_message() );
		ON_ERROR_RETURN( result );
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
		ON_ERROR_RETURN( result );
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
		ON_ERROR_RETURN( result );
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
		ON_ERROR_RETURN( result );
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
		ON_ERROR_RETURN( result );
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
		free( path );
		result = ( syscall_sock->end_of_message() );
		ON_ERROR_RETURN( result );
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
		ON_ERROR_RETURN( result );
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
		ON_ERROR_RETURN( syscall_sock->end_of_message() );;
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
		std::string phase;
		result = ( syscall_sock->code(phase) );
		ASSERT( result );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );

		rval = 0;
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
		std::string expr;
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
			ASSERT( syscall_sock->put(expr) );
		}
		free( (char *)attrname );
		ON_ERROR_RETURN( syscall_sock->end_of_message() );;
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
		ON_ERROR_RETURN( syscall_sock->end_of_message() );;
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
		ON_ERROR_RETURN( syscall_sock->end_of_message() );;
		return 0;
	}
	case CONDOR_get_sec_session_info:
	{
		std::string starter_reconnect_session_info;
		std::string starter_filetrans_session_info;
		std::string reconnect_session_id;
		std::string reconnect_session_info;
		std::string reconnect_session_key;
		std::string filetrans_session_id;
		std::string filetrans_session_info;
		std::string filetrans_session_key;
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
			starter_reconnect_session_info.c_str(),
			reconnect_session_id,
			reconnect_session_info,
			reconnect_session_key,
			starter_filetrans_session_info.c_str(),
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

		ON_ERROR_RETURN( syscall_sock->end_of_message() );

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
		ON_ERROR_RETURN( result );
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
		ON_ERROR_RETURN( result );
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
		ON_ERROR_RETURN( result );
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
		ON_ERROR_RETURN( result );
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
		ON_ERROR_RETURN( result );
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
		
		if (read_access(path)) {
			errno = 0;
			fd = safe_open_wrapper_follow(path, O_RDONLY | _O_BINARY);
		}
		else {
			errno = EACCES;
			fd = -1;
		}

		if(fd >= 0) {
			struct stat info;
			int rc = stat(path, &info);
			if (rc >= 0) {
				length = info.st_size;
				buf = (void *)malloc( (unsigned)length );
				ASSERT( buf );
				memset( buf, 0, (unsigned)length );

				errno = 0;
				rval = read( fd , buf , length);
			} else {
				// stat failed.
				rval = rc;
				dprintf(D_ALWAYS, "getfile:: stat of %s failed: %d\n", path, errno);
			}
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
		close(fd);
		result = ( syscall_sock->end_of_message() );
		ON_ERROR_RETURN( result );
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
		
		if (write_access(path)) {
			errno = 0;
			fd = safe_open_wrapper_follow(path, O_CREAT | O_WRONLY | O_TRUNC | _O_BINARY, mode);
		}
		else {
			errno = EACCES;
			fd = -1;
		}
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
		ON_ERROR_RETURN( result );
		free((char*)path);

        if (length <= 0) {
			if (fd >= 0) close(fd);
			return 0;
		}
		
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
		ON_ERROR_RETURN( result );
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
		std::string msg, check;
		const char *next;
		Directory directory(path);
		struct stat stat_buf;
		char line[1024];
		memset( line, 0, sizeof(line) );
		
		// Get directory's contents
		if(directory.Rewind()) {
			rval = 0;
			while((next = directory.Next())) {
				dprintf(D_ALWAYS, "next: %s\n", next);
				msg += next;
				msg += "\n";
				formatstr(check, "%s%c%s", path, DIR_DELIM_CHAR, next);
				rval = stat(check.c_str(), &stat_buf);
				terrno = (condor_errno_t)errno;
				if(rval == -1) {
					break;
				}
				if(stat_string(line, sizeof(line), &stat_buf) < 0) {
					rval = -1;
					break;
				}
				msg += line;
			}
			if(rval >= 0) {
				msg += "\n";	// Needed to signify end of data
				rval = msg.length();
			}
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
			result = ( syscall_sock->put(msg) );
			ASSERT( result );
		}
		free((char*)path);
		result = ( syscall_sock->end_of_message() );
		ON_ERROR_RETURN( result );
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
		std::string msg;
		const char *next;
		Directory directory(path);

		// Get directory's contents
		if(directory.Rewind()) {
			while((next = directory.Next())) {
				msg += next;
				msg += "\n";
			}
			msg += "\n";	// Needed to signify end of data
			rval = (int)msg.length();
		}
		terrno = (condor_errno_t)errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		// NOTE: Older starters treated rval==0 as an error.
		//   Any successful reply should have a value greater than 0
		//   (at least 1 for the terminating "\n").
		syscall_sock->encode();
		result = ( syscall_sock->code(rval) );
		ASSERT( result );
		if( rval < 0 ) {
			result = ( syscall_sock->code( terrno ) );
			ASSERT( result );
		}
		else {
			result = ( syscall_sock->put(msg) );
			ASSERT( result );
		}
		free((char*)path);
		result = ( syscall_sock->end_of_message() );
		ON_ERROR_RETURN( result );
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
			rval = snprintf(buffer, length, "CONDOR");
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
		ON_ERROR_RETURN( result );
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
			rval = snprintf(buffer, length, "UNKNOWN");
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
		ON_ERROR_RETURN( result );
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
		struct statfs statfs_buf;
		rval = fstatfs(fd, &statfs_buf);
		terrno = (condor_errno_t)errno;
		char line[1024];
		memset( line, 0, sizeof(line) );
		if(rval == 0) {
			if(statfs_string(line, sizeof(line), &statfs_buf) < 0) {
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
		ON_ERROR_RETURN( result );
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
		ON_ERROR_RETURN( result );
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
		ON_ERROR_RETURN( result );
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
		ON_ERROR_RETURN( result );
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
		ON_ERROR_RETURN( result );
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
		ON_ERROR_RETURN( result );
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
		ON_ERROR_RETURN( result );
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
		memset( line, 0, sizeof(line) );
		if(rval == 0) {
			if(stat_string(line, sizeof(line), &stat_buf) < 0) {
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
		ON_ERROR_RETURN( result );
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
		struct statfs statfs_buf;
		rval = statfs(path, &statfs_buf);
		terrno = (condor_errno_t)errno;
		char line[1024];
		memset( line, 0, sizeof(line) );
		if(rval == 0) {
			if(statfs_string(line, sizeof(line), &statfs_buf) < 0) {
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
		ON_ERROR_RETURN( result );
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
		ON_ERROR_RETURN( result );
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
		ON_ERROR_RETURN( result );
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
		ON_ERROR_RETURN( result );
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
		memset( line, 0, sizeof(line) );
		if(rval == 0) {
			if(stat_string(line, sizeof(line), &stat_buf) < 0) {
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
		ON_ERROR_RETURN( result );
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
		memset( line, 0, sizeof(line) );
		if(rval == 0) {
			if(stat_string(line, sizeof(line), &stat_buf) < 0) {
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
		ON_ERROR_RETURN( result );
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
		ON_ERROR_RETURN( result );
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
		ON_ERROR_RETURN( result );
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
		ON_ERROR_RETURN( result );
		return 0;
	}
	case CONDOR_dprintf_stats:
	{
		char *message = NULL;

		result = ( syscall_sock->code(message) );
		ASSERT( result );

		result = ( syscall_sock->end_of_message() );
		ASSERT( result );

		dprintf(D_STATS, "%s", message);
		rval = 0; // don't check return value from dprintf
		free(message);

		syscall_sock->encode();
		result = ( syscall_sock->code(rval) );
		ASSERT( result );


		result = ( syscall_sock->end_of_message() );
		ON_ERROR_RETURN( result );
		return 0;
	}
	case CONDOR_getcreds:
	{
		dprintf(D_SECURITY, "ENTERING getcreds syscall\n");

		// read string.  ignored for now, just present for future compatibility.
		result = ( syscall_sock->code(path) );
		ASSERT( result );
		dprintf( D_SECURITY|D_FULLDEBUG, "  path = %s\n", path );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );

		// send response
		syscall_sock->encode();

		std::string user;
		int cluster_id, proc_id;
		ClassAd* ad;
		pseudo_get_job_ad(ad);
		ad->LookupInteger("ClusterId", cluster_id);
		ad->LookupInteger("ProcId", proc_id);
		ad->LookupString("Owner", user);

		bool trust_cred_dir = param_boolean("TRUST_CREDENTIAL_DIRECTORY", false);

		auto_free_ptr cred_dir(param("SEC_CREDENTIAL_DIRECTORY_OAUTH"));
		if (!cred_dir) {
			dprintf(D_ALWAYS, "ERROR: CONDOR_getcreds doesn't have SEC_CREDENTIAL_DIRECTORY_OAUTH defined.\n");
			result = ( syscall_sock->put(-1) );
			ASSERT( result );
			result = ( syscall_sock->end_of_message() );
			ASSERT( result );
			Shadow->holdJob("Job credentials are not available", CONDOR_HOLD_CODE::CorruptedCredential, 0);
			return -1;
		}
		std::string cred_dir_name;
		dircat(cred_dir, user.c_str(), cred_dir_name);

		// CRUFT Older starters expect the service name to be the
		// filename that should be written. Depending on the version,
		// this may include appending '.use' and replacing '*' with '_'.
		bool service_add_use = true;
		bool service_sub_star = true;
		const ClassAd* starter_ad = thisRemoteResource->getStarterAd();
		std::string starter_ver;
		if (starter_ad && starter_ad->LookupString(ATTR_VERSION, starter_ver)) {
			CondorVersionInfo cvi(starter_ver.c_str());
			if (cvi.built_since_version(10, 5, 0)) {
				service_sub_star = false;
			}
			if (cvi.built_since_version(10, 8, 0)) {
				service_add_use = false;
			}
		}

		// what we want to do is send only the ".use" creds, and only
		// the ones required for this job.  we will need to get that
		// list of names from the Job Ad.
		std::string services_needed;
		ad->LookupString(ATTR_OAUTH_SERVICES_NEEDED, services_needed);
		dprintf( D_SECURITY, "CONDOR_getcreds: for job ID %i.%i sending OAuth creds from %s for services %s\n", cluster_id, proc_id, cred_dir_name.c_str(), services_needed.c_str());

		bool had_error = false;
		for (auto& curr: StringTokenIterator(services_needed)) {
			std::string service_name = curr;

			if (service_add_use) {
				service_name += ".use";
			}
			if (service_sub_star) {
				replace_str(service_name, "*", "_");
			}

			std::string fname,fullname;
			formatstr(fname, "%s.use", curr.c_str());

			// change the '*' to an '_'.  These are stored that way
			// so that the original service name can be cleanly
			// separate if needed.  we don't care, so just change
			// them all up front.
			replace_str(fname, "*", "_");

			formatstr(fullname, "%s%c%s", cred_dir_name.c_str(), DIR_DELIM_CHAR, fname.c_str());

			dprintf(D_SECURITY, "CONDOR_getcreds: sending %s (from service name %s).\n", fullname.c_str(), curr.c_str());
			// read the file (fourth argument "true" means as_root)
			unsigned char *buf = 0;
			size_t len = 0;
			const bool as_root = true;
			const int verify_mode = trust_cred_dir ? 0 : SECURE_FILE_VERIFY_ALL;
			bool rc = read_secure_file(fullname.c_str(), (void**)(&buf), &len, as_root, verify_mode);
			if(!rc) {
				dprintf( D_ALWAYS, "CONDOR_getcreds: ERROR reading contents of %s\n", fullname.c_str() );
				had_error = true;
				break;
			}
			std::string b64 = Base64::zkm_base64_encode(buf, len);
			free(buf);

			ClassAd ad;
			ad.Assign("Service", service_name);
			ad.Assign("Data", b64);

			int more_ads = 1;
			result = ( syscall_sock->code(more_ads) );
			ASSERT( result );
			result = ( putClassAd(syscall_sock, ad) );
			ASSERT( result );
			if (param_boolean("SEC_DEBUG_PRINT_KEYS", false)) {
				dprintf( D_SECURITY|D_FULLDEBUG, "CONDOR_getcreds: sent ad:\n" );
				dPrintAd(D_SECURITY|D_FULLDEBUG, ad);
			}
		}

		int last_command = 0;
		if (had_error) {
			Shadow->holdJob("Job credentials are not available", CONDOR_HOLD_CODE::CorruptedCredential, 0);
			last_command = -1;
		}

		// transmit our success or failure
		dprintf( D_SECURITY|D_FULLDEBUG, "CONDOR_getcreds: finishing send with value %i\n", last_command );

		result = ( syscall_sock->code(last_command) );
		ASSERT( result );
		result = ( syscall_sock->end_of_message() );
		ON_ERROR_RETURN( result );

		// return our success or failure
		return last_command;
	}

	case CONDOR_get_delegated_proxy:
	{
		dprintf( D_SECURITY, "ENTERING CONDOR_get_delegated_proxy syscall\n" );

		std::string rpc_proxy_source_path;
		ClassAd* job_ad;
		filesize_t bytes;
		std::string job_ad_proxy_source_path;
		time_t job_ad_proxy_expiration;
		time_t rpc_proxy_expiration;
		time_t proxy_expiration;

		pseudo_get_job_ad( job_ad );

		// Read proxy file location from the job ad
		job_ad->LookupString( ATTR_X509_USER_PROXY, job_ad_proxy_source_path );
		dprintf( D_SECURITY|D_FULLDEBUG, "CONDOR_get_delegated_proxy: job_ad_proxy_source_path = %s\n", job_ad_proxy_source_path.c_str() );

		// Read path to proxy file via RPC, but ignore it for now
		// TODO: Compare this to the path read in from the job ad. Return failure if they are different.
		result = ( syscall_sock->code( rpc_proxy_source_path ) );
		ASSERT( result );

		// Proxy expiration time is the lesser value of: expiration time passed 
		// via the RPC call, and the time returned from the job ad
		result = ( syscall_sock->code( rpc_proxy_expiration ) );
		ASSERT( result );
		job_ad_proxy_expiration = GetDesiredDelegatedJobCredentialExpiration( job_ad );
		proxy_expiration = ( job_ad_proxy_expiration < rpc_proxy_expiration ) ? job_ad_proxy_expiration : rpc_proxy_expiration;
		dprintf( D_SECURITY|D_FULLDEBUG, "CONDOR_get_delegated_proxy: proxy_expiration = %lu\n", proxy_expiration );

		// Wait for the end_of_message signal
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );

		// Switch to send/write mode and call the globus x509 delegation
		syscall_sock->encode();
		int put_x509_rc = syscall_sock->put_x509_delegation( &bytes, job_ad_proxy_source_path.c_str(), proxy_expiration, NULL );
		dprintf( D_SECURITY|D_FULLDEBUG, "CONDOR_get_delegated_proxy: finishing send with value %i\n", put_x509_rc );

		// End of message, cleanup and return
		result = ( syscall_sock->end_of_message() );
		ON_ERROR_RETURN( result );
		return put_x509_rc;
	}

	case CONDOR_event_notification:
	{
		ClassAd eventAd;
		result = getClassAd(syscall_sock, eventAd);
		ASSERT(result);
		result = syscall_sock->end_of_message();
		ASSERT(result);

		errno = 0;
		rval = pseudo_event_notification(eventAd);
		terrno = (condor_errno_t)errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();
		result = syscall_sock->code(rval);
		ASSERT( result );
		result = syscall_sock->end_of_message();
		ON_ERROR_RETURN( result );

		return 0;
	}

	case CONDOR_request_guidance:
	{
		ClassAd requestAd;
		result = getClassAd(syscall_sock, requestAd);
		ASSERT(result);
		result = syscall_sock->end_of_message();
		ASSERT(result);

		errno = 0;
		ClassAd guidanceAd;
		rval = static_cast<int>(pseudo_request_guidance(requestAd, guidanceAd));
		terrno = (condor_errno_t)errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();
		result = syscall_sock->code(rval);
		ASSERT( result );
		result = putClassAd(syscall_sock, guidanceAd);
		ASSERT( result );
		result = syscall_sock->end_of_message();
		ON_ERROR_RETURN( result );

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
