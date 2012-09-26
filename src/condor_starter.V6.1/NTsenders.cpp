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
#include "condor_debug.h"
#include "condor_fix_assert.h"
#include "condor_io.h"
#include "condor_constants.h"
#include "condor_classad.h"
#include "condor_sys.h"
#include "starter.h"
#include "condor_event.h"

#include "NTsenders.h"

#define ON_ERROR_RETURN(x) if (x <= 0) {dprintf(D_ALWAYS, "i/o error result is %d, errno is %d\n", x, errno);errno=ETIMEDOUT;return x;}

static int CurrentSysCall;
extern ReliSock *syscall_sock;
extern CStarter *Starter;

extern "C" {
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
REMOTE_CONDOR_register_job_info( ClassAd* ad )
{
	condor_errno_t		terrno;
	int		rval=-1;

	dprintf ( D_SYSCALLS, "Doing CONDOR_register_job_info\n" );

	CurrentSysCall = CONDOR_register_job_info;

		/*
		  This RSC is a special-case.  If there are any network
		  failures at all, instead of blowing an ASSERT(), we want to
		  just return an error code.  The shadow never returns failure
		  for this pseudo call, so if the starter sees -1 from this,
		  it knows there was a network error.  in this case, it sets a
		  timer to wait for the DisconnectedRunTimeout to expire,
		  hoping to get a request for a reconnect.
		*/

	if( ! ad ) {
		EXCEPT( "CONDOR_register_job_info called with NULL ClassAd!" ); 
		return -1;
	}

	if( ! syscall_sock->is_connected() ) {
		return -1;
	}

	syscall_sock->encode();
	if (!syscall_sock->code(CurrentSysCall)) {
		return -1;
	}
	if (!ad->put(*syscall_sock)) {
		return -1;
	}
	if (!syscall_sock->end_of_message()) {
		return -1;
	}

	syscall_sock->decode();
	if (!syscall_sock->code(rval)) {
		return -1;
	}
	if( rval < 0 ) {
		if (!syscall_sock->code(terrno)) {
			return -1;
		}
		if (!syscall_sock->end_of_message()) {
			return -1;
		}
		errno = terrno;
		dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
		return rval;
	}
	if (!syscall_sock->end_of_message()) {
		return -1;
	}
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
	
	dprintf ( D_SYSCALLS, "Doing CONDOR_job_exit\n" );

	CurrentSysCall = CONDOR_job_exit;

		/*
		  This RSC is a special-case.  if there are any network
		  failures at all, instead of blowing an ASSERT(), we want to
		  just return an error code.  The shadow never returns failure
		  for this pseudo call, so if the starter sees -1 from this,
		  it knows there was a network error.  in this case, it sets a
		  timer to wait for the DisconnectedRunTimeout to expire,
		  hoping to get a request for a reconnect.
		*/
	if( ! syscall_sock->is_connected() ) {
		return -1;
	}

	syscall_sock->encode();
	if( ! syscall_sock->code(CurrentSysCall) ) {
		return -1;
	}
	if( ! syscall_sock->code(status) ) {
		return -1;
	}
	if( ! syscall_sock->code(reason) ) {
		return -1;
	}
	if ( ad ) {
		if( ! ad->put(*syscall_sock) ) {
			return -1;
		}
	}
	if( ! syscall_sock->end_of_message() ) {
		return -1;
	}
	syscall_sock->decode();
	if( ! syscall_sock->code(rval) ) {
		return -1;
	}
	if( rval < 0 ) {
		if( ! syscall_sock->code(terrno) ) {
			return -1;
		}
		if( ! syscall_sock->end_of_message() ) {
			return -1;
		}
		errno = terrno;
		dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
		return rval;
	}
	if( ! syscall_sock->end_of_message() ) {
		return -1;
	}
	return rval;
}

int
REMOTE_CONDOR_job_termination( ClassAd* ad )
{
	condor_errno_t		terrno;
	int		rval=-1;
	int result = 0;

	dprintf ( D_SYSCALLS, "Doing CONDOR_job_termination\n" );

	CurrentSysCall = CONDOR_job_termination;

	if( ! ad ) {
		EXCEPT( "CONDOR_job_termination called with NULL ClassAd!" ); 
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
REMOTE_CONDOR_open( char const *  path , open_flags_t flags , int   lastarg)
{
        int     rval;
        condor_errno_t     terrno;
		int result = 0;

        dprintf ( D_SYSCALLS, "Doing CONDOR_open\n" );

        CurrentSysCall = CONDOR_open;

        syscall_sock->encode();
        result = syscall_sock->code(CurrentSysCall);
		ON_ERROR_RETURN( result );
        result = syscall_sock->code(flags);
		ON_ERROR_RETURN( result );
        result = syscall_sock->code(lastarg);
		ON_ERROR_RETURN( result );
        result = syscall_sock->put(path);
		ON_ERROR_RETURN( result );
        result = syscall_sock->end_of_message();
		ON_ERROR_RETURN( result );

        syscall_sock->decode();
        result = syscall_sock->code(rval);
		ON_ERROR_RETURN( result );
        if( rval < 0 ) {
                result = syscall_sock->code(terrno);
				ON_ERROR_RETURN( result );
                result = syscall_sock->end_of_message();
				ON_ERROR_RETURN( result );
                errno = terrno;
                dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
                return rval;
        }
        result = syscall_sock->end_of_message();
		ON_ERROR_RETURN( result );
        return rval;
}

int
REMOTE_CONDOR_close(int   fd)
{
        int     rval;
        condor_errno_t     terrno;
		int result = 0;

        dprintf ( D_SYSCALLS, "Doing CONDOR_close\n" );

        CurrentSysCall = CONDOR_close;

        syscall_sock->encode();
        result = syscall_sock->code(CurrentSysCall);
		ON_ERROR_RETURN( result );
        result = syscall_sock->code(fd);
		ON_ERROR_RETURN( result );
        result = syscall_sock->end_of_message();
		ON_ERROR_RETURN( result );

        syscall_sock->decode();
        result = syscall_sock->code(rval);
		ON_ERROR_RETURN( result );
        if( rval < 0 ) {
                result = syscall_sock->code(terrno);
				ON_ERROR_RETURN( result );
                result = syscall_sock->end_of_message();
				ON_ERROR_RETURN( result );
                errno = terrno;
                dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
                return rval;
        }
        result = syscall_sock->end_of_message();
		ON_ERROR_RETURN( result );
        return rval;
}

int
REMOTE_CONDOR_read(int   fd , void *  buf , size_t   len)
{
        int     rval;
        condor_errno_t     terrno;
		int result = 0;

        dprintf ( D_SYSCALLS, "Doing CONDOR_read\n" );

        CurrentSysCall = CONDOR_read;

        syscall_sock->encode();
        result = ( syscall_sock->code(CurrentSysCall) );
		ON_ERROR_RETURN( result );
        result = ( syscall_sock->code(fd) );
		ON_ERROR_RETURN( result );
        result = ( syscall_sock->code(len) );
		ON_ERROR_RETURN( result );
        result = ( syscall_sock->end_of_message() );
		ON_ERROR_RETURN( result );

        syscall_sock->decode();
        result = ( syscall_sock->code(rval) );
		ON_ERROR_RETURN( result );
        if( rval < 0 ) {
                result = ( syscall_sock->code(terrno) );
				ON_ERROR_RETURN( result );
                result = ( syscall_sock->end_of_message() );
				ON_ERROR_RETURN( result );
                errno = terrno;
                dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
                return rval;
        }
        result = ( syscall_sock->code_bytes_bool(buf, rval) );
		ON_ERROR_RETURN( result );
        result = ( syscall_sock->end_of_message() );
		ON_ERROR_RETURN( result );
        return rval;
}

int
REMOTE_CONDOR_write(int   fd , void *  buf , size_t   len)
{
        int     rval;
        condor_errno_t     terrno;
		int result = 0;

        dprintf ( D_SYSCALLS, "Doing CONDOR_write\n" );

        CurrentSysCall = CONDOR_write;

        syscall_sock->encode();
        result = ( syscall_sock->code(CurrentSysCall) );
		ON_ERROR_RETURN( result );
        result = ( syscall_sock->code(fd) );
		ON_ERROR_RETURN( result );
        result = ( syscall_sock->code(len) );
		ON_ERROR_RETURN( result );
        result = ( syscall_sock->code_bytes_bool(buf, len) );
		ON_ERROR_RETURN( result );
        result = ( syscall_sock->end_of_message() );
		ON_ERROR_RETURN( result );

        syscall_sock->decode();
        result = ( syscall_sock->code(rval) );
		ON_ERROR_RETURN( result );
        if( rval < 0 ) {
                result = ( syscall_sock->code(terrno) );
				ON_ERROR_RETURN( result );
                result = ( syscall_sock->end_of_message() );
				ON_ERROR_RETURN( result );
                errno = terrno;
                dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
                return rval;
        }
        result = ( syscall_sock->end_of_message() );
		ON_ERROR_RETURN( result );
        return rval;
}


int
REMOTE_CONDOR_lseek(int   fd , off_t   offset , int   whence)
{
        int     rval;
        condor_errno_t     terrno;
		int result = 0;

        dprintf ( D_SYSCALLS, "Doing CONDOR_lseek\n" );

        CurrentSysCall = CONDOR_lseek;

        syscall_sock->encode();
        result = ( syscall_sock->code(CurrentSysCall) );
		ON_ERROR_RETURN( result );
        result = ( syscall_sock->code(fd) );
		ON_ERROR_RETURN( result );
        result = ( syscall_sock->code(offset) );
		ON_ERROR_RETURN( result );
        result = ( syscall_sock->code(whence) );
		ON_ERROR_RETURN( result );
        result = ( syscall_sock->end_of_message() );
		ON_ERROR_RETURN( result );

        syscall_sock->decode();
        result = ( syscall_sock->code(rval) );
		ON_ERROR_RETURN( result );
        if( rval < 0 ) {
                result = ( syscall_sock->code(terrno) );
				ON_ERROR_RETURN( result );
                result = ( syscall_sock->end_of_message() );
				ON_ERROR_RETURN( result );
                errno = terrno;
                dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
                return rval;
        }
        result = ( syscall_sock->end_of_message() );
		ON_ERROR_RETURN( result );
        return rval;
}

int
REMOTE_CONDOR_unlink( char *  path )
{
        int     rval;
        condor_errno_t     terrno;
		int result = 0;

        dprintf ( D_SYSCALLS, "Doing CONDOR_unlink\n" );

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
                dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
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

        dprintf ( D_SYSCALLS, "Doing CONDOR_rename\n" );

        CurrentSysCall = CONDOR_rename;

        syscall_sock->encode();
        result = ( syscall_sock->code(CurrentSysCall) );
		ASSERT( result );
        result = ( syscall_sock->code(from) );
		ASSERT( result );
        result = ( syscall_sock->code(to) );
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

        dprintf ( D_SYSCALLS, "Doing CONDOR_mkdir\n" );

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
                dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
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

        dprintf ( D_SYSCALLS, "Doing CONDOR_rmdir\n" );

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
                dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
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

        dprintf ( D_SYSCALLS, "Doing CONDOR_fsync\n" );

        CurrentSysCall = CONDOR_fsync;

        syscall_sock->encode();
        result = ( syscall_sock->code(CurrentSysCall) );
		ON_ERROR_RETURN( result );
        result = ( syscall_sock->code(fd) );
		ON_ERROR_RETURN( result );
        result = ( syscall_sock->end_of_message() );
		ON_ERROR_RETURN( result );

        syscall_sock->decode();
        result = ( syscall_sock->code(rval) );
		ON_ERROR_RETURN( result );
        if( rval < 0 ) {
                result = ( syscall_sock->code(terrno) );
				ON_ERROR_RETURN( result );
                result = ( syscall_sock->end_of_message() );
				ON_ERROR_RETURN( result );
                errno = terrno;
                dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
                return rval;
        }
        result = ( syscall_sock->end_of_message() );
		ON_ERROR_RETURN( result );
        return rval;
}

int
REMOTE_CONDOR_get_file_info_new(char *  logical_name , char *&actual_url)
{
        int     rval;
        condor_errno_t  terrno;
 
        dprintf ( D_SYSCALLS, "Doing CONDOR_get_file_info_new\n" );

        CurrentSysCall = CONDOR_get_file_info_new;

		ASSERT( actual_url == NULL );
 
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
                dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
                return rval;
        }
        ASSERT( syscall_sock->get(actual_url) );
        ASSERT( syscall_sock->end_of_message() );

        return rval;
}                                                                              

int REMOTE_CONDOR_ulog_error_printf( int hold_reason_code, int hold_reason_subcode, char const *str, ... )
{
	MyString buf;
	va_list args;
	int retval;

	va_start(args,str);
	buf.vformatstr(str,args);
	retval = REMOTE_CONDOR_ulog_error( hold_reason_code, hold_reason_subcode, buf.Value() );
	va_end(args);

	return retval;
}

int
REMOTE_CONDOR_ulog_error( int hold_reason_code, int hold_reason_subcode, char const *str )
{
	RemoteErrorEvent event;
	ClassAd *ad;
	//NOTE: "ExecuteHost" info will be inserted by the shadow.
	event.setDaemonName("starter"); //TODO: where should this come from?
	event.setErrorText( str );
	event.setCriticalError( true );
	event.setHoldReasonCode( hold_reason_code );
	event.setHoldReasonSubCode( hold_reason_subcode );
	ad = event.toClassAd();
	ASSERT(ad);
	int retval = REMOTE_CONDOR_ulog( ad );
	delete ad;
	return retval;
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
REMOTE_CONDOR_get_job_attr(char *  attrname , char *& expr)
{
	int	rval;
	condor_errno_t	terrno;

	dprintf ( D_SYSCALLS, "Doing CONDOR_get_job_attr\n" );

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
		dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
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

	dprintf ( D_SYSCALLS, "Doing CONDOR_set_job_attr\n" );

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
		dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
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

	dprintf ( D_SYSCALLS, "Doing CONDOR_constrain\n" );

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
		dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
		return rval;
	}
	ASSERT( syscall_sock->end_of_message() );
	return rval;
}

int REMOTE_CONDOR_get_sec_session_info(
	char const *starter_reconnect_session_info,
	MyString &reconnect_session_id,
	MyString &reconnect_session_info,
	MyString &reconnect_session_key,
	char const *starter_filetrans_session_info,
	MyString &filetrans_session_id,
	MyString &filetrans_session_info,
	MyString &filetrans_session_key)
{
	int	rval=-1;
	condor_errno_t	terrno;

	dprintf ( D_SYSCALLS, "Doing CONDOR_get_sec_session_info\n" );

	CurrentSysCall = CONDOR_get_sec_session_info;

	syscall_sock->encode();
	ASSERT( syscall_sock->code(CurrentSysCall) );

	bool socket_default_crypto = syscall_sock->get_encryption();
	if( !socket_default_crypto ) {
		// always encrypt; we are exchanging super secret session keys
		syscall_sock->set_crypto_mode(true);
	}

	ASSERT( syscall_sock->put(starter_reconnect_session_info) );
	ASSERT( syscall_sock->put(starter_filetrans_session_info) );
	ASSERT( syscall_sock->end_of_message() );

	syscall_sock->decode();
	ASSERT( syscall_sock->code(rval) );
	if( rval < 0 ) {
		ASSERT( syscall_sock->code(terrno) );
		ASSERT( syscall_sock->end_of_message() );
		errno = (int)terrno;
	}
	else {
		ASSERT( syscall_sock->code(reconnect_session_id) );
		ASSERT( syscall_sock->code(reconnect_session_info) );
		ASSERT( syscall_sock->code(reconnect_session_key) );

		ASSERT( syscall_sock->code(filetrans_session_id) );
		ASSERT( syscall_sock->code(filetrans_session_info) );
		ASSERT( syscall_sock->code(filetrans_session_key) );

		ASSERT( syscall_sock->end_of_message() );
	}

	if( !socket_default_crypto ) {
		syscall_sock->set_crypto_mode( false );  // restore default
	}
	return rval;
}


int
REMOTE_CONDOR_pread(int fd , void* buf , size_t len, size_t offset)
{
	int rval = -1, result = 0;
	condor_errno_t terrno;

	dprintf ( D_SYSCALLS, "Doing CONDOR_pread\n" );

	CurrentSysCall = CONDOR_pread;

	syscall_sock->encode();
	result = ( syscall_sock->code(CurrentSysCall) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->code(fd) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->code(len) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->code(offset) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->end_of_message() );
	ON_ERROR_RETURN( result );

	syscall_sock->decode();
	result = ( syscall_sock->code(rval) );
	ON_ERROR_RETURN( result );
	if( rval < 0 ) {
		result = ( syscall_sock->code(terrno) );
		ON_ERROR_RETURN( result );
		result = ( syscall_sock->end_of_message() );
		ON_ERROR_RETURN( result );
		errno = terrno;
		dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
		return rval;
	}
	result = ( syscall_sock->code_bytes_bool(buf, rval) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->end_of_message() );
	ON_ERROR_RETURN( result );
	return rval;
}

int
REMOTE_CONDOR_pwrite(int fd , void* buf ,size_t len, size_t offset)
{
	int rval = -1, result = 0;
	condor_errno_t terrno;

	dprintf ( D_SYSCALLS, "Doing CONDOR_pwrite\n" );

	CurrentSysCall = CONDOR_pwrite;

	syscall_sock->encode();
	result = ( syscall_sock->code(CurrentSysCall) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->code(fd) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->code(len) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->code(offset) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->code_bytes_bool(buf, len) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->end_of_message() );
	ON_ERROR_RETURN( result );

	syscall_sock->decode();
	result = ( syscall_sock->code(rval) );
	ON_ERROR_RETURN( result );
	if( rval < 0 ) {
		result = ( syscall_sock->code(terrno) );
		ON_ERROR_RETURN( result );
		result = ( syscall_sock->end_of_message() );
		ON_ERROR_RETURN( result );
		errno = terrno;
		dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
		return rval;
	}
	result = ( syscall_sock->end_of_message() );
	ON_ERROR_RETURN( result );
	return rval;
}

int
REMOTE_CONDOR_sread(int fd , void* buf , size_t len, size_t offset,
	size_t stride_length, size_t stride_skip)
{
	int rval = -1, result = 0;
	condor_errno_t terrno;

	dprintf ( D_SYSCALLS, "Doing CONDOR_sread\n" );

	CurrentSysCall = CONDOR_sread;

	syscall_sock->encode();
	result = ( syscall_sock->code(CurrentSysCall) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->code(fd) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->code(len) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->code(offset) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->code(stride_length) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->code(stride_skip) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->end_of_message() );
	ON_ERROR_RETURN( result );

	syscall_sock->decode();
	result = ( syscall_sock->code(rval) );
	ON_ERROR_RETURN( result );
	if( rval < 0 ) {
		result = ( syscall_sock->code(terrno) );
		ON_ERROR_RETURN( result );
		result = ( syscall_sock->end_of_message() );
		ON_ERROR_RETURN( result );
		errno = terrno;
		dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
		return rval;
	}
	result = ( syscall_sock->code_bytes_bool(buf, rval) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->end_of_message() );
	ON_ERROR_RETURN( result );
	return rval;
}

int
REMOTE_CONDOR_swrite(int fd , void* buf ,size_t len, size_t offset, 
		size_t stride_length, size_t stride_skip)
{
	int rval = -1, result = 0;
	condor_errno_t terrno;

	dprintf ( D_SYSCALLS, "Doing CONDOR_swrite\n" );

	CurrentSysCall = CONDOR_swrite;

	syscall_sock->encode();
	result = ( syscall_sock->code(CurrentSysCall) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->code(fd) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->code(len) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->code(offset) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->code(stride_length) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->code(stride_skip) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->code_bytes_bool(buf, len) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->end_of_message() );
	ON_ERROR_RETURN( result );

	syscall_sock->decode();
	result = ( syscall_sock->code(rval) );
	ON_ERROR_RETURN( result );
	if( rval < 0 ) {
		result = ( syscall_sock->code(terrno) );
		ON_ERROR_RETURN( result );
		result = ( syscall_sock->end_of_message() );
		ON_ERROR_RETURN( result );
		errno = terrno;
		dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
		return rval;
	}
	result = ( syscall_sock->end_of_message() );
	ON_ERROR_RETURN( result );
	return rval;
}

int
REMOTE_CONDOR_rmall(char *path)
{
	int rval = -1, result = 0;
	condor_errno_t terrno;

	dprintf ( D_SYSCALLS, "Doing CONDOR_rmall\n" );

	CurrentSysCall = CONDOR_rmall;

	syscall_sock->encode();
	result = ( syscall_sock->code(CurrentSysCall) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->code(path) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->end_of_message() );
	ON_ERROR_RETURN( result );

	syscall_sock->decode();
	result = ( syscall_sock->code(rval) );
	ON_ERROR_RETURN( result );
	if( rval < 0 ) {
		result = ( syscall_sock->code(terrno) );
		ON_ERROR_RETURN( result );
		result = ( syscall_sock->end_of_message() );
		ON_ERROR_RETURN( result );
		errno = terrno;
		dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
		return rval;
	}
	result = ( syscall_sock->end_of_message() );
	ON_ERROR_RETURN( result );
	return rval;
}

int
REMOTE_CONDOR_getfile(char *path, char **buffer)
{
	int rval = -1, result = 0;
	condor_errno_t terrno;

	dprintf ( D_SYSCALLS, "Doing CONDOR_getfile\n" );

	CurrentSysCall = CONDOR_getfile;

	syscall_sock->encode();
	result = ( syscall_sock->code(CurrentSysCall) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->code(path) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->end_of_message() );
	ON_ERROR_RETURN( result );

	syscall_sock->decode();
	result = ( syscall_sock->code(rval) );
	ON_ERROR_RETURN( result );
	if( rval < 0 ) {
		result = ( syscall_sock->code(terrno) );
		ON_ERROR_RETURN( result );
		result = ( syscall_sock->end_of_message() );
		ON_ERROR_RETURN( result );
		errno = terrno;
		dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
		return rval;
	}
	*buffer = (char*)malloc(rval);
	result = ( syscall_sock->code_bytes_bool(*buffer, rval) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->end_of_message() );
	ON_ERROR_RETURN( result );
	return rval;
}

int
REMOTE_CONDOR_putfile(char *path, int mode, int length)
{
	int rval = -1, result = 0;
	condor_errno_t terrno;

	dprintf ( D_SYSCALLS, "Doing CONDOR_putfile\n" );

	CurrentSysCall = CONDOR_putfile;

	syscall_sock->encode();
	result = ( syscall_sock->code(CurrentSysCall) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->code(path) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->code(mode) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->code(length) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->end_of_message() );
	ON_ERROR_RETURN( result );

	syscall_sock->decode();
	result = ( syscall_sock->code(rval) );
	ON_ERROR_RETURN( result );
	if( rval < 0 ) {
		result = ( syscall_sock->code(terrno) );
		ON_ERROR_RETURN( result );
		errno = terrno;
		dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
	}
	result = ( syscall_sock->end_of_message() );
	ON_ERROR_RETURN( result );
	return rval;
}

int
REMOTE_CONDOR_getlongdir(char *path, char *&buffer)
{
	int rval = -1, result = 0;
	condor_errno_t terrno;

	dprintf ( D_SYSCALLS, "Doing CONDOR_getlongdir\n" );

	CurrentSysCall = CONDOR_getlongdir;

	syscall_sock->encode();
	result = ( syscall_sock->code(CurrentSysCall) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->code(path) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->end_of_message() );
	ON_ERROR_RETURN( result );

	syscall_sock->decode();
	result = ( syscall_sock->code(rval) );
	ON_ERROR_RETURN( result );
	if( rval < 0 ) {
		result = ( syscall_sock->code(terrno) );
		ON_ERROR_RETURN( result );
		result = ( syscall_sock->end_of_message() );
		ON_ERROR_RETURN( result );
		errno = terrno;
		dprintf ( D_SYSCALLS, "Return val problem(%d), errno = %d\n", rval, errno );
		return rval;
	}
	result = ( syscall_sock->code(buffer) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->end_of_message() );
	ON_ERROR_RETURN( result );
	return rval;
}

int
REMOTE_CONDOR_getdir(char *path, char *&buffer)
{
	int rval = -1, result = 0;
	condor_errno_t terrno;
	
	dprintf ( D_SYSCALLS, "Doing CONDOR_getdir\n" );

	CurrentSysCall = CONDOR_getdir;

	syscall_sock->encode();
	result = ( syscall_sock->code(CurrentSysCall) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->code(path) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->end_of_message() );
	ON_ERROR_RETURN( result );

	syscall_sock->decode();
	result = ( syscall_sock->code(rval) );
	ON_ERROR_RETURN( result );
	if( rval <= 0 ) {
		result = ( syscall_sock->code(terrno) );
		ON_ERROR_RETURN( result );
		result = ( syscall_sock->end_of_message() );
		ON_ERROR_RETURN( result );
		errno = terrno;
		dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
		return rval;
	}
	result = ( syscall_sock->code(buffer) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->end_of_message() );
	ON_ERROR_RETURN( result );
	return rval;
}

int
REMOTE_CONDOR_whoami(int length, void *buffer)
{
	int rval = -1, result = 0;
	condor_errno_t terrno;

	dprintf ( D_SYSCALLS, "Doing CONDOR_whoami\n" );

	CurrentSysCall = CONDOR_whoami;

	syscall_sock->encode();
	result = ( syscall_sock->code(CurrentSysCall) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->code(length) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->end_of_message() );
	ON_ERROR_RETURN( result );

	syscall_sock->decode();
	result = ( syscall_sock->code(rval) );
	ON_ERROR_RETURN( result );
	if( rval < 0 ) {
		result = ( syscall_sock->code(terrno) );
		ON_ERROR_RETURN( result );
		result = ( syscall_sock->end_of_message() );
		ON_ERROR_RETURN( result );
		errno = terrno;
		dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
		return rval;
	}
	result = ( syscall_sock->code_bytes_bool(buffer, rval) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->end_of_message() );
	ON_ERROR_RETURN( result );
	return rval;
}

int
REMOTE_CONDOR_whoareyou(char *host, int length, void *buffer)
{
	int rval = -1, result = 0;
	condor_errno_t terrno;

	dprintf ( D_SYSCALLS, "Doing CONDOR_whoareyou\n" );

	CurrentSysCall = CONDOR_whoareyou;

	syscall_sock->encode();
	result = ( syscall_sock->code(CurrentSysCall) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->code(host) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->code(length) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->end_of_message() );
	ON_ERROR_RETURN( result );

	syscall_sock->decode();
	result = ( syscall_sock->code(rval) );
	ON_ERROR_RETURN( result );
	if( rval < 0 ) {
		result = ( syscall_sock->code(terrno) );
		ON_ERROR_RETURN( result );
		result = ( syscall_sock->end_of_message() );
		ON_ERROR_RETURN( result );
		errno = terrno;
		dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
		return rval;
	}
	result = ( syscall_sock->code_bytes_bool(buffer, rval) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->end_of_message() );
	ON_ERROR_RETURN( result );
	return rval;
}

int
REMOTE_CONDOR_fstat(int fd, char* buffer)
{
	int rval = -1, result = 0;
	condor_errno_t terrno;

	dprintf ( D_SYSCALLS, "Doing CONDOR_fstat\n" );

	CurrentSysCall = CONDOR_fstat;

	syscall_sock->encode();
	result = ( syscall_sock->code(CurrentSysCall) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->code(fd) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->end_of_message() );
	ON_ERROR_RETURN( result );

	syscall_sock->decode();
	result = ( syscall_sock->code(rval) );
	ON_ERROR_RETURN( result );
	if( rval < 0 ) {
		result = ( syscall_sock->code(terrno) );
		ON_ERROR_RETURN( result );
		result = ( syscall_sock->end_of_message() );
		ON_ERROR_RETURN( result );
		errno = terrno;
		dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
		return rval;
	}
	result = ( syscall_sock->code_bytes_bool(buffer, 1024) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->end_of_message() );
	ON_ERROR_RETURN( result );
	return rval;
}

int
REMOTE_CONDOR_fstatfs(int fd, char* buffer)
{
	int rval = -1, result = 0;
	condor_errno_t terrno;

	dprintf ( D_SYSCALLS, "Doing CONDOR_fstatfs\n" );

	CurrentSysCall = CONDOR_fstat;

	syscall_sock->encode();
	result = ( syscall_sock->code(CurrentSysCall) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->code(fd) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->end_of_message() );
	ON_ERROR_RETURN( result );

	syscall_sock->decode();
	result = ( syscall_sock->code(rval) );
	ON_ERROR_RETURN( result );
	if( rval < 0 ) {
		result = ( syscall_sock->code(terrno) );
		ON_ERROR_RETURN( result );
		result = ( syscall_sock->end_of_message() );
		ON_ERROR_RETURN( result );
		errno = terrno;
		dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
		return rval;
	}
	result = ( syscall_sock->code_bytes_bool(buffer, 1024) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->end_of_message() );
	ON_ERROR_RETURN( result );
	return rval;
}

int
REMOTE_CONDOR_fchown(int fd, int uid, int gid)
{
	int rval = -1, result = 0;
	condor_errno_t terrno;

	dprintf ( D_SYSCALLS, "Doing CONDOR_fchown\n" );

	CurrentSysCall = CONDOR_fchown;

	syscall_sock->encode();
	result = ( syscall_sock->code(CurrentSysCall) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->code(fd) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->code(uid) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->code(gid) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->end_of_message() );
	ON_ERROR_RETURN( result );

	syscall_sock->decode();
	result = ( syscall_sock->code(rval) );
	ON_ERROR_RETURN( result );
	if( rval < 0 ) {
		result = ( syscall_sock->code(terrno) );
		ON_ERROR_RETURN( result );
		result = ( syscall_sock->end_of_message() );
		ON_ERROR_RETURN( result );
		errno = terrno;
		dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
		return rval;
	}
	result = ( syscall_sock->end_of_message() );
	ON_ERROR_RETURN( result );
	return rval;
}

int
REMOTE_CONDOR_fchmod(int fd, int mode)
{
	int rval = -1, result = 0;
	condor_errno_t terrno;

	dprintf ( D_SYSCALLS, "Doing CONDOR_fchmod\n" );

	CurrentSysCall = CONDOR_fchmod;

	syscall_sock->encode();
	result = ( syscall_sock->code(CurrentSysCall) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->code(fd) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->code(mode) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->end_of_message() );
	ON_ERROR_RETURN( result );

	syscall_sock->decode();
	result = ( syscall_sock->code(rval) );
	ON_ERROR_RETURN( result );
	if( rval < 0 ) {
		result = ( syscall_sock->code(terrno) );
		ON_ERROR_RETURN( result );
		result = ( syscall_sock->end_of_message() );
		ON_ERROR_RETURN( result );
		errno = terrno;
		dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
		return rval;
	}
	result = ( syscall_sock->end_of_message() );
	ON_ERROR_RETURN( result );
	return rval;
}

int
REMOTE_CONDOR_ftruncate(int fd, int length)
{
	int rval = -1, result = 0;
	condor_errno_t terrno;

	dprintf ( D_SYSCALLS, "Doing CONDOR_truncate\n" );

	CurrentSysCall = CONDOR_ftruncate;

	syscall_sock->encode();
	result = ( syscall_sock->code(CurrentSysCall) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->code(fd) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->code(length) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->end_of_message() );
	ON_ERROR_RETURN( result );

	syscall_sock->decode();
	result = ( syscall_sock->code(rval) );
	ON_ERROR_RETURN( result );
	if( rval < 0 ) {
		result = ( syscall_sock->code(terrno) );
		ON_ERROR_RETURN( result );
		result = ( syscall_sock->end_of_message() );
		ON_ERROR_RETURN( result );
		errno = terrno;
		dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
		return rval;
	}
	result = ( syscall_sock->end_of_message() );
	ON_ERROR_RETURN( result );
	return rval;
}





int
REMOTE_CONDOR_putfile_buffer(void *buffer, int length)
{
	int rval = -1, result = 0;
	condor_errno_t terrno;
	
	dprintf ( D_SYSCALLS, "Doing CONDOR_putfile_buffer\n" );
	
	syscall_sock->encode();
	result = ( syscall_sock->code_bytes_bool(buffer, length) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->end_of_message() );
	ON_ERROR_RETURN( result );
	
	syscall_sock->decode();
	result = ( syscall_sock->code(rval) );
	ON_ERROR_RETURN( result );
	if( rval < 0 ) {
		result = ( syscall_sock->code(terrno) );
		ON_ERROR_RETURN( result );
		errno = terrno;
		dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
	}
	result = ( syscall_sock->end_of_message() );
	ON_ERROR_RETURN( result );
	return rval;
}

int
REMOTE_CONDOR_link(char *path, char *newpath)
{
	int rval = -1, result = 0;
	condor_errno_t terrno;

	dprintf ( D_SYSCALLS, "Doing CONDOR_link\n" );

	CurrentSysCall = CONDOR_link;

	syscall_sock->encode();
	result = ( syscall_sock->code(CurrentSysCall) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->code(path) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->code(newpath) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->end_of_message() );
	ON_ERROR_RETURN( result );

	syscall_sock->decode();
	result = ( syscall_sock->code(rval) );
	ON_ERROR_RETURN( result );
	if( rval < 0 ) {
		result = ( syscall_sock->code(terrno) );
		ON_ERROR_RETURN( result );
		result = ( syscall_sock->end_of_message() );
		ON_ERROR_RETURN( result );
		errno = terrno;
		dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
		return rval;
	}
	result = ( syscall_sock->end_of_message() );
	ON_ERROR_RETURN( result );
	return rval;
}

int
REMOTE_CONDOR_symlink(char *path, char *newpath)
{
	int rval = -1, result = 0;
	condor_errno_t terrno;

	dprintf ( D_SYSCALLS, "Doing CONDOR_symlink\n" );

	CurrentSysCall = CONDOR_symlink;

	syscall_sock->encode();
	result = ( syscall_sock->code(CurrentSysCall) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->code(path) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->code(newpath) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->end_of_message() );
	ON_ERROR_RETURN( result );

	syscall_sock->decode();
	result = ( syscall_sock->code(rval) );
	ON_ERROR_RETURN( result );
	if( rval < 0 ) {
		result = ( syscall_sock->code(terrno) );
		ON_ERROR_RETURN( result );
		result = ( syscall_sock->end_of_message() );
		ON_ERROR_RETURN( result );
		errno = terrno;
		dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
		return rval;
	}
	result = ( syscall_sock->end_of_message() );
	ON_ERROR_RETURN( result );
	return rval;
}

int
REMOTE_CONDOR_readlink(char *path, int length, char **buffer )
{
	int rval = -1, result = 0;
	condor_errno_t terrno;

	dprintf ( D_SYSCALLS, "Doing CONDOR_readlink\n" );

	CurrentSysCall = CONDOR_readlink;

	syscall_sock->encode();
	result = ( syscall_sock->code(CurrentSysCall) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->code(path) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->code(length) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->end_of_message() );
	ON_ERROR_RETURN( result );

	syscall_sock->decode();
	result = ( syscall_sock->code(rval) );
	ON_ERROR_RETURN( result );
	if( rval < 0 ) {
		result = ( syscall_sock->code(terrno) );
		ON_ERROR_RETURN( result );
		result = ( syscall_sock->end_of_message() );
		ON_ERROR_RETURN( result );
		errno = terrno;
		dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
		return rval;
	}
	*buffer = (char*)malloc(rval);
	result = ( syscall_sock->code_bytes_bool(*buffer, rval) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->end_of_message() );
	ON_ERROR_RETURN( result );
	return rval;
}

int
REMOTE_CONDOR_stat(char *path, char *buffer)
{
	int rval = -1, result = 0;
	condor_errno_t terrno;

	dprintf ( D_SYSCALLS, "Doing CONDOR_stat\n" );

	CurrentSysCall = CONDOR_stat;

	syscall_sock->encode();
	result = ( syscall_sock->code(CurrentSysCall) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->code(path) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->end_of_message() );
	ON_ERROR_RETURN( result );

	syscall_sock->decode();
	result = ( syscall_sock->code(rval) );
	ON_ERROR_RETURN( result );
	if( rval == -1 ) {
		result = ( syscall_sock->code(terrno) );
		ON_ERROR_RETURN( result );
		result = ( syscall_sock->end_of_message() );
		ON_ERROR_RETURN( result );
		errno = terrno;
		dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
		return rval;
	}
	result = ( syscall_sock->code_bytes_bool(buffer, 1024) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->end_of_message() );
	ON_ERROR_RETURN( result );
	return rval;
}

int
REMOTE_CONDOR_lstat(char *path, char *buffer)
{
	int rval = -1, result = 0;
	condor_errno_t terrno;

	dprintf ( D_SYSCALLS, "Doing CONDOR_lstat\n" );

	CurrentSysCall = CONDOR_lstat;

	syscall_sock->encode();
	result = ( syscall_sock->code(CurrentSysCall) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->code(path) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->end_of_message() );
	ON_ERROR_RETURN( result );

	syscall_sock->decode();
	result = ( syscall_sock->code(rval) );
	ON_ERROR_RETURN( result );
	if( rval < 0 ) {
		result = ( syscall_sock->code(terrno) );
		ON_ERROR_RETURN( result );
		result = ( syscall_sock->end_of_message() );
		ON_ERROR_RETURN( result );
		errno = terrno;
		dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
		return rval;
	}
	result = ( syscall_sock->code_bytes_bool(buffer, 1024) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->end_of_message() );
	ON_ERROR_RETURN( result );
	return rval;
}

int
REMOTE_CONDOR_statfs(char *path, char *buffer)
{
	int rval = -1, result = 0;
	condor_errno_t terrno;

	dprintf ( D_SYSCALLS, "Doing CONDOR_statfs\n" );

	CurrentSysCall = CONDOR_statfs;

	syscall_sock->encode();
	result = ( syscall_sock->code(CurrentSysCall) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->code(path) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->end_of_message() );
	ON_ERROR_RETURN( result );

	syscall_sock->decode();
	result = ( syscall_sock->code(rval) );
	ON_ERROR_RETURN( result );
	if( rval < 0 ) {
		result = ( syscall_sock->code(terrno) );
		ON_ERROR_RETURN( result );
		result = ( syscall_sock->end_of_message() );
		ON_ERROR_RETURN( result );
		errno = terrno;
		dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
		return rval;
	}
	result = ( syscall_sock->code_bytes_bool(buffer, 1024) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->end_of_message() );
	ON_ERROR_RETURN( result );
	return rval;
}

int
REMOTE_CONDOR_access(char *path, int mode)
{
	int rval = -1, result = 0;
	condor_errno_t terrno;

	dprintf ( D_SYSCALLS, "Doing CONDOR_access\n" );

	CurrentSysCall = CONDOR_access;

	syscall_sock->encode();
	result = ( syscall_sock->code(CurrentSysCall) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->code(path) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->code(mode) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->end_of_message() );
	ON_ERROR_RETURN( result );

	syscall_sock->decode();
	result = ( syscall_sock->code(rval) );
	ON_ERROR_RETURN( result );
	if( rval < 0 ) {
		result = ( syscall_sock->code(terrno) );
		ON_ERROR_RETURN( result );
		result = ( syscall_sock->end_of_message() );
		ON_ERROR_RETURN( result );
		errno = terrno;
		dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
		return rval;
	}
	result = ( syscall_sock->end_of_message() );
	ON_ERROR_RETURN( result );
	return rval;
}

int
REMOTE_CONDOR_chmod(char *path, int mode)
{
	int rval = -1, result = 0;
	condor_errno_t terrno;

	dprintf ( D_SYSCALLS, "Doing CONDOR_chmod\n" );

	CurrentSysCall = CONDOR_chmod;

	syscall_sock->encode();
	result = ( syscall_sock->code(CurrentSysCall) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->code(path) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->code(mode) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->end_of_message() );
	ON_ERROR_RETURN( result );

	syscall_sock->decode();
	result = ( syscall_sock->code(rval) );
	ON_ERROR_RETURN( result );
	if( rval < 0 ) {
		result = ( syscall_sock->code(terrno) );
		ON_ERROR_RETURN( result );
		result = ( syscall_sock->end_of_message() );
		ON_ERROR_RETURN( result );
		errno = terrno;
		dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
		return rval;
	}
	result = ( syscall_sock->end_of_message() );
	ON_ERROR_RETURN( result );
	return rval;
}

int
REMOTE_CONDOR_chown(char *path, int uid, int gid)
{
	int rval = -1, result = 0;
	condor_errno_t terrno;

	dprintf ( D_SYSCALLS, "Doing CONDOR_chown\n" );

	CurrentSysCall = CONDOR_chown;

	syscall_sock->encode();
	result = ( syscall_sock->code(CurrentSysCall) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->code(path) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->code(uid) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->code(gid) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->end_of_message() );
	ON_ERROR_RETURN( result );

	syscall_sock->decode();
	result = ( syscall_sock->code(rval) );
	ON_ERROR_RETURN( result );
	if( rval < 0 ) {
		result = ( syscall_sock->code(terrno) );
		ON_ERROR_RETURN( result );
		result = ( syscall_sock->end_of_message() );
		ON_ERROR_RETURN( result );
		errno = terrno;
		dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
		return rval;
	}
	result = ( syscall_sock->end_of_message() );
	ON_ERROR_RETURN( result );
	return rval;
}

int
REMOTE_CONDOR_lchown(char *path, int uid, int gid)
{
	int rval = -1, result = 0;
	condor_errno_t terrno;

	dprintf ( D_SYSCALLS, "Doing CONDOR_lchown\n" );

	CurrentSysCall = CONDOR_lchown;

	syscall_sock->encode();
	result = ( syscall_sock->code(CurrentSysCall) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->code(path) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->code(uid) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->code(gid) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->end_of_message() );
	ON_ERROR_RETURN( result );

	syscall_sock->decode();
	result = ( syscall_sock->code(rval) );
	ON_ERROR_RETURN( result );
	if( rval < 0 ) {
		result = ( syscall_sock->code(terrno) );
		ON_ERROR_RETURN( result );
		result = ( syscall_sock->end_of_message() );
		ON_ERROR_RETURN( result );
		errno = terrno;
		dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
		return rval;
	}
	result = ( syscall_sock->end_of_message() );
	ON_ERROR_RETURN( result );
	return rval;
}

int
REMOTE_CONDOR_truncate(char *path, int length)
{
	int rval = -1, result = 0;
	condor_errno_t terrno;

	dprintf ( D_SYSCALLS, "Doing CONDOR_truncate\n" );

	CurrentSysCall = CONDOR_truncate;

	syscall_sock->encode();
	result = ( syscall_sock->code(CurrentSysCall) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->code(path) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->code(length) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->end_of_message() );
	ON_ERROR_RETURN( result );

	syscall_sock->decode();
	result = ( syscall_sock->code(rval) );
	ON_ERROR_RETURN( result );
	if( rval < 0 ) {
		result = ( syscall_sock->code(terrno) );
		ON_ERROR_RETURN( result );
		result = ( syscall_sock->end_of_message() );
		ON_ERROR_RETURN( result );
		errno = terrno;
		dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
		return rval;
	}
	result = ( syscall_sock->end_of_message() );
	ON_ERROR_RETURN( result );
	return rval;
}

int
REMOTE_CONDOR_utime(char *path, int actime, int modtime)
{
	int rval = -1, result = 0;
	condor_errno_t terrno;

	dprintf ( D_SYSCALLS, "Doing CONDOR_utime\n" );
	
	CurrentSysCall = CONDOR_utime;

	syscall_sock->encode();
	result = ( syscall_sock->code(CurrentSysCall) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->code(path) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->code(actime) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->code(modtime) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->end_of_message() );
	ON_ERROR_RETURN( result );

	syscall_sock->decode();
	result = ( syscall_sock->code(rval) );
	ON_ERROR_RETURN( result );
	if( rval < 0 ) {
		result = ( syscall_sock->code(terrno) );
		ON_ERROR_RETURN( result );
		result = ( syscall_sock->end_of_message() );
		ON_ERROR_RETURN( result );
		errno = terrno;
		dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
		return rval;
	}
	result = ( syscall_sock->end_of_message() );
	ON_ERROR_RETURN( result );
	return rval;
}

} // extern "C"
