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

int
do_REMOTE_syscall()
{
	int condor_sysnum;
	int	rval;
	int terrno;

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
		dprintf( D_SYSCALLS, "  key = %d\n", key);
		assert( syscall_sock->end_of_message() );

		errno = 0;
		rval = pseudo_register_machine_info(uiddomain, fsdomain, 
											address, fullHostname);
		terrno = errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();
		assert( syscall_sock->code(rval) );
		if( rval < 0 ) {
			assert( syscall_sock->code(terrno) );
		}
		assert( syscall_sock->end_of_message() );
		free(uiddomain);
		free(fsdomain);
		free(address);
		free(fullHostname);
		return 0;
	}

	case CONDOR_get_job_info:
	{
		ClassAd *ad = NULL;

		assert( syscall_sock->end_of_message() );

		errno = 0;
		rval = pseudo_get_job_info(ad);
		terrno = errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();
		assert( syscall_sock->code(rval) );
		if( rval < 0 ) {
			assert( syscall_sock->code(terrno) );
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
		assert( ad.get(*syscall_sock) );
		assert( syscall_sock->end_of_message() );

		errno = 0;
		rval = pseudo_job_exit(status, reason, &ad);
		terrno = errno;
		dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );

		syscall_sock->encode();
		assert( syscall_sock->code(rval) );
		if( rval < 0 ) {
			assert( syscall_sock->code(terrno) );
		}
		assert( syscall_sock->end_of_message() );
		return -1;
	}

	default:
	{
		EXCEPT( "unknown syscall %d received\n", condor_sysnum );
	}

	}	/* End of switch on system call number */

	return -1;

}	/* End of do_REMOTE_syscall() procedure */
