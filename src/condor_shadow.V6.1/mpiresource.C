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
#include "mpiresource.h"

MpiResource::MpiResource( BaseShadow *shadow ) :
	RemoteResource( shadow ) 
{
	state = PRE;
}

MpiResource::MpiResource( BaseShadow *shadow, 
						  const char *executingHost, 
						  const char *capability ) :
	RemoteResource( shadow, executingHost, capability ) 
{
	state = PRE;
}


int 
MpiResource::requestIt( int starterVersion = 2 )  {
	int r = RemoteResource::requestIt( starterVersion );
	if ( r == 0 ) { // success
		setResourceState( EXECUTING );
	}
	return r;
}

int
MpiResource::killStarter() {
	int r = RemoteResource::killStarter();
	if ( r == 0 ) {
		setResourceState( PENDING_DEATH );
	}
	return r;
}

void
MpiResource::dprintfSelf( int debugLevel) {
	RemoteResource::dprintfSelf( debugLevel );
	shadow->dprintf ( debugLevel, "\tstate:         %s\n", 
					  Resource_State_String[state] );
}

void 
MpiResource::printExit( FILE *fp ) {

	fprintf ( fp, "%25s    ", machineName ? machineName : "Unknown machine" );
	RemoteResource::printExit( fp );
}

void 
MpiResource::setResourceState( Resource_State s ) {
	shadow->dprintf ( D_FULLDEBUG,"Resource %s changing state from %s to %s\n",
					  machineName ? machineName : "???", 
					  Resource_State_String[state], 
					  Resource_State_String[s] );
	state = s;
}

void
MpiResource::setExitReason( int reason ) {
		/* If you're setting the exit reason, you must be done! */
	RemoteResource::setExitReason( reason );
	setResourceState( FINISHED );
}
