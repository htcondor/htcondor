/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-2002 CONDOR Team, Computer Sciences Department,
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.
 * No use of the CONDOR Software Program Source Code is authorized
 * without the express consent of the CONDOR Team.  For more
 * information contact: CONDOR Team, Attention: Professor Miron Livny,
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
#include "condor_debug.h"
#include "condor_classad.h"
#include "condor_commands.h"
#include "condor_attributes.h"
#include "daemon.h"
#include "dc_schedd.h"
#include "proc.h"


// // // // //
// DCSchedd
// // // // //


DCSchedd::DCSchedd( const char* name, const char* pool ) 
	: Daemon( DT_SCHEDD, name, pool )
{
}


DCSchedd::~DCSchedd( void )
{
}


ClassAd*
DCSchedd::holdJobs( const char* constraint, const char* reason,
					action_result_type_t result_type,
					bool notify_scheduler )
{
	if( ! constraint ) {
		dprintf( D_ALWAYS, "DCSchedd::holdJobs: "
				 "constraint is NULL, aborting\n" );
		return NULL;
	}
	return actOnJobs( JA_HOLD_JOBS, "hold", constraint, NULL, 
					  reason, ATTR_HOLD_REASON, result_type,
					  notify_scheduler );
}


ClassAd*
DCSchedd::removeJobs( const char* constraint, const char* reason,
					  action_result_type_t result_type,
					  bool notify_scheduler )
{
	if( ! constraint ) {
		dprintf( D_ALWAYS, "DCSchedd::removeJobs: "
				 "constraint is NULL, aborting\n" );
		return NULL;
	}
	return actOnJobs( JA_REMOVE_JOBS, "remove", constraint, NULL,
					  reason, ATTR_REMOVE_REASON, result_type,
					  notify_scheduler );
}


ClassAd*
DCSchedd::releaseJobs( const char* constraint, const char* reason,
					   action_result_type_t result_type,
					   bool notify_scheduler )
{
	if( ! constraint ) {
		dprintf( D_ALWAYS, "DCSchedd::releaseJobs: "
				 "constraint is NULL, aborting\n" );
		return NULL;
	}
	return actOnJobs( JA_RELEASE_JOBS, "release", constraint, NULL,
					  reason, ATTR_RELEASE_REASON, result_type,
					  notify_scheduler );
}


ClassAd*
DCSchedd::holdJobs( StringList* ids, const char* reason,
					action_result_type_t result_type,
					bool notify_scheduler )
{
	if( ! ids ) {
		dprintf( D_ALWAYS, "DCSchedd::holdJobs: "
				 "list of jobs is NULL, aborting\n" );
		return NULL;
	}
	return actOnJobs( JA_HOLD_JOBS, "hold", NULL, ids, reason,
					  ATTR_HOLD_REASON, result_type,
					  notify_scheduler );
}


ClassAd*
DCSchedd::removeJobs( StringList* ids, const char* reason,
					action_result_type_t result_type,
					bool notify_scheduler )
{
	if( ! ids ) {
		dprintf( D_ALWAYS, "DCSchedd::removeJobs: "
				 "list of jobs is NULL, aborting\n" );
		return NULL;
	}
	return actOnJobs( JA_REMOVE_JOBS, "remove", NULL, ids,
					  reason, ATTR_REMOVE_REASON, result_type,
					  notify_scheduler );
}


ClassAd*
DCSchedd::releaseJobs( StringList* ids, const char* reason,
					   action_result_type_t result_type,
					   bool notify_scheduler )
{
	if( ! ids ) {
		dprintf( D_ALWAYS, "DCSchedd::releaseJobs: "
				 "list of jobs is NULL, aborting\n" );
		return NULL;
	}
	return actOnJobs( JA_RELEASE_JOBS, "release", NULL, ids,
					  reason, ATTR_RELEASE_REASON, result_type,
					  notify_scheduler );
}


ClassAd*
DCSchedd::actOnJobs( job_action_t action, const char* action_str, 
					 const char* constraint, StringList* ids, 
					 const char* reason, const char* reason_attr,
					 action_result_type_t result_type,
					 bool notify_scheduler )
{
	char* tmp = NULL;
	char buf[512];
	int size, reply;
	ReliSock rsock;

		// // // // // // // //
		// Construct the ad we want to send
		// // // // // // // //

	ClassAd cmd_ad;

	sprintf( buf, "%s = %d", ATTR_JOB_ACTION, action );
	cmd_ad.Insert( buf );
	
	sprintf( buf, "%s = %d", ATTR_ACTION_RESULT_TYPE, 
			 (int)result_type );
	cmd_ad.Insert( buf );

	sprintf( buf, "%s = %s", ATTR_NOTIFY_JOB_SCHEDULER, 
			 notify_scheduler ? "True" : "False" );
	cmd_ad.Insert( buf );

	if( constraint ) {
		if( ids ) {
				// This is a programming error, not a run-time one
			EXCEPT( "DCSchedd::actOnJobs has both constraint and ids!" );
		}
		size = strlen(constraint) + strlen(ATTR_ACTION_CONSTRAINT) + 4;  
		tmp = (char*) malloc( size*sizeof(char) );
		if( !tmp ) {
			EXCEPT( "Out of memory!" );
		}
		sprintf( tmp, "%s = %s", ATTR_ACTION_CONSTRAINT, constraint ); 
		if( ! cmd_ad.Insert(tmp) ) {
			dprintf( D_ALWAYS, "DCSchedd::actOnJobs: "
					 "Can't insert constraint (%s) into ClassAd!\n",
					 constraint );
			free( tmp );
			return NULL;
		}			
		free( tmp );
		tmp = NULL;
	} else if( ids ) {
		char* action_ids = ids->print_to_string();
		size = strlen(action_ids) + strlen(ATTR_ACTION_IDS) + 7;
		tmp = (char*) malloc( size*sizeof(char) );
		if( !tmp ) {
			EXCEPT( "Out of memory!" );
		}
		sprintf( tmp, "%s = \"%s\"", ATTR_ACTION_IDS, action_ids );
		cmd_ad.Insert( tmp );
		free( tmp );
		tmp = NULL;
	} else {
		EXCEPT( "DCSchedd::actOnJobs called without constraint or ids" );
	}

	if( reason_attr && reason ) {
		size = strlen(reason_attr) + strlen(reason) + 7;
		tmp = (char*) malloc( size*sizeof(char) );
		if( !tmp ) {
			EXCEPT( "Out of memory!" );
		}
		sprintf( tmp, "%s = \"%s\"", reason_attr, reason );
		cmd_ad.Insert( tmp );
		free( tmp );
		tmp = NULL;
	}

		// // // // // // // //
		// On the wire protocol
		// // // // // // // //

	rsock.timeout(20);   // years of research... :)
	if( ! rsock.connect(_addr) ) {
		dprintf( D_ALWAYS, "DCSchedd::actOnJobs: "
				 "Failed to connect to schedd (%s)\n", _addr );
		return NULL;
	}
	if( ! startCommand(ACT_ON_JOBS, (Sock*)&rsock) ) {
		dprintf( D_ALWAYS, "DCSchedd::actOnJobs: "
				 "Failed to send command (ACT_ON_JOBS) to the schedd\n" );
		return NULL;
	}
		// First, if we're not already authenticated, force that now. 
	if( ! rsock.isAuthenticated() ) {
		rsock.authenticate();
	}

		// Now, put the command classad on the wire
	if( ! (cmd_ad.put(rsock) && rsock.eom()) ) {
		dprintf( D_ALWAYS, "DCSchedd:actOnJobs: Can't send classad\n" );
		return NULL;
	}

		// Next, we need to read the reply from the schedd if things
		// are ok and it's going to go forward.  If the schedd can't
		// read our reply to this ClassAd, it assumes we got killed
		// and it should abort its transaction
	rsock.decode();
	ClassAd* result_ad = new ClassAd();
	if( ! (result_ad->initFromStream(rsock) && rsock.eom()) ) {
		dprintf( D_ALWAYS, "DCSchedd:actOnJobs: "
				 "Can't read response ad from %s\n", _addr );
		delete( result_ad );
		return NULL;
	}

		// If the action totally failed, the schedd will already have
		// aborted the transaction and closed up shop, so there's no
		// reason trying to continue.  However, we still want to
		// return the result ad we got back so that our caller can
		// figure out what went wrong.
	reply = FALSE;
	result_ad->LookupInteger( ATTR_ACTION_RESULT, reply );
	if( reply != OK ) {
		dprintf( D_ALWAYS, "DCSchedd:actOnJobs: Action failed\n" );
		return result_ad;
	}

		// Tell the schedd we're still here and ready to go
	rsock.encode();
	int answer = OK;
	if( ! (rsock.code(answer) && rsock.eom()) ) {
		dprintf( D_ALWAYS, "DCSchedd:actOnJobs: Can't send reply\n" );
		delete( result_ad );
		return NULL;
	}
	
		// finally, make sure the schedd didn't blow up trying to
		// commit these changes to the job queue...
	rsock.decode();
	if( ! (rsock.code(reply) && rsock.eom()) ) {
		dprintf( D_ALWAYS, "DCSchedd:actOnJobs: "
				 "Can't read confirmation from %s\n", _addr );
		delete( result_ad );
		return NULL;
	}

	return result_ad;
}


// // // // // // //
// JobActionResults
// // // // // // //


JobActionResults::JobActionResults( action_result_type_t res_type )
{
	result_type = res_type;
	result_ad = NULL;

	ar_success = 0;
	ar_not_found = 0;
	ar_permission_denied = 0;
	ar_bad_status = 0;
	ar_already_done = 0;
	ar_error = 0;
}


JobActionResults::~JobActionResults()
{
	if( result_ad ) {
		delete( result_ad );
	}
}


void
JobActionResults::record( PROC_ID job_id, action_result_t result ) 
{
	char buf[64];

	if( ! result_ad ) {
		result_ad = new ClassAd();
	}

	if( result_type == AR_LONG ) {
			// Put it directly in our ad
		sprintf( buf, "job_%d_%d = %d", job_id.cluster, job_id.proc,
				 (int)result );
		result_ad->Insert( buf );
		return;
	}

		// otherwise, we just want totals, so record it and we'll
		// publish all those at the end of the action...
	switch( result ) {
	case AR_SUCCESS:
		ar_success++;
		break;
	case AR_ERROR:
		ar_error++;
		break;
	case AR_NOT_FOUND:
		ar_not_found++;
		break;
	case AR_PERMISSION_DENIED: 
		ar_permission_denied++;
		break;
	case AR_BAD_STATUS:
		ar_bad_status++;
		break;
	case AR_ALREADY_DONE:
		ar_already_done++;
		break;
	}
}


void
JobActionResults::readResults( ClassAd* ad ) 
{
	char attr_name[64];

	if( ! ad ) {
		return;
	}

	if( result_ad ) {
		delete( result_ad );
	}
	result_ad = new ClassAd( *ad );

	action = JA_ERROR;
	int tmp = 0;
	if( ad->LookupInteger(ATTR_JOB_ACTION, tmp) ) {
		switch( tmp ) {
		case JA_HOLD_JOBS:
		case JA_REMOVE_JOBS:
		case JA_RELEASE_JOBS:
			action = (job_action_t)tmp;
			break;
		default:
			action = JA_ERROR;
		}
	}

	tmp = 0;
	result_type = AR_TOTALS;
	if( ad->LookupInteger(ATTR_ACTION_RESULT_TYPE, tmp) ) {
		if( tmp == AR_LONG ) {
			result_type = AR_LONG;
		}
	}

	sprintf( attr_name, "result_total_%d", AR_ERROR );
	ad->LookupInteger( attr_name, ar_error );

	sprintf( attr_name, "result_total_%d", AR_SUCCESS );
	ad->LookupInteger( attr_name, ar_success );

	sprintf( attr_name, "result_total_%d", AR_NOT_FOUND );
	ad->LookupInteger( attr_name, ar_not_found );

	sprintf( attr_name, "result_total_%d", AR_BAD_STATUS );
	ad->LookupInteger( attr_name, ar_bad_status );

	sprintf( attr_name, "result_total_%d", AR_ALREADY_DONE );
	ad->LookupInteger( attr_name, ar_already_done );

	sprintf( attr_name, "result_total_%d", AR_PERMISSION_DENIED );
	ad->LookupInteger( attr_name, ar_permission_denied );

}


ClassAd*
JobActionResults::publishResults( void ) 
{
	char buf[128];

		// no matter what they want, give them a few things of
		// interest, like what kind of results we're giving them. 
	if( ! result_ad ) {
		result_ad = new ClassAd();
	}

	sprintf( buf, "%s = %d", ATTR_ACTION_RESULT_TYPE, 
			 (int)result_type ); 
	result_ad->Insert( buf );

	if( result_type == AR_LONG ) {
			// we've got everything we need in our ad already, nothing
			// to do.
		return result_ad;
	}

		// They want totals for each possible result
	sprintf( buf, "result_total_%d = %d", AR_ERROR, ar_error );
	result_ad->Insert( buf );

	sprintf( buf, "result_total_%d = %d", AR_SUCCESS,
			 ar_success ); 
	result_ad->Insert( buf );
		
	sprintf( buf, "result_total_%d = %d", AR_NOT_FOUND,
			 ar_not_found ); 
	result_ad->Insert( buf );

	sprintf( buf, "result_total_%d = %d", AR_BAD_STATUS,
			 ar_bad_status );
	result_ad->Insert( buf );

	sprintf( buf, "result_total_%d = %d", AR_ALREADY_DONE,
			 ar_already_done );
	result_ad->Insert( buf );

	sprintf( buf, "result_total_%d = %d", AR_PERMISSION_DENIED,
			 ar_permission_denied );
	result_ad->Insert( buf );

	return result_ad;
}


action_result_t
JobActionResults::getResult( PROC_ID job_id )
{
	char buf[64];
	int result;

	if( ! result_ad ) { 
		return AR_ERROR;
	}
	sprintf( buf, "job_%d_%d", job_id.cluster, job_id.proc );
	if( ! result_ad->LookupInteger(buf, result) ) {
		return AR_ERROR;
	}
	return (action_result_t) result;
}


bool
JobActionResults::getResultString( PROC_ID job_id, char** str )
{
	char buf[1024];
	action_result_t result;
	bool rval = false;

	if( ! str ) {
		return false;
	}

	result = getResult( job_id );

		// construct the appropriate string based on the result and
		// the action

	switch( result ) {

	case AR_SUCCESS:
		sprintf( buf, "Job %d.%d %s", job_id.cluster, job_id.proc,
				 (action==JA_REMOVE_JOBS)?"marked for removal":
				 (action==JA_HOLD_JOBS)?"held":"released" );
		rval = true;
		break;

	case AR_ERROR:
		sprintf( buf, "No result found for job %d.%d", job_id.cluster,
				 job_id.proc );
		break;

	case AR_NOT_FOUND:
		sprintf( buf, "Job %d.%d not found", job_id.cluster,
				 job_id.proc ); 
		break;

	case AR_PERMISSION_DENIED: 
		sprintf( buf, "Permission denied to %s job %d.%d", 
				 (action==JA_REMOVE_JOBS)?"remove":
				 (action==JA_HOLD_JOBS)?"hold":"release", 
				 job_id.cluster, job_id.proc );
		break;

	case AR_BAD_STATUS:
		if( action == JA_RELEASE_JOBS ) {
			sprintf( buf, "Job %d.%d not held to be released", 
					 job_id.cluster, job_id.proc );
		} else {
				// Nothing else should use this.
			sprintf( buf, "Invalid result for job %d.%d", 
					 job_id.cluster, job_id.proc );
		}
		break;

	case AR_ALREADY_DONE:
		if( action == JA_HOLD_JOBS ) {
			sprintf( buf, "Job %d.%d already held", 
					 job_id.cluster, job_id.proc );
		} else if( action == JA_REMOVE_JOBS ) { 
			sprintf( buf, "Job %d.%d already marked for removal",
					 job_id.cluster, job_id.proc );
		} else {
				// we should have gotten AR_BAD_STATUS if we tried to
				// release a job that wasn't held...
			sprintf( buf, "Invalid result for job %d.%d", 
					 job_id.cluster, job_id.proc );
		}
		break;

	}
	*str = strdup( buf );
	return rval;
}


