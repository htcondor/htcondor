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
#include "condor_config.h"
#include "exit.h"
#include "condor_version.h"

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

		/* For backwards compatibility, if we get this old pseudo call
		   from the starter, we assume we're not going to get the
		   happy new pseudo_begin_execution call.  So, pretend we got
		   it now so we still log execute events and so on, even if
		   it's not as acurate as we'd like.
		*/
	thisRemoteResource->beginExecution();
	return 0;
}


int
pseudo_register_starter_info( ClassAd* ad )
{
	char* tmp = NULL;

	if( ad->LookupString(ATTR_UID_DOMAIN, &tmp) ) {
		thisRemoteResource->setUidDomain( tmp );
		dprintf( D_SYSCALLS, "  %s = %s\n", ATTR_UID_DOMAIN, tmp );
		free( tmp );
		tmp = NULL;
	}

	if( ad->LookupString(ATTR_FILE_SYSTEM_DOMAIN, &tmp) ) {
		thisRemoteResource->setFilesystemDomain( tmp );
		dprintf( D_SYSCALLS, "  %s = %s\n", ATTR_FILE_SYSTEM_DOMAIN,
				 tmp );  
		free( tmp );
		tmp = NULL;
	}

	if( ad->LookupString(ATTR_MACHINE, &tmp) ) {
		thisRemoteResource->setMachineName( tmp );
		dprintf( D_SYSCALLS, "  %s = %s\n", ATTR_MACHINE, tmp );
		free( tmp );
		tmp = NULL;
	}

	if( ad->LookupString(ATTR_STARTER_IP_ADDR, &tmp) ) {
		thisRemoteResource->setStarterAddress( tmp );
		dprintf( D_SYSCALLS, "  %s = %s\n", ATTR_STARTER_IP_ADDR, tmp ); 
		free( tmp );
		tmp = NULL;
	}

	if( ad->LookupString(ATTR_ARCH, &tmp) ) {
		thisRemoteResource->setStarterArch( tmp );
		dprintf( D_SYSCALLS, "  %s = %s\n", ATTR_ARCH, tmp ); 
		free( tmp );
		tmp = NULL;
	}

	if( ad->LookupString(ATTR_OPSYS, &tmp) ) {
		thisRemoteResource->setStarterOpsys( tmp );
		dprintf( D_SYSCALLS, "  %s = %s\n", ATTR_OPSYS, tmp ); 
		free( tmp );
		tmp = NULL;
	}

	if( ad->LookupString(ATTR_VERSION, &tmp) ) {
		thisRemoteResource->setStarterVersion( tmp );
		dprintf( D_SYSCALLS, "  %s = %s\n", ATTR_VERSION, tmp ); 
		free( tmp );
		tmp = NULL;
	}

	return 0;
}


int
pseudo_begin_execution()
{
	thisRemoteResource->beginExecution();
	return 0;
}


int
pseudo_get_job_info(ClassAd *&ad)
{
	ClassAd * the_ad;

	the_ad = thisRemoteResource->getJobAd();
	ASSERT( the_ad );

		// FileTransfer now makes sure we only do Init() once
	thisRemoteResource->filetrans.Init( the_ad, true, PRIV_USER );

	// let the starter know the version of the shadow
	int size = 10 + strlen(CondorVersion()) + strlen(ATTR_SHADOW_VERSION);
	char *shadow_ver = (char *) malloc(size);
	ASSERT(shadow_ver);
	sprintf(shadow_ver,"%s=\"%s\"",ATTR_SHADOW_VERSION,CondorVersion());
	the_ad->Insert(shadow_ver);
	free(shadow_ver);

		// Also, try to include our value for UidDomain, so that the
		// starter can properly compare them...
	char* uid_domain = param( "UID_DOMAIN" );
	if( uid_domain ) {
		size = 10 + strlen(uid_domain) + strlen(ATTR_UID_DOMAIN);
		char* uid_domain_expr = (char*) malloc( size );
		ASSERT(uid_domain_expr);
		sprintf( uid_domain_expr, "%s=\"%s\"", ATTR_UID_DOMAIN,
				 uid_domain );
		the_ad->Insert( uid_domain_expr );
		free( uid_domain_expr );
		free( uid_domain );
	}

	ad = the_ad;
	return 0;
}


int
pseudo_get_user_info(ClassAd *&ad)
{
	static ClassAd* user_ad = NULL;
	char buf[1024];

	if( ! user_ad ) {
			// if we don't have the ClassAd yet, allocate it and fill
			// it in with the info we care about
		user_ad = new ClassAd;

#ifndef WIN32
		sprintf( buf, "%s = %d", ATTR_UID, (int)get_user_uid() );
		user_ad->Insert( buf );

		sprintf( buf, "%s = %d", ATTR_GID, (int)get_user_gid() );
		user_ad->Insert( buf );
#endif

	}

	ad = user_ad;
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


int
pseudo_register_mpi_master_info( ClassAd* ad ) 
{
	char *addr = NULL;

	if( ! ad->LookupString(ATTR_MPI_MASTER_ADDR, &addr) ) {
		dprintf( D_ALWAYS,
				 "ERROR: mpi_master_info ClassAd doesn't contain %s\n",
				 ATTR_MPI_MASTER_ADDR );
		return -1;
	}
	if( ! Shadow->setMpiMasterInfo(addr) ) {
		dprintf( D_ALWAYS, "ERROR: recieved "
				 "pseudo_register_mpi_master_info for a non-MPI job!\n" );
		return -1;
	}
	return 0;
}
