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
#include "condor_event.h"

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
	int result = 0;

	dprintf ( D_SYSCALLS, "Doing CONDOR_register_machine_info\n" );

	CurrentSysCall = CONDOR_register_machine_info;

	syscall_sock->encode();
	result = syscall_sock->code(CurrentSysCall);
	ASSERT( result );
	result = syscall_sock->code(uiddomain);
	ASSERT( result );
	result = syscall_sock->code(fsdomain);
	ASSERT( result );
	result = syscall_sock->code(address);
	ASSERT( result );
	result = syscall_sock->code(fullHostname);
	ASSERT( result );
	result = syscall_sock->code(key);
	ASSERT( result );
	result = syscall_sock->end_of_message();
	ASSERT( result );

	syscall_sock->decode();
	result = syscall_sock->code(rval);
	ASSERT( result );

	if( rval < 0 ) {
		result = syscall_sock->code(terrno);
		ASSERT( result );
		result = syscall_sock->end_of_message();
		ASSERT( result );
		errno = terrno;
		dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
		return rval;
	}
	result = syscall_sock->end_of_message();
	ASSERT( result );
	return rval;
}

int
REMOTE_CONDOR_register_starter_info( ClassAd* ad )
{
	condor_errno_t		terrno;
	int		rval=-1;
	int result = 0;

	dprintf ( D_SYSCALLS, "Doing CONDOR_register_starter_info\n" );

	CurrentSysCall = CONDOR_register_starter_info;

	if( ! ad ) {
		EXCEPT( "CONDOR_register_starter_info called with NULL ClassAd!" ); 
		return -1;
	}

	syscall_sock->encode();
	result = syscall_sock->code(CurrentSysCall);
	ASSERT( result );
	result = ad->put(*syscall_sock);
	ASSERT( result );
	result = syscall_sock->end_of_message();
	ASSERT( result );

	syscall_sock->decode();
	result =  syscall_sock->code(rval);
	ASSERT( result );
	if( rval < 0 ) {
		result = syscall_sock->code(terrno);
		ASSERT( result );
		result = syscall_sock->end_of_message();
		ASSERT( result );
		errno = terrno;
		dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
		return rval;
	}
	result = syscall_sock->end_of_message();
	ASSERT( result );
	return rval;
}


int
REMOTE_CONDOR_get_job_info(ClassAd *ad)
{
	condor_errno_t terrno;
	int		rval=-42;
	int result = 0;

	dprintf ( D_SYSCALLS, "Doing CONDOR_get_job_info\n" );

	CurrentSysCall = CONDOR_get_job_info;

	syscall_sock->encode();
	syscall_sock->code(CurrentSysCall);
	result = syscall_sock->end_of_message();
	ASSERT( result );


	syscall_sock->decode();
	result = syscall_sock->code(rval);
	ASSERT( result );
	dprintf(D_SYSCALLS, "\trval = %d\n", rval);
	if( rval < 0 ) {
		result = syscall_sock->code(terrno);
		ASSERT( result );
		result = syscall_sock->end_of_message();
		ASSERT( result );
		errno = terrno;
		dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
		return rval;
	}
	result = ad->initFromStream(*syscall_sock);
	ASSERT( result );
	result = syscall_sock->end_of_message();
	ASSERT( result );
	return rval;
}


int
REMOTE_CONDOR_get_user_info(ClassAd *ad)
{
	condor_errno_t		terrno;
	int		rval=-1;
	int result = 0;

	dprintf ( D_SYSCALLS, "Doing CONDOR_get_user_info\n" );

	CurrentSysCall = CONDOR_get_user_info;

	syscall_sock->encode();
	result = syscall_sock->code(CurrentSysCall);
	ASSERT( result );
	result = syscall_sock->end_of_message();
	ASSERT( result );

	syscall_sock->decode();
	result = syscall_sock->code(rval);
	ASSERT( result );
	if( rval < 0 ) {
		result = syscall_sock->code(terrno);
		ASSERT( result );
		result =syscall_sock->end_of_message();
		ASSERT( result );
		errno = terrno;
		dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
		return rval;
	}
	result = ad->initFromStream(*syscall_sock);
	ASSERT( result );
	result = syscall_sock->end_of_message();
	ASSERT( result );
	return rval;
}


#if 0
int
REMOTE_CONDOR_get_executable(char *destination)
{
	condor_errno_t		terrno=0;
	int		rval=-1;
	int result = 0;

	dprintf ( D_SYSCALLS, "Doing CONDOR_get_executable\n" );

	CurrentSysCall = CONDOR_get_executable;

	syscall_sock->encode();
	result = syscall_sock->code(CurrentSysCall);
	ASSERT( result );
	result = syscall_sock->end_of_message();
	ASSERT( result );

	syscall_sock->decode();
	result = syscall_sock->code(rval);
	ASSERT( result );
	if( rval < 0 ) {
		result = syscall_sock->code(terrno);
		ASSERT( result );
		result = syscall_sock->end_of_message();
		ASSERT( result );
		errno = terrno;
		dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
		return rval;
	}

	result = ( syscall_sock->get_file(destination) > -1 );
	ASSERT( result );
	result = syscall_sock->end_of_message();
	ASSERT( result );
	return rval;
}
#endif

int
REMOTE_CONDOR_job_exit(int status, int reason, ClassAd *ad)
{
	condor_errno_t		terrno;
	int		rval=-1;
	int result = 0;
	
	dprintf ( D_SYSCALLS, "Doing CONDOR_job_exit\n" );

	CurrentSysCall = CONDOR_job_exit;

	syscall_sock->encode();
	result = syscall_sock->code(CurrentSysCall);
	ASSERT( result );
	result = syscall_sock->code(status);
	ASSERT( result );
	result = syscall_sock->code(reason);
	ASSERT( result );
	if ( ad ) {
		result = ad->put(*syscall_sock);
		ASSERT( result );
	}
	result = syscall_sock->end_of_message();
	ASSERT( result );

	syscall_sock->decode();
	result = syscall_sock->code(rval);
	ASSERT( result );
	if( rval < 0 ) {
		result = syscall_sock->code(terrno);
		ASSERT( result );
		result = syscall_sock->end_of_message();
		ASSERT( result );
		errno = terrno;
		dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
		return rval;
	}
	result = syscall_sock->end_of_message();
	ASSERT( result );
	return rval;
}


int
REMOTE_CONDOR_begin_execution( void )
{
	condor_errno_t		terrno;
	int		rval=-1;
	int result = 0;

	dprintf ( D_SYSCALLS, "Doing CONDOR_begin_execution\n" );

	CurrentSysCall = CONDOR_begin_execution;

	syscall_sock->encode();
	result = syscall_sock->code(CurrentSysCall);
	ASSERT( result );
	result = syscall_sock->end_of_message();
	ASSERT( result );

	syscall_sock->decode();
	result = syscall_sock->code(rval);
	ASSERT( result );
	if( rval < 0 ) {
		result = syscall_sock->code(terrno);
		ASSERT( result );
		result = syscall_sock->end_of_message();
		ASSERT( result );
		errno = terrno;
		dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
		return rval;
	}
	result = syscall_sock->end_of_message();
	ASSERT( result );
	return rval;
}


int
REMOTE_CONDOR_open( char *  path , open_flags_t flags , int   lastarg)
{
        int     rval;
        condor_errno_t     terrno;
		int result = 0;

        CurrentSysCall = CONDOR_open;

        syscall_sock->encode();
        result = syscall_sock->code(CurrentSysCall);
		ASSERT( result );
        result = syscall_sock->code(flags);
		ASSERT( result );
        result = syscall_sock->code(lastarg);
		ASSERT( result );
        result = syscall_sock->code(path);
		ASSERT( result );
        result = syscall_sock->end_of_message();
		ASSERT( result );

        syscall_sock->decode();
        result = syscall_sock->code(rval);
		ASSERT( result );
        if( rval < 0 ) {
                result = syscall_sock->code(terrno);
				ASSERT( result );
                result = syscall_sock->end_of_message();
				ASSERT( result );
                errno = terrno;
                return rval;
        }
        result = syscall_sock->end_of_message();
		ASSERT( result );
        return rval;
}

int
REMOTE_CONDOR_close(int   fd)
{
        int     rval;
        condor_errno_t     terrno;
		int result = 0;

        CurrentSysCall = CONDOR_close;

        syscall_sock->encode();
        result = syscall_sock->code(CurrentSysCall);
		ASSERT( result );
        result = syscall_sock->code(fd);
		ASSERT( result );
        result = syscall_sock->end_of_message();
		ASSERT( result );

        syscall_sock->decode();
        result = syscall_sock->code(rval);
		ASSERT( result );
        if( rval < 0 ) {
                result = syscall_sock->code(terrno);
				ASSERT( result );
                result = syscall_sock->end_of_message();
				ASSERT( result );
                errno = terrno;
                return rval;
        }
        result = syscall_sock->end_of_message();
		ASSERT( result );
        return rval;
}

int
REMOTE_CONDOR_read(int   fd , void *  buf , size_t   len)
{
        int     rval;
        condor_errno_t     terrno;
		int result = 0;

        CurrentSysCall = CONDOR_read;

        syscall_sock->encode();
        result = ( syscall_sock->code(CurrentSysCall) );
		ASSERT( result );
        result = ( syscall_sock->code(fd) );
		ASSERT( result );
        result = ( syscall_sock->code(len) );
		ASSERT( result );
        result = ( syscall_sock->end_of_message() );
		ASSERT( result );

        syscall_sock->decode();
        result = ( syscall_sock->code(rval) );
		ASSERT( result );
        if( rval < 0 ) {
                result = ( syscall_sock->code(terrno) );
				ASSERT( result );
                result = ( syscall_sock->end_of_message() );
				ASSERT( result );
                errno = terrno;
                return rval;
        }
        result = ( syscall_sock->code_bytes_bool(buf, rval) );
		ASSERT( result );
        result = ( syscall_sock->end_of_message() );
		ASSERT( result );
        return rval;
}

int
REMOTE_CONDOR_write(int   fd , void *  buf , size_t   len)
{
        int     rval;
        condor_errno_t     terrno;
		int result = 0;

        CurrentSysCall = CONDOR_write;

        syscall_sock->encode();
        result = ( syscall_sock->code(CurrentSysCall) );
		ASSERT( result );
        result = ( syscall_sock->code(fd) );
		ASSERT( result );
        result = ( syscall_sock->code(len) );
		ASSERT( result );
        result = ( syscall_sock->code_bytes_bool(buf, len) );
		ASSERT( result );
        result = ( syscall_sock->end_of_message() );
		ASSERT( result );

        syscall_sock->decode();
        result = ( syscall_sock->code(rval) );
		ASSERT( result );
        if( rval < 0 ) {
                result = ( syscall_sock->code(terrno) );
				ASSERT( result );
                result = ( syscall_sock->end_of_message() );
				ASSERT( result );
                errno = terrno;
                return rval;
        }
        result = ( syscall_sock->end_of_message() );
		ASSERT( result );
        return rval;
}

int
REMOTE_CONDOR_lseek(int   fd , off_t   offset , int   whence)
{
        int     rval;
        condor_errno_t     terrno;
		int result = 0;

        CurrentSysCall = CONDOR_lseek;

        syscall_sock->encode();
        result = ( syscall_sock->code(CurrentSysCall) );
		ASSERT( result );
        result = ( syscall_sock->code(fd) );
		ASSERT( result );
        result = ( syscall_sock->code(offset) );
		ASSERT( result );
        result = ( syscall_sock->code(whence) );
		ASSERT( result );
        result = ( syscall_sock->end_of_message() );
		ASSERT( result );

        syscall_sock->decode();
        result = ( syscall_sock->code(rval) );
		ASSERT( result );
        if( rval < 0 ) {
                result = ( syscall_sock->code(terrno) );
				ASSERT( result );
                result = ( syscall_sock->end_of_message() );
				ASSERT( result );
                errno = terrno;
                return rval;
        }
        result = ( syscall_sock->end_of_message() );
		ASSERT( result );
        return rval;
}

int
REMOTE_CONDOR_unlink( char *  path )
{
        int     rval;
        condor_errno_t     terrno;
		int result = 0;

        CurrentSysCall = CONDOR_unlink;

        syscall_sock->encode();
        result = ( syscall_sock->code(CurrentSysCall) );
		ASSERT( result );
        result = ( syscall_sock->code(path) );
		ASSERT( result );
        result = ( syscall_sock->end_of_message() );
		ASSERT( result );

        syscall_sock->decode();
        result = ( syscall_sock->code(rval) );
		ASSERT( result );
        if( rval < 0 ) {
                result = ( syscall_sock->code(terrno) );
				ASSERT( result );
                result = ( syscall_sock->end_of_message() );
				ASSERT( result );
                errno = terrno;
                return rval;
        }
        result = ( syscall_sock->end_of_message() );
		ASSERT( result );
        return rval;
}

int
REMOTE_CONDOR_rename( char *  from , char *  to)
{
        int     rval;
        condor_errno_t     terrno;
		int result = 0;

        CurrentSysCall = CONDOR_rename;

        syscall_sock->encode();
        result = ( syscall_sock->code(CurrentSysCall) );
		ASSERT( result );
        result = ( syscall_sock->code(to) );
		ASSERT( result );
        result = ( syscall_sock->code(from) );
		ASSERT( result );
        result = ( syscall_sock->end_of_message() );
		ASSERT( result );

        syscall_sock->decode();
        result = ( syscall_sock->code(rval) );
		ASSERT( result );
        if( rval < 0 ) {
                result = ( syscall_sock->code(terrno) );
				ASSERT( result );
                result = ( syscall_sock->end_of_message() );
				ASSERT( result );
                errno = terrno;
                return rval;
        }
        result = ( syscall_sock->end_of_message() );
		ASSERT( result );
        return rval;
}

int
REMOTE_CONDOR_register_mpi_master_info( ClassAd* ad )
{
	condor_errno_t		terrno;
	int		rval=-1;
	int result = 0;
	
	dprintf ( D_SYSCALLS, "Doing CONDOR_register_mpi_master_info\n" );

	CurrentSysCall = CONDOR_register_mpi_master_info;

	if( ! ad ) {
		EXCEPT( "CONDOR_register_mpi_master_info called with NULL ClassAd!" ); 
		return -1;
	}

	syscall_sock->encode();
	result = ( syscall_sock->code(CurrentSysCall) );
	ASSERT( result );
	result = ( ad->put(*syscall_sock) );
	ASSERT( result );
	result = ( syscall_sock->end_of_message() );
	ASSERT( result );

	syscall_sock->decode();
	result = ( syscall_sock->code(rval) );
	ASSERT( result );
	if( rval < 0 ) {
		result = ( syscall_sock->code(terrno) );
		ASSERT( result );
		result = ( syscall_sock->end_of_message() );
		ASSERT( result );
		errno = terrno;
		dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
		return rval;
	}
	result = ( syscall_sock->end_of_message() );
	ASSERT( result );
	return rval;
}

int
REMOTE_CONDOR_mkdir( char *  path, int mode )
{
        int     rval;
        condor_errno_t     terrno;
		int result = 0;

        CurrentSysCall = CONDOR_mkdir;

        syscall_sock->encode();
        result = ( syscall_sock->code(CurrentSysCall) );
		ASSERT( result );
        result = ( syscall_sock->code(path) );
		ASSERT( result );
        result = ( syscall_sock->code(mode) );
		ASSERT( result );
        result = ( syscall_sock->end_of_message() );
		ASSERT( result );

        syscall_sock->decode();
        result = ( syscall_sock->code(rval) );
		ASSERT( result );
        if( rval < 0 ) {
                result = ( syscall_sock->code(terrno) );
				ASSERT( result );
                result = ( syscall_sock->end_of_message() );
				ASSERT( result );
                errno = terrno;
                return rval;
        }
        result = ( syscall_sock->end_of_message() );
		ASSERT( result );
        return rval;
}

int
REMOTE_CONDOR_rmdir( char *  path )
{
        int     rval;
        condor_errno_t     terrno;
		int result = 0;

        CurrentSysCall = CONDOR_rmdir;

        syscall_sock->encode();
        result = ( syscall_sock->code(CurrentSysCall) );
		ASSERT( result );
        result = ( syscall_sock->code(path) );
		ASSERT( result );
        result = ( syscall_sock->end_of_message() );
		ASSERT( result );

        syscall_sock->decode();
        result = ( syscall_sock->code(rval) );
		ASSERT( result );
        if( rval < 0 ) {
                result = ( syscall_sock->code(terrno) );
				ASSERT( result );
                result = ( syscall_sock->end_of_message() );
				ASSERT( result );
                errno = terrno;
                return rval;
        }
        result = ( syscall_sock->end_of_message() );
		ASSERT( result );
        return rval;
}

int
REMOTE_CONDOR_fsync(int   fd)
{
        int     rval;
        condor_errno_t     terrno;
		int result = 0;

        CurrentSysCall = CONDOR_fsync;

        syscall_sock->encode();
        result = ( syscall_sock->code(CurrentSysCall) );
		ASSERT( result );
        result = ( syscall_sock->code(fd) );
		ASSERT( result );
        result = ( syscall_sock->end_of_message() );
		ASSERT( result );

        syscall_sock->decode();
        result = ( syscall_sock->code(rval) );
		ASSERT( result );
        if( rval < 0 ) {
                result = ( syscall_sock->code(terrno) );
				ASSERT( result );
                result = ( syscall_sock->end_of_message() );
				ASSERT( result );
                errno = terrno;
                return rval;
        }
        result = ( syscall_sock->end_of_message() );
		ASSERT( result );
        return rval;
}

int
REMOTE_CONDOR_get_file_info_new(char *  logical_name , char *  actual_url)
{
        int     rval;
        condor_errno_t  terrno;
 
        CurrentSysCall = CONDOR_get_file_info_new;
 
        syscall_sock->encode();
        ASSERT( syscall_sock->code(CurrentSysCall) );
        ASSERT( syscall_sock->code(logical_name) );
        ASSERT( syscall_sock->end_of_message() );
 
        syscall_sock->decode();
        ASSERT( syscall_sock->code(rval) );
        if( rval < 0 ) {
                ASSERT( syscall_sock->code(terrno) );
                ASSERT( syscall_sock->end_of_message() );
                errno = (int)terrno;
                return rval;
        }
        ASSERT( syscall_sock->code(actual_url) );
        ASSERT( syscall_sock->end_of_message() );

        return rval;
}                                                                              

int REMOTE_CONDOR_ulog_printf( char const *str, ... )
{
	MyString buf;
	va_list args;
	int retval;

	va_start(args,str);
	buf.vsprintf(str,args);
	retval = REMOTE_CONDOR_ulog_error( buf.GetCStr() );
	va_end(args);

	return retval;
}

int
REMOTE_CONDOR_ulog_error( char const *str )
{
	RemoteErrorEvent event;
	//NOTE: "ExecuteHost" info will be inserted by the shadow.
	event.setDaemonName("starter"); //TODO: where should this come from?
	event.setErrorText( str );
	event.setCriticalError( true );
	return REMOTE_CONDOR_ulog( event.toClassAd() );
}

int
REMOTE_CONDOR_ulog( ClassAd *ad )
{
	int result = 0;

	dprintf ( D_SYSCALLS, "Doing CONDOR_ulog\n" );

	CurrentSysCall = CONDOR_ulog;

	if( ! ad ) {
		EXCEPT( "CONDOR_ulog called with NULL ClassAd!" ); 
		return -1;
	}

	syscall_sock->encode();
	result = syscall_sock->code(CurrentSysCall);
	ASSERT( result );
	result = ad->put(*syscall_sock);
	ASSERT( result );
	result = syscall_sock->end_of_message();
	ASSERT( result );

	//NOTE: we expect no response.

	return 0;
}

int
REMOTE_CONDOR_get_job_attr(char *  attrname , char *  expr)
{
	int	rval;
	condor_errno_t	terrno;

	CurrentSysCall = CONDOR_get_job_attr;

	syscall_sock->encode();
	ASSERT( syscall_sock->code(CurrentSysCall) );
	ASSERT( syscall_sock->code(attrname) );
	ASSERT( syscall_sock->end_of_message() );

	syscall_sock->decode();
	ASSERT( syscall_sock->code(rval) );
	if( rval < 0 ) {
		ASSERT( syscall_sock->code(terrno) );
		ASSERT( syscall_sock->end_of_message() );
		errno = (int)terrno;
		return rval;
	}
	ASSERT( syscall_sock->code(expr) );
	ASSERT( syscall_sock->end_of_message() );
	return rval;
}

int
REMOTE_CONDOR_set_job_attr(char *  attrname , char *  expr)
{
	int	rval;
	condor_errno_t	terrno;

	CurrentSysCall = CONDOR_set_job_attr;

	syscall_sock->encode();
	ASSERT( syscall_sock->code(CurrentSysCall) );
	ASSERT( syscall_sock->code(expr) );
	ASSERT( syscall_sock->code(attrname) );
	ASSERT( syscall_sock->end_of_message() );

	syscall_sock->decode();
	ASSERT( syscall_sock->code(rval) );
	if( rval < 0 ) {
		ASSERT( syscall_sock->code(terrno) );
		ASSERT( syscall_sock->end_of_message() );
		errno = (int)terrno;
		return rval;
	}
	ASSERT( syscall_sock->end_of_message() );
	return rval;
}

int
REMOTE_CONDOR_constrain( char *  expr)
{
	int	rval;
	condor_errno_t	terrno;

	CurrentSysCall = CONDOR_constrain;

	syscall_sock->encode();
	ASSERT( syscall_sock->code(CurrentSysCall) );
	ASSERT( syscall_sock->code(expr) );
	ASSERT( syscall_sock->end_of_message() );

	syscall_sock->decode();
	ASSERT( syscall_sock->code(rval) );
	if( rval < 0 ) {
		ASSERT( syscall_sock->code(terrno) );
		ASSERT( syscall_sock->end_of_message() );
		errno = (int)terrno;
		return rval;
	}
	ASSERT( syscall_sock->end_of_message() );
	return rval;
}

} // extern "C"
