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
#include "condor_debug.h"
#include "condor_fix_assert.h"
#include "condor_io.h"
#include "condor_constants.h"
#include "condor_classad.h"
#include "condor_sys.h"
#include "starter.h"

#include "NTsenders.h"

int CurrentSysCall;
extern ReliSock *syscall_sock;
extern CStarter *Starter;

extern "C" {
int
REMOTE_CONDOR_register_machine_info(char *uiddomain, char *fsdomain,
	char *address, char *fullHostname, int key)
{
	condor_errno_t		terrno;
	int		rval=-1;

	dprintf ( D_SYSCALLS, "Doing CONDOR_register_machine_info\n" );

	CurrentSysCall = CONDOR_register_machine_info;

	syscall_sock->encode();
	assert( syscall_sock->code(CurrentSysCall) );
	assert( syscall_sock->code(uiddomain) );
	assert( syscall_sock->code(fsdomain) );
	assert( syscall_sock->code(address) );
	assert( syscall_sock->code(fullHostname) );
	assert( syscall_sock->code(key) );
	assert( syscall_sock->end_of_message() );

	syscall_sock->decode();
	assert( syscall_sock->code(rval) );
	if( rval < 0 ) {
		assert( syscall_sock->code(terrno) );
		assert( syscall_sock->end_of_message() );
		errno = terrno;
		dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
		return rval;
	}
	assert( syscall_sock->end_of_message() );
	return rval;
}


int
REMOTE_CONDOR_register_starter_info( ClassAd* ad )
{
	condor_errno_t		terrno;
	int		rval=-1;

	dprintf ( D_SYSCALLS, "Doing CONDOR_register_starter_info\n" );

	CurrentSysCall = CONDOR_register_starter_info;

	if( ! ad ) {
		EXCEPT( "CONDOR_register_starter_info called with NULL ClassAd!" ); 
		return -1;
	}

	syscall_sock->encode();
	assert( syscall_sock->code(CurrentSysCall) );
	assert( ad->put(*syscall_sock) );
	assert( syscall_sock->end_of_message() );

	syscall_sock->decode();
	assert( syscall_sock->code(rval) );
	if( rval < 0 ) {
		assert( syscall_sock->code(terrno) );
		assert( syscall_sock->end_of_message() );
		errno = terrno;
		dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
		return rval;
	}
	assert( syscall_sock->end_of_message() );
	return rval;
}


int
REMOTE_CONDOR_get_job_info(ClassAd *ad)
{
	condor_errno_t		terrno;
	int		rval=-1;

	dprintf ( D_SYSCALLS, "Doing CONDOR_get_job_info\n" );

	CurrentSysCall = CONDOR_get_job_info;

	syscall_sock->encode();
	assert( syscall_sock->code(CurrentSysCall) );
	assert( syscall_sock->end_of_message() );

	syscall_sock->decode();
	assert( syscall_sock->code(rval) );
	if( rval < 0 ) {
		assert( syscall_sock->code(terrno) );
		assert( syscall_sock->end_of_message() );
		errno = terrno;
		dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
		return rval;
	}
	assert( ad->initFromStream(*syscall_sock) );
	assert( syscall_sock->end_of_message() );
	return rval;
}


int
REMOTE_CONDOR_get_user_info(ClassAd *ad)
{
	condor_errno_t		terrno;
	int		rval=-1;

	dprintf ( D_SYSCALLS, "Doing CONDOR_get_user_info\n" );

	CurrentSysCall = CONDOR_get_user_info;

	syscall_sock->encode();
	assert( syscall_sock->code(CurrentSysCall) );
	assert( syscall_sock->end_of_message() );

	syscall_sock->decode();
	assert( syscall_sock->code(rval) );
	if( rval < 0 ) {
		assert( syscall_sock->code(terrno) );
		assert( syscall_sock->end_of_message() );
		errno = terrno;
		dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
		return rval;
	}
	assert( ad->initFromStream(*syscall_sock) );
	assert( syscall_sock->end_of_message() );
	return rval;
}


#if 0
int
REMOTE_CONDOR_get_executable(char *destination)
{
	condor_errno_t		terrno=0;
	int		rval=-1;

	dprintf ( D_SYSCALLS, "Doing CONDOR_get_executable\n" );

	CurrentSysCall = CONDOR_get_executable;

	syscall_sock->encode();
	assert( syscall_sock->code(CurrentSysCall) );
	assert( syscall_sock->end_of_message() );

	syscall_sock->decode();
	assert( syscall_sock->code(rval) );
	if( rval < 0 ) {
		assert( syscall_sock->code(terrno) );
		assert( syscall_sock->end_of_message() );
		errno = terrno;
		dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
		return rval;
	}

	assert( syscall_sock->get_file(destination) > -1 );
	assert( syscall_sock->end_of_message() );
	return rval;
}
#endif

int
REMOTE_CONDOR_job_exit(int status, int reason, ClassAd *ad)
{
	condor_errno_t		terrno;
	int		rval=-1;
	
	dprintf ( D_SYSCALLS, "Doing CONDOR_job_exit\n" );

	CurrentSysCall = CONDOR_job_exit;

	syscall_sock->encode();
	assert( syscall_sock->code(CurrentSysCall) );
	assert( syscall_sock->code(status) );
	assert( syscall_sock->code(reason) );
	if ( ad ) {
		assert( ad->put(*syscall_sock) );
	}
	assert( syscall_sock->end_of_message() );

	syscall_sock->decode();
	assert( syscall_sock->code(rval) );
	if( rval < 0 ) {
		assert( syscall_sock->code(terrno) );
		assert( syscall_sock->end_of_message() );
		errno = terrno;
		dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
		return rval;
	}
	assert( syscall_sock->end_of_message() );
	return rval;
}


int
REMOTE_CONDOR_begin_execution( void )
{
	condor_errno_t		terrno;
	int		rval=-1;
	
	dprintf ( D_SYSCALLS, "Doing CONDOR_begin_execution\n" );

	CurrentSysCall = CONDOR_begin_execution;

	syscall_sock->encode();
	assert( syscall_sock->code(CurrentSysCall) );
	assert( syscall_sock->end_of_message() );

	syscall_sock->decode();
	assert( syscall_sock->code(rval) );
	if( rval < 0 ) {
		assert( syscall_sock->code(terrno) );
		assert( syscall_sock->end_of_message() );
		errno = terrno;
		dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
		return rval;
	}
	assert( syscall_sock->end_of_message() );
	return rval;
}


int
REMOTE_CONDOR_open( char *  path , open_flags_t flags , int   lastarg)
{
        int     rval;
        condor_errno_t     terrno;

        CurrentSysCall = CONDOR_open;

        syscall_sock->encode();
        assert( syscall_sock->code(CurrentSysCall) );
        assert( syscall_sock->code(flags) );
        assert( syscall_sock->code(lastarg) );
        assert( syscall_sock->code(path) );
        assert( syscall_sock->end_of_message() );

        syscall_sock->decode();
        assert( syscall_sock->code(rval) );
        if( rval < 0 ) {
                assert( syscall_sock->code(terrno) );
                assert( syscall_sock->end_of_message() );
                errno = terrno;
                return rval;
        }
        assert( syscall_sock->end_of_message() );
        return rval;
}

int
REMOTE_CONDOR_close(int   fd)
{
        int     rval;
        condor_errno_t     terrno;

        CurrentSysCall = CONDOR_close;

        syscall_sock->encode();
        assert( syscall_sock->code(CurrentSysCall) );
        assert( syscall_sock->code(fd) );
        assert( syscall_sock->end_of_message() );

        syscall_sock->decode();
        assert( syscall_sock->code(rval) );
        if( rval < 0 ) {
                assert( syscall_sock->code(terrno) );
                assert( syscall_sock->end_of_message() );
                errno = terrno;
                return rval;
        }
        assert( syscall_sock->end_of_message() );
        return rval;
}

int
REMOTE_CONDOR_read(int   fd , void *  buf , size_t   len)
{
        int     rval;
        condor_errno_t     terrno;

        CurrentSysCall = CONDOR_read;

        syscall_sock->encode();
        assert( syscall_sock->code(CurrentSysCall) );
        assert( syscall_sock->code(fd) );
        assert( syscall_sock->code(len) );
        assert( syscall_sock->end_of_message() );

        syscall_sock->decode();
        assert( syscall_sock->code(rval) );
        if( rval < 0 ) {
                assert( syscall_sock->code(terrno) );
                assert( syscall_sock->end_of_message() );
                errno = terrno;
                return rval;
        }
        assert( syscall_sock->code_bytes_bool(buf, rval) );
        assert( syscall_sock->end_of_message() );
        return rval;
}

int
REMOTE_CONDOR_write(int   fd , void *  buf , size_t   len)
{
        int     rval;
        condor_errno_t     terrno;

        CurrentSysCall = CONDOR_write;

        syscall_sock->encode();
        assert( syscall_sock->code(CurrentSysCall) );
        assert( syscall_sock->code(fd) );
        assert( syscall_sock->code(len) );
        assert( syscall_sock->code_bytes_bool(buf, len) );
        assert( syscall_sock->end_of_message() );

        syscall_sock->decode();
        assert( syscall_sock->code(rval) );
        if( rval < 0 ) {
                assert( syscall_sock->code(terrno) );
                assert( syscall_sock->end_of_message() );
                errno = terrno;
                return rval;
        }
        assert( syscall_sock->end_of_message() );
        return rval;
}

int
REMOTE_CONDOR_lseek(int   fd , off_t   offset , int   whence)
{
        int     rval;
        condor_errno_t     terrno;

        CurrentSysCall = CONDOR_lseek;

        syscall_sock->encode();
        assert( syscall_sock->code(CurrentSysCall) );
        assert( syscall_sock->code(fd) );
        assert( syscall_sock->code(offset) );
        assert( syscall_sock->code(whence) );
        assert( syscall_sock->end_of_message() );

        syscall_sock->decode();
        assert( syscall_sock->code(rval) );
        if( rval < 0 ) {
                assert( syscall_sock->code(terrno) );
                assert( syscall_sock->end_of_message() );
                errno = terrno;
                return rval;
        }
        assert( syscall_sock->end_of_message() );
        return rval;
}

int
REMOTE_CONDOR_unlink( char *  path )
{
        int     rval;
        condor_errno_t     terrno;

        CurrentSysCall = CONDOR_unlink;

        syscall_sock->encode();
        assert( syscall_sock->code(CurrentSysCall) );
        assert( syscall_sock->code(path) );
        assert( syscall_sock->end_of_message() );

        syscall_sock->decode();
        assert( syscall_sock->code(rval) );
        if( rval < 0 ) {
                assert( syscall_sock->code(terrno) );
                assert( syscall_sock->end_of_message() );
                errno = terrno;
                return rval;
        }
        assert( syscall_sock->end_of_message() );
        return rval;
}

int
REMOTE_CONDOR_rename( char *  from , char *  to)
{
        int     rval;
        condor_errno_t     terrno;

        CurrentSysCall = CONDOR_rename;

        syscall_sock->encode();
        assert( syscall_sock->code(CurrentSysCall) );
        assert( syscall_sock->code(to) );
        assert( syscall_sock->code(from) );
        assert( syscall_sock->end_of_message() );

        syscall_sock->decode();
        assert( syscall_sock->code(rval) );
        if( rval < 0 ) {
                assert( syscall_sock->code(terrno) );
                assert( syscall_sock->end_of_message() );
                errno = terrno;
                return rval;
        }
        assert( syscall_sock->end_of_message() );
        return rval;
}

int
REMOTE_CONDOR_register_mpi_master_info( ClassAd* ad )
{
	condor_errno_t		terrno;
	int		rval=-1;
	
	dprintf ( D_SYSCALLS, "Doing CONDOR_register_mpi_master_info\n" );

	CurrentSysCall = CONDOR_register_mpi_master_info;

	if( ! ad ) {
		EXCEPT( "CONDOR_register_mpi_master_info called with NULL ClassAd!" ); 
		return -1;
	}

	syscall_sock->encode();
	assert( syscall_sock->code(CurrentSysCall) );
	assert( ad->put(*syscall_sock) );
	assert( syscall_sock->end_of_message() );

	syscall_sock->decode();
	assert( syscall_sock->code(rval) );
	if( rval < 0 ) {
		assert( syscall_sock->code(terrno) );
		assert( syscall_sock->end_of_message() );
		errno = terrno;
		dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
		return rval;
	}
	assert( syscall_sock->end_of_message() );
	return rval;
}

int
REMOTE_CONDOR_mkdir( char *  path, int mode )
{
        int     rval;
        condor_errno_t     terrno;

        CurrentSysCall = CONDOR_mkdir;

        syscall_sock->encode();
        assert( syscall_sock->code(CurrentSysCall) );
        assert( syscall_sock->code(path) );
        assert( syscall_sock->code(mode) );
        assert( syscall_sock->end_of_message() );

        syscall_sock->decode();
        assert( syscall_sock->code(rval) );
        if( rval < 0 ) {
                assert( syscall_sock->code(terrno) );
                assert( syscall_sock->end_of_message() );
                errno = terrno;
                return rval;
        }
        assert( syscall_sock->end_of_message() );
        return rval;
}

int
REMOTE_CONDOR_rmdir( char *  path )
{
        int     rval;
        condor_errno_t     terrno;

        CurrentSysCall = CONDOR_rmdir;

        syscall_sock->encode();
        assert( syscall_sock->code(CurrentSysCall) );
        assert( syscall_sock->code(path) );
        assert( syscall_sock->end_of_message() );

        syscall_sock->decode();
        assert( syscall_sock->code(rval) );
        if( rval < 0 ) {
                assert( syscall_sock->code(terrno) );
                assert( syscall_sock->end_of_message() );
                errno = terrno;
                return rval;
        }
        assert( syscall_sock->end_of_message() );
        return rval;
}

int
REMOTE_CONDOR_fsync(int   fd)
{
        int     rval;
        condor_errno_t     terrno;

        CurrentSysCall = CONDOR_fsync;

        syscall_sock->encode();
        assert( syscall_sock->code(CurrentSysCall) );
        assert( syscall_sock->code(fd) );
        assert( syscall_sock->end_of_message() );

        syscall_sock->decode();
        assert( syscall_sock->code(rval) );
        if( rval < 0 ) {
                assert( syscall_sock->code(terrno) );
                assert( syscall_sock->end_of_message() );
                errno = terrno;
                return rval;
        }
        assert( syscall_sock->end_of_message() );
        return rval;
}

} // extern "C"
