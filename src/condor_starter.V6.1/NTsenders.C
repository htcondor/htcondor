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


int CurrentSysCall;

extern ReliSock *syscall_sock;

extern "C" {

int
REMOTE_syscall( int syscall_num, ... )
{
	int		terrno=0;
	int		rval=-1;
	va_list ap;

	va_start( ap, syscall_num );
	
	switch( syscall_num ) {

	case CONDOR_register_machine_info:
	{
		char *uiddomain;
		char *fsdomain;
		char *address;
		char *fullHostname;
		int key;

        dprintf ( D_SYSCALLS, "Doing CONDOR_register_machine_info\n" );

		CurrentSysCall = CONDOR_register_machine_info;

		uiddomain = va_arg( ap, char *);
		fsdomain = va_arg( ap, char *);
		address = va_arg( ap, char *);
		fullHostname = va_arg( ap, char *);
		key = va_arg( ap, int);

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
			break;
		}
		assert( syscall_sock->end_of_message() );
		break;
	}

	case CONDOR_get_job_info:
	{
		ClassAd *ad;

        dprintf ( D_SYSCALLS, "Doing CONDOR_get_job_info\n" );

		CurrentSysCall = CONDOR_get_job_info;

		ad = va_arg( ap, ClassAd *);

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
			break;
		}
		assert( ad->get(*syscall_sock) );
		assert( syscall_sock->end_of_message() );
		break;
	}

#if 0
	case CONDOR_get_executable:
	{
		char *destination;

        dprintf ( D_SYSCALLS, "Doing CONDOR_get_executable\n" );

		CurrentSysCall = CONDOR_get_executable;

		destination = va_arg( ap, char *);

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
			break;
		}
		assert( syscall_sock->get_file(destination) > -1 );
		assert( syscall_sock->end_of_message() );
		break;
	}
#endif

	case CONDOR_job_exit:
	{
		int status;
		int reason;
		ClassAd* ad;

        dprintf ( D_SYSCALLS, "Doing CONDOR_job_exit\n" );

		CurrentSysCall = CONDOR_job_exit;

		status = va_arg( ap, int );
		reason = va_arg( ap, int );
		ad = va_arg( ap, ClassAd * );

		syscall_sock->encode();
		assert( syscall_sock->code(CurrentSysCall) );
		assert( syscall_sock->code(status) );
		assert( syscall_sock->code(reason) );
		assert( ad->put(*syscall_sock) );
		assert( syscall_sock->end_of_message() );

		syscall_sock->decode();
		assert( syscall_sock->code(rval) );
		if( rval < 0 ) {
			assert( syscall_sock->code(terrno) );
			assert( syscall_sock->end_of_message() );
			errno = terrno;
            dprintf ( D_SYSCALLS, "Return val problem, errno = %d\n", errno );
			break;
		}
		assert( syscall_sock->end_of_message() );
		break;
	}


	default:
		fprintf( stderr,
				 "Don't know how to do system call %d remotely - yet\n",
				 syscall_num );
		rval =  -1;
	}

	return rval;
}

} // extern "C"
