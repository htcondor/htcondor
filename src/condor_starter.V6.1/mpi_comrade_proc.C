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
#include "mpi_comrade_proc.h"
#include "condor_attributes.h"


MPIComradeProc::MPIComradeProc( ClassAd * jobAd ) : VanillaProc( jobAd )
{
    dprintf ( D_FULLDEBUG, "Constructor of MPIComradeProc::MPIComradeProc\n" );
	Node = -1;
}

MPIComradeProc::~MPIComradeProc()  {}


int 
MPIComradeProc::StartJob()
{ 
	dprintf(D_FULLDEBUG,"in MPIComradeProc::StartJob()\n");

	if ( !JobAd ) {
		dprintf ( D_ALWAYS, "No JobAd in MPIComradeProc::StartJob()!\n" ); 
		return 0;
	}
		// Grab ATTR_NODE out of the job ad and stick it in our
		// protected member so we can insert it back on updates, etc. 
	if( JobAd->LookupInteger(ATTR_NODE, Node) != 1 ) {
		dprintf( D_ALWAYS, "ERROR in MPIComradeProc::StartJob(): "
				 "No %s in job ad, aborting!\n", ATTR_NODE );
		return 0;
	} else {
		dprintf( D_FULLDEBUG, "Found %s = %d in job ad\n", ATTR_NODE, 
				 Node ); 
	}

    dprintf(D_PROTOCOL, "#11 - Comrade starting up....\n" );

        // special args already in ad; simply start it up
    return VanillaProc::StartJob();
}


int 
MPIComradeProc::JobExit( int pid, int status ) { 
	dprintf(D_FULLDEBUG,"in MPIComradeProc::JobExit()\n");
    return VanillaProc::JobExit( pid, status );
}

void 
MPIComradeProc::Suspend() { 
        /* We Comrades don't ever want to be suspended.  We 
           take it as a violation of our basic rights.  Therefore, 
           we walk off the job and notify the shadow immediately! */
	dprintf(D_FULLDEBUG,"in MPIComradeProc::Suspend()\n");
		// must do this so that we exit...
	daemonCore->Send_Signal( daemonCore->getpid(), DC_SIGQUIT );
}

void 
MPIComradeProc::Continue() { 
	dprintf(D_FULLDEBUG,"in MPIComradeProc::Continue() (!)\n");    
        // really should never get here, but just in case.....
    VanillaProc::Continue();
}

bool 
MPIComradeProc::ShutDownGraceful() { 
	dprintf(D_FULLDEBUG,"in MPIComradeProc::ShutDownGraceful()\n");
	return VanillaProc::ShutdownGraceful();
}

bool
MPIComradeProc::ShutdownFast() {
	dprintf(D_FULLDEBUG,"in MPIComradeProc::ShutDownFast()\n");
    return VanillaProc::ShutdownFast();
}


bool
MPIComradeProc::PublishUpdateAd( ClassAd* ad )
{
	dprintf( D_FULLDEBUG, "In MPIComradeProc::PublishUpdateAd()\n" );
	char buf[64];
	sprintf( buf, "%s = %d", ATTR_NODE, Node );
	ad->Insert( buf );

		// Now, call our parent class's version
	return VanillaProc::PublishUpdateAd( ad );
}

