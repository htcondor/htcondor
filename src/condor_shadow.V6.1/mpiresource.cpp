/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/



#include "condor_common.h"
#include "mpiresource.h"
#include "classad_helpers.h"

MpiResource::MpiResource( BaseShadow *base_shadow ) :
	RemoteResource( base_shadow ) 
{
	node_num = -1;
}


void 
MpiResource::printExit( FILE *fp )
{
	std::string line;
	formatstr( line, "%25s    ", machineName ? machineName : "Unknown machine" );
	printExitString( jobAd, exit_reason, line );
	fprintf( fp, "%s\n", line.c_str() );
}

void
MpiResource::hadContact(void) {
	RemoteResource::hadContact();

    shadow->getJobAd()->Assign( ATTR_LAST_JOB_LEASE_RENEWAL,
	                            last_job_lease_renewal );

}

void
MpiResource::resourceExit( int reason, int status ) 
{
	dprintf( D_FULLDEBUG, "Inside MpiResource::resourceExit()\n" );

	RemoteResource::resourceExit( reason, status );

		// Also log a NodeTerminatedEvent to the ULog

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
			
			bool had_core = false;
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
	shadow->uLog.setJobId( shadow->getCluster(), shadow->getProc(), node_num );
	rval = RemoteResource::writeULogEvent( event );
	shadow->uLog.setJobId( shadow->getCluster(), shadow->getProc(), 0 );
	return rval;
}


void
MpiResource::beginExecution( void )
{
	const char* startd_addr;
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
	event.setExecuteHost( startd_addr );
	event.node = node_num;
	if( ! writeULogEvent(&event) ) {
		dprintf( D_ALWAYS, "Unable to log NODE_EXECUTE event.\n" );
	}

		// Call our parent class's version to handle everything else. 
	RemoteResource::beginExecution();
}


void
MpiResource::reconnect( void )
{
	RemoteResource::reconnect();
}


void
MpiResource::attemptReconnect( void )
{
	RemoteResource::attemptReconnect();
}


bool
MpiResource::supportsReconnect( void )
{
	return RemoteResource::supportsReconnect();
}


