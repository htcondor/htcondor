/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "condor_classad.h"
#include "condor_debug.h"
#include "condor_io.h"
#include "condor_constants.h"
#include "pseudo_ops.h"
#include "condor_sys.h"


extern ReliSock *syscall_sock;


#ifdef WIN32
	// This block of code is just temporary for Win32, and should be removed
	// once the shadow runs as the user on Win32.
	// These functions check to see if the owner has permission to read or
	// write a given file.  They are all just stub functions on Unix, because
	// on Unix the Shadow already runs as the user, so it can just try to 
	// do a read or write directly and let the OS tell us if it fails.
	// -Todd Tannenbaum, 1/02
	#include "perm.h"
	#include "condor_attributes.h"
	static perm * perm_obj = NULL;

	static 
	void
	initialize_perm_checks()
	{
		ClassAd *Ad = NULL;

		pseudo_get_job_info(Ad);
		if ( !Ad ) {
			// failure
			return;
		}

		char owner[150];
		if (Ad->LookupString(ATTR_OWNER, owner) != 1) {
			// no owner specified in ad
			return;		
		}
		// lookup the domain
		char ntdomain[80];
		char *p_ntdomain = ntdomain;
		if (Ad->LookupString(ATTR_NT_DOMAIN, ntdomain) != 1) {
			// no nt domain specified in the ad; assume local account
			p_ntdomain = NULL;
		}
		perm_obj = new perm();
		if ( !perm_obj->init(owner,p_ntdomain) ) {
			// could not find the owner on this system; perm object
			// already did a dprintf so we don't have to.
			delete perm_obj;
			perm_obj = NULL;
			return;
		} 
	}

	static 
	bool
	read_access(const char *filename)
	{
		bool result = true;

		if ( !perm_obj || (perm_obj->read_access(filename) != 1) ) {
			// we do _not_ have permission to read this file!!
			dprintf(D_ALWAYS,
				"Permission denied to read file %s\n",filename);
			result = false;
		}

		return result;
	}

	static 
	bool 
	write_access(const char *filename)
	{
		bool result = true;;

		// check for write permission on this file
		if ( !perm_obj || (perm_obj->write_access(filename) != 1) ) {
			// we do _not_ have permission to write this file!!
			dprintf(D_ALWAYS,"Permission denied to write file %s!\n",
					filename);
			result = false;
		}

		return result;
	}
#else  
	// Some stub functions on Unix.  See comment above.
	static void initialize_perm_checks() { return; }
	static bool read_access(const char *filename) { return true; }
	static bool write_access(const char *filename) { return true; }
#endif 


int
do_REMOTE_syscall()
{
	int condor_sysnum;
	int	rval;
	condor_errno_t terrno;

	initialize_perm_checks();

	syscall_sock->decode();

	rval = syscall_sock->code(condor_sysnum);
	if (!rval) {
	 EXCEPT("Can no longer communicate with condor_starter on execute machine");
	}

	dprintf(D_SYSCALLS,
		"Got request for syscall %d\n",
		condor_sysnum
		// , _condor_syscall_name(condor_sysnum)
	);

	switch( condor_sysnum ) {

	case CONDOR_register_machine_info:
	{
		char *uiddomain = NULL;
		char *fsdomain = NULL;
		char *address = NULL;
		char *fullHostname = NULL;
		int key = -1;

		assert( syscall_sock->code(uiddomain) );
		dprintf( D_SYSCALLS, "  uiddomain = %s\n", uiddomain);
		assert( syscall_sock->code(fsdomain) );
		dprintf( D_SYSCALLS, "  fsdomain = %s\n", fsdomain);
		assert( syscall_sock->code(address) );
		dprintf( D_SYSCALLS, "  address = %s\n", address);
		assert( syscall_sock->code(fullHostname) );
		dprintf( D_SYSCALLS, "  fullHostname = %s\n", fullHostname );
		assert( syscall_sock->code(key) );
			// key is never used, so don't bother printing it.  we
			// just have to read it off the wire for compatibility.
			// newer versions of the starter don't even use this RSC,
			// so they don't send it...
		assert( syscall_sock->end_of_message() );

		errno = 0;
		rval = pseudo_register_machine_info(uiddomain, fsdomain, 
											address, fullHostname);
		terrno = (condor_errno_t)errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();
		assert( syscall_sock->code(rval) );
		if( rval < 0 ) {
			assert( syscall_sock->code( terrno ) );
		}
		assert( syscall_sock->end_of_message() );
		free(uiddomain);
		free(fsdomain);
		free(address);
		free(fullHostname);
		return 0;
	}

	case CONDOR_register_starter_info:
	{
		ClassAd ad;
		assert( ad.initFromStream(*syscall_sock) );
		assert( syscall_sock->end_of_message() );

		errno = 0;
		rval = pseudo_register_starter_info( &ad );
		terrno = (condor_errno_t)errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();
		assert( syscall_sock->code(rval) );
		if( rval < 0 ) {
			assert( syscall_sock->code( terrno ) );
		}
		assert( syscall_sock->end_of_message() );
		return 0;
	}

	case CONDOR_get_job_info:
	{
		ClassAd *ad = NULL;

		assert( syscall_sock->end_of_message() );

		errno = 0;
		rval = pseudo_get_job_info(ad);
		terrno = (condor_errno_t)errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();
		assert( syscall_sock->code(rval) );
		if( rval < 0 ) {
			assert( syscall_sock->code( terrno ) );
		} else {
			assert( ad->put(*syscall_sock) );
		}
		assert( syscall_sock->end_of_message() );
		return 0;
	}


	case CONDOR_get_user_info:
	{
		ClassAd *ad = NULL;

		assert( syscall_sock->end_of_message() );

		errno = 0;
		rval = pseudo_get_user_info(ad);
		terrno = (condor_errno_t)errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();
		assert( syscall_sock->code(rval) );
		if( rval < 0 ) {
			assert( syscall_sock->code( terrno ) );
		} else {
			assert( ad->put(*syscall_sock) );
		}
		assert( syscall_sock->end_of_message() );
		return 0;
	}


	case CONDOR_job_exit:
	{
		int status=0;
		int reason=0;
		ClassAd ad;

		assert( syscall_sock->code(status) );
		assert( syscall_sock->code(reason) );
		assert( ad.initFromStream(*syscall_sock) );
		assert( syscall_sock->end_of_message() );

		errno = 0;
		rval = pseudo_job_exit(status, reason, &ad);
		terrno = (condor_errno_t)errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();
		assert( syscall_sock->code(rval) );
		if( rval < 0 ) {
			assert( syscall_sock->code( terrno ) );
		}
		assert( syscall_sock->end_of_message() );
		return -1;
	}

	case CONDOR_begin_execution:
	{
		assert( syscall_sock->end_of_message() );

		errno = 0;
		rval = pseudo_begin_execution();
		terrno = (condor_errno_t)errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();
		assert( syscall_sock->code(rval) );
		if( rval < 0 ) {
			assert( syscall_sock->code( terrno ) );
		}
		assert( syscall_sock->end_of_message() );
		return 0;
	}

	case CONDOR_open:
	  {
		char *  path;
		open_flags_t flags;
		int   lastarg;

		assert( syscall_sock->code(flags) );
		dprintf( D_SYSCALLS, "  flags = %d\n", flags );
		assert( syscall_sock->code(lastarg) );
		dprintf( D_SYSCALLS, "  lastarg = %d\n", lastarg );
		path = (char *)malloc( (unsigned)_POSIX_PATH_MAX );
		memset( path, 0, (unsigned)_POSIX_PATH_MAX );
		assert( syscall_sock->code(path) );
		assert( syscall_sock->end_of_message() );;


		bool access_ok;
		if ( flags & O_RDONLY ) {
			access_ok = read_access(path);
		} else {
			access_ok = write_access(path);
		}

		errno = 0;
		if ( access_ok ) {
			rval = open( path , flags , lastarg);
		} else {
			rval = -1;
			errno = EACCES;
		}
		terrno = (condor_errno_t)errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();
		assert( syscall_sock->code(rval) );
		if( rval < 0 ) {
			assert( syscall_sock->code( terrno ) );
		}
		free( (char *)path );
		assert( syscall_sock->end_of_message() );;
		return 0;
	}

	case CONDOR_close:
	  {
		int   fd;

		assert( syscall_sock->code(fd) );
		dprintf( D_SYSCALLS, "  fd = %d\n", fd );
		assert( syscall_sock->end_of_message() );;

		errno = 0;
		rval = close( fd);
		terrno = (condor_errno_t)errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();
		assert( syscall_sock->code(rval) );
		if( rval < 0 ) {
			assert( syscall_sock->code( terrno ) );
		}
		assert( syscall_sock->end_of_message() );;
		return 0;
	}
	case CONDOR_read:
	  {
		int   fd;
		void *  buf;
		size_t   len;

		assert( syscall_sock->code(fd) );
		dprintf( D_SYSCALLS, "  fd = %d\n", fd );
		assert( syscall_sock->code(len) );
		dprintf( D_SYSCALLS, "  len = %d\n", len );
		buf = (void *)malloc( (unsigned)len );
		memset( buf, 0, (unsigned)len );
		assert( syscall_sock->end_of_message() );;

		errno = 0;
		rval = read( fd , buf , len);
		terrno = (condor_errno_t)errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();
		assert( syscall_sock->code(rval) );
		if( rval < 0 ) {
			assert( syscall_sock->code( terrno ) );
		}
		if( rval >= 0 ) {
			assert( syscall_sock->code_bytes_bool(buf, rval) );
		}
		free( (char *)buf );
		assert( syscall_sock->end_of_message() );;
		return 0;
	}

	case CONDOR_write:
	  {
		int   fd;
		void *  buf;
		size_t   len;

		assert( syscall_sock->code(fd) );
		dprintf( D_SYSCALLS, "  fd = %d\n", fd );
		assert( syscall_sock->code(len) );
		dprintf( D_SYSCALLS, "  len = %d\n", len );
		buf = (void *)malloc( (unsigned)len );
		memset( buf, 0, (unsigned)len );
		assert( syscall_sock->code_bytes_bool(buf, len) );
		assert( syscall_sock->end_of_message() );;

		errno = 0;
		rval = write( fd , buf , len);
		terrno = (condor_errno_t)errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();
		assert( syscall_sock->code(rval) );
		if( rval < 0 ) {
			assert( syscall_sock->code( terrno ) );
		}
		free( (char *)buf );
		assert( syscall_sock->end_of_message() );;
		return 0;
	}

	case CONDOR_lseek:
	case CONDOR_lseek64:
	case CONDOR_llseek:
	  {
		int   fd;
		off_t   offset;
		int   whence;

		assert( syscall_sock->code(fd) );
		dprintf( D_SYSCALLS, "  fd = %d\n", fd );
		assert( syscall_sock->code(offset) );
		dprintf( D_SYSCALLS, "  offset = %d\n", offset );
		assert( syscall_sock->code(whence) );
		dprintf( D_SYSCALLS, "  whence = %d\n", whence );
		assert( syscall_sock->end_of_message() );;

		errno = 0;
		rval = lseek( fd , offset , whence);
		terrno = (condor_errno_t)errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();
		assert( syscall_sock->code(rval) );
		if( rval < 0 ) {
			assert( syscall_sock->code( terrno ) );
		}
		assert( syscall_sock->end_of_message() );;
		return 0;
	}

	case CONDOR_unlink:
	  {
		char *  path;

		path = (char *)malloc( (unsigned)_POSIX_PATH_MAX );
		memset( path, 0, (unsigned)_POSIX_PATH_MAX );
		assert( syscall_sock->code(path) );
		assert( syscall_sock->end_of_message() );;

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
		assert( syscall_sock->code(rval) );
		if( rval < 0 ) {
			assert( syscall_sock->code( terrno ) );
		}
		free( (char *)path );
		assert( syscall_sock->end_of_message() );;
		return 0;
	}

	case CONDOR_rename:
	  {
		char *  from;
		char *  to;

		to = (char *)malloc( (unsigned)_POSIX_PATH_MAX );
		memset( to, 0, (unsigned)_POSIX_PATH_MAX );
		from = (char *)malloc( (unsigned)_POSIX_PATH_MAX );
		memset( from, 0, (unsigned)_POSIX_PATH_MAX );
		assert( syscall_sock->code(to) );
		assert( syscall_sock->code(from) );
		assert( syscall_sock->end_of_message() );;

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
		assert( syscall_sock->code(rval) );
		if( rval < 0 ) {
			assert( syscall_sock->code( terrno ) );
		}
		free( (char *)to );
		free( (char *)from );
		assert( syscall_sock->end_of_message() );;
		return 0;
	}

	case CONDOR_register_mpi_master_info:
	{
		ClassAd ad;
		assert( ad.initFromStream(*syscall_sock) );
		assert( syscall_sock->end_of_message() );

		errno = 0;
		rval = pseudo_register_mpi_master_info( &ad );
		terrno = (condor_errno_t)errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();
		assert( syscall_sock->code(rval) );
		if( rval < 0 ) {
			assert( syscall_sock->code( terrno ) );
		}
		assert( syscall_sock->end_of_message() );
		return 0;
	}

	case CONDOR_mkdir:
	  {
		char *  path;
		int mode;

		path = (char *)malloc( (unsigned)_POSIX_PATH_MAX );
		memset( path, 0, (unsigned)_POSIX_PATH_MAX );
		assert( syscall_sock->code(path) );
		assert( syscall_sock->code(mode) );
		assert( syscall_sock->end_of_message() );;

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
		assert( syscall_sock->code(rval) );
		if( rval < 0 ) {
			assert( syscall_sock->code( terrno ) );
		}
		free( (char *)path );
		assert( syscall_sock->end_of_message() );;
		return 0;
	}

	case CONDOR_rmdir:
	  {
		char *  path;

		path = (char *)malloc( (unsigned)_POSIX_PATH_MAX );
		memset( path, 0, (unsigned)_POSIX_PATH_MAX );
		assert( syscall_sock->code(path) );
		assert( syscall_sock->end_of_message() );;

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
		assert( syscall_sock->code(rval) );
		if( rval < 0 ) {
			assert( syscall_sock->code( terrno ) );
		}
		free( (char *)path );
		assert( syscall_sock->end_of_message() );;
		return 0;
	}

	case CONDOR_fsync:
	  {
		int fd;

		assert( syscall_sock->code(fd) );
		assert( syscall_sock->end_of_message() );;

		errno = 0;
		rval = fsync(fd);
		terrno = (condor_errno_t)errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();
		assert( syscall_sock->code(rval) );
		if( rval < 0 ) {
			assert( syscall_sock->code( terrno ) );
		}
		assert( syscall_sock->end_of_message() );;
		return 0;
	}

	default:
	{
		EXCEPT( "unknown syscall %d received\n", condor_sysnum );
	}

	}	/* End of switch on system call number */

	return -1;

}	/* End of do_REMOTE_syscall() procedure */
