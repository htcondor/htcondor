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
#include "shadow.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_qmgr.h"         // need to talk to schedd's qmgr
#include "condor_attributes.h"   // for ATTR_ ClassAd stuff
#include "condor_email.h"        // for email.

static char *_FileName_ = __FILE__;   /* Used by EXCEPT (see except.h) */

UniShadow::UniShadow() {
		// pass RemoteResource ourself, so it knows where to go if
		// it has to call something like shutDown().
	remRes = new RemoteResource( this );
}

UniShadow::~UniShadow() {
	if ( remRes ) delete remRes;
}

void UniShadow::init( ClassAd *jobAd, char schedd_addr[], char host[], 
					  char capability[], char cluster[], char proc[])
{
		// jobAd should have been passed to the constructor...
	if ( !jobAd ) {
		EXCEPT("No jobAd defined!");
	}

		// we're only dealing with one host, so this is trivial:
	remRes->setExecutingHost( host );
	remRes->setCapability( capability );

		// base init takes care of lots of stuff:
	baseInit( jobAd, schedd_addr, cluster, proc );

		// yak with startd:
	if ( remRes->requestIt( jobAd ) == -1 ) {
		shutDown( JOB_NOT_STARTED, 0 );
	}

	char *buf = NULL;
	remRes->getExecutingHost( buf );
	makeExecuteEvent( buf );
	delete [] buf;

		// Register the remote instance's claimSock for remote
		// system calls.  It's a bit funky, but works.
	daemonCore->Register_Socket(remRes->getClaimSock(), "RSC Socket", 
		(SocketHandlercpp)&RemoteResource::handleSysCalls, "HandleSyscalls", 
								remRes);

}

void UniShadow::shutDown( int reason, int exitStatus ) {

		// update job queue with relavent info
	if ( getJobAd() ) {
		char *tmp = NULL;
		remRes->getExecutingHost( tmp );
		ConnectQ( getScheddAddr() );
		DeleteAttribute( getCluster(), getProc(), ATTR_REMOTE_HOST );
		SetAttributeString( getCluster(), getProc(), 
							ATTR_LAST_REMOTE_HOST, tmp );
		DisconnectQ( NULL );
		delete [] tmp;
	}
	else {
		DC_Exit( reason );
	}
	
		// if we are being called from the exception handler, return
		// now.
	if ( reason == JOB_EXCEPTION ) {
		return;
	}

		// returns a mailer if desired
	FILE* mailer;
	mailer = shutDownEmail( reason, exitStatus );
	if ( mailer ) {
		fprintf( mailer, "Your Condor job %d.%d exited with status"
				 "%d (0x%X).\n", getCluster(), getProc(), remRes->getExitStatus(),
				 remRes->getExitStatus() );
				 
		fprintf( mailer, "\nHave a nice day.\n" );
		email_close(mailer);
	}
	
		// does not return.
	DC_Exit( reason );
}

int UniShadow::handleJobRemoval(int sig) {
	remRes->setExitReason( JOB_KILLED );
	remRes->killStarter();
		// more?
	return 0;
}
