/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/


#include "condor_common.h"
#include "mpiresource.h"

MpiResource::MpiResource( BaseShadow *shadow ) :
	RemoteResource( shadow ) 
{
	node_num = -1;
}


void 
MpiResource::printExit( FILE *fp ) {

	fprintf ( fp, "%25s    ", machineName ? machineName : "Unknown machine" );
	RemoteResource::printExit( fp );
}


void
MpiResource::resourceExit( int reason, int status ) 
{
	dprintf( D_FULLDEBUG, "Inside MpiResource::resourceExit()\n" );

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
				 "MpiResource::resourceExit()\n", reason );  
	}	

}


bool
MpiResource::writeULogEvent( ULogEvent* event )
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
MpiResource::beginExecution( void )
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


void
MpiResource::reconnect( void )
{
	EXCEPT( "The MpiResource class does not support reconnect" ); 
}


void
MpiResource::attemptReconnect( void )
{
	EXCEPT( "The MpiResource class does not support reconnect" ); 
}


bool
MpiResource::supportsReconnect( void )
{
	return false;
}


