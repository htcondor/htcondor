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
#include "parallelresource.h"

ParallelResource::ParallelResource( BaseShadow *shadow ) :
	RemoteResource( shadow ) 
{
	node_num = -1;
}


void 
ParallelResource::printExit( FILE *fp ) {

	fprintf ( fp, "%25s    ", machineName ? machineName : "Unknown machine" );
	RemoteResource::printExit( fp );
}


void
ParallelResource::resourceExit( int reason, int status ) 
{
	dprintf( D_FULLDEBUG, "Inside ParallelResource::resourceExit()\n" );

	RemoteResource::resourceExit( reason, status );

		// Also log a NodeTerminatedEvent to the ULog

	NodeTerminatedEvent event;
	switch( reason ) {
	case JOB_COREDUMPED:
	case JOB_EXITED:
		{
			// Job exited on its own, normally or abnormally
			NodeTerminatedEvent event;
			event.node = node_num;
			if( exited_by_signal ) {
				event.normal = false;
				event.signalNumber = exit_value;
			} else {
				event.normal = true;
				event.returnValue = exit_value;
			}
			
			int had_core = 0;
			jobAd->LookupBool( ATTR_JOB_CORE_DUMPED, had_core );
			if( had_core ) {
				event.setCoreFile( shadow->getCoreName() );
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
			if( !writeULogEvent(&event) ) {
				dprintf( D_ALWAYS,"Unable to log "
						 "ULOG_NODE_TERMINATED event\n" );
			}
		}
		break;	
	case JOB_CKPTED:
	case JOB_NOT_CKPTED:
			// XXX Todo: Do we want a Node evicted event?
		break;
	default:
		dprintf( D_ALWAYS, "Warning: Unknown exit_reason %d in "
				 "ParallelResource::resourceExit()\n", reason );  
	}	

}


bool
ParallelResource::writeULogEvent( ULogEvent* event )
{
	bool rval;
	shadow->uLog.initialize( shadow->getCluster(), 
							 shadow->getProc(), node_num );
	rval = RemoteResource::writeULogEvent( event );
	shadow->uLog.initialize( shadow->getCluster(), 
							 shadow->getProc(), 0 ); 
	return rval;
}


void
ParallelResource::beginExecution( void )
{
	char* startd_addr;
	if( ! dc_startd ) {
		dprintf( D_ALWAYS, "beginExecution() "
				 "called with no DCStartd object!\n" ); 
		return;
	}
	if( ! (startd_addr = dc_startd->addr()) ) {
		dprintf( D_ALWAYS, "beginExecution() "
				 "called with no startd contact info!\n" );
		return;
	}

	NodeExecuteEvent event;
	strcpy( event.executeHost, startd_addr );
	event.node = node_num;
	if( ! writeULogEvent(&event) ) {
		dprintf( D_ALWAYS, "Unable to log NODE_EXECUTE event." );
	}

		// Call our parent class's version to handle everything else. 
	RemoteResource::beginExecution();
}
