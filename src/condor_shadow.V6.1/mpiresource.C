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
	node_num = -1;
}

MpiResource::MpiResource( BaseShadow *shadow, 
						  const char *executingHost, 
						  const char *capability ) :
	RemoteResource( shadow, executingHost, capability ) 
{
	state = PRE;
	node_num = -1;
}


int 
MpiResource::requestIt( int starterVersion )  {

	char buf[256];
	sprintf( buf, "%s, node: %d", "MpiResource::requestIt()", node_num );
	dumpClassad( buf, jobAd, D_JOB );

	int r = RemoteResource::requestIt( starterVersion );
	if ( r == 0 ) { // success
		setResourceState( EXECUTING );
		NodeExecuteEvent event;
        strcpy( event.executeHost, executingHost );
		event.node = node_num;
		shadow->uLog.initialize( shadow->getCluster(),
								 shadow->getProc(), node_num );
        if ( !shadow->uLog.writeEvent( &event )) {
            dprintf ( D_ALWAYS, "Unable to log NODE_EXECUTE event." );
        }
		shadow->uLog.initialize( shadow->getCluster(),
								 shadow->getProc(), 0 );
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
MpiResource::setExitStatus( int status ) {
		/* If you're setting the exit status, you must be done! */
	RemoteResource::setExitStatus( status );
	setResourceState( FINISHED );
}


void
MpiResource::resourceExit( int exit_reason, int exit_status ) 
{
	dprintf( D_FULLDEBUG, "Inside MpiResource::resourceExit()\n" );

	RemoteResource::resourceExit( exit_reason, exit_status );

		// Also log a NodeTerminatedEvent to the ULog

	NodeTerminatedEvent event;
	switch( exit_reason ) {
	case JOB_EXITED:
		{
			// Job exited on its own, normally or abnormally
			NodeTerminatedEvent event;
			event.node = node_num;
			if( (event.normal = (WIFEXITED(exitStatus)!=0)) ) {
				event.returnValue = WEXITSTATUS(exitStatus);
			} else {
				event.signalNumber = WTERMSIG(exitStatus);
			}
			
				// TODO: fill in local/total rusage
				// event.run_local_rusage = r;
			event.run_remote_rusage = remote_rusage;
				// event.total_local_rusage = r;
			event.total_remote_rusage = remote_rusage;
			
				/* we want to log the events from the perspective
				   of the user job, so if the shadow *sent* the
				   bytes, then that means the user job *received*
				   the bytes */
			event.recvd_bytes = bytesSent();
			event.sent_bytes = bytesReceived();
				// TODO: total sent and recvd
			event.total_recvd_bytes = bytesSent();
			event.total_sent_bytes = bytesReceived();
			
			shadow->uLog.initialize( shadow->getCluster(),
									 shadow->getProc(), node_num );
			if( !shadow->uLog.writeEvent(&event) ) {
				dprintf( D_ALWAYS,"Unable to log "
						 "ULOG_NODE_TERMINATED event\n" );
			}
			shadow->uLog.initialize( shadow->getCluster(),
									 shadow->getProc(), 0 );

		}
		break;	
	case JOB_CKPTED:
	case JOB_NOT_CKPTED:
			// XXX Todo: Do we want a Node evicted event?
		break;
	default:
		dprintf( D_ALWAYS, "Warning: Unknown exit_reason %d in "
				 "MpiResource::resourceExit()\n", exit_reason );  
	}	

}
