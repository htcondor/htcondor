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
	if ( !jobAd ) {
		EXCEPT("No jobAd defined!");
	}

		// we're only dealing with one host, so this is trivial:
	remRes->setExecutingHost( host );
	remRes->setCapability( capability );
	remRes->setMachineName( "Unknown" );
	
		// base init takes care of lots of stuff:
	baseInit( jobAd, schedd_addr, cluster, proc );

		// In this case we just pass the pointer along...
	remRes->setJobAd( jobAd );
	
		// yak with startd:
	if ( remRes->requestIt() == -1 ) {
		shutDown( JOB_NOT_STARTED, 0 );
	}

		// Make an execute event:
	ExecuteEvent event;
	strcpy( event.executeHost, host );
	if ( !uLog.writeEvent(&event)) {
		dprintf(D_ALWAYS, "Unable to log ULOG_EXECUTE event\n");
	}

		// Register the remote instance's claimSock for remote
		// system calls.  It's a bit funky, but works.
	daemonCore->Register_Socket(remRes->getClaimSock(), "RSC Socket", 
		(SocketHandlercpp)&RemoteResource::handleSysCalls, "HandleSyscalls", 
								remRes);

}

void UniShadow::shutDown( int reason, int exitStatus ) {

		// exit now if there is no job ad
	if ( !getJobAd() ) {
		DC_Exit( reason );
	}
	
		// if we are being called from the exception handler, return
		// now.
	if ( reason == JOB_EXCEPTION ) {
		return;
	}

		// write stuff to user log:
	endingUserLog( exitStatus, reason, remRes );

		// returns a mailer if desired
	FILE* mailer;
	mailer = shutDownEmail( reason, exitStatus );
	if ( mailer ) {
		fprintf( mailer, "Your Condor job %d.%d has completed.\n\n", 
				 getCluster(), getProc() );

		fprintf ( mailer, "It  " );

		remRes->printExit( mailer );

		fprintf( mailer, "\nHave a nice day.\n" );

		email_close(mailer);
	}

	remRes->dprintfSelf( D_ALWAYS );
	
		// does not return.
	DC_Exit( reason );
}

int UniShadow::handleJobRemoval(int sig) {
    dprintf ( D_FULLDEBUG, "In handleJobRemoval(), sig %d\n", sig );
	remRes->setExitReason( JOB_KILLED );
	remRes->killStarter();
		// more?
	return 0;
}


