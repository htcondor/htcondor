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
#include "MyString.h"


extern ReliSock *syscall_sock;
extern BaseShadow *Shadow;
extern RemoteResource *thisRemoteResource;


	// Some stub functions. In days of old, we would fire up
	// a perm object on windows, but since the shadow runs as
	// the user now, we don't need to do that stuff.
static void initialize_perm_checks() { return; }
static bool read_access(const char * /* filename */) { return true; }
static bool write_access(const char * /* filename */) { return true; }

static const char * shadow_syscall_name(int condor_sysnum)
{
	switch(condor_sysnum) {
        case CONDOR_register_job_info: return "register_job_info";
        case CONDOR_register_machine_info: return "register_machine_info";
        case CONDOR_register_starter_info: return "register_starter_info";
        case CONDOR_get_job_info: return "get_job_info";
        case CONDOR_get_user_info: return "get_user_info";
        case CONDOR_job_exit: return "job_exit";
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
        case CONDOR_get_job_attr: return "get_job_attr";
        case CONDOR_set_job_attr: return "set_job_attr";
        case CONDOR_constrain: return "constrain";
        case CONDOR_get_sec_session_info: return "get_sec_session_info";
	}
	return "unknown";
}

int
do_REMOTE_syscall()
{
	int condor_sysnum;
	int	rval;
	condor_errno_t terrno;
	int result = 0;

	initialize_perm_checks();

	syscall_sock->decode();

	dprintf(D_SYSCALLS, "About to decode condor_sysnum\n");

	rval = syscall_sock->code(condor_sysnum);
	if (!rval) {
		MyString err_msg;
		err_msg = "Can no longer talk to condor_starter <";
		err_msg += syscall_sock->peer_ip_str();
		err_msg += ':';
		err_msg += syscall_sock->peer_port();
		err_msg += '>';
		if( Shadow->supportsReconnect() ) {
				// instead of having to EXCEPT, we can now try to
				// reconnect.  happy day! :)
			dprintf( D_ALWAYS, "%s\n", err_msg.Value() );
				// the socket is closed, there's no way to recover
				// from this.  so, we have to cancel the socket
				// handler in daemoncore and delete the relisock.
			thisRemoteResource->closeClaimSock();

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

	case CONDOR_register_machine_info:
	{
		char *uiddomain = NULL;
		char *fsdomain = NULL;
		char *address = NULL;
		char *fullHostname = NULL;
		int key = -1;

		
		result = ( syscall_sock->code(uiddomain) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  uiddomain = %s\n", uiddomain);

		result = ( syscall_sock->code(fsdomain) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  fsdomain = %s\n", fsdomain);

		result = ( syscall_sock->code(address) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  address = %s\n", address);

		result = ( syscall_sock->code(fullHostname) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  fullHostname = %s\n", fullHostname );

		result = ( syscall_sock->code(key) );
		ASSERT( result );
			// key is never used, so don't bother printing it.  we
			// just have to read it off the wire for compatibility.
			// newer versions of the starter don't even use this RSC,
			// so they don't send it...
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		errno = 0;
		rval = pseudo_register_machine_info(uiddomain, fsdomain, 
											address, fullHostname);
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
		free(uiddomain);
		free(fsdomain);
		free(address);
		free(fullHostname);
		return 0;
	}

	case CONDOR_register_starter_info:
	{
		ClassAd ad;
		result = ( ad.initFromStream(*syscall_sock) );
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
		result = ( ad.initFromStream(*syscall_sock) );
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

		result = ( syscall_sock->end_of_message() );
		ASSERT( result );

		errno = 0;
		rval = pseudo_get_job_info(ad);
		terrno = (condor_errno_t)errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();
		result = ( syscall_sock->code(rval) );
		ASSERT( result );
		if( rval < 0 ) {
			result = ( syscall_sock->code( terrno ) );
			ASSERT( result );
		} else {
			result = ( ad->put(*syscall_sock) );
			ASSERT( result );
		}
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
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
			result = ( ad->put(*syscall_sock) );
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
		result = ( ad.initFromStream(*syscall_sock) );
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
		char *  path;
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
		ASSERT( result );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );


		bool access_ok;
		if ( flags & O_RDONLY ) {
			access_ok = read_access(path);
		} else {
			access_ok = write_access(path);
		}

		errno = 0;
		if ( access_ok ) {
			rval = safe_open_wrapper( path , flags , lastarg);
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
		int   fd;

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
		int   fd;
		void *  buf;
		size_t   len;

		result = ( syscall_sock->code(fd) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  fd = %d\n", fd );
		result = ( syscall_sock->code(len) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  len = %ld\n", (long)len );
		buf = (void *)malloc( (unsigned)len );
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
		free( (char *)buf );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		return 0;
	}

	case CONDOR_write:
	  {
		int   fd;
		void *  buf;
		size_t   len;

		result = ( syscall_sock->code(fd) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  fd = %d\n", fd );
		result = ( syscall_sock->code(len) );
		ASSERT( result );
		dprintf( D_SYSCALLS, "  len = %ld\n", (long)len );
		buf = (void *)malloc( (unsigned)len );
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
		free( (char *)buf );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		return 0;
	}

	case CONDOR_lseek:
	case CONDOR_lseek64:
	case CONDOR_llseek:
	  {
		int   fd;
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
		char *  path;

		path = NULL;
		result = ( syscall_sock->code(path) );
		ASSERT( result );
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
		result = ( syscall_sock->code(to) );
		ASSERT( result );
		result = ( syscall_sock->code(from) );
		ASSERT( result );
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
		result = ( ad.initFromStream(*syscall_sock) );
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
		char *  path;
		int mode;

		path = NULL;
		result = ( syscall_sock->code(path) );
		ASSERT( result );
		result = ( syscall_sock->code(mode) );
		ASSERT( result );
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
		char *  path;

		path = NULL;
		result = ( syscall_sock->code(path) );
		ASSERT( result );
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
		free( (char *)path );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		return 0;
	}

	case CONDOR_fsync:
	  {
		int fd;

		result = ( syscall_sock->code(fd) );
		ASSERT( result );
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

		result = ( ad.initFromStream(*syscall_sock) );
		ASSERT( result );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );

		rval = pseudo_ulog(&ad);
		dprintf( D_SYSCALLS, "\trval = %d\n", rval );

		//NOTE: caller does not expect a response.

		return 0;
	}

	case CONDOR_get_job_attr:
	  {
		char *  attrname = 0;

		assert( syscall_sock->code(attrname) );
		assert( syscall_sock->end_of_message() );;

		errno = (condor_errno_t)0;
		MyString expr;
		rval = pseudo_get_job_attr( attrname , expr);
		terrno = (condor_errno_t)errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, (int)terrno );

		syscall_sock->encode();
		assert( syscall_sock->code(rval) );
		if( rval < 0 ) {
			assert( syscall_sock->code(terrno) );
		}
		if( rval >= 0 ) {
			assert( syscall_sock->put(expr.Value()) );
		}
		free( (char *)attrname );
		assert( syscall_sock->end_of_message() );;
		return 0;
	}

	case CONDOR_set_job_attr:
	  {
		char *  attrname = 0;
		char *  expr = 0;

		assert( syscall_sock->code(expr) );
		assert( syscall_sock->code(attrname) );
		assert( syscall_sock->end_of_message() );;

		errno = (condor_errno_t)0;
		rval = pseudo_set_job_attr( attrname , expr);
		terrno = (condor_errno_t)errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, (int)terrno );

		syscall_sock->encode();
		assert( syscall_sock->code(rval) );
		if( rval < 0 ) {
			assert( syscall_sock->code(terrno) );
		}
		free( (char *)expr );
		free( (char *)attrname );
		assert( syscall_sock->end_of_message() );;
		return 0;
	}

	case CONDOR_constrain:
	  {
		char *  expr = 0;

		assert( syscall_sock->code(expr) );
		assert( syscall_sock->end_of_message() );;

		errno = (condor_errno_t)0;
		rval = pseudo_constrain( expr);
		terrno = (condor_errno_t)errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, (int)terrno );

		syscall_sock->encode();
		assert( syscall_sock->code(rval) );
		if( rval < 0 ) {
			assert( syscall_sock->code(terrno) );
		}
		free( (char *)expr );
		assert( syscall_sock->end_of_message() );;
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
		assert( syscall_sock->code(starter_reconnect_session_info) );
		assert( syscall_sock->code(starter_filetrans_session_info) );
		assert( syscall_sock->end_of_message() );

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
		assert( syscall_sock->code(rval) );
		if( rval < 0 ) {
			assert( syscall_sock->code(terrno) );
		}
		else {
			assert( syscall_sock->code(reconnect_session_id) );
			assert( syscall_sock->code(reconnect_session_info) );
			assert( syscall_sock->code(reconnect_session_key) );

			assert( syscall_sock->code(filetrans_session_id) );
			assert( syscall_sock->code(filetrans_session_info) );
			assert( syscall_sock->code(filetrans_session_key) );
		}

		assert( syscall_sock->end_of_message() );

		if( !socket_default_crypto ) {
			syscall_sock->set_crypto_mode( false );  // restore default
		}
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
