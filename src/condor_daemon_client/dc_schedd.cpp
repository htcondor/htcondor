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
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_classad.h"
#include "condor_commands.h"
#include "condor_attributes.h"
#include "daemon.h"
#include "condor_daemon_core.h"
#include "dc_schedd.h"
#include "proc.h"
#include "file_transfer.h"
#include "condor_version.h"
#include "condor_ftp.h"

#include <sstream>

// // // // //
// DCSchedd
// // // // //


DCSchedd::DCSchedd( const char* the_name, const char* the_pool ) 
	: Daemon( DT_SCHEDD, the_name, the_pool )
{
}

DCSchedd::DCSchedd( const ClassAd& ad, const char* the_pool )
	: Daemon( &ad, DT_SCHEDD, the_pool )
{
}

DCSchedd::~DCSchedd( void )
{
}


ClassAd*
DCSchedd::holdJobs( const char* constraint, const char* reason,
					const char *reason_code,
					CondorError * errstack,
					action_result_type_t result_type )
{
	if( ! constraint ) {
		dprintf( D_ALWAYS, "DCSchedd::holdJobs: "
				 "constraint is NULL, aborting\n" );
		return NULL;
	}
	return actOnJobs( JA_HOLD_JOBS, constraint, NULL, 
					  reason, ATTR_HOLD_REASON, reason_code, ATTR_HOLD_REASON_SUBCODE, result_type,
					  errstack );
}


ClassAd*
DCSchedd::removeJobs( const char* constraint, const char* reason,
					  CondorError * errstack,
					  action_result_type_t result_type )
{
	if( ! constraint ) {
		dprintf( D_ALWAYS, "DCSchedd::removeJobs: "
				 "constraint is NULL, aborting\n" );
		return NULL;
	}
	return actOnJobs( JA_REMOVE_JOBS, constraint, NULL,
					  reason, ATTR_REMOVE_REASON, NULL, NULL, result_type,
					  errstack );
}


ClassAd*
DCSchedd::removeXJobs( const char* constraint, const char* reason,
					   CondorError * errstack,
					   action_result_type_t result_type )
{
	if( ! constraint ) {
		dprintf( D_ALWAYS, "DCSchedd::removeXJobs: "
				 "constraint is NULL, aborting\n" );
		return NULL;
	}
	return actOnJobs( JA_REMOVE_X_JOBS, constraint, NULL,
					  reason, ATTR_REMOVE_REASON, NULL, NULL, result_type,
					  errstack );
}


ClassAd*
DCSchedd::releaseJobs( const char* constraint, const char* reason,
					   CondorError * errstack,
					   action_result_type_t result_type )
{
	if( ! constraint ) {
		dprintf( D_ALWAYS, "DCSchedd::releaseJobs: "
				 "constraint is NULL, aborting\n" );
		return NULL;
	}
	return actOnJobs( JA_RELEASE_JOBS, constraint, NULL,
					  reason, ATTR_RELEASE_REASON, NULL, NULL, result_type,
					  errstack );
}


ClassAd*
DCSchedd::holdJobs( StringList* ids, const char* reason,
					const char* reason_code,
					CondorError * errstack,
					action_result_type_t result_type )
{
	if( ! ids ) {
		dprintf( D_ALWAYS, "DCSchedd::holdJobs: "
				 "list of jobs is NULL, aborting\n" );
		return NULL;
	}
	return actOnJobs( JA_HOLD_JOBS, NULL, ids, reason,
					  ATTR_HOLD_REASON,
					  reason_code, ATTR_HOLD_REASON_SUBCODE,
					  result_type,
					  errstack );
}


ClassAd*
DCSchedd::removeJobs( StringList* ids, const char* reason,
					CondorError * errstack,
					action_result_type_t result_type )
{
	if( ! ids ) {
		dprintf( D_ALWAYS, "DCSchedd::removeJobs: "
				 "list of jobs is NULL, aborting\n" );
		return NULL;
	}
	return actOnJobs( JA_REMOVE_JOBS, NULL, ids,
					  reason, ATTR_REMOVE_REASON, NULL, NULL, result_type,
					  errstack );
}


ClassAd*
DCSchedd::removeXJobs( StringList* ids, const char* reason,
					   CondorError * errstack,
					   action_result_type_t result_type )
{
	if( ! ids ) {
		dprintf( D_ALWAYS, "DCSchedd::removeXJobs: "
				 "list of jobs is NULL, aborting\n" );
		return NULL;
	}
	return actOnJobs( JA_REMOVE_X_JOBS, NULL, ids,
					  reason, ATTR_REMOVE_REASON, NULL, NULL, result_type,
					  errstack );
}


ClassAd*
DCSchedd::releaseJobs( StringList* ids, const char* reason,
					   CondorError * errstack,
					   action_result_type_t result_type )
{
	if( ! ids ) {
		dprintf( D_ALWAYS, "DCSchedd::releaseJobs: "
				 "list of jobs is NULL, aborting\n" );
		return NULL;
	}
	return actOnJobs( JA_RELEASE_JOBS, NULL, ids,
					  reason, ATTR_RELEASE_REASON, NULL, NULL, result_type,
					  errstack );
}


ClassAd*
DCSchedd::vacateJobs( const char* constraint, VacateType vacate_type,
					  CondorError * errstack,
					  action_result_type_t result_type )
{
	if( ! constraint ) {
		dprintf( D_ALWAYS, "DCSchedd::vacateJobs: "
				 "constraint is NULL, aborting\n" );
		return NULL;
	}
	JobAction cmd;
	if( vacate_type == VACATE_FAST ) {
		cmd = JA_VACATE_FAST_JOBS;
	} else {
		cmd = JA_VACATE_JOBS;
	}
	return actOnJobs( cmd, constraint, NULL, NULL, NULL, NULL, NULL,
					  result_type, errstack );
}


ClassAd*
DCSchedd::vacateJobs( StringList* ids, VacateType vacate_type,
					  CondorError * errstack,
					  action_result_type_t result_type )
{
	if( ! ids ) {
		dprintf( D_ALWAYS, "DCSchedd::vacateJobs: "
				 "list of jobs is NULL, aborting\n" );
		return NULL;
	}
	JobAction cmd;
	if( vacate_type == VACATE_FAST ) {
		cmd = JA_VACATE_FAST_JOBS;
	} else {
		cmd = JA_VACATE_JOBS;
	}
	return actOnJobs( cmd, NULL, ids, NULL, NULL, NULL, NULL,
					  result_type, errstack );
}


ClassAd*
DCSchedd::suspendJobs( StringList* ids, const char* reason,
					CondorError * errstack,
					action_result_type_t result_type )
{
	if( ! ids ) {
		dprintf( D_ALWAYS, "DCSchedd::suspendJobs: "
				 "list of jobs is NULL, aborting\n" );
		return NULL;
	}
	return actOnJobs( JA_SUSPEND_JOBS, NULL, ids,
					  reason, ATTR_SUSPEND_REASON, NULL, NULL, result_type,
					  errstack );
}


ClassAd*
DCSchedd::suspendJobs( const char* constraint, const char* reason,
					  CondorError * errstack,
					  action_result_type_t result_type )
{
	if( ! constraint ) {
		dprintf( D_ALWAYS, "DCSchedd::suspendJobs: "
				 "constraint is NULL, aborting\n" );
		return NULL;
	}
	return actOnJobs( JA_SUSPEND_JOBS, constraint, NULL,
					  reason, ATTR_SUSPEND_REASON, NULL, NULL, result_type,
					  errstack );
}

ClassAd*
DCSchedd::continueJobs( StringList* ids, const char* reason,
					CondorError * errstack,
					action_result_type_t result_type )
{
	if( ! ids ) {
		dprintf( D_ALWAYS, "DCSchedd::continueJobs: "
				 "list of jobs is NULL, aborting\n" );
		return NULL;
	}
	return actOnJobs( JA_CONTINUE_JOBS, NULL, ids,
					  reason, ATTR_CONTINUE_REASON, NULL, NULL, result_type,
					  errstack );
}


ClassAd*
DCSchedd::continueJobs( const char* constraint, const char* reason,
					  CondorError * errstack,
					  action_result_type_t result_type )
{
	if( ! constraint ) {
		dprintf( D_ALWAYS, "DCSchedd::continueJobs: "
				 "constraint is NULL, aborting\n" );
		return NULL;
	}
	return actOnJobs( JA_CONTINUE_JOBS, constraint, NULL,
					  reason, ATTR_CONTINUE_REASON, NULL, NULL, result_type,
					  errstack );
}


ClassAd*
DCSchedd::clearDirtyAttrs( StringList* ids, CondorError * errstack,
                                         action_result_type_t result_type )
{
	if( ! ids ) {
		dprintf( D_ALWAYS, "DCSchedd::clearDirtyAttrs: "
				"list of jobs is NULL, aborting\n" );
		return NULL;
	}
	return actOnJobs( JA_CLEAR_DIRTY_JOB_ATTRS, NULL, ids, NULL, NULL,
					  NULL, NULL, result_type, errstack );
}


bool
DCSchedd::reschedule()
{
	return sendCommand(RESCHEDULE, hasUDPCommandPort() ? Stream::safe_sock : Stream::reli_sock, 0);
}

bool 
DCSchedd::receiveJobSandbox(const char* constraint, CondorError * errstack, int * numdone /*=0*/)
{
	if(numdone) { *numdone = 0; }
	ExprTree *tree = NULL;
	const char *lhstr;
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
		if ( errstack ) {
			errstack->push( "DCSchedd::receiveJobSandbox",
							CEDAR_ERR_CONNECT_FAILED,
							"Failed to connect to schedd" );
		}
		return false;
	}
	if ( use_new_command ) {
		if( ! startCommand(TRANSFER_DATA_WITH_PERMS, (Sock*)&rsock, 0,
						   errstack) ) {
			dprintf( D_ALWAYS, "DCSchedd::receiveJobSandbox: "
					 "Failed to send command (TRANSFER_DATA_WITH_PERMS) "
					 "to the schedd\n" );
			return false;
		}
	} else {
		if( ! startCommand(TRANSFER_DATA, (Sock*)&rsock, 0, errstack) ) {
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
			errstack ? errstack->getFullText().c_str() : "" );
		return false;
	}

	// If we don't already know the version of the schedd, try to pull
	// it out of CEDAR. It's important to know for the FileTransfer
	// protocol.
	const CondorVersionInfo *peer_version = rsock.get_peer_version();
	if ( _version == NULL && peer_version != NULL ) {
		_version = peer_version->get_version_string();
	}
	if ( _version == NULL ) {
		dprintf( D_ALWAYS, "Unable to determine schedd version for file transfer\n" );
	}

	rsock.encode();

		// Send our version if using the new command
	if ( use_new_command ) {
		if ( !rsock.put( CondorVersion() ) ) {
			dprintf(D_ALWAYS,"DCSchedd:receiveJobSandbox: "
					"Can't send version string to the schedd\n");
			if ( errstack ) {
				errstack->push( "DCSchedd::receiveJobSandbox",
								CEDAR_ERR_PUT_FAILED,
								"Can't send version string to the schedd" );
			}
			return false;
		}
	}

		// Send the constraint
	if ( !rsock.put(constraint) ) {
		dprintf(D_ALWAYS,"DCSchedd:receiveJobSandbox: "
				"Can't send JobAdsArrayLen to the schedd\n");
		if ( errstack ) {
			errstack->push( "DCSchedd::receiveJobSandbox",
							CEDAR_ERR_PUT_FAILED,
							"Can't send JobAdsArrayLen to the schedd" );
		}
		return false;
	}

	if ( !rsock.end_of_message() ) {
		std::string errmsg;
		formatstr(errmsg,
				"Can't send initial message (version + constraint) to schedd (%s), probably an authorization failure",
				_addr);

		dprintf(D_ALWAYS,"DCSchedd::receiveJobSandbox: %s\n", errmsg.c_str());

		if( errstack ) {
			errstack->push(
				"DCSchedd::receiveJobSandbox",
				CEDAR_ERR_EOM_FAILED,
				errmsg.c_str());
		}
		return false;
	}

		// Now, read how many jobs matched the constraint.
	rsock.decode();
	if ( !rsock.code(JobAdsArrayLen) ) {
		std::string errmsg;
		formatstr(errmsg,
				"Can't receive JobAdsArrayLen from the schedd (%s)",
				_addr);

		dprintf(D_ALWAYS,"DCSchedd::receiveJobSandbox: %s\n", errmsg.c_str());

		if( errstack ) {
			errstack->push(
				"DCSchedd::receiveJobSandbox",
				CEDAR_ERR_GET_FAILED,
				errmsg.c_str());
		}
		return false;
	}

	rsock.end_of_message();

	dprintf(D_FULLDEBUG,"DCSchedd:receiveJobSandbox: "
		"%d jobs matched my constraint (%s)\n",
		JobAdsArrayLen, constraint);

		// Now read all the files via the file transfer object
	for (i=0; i<JobAdsArrayLen; i++) {
		FileTransfer ftrans;
		ClassAd job;

			// grab job ClassAd
		if ( !getClassAd(&rsock, job) ) {
			std::string errmsg;
			formatstr(errmsg, "Can't receive job ad %d from the schedd", i);

			dprintf(D_ALWAYS, "DCSchedd::receiveJobSandbox: %s\n", errmsg.c_str());

			if( errstack ) {
				errstack->push(
							   "DCSchedd::receiveJobSandbox",
							   CEDAR_ERR_GET_FAILED,
							   errmsg.c_str());
			}
			return false;
		}

		rsock.end_of_message();

			// translate the job ad by replacing the 
			// saved SUBMIT_ attributes
		for ( auto itr = job.begin(); itr != job.end(); itr++ ) {
			lhstr = itr->first.c_str();
			tree = itr->second;
			if ( lhstr && strncasecmp("SUBMIT_",lhstr,7)==0 ) {
					// this attr name starts with SUBMIT_
					// compute new lhs (strip off the SUBMIT_)
				const char *new_attr_name = strchr(lhstr,'_');
				ExprTree * pTree;
				ASSERT(new_attr_name);
				new_attr_name++;
					// insert attribute
				pTree = tree->Copy();
				job.Insert(new_attr_name, pTree);
			}
		}

		if ( !ftrans.SimpleInit(&job,false,false,&rsock) ) {
			if( errstack ) {
				int cluster = -1, proc = -1;
				job.LookupInteger(ATTR_CLUSTER_ID,cluster);
				job.LookupInteger(ATTR_PROC_ID,proc);
				errstack->pushf(
					"DCSchedd::receiveJobSandbox",
					FILETRANSFER_INIT_FAILED,
					"File transfer initialization failed for target job %d.%d",
					cluster, proc );
			}
			return false;
		}
		// We want files to be copied to their final places, so apply
		// any filename remaps when downloading.
		if ( !ftrans.InitDownloadFilenameRemaps(&job) ) {
			return false;
		}
		if ( use_new_command ) {
			ftrans.setPeerVersion( version() );
		}
		if ( !ftrans.DownloadFiles() ) {
			if( errstack ) {
				FileTransfer::FileTransferInfo ft_info = ftrans.GetInfo();

				int cluster = -1, proc = -1;
				job.LookupInteger(ATTR_CLUSTER_ID,cluster);
				job.LookupInteger(ATTR_PROC_ID,proc);
				errstack->pushf(
					"DCSchedd::receiveJobSandbox",
					FILETRANSFER_DOWNLOAD_FAILED,
					"File transfer failed for target job %d.%d: %s",
					cluster, proc, ft_info.error_desc.c_str() );
			}
			return false;
		}
	}	
		
	rsock.end_of_message();

	rsock.encode();

	reply = OK;
	rsock.code(reply);
	rsock.end_of_message();

	if(numdone) { *numdone = JobAdsArrayLen; }

	return true;
}

// when a transferd registers itself, it identifies who it is. The connection
// is then held open and the schedd periodically might send more transfer
// requests to the transferd. Also, if the transferd dies, the schedd is 
// informed quickly and reliably due to the closed connection.
bool
DCSchedd::register_transferd(const std::string &sinful, const std::string &id, int timeout,
		ReliSock **regsock_ptr, CondorError *errstack) 
{
	ReliSock *rsock;
	int invalid_request = 0;
	ClassAd regad;
	ClassAd respad;
	std::string errstr;
	std::string reason;

	if (regsock_ptr != NULL) {
		// Our caller wants a pointer to the socket we used to succesfully
		// register the claim. The NULL pointer will represent failure and
		// this will only be set to something real if everything was ok.
		*regsock_ptr = NULL;
	}

	// This call with automatically connect to _addr, which was set in the
	// constructor of this object to be the schedd in question.
	rsock = (ReliSock*)startCommand(TRANSFERD_REGISTER, Stream::reli_sock,
		timeout, errstack);

	if( ! rsock ) {
		dprintf( D_ALWAYS, "DCSchedd::register_transferd: "
				 "Failed to send command (TRANSFERD_REGISTER) "
				 "to the schedd\n" );
		errstack->push("DC_SCHEDD", 1, 
			"Failed to start a TRANSFERD_REGISTER command.");
		return false;
	}

		// First, if we're not already authenticated, force that now. 
	if (!forceAuthentication( rsock, errstack )) {
		dprintf( D_ALWAYS, "DCSchedd::register_transferd authentication "
				"failure: %s\n", errstack->getFullText().c_str() );
		errstack->push("DC_SCHEDD", 1, 
			"Failed to authenticate properly.");
		return false;
	}

	rsock->encode();

	// set up my registration request.
	regad.Assign(ATTR_TREQ_TD_SINFUL, sinful);
	regad.Assign(ATTR_TREQ_TD_ID, id);

	// This is the initial registration identification ad to the schedd
	// It contains:
	//	ATTR_TREQ_TD_SINFUL
	//	ATTR_TREQ_TD_ID
	putClassAd(rsock, regad);
	rsock->end_of_message();

	// Get the response from the schedd.
	rsock->decode();

	// This is the response ad from the schedd:
	// It contains:
	//	ATTR_TREQ_INVALID_REQUEST
	//
	// OR
	// 
	//	ATTR_TREQ_INVALID_REQUEST
	//	ATTR_TREQ_INVALID_REASON
	getClassAd(rsock, respad);
	rsock->end_of_message();

	respad.LookupInteger(ATTR_TREQ_INVALID_REQUEST, invalid_request);

	if (invalid_request == FALSE) {
		// not an invalid request
		if (regsock_ptr)
			*regsock_ptr = rsock;
		return true;
	}

	respad.LookupString(ATTR_TREQ_INVALID_REASON, reason);
	errstack->pushf("DC_SCHEDD", 1, "Schedd refused registration: %s", reason.c_str());

	return false;
}

/*
[Request Sandbox Location Ad]

FileTransferProtocol = "CondorInternalFileTransfer"
PeerVersion = "..."
HasConstraint = TRUE
Constraint = "(cluster == 120 && procid == 0)"

OR

FileTransferProtocol = "CondorInternalFileTransfer"
PeerVersion = "..."
HasConstraint = FALSE
NumJobIDs = 10
JobIDs = "12.0,12.1,12.2,12.3,12.4,12.5,12.6,12.7,12.8,12.9"
*/

// using jobids structure so the schedd doesn't have to iterate over all the
// job ads.
bool 
DCSchedd::requestSandboxLocation(int direction, 
	int JobAdsArrayLen, ClassAd* JobAdsArray[], int protocol, 
	ClassAd *respad, CondorError * errstack)
{
	StringList sl;
	ClassAd reqad;
	std::string str;
	int i;
	int cluster, proc;
	char *tmp = NULL;

	////////////////////////////////////////////////////////////////////////
	// This request knows exactly which jobs it wants to talk about, so
	// just compact them into the classad to send to the schedd.
	////////////////////////////////////////////////////////////////////////

	reqad.Assign(ATTR_TREQ_DIRECTION, direction);
	reqad.Assign(ATTR_TREQ_PEER_VERSION, CondorVersion());
	reqad.Assign(ATTR_TREQ_HAS_CONSTRAINT, false);

	for (i = 0; i < JobAdsArrayLen; i++) {
		if (!JobAdsArray[i]->LookupInteger(ATTR_CLUSTER_ID,cluster)) {
			dprintf(D_ALWAYS,"DCSchedd:requestSandboxLocation: "
					"Job ad %d did not have a cluster id\n",i);
			if ( errstack ) {
				errstack->pushf( "DCSchedd::requestSandboxLocation", 1,
								 "Job ad %d did not have a cluster id", i );
			}
			return false;
		}
		if (!JobAdsArray[i]->LookupInteger(ATTR_PROC_ID,proc)) {
			dprintf(D_ALWAYS,"DCSchedd:requestSandboxLocation(): "
					"Job ad %d did not have a proc id\n",i);
			if ( errstack ) {
				errstack->pushf( "DCSchedd::requestSandboxLocation", 1,
								 "Job ad %d did not have a proc id", i );
			}
			return false;
		}
		
		formatstr(str, "%d.%d", cluster, proc);

		// make something like: 1.0, 1.1, 1.2, ....
		sl.append(str.c_str());
	}

	tmp = sl.print_to_string();

	reqad.Assign(ATTR_TREQ_JOBID_LIST, tmp);

	free(tmp);
	tmp = NULL;

	// XXX This should be a realized function to convert between the
	// protocol enum and a string description. That way both functions can
	// use it and it won't seem so bad.
	switch(protocol) {
		case FTP_CFTP:
			reqad.Assign(ATTR_TREQ_FTP, FTP_CFTP);
			break;
		default:
			dprintf(D_ALWAYS, "DCSchedd::requestSandboxLocation(): "
				"Can't make a request for a sandbox with an unknown file "
				"transfer protocol!");
			if ( errstack ) {
				errstack->push( "DCSchedd::requestSandboxLocation", 1,
								"Unknown file transfer protocol" );
			}
			return false;
			break;
	}

	// now connect to the schedd and get the response.
	return requestSandboxLocation(&reqad, respad, errstack);
}

// using a constraint for which the schedd must iterate over all the jobads.
bool
DCSchedd::requestSandboxLocation(int direction, std::string & constraint,
	int protocol, ClassAd *respad, CondorError * errstack)
{
	ClassAd reqad;

	////////////////////////////////////////////////////////////////////////
	// This request specifies a collection of job ads as defined by a
	// constraint. Later, then the transfer actually happens to the transferd,
	// this constraint will get converted to actual job ads at that time and
	// the client will have to get them back so it knows what to send.
	////////////////////////////////////////////////////////////////////////

	reqad.Assign(ATTR_TREQ_DIRECTION, direction);
	reqad.Assign(ATTR_TREQ_PEER_VERSION, CondorVersion());
	reqad.Assign(ATTR_TREQ_HAS_CONSTRAINT, true);
	reqad.Assign(ATTR_TREQ_CONSTRAINT, constraint.c_str());

	// XXX This should be a realized function to convert between the
	// protocol enum and a string description. That way both functions can
	// use it and it won't seem so bad.
	switch(protocol) {
		case FTP_CFTP:
			reqad.Assign(ATTR_TREQ_FTP, FTP_CFTP);
			break;
		default:
			dprintf(D_ALWAYS, "DCSchedd::requestSandboxLocation(): "
				"Can't make a request for a sandbox with an unknown file "
				"transfer protocol!");
			if ( errstack ) {
				errstack->push( "DCSchedd::requestSandboxLocation", 1,
								"Unknown file transfer protocol" );
			}
			return false;
			break;
	}

	// now connect to the schedd and get the response.
	return requestSandboxLocation(&reqad, respad, errstack);
}


// I'm going to ask the schedd for where I can put the files for the jobs I've
// specified. The schedd is going to respond with A) a message telling me it
// has the answer right away, or B) an answer telling me I have to wait 
// an unknown length of time for the schedd to schedule me a place to put it.
bool 
DCSchedd::requestSandboxLocation(ClassAd *reqad, ClassAd *respad, 
	CondorError * errstack)
{
	ReliSock rsock;
	int will_block;
	ClassAd status_ad;

	rsock.timeout(20);   // years of research... :)
	if( ! rsock.connect(_addr) ) {
		dprintf( D_ALWAYS, "DCSchedd::requestSandboxLocation(): "
				 "Failed to connect to schedd (%s)\n", _addr );
		if ( errstack ) {
			errstack->push( "DCSchedd::requestSandboxLocation",
							CEDAR_ERR_CONNECT_FAILED,
							"Failed to connect to schedd" );
		}
		return false;
	}
	if( ! startCommand(REQUEST_SANDBOX_LOCATION, (Sock*)&rsock, 0,
						   errstack) ) {
		dprintf( D_ALWAYS, "DCSchedd::requestSandboxLocation(): "
				 "Failed to send command (REQUEST_SANDBOX_LOCATION) "
				 "to schedd (%s)\n", _addr );
		return false;
	}

		// First, if we're not already authenticated, force that now. 
	if (!forceAuthentication( &rsock, errstack )) {
		dprintf( D_ALWAYS, "DCSchedd: authentication failure: %s\n",
				 errstack->getFullText().c_str() );
		return false;
	}

	rsock.encode();

	///////////////////////////////////////////////////////////////////////
	// Send my sandbox location request packet to the schedd.
	///////////////////////////////////////////////////////////////////////

	// This request ad will either contain:
	//	ATTR_TREQ_PEER_VERSION
	//	ATTR_TREQ_HAS_CONSTRAINT
	//	ATTR_TREQ_JOBID_LIST
	//	ATTR_TREQ_FTP
	//
	// OR
	//
	//	ATTR_TREQ_DIRECTION
	//	ATTR_TREQ_PEER_VERSION
	//	ATTR_TREQ_HAS_CONSTRAINT
	//	ATTR_TREQ_CONSTRAINT
	//	ATTR_TREQ_FTP
	dprintf(D_ALWAYS, "Sending request ad.\n");
	if (putClassAd(&rsock, *reqad) != 1) {
		dprintf(D_ALWAYS,"DCSchedd:requestSandboxLocation(): "
				"Can't send reqad to the schedd\n");
		if ( errstack ) {
			errstack->push( "DCSchedd::requestSandboxLocation",
							CEDAR_ERR_PUT_FAILED,
							"Can't send reqad to the schedd" );
		}
		return false;
	}
	rsock.end_of_message();

	rsock.decode();

	///////////////////////////////////////////////////////////////////////
	// Read back a response ad which will tell me which jobs the schedd
	// said I could modify and whether or not I'm am going to have to block
	// before getting the payload of the transferd location/capability ad.
	///////////////////////////////////////////////////////////////////////

	// This status ad will contain
	//	ATTR_TREQ_INVALID_REQUEST (set to true)
	//	ATTR_TREQ_INVALID_REASON
	//
	// OR
	//	ATTR_TREQ_INVALID_REQUEST (set to false)
	//	ATTR_TREQ_JOBID_ALLOW_LIST
	//	ATTR_TREQ_JOBID_DENY_LIST
	//	ATTR_TREQ_WILL_BLOCK

	dprintf(D_ALWAYS, "Receiving status ad.\n");
	if (getClassAd(&rsock, status_ad) == false) {
		dprintf(D_ALWAYS, "Schedd closed connection to me. Aborting sandbox "
			"submission.\n");
		if ( errstack ) {
			errstack->push( "DCSchedd::requestSandboxLocation",
							CEDAR_ERR_GET_FAILED,
							"Schedd closed connection" );
		}
		return false;
	}
	rsock.end_of_message();

	status_ad.LookupInteger(ATTR_TREQ_WILL_BLOCK, will_block);

	dprintf(D_ALWAYS, "Client will %s\n", will_block==1?"block":"not block");

	if (will_block == 1) {
		// set to 20 minutes.
		rsock.timeout(60*20);
	}

	///////////////////////////////////////////////////////////////////////
	// Read back the payload ad from the schedd about the transferd location
	// and capability string I can use for the fileset I wish to transfer.
	///////////////////////////////////////////////////////////////////////

	// read back the response ad from the schedd which contains a 
	// td sinful string, and a capability. These represent my ability to
	// read/write a certain fileset somewhere.

	// This response ad from the schedd will contain:
	//
	//	ATTR_TREQ_INVALID_REQUEST (set to true)
	//	ATTR_TREQ_INVALID_REASON
	//
	// OR
	//
	//	ATTR_TREQ_INVALID_REQUEST (set to false)
	//	ATTR_TREQ_CAPABILITY
	//	ATTR_TREQ_TD_SINFUL
	//	ATTR_TREQ_JOBID_ALLOW_LIST

	dprintf(D_ALWAYS, "Receiving response ad.\n");
	if (getClassAd(&rsock, *respad) != true) {
		dprintf(D_ALWAYS,"DCSchedd:requestSandboxLocation(): "
				"Can't receive response ad from the schedd\n");
		if ( errstack ) {
			errstack->push( "DCSchedd::requestSandboxLocation",
							CEDAR_ERR_GET_FAILED,
							"Can't receive response ad from the schedd" );
		}
		return false;
	}
	rsock.end_of_message();

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

	rsock.timeout(20);   // years of research... :)
	if( ! rsock.connect(_addr) ) {
		std::string errmsg;
		formatstr(errmsg, "Failed to connect to schedd (%s)", _addr);

		dprintf( D_ALWAYS, "DCSchedd::spoolJobFiles: %s\n", errmsg.c_str() );

		if( errstack ) {
			errstack->push(
				"DCSchedd::spoolJobFiles",CEDAR_ERR_CONNECT_FAILED,
				errmsg.c_str() );
		}
		return false;
	}
	if ( use_new_command ) {
		if( ! startCommand(SPOOL_JOB_FILES_WITH_PERMS, (Sock*)&rsock, 0,
						   errstack) ) {

			dprintf( D_ALWAYS, "DCSchedd::spoolJobFiles: "
				"Failed to send command (SPOOL_JOB_FILES_WITH_PERMS) "
				"to the schedd (%s)\n", _addr );

			return false;
		}
	} else {
		if( ! startCommand(SPOOL_JOB_FILES, (Sock*)&rsock, 0, errstack) ) {
			dprintf( D_ALWAYS, "DCSchedd::spoolJobFiles: "
					 "Failed to send command (SPOOL_JOB_FILES) "
					 "to the schedd (%s)\n", _addr );

			return false;
		}
	}

		// First, if we're not already authenticated, force that now. 
	if (!forceAuthentication( &rsock, errstack )) {
		dprintf( D_ALWAYS, "DCSchedd: authentication failure: %s\n",
				 errstack ? errstack->getFullText().c_str() : "" );
		return false;
	}

	// If we don't already know the version of the schedd, try to pull
	// it out of CEDAR. It's important to know for the FileTransfer
	// protocol.
	const CondorVersionInfo *peer_version = rsock.get_peer_version();
	if ( _version == NULL && peer_version != NULL ) {
		_version = peer_version->get_version_string();
	}
	if ( _version == NULL ) {
		dprintf( D_ALWAYS, "Unable to determine schedd version for file transfer\n" );
	}

	rsock.encode();

		// Send our version if using the new command
	if ( use_new_command ) {
		if ( !rsock.put( CondorVersion() ) ) {
			dprintf(D_ALWAYS,"DCSchedd:spoolJobFiles: "
					"Can't send version string to the schedd\n");
			if ( errstack ) {
				errstack->push( "DCSchedd::spoolJobFiles",
								CEDAR_ERR_PUT_FAILED,
								"Can't send version string to the schedd" );
			}
			return false;
		}
	}

		// Send the number of jobs
	if ( !rsock.code(JobAdsArrayLen) ) {
		dprintf(D_ALWAYS,"DCSchedd:spoolJobFiles: "
				"Can't send JobAdsArrayLen to the schedd\n");
		if ( errstack ) {
			errstack->push( "DCSchedd::spoolJobFiles",
							CEDAR_ERR_PUT_FAILED,
							"Can't send JobAdsArrayLen to the schedd" );
		}
		return false;
	}

	if( !rsock.end_of_message() ) {
		std::string errmsg;
		formatstr(errmsg,
				"Can't send initial message (version + count) to schedd (%s), probably an authorization failure",
				_addr);

		dprintf(D_ALWAYS,"DCSchedd:spoolJobFiles: %s\n", errmsg.c_str());

		if( errstack ) {
			errstack->push(
				"DCSchedd::spoolJobFiles",
				CEDAR_ERR_EOM_FAILED,
				errmsg.c_str());
		}
		return false;
	}

		// Now, put the job ids onto the wire
	PROC_ID jobid;
	for (i=0; i<JobAdsArrayLen; i++) {
		if (!JobAdsArray[i]->LookupInteger(ATTR_CLUSTER_ID,jobid.cluster)) {
			dprintf(D_ALWAYS,"DCSchedd:spoolJobFiles: "
					"Job ad %d did not have a cluster id\n",i);
			if ( errstack ) {
				errstack->pushf( "DCSchedd::spoolJobFiles", 1,
								 "Job ad %d did not have a cluster id", i );
			}
			return false;
		}
		if (!JobAdsArray[i]->LookupInteger(ATTR_PROC_ID,jobid.proc)) {
			dprintf(D_ALWAYS,"DCSchedd:spoolJobFiles: "
					"Job ad %d did not have a proc id\n",i);
			if ( errstack ) {
				errstack->pushf( "DCSchedd::spoolJobFiles", 1,
								 "Job ad %d did not have a proc id", i );
			}
			return false;
		}
		rsock.code(jobid);
	}

	if( !rsock.end_of_message() ) {
		std::string errmsg;
		formatstr(errmsg, "Failed while sending job ids to schedd (%s)", _addr);

		dprintf(D_ALWAYS,"DCSchedd:spoolJobFiles: %s\n", errmsg.c_str());

		if( errstack ) {
			errstack->push(
				"DCSchedd::spoolJobFiles",
				CEDAR_ERR_EOM_FAILED,
				errmsg.c_str());
		}
		return false;
	}

		// Now send all the files via the file transfer object
	for (i=0; i<JobAdsArrayLen; i++) {
		FileTransfer ftrans;
		if ( !ftrans.SimpleInit(JobAdsArray[i], false, false, &rsock, PRIV_UNKNOWN, false, true) ) {
			if( errstack ) {
				int cluster = -1, proc = -1;
				if(JobAdsArray[i]) {
					JobAdsArray[i]->LookupInteger(ATTR_CLUSTER_ID,cluster);
					JobAdsArray[i]->LookupInteger(ATTR_PROC_ID,proc);
				}
				errstack->pushf(
					"DCSchedd::spoolJobFiles",
					FILETRANSFER_INIT_FAILED,
					"File transfer initialization failed for target job %d.%d",
					cluster, proc );
			}
			return false;
		}
		if ( use_new_command ) {
			ftrans.setPeerVersion( version() );
		}
		if ( !ftrans.UploadFiles(true,false) ) {
			if( errstack ) {
				FileTransfer::FileTransferInfo ft_info = ftrans.GetInfo();

				int cluster = -1, proc = -1;
				if(JobAdsArray[i]) {
					JobAdsArray[i]->LookupInteger(ATTR_CLUSTER_ID,cluster);
					JobAdsArray[i]->LookupInteger(ATTR_PROC_ID,proc);
				}
				errstack->pushf(
					"DCSchedd::spoolJobFiles",
					FILETRANSFER_UPLOAD_FAILED,
					"File transfer failed for target job %d.%d: %s",
					cluster, proc, ft_info.error_desc.c_str() );
			}
			return false;
		}
	}	
		
		
	rsock.end_of_message();

	rsock.decode();

	reply = 0;
	rsock.code(reply);
	rsock.end_of_message();

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
		if ( errstack ) {
			errstack->push( "DCSchedd::updateGSIcredential", 1,
							"bad parameters" );
		}
		return false;
	}

		// connect to the schedd, send the UPDATE_GSI_CRED command
	rsock.timeout(20);   // years of research... :)
	if( ! rsock.connect(_addr) ) {
		dprintf( D_ALWAYS, "DCSchedd::updateGSIcredential: "
				 "Failed to connect to schedd (%s)\n", _addr );
		if ( errstack ) {
			errstack->push( "DCSchedd::updateGSIcredential",
							CEDAR_ERR_CONNECT_FAILED,
							"Failed to connect to schedd" );
		}
		return false;
	}
	if( ! startCommand(UPDATE_GSI_CRED, (Sock*)&rsock, 0, errstack) ) {
		dprintf( D_ALWAYS, "DCSchedd::updateGSIcredential: "
				 "Failed send command to the schedd: %s\n",
				 errstack->getFullText().c_str());
		return false;
	}


		// If we're not already authenticated, force that now. 
	if (!forceAuthentication( &rsock, errstack )) {
		dprintf( D_ALWAYS, 
				"DCSchedd:updateGSIcredential authentication failure: %s\n",
				 errstack->getFullText().c_str() );
		return false;
	}

		// Send the job id
	rsock.encode();
	PROC_ID jobid;
	jobid.cluster = cluster;
	jobid.proc = proc;	
	if ( !rsock.code(jobid) || !rsock.end_of_message() ) {
		dprintf(D_ALWAYS,"DCSchedd:updateGSIcredential: "
				"Can't send jobid to the schedd, probably an authorization failure\n");
		if ( errstack ) {
			errstack->push( "DCSchedd::updateGSIcredential",
							CEDAR_ERR_PUT_FAILED,
							"Can't send jobid to the schedd, probably an authorization failure" );
		}
		return false;
	}

		// Send the gsi proxy
	filesize_t file_size = 0;	// will receive the size of the file
	if ( rsock.put_file(&file_size,path_to_proxy_file) < 0 ) {
		dprintf(D_ALWAYS,
			"DCSchedd:updateGSIcredential "
			"failed to send proxy file %s (size=%ld)\n",
			path_to_proxy_file, (long) file_size);
		if ( errstack ) {
			errstack->push( "DCSchedd::updateGSIcredential",
							CEDAR_ERR_PUT_FAILED,
							"Failed to send proxy file" );
		}
		return false;
	}
		
		// Fetch the result
	rsock.decode();
	reply = 0;
	rsock.code(reply);
	rsock.end_of_message();

	if ( reply == 1 ) 
		return true;
	else
		return false;
}

bool 
DCSchedd::delegateGSIcredential(const int cluster, const int proc, 
								const char* path_to_proxy_file,
								time_t expiration_time,
								time_t *result_expiration_time,
								CondorError * errstack)
{
	int reply;
	ReliSock rsock;

		// check the parameters
	if ( cluster < 1 || proc < 0 || !path_to_proxy_file || !errstack ) {
		dprintf(D_FULLDEBUG,"DCSchedd::delegateGSIcredential: bad parameters\n");
		if ( errstack ) {
			errstack->push( "DCSchedd::delegateGSIcredential", 1,
							"bad parameters" );
		}
		return false;
	}

		// connect to the schedd, send the DELEGATE_GSI_CRED_SCHEDD command
	rsock.timeout(20);   // years of research... :)
	if( ! rsock.connect(_addr) ) {
		dprintf( D_ALWAYS, "DCSchedd::delegateGSIcredential: "
				 "Failed to connect to schedd (%s)\n", _addr );
		if ( errstack ) {
			errstack->push( "DCSchedd::delegateGSIcredential",
							CEDAR_ERR_CONNECT_FAILED,
							"Failed to connect to schedd" );
		}
		return false;
	}
	if( ! startCommand(DELEGATE_GSI_CRED_SCHEDD, (Sock*)&rsock, 0, errstack) ) {
		dprintf( D_ALWAYS, "DCSchedd::delegateGSIcredential: "
				 "Failed send command to the schedd: %s\n",
				 errstack->getFullText().c_str());
		return false;
	}


		// If we're not already authenticated, force that now. 
	if (!forceAuthentication( &rsock, errstack )) {
		dprintf( D_ALWAYS, 
				"DCSchedd::delegateGSIcredential authentication failure: %s\n",
				 errstack->getFullText().c_str() );
		return false;
	}

		// Send the job id
	rsock.encode();
	PROC_ID jobid;
	jobid.cluster = cluster;
	jobid.proc = proc;	
	if ( !rsock.code(jobid) || !rsock.end_of_message() ) {
		dprintf(D_ALWAYS,"DCSchedd::delegateGSIcredential: "
				"Can't send jobid to the schedd, probably an authorization failure\n");
		if ( errstack ) {
			errstack->push( "DCSchedd::delegateGSIcredential",
							CEDAR_ERR_PUT_FAILED,
							"Can't send jobid to the schedd, probably an authorization failure" );
		}
		return false;
	}

		// Delegate the gsi proxy
	filesize_t file_size = 0;	// will receive the size of the file
	if ( rsock.put_x509_delegation(&file_size,path_to_proxy_file,expiration_time,result_expiration_time) < 0 ) {
		dprintf(D_ALWAYS,
			"DCSchedd::delegateGSIcredential "
			"failed to send proxy file %s\n",
			path_to_proxy_file);
		if ( errstack ) {
			errstack->push( "DCSchedd::delegateGSIcredential",
							CEDAR_ERR_PUT_FAILED,
							"Failed to send proxy file" );
		}
		return false;
	}
		
		// Fetch the result
	rsock.decode();
	reply = 0;
	rsock.code(reply);
	rsock.end_of_message();

	if ( reply == 1 )
		return true;
	else
		return false;
}

ClassAd*
DCSchedd::actOnJobs( JobAction action,
					 const char* constraint, StringList* ids,
					 const char* reason, const char* reason_attr,
					 const char* reason_code, const char* reason_code_attr,
					 action_result_type_t result_type,
					 CondorError * errstack )
{
	int reply;
	ReliSock rsock;

		// // // // // // // //
		// Construct the ad we want to send
		// // // // // // // //

	ClassAd cmd_ad;

	cmd_ad.Assign( ATTR_JOB_ACTION, action );
	
	cmd_ad.Assign( ATTR_ACTION_RESULT_TYPE, (int)result_type );

	if( constraint ) {
		if( ids ) {
				// This is a programming error, not a run-time one
			EXCEPT( "DCSchedd::actOnJobs has both constraint and ids!" );
		}
		if( ! cmd_ad.AssignExpr(ATTR_ACTION_CONSTRAINT, constraint) ) {
			dprintf( D_ALWAYS, "DCSchedd::actOnJobs: "
					 "Can't insert constraint (%s) into ClassAd!\n",
					 constraint );
			if ( errstack ) {
				errstack->push( "DCSchedd::actOnJobs", 1,
								"Can't insert constraint into ClassAd" );
			}
			return NULL;
		}			
	} else if( ids ) {
		std::string action_ids = ids->to_string();
		if (!action_ids.empty()) {
			cmd_ad.Assign( ATTR_ACTION_IDS, action_ids );
		}
	} else {
		EXCEPT( "DCSchedd::actOnJobs called without constraint or ids" );
	}

	if( reason_attr && reason ) {
		cmd_ad.Assign( reason_attr, reason );
	}

	if( reason_code_attr && reason_code ) {
		cmd_ad.AssignExpr(reason_code_attr,reason_code);
	}

		// // // // // // // //
		// On the wire protocol
		// // // // // // // //

	rsock.timeout(20);   // years of research... :)
	if( ! rsock.connect(_addr) ) {
		dprintf( D_ALWAYS, "DCSchedd::actOnJobs: "
				 "Failed to connect to schedd (%s)\n", _addr );
		if ( errstack ) {
			errstack->push( "DCSchedd::actOnJobs", CEDAR_ERR_CONNECT_FAILED,
							"Failed to connect to schedd" );
		}
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
				 errstack->getFullText().c_str() );
		return NULL;
	}

		// Now, put the command classad on the wire
	if( ! (putClassAd(&rsock, cmd_ad) && rsock.end_of_message()) ) {
		dprintf( D_ALWAYS, "DCSchedd:actOnJobs: Can't send classad, probably an authorization failure\n" );
		if ( errstack ) {
			errstack->push( "DCSchedd::actOnJobs", CEDAR_ERR_PUT_FAILED,
							"Can't send classad, probably an authorization failure" );
		}
		return NULL;
	}

		// Next, we need to read the reply from the schedd if things
		// are ok and it's going to go forward.  If the schedd can't
		// read our reply to this ClassAd, it assumes we got killed
		// and it should abort its transaction
	rsock.decode();
	ClassAd* result_ad = new ClassAd();
	if( ! (getClassAd(&rsock, *result_ad) && rsock.end_of_message()) ) {
		dprintf( D_ALWAYS, "DCSchedd:actOnJobs: "
				 "Can't read response ad from %s\n", _addr );
		if ( errstack ) {
			errstack->push( "DCSchedd::actOnJobs", CEDAR_ERR_GET_FAILED,
							"Can't read response ad" );
		}
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
	if( ! (rsock.code(answer) && rsock.end_of_message()) ) {
		dprintf( D_ALWAYS, "DCSchedd:actOnJobs: Can't send reply\n" );
		if ( errstack ) {
			errstack->push( "DCSchedd::actOnJobs", CEDAR_ERR_PUT_FAILED,
							"Can't send reply" );
		}
		delete( result_ad );
		return NULL;
	}
	
		// finally, make sure the schedd didn't blow up trying to
		// commit these changes to the job queue...
	rsock.decode();
	if( ! (rsock.code(reply) && rsock.end_of_message()) ) {
		dprintf( D_ALWAYS, "DCSchedd:actOnJobs: "
				 "Can't read confirmation from %s\n", _addr );
		if ( errstack ) {
			errstack->push( "DCSchedd::actOnJobs", CEDAR_ERR_GET_FAILED,
							"Can't read confirmation" );
		}
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
	action = JA_ERROR;
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
		if (job_id.proc < 0) {
			sprintf( buf, "cluster_%d", job_id.cluster );
		} else {
			sprintf( buf, "job_%d_%d", job_id.cluster, job_id.proc );
		}
		result_ad->Assign( buf, (int)result );
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
		case JA_SUSPEND_JOBS:
		case JA_CONTINUE_JOBS:
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

	result_ad->Assign( ATTR_ACTION_RESULT_TYPE, (int)result_type );

	if( result_type == AR_LONG ) {
			// we've got everything we need in our ad already, nothing
			// to do.
		return result_ad;
	}

		// They want totals for each possible result
	sprintf( buf, "result_total_%d", AR_ERROR );
	result_ad->Assign( buf, ar_error );

	sprintf( buf, "result_total_%d", AR_SUCCESS );
	result_ad->Assign( buf, ar_success );
		
	sprintf( buf, "result_total_%d", AR_NOT_FOUND );
	result_ad->Assign( buf, ar_not_found );

	sprintf( buf, "result_total_%d", AR_BAD_STATUS );
	result_ad->Assign( buf, ar_bad_status );

	sprintf( buf, "result_total_%d", AR_ALREADY_DONE );
	result_ad->Assign( buf, ar_already_done );

	sprintf( buf, "result_total_%d", AR_PERMISSION_DENIED );
	result_ad->Assign( buf, ar_permission_denied );

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
	buf[0] = 0; // in case result is bogus..

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
				 (action==JA_SUSPEND_JOBS)?"suspended":
				 (action==JA_CONTINUE_JOBS)?"continued":
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
				 (action==JA_SUSPEND_JOBS)?"suspend":
				 (action==JA_CONTINUE_JOBS)?"continue":
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
		}else if( action == JA_SUSPEND_JOBS ) {
			sprintf( buf, "Job %d.%d not running to be suspended", 
					 job_id.cluster, job_id.proc );
		}else if( action == JA_CONTINUE_JOBS ) {
			sprintf( buf, "Job %d.%d not running to be continued", 
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
		}else if( action == JA_SUSPEND_JOBS ) { 
			sprintf( buf, "Job %d.%d already suspended",
					 job_id.cluster, job_id.proc );
		}else if( action == JA_CONTINUE_JOBS ) { 
			sprintf( buf, "Job %d.%d already running",
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

bool DCSchedd::getJobConnectInfo(
	PROC_ID jobid,
	int subproc,
	char const *session_info,
	int timeout,
	CondorError *errstack,
	std::string &starter_addr,
	std::string &starter_claim_id,
	std::string &starter_version,
	std::string &slot_name,
	std::string &error_msg,
	bool &retry_is_sensible,
	int &job_status,
	std::string &hold_reason)
{
	ClassAd input;
	ClassAd output;

	input.Assign(ATTR_CLUSTER_ID,jobid.cluster);
	input.Assign(ATTR_PROC_ID,jobid.proc);
	if( subproc != -1 ) {
		input.Assign(ATTR_SUB_PROC_ID,subproc);
	}
	input.Assign(ATTR_SESSION_INFO,session_info);

	if (IsDebugLevel(D_COMMAND)) {
		dprintf (D_COMMAND, "DCSchedd::getJobConnectInfo(%s,...) making connection to %s\n",
			getCommandStringSafe(GET_JOB_CONNECT_INFO), _addr ? _addr : "NULL");
	}

	ReliSock sock;
	if( !connectSock(&sock,timeout,errstack) ) {
		error_msg = "Failed to connect to schedd";
		dprintf( D_ALWAYS, "%s\n",error_msg.c_str());
		return false;
	}

	if( !startCommand(GET_JOB_CONNECT_INFO, &sock, timeout, errstack) ) {
		error_msg = "Failed to send GET_JOB_CONNECT_INFO to schedd";
		dprintf( D_ALWAYS, "%s\n",error_msg.c_str());
		return false;
	}

	if( !forceAuthentication(&sock, errstack) ) {
		error_msg = "Failed to authenticate";
		dprintf( D_ALWAYS, "%s\n",error_msg.c_str());
		return false;
	}

	sock.encode();
	if( !putClassAd(&sock, input) || !sock.end_of_message() ) {
		error_msg = "Failed to send GET_JOB_CONNECT_INFO to schedd";
		dprintf( D_ALWAYS, "%s\n",error_msg.c_str());
		return false;
	}

	sock.decode();
	if( !getClassAd(&sock, output) || !sock.end_of_message() ) {
		error_msg = "Failed to get response from schedd";
		dprintf( D_ALWAYS, "%s\n",error_msg.c_str());
		return false;
	}

	if( IsFulldebug(D_FULLDEBUG) ) {
		std::string adstr;
		sPrintAd(adstr, output);
		dprintf(D_FULLDEBUG,"Response for GET_JOB_CONNECT_INFO:\n%s\n",
				adstr.c_str());
	}

	bool result=false;
	output.LookupBool(ATTR_RESULT,result);

	if( !result ) {
		output.LookupString(ATTR_HOLD_REASON,hold_reason);
		output.LookupString(ATTR_ERROR_STRING,error_msg);
		retry_is_sensible = false;
		output.LookupBool(ATTR_RETRY,retry_is_sensible);
		output.LookupInteger(ATTR_JOB_STATUS,job_status);
	}
	else {
		output.LookupString(ATTR_STARTER_IP_ADDR,starter_addr);
		output.LookupString(ATTR_CLAIM_ID,starter_claim_id);
		output.LookupString(ATTR_VERSION,starter_version);
		output.LookupString(ATTR_REMOTE_HOST,slot_name);
	}

	return result;
}

bool DCSchedd::recycleShadow( int previous_job_exit_reason, ClassAd **new_job_ad, std::string & error_msg )
{
	int timeout = 300;
	CondorError errstack;

	if (IsDebugLevel(D_COMMAND)) {
		dprintf (D_COMMAND, "DCSchedd::recycleShadow(%s,...) making connection to %s\n",
			getCommandStringSafe(RECYCLE_SHADOW), _addr ? _addr : "NULL");
	}

	ReliSock sock;
	if( !connectSock(&sock,timeout,&errstack) ) {
		formatstr(error_msg, "Failed to connect to schedd: %s",
						  errstack.getFullText().c_str());
		return false;
	}

	if( !startCommand(RECYCLE_SHADOW, &sock, timeout, &errstack) ) {
		formatstr(error_msg, "Failed to send RECYCLE_SHADOW to schedd: %s",
						  errstack.getFullText().c_str());
		return false;
	}

	if( !forceAuthentication(&sock, &errstack) ) {
		formatstr(error_msg, "Failed to authenticate: %s",
						  errstack.getFullText().c_str());
		return false;
	}

	sock.encode();
	int mypid = getpid();
	if( !sock.put( mypid ) ||
		!sock.put( previous_job_exit_reason ) ||
		!sock.end_of_message() )
	{
		error_msg = "Failed to send job exit reason";
		return false;
	}

	sock.decode();

	int found_new_job = 0;
	sock.get( found_new_job );

	if( found_new_job ) {
		*new_job_ad = new ClassAd();
		if( !getClassAd( &sock, *(*new_job_ad) ) ) {
			error_msg = "Failed to receive new job ClassAd";
			delete *new_job_ad;
			*new_job_ad = NULL;
			return false;
		}
	}

	if( !sock.end_of_message() ) {
		error_msg = "Failed to receive end of message";
		delete *new_job_ad;
		*new_job_ad = NULL;
		return false;
	}

	if( *new_job_ad ) {
		sock.encode();
		int ok=1;
		if( !sock.put(ok) ||
			!sock.end_of_message() )
		{
			error_msg = "Failed to send ok";
			delete *new_job_ad;
			*new_job_ad = NULL;
			return false;
		}
	}

	return true;
}

bool
DCSchedd::reassignSlot( PROC_ID bid, ClassAd & reply, std::string & errorMessage, PROC_ID * vids, unsigned vCount, int flags ) {
	std::string vidList;
	formatstr( vidList, "%d.%d", vids[0].cluster, vids[0].proc );
	for( unsigned i = 1; i < vCount; ++i ) {
		formatstr_cat( vidList, ", %d.%d", vids[i].cluster, vids[i].proc );
	}

	if( IsDebugLevel( D_COMMAND ) ) {
		dprintf( D_COMMAND, "DCSchedd::reassignSlot( %d.%d <- %s ) making connection to %s\n",
			bid.cluster, bid.proc, vidList.c_str(),
			_addr ? _addr : "NULL");
	}

	ReliSock sock;
	CondorError errorStack;

	if(! connectSock( & sock, 20, & errorStack )) {
		errorMessage = "failed to connect to schedd";
		dprintf( D_ALWAYS, "DCSchedd::reassignSlot(): %s.\n", errorMessage.c_str() );
		return false;
	}
	if(! startCommand( REASSIGN_SLOT, & sock, 20, & errorStack )) {
		errorMessage = "failed to start command";
		dprintf( D_ALWAYS, "DCSchedd::reassignSlot(): %s.\n", errorMessage.c_str() );
		return false;
	}
	if(! forceAuthentication( & sock, & errorStack )) {
		errorMessage = "failed to authenticate";
		dprintf( D_ALWAYS, "DCSchedd::reassignSlot(): %s.\n", errorMessage.c_str() );
		return false;
	}

	// It would seem obvious to construct a ClassAd list of ClassAds
	// with attributes "Cluster" and "Proc", but it turns out to be
	// way easier to send a StringList of the x.y notation, instead.
	//
	// It's also marginally more efficient to send a string than two
	// 64-bit ints, so encode the beneficiary job ID that way.
	char bidStr[PROC_ID_STR_BUFLEN];
	ProcIdToStr( bid, bidStr );

	ClassAd request;
	request.Assign( "VictimJobIDs", vidList );
	request.Assign( "BeneficiaryJobID", bidStr );
	if( flags != 0 ) {
		request.Assign( "Flags", flags );
	}

	sock.encode();
	if(! putClassAd( & sock, request )) {
		errorMessage = "failed to send command payload";
		dprintf( D_ALWAYS, "DCSchedd::reassignSlot(): %s.\n", errorMessage.c_str() );
		return false;
	}
	if(! sock.end_of_message()) {
		errorMessage = "failed to send command payload terminator";
		dprintf( D_ALWAYS, "DCSchedd::reassignSlot(): %s.\n", errorMessage.c_str() );
		return false;
	}

	sock.decode();
	if(! getClassAd( & sock, reply )) {
		errorMessage = "failed to receive payload";
		dprintf( D_ALWAYS, "DCSchedd::reassignSlot(): %s.\n", errorMessage.c_str() );
		return false;
	}
	if(! sock.end_of_message()) {
		errorMessage = "failed to receive command payload terminator";
		dprintf( D_ALWAYS, "DCSchedd::reassignSlot(): %s.\n", errorMessage.c_str() );
		return false;
	}

	bool result;
	reply.LookupBool( ATTR_RESULT, result );
	if(! result) {
		reply.LookupString( ATTR_ERROR_STRING, errorMessage );
		if( errorMessage.empty() ) {
			errorMessage = "unspecified schedd error";
		}
		dprintf( D_ALWAYS, "DCSchedd::reassignSlot(): %s.\n", errorMessage.c_str() );
		return false;
	}

	return true;
}


class ImpersonationTokenContinuation : Service {

public:
	ImpersonationTokenContinuation(const std::string &identity,
		const std::vector<std::string> &authz_bounding_set,
		int lifetime,
		ImpersonationTokenCallbackType *callback,
		void *misc_data)
	:
	  m_identity(identity),
	  m_authz_bounding_set(authz_bounding_set),
	  m_lifetime(lifetime),
	  m_callback(callback),
	  m_misc_data(misc_data)
	{}

	static void startCommandCallback(bool success, Sock *sock, CondorError *errstack,
		const std::string & /*trust_domain*/, bool /*should_try_token_request*/,
		void *misc_data);

	int finish(Stream*);

private:
	std::string m_identity;
	std::vector<std::string> m_authz_bounding_set;
	int m_lifetime{-1};
	ImpersonationTokenCallbackType *m_callback{nullptr};
	void *m_misc_data{nullptr};
};


int ImpersonationTokenContinuation::finish(Stream *stream)
{
	auto &sock = *static_cast<Sock *>(stream);
	CondorError err;
	std::unique_ptr<ImpersonationTokenContinuation> myself(this);

	stream->decode();
	classad::ClassAd ad;
	if (!getClassAd(&sock, ad) || !sock.end_of_message()) {
		err.push("DCSCHEDD", 5, "Failed to receive response from schedd.");
		m_callback(false, "", err, m_misc_data);
		return false;
	}

	int error_code;
	std::string error_string = "(unknown)";
	if (ad.EvaluateAttrInt(ATTR_ERROR_CODE, error_code)) {
		ad.EvaluateAttrString(ATTR_ERROR_STRING, error_string);
		err.push("SCHEDD", error_code, error_string.c_str());
		m_callback(false, "", err, m_misc_data);
		return false;
	}

	std::string token;
	if (!ad.EvaluateAttrString(ATTR_SEC_TOKEN, token)) {
		err.push("DCSCHEDD", 6, "Remote schedd failed to return a token.");
		m_callback(false, "", err, m_misc_data);
		return false;
	}

	m_callback(true, token, err, m_misc_data);
	return true;
}


void
ImpersonationTokenContinuation::startCommandCallback(bool success, Sock *sock, CondorError *errstack,
	const std::string & /*trust_domain*/, bool /*should_try_token_request*/, void *misc_data)
{
		// Automatically free our callback data at function exit.
	std::unique_ptr<class ImpersonationTokenContinuation> callback_ptr(
		static_cast<class ImpersonationTokenContinuation*>(misc_data));
	ImpersonationTokenContinuation &callback_data = *callback_ptr;

	if (!success) {
		callback_data.m_callback(false, "", *errstack, callback_data.m_misc_data);
		return;
	}
		// Ok, we have successfully established a connection.  Let's build the request ad
		// and shoot it off.
	classad::ClassAd request_ad;
	if (!request_ad.InsertAttr(ATTR_SEC_USER, callback_data.m_identity) ||
		!request_ad.InsertAttr(ATTR_SEC_TOKEN_LIFETIME, callback_data.m_lifetime))
	{
		errstack->push("DCSCHEDD", 2, "Failed to create schedd request ad.");
		callback_data.m_callback(false, "", *errstack, callback_data.m_misc_data);
		return;
	}
	if (!callback_data.m_authz_bounding_set.empty()) {
		std::stringstream ss;
		bool first = true;
		for (const auto &authz : callback_data.m_authz_bounding_set) {
			if (first) {first = false;}
			else {ss << ",";}
			ss << authz;
		}
		if (!request_ad.InsertAttr(ATTR_SEC_LIMIT_AUTHORIZATION, ss.str()))
		{
			errstack->push("DCSCHEDD", 2, "Failed to create schedd request ad.");
			callback_data.m_callback(false, "", *errstack, callback_data.m_misc_data);
			return;
		}
	}

	sock->encode();
	if (!putClassAd(sock, request_ad) ||
		!sock->end_of_message())
	{
		errstack->push("DCSCHEDD", 3, "Failed to send impersonation token request ad"
			" to remote schedd.");
		callback_data.m_callback(false, "", *errstack, callback_data.m_misc_data);
		return;
	}

		// Now, we must register a callback to wait for a response.
	auto rc = daemonCore->Register_Socket(sock, "Impersonation Token Request",
		(SocketHandlercpp)&ImpersonationTokenContinuation::finish,
		"Finish impersonation token request",
		callback_ptr.get(), ALLOW, HANDLE_READ);
	if (rc < 0) {
		errstack->push("DCSCHEDD", 4, "Failed to register callback for schedd response");
		callback_data.m_callback(false, "", *errstack, callback_data.m_misc_data);
		return;
	}

		// At this point, the callback has been registered and DaemonCore owns the
		// memory; release the unique_ptr to prevent it from deleting the callback
		// object at exit.
	callback_ptr.release();
}


bool
DCSchedd::requestImpersonationTokenAsync(const std::string &identity,
	const std::vector<std::string> &authz_bounding_set, int lifetime,
	ImpersonationTokenCallbackType callback, void *misc_data, CondorError &err)
{
	if (IsDebugLevel(D_COMMAND)) {
		dprintf(D_COMMAND, "DCSchedd::requestImpersonationTokenAsync() making connection "
			" to '%s'\n", _addr ? _addr : "NULL" );
	}

	if (identity.empty()) {
		err.push("DC_SCHEDD", 1, "Impersonation token identity not provided.");
		dprintf(D_FULLDEBUG, "Impersonation token identity not provided.\n");
		return false;
	}
	std::string full_identity = identity;
	auto at_sign = identity.find('@');
	if (at_sign == std::string::npos) {
		std::string domain;
		if (!param(domain, "UID_DOMAIN")) {
			err.push("DAEMON", 1, "No UID_DOMAIN set!");
			dprintf(D_FULLDEBUG, "No UID_DOMAIN set!\n");
			return false;
		}
		full_identity = identity + "@" + domain;
	}

		// Connect to the schedd (if necessary) and start a non-blocking command.
		// The continuation object holds the state needed to make the request ad later.
	auto continuation = new ImpersonationTokenContinuation(identity, authz_bounding_set,
		lifetime, callback, misc_data);
	auto result = startCommand_nonblocking(COLLECTOR_TOKEN_REQUEST, Stream::reli_sock, 20, &err,
		ImpersonationTokenContinuation::startCommandCallback, continuation,
		"requestImpersonationToken");

	if (result == StartCommandFailed) {
		return false; // Assume startCommand already left a reasonable error message.
	}
	return true;
}
