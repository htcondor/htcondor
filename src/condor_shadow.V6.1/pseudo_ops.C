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
#include "condor_attributes.h"
#include "condor_debug.h"
#include "condor_io.h"
#include "condor_uid.h"
#include "shadow.h"
#include "pseudo_ops.h"
#include "exit.h"

extern ReliSock *syscall_sock;
extern BaseShadow *Shadow;
extern RemoteResource *thisRemoteResource;

int
pseudo_register_machine_info(char *uiddomain, char *fsdomain, 
							 char * starterAddr, char *full_hostname )
{

	thisRemoteResource->setUidDomain( uiddomain );
	thisRemoteResource->setFilesystemDomain( fsdomain );
	thisRemoteResource->setStarterAddress( starterAddr );
	thisRemoteResource->setMachineName( full_hostname );

	return 0;
}

int
pseudo_get_job_info(ClassAd *&ad)
{
	ClassAd * the_ad;

	the_ad = thisRemoteResource->getJobAd();
	ASSERT( the_ad );

	thisRemoteResource->filetrans.Init( the_ad, true, PRIV_USER );

	ad = the_ad;
	return 0;
}


int
pseudo_job_exit(int status, int reason, ClassAd* ad)
{
	// reset the reason if less than EXIT_CODE_OFFSET so that
	// an older starter can be made compatible with the newer
	// schedd exit reasons.
	if ( reason < EXIT_CODE_OFFSET ) {
		if ( reason != JOB_EXCEPTION && reason != DPRINTF_ERROR ) {
			reason += EXIT_CODE_OFFSET;
			dprintf(D_SYSCALLS, "in pseudo_job_exit: old starter, reason reset"
				" from %d to %d\n",reason-EXIT_CODE_OFFSET,reason);
		}
	}
	dprintf(D_SYSCALLS, "in pseudo_job_exit: status=%d,reason=%d\n",
			status, reason);
	thisRemoteResource->updateFromStarter( ad );
	thisRemoteResource->resourceExit( reason, status );
	return 0;
}



