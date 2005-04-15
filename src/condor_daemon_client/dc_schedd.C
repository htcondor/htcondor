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
#include "condor_debug.h"
#include "condor_classad.h"
#include "condor_commands.h"
#include "condor_attributes.h"
#include "daemon.h"
#include "dc_schedd.h"
#include "proc.h"
#include "file_transfer.h"
#include "condor_version.h"


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
					CondorError * errstack,
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
					  notify_scheduler, errstack );
}


ClassAd*
DCSchedd::removeJobs( const char* constraint, const char* reason,
					  CondorError * errstack,
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
					  notify_scheduler, errstack );
}


ClassAd*
DCSchedd::removeXJobs( const char* constraint, const char* reason,
					   CondorError * errstack,
					   action_result_type_t result_type,
					   bool notify_scheduler )
{
	if( ! constraint ) {
		dprintf( D_ALWAYS, "DCSchedd::removeXJobs: "
				 "constraint is NULL, aborting\n" );
		return NULL;
	}
	return actOnJobs( JA_REMOVE_X_JOBS, "removeX", constraint, NULL,
					  reason, ATTR_REMOVE_REASON, result_type,
					  notify_scheduler, errstack );
}


ClassAd*
DCSchedd::releaseJobs( const char* constraint, const char* reason,
					   CondorError * errstack,
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
					  notify_scheduler, errstack );
}


ClassAd*
DCSchedd::holdJobs( StringList* ids, const char* reason,
					CondorError * errstack,
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
					  notify_scheduler, errstack );
}


ClassAd*
DCSchedd::removeJobs( StringList* ids, const char* reason,
					CondorError * errstack,
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
					  notify_scheduler, errstack );
}


ClassAd*
DCSchedd::removeXJobs( StringList* ids, const char* reason,
					   CondorError * errstack,
					   action_result_type_t result_type,
					   bool notify_scheduler )
{
	if( ! ids ) {
		dprintf( D_ALWAYS, "DCSchedd::removeXJobs: "
				 "list of jobs is NULL, aborting\n" );
		return NULL;
	}
	return actOnJobs( JA_REMOVE_X_JOBS, "removeX", NULL, ids,
					  reason, ATTR_REMOVE_REASON, result_type,
					  notify_scheduler, errstack );
}


ClassAd*
DCSchedd::releaseJobs( StringList* ids, const char* reason,
					   CondorError * errstack,
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
					  notify_scheduler, errstack );
}


ClassAd*
DCSchedd::vacateJobs( const char* constraint, VacateType vacate_type,
					  CondorError * errstack,
					  action_result_type_t result_type,
					  bool notify_scheduler )
{
	if( ! constraint ) {
		dprintf( D_ALWAYS, "DCSchedd::vacateJobs: "
				 "constraint is NULL, aborting\n" );
		return NULL;
	}
	JobAction cmd;
	const char* cmd_str = NULL;
	if( vacate_type == VACATE_FAST ) {
		cmd = JA_VACATE_FAST_JOBS;
		cmd_str = "vacate-fast";
	} else {
		cmd = JA_VACATE_JOBS;
		cmd_str = "vacate";
	}
	return actOnJobs( cmd, cmd_str, constraint, NULL, NULL, NULL,
					  result_type, notify_scheduler, errstack );
}


ClassAd*
DCSchedd::vacateJobs( StringList* ids, VacateType vacate_type,
					  CondorError * errstack,
					  action_result_type_t result_type,
					  bool notify_scheduler )
{
	if( ! ids ) {
		dprintf( D_ALWAYS, "DCSchedd::vacateJobs: "
				 "list of jobs is NULL, aborting\n" );
		return NULL;
	}
	JobAction cmd;
	const char* cmd_str = NULL;
	if( vacate_type == VACATE_FAST ) {
		cmd = JA_VACATE_FAST_JOBS;
		cmd_str = "vacate-fast";
	} else {
		cmd = JA_VACATE_JOBS;
		cmd_str = "vacate";
	}
	return actOnJobs( cmd, cmd_str, NULL, ids, NULL, NULL,
					  result_type, notify_scheduler, errstack );
}


bool
DCSchedd::reschedule()
{
	return sendCommand(RESCHEDULE, Stream::safe_sock, 0);
}

bool
DCSchedd::receiveJobSandbox(const char* constraint, CondorError * errstack)
{
	ExprTree *tree = NULL, *lhs = NULL, *rhs = NULL;
	char *lhstr, *rhstr;
	int reply;
	int i;
	ReliSock rsock;
	int JobAdsArrayLen;
	bool use_new_command = true;

	if ( version() ) {
		CondorVersionInfo vi( version() );
		if ( vi.built_since_version(6,7,7) ) {
			use_new_command = true;
		} else {
			use_new_command = false;
		}
	}

		// // // // // // // //
		// On the wire protocol
		// // // // // // // //

	rsock.timeout(20);   // years of research... :)
	if( ! rsock.connect(_addr) ) {
		dprintf( D_ALWAYS, "DCSchedd::receiveJobSandbox: "
				 "Failed to connect to schedd (%s)\n", _addr );
		return false;
	}
	if ( use_new_command ) {
		if( ! startCommand(TRANSFER_DATA_WITH_PERMS, (Sock*)&rsock) ) {
			dprintf( D_ALWAYS, "DCSchedd::receiveJobSandbox: "
					 "Failed to send command (TRANSFER_DATA_WITH_PERMS) "
					 "to the schedd\n" );
			return false;
		}
	} else {
		if( ! startCommand(TRANSFER_DATA, (Sock*)&rsock) ) {
			dprintf( D_ALWAYS, "DCSchedd::receiveJobSandbox: "
					 "Failed to send command (TRANSFER_DATA) "
					 "to the schedd\n" );
			return false;
		}
	}

		// First, if we're not already authenticated, force that now. 
	if (!forceAuthentication( &rsock, errstack )) {
		dprintf( D_ALWAYS, 
			"DCSchedd::receiveJobSandbox: authentication failure: %s\n",
			errstack->getFullText() );
		return false;
	}

	rsock.encode();

		// Send our version if using the new command
	if ( use_new_command ) {
			// Need to use a named variable, else the wrong version of	
			// code() is called.
		char *my_version = strdup( CondorVersion() );
		if ( !rsock.code(my_version) ) {
			dprintf(D_ALWAYS,"DCSchedd:spoolJobFiles: "
					"Can't send version string to the schedd\n");
			free( my_version );
			return false;
		}
		free( my_version );
	}

		// Send the constraint
	char * nc_constraint = (char*) constraint;	// cast away const... sigh.
	if ( !rsock.code(nc_constraint) ) {
		dprintf(D_ALWAYS,"DCSchedd:receiveJobSandbox: "
				"Can't send JobAdsArrayLen to the schedd\n");
		return false;
	}

	rsock.eom();

		// Now, read how many jobs matched the constraint.
	rsock.decode();
	if ( !rsock.code(JobAdsArrayLen) ) {
		dprintf(D_ALWAYS,"DCSchedd:receiveJobSandbox: "
				"Can't receive JobAdsArrayLen from the schedd\n");
		return false;
	}

	rsock.eom();

		// Now read all the files via the file transfer object
	for (i=0; i<JobAdsArrayLen; i++) {
		FileTransfer ftrans;
		ClassAd job;

			// grab job ClassAd
		if ( !job.initFromStream(rsock) ) {
			dprintf(D_ALWAYS,"DCSchedd:receiveJobSandbox: "
				"Can't receive job ad %d from the schedd\n", i+1);
			return false;
		}

		rsock.eom();

			// translate the job ad by replacing the 
			// saved SUBMIT_ attributes
		job.ResetExpr();
		while( (tree = job.NextExpr()) ) {
			lhstr = NULL;
			if( (lhs = tree->LArg()) ) { 
				lhs->PrintToNewStr (&lhstr); 
			}
			if ( lhstr && strncasecmp("SUBMIT_",lhstr,7)==0 ) {
					// this attr name starts with SUBMIT_
					// compute new lhs (strip off the SUBMIT_)
				char *new_attr_name = strchr(lhstr,'_');
				ASSERT(new_attr_name);
				new_attr_name++;
					// compute new rhs (just use the same)
				rhstr = NULL;
				if( (rhs = tree->RArg()) ) { 
					rhs->PrintToNewStr (&rhstr); 
				}
					// insert attribute
				if(rhstr) {
					MyString newattr;
					newattr += new_attr_name;
					newattr += "=";
					newattr += rhstr;
					job.Insert(newattr.Value());
					free(rhstr);
				}
			}
			if ( lhstr ) {
				free(lhstr);
			}
		}	// while next expr

		if ( !ftrans.SimpleInit(&job,false,false,&rsock) ) {
			return false;
		}
		if ( use_new_command ) {
			ftrans.setPeerVersion( version() );
		}
		if ( !ftrans.DownloadFiles() ) {
			return false;
		}
	}	
		
		
	rsock.eom();

	rsock.encode();

	reply = OK;
	rsock.code(reply);
	rsock.eom();

	return true;
}


bool 
DCSchedd::spoolJobFiles(int JobAdsArrayLen, ClassAd* JobAdsArray[], CondorError * errstack)
{
	int reply;
	int i;
	ReliSock rsock;
	bool use_new_command = true;

	if ( version() ) {
		CondorVersionInfo vi( version() );
		if ( vi.built_since_version(6,7,7) ) {
			use_new_command = true;
		} else {
			use_new_command = false;
		}
	}

		// // // // // // // //
		// On the wire protocol
		// // // // // // // //

	rsock.timeout(60*60*8);   // years of research... :)
	if( ! rsock.connect(_addr) ) {
		dprintf( D_ALWAYS, "DCSchedd::spoolJobFiles: "
				 "Failed to connect to schedd (%s)\n", _addr );
		return false;
	}
	if ( use_new_command ) {
		if( ! startCommand(SPOOL_JOB_FILES_WITH_PERMS, (Sock*)&rsock, 0,
						   errstack) ) {
			dprintf( D_ALWAYS, "DCSchedd::spoolJobFiles: "
					 "Failed to send command (SPOOL_JOB_FILES_WITH_PERMS) "
					 "to the schedd\n" );
			return false;
		}
	} else {
		if( ! startCommand(SPOOL_JOB_FILES, (Sock*)&rsock, 0, errstack) ) {
			dprintf( D_ALWAYS, "DCSchedd::spoolJobFiles: "
					 "Failed to send command (SPOOL_JOB_FILES) "
					 "to the schedd\n" );
			return false;
		}
	}

		// First, if we're not already authenticated, force that now. 
	if (!forceAuthentication( &rsock, errstack )) {
		dprintf( D_ALWAYS, "DCSchedd: authentication failure: %s\n",
				 errstack->getFullText() );
		return false;
	}

	rsock.encode();

		// Send our version if using the new command
	if ( use_new_command ) {
			// Need to use a named variable, else the wrong version of	
			// code() is called.
		char *my_version = strdup( CondorVersion() );
		if ( !rsock.code(my_version) ) {
			dprintf(D_ALWAYS,"DCSchedd:spoolJobFiles: "
					"Can't send version string to the schedd\n");
			free( my_version );
			return false;
		}
		free( my_version );
	}

		// Send the number of jobs
	if ( !rsock.code(JobAdsArrayLen) ) {
		dprintf(D_ALWAYS,"DCSchedd:spoolJobFiles: "
				"Can't send JobAdsArrayLen to the schedd\n");
		return false;
	}

	rsock.eom();

		// Now, put the job ids onto the wire
	PROC_ID jobid;
	for (i=0; i<JobAdsArrayLen; i++) {
		if (!JobAdsArray[i]->LookupInteger(ATTR_CLUSTER_ID,jobid.cluster)) {
			dprintf(D_ALWAYS,"DCSchedd:spoolJobFiles: "
					"Job ad %d did not have a cluster id\n",i);
			return false;
		}
		if (!JobAdsArray[i]->LookupInteger(ATTR_PROC_ID,jobid.proc)) {
			dprintf(D_ALWAYS,"DCSchedd:spoolJobFiles: "
					"Job ad %d did not have a proc id\n",i);
			return false;
		}
		rsock.code(jobid);
	}

	rsock.eom();

		// Now send all the files via the file transfer object
	for (i=0; i<JobAdsArrayLen; i++) {
		FileTransfer ftrans;
		if ( !ftrans.SimpleInit(JobAdsArray[i], false, false, &rsock) ) {
			return false;
		}
		if ( use_new_command ) {
			ftrans.setPeerVersion( version() );
		}
		if ( !ftrans.UploadFiles(true,false) ) {
			return false;
		}
	}	
		
		
	rsock.eom();

	rsock.decode();

	reply = 0;
	rsock.code(reply);
	rsock.eom();

	if ( reply == 1 ) 
		return true;
	else
		return false;
}

bool 
DCSchedd::updateGSIcredential(const int cluster, const int proc, 
							  const char* path_to_proxy_file,
							  CondorError * errstack)
{
	int reply;
	ReliSock rsock;

		// check the parameters
	if ( cluster < 1 || proc < 0 || !path_to_proxy_file || !errstack ) {
		dprintf(D_FULLDEBUG,"DCSchedd::updateGSIcredential: bad parameters\n");
		return false;
	}

		// connect to the schedd, send the UPDATE_GSI_CRED command
	rsock.timeout(60*60*8);   // years of research... :)
	if( ! rsock.connect(_addr) ) {
		dprintf( D_ALWAYS, "DCSchedd::updateGSIcredential: "
				 "Failed to connect to schedd (%s)\n", _addr );
		return false;
	}
	if( ! startCommand(UPDATE_GSI_CRED, (Sock*)&rsock, 0, errstack) ) {
		dprintf( D_ALWAYS, "DCSchedd::updateGSIcredential: "
				 "Failed send command to the schedd: %s\n",
				 errstack->getFullText());
		return false;
	}


		// If we're not already authenticated, force that now. 
	if (!forceAuthentication( &rsock, errstack )) {
		dprintf( D_ALWAYS, 
				"DCSchedd:updateGSIcredential authentication failure: %s\n",
				 errstack->getFullText() );
		return false;
	}

		// Send the job id
	rsock.encode();
	PROC_ID jobid;
	jobid.cluster = cluster;
	jobid.proc = proc;	
	if ( !rsock.code(jobid) || !rsock.eom() ) {
		dprintf(D_ALWAYS,"DCSchedd:updateGSIcredential: "
				"Can't send jobid to the schedd\n");
		return false;
	}

		// Send the gsi proxy
	filesize_t file_size = 0;	// will receive the size of the file
	if ( rsock.put_file(&file_size,path_to_proxy_file) < 0 ) {
		dprintf(D_ALWAYS,
			"DCSchedd:updateGSIcredential "
			"failed to send proxy file %s (size=%d)\n",
			path_to_proxy_file, file_size);
		return false;
	}
		
		// Fetch the result
	rsock.decode();
	reply = 0;
	rsock.code(reply);
	rsock.eom();

	if ( reply == 1 ) 
		return true;
	else
		return false;
}

ClassAd*
DCSchedd::actOnJobs( JobAction action, const char* action_str, 
					 const char* constraint, StringList* ids, 
					 const char* reason, const char* reason_attr,
					 action_result_type_t result_type,
					 bool notify_scheduler,
					 CondorError * errstack )
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
	if( ! startCommand(ACT_ON_JOBS, (Sock*)&rsock, 0, errstack) ) {
		dprintf( D_ALWAYS, "DCSchedd::actOnJobs: "
				 "Failed to send command (ACT_ON_JOBS) to the schedd\n" );
		return NULL;
	}
		// First, if we're not already authenticated, force that now. 
	if (!forceAuthentication( &rsock, errstack )) {
		dprintf( D_ALWAYS, "DCSchedd: authentication failure: %s\n",
				 errstack->getFullText() );
		return NULL;
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
		case JA_REMOVE_X_JOBS:
		case JA_RELEASE_JOBS:
		case JA_VACATE_JOBS:
		case JA_VACATE_FAST_JOBS:
			action = (JobAction)tmp;
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
				 (action==JA_REMOVE_X_JOBS)?
				 "removed locally (remote state unknown)":
				 (action==JA_HOLD_JOBS)?"held":
				 (action==JA_RELEASE_JOBS)?"released":
				 (action==JA_VACATE_JOBS)?"vacated":
				 (action==JA_VACATE_FAST_JOBS)?"fast-vacated":"ERROR" );
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
				 (action==JA_REMOVE_X_JOBS)?"force removal of":
				 (action==JA_HOLD_JOBS)?"hold":
				 (action==JA_RELEASE_JOBS)?"release":
				 (action==JA_VACATE_JOBS)?"vacate":
				 (action==JA_VACATE_FAST_JOBS)?"fast-vacate":"ERROR",
				 job_id.cluster, job_id.proc );
		break;

	case AR_BAD_STATUS:
		if( action == JA_RELEASE_JOBS ) { 
			sprintf( buf, "Job %d.%d not held to be released", 
					 job_id.cluster, job_id.proc );
		} else if( action == JA_REMOVE_X_JOBS ) {
			sprintf( buf, "Job %d.%d not in `X' state to be forcibly removed", 
					 job_id.cluster, job_id.proc );
		} else if( action == JA_VACATE_JOBS ) {
			sprintf( buf, "Job %d.%d not running to be vacated", 
					 job_id.cluster, job_id.proc );
		} else if( action == JA_VACATE_FAST_JOBS ) {
			sprintf( buf, "Job %d.%d not running to be fast-vacated", 
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
		} else if( action == JA_REMOVE_X_JOBS ) { 
				// pfc: due to the immediate nature of a forced
				// remove, i'm not sure this should ever happen, but
				// just in case...
			sprintf( buf, "Job %d.%d already marked for forced removal",
					 job_id.cluster, job_id.proc );
		} else {
				// we should have gotten AR_BAD_STATUS if we tried to
				// act on a job that had already had the action done
			sprintf( buf, "Invalid result for job %d.%d", 
					 job_id.cluster, job_id.proc );
		}
		break;

	}
	*str = strdup( buf );
	return rval;
}

