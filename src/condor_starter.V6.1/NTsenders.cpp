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
#include "condor_config.h"
#include "secure_file.h"
#include "zkm_base64.h"

#include "NTsenders.h"

#define ON_ERROR_RETURN(x) if (x <= 0) {dprintf(D_ALWAYS, "i/o error result is %d, errno is %d\n", x, errno);errno=ETIMEDOUT;return -1;}

static int CurrentSysCall;
extern ReliSock *syscall_sock;
extern Starter *Starter;

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

	if( ! syscall_sock->is_connected() ) {
		dprintf(D_ALWAYS, "RPC error: disconnected from shadow\n");
		errno = ETIMEDOUT;
		return -1;
	}

	syscall_sock->encode();
	result = syscall_sock->code(CurrentSysCall);
	ON_ERROR_RETURN( result );
	result = putClassAd(syscall_sock, *ad);
	ON_ERROR_RETURN( result );
	result = syscall_sock->end_of_message();
	ON_ERROR_RETURN( result );

	syscall_sock->decode();
	result =  syscall_sock->code(rval);
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
		dprintf(D_ALWAYS, "RPC error: disconnected from shadow\n");
		errno = ETIMEDOUT;
		return -1;
	}

	syscall_sock->encode();
	ON_ERROR_RETURN( syscall_sock->code(CurrentSysCall) );
	ON_ERROR_RETURN( putClassAd(syscall_sock, *ad) );
	ON_ERROR_RETURN( syscall_sock->end_of_message() );

	syscall_sock->decode();
	ON_ERROR_RETURN( syscall_sock->code(rval) );
	if( rval < 0 ) {
		ON_ERROR_RETURN( syscall_sock->code(terrno) );
		ON_ERROR_RETURN( syscall_sock->end_of_message() );
		errno = terrno;
		dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
		return rval;
	}
	ON_ERROR_RETURN( syscall_sock->end_of_message() );
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

	if( ! syscall_sock->is_connected() ) {
		dprintf(D_ALWAYS, "RPC error: disconnected from shadow\n");
		errno = ETIMEDOUT;
		return -1;
	}

	syscall_sock->encode();
	result = syscall_sock->code(CurrentSysCall);
	ON_ERROR_RETURN( result );
	result = syscall_sock->end_of_message();
	ON_ERROR_RETURN( result );


	syscall_sock->decode();
	result = syscall_sock->code(rval);
	ON_ERROR_RETURN( result );
	dprintf(D_SYSCALLS, "\trval = %d\n", rval);
	if( rval < 0 ) {
		result = syscall_sock->code(terrno);
		ON_ERROR_RETURN( result );
		result = syscall_sock->end_of_message();
		ON_ERROR_RETURN( result );
		errno = terrno;
		dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
		return rval;
	}
	result = getClassAd(syscall_sock, *ad);
	ON_ERROR_RETURN( result );
	result = syscall_sock->end_of_message();
	ON_ERROR_RETURN( result );
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

	if( ! syscall_sock->is_connected() ) {
		dprintf(D_ALWAYS, "RPC error: disconnected from shadow\n");
		errno = ETIMEDOUT;
		return -1;
	}

	syscall_sock->encode();
	result = syscall_sock->code(CurrentSysCall);
	ON_ERROR_RETURN( result );
	result = syscall_sock->end_of_message();
	ON_ERROR_RETURN( result );

	syscall_sock->decode();
	result = syscall_sock->code(rval);
	ON_ERROR_RETURN( result );
	if( rval < 0 ) {
		result = syscall_sock->code(terrno);
		ON_ERROR_RETURN( result );
		result =syscall_sock->end_of_message();
		ON_ERROR_RETURN( result );
		errno = terrno;
		dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
		return rval;
	}
	result = getClassAd(syscall_sock, *ad);
	ON_ERROR_RETURN( result );
	result = syscall_sock->end_of_message();
	ON_ERROR_RETURN( result );
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

	if( ! syscall_sock->is_connected() ) {
		dprintf(D_ALWAYS, "RPC error: disconnected from shadow\n");
		errno = ETIMEDOUT;
		return -1;
	}

	syscall_sock->encode();
	result = syscall_sock->code(CurrentSysCall);
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

	result = ( syscall_sock->get_file(destination) > -1 );
	ON_ERROR_RETURN( result );
	result = syscall_sock->end_of_message();
	ON_ERROR_RETURN( result );
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
		dprintf(D_ALWAYS, "RPC error: disconnected from shadow\n");
		errno = ETIMEDOUT;
		return -1;
	}

	syscall_sock->encode();
	ON_ERROR_RETURN( syscall_sock->code(CurrentSysCall) );
	ON_ERROR_RETURN( syscall_sock->code(status) );
	ON_ERROR_RETURN( syscall_sock->code(reason) );
	if ( ad ) {
		ON_ERROR_RETURN( putClassAd(syscall_sock, *ad) );
	}
	ON_ERROR_RETURN( syscall_sock->end_of_message() );
	syscall_sock->decode();
	ON_ERROR_RETURN( syscall_sock->code(rval) );
	if( rval < 0 ) {
		ON_ERROR_RETURN( syscall_sock->code(terrno) );
		ON_ERROR_RETURN( syscall_sock->end_of_message() );
		errno = terrno;
		dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
		return rval;
	}
	ON_ERROR_RETURN( syscall_sock->end_of_message() );
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

	if( ! syscall_sock->is_connected() ) {
		dprintf(D_ALWAYS, "RPC error: disconnected from shadow\n");
		errno = ETIMEDOUT;
		return -1;
	}

	syscall_sock->encode();
	result = syscall_sock->code(CurrentSysCall);
	ON_ERROR_RETURN( result );
	result = putClassAd(syscall_sock, *ad);
	ON_ERROR_RETURN( result );
	result = syscall_sock->end_of_message();
	ON_ERROR_RETURN( result );

	syscall_sock->decode();
	result =  syscall_sock->code(rval);
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
REMOTE_CONDOR_begin_execution( void )
{
	condor_errno_t		terrno;
	int		rval=-1;
	int result = 0;

	dprintf ( D_SYSCALLS, "Doing CONDOR_begin_execution\n" );

	CurrentSysCall = CONDOR_begin_execution;

	if( ! syscall_sock->is_connected() ) {
		dprintf(D_ALWAYS, "RPC error: disconnected from shadow\n");
		errno = ETIMEDOUT;
		return -1;
	}

	syscall_sock->encode();
	result = syscall_sock->code(CurrentSysCall);
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
REMOTE_CONDOR_open( char const *  path , open_flags_t flags , int   lastarg)
{
        int     rval;
        condor_errno_t     terrno;
		int result = 0;

        dprintf ( D_SYSCALLS, "Doing CONDOR_open\n" );

        CurrentSysCall = CONDOR_open;

        if( ! syscall_sock->is_connected() ) {
                dprintf(D_ALWAYS, "RPC error: disconnected from shadow\n");
                errno = ETIMEDOUT;
                return -1;
        }

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

        if( ! syscall_sock->is_connected() ) {
                dprintf(D_ALWAYS, "RPC error: disconnected from shadow\n");
                errno = ETIMEDOUT;
	            return -1;
        }

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

        if( ! syscall_sock->is_connected() ) {
                dprintf(D_ALWAYS, "RPC error: disconnected from shadow\n");
                errno = ETIMEDOUT;
                return -1;
        }

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

        if( ! syscall_sock->is_connected() ) {
                dprintf(D_ALWAYS, "RPC error: disconnected from shadow\n");
                errno = ETIMEDOUT;
                return -1;
        }

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


off_t
REMOTE_CONDOR_lseek(int   fd , off_t   offset , int   whence)
{
        off_t     rval;
        condor_errno_t     terrno;
		int result = 0;

        dprintf ( D_SYSCALLS, "Doing CONDOR_lseek\n" );

        CurrentSysCall = CONDOR_lseek;

        if( ! syscall_sock->is_connected() ) {
                dprintf(D_ALWAYS, "RPC error: disconnected from shadow\n");
                errno = ETIMEDOUT;
                return -1;
        }

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

        if( ! syscall_sock->is_connected() ) {
                dprintf(D_ALWAYS, "RPC error: disconnected from shadow\n");
                errno = ETIMEDOUT;
                return -1;
        }

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
REMOTE_CONDOR_rename( char *  from , char *  to)
{
        int     rval;
        condor_errno_t     terrno;
		int result = 0;

        dprintf ( D_SYSCALLS, "Doing CONDOR_rename\n" );

        CurrentSysCall = CONDOR_rename;

        if( ! syscall_sock->is_connected() ) {
                dprintf(D_ALWAYS, "RPC error: disconnected from shadow\n");
                errno = ETIMEDOUT;
                return -1;
        }

        syscall_sock->encode();
        result = ( syscall_sock->code(CurrentSysCall) );
		ON_ERROR_RETURN( result );
        result = ( syscall_sock->code(from) );
		ON_ERROR_RETURN( result );
        result = ( syscall_sock->code(to) );
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

	if( ! syscall_sock->is_connected() ) {
		dprintf(D_ALWAYS, "RPC error: disconnected from shadow\n");
		errno = ETIMEDOUT;
		return -1;
	}

	syscall_sock->encode();
	result = ( syscall_sock->code(CurrentSysCall) );
	ON_ERROR_RETURN( result );
	result = ( putClassAd(syscall_sock, *ad) );
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
REMOTE_CONDOR_mkdir( char *  path, int mode )
{
        int     rval;
        condor_errno_t     terrno;
		int result = 0;

        dprintf ( D_SYSCALLS, "Doing CONDOR_mkdir\n" );

        CurrentSysCall = CONDOR_mkdir;

        if( ! syscall_sock->is_connected() ) {
                dprintf(D_ALWAYS, "RPC error: disconnected from shadow\n");
                errno = ETIMEDOUT;
                return -1;
        }

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
REMOTE_CONDOR_rmdir( char *  path )
{
        int     rval;
        condor_errno_t     terrno;
		int result = 0;

        dprintf ( D_SYSCALLS, "Doing CONDOR_rmdir\n" );

        CurrentSysCall = CONDOR_rmdir;

        if( ! syscall_sock->is_connected() ) {
                dprintf(D_ALWAYS, "RPC error: disconnected from shadow\n");
                errno = ETIMEDOUT;
                return -1;
        }

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
REMOTE_CONDOR_fsync(int   fd)
{
        int     rval;
        condor_errno_t     terrno;
		int result = 0;

        dprintf ( D_SYSCALLS, "Doing CONDOR_fsync\n" );

        CurrentSysCall = CONDOR_fsync;

        if( ! syscall_sock->is_connected() ) {
                dprintf(D_ALWAYS, "RPC error: disconnected from shadow\n");
                errno = ETIMEDOUT;
                return -1;
        }

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
 
        if( ! syscall_sock->is_connected() ) {
                dprintf(D_ALWAYS, "RPC error: disconnected from shadow\n");
                errno = ETIMEDOUT;
                return -1;
        }

        syscall_sock->encode();
        ON_ERROR_RETURN( syscall_sock->code(CurrentSysCall) );
        ON_ERROR_RETURN( syscall_sock->code(logical_name) );
        ON_ERROR_RETURN( syscall_sock->end_of_message() );
 
        syscall_sock->decode();
        ON_ERROR_RETURN( syscall_sock->code(rval) );
        if( rval < 0 ) {
                ON_ERROR_RETURN( syscall_sock->code(terrno) );
                ON_ERROR_RETURN( syscall_sock->end_of_message() );
                errno = (int)terrno;
                dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
                return rval;
        }
        ON_ERROR_RETURN( syscall_sock->get(actual_url) );
        ON_ERROR_RETURN( syscall_sock->end_of_message() );

        return rval;
}                                                                              

int REMOTE_CONDOR_ulog_error_printf( int hold_reason_code, int hold_reason_subcode, char const *str, ... )
{
	std::string buf;
	va_list args;
	int retval;

	va_start(args,str);
	vformatstr(buf, str, args);
	retval = REMOTE_CONDOR_ulog_error( hold_reason_code, hold_reason_subcode, buf.c_str() );
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
	ad = event.toClassAd(true);
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

	if( ! syscall_sock->is_connected() ) {
		dprintf(D_ALWAYS, "RPC error: disconnected from shadow\n");
		errno = ETIMEDOUT;
		return -1;
	}

	syscall_sock->encode();
	result = syscall_sock->code(CurrentSysCall);
	ON_ERROR_RETURN( result );
	result = putClassAd(syscall_sock, *ad);
	ON_ERROR_RETURN( result );
	result = syscall_sock->end_of_message();
	ON_ERROR_RETURN( result );

	//NOTE: we expect no response.

	return 0;
}

int
REMOTE_CONDOR_phase( char *phase )
{
	int result = 0;

	dprintf ( D_SYSCALLS, "Doing CONDOR_phase\n" );

	CurrentSysCall = CONDOR_phase;

	if( ! phase ) {
		EXCEPT( "CONDOR_phase called with NULL phase!" );
		return -1;
	}

	if( ! syscall_sock->is_connected() ) {
		dprintf(D_ALWAYS, "RPC error: disconnected from shadow\n");
		errno = ETIMEDOUT;
		return -1;
	}

	syscall_sock->encode();
	result = syscall_sock->code(CurrentSysCall);
	ON_ERROR_RETURN( result );
	result = syscall_sock->code(phase);
	ON_ERROR_RETURN( result );
	result = syscall_sock->end_of_message();
	ON_ERROR_RETURN( result );

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

	if( ! syscall_sock->is_connected() ) {
		dprintf(D_ALWAYS, "RPC error: disconnected from shadow\n");
		errno = ETIMEDOUT;
		return -1;
	}

	syscall_sock->encode();
	ON_ERROR_RETURN( syscall_sock->code(CurrentSysCall) );
	ON_ERROR_RETURN( syscall_sock->code(attrname) );
	ON_ERROR_RETURN( syscall_sock->end_of_message() );

	syscall_sock->decode();
	ON_ERROR_RETURN( syscall_sock->code(rval) );
	if( rval < 0 ) {
		ON_ERROR_RETURN( syscall_sock->code(terrno) );
		ON_ERROR_RETURN( syscall_sock->end_of_message() );
		errno = (int)terrno;
		dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
		return rval;
	}
	ON_ERROR_RETURN( syscall_sock->code(expr) );
	ON_ERROR_RETURN( syscall_sock->end_of_message() );
	return rval;
}

int
REMOTE_CONDOR_set_job_attr(char *  attrname , char *  expr)
{
	int	rval;
	condor_errno_t	terrno;

	dprintf ( D_SYSCALLS, "Doing CONDOR_set_job_attr\n" );

	CurrentSysCall = CONDOR_set_job_attr;

	if( ! syscall_sock->is_connected() ) {
		dprintf(D_ALWAYS, "RPC error: disconnected from shadow\n");
		errno = ETIMEDOUT;
		return -1;
	}

	syscall_sock->encode();
	ON_ERROR_RETURN( syscall_sock->code(CurrentSysCall) );
	ON_ERROR_RETURN( syscall_sock->code(expr) );
	ON_ERROR_RETURN( syscall_sock->code(attrname) );
	ON_ERROR_RETURN( syscall_sock->end_of_message() );

	syscall_sock->decode();
	ON_ERROR_RETURN( syscall_sock->code(rval) );
	if( rval < 0 ) {
		ON_ERROR_RETURN( syscall_sock->code(terrno) );
		ON_ERROR_RETURN( syscall_sock->end_of_message() );
		errno = (int)terrno;
		dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
		return rval;
	}
	ON_ERROR_RETURN( syscall_sock->end_of_message() );
	return rval;
}

int
REMOTE_CONDOR_constrain( char *  expr)
{
	int	rval;
	condor_errno_t	terrno;

	dprintf ( D_SYSCALLS, "Doing CONDOR_constrain\n" );

	CurrentSysCall = CONDOR_constrain;

	if( ! syscall_sock->is_connected() ) {
		dprintf(D_ALWAYS, "RPC error: disconnected from shadow\n");
		errno = ETIMEDOUT;
		return -1;
	}

	syscall_sock->encode();
	ON_ERROR_RETURN( syscall_sock->code(CurrentSysCall) );
	ON_ERROR_RETURN( syscall_sock->code(expr) );
	ON_ERROR_RETURN( syscall_sock->end_of_message() );

	syscall_sock->decode();
	ON_ERROR_RETURN( syscall_sock->code(rval) );
	if( rval < 0 ) {
		ON_ERROR_RETURN( syscall_sock->code(terrno) );
		ON_ERROR_RETURN( syscall_sock->end_of_message() );
		errno = (int)terrno;
		dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
		return rval;
	}
	ON_ERROR_RETURN( syscall_sock->end_of_message() );
	return rval;
}

int REMOTE_CONDOR_get_sec_session_info(
	char const *starter_reconnect_session_info,
	std::string &reconnect_session_id,
	std::string &reconnect_session_info,
	std::string &reconnect_session_key,
	char const *starter_filetrans_session_info,
	std::string &filetrans_session_id,
	std::string &filetrans_session_info,
	std::string &filetrans_session_key)
{
	int	rval=-1;
	condor_errno_t	terrno;

	dprintf ( D_SYSCALLS, "Doing CONDOR_get_sec_session_info\n" );

	CurrentSysCall = CONDOR_get_sec_session_info;

	if( ! syscall_sock->is_connected() ) {
		dprintf(D_ALWAYS, "RPC error: disconnected from shadow\n");
		errno = ETIMEDOUT;
		return -1;
	}

	syscall_sock->encode();
	ON_ERROR_RETURN( syscall_sock->code(CurrentSysCall) );

	bool socket_default_crypto = syscall_sock->get_encryption();
	if( !socket_default_crypto ) {
		// always encrypt; we are exchanging super secret session keys
		syscall_sock->set_crypto_mode(true);
	}

	ON_ERROR_RETURN( syscall_sock->put(starter_reconnect_session_info) );
	ON_ERROR_RETURN( syscall_sock->put(starter_filetrans_session_info) );
	ON_ERROR_RETURN( syscall_sock->end_of_message() );

	syscall_sock->decode();
	ON_ERROR_RETURN( syscall_sock->code(rval) );
	if( rval < 0 ) {
		ON_ERROR_RETURN( syscall_sock->code(terrno) );
		ON_ERROR_RETURN( syscall_sock->end_of_message() );
		errno = (int)terrno;
	}
	else {
		ON_ERROR_RETURN( syscall_sock->code(reconnect_session_id) );
		ON_ERROR_RETURN( syscall_sock->code(reconnect_session_info) );
		ON_ERROR_RETURN( syscall_sock->code(reconnect_session_key) );

		ON_ERROR_RETURN( syscall_sock->code(filetrans_session_id) );
		ON_ERROR_RETURN( syscall_sock->code(filetrans_session_info) );
		ON_ERROR_RETURN( syscall_sock->code(filetrans_session_key) );

		ON_ERROR_RETURN( syscall_sock->end_of_message() );
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

	if( ! syscall_sock->is_connected() ) {
		dprintf(D_ALWAYS, "RPC error: disconnected from shadow\n");
		errno = ETIMEDOUT;
		return -1;
	}

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

	if( ! syscall_sock->is_connected() ) {
		dprintf(D_ALWAYS, "RPC error: disconnected from shadow\n");
		errno = ETIMEDOUT;
		return -1;
	}

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

	if( ! syscall_sock->is_connected() ) {
		dprintf(D_ALWAYS, "RPC error: disconnected from shadow\n");
		errno = ETIMEDOUT;
		return -1;
	}

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

	if( ! syscall_sock->is_connected() ) {
		dprintf(D_ALWAYS, "RPC error: disconnected from shadow\n");
		errno = ETIMEDOUT;
		return -1;
	}

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

	if( ! syscall_sock->is_connected() ) {
		dprintf(D_ALWAYS, "RPC error: disconnected from shadow\n");
		errno = ETIMEDOUT;
		return -1;
	}

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

	if( ! syscall_sock->is_connected() ) {
		dprintf(D_ALWAYS, "RPC error: disconnected from shadow\n");
		errno = ETIMEDOUT;
		return -1;
	}

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

	if( ! syscall_sock->is_connected() ) {
		dprintf(D_ALWAYS, "RPC error: disconnected from shadow\n");
		errno = ETIMEDOUT;
		return -1;
	}

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

	if( ! syscall_sock->is_connected() ) {
		dprintf(D_ALWAYS, "RPC error: disconnected from shadow\n");
		errno = ETIMEDOUT;
		return -1;
	}

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

	if( ! syscall_sock->is_connected() ) {
		dprintf(D_ALWAYS, "RPC error: disconnected from shadow\n");
		errno = ETIMEDOUT;
		return -1;
	}

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

	if( ! syscall_sock->is_connected() ) {
		dprintf(D_ALWAYS, "RPC error: disconnected from shadow\n");
		errno = ETIMEDOUT;
		return -1;
	}

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

	if( ! syscall_sock->is_connected() ) {
		dprintf(D_ALWAYS, "RPC error: disconnected from shadow\n");
		errno = ETIMEDOUT;
		return -1;
	}

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

	if( ! syscall_sock->is_connected() ) {
		dprintf(D_ALWAYS, "RPC error: disconnected from shadow\n");
		errno = ETIMEDOUT;
		return -1;
	}

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

	if( ! syscall_sock->is_connected() ) {
		dprintf(D_ALWAYS, "RPC error: disconnected from shadow\n");
		errno = ETIMEDOUT;
		return -1;
	}

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

	if( ! syscall_sock->is_connected() ) {
		dprintf(D_ALWAYS, "RPC error: disconnected from shadow\n");
		errno = ETIMEDOUT;
		return -1;
	}

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

	if( ! syscall_sock->is_connected() ) {
		dprintf(D_ALWAYS, "RPC error: disconnected from shadow\n");
		errno = ETIMEDOUT;
		return -1;
	}

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

	if( ! syscall_sock->is_connected() ) {
		dprintf(D_ALWAYS, "RPC error: disconnected from shadow\n");
		errno = ETIMEDOUT;
		return -1;
	}

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
	
	if( ! syscall_sock->is_connected() ) {
		dprintf(D_ALWAYS, "RPC error: disconnected from shadow\n");
		errno = ETIMEDOUT;
		return -1;
	}

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

	if( ! syscall_sock->is_connected() ) {
		dprintf(D_ALWAYS, "RPC error: disconnected from shadow\n");
		errno = ETIMEDOUT;
		return -1;
	}

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

	if( ! syscall_sock->is_connected() ) {
		dprintf(D_ALWAYS, "RPC error: disconnected from shadow\n");
		errno = ETIMEDOUT;
		return -1;
	}

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

	if( ! syscall_sock->is_connected() ) {
		dprintf(D_ALWAYS, "RPC error: disconnected from shadow\n");
		errno = ETIMEDOUT;
		return -1;
	}

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

	if( ! syscall_sock->is_connected() ) {
		dprintf(D_ALWAYS, "RPC error: disconnected from shadow\n");
		errno = ETIMEDOUT;
		return -1;
	}

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

	if( ! syscall_sock->is_connected() ) {
		dprintf(D_ALWAYS, "RPC error: disconnected from shadow\n");
		errno = ETIMEDOUT;
		return -1;
	}

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

	if( ! syscall_sock->is_connected() ) {
		dprintf(D_ALWAYS, "RPC error: disconnected from shadow\n");
		errno = ETIMEDOUT;
		return -1;
	}

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

	if( ! syscall_sock->is_connected() ) {
		dprintf(D_ALWAYS, "RPC error: disconnected from shadow\n");
		errno = ETIMEDOUT;
		return -1;
	}

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

	if( ! syscall_sock->is_connected() ) {
		dprintf(D_ALWAYS, "RPC error: disconnected from shadow\n");
		errno = ETIMEDOUT;
		return -1;
	}

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

	if( ! syscall_sock->is_connected() ) {
		dprintf(D_ALWAYS, "RPC error: disconnected from shadow\n");
		errno = ETIMEDOUT;
		return -1;
	}

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

	if( ! syscall_sock->is_connected() ) {
		dprintf(D_ALWAYS, "RPC error: disconnected from shadow\n");
		errno = ETIMEDOUT;
		return -1;
	}

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

	if( ! syscall_sock->is_connected() ) {
		dprintf(D_ALWAYS, "RPC error: disconnected from shadow\n");
		errno = ETIMEDOUT;
		return -1;
	}

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

	if( ! syscall_sock->is_connected() ) {
		dprintf(D_ALWAYS, "RPC error: disconnected from shadow\n");
		errno = ETIMEDOUT;
		return -1;
	}

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

int
REMOTE_CONDOR_dprintf_stats(const char *message)
{
	int rval = -1, result = 0;
	condor_errno_t terrno;

	dprintf ( D_SYSCALLS, "Doing CONDOR_dprintf_stats\n" );
	
	CurrentSysCall = CONDOR_dprintf_stats;

	if( ! syscall_sock->is_connected() ) {
		dprintf(D_ALWAYS, "RPC error: disconnected from shadow\n");
		errno = ETIMEDOUT;
		return -1;
	}

	syscall_sock->encode();
	result = ( syscall_sock->code(CurrentSysCall) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->put(message));
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


// takes the directory that creds should be written into.  the shadow knows
// which creds belong to the job and will only give out those creds.
//
// on the wire, for future compatibility, we send a string.  currently
// this is ignored by the receiver.
int
REMOTE_CONDOR_getcreds(const char* creds_receive_dir)
{
	int result = 0;

	if(!(syscall_sock->get_encryption())) {
		if (can_switch_ids() || ! param_boolean("ALLOW_OAUTH_WITHOUT_ENCRYPTION", false)) {
			dprintf(D_ALWAYS, "ERROR: Can't do CONDOR_getcreds, syscall_sock not encrypted\n");
			// fail
			return -1;
		}
	}

	dprintf ( D_SECURITY|D_FULLDEBUG, "Doing CONDOR_getcreds into path %s\n", creds_receive_dir );

	CurrentSysCall = CONDOR_getcreds;
	char empty[1] = "";
	char* empty_ptr = empty;

	if( ! syscall_sock->is_connected() ) {
		dprintf(D_ALWAYS, "RPC error: disconnected from shadow\n");
		errno = ETIMEDOUT;
		return -1;
	}

	syscall_sock->encode();
	result = ( syscall_sock->code(CurrentSysCall) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->code(empty_ptr) );
	ON_ERROR_RETURN( result );
	result = ( syscall_sock->end_of_message() );
	ON_ERROR_RETURN( result );

	// receive response
	syscall_sock->decode();

	// driver:  receive int.  -1 == error, 0 == done, 1 == cred classad coming
	int cmd;
	for(;;) {
		result = ( syscall_sock->code(cmd) );
		ON_ERROR_RETURN( result );
		if( cmd <= 0 ) {
			// no more creds to receive, or error
			break;
		}

		ClassAd ad;
		result = ( getClassAd(syscall_sock, ad) );
		ON_ERROR_RETURN( result );
		dprintf( D_SECURITY|D_FULLDEBUG, "CONDOR_getcreds: received ad:\n" );
		dPrintAd(D_SECURITY|D_FULLDEBUG, ad);

		std::string fname, b64;
		ad.LookupString("Service", fname);
		ad.LookupString("Data", b64);

		std::string full_name = creds_receive_dir;
		full_name += DIR_DELIM_CHAR;
		full_name += fname;

		std::string tmpname = full_name;
		tmpname += ".tmp";

		// contents of pw are base64 encoded.  decode now just before they go
		// into the file.
		int rawlen = -1;
		unsigned char* rawbuf = NULL;
		zkm_base64_decode(b64.c_str(), &rawbuf, &rawlen);

		if (rawlen <= 0) {
			EXCEPT("Failed to decode credential sent by shadow!");
		}

		// write temp file
		dprintf (D_SECURITY, "Writing data to %s\n", tmpname.c_str());
		// 4th param false means "as user"  (as opposed to as root)
		bool rc = write_secure_file(tmpname.c_str(), rawbuf, rawlen, false);

		// caller of condor_base64_decode is responsible for freeing buffer
		free(rawbuf);

		if (rc != true) {
			dprintf(D_ALWAYS, "REMOTE_CONDOR_getcreds: failed to write secure temp file %s\n", tmpname.c_str());
			EXCEPT("failure");
		}

		// do this as the user (no priv switching to root)
		dprintf (D_SECURITY, "Moving %s to %s\n", tmpname.c_str(), full_name.c_str());
		rename(tmpname.c_str(), full_name.c_str());
	}

	result = ( syscall_sock->end_of_message() );
	ON_ERROR_RETURN( result );

	// return status
	// >0:  Success
	// <=0: Failure
	return (cmd >= 0) ? 1 : 0;
}

int
REMOTE_CONDOR_get_delegated_proxy( const char* proxy_source_path, const char* proxy_dest_path, time_t proxy_expiration )
{
	int result = 0;
	dprintf( D_SECURITY|D_FULLDEBUG, "Doing CONDOR_get_delegated_proxy\n" );

	CurrentSysCall = CONDOR_get_delegated_proxy;

	if( ! syscall_sock->is_connected() ) {
		dprintf(D_ALWAYS, "RPC error: disconnected from shadow\n");
		errno = ETIMEDOUT;
		return -1;
	}

	// Send message telling receiver we are requesting a delegated proxy
	syscall_sock->encode();
	result = ( syscall_sock->code( CurrentSysCall ) );
	ON_ERROR_RETURN( result );

	// Send proxy path
	result = ( syscall_sock->put( proxy_source_path ) );
	ON_ERROR_RETURN( result );

	// Send proxy expiration time
	result = ( syscall_sock->put( proxy_expiration ) );
	ON_ERROR_RETURN( result );

	// Now we're done sending our request, so send an end_of_message.
	result = ( syscall_sock->end_of_message() );
	ON_ERROR_RETURN( result );

	// Switch to receive/read mode and wait for delegated x509 proxy
	syscall_sock->decode();
	int get_x509_rc = ( syscall_sock->get_x509_delegation( proxy_dest_path, false, NULL ) == ReliSock::delegation_ok ) ? 0 : -1;
	dprintf( D_FULLDEBUG, "CONDOR_get_delegated_proxy: get_x509_delegation() returned %d\n", get_x509_rc );

	// Wait for end_of_message and return
	result = ( syscall_sock->end_of_message() );
	ON_ERROR_RETURN( result );
	return get_x509_rc;
}


} // extern "C"
