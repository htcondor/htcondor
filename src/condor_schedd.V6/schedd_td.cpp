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
#include "condor_daemon_core.h"
#include "condor_daemon_client.h"
#include "dc_transferd.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "qmgmt.h"
#include "condor_qmgr.h"
#include "scheduler.h"
#include "basename.h"
#include "nullfile.h"
#include "spooled_job_files.h"
#include "condor_url.h"


extern const std::string & attr_JobUser; // the attribute name we use for the "owner" of the job, historically ATTR_OWNER 
extern bool user_is_the_new_owner; // set in schedd.cpp at startup
inline const char * EffectiveUser(Sock * sock) {
	if (!sock) return "";
	if (user_is_the_new_owner) {
		return sock->getFullyQualifiedUser();
	} else {
		return sock->getOwner();
	}
	return "";
}

/* In this service function, the client tells the schedd a bunch of jobs
	it would like to perform a transfer for into/out of a sandbox. The
	schedd will hold open the connection back to the client
	(potentially across to another callback) until it gets some information
	from a transferd about the request and can give it back to the client
	via callbacks.
*/
int
Scheduler::requestSandboxLocation(int mode, Stream* s)
{
	ReliSock* rsock = (ReliSock*)s;
	TransferDaemon *td = NULL;
	std::string rand_id;
	std::string fquser;
	ClassAd reqad, respad;
	std::string jids, jids_allow, jids_deny;
	std::vector<PROC_ID> *jobs = NULL;
	std::vector<PROC_ID> *modify_allow_jobs = NULL;
	std::vector<PROC_ID> *modify_deny_jobs = NULL;
	ClassAd *tmp_ad = NULL;
	std::string constraint_string;
	int protocol;
	std::string peer_version;
	bool has_constraint;
	int direction;
	std::string desc;

	(void)mode; // quiet the compiler

	dprintf(D_ALWAYS, "Entering requestSandboxLocation()\n");

		// make sure this connection is authenticated, and we know who
		// the user is.  also, set a timeout, since we don't want to
		// block long trying to read from our client.   
	rsock->timeout( 20 );  

	////////////////////////////////////////////////////////////////////////
	// Authenticate the socket
	////////////////////////////////////////////////////////////////////////

	if( ! rsock->triedAuthentication() ) {
		CondorError errstack;
		if( ! SecMan::authenticate_sock(rsock, WRITE, &errstack) ) {
				// we failed to authenticate, we should bail out now
				// since we don't know what user is trying to perform
				// this action.
				// TODO: it'd be nice to print out what failed, but we
				// need better error propagation for that...
			errstack.push( "SCHEDD", SCHEDD_ERR_SPOOL_FILES_FAILED,
					"Failure to spool job files - Authentication failed" );
			dprintf( D_ALWAYS, "requestSandBoxLocation() aborting: %s\n",
					 errstack.getFullText().c_str() );

			respad.Assign(ATTR_TREQ_INVALID_REQUEST, TRUE);
			respad.Assign(ATTR_TREQ_INVALID_REASON, "Authentication failed.");
			putClassAd(rsock, respad);
			rsock->end_of_message();

			return FALSE;
		}
	}	

	// to whom does the client authenticate?
	fquser = rsock->getFullyQualifiedUser();

	rsock->decode();

	////////////////////////////////////////////////////////////////////////
	// read the request ad from the client about what it wants to transfer
	////////////////////////////////////////////////////////////////////////

	// This request ad from the client will contain
	//	ATTR_TREQ_DIRECTION
	//	ATTR_TREQ_PEER_VERSION
	//	ATTR_TREQ_HAS_CONSTRAINT
	//	ATTR_TREQ_JOBID_LIST
	//	ATTR_TREQ_XFP
	//
	//	OR
	//
	//	ATTR_TREQ_DIRECTION
	//	ATTR_TREQ_PEER_VERSION
	//	ATTR_TREQ_HAS_CONSTRAINT
	//	ATTR_TREQ_CONSTRAINT
	//	ATTR_TREQ_XFP

	if (!getClassAd(rsock, reqad)) {
			rsock->end_of_message();
			return CLOSE_STREAM;
	}
	rsock->end_of_message();

	if (reqad.LookupBool(ATTR_TREQ_HAS_CONSTRAINT, has_constraint) == 0) {
		dprintf(D_ALWAYS, "requestSandBoxLocation(): Client reqad from %s "
			"must have %s as an attribute.\n", fquser.c_str(), 
			ATTR_TREQ_HAS_CONSTRAINT);

		respad.Assign(ATTR_TREQ_INVALID_REQUEST, TRUE);
		respad.Assign(ATTR_TREQ_INVALID_REASON, "Missing constraint bool.");
		putClassAd(rsock, respad);
		rsock->end_of_message();

		return FALSE;
	}

	////////////////////////////////////////////////////////////////////////
	// Let's validate the jobid set the user wishes to modify with a
	// file transfer. The reason we sometimes use a constraint and sometimes
	// not is an optimization for speed. If the client already has the
	// ads, then we don't iterate over the job queue log, which is 
	// extremely expensive.
	////////////////////////////////////////////////////////////////////////
	
	
	/////////////
	// The user specified the jobids directly it would like to work with.
	// We assume the client already has the ads it wishes to transfer.
	/////////////
	if (!has_constraint) {
		
		dprintf(D_ALWAYS, "Submittor provides procids.\n");

		modify_allow_jobs = new std::vector<PROC_ID>;
		ASSERT(modify_allow_jobs);

		modify_deny_jobs = new std::vector<PROC_ID>;
		ASSERT(modify_deny_jobs);

		if (reqad.LookupString(ATTR_TREQ_JOBID_LIST, jids) == 0) {
			dprintf(D_ALWAYS, "requestSandBoxLocation(): Submitter "
				"%s's reqad must have %s as an attribute.\n", 
				fquser.c_str(), ATTR_TREQ_JOBID_LIST);

			respad.Assign(ATTR_TREQ_INVALID_REQUEST, TRUE);
			respad.Assign(ATTR_TREQ_INVALID_REASON, "Missing jobid list.");
			putClassAd(rsock, respad);
			rsock->end_of_message();

			delete modify_allow_jobs;
			delete modify_deny_jobs;
			return FALSE;
		}

		//////////////////////
		// convert the stringlist of jobids into an actual vector of
		// PROC_IDs. we are responsible for this newly allocated memory.
		//////////////////////
		jobs = string_to_procids(jids);

		if (jobs == NULL) {
			// can't have no constraint and no jobids, bail.
			dprintf(D_ALWAYS, "Submitter %s sent inconsistant ad with no "
				"constraint and also no jobids on which to perform sandbox "
				"manipulations.\n", fquser.c_str());

			respad.Assign(ATTR_TREQ_INVALID_REQUEST, TRUE);
			respad.Assign(ATTR_TREQ_INVALID_REASON, 
				"No constraint and no jobid list.");
			putClassAd(rsock, respad);
			rsock->end_of_message();

			delete modify_allow_jobs;
			delete modify_deny_jobs;
			return FALSE;
		}

		//////////////////////
		// Filter the jobs into a two arrays, those that we can
		// modify and those we cannot because the client is not 
		// authorized to.
		//////////////////////
		for (size_t i = 0; i < jobs->size(); i++) {
			std::string job_owner = "";
			GetAttributeString((*jobs)[i].cluster, (*jobs)[i].proc, attr_JobUser.c_str(), job_owner);
			if (UserCheck2(NULL, EffectiveUser(rsock), job_owner.c_str())) {
				// only allow the user to manipulate jobs it is entitled to.
				// structure copy...
				modify_allow_jobs->push_back((*jobs)[i]);
			} else {
				// client can't modify this ad due to not having authority
				dprintf(D_ALWAYS, 
					"Scheduler::requestSandBoxLocation(): "
					"WARNING: Submitter %s tried to request a sandbox "
					"location for jobid %d.%d which is not owned by the "
					"submitter. Denied modification to specified job.\n",
					fquser.c_str(), (*jobs)[i].cluster, (*jobs)[i].proc);

				// structure copy...
				modify_deny_jobs->push_back((*jobs)[i]);
			}
		}

		// pack back into the reqad both allow and deny arrays so the client
		// knows for which jobids it may transfer the files.
		procids_to_string(modify_allow_jobs, jids_allow);
		procids_to_string(modify_deny_jobs, jids_deny);

		respad.Assign(ATTR_TREQ_JOBID_ALLOW_LIST, jids_allow);
		respad.Assign(ATTR_TREQ_JOBID_DENY_LIST, jids_deny);

		// don't need what this contains anymore
		delete modify_deny_jobs;
		modify_deny_jobs = NULL;

		// don't need what this contains anymore
		delete jobs;
		jobs = NULL;

	} else {

		/////////////
		// The user specified by a constraint the jobids it would like to use.
		// Notice that we check the permissions of said constraint, and then
		// make a jobids array for what the constraint returned. This means
		// that for this specific request, it is resolved to a hard set of
		// jobids right here. So, if after this request is stored, some more
		// jobs come into the queue that this constraint would have matched,
		// they are ignored and not added to the fileset represented by the
		// capability the transferd will return. However, I do reupdate the
		// jobads in the pre_push_callback handler to ensure I gather any
		// changes that a condor_qedit might have performed in between now
		// and the pre_push_callback being called. Here we don't assume the
		// client has the jobads and the transferd transfer thread must 
		// send them over one by one to the client before initializing a file
		// transfer object, or any other type of protocol, to ensure the client
		// and transferd are synchronized in what they are transferring.
		/////////////

		dprintf(D_ALWAYS, "Submittor provides constraint.\n");

		if (reqad.LookupString(ATTR_TREQ_CONSTRAINT,constraint_string)==0)
		{
			dprintf(D_ALWAYS, "Submitter %s sent inconsistant ad with "
				"no constraint to find any jobids\n",
				fquser.c_str());
		}

		// By definition we'll only save the jobids the user may modify
		modify_allow_jobs = new std::vector<PROC_ID>;
		ASSERT(modify_allow_jobs);

		// Walk the job queue looking for jobs which match the constraint
		// filter. Then filter that set with OwnerCheck to ensure 
		// the client has the correct authority to modify these jobids.
		tmp_ad = GetNextJobByConstraint(constraint_string.c_str(), 1);
		while (tmp_ad) {
			PROC_ID job_id;
			if ( UserCheck2(tmp_ad, EffectiveUser(rsock)) )
			{
				modify_allow_jobs->push_back(job_id);
			}
			tmp_ad = GetNextJobByConstraint(constraint_string.c_str(), 0);
		}

		// Let the client know what jobids it may actually transfer for.
		procids_to_string(modify_allow_jobs, jids_allow);
		respad.Assign(ATTR_TREQ_JOBID_ALLOW_LIST, jids_allow);
		respad.Assign(ATTR_TREQ_JOBID_DENY_LIST, "");

	}

	// At this point, modify_allow_jobs contains an array of jobids the
	// schedd has said the client is able to transfer files for.
	// XXX Check for empty set.

	////////////////////////////////////////////////////////////////////////
	// The protocol the user specified must get validated later during the
	// push the the transferd. The transferd has the ultimate say on which
	// protocols it is willing to use and it'll be in the final ad back to
	// the client. For now, we just ensure a file tranfer protocol is present.
	////////////////////////////////////////////////////////////////////////

	if (reqad.LookupInteger(ATTR_TREQ_FTP, protocol) == 0) {
		dprintf(D_ALWAYS, "requestSandBoxLocation(): Submitter "
			"%s's reqad must have %s as an attribute.\n", 
			fquser.c_str(), ATTR_TREQ_FTP);

		respad.Assign(ATTR_TREQ_INVALID_REQUEST, TRUE);
		respad.Assign(ATTR_TREQ_INVALID_REASON, 
			"No file transfer protocol specified.");
		putClassAd(rsock, respad);
		rsock->end_of_message();

		delete modify_allow_jobs;
		return FALSE;
	}

	////////////////////////////////////////////////////////////////////////
	// Ensure there is a peer version in the reqad. This is an opaque
	// version string that gets passed silently to lower layers which might
	// need it. 
	////////////////////////////////////////////////////////////////////////

	if (reqad.LookupString(ATTR_TREQ_PEER_VERSION, peer_version) == 0) {
		dprintf(D_ALWAYS, "requestSandBoxLocation(): Submitter "
			"%s's reqad must have %s as an attribute.\n", 
			fquser.c_str(), ATTR_TREQ_PEER_VERSION);

		respad.Assign(ATTR_TREQ_INVALID_REQUEST, TRUE);
		respad.Assign(ATTR_TREQ_INVALID_REASON, 
			"No peer version specified.");
		putClassAd(rsock, respad);
		rsock->end_of_message();

		delete modify_allow_jobs;
		return FALSE;
	}

	////////////////////////////////////////////////////////////////////////
	// Ensure we have a direction that transfer request is supposed to be for.
	////////////////////////////////////////////////////////////////////////

	if (reqad.LookupInteger(ATTR_TREQ_DIRECTION, direction) == 0) {
		dprintf(D_ALWAYS, "requestSandBoxLocation(): Submitter "
			"%s's reqad must have %s as an attribute.\n", 
			fquser.c_str(), ATTR_TREQ_DIRECTION);

		respad.Assign(ATTR_TREQ_INVALID_REQUEST, TRUE);
		respad.Assign(ATTR_TREQ_INVALID_REASON, 
			"No file transfer direction specified.");
		putClassAd(rsock, respad);
		rsock->end_of_message();

		delete modify_allow_jobs;
		return FALSE;
	}

	////////////////////////////////////////////////////////////////////////
	// construct the transfer request for this submittor's file set
	////////////////////////////////////////////////////////////////////////

	TransferRequest *treq = new TransferRequest();
	ASSERT(treq != NULL);

	// Set up the header which will get serialized to the transferd.
	treq->set_direction(direction);
	treq->set_used_constraint(has_constraint);
	treq->set_peer_version(peer_version);
	treq->set_xfer_protocol(protocol);
	treq->set_transfer_service("Passive"); // XXX fixme to use enum
	treq->set_num_transfers(modify_allow_jobs->size());
	treq->set_protocol_version(0); // for the treq structure, not xfer protocol

	// Give the procids array to the treq, later, just before it is pushed to
	// the td, the schedd is given a chance to play with the jobads represented
	// by this array, and then update the treq with the real jobads it wants
	// the td to use.
	// This treq object gets ownership over the passed in memory.
	treq->set_procids(modify_allow_jobs);

	// Stash the client socket into the treq for later use in a callback...
	treq->set_client_sock(rsock);

	////////////////////////////////////////////////////////////////////////
	// Set the callback handlers to work on this treq as it progresses through
	// the process of going & comming back from the transferd.
	////////////////////////////////////////////////////////////////////////

	// Callbacks to allow the schedd to process these requests during various
	// stages of being processed by the transferd. The callback is called
	// with the tranfer request in question and the transfer daemon object
	// responsible for it (except in the case of the reaper). However,
	// there are different callbacks for treqs which are for different
	// purposes, like uploading or downloading.

	switch(direction) 
	{
		case FTPD_UPLOAD:

			// called just before the request is sent to the td itself.
			// used to modify the jobads for starting of transfer time

			desc = "Treq Upload Pre Push Callback Handler";
			treq->set_pre_push_callback(desc,
				(TreqPrePushCallback)
					&Scheduler::treq_upload_pre_push_callback, this);

			// called after the push and response from the schedd, gives schedd
			// access to the treq capability string, the client was already
			// notified.

			desc = "Treq Upload Post Push Callback Handler";
			treq->set_post_push_callback(desc,
				(TreqPostPushCallback)
					&Scheduler::treq_upload_post_push_callback, this);

			// called with an update status from the td about this request.
			// (completed, not completed, etc, etc, etc)
			// job ads are processed, job taken off hold, etc if a successful 
			// completion had happend

			desc = "Treq Upload update Callback Handler";
			treq->set_update_callback(desc,
				(TreqUpdateCallback)
					&Scheduler::treq_upload_update_callback, this);

			// called when the td dies, if the td handles and updates and 
			// everything correctly, this is not normally called.

			desc = "Treq Upload Reaper Callback Handler";
			treq->set_reaper_callback(desc,
				(TreqReaperCallback)
					&Scheduler::treq_upload_reaper_callback, this);
	
			break;
		
		case FTPD_DOWNLOAD:

			// called just before the request is sent to the td itself.
			// used to modify the jobads for starting of transfer time

			desc = "Treq Download Pre Push Callback Handler";
			treq->set_pre_push_callback(desc,
				(TreqPrePushCallback)
					&Scheduler::treq_download_pre_push_callback, this);

			// called after the push and response from the schedd, gives schedd
			// access to the treq capability string, the client was already
			// notified.

			desc = "Treq Download Post Push Callback Handler";
			treq->set_post_push_callback(desc,
				(TreqPostPushCallback)
					&Scheduler::treq_download_post_push_callback, this);

			// called with an update status from the td about this request.
			// (completed, not completed, etc, etc, etc)
			// job ads are processed, job taken off hold, etc if a successful 
			// completion had happend

			desc = "Treq Download update Callback Handler";
			treq->set_update_callback(desc,
				(TreqUpdateCallback)
					&Scheduler::treq_download_update_callback, this);

			// called when the td dies, if the td handles and updates and 
			// everything correctly, this is not normally called.

			desc = "Treq Download Reaper Callback Handler";
			treq->set_reaper_callback(desc,
				(TreqReaperCallback)
					&Scheduler::treq_download_reaper_callback, this);
			break;
		
		default:
			// XXX Figure out this case:
			break;
	}

	rsock->encode();

	////////////////////////////////////////////////////////////////////////
	// locate a transferd
	////////////////////////////////////////////////////////////////////////

	// Ok, figure out if I have a transferd already setup for this user.
	td = m_tdman.find_td_by_user(fquser);
	if (td == NULL || 
		((td->get_status() != TD_REGISTERED) && 
			(td->get_status() != TD_INVOKED)) )
	{
		// Since it looks like I'm going to have to wait for a transferd
		// to wake up and register to this schedd, let the client know we
		// might block for a while.
		respad.Assign(ATTR_TREQ_WILL_BLOCK, 1);
		if (putClassAd(rsock, respad) == 0) {
			dprintf(D_ALWAYS, "Submittor %s closed connection. Aborting "
				"getting sandbox info for user.\n", fquser.c_str());
			delete treq;
			return FALSE;
		}
		rsock->end_of_message();

		if (td == NULL) {
			// Create a TransferDaemon object, and hand it to the td
			// manager for it to start. Stash the client socket into the object
			// so when it comes online, we can continue our discussion with the
			// client.

			// XXX Should I test this against the keys in the manager table
			// to ensure there are unique ids for the transferds I have
			// requested to invoke--a collision would be nasty here.
			randomlyGenerateInsecureHex(rand_id, 64);

			td = new TransferDaemon(fquser, rand_id, TD_PRE_INVOKED);

			ASSERT(td != NULL);
			// set up the registration callback
			desc = "Transferd Registration callback";
			td->set_reg_callback(desc, 
				(TDRegisterCallback)
					&Scheduler::td_register_callback, this);

			// set up the reaper callback
			desc = "Transferd Reaper callback";
			td->set_reaper_callback(desc, 
				(TDReaperCallback)
					&Scheduler::td_reaper_callback, this);

		} else {
			// clear the fact there was a living daemon associated with this
			// object, and let the invoke_a_td() call restart the daemon.
			td->clear();
		}

		// pair the transfer request with the client that requested it and
		// store the request which will get pushed later when the td 
		// registers
		td->add_transfer_request(treq);

		// let the manager object start it up for us....
		m_tdman.invoke_a_td(td);

		// The socket is going to be deleted later in a callback the
		// schedd deals with and I don't want daemoncore to also delete
		// the socket.
		return KEEP_STREAM;
	}

	////////////////////////////////////////////////////////////////////////
	// Since I already found one, this should go fast.
	////////////////////////////////////////////////////////////////////////

	respad.Assign(ATTR_TREQ_WILL_BLOCK, 0);
	if (putClassAd(rsock, respad) == 0) {
		dprintf(D_ALWAYS, "Submittor %s closed connection. Aborting "
			"getting sandbox info for user.\n", fquser.c_str());
		delete treq;
		return FALSE;
	}
	rsock->end_of_message();

	// queue the transfer request to the waiting td who will own the 
	// transfer request memory which owns the socket.
	td->add_transfer_request(treq);

	// Push the request to the td itself, where the callbacks from the 
	// transfer request will contact the client as needed.
	td->push_transfer_requests();

	// The socket is going to be deleted later in a callback the
	// schedd deals with and I don't want daemoncore to also delete
	// the socket.
	return KEEP_STREAM;
}

///////////////////////////////////////////////////////////////////////////
// A callback notification from the TDMan object the schedd gets when the 
// registration of a transferd is complete and the transferd is considered
// open for business.
///////////////////////////////////////////////////////////////////////////
TdAction
Scheduler::td_register_callback(TransferDaemon *)
{
	dprintf(D_ALWAYS, "Scheduler::td_register_callback() called\n");

	return TD_ACTION_CONTINUE;
}

///////////////////////////////////////////////////////////////////////////
// A callback notification from the TDMan object the schedd gets when the 
// transferd has died.
///////////////////////////////////////////////////////////////////////////
TdAction
Scheduler::td_reaper_callback(long, int, TransferDaemon *)
{
	dprintf(D_ALWAYS, "Scheduler::td_reaper_callback() called\n");

	return TD_ACTION_TERMINATE;
}


///////////////////////////////////////////////////////////////////////////
// These are the callbacks related to transfer requests which are upload
// requests.
///////////////////////////////////////////////////////////////////////////

// In this handler, if the handler wants to return TREQ_ACTION_FORGET, then
// the treq pointer is owned by this handler and must be deleted by this
// handler, for all other return values this handler should not delete the treq.
TreqAction
Scheduler::treq_upload_pre_push_callback(TransferRequest *treq, 
	TransferDaemon *)
{
	int cluster;
	int proc;
	std::vector<PROC_ID> *jobs = NULL;
	size_t i;
	time_t now;

	dprintf(D_ALWAYS, "Scheduler::treq_upload_pre_push_callback() called.\n");

	jobs = treq->get_procids();

	now = time(NULL);

	// set the stage in start time.
	for (i = 0; i < (*jobs).size(); i++) {
		SetAttributeInt((*jobs)[i].cluster, (*jobs)[i].proc, 
			ATTR_STAGE_IN_START, now);
	}

	// Get the actual (now modified) job ads and shove them into the request
	// for the transferd to munch on.

	for (i=0; i < (*jobs).size(); i++) {
		cluster = (*jobs)[i].cluster;
		proc = (*jobs)[i].proc;
		ClassAd * jad = GetJobAd( cluster, proc );
		treq->append_task(jad);
	}

	// keep processing this request.
	return TREQ_ACTION_CONTINUE;
}


// In this handler, if the handler wants to return TREQ_ACTION_FORGET, then
// the treq pointer is owned by this handler and must be deleted by this
// handler, for all other return values this handler should not delete the treq.
TreqAction
Scheduler::treq_upload_post_push_callback(TransferRequest *treq, 
	TransferDaemon *td)
{
	ReliSock *rsock = NULL;
	std::string sinful;
	std::string capability;
	ClassAd respad;
	std::string jids;
	std::string reason;

	////////////////////////////////////////////////////////////////////////
	// Respond to the client with a capability, a td sinful, the list of
	// jobids allowable for this fileset, and a list of protocols the td is
	// willing to use.
	// Use the sock stashed in the treq, then close the socket to the client.
	////////////////////////////////////////////////////////////////////////

	dprintf(D_ALWAYS, "Scheduler::treq_upload_post_push_callback() called.\n");

	rsock = treq->get_client_sock();
	// This means I might have accidentily recycled a treq without 
	// properly cleaning it up.
	ASSERT(rsock != NULL);

	rsock->encode();

	// If the request was rejected, then tell the client why
	if (treq->get_rejected() == true) {
		respad.Assign(ATTR_TREQ_INVALID_REQUEST, TRUE);
		reason = treq->get_rejected_reason();
		respad.Assign(ATTR_TREQ_INVALID_REASON, reason);
	} else {

		sinful = td->get_sinful();
		capability = treq->get_capability();

		procids_to_string(treq->get_procids(), jids);

		// This is what the transferd is willing to do for this request.
		respad.Assign(ATTR_TREQ_INVALID_REQUEST, FALSE);
		respad.Assign(ATTR_TREQ_CAPABILITY, capability);
		respad.Assign(ATTR_TREQ_TD_SINFUL, sinful);
		respad.Assign(ATTR_TREQ_FTP, treq->get_xfer_protocol());
		respad.Assign(ATTR_TREQ_JOBID_ALLOW_LIST, jids);
	}

	// This response ad from the schedd will contain:
	//	ATTR_TREQ_INVALID_REQUEST (set to true)
	//	ATTR_TREQ_INVALID_REASON
	//
	//	OR
	//
	//	ATTR_TREQ_INVALID_REQUEST (set to false)
	//	ATTR_TREQ_CAPABILITY
	//	ATTR_TREQ_TD_SINFUL
	//	ATTR_TREQ_FTP
	//	ATTR_TREQ_JOBID_ALLOW_LIST
	//
	dprintf(D_ALWAYS, "Scheduler::treq_post_push_callback() "
		"Responding to client about sandbox request and closing connection.\n");
	putClassAd(rsock, respad);
	rsock->end_of_message();

	// close the connection to the client now that I've told it where it can
	// put its files.
	delete rsock;
	
	// I told the client to go away, so I don't have this anymore.
	treq->set_client_sock(NULL);

	// Keep processing this request
	return TREQ_ACTION_CONTINUE;
}

// In this handler, if the handler wants to return TREQ_ACTION_FORGET, then
// the treq pointer is owned by this handler and must be deleted by this
// handler, for all other return values this handler should not delete the treq.
// If the update status is satisfactory, then update the transfer time
// for the job and remove it from being on hold.
TreqAction
Scheduler::treq_upload_update_callback(TransferRequest *treq, 
	TransferDaemon *, ClassAd *)
{
	int cluster,proc,index;
	char new_attr_value[500];
	char *buf = NULL;
	ExprTree *expr = NULL;
	std::string SpoolSpace;
	time_t now = time(NULL);
	SimpleList<ClassAd*> *treq_ads = NULL;
	char *mySpool = NULL;
	const char *AttrsToModify[] = { 
		ATTR_JOB_CMD,
		ATTR_JOB_INPUT,
		ATTR_JOB_OUTPUT,
		ATTR_JOB_ERROR,
		ATTR_TRANSFER_INPUT_FILES,
		ATTR_TRANSFER_OUTPUT_FILES,
		ATTR_ULOG_FILE,
		ATTR_X509_USER_PROXY,
		NULL };		// list must end with a NULL
	ClassAd *old_ad = NULL;
	ClassAd *jad = NULL;

	dprintf(D_ALWAYS, "Scheduler::treq_upload_update_callback() called.\n");

	if( !(mySpool = param("SPOOL")) ) {
		EXCEPT( "No spool directory specified in config file" );
	}

	////////////////////////////////////////////////////////////////////////
	// Determine if I like the update callback.
	////////////////////////////////////////////////////////////////////////

	// XXX TODO
	// Assume this update is because the file were transferred correctly.

	// XXX TODO
	// Ensure that if the STAGE_IN variable is already set, I bail with
	// an error since the job might be running. Later, we might give the 
	// user the possibility of wiping
	// clean the stage in sandbox and reinitializing it, in which case the
	// job should be put back on hold during that operation if it is not
	// atomic. TO IMPLEMENTOR: Look at the old usage of the STAGE_IN
	// variable in the other code path and check to make sure the usage over
	// here is the same.

	////////////////////////////////////////////////////////////////////////
	// The Mojo to get the job off of hold now that the transfer is complete
	////////////////////////////////////////////////////////////////////////

		// For each job, modify its ClassAd
	treq_ads = treq->todo_tasks();
	
	treq_ads->Rewind();
	while(treq_ads->Next(old_ad))
	{
		old_ad->LookupInteger(ATTR_CLUSTER_ID, cluster);
		old_ad->LookupInteger(ATTR_PROC_ID, proc);

		// Get a fresh ad from the queue since it might have been qedited in
		// the time the job was waiting for file transfer to complete.
		jad = GetJobAd(cluster,proc);

		SpooledJobFiles::getJobSpoolPath(jad, SpoolSpace);
		ASSERT(!SpoolSpace.empty());

		BeginTransaction();

			// Backup the original IWD at submit time
		if (buf) free(buf);
		buf = NULL;
		jad->LookupString(ATTR_JOB_IWD,&buf);
		if ( buf ) {
			snprintf(new_attr_value,500,"SUBMIT_%s",ATTR_JOB_IWD);
			SetAttributeString(cluster,proc,new_attr_value,buf);
			free(buf);
			buf = NULL;
		}
			// Modify the IWD to point to the spool space			
		SetAttributeString(cluster,proc,ATTR_JOB_IWD,SpoolSpace.c_str());

			// Backup the original TRANSFER_OUTPUT_REMAPS at submit time
		expr = jad->LookupExpr(ATTR_TRANSFER_OUTPUT_REMAPS);
		snprintf(new_attr_value,500,"SUBMIT_%s",ATTR_TRANSFER_OUTPUT_REMAPS);
		if ( expr ) {
			const char *remap_buf = ExprTreeToString( expr );
			ASSERT(remap_buf);
			SetAttribute(cluster,proc,new_attr_value,remap_buf);
		}
		else if(jad->LookupExpr(new_attr_value)) {
				// SUBMIT_TransferOutputRemaps is defined, but
				// TransferOutputRemaps is not; disable the former,
				// so that when somebody fetches the sandbox, nothing
				// gets remapped.
			SetAttribute(cluster,proc,new_attr_value,"Undefined");
		}
			// Set TRANSFER_OUTPUT_REMAPS to Undefined so that we don't
			// do remaps when the job's output files come back into the
			// spool space. We only want to remap when the submitter
			// retrieves the files.
		SetAttribute(cluster,proc,ATTR_TRANSFER_OUTPUT_REMAPS,"Undefined");

			// Now, for all the attributes listed in 
			// AttrsToModify, change them to be relative to new IWD
			// by taking the basename of all file paths.
		index = -1;
		while ( AttrsToModify[++index] ) {
				// Lookup original value
			if (buf) free(buf);
			buf = NULL;
			jad->LookupString(AttrsToModify[index],&buf);
			if (!buf) {
				// attribute not found, so no need to modify it
				continue;
			}
			if ( nullFile(buf) ) {
				// null file -- no need to modify it
				continue;
			}
				// Create new value - deal with the fact that
				// some of these attributes contain a list of pathnames
			StringList old_paths(buf,",");
			StringList new_paths(NULL,",");
			old_paths.rewind();
			char *old_path_buf;
			bool changed = false;
			const char *base = NULL;
			std::string new_path_buf;
			while ( (old_path_buf=old_paths.next()) ) {
				base = condor_basename(old_path_buf);
				if ((strcmp(AttrsToModify[index], ATTR_TRANSFER_INPUT_FILES)==0) && IsUrl(old_path_buf)) {
					base = old_path_buf;
				} else if ( strcmp(base,old_path_buf)!=0 ) {
					formatstr(new_path_buf,
						"%s%c%s",SpoolSpace.c_str(),DIR_DELIM_CHAR,base);
					base = new_path_buf.c_str();
					changed = true;
				}
				new_paths.append(base);
			}
			if ( changed ) {
					// Backup original value
				snprintf(new_attr_value,500,"SUBMIT_%s",AttrsToModify[index]);
				SetAttributeString(cluster,proc,new_attr_value,buf);
					// Store new value
				char *new_value = new_paths.print_to_string();
				ASSERT(new_value);
				SetAttributeString(cluster,proc,AttrsToModify[index],new_value);
				free(new_value);
			}
		}

			// Set ATTR_STAGE_IN_FINISH if not already set.
		int spool_completion_time = 0;
		jad->LookupInteger(ATTR_STAGE_IN_FINISH,spool_completion_time);
		if ( !spool_completion_time ) {
			// The transfer thread specifically slept for 1 second
			// to ensure that the job can't possibly start (and finish)
			// prior to the timestamps on the file.  Unfortunately,
			// we note the transfer finish time _here_.  So we've got 
			// to back off 1 second.
			SetAttributeInt(cluster,proc,ATTR_STAGE_IN_FINISH,now - 1);
		}

			// And now release the job.
		releaseJob(cluster,proc,"Data files spooled",false,false,false,false);
		CommitTransactionOrDieTrying();
	}

	daemonCore->Register_Timer( 0, 
					(TimerHandlercpp)&Scheduler::reschedule_negotiator_timer,
					"Scheduler::reschedule_negotiator", this );

	if (mySpool) free(mySpool);
	if (buf) free(buf);

	////////////////////////////////////////////////////////////////////////
	// After proper fixing up of the job in the queue, tell the callback engine
	// that I'm done with this request. A termination returns value will
	// have the callback engine free the treq.
	////////////////////////////////////////////////////////////////////////

	return TREQ_ACTION_TERMINATE;
}

// This function does NOT own the memory passed to it ever. If you want a copy
// of the treq, you must make one.
TreqAction
Scheduler::treq_upload_reaper_callback(TransferRequest *)
{
	dprintf(D_ALWAYS, "Scheduler::treq_upload_reaper_callback() called.\n");

	////////////////////////////////////////////////////////////////////////
	// Until we figure out a better solution to what we do here, leave the
	// job on hold, and terminate the request.
	////////////////////////////////////////////////////////////////////////
	
	return TREQ_ACTION_TERMINATE;
}





///////////////////////////////////////////////////////////////////////////
// These are the callbacks related to transfer requests which are download
// requests.
///////////////////////////////////////////////////////////////////////////

TreqAction
Scheduler::treq_download_pre_push_callback(TransferRequest *treq, 
	TransferDaemon *)
{
	int cluster;
	int proc;
	std::vector<PROC_ID> *jobs = NULL;
	size_t i;
	time_t now;

	dprintf(D_ALWAYS, "Scheduler::treq_download_pre_push_callback() called.\n");

	////////////////////////////////////////////////////////////////////////
	// Set ATTR_STAGE_OUT_START timestamp
	////////////////////////////////////////////////////////////////////////

	jobs = treq->get_procids();

	now = time(NULL);

	// set the stage out start time.
	for (i = 0; i < (*jobs).size(); i++) {
		SetAttributeInt((*jobs)[i].cluster,(*jobs)[i].proc,
						ATTR_STAGE_OUT_START,now);
	}

	////////////////////////////////////////////////////////////////////////
	// Get the actual (now modified) job ads and shove them into the request
	// for the transferd to munch on.
	////////////////////////////////////////////////////////////////////////

	for (i=0; i < (*jobs).size(); i++) {
		cluster = (*jobs)[i].cluster;
		proc = (*jobs)[i].proc;
		ClassAd * jad = GetJobAd( cluster, proc );
		treq->append_task(jad);
	}

	// Keep processing this request
	return TREQ_ACTION_CONTINUE;
}

TreqAction
Scheduler::treq_download_post_push_callback(TransferRequest *treq, 
	TransferDaemon *td)
{
	ReliSock *rsock = NULL;
	std::string sinful;
	std::string capability;
	ClassAd respad;
	std::string jids;
	std::string reason;

	////////////////////////////////////////////////////////////////////////
	// Respond to the client with a capability, a td sinful, the list of
	// jobids allowable for this fileset, and a list of protocols the td is
	// willing to use.
	// Use the sock stashed in the treq, then close the socket to the client.
	////////////////////////////////////////////////////////////////////////

	dprintf(D_ALWAYS, 
		"Scheduler::treq_download_post_push_callback() called.\n");

	rsock = treq->get_client_sock();
	// This means I might have accidentily recycled a treq without 
	// properly cleaning it up.
	ASSERT(rsock != NULL);

	rsock->encode();

	// If the request was rejected, then tell the client why
	if (treq->get_rejected() == true) {
		respad.Assign(ATTR_TREQ_INVALID_REQUEST, TRUE);
		reason = treq->get_rejected_reason();
		respad.Assign(ATTR_TREQ_INVALID_REASON, reason);
	} else {

		sinful = td->get_sinful();
		capability = treq->get_capability();

		procids_to_string(treq->get_procids(), jids);

		// This is what the transferd is willing to do for this request.
		respad.Assign(ATTR_TREQ_INVALID_REQUEST, FALSE);
		respad.Assign(ATTR_TREQ_CAPABILITY, capability);
		respad.Assign(ATTR_TREQ_TD_SINFUL, sinful);
		respad.Assign(ATTR_TREQ_FTP, treq->get_xfer_protocol());
		respad.Assign(ATTR_TREQ_JOBID_ALLOW_LIST, jids);
	}

	// This response ad from the schedd will contain:
	//	ATTR_TREQ_INVALID_REQUEST (set to true)
	//	ATTR_TREQ_INVALID_REASON
	//
	//	OR
	//
	//	ATTR_TREQ_INVALID_REQUEST (set to false)
	//	ATTR_TREQ_CAPABILITY
	//	ATTR_TREQ_TD_SINFUL
	//	ATTR_TREQ_FTP
	//	ATTR_TREQ_JOBID_ALLOW_LIST
	//
	dprintf(D_ALWAYS, "Scheduler::treq_download_post_push_callback() "
		"Responding to client about sandbox request and closing connection.\n");
	putClassAd(rsock, respad);
	rsock->end_of_message();

	// close the connection to the client now that I've told it where it can
	// get its files.
	delete rsock;
	
	// I told the client to go away, so I don't have this anymore.
	treq->set_client_sock(NULL);

	// Keep processing this request
	return TREQ_ACTION_CONTINUE;
}

TreqAction
Scheduler::treq_download_update_callback(TransferRequest *treq, 
	TransferDaemon *, ClassAd *)
{
	size_t i;
	std::vector<PROC_ID> *jobs;
	time_t now;

	dprintf(D_ALWAYS, "Scheduler::treq_download_update_callback() called.\n");

	////////////////////////////////////////////////////////////////////////
	// Determine if I like the update callback.
	////////////////////////////////////////////////////////////////////////

	// XXX TODO
	// assume it is ok.

	////////////////////////////////////////////////////////////////////////
	// The Mojo to get the job off of hold now that the transfer is complete
	////////////////////////////////////////////////////////////////////////

	jobs = treq->get_procids();
	ASSERT(jobs);

	now = time(NULL);

	for (i=0; i < jobs->size(); i++) {
		SetAttributeInt((*jobs)[i].cluster, (*jobs)[i].proc,
			ATTR_STAGE_OUT_FINISH, now);
	}

	////////////////////////////////////////////////////////////////////////
	// After proper fixing up of the job in the queue, tell the callback engine
	// that I'm done with this request. A termination returns value will
	// have the callback engine free the treq.
	////////////////////////////////////////////////////////////////////////

	return TREQ_ACTION_TERMINATE;
}

// This function does NOT own the memory passed to it. If you want a copy
// of the treq, you must make one.
TreqAction
Scheduler::treq_download_reaper_callback(TransferRequest *)
{
	dprintf(D_ALWAYS, "Scheduler::treq_download_reaper_callback() called.\n");

	////////////////////////////////////////////////////////////////////////
	// Until we figure out a better solution to what we do here, leave the
	// job on hold, and terminate the request.
	////////////////////////////////////////////////////////////////////////
	
	return TREQ_ACTION_TERMINATE;
}


































































#if 0


// a client is uploading files to the schedd
int
Scheduler::spoolJobFilesWorkerThread(void *arg, Stream* s)
{
	int ret_val;

	// a client is uploading files to the schedd
	ret_val = uploadGeneralJobFilesWorkerThread(arg,s);

		// Now we sleep here for one second.  Why?  So we are certain
		// to transfer back output files even if the job ran for less 
		// than one second. This is because:
		// stat() can't tell the difference between:
		//   1) A job starts up, touches a file, and exits all in one second
		//   2) A job starts up, doesn't touch the file, and exits all in one 
		//	  second
		// So if we force the start time of the job to be one second later than
		// the time we know the files were written, stat() should be able
		// to perceive what happened, if anything.
		dprintf(D_ALWAYS,"Scheduler::spoolJobFilesWorkerThread(void *arg, Stream* s) NAP TIME\n");
	sleep(1);
	return ret_val;
}

// upload files to the schedd
int
Scheduler::uploadGeneralJobFilesWorkerThread(void *arg, Stream* s)
{
	ReliSock* rsock = (ReliSock*)s;
	int JobAdsArrayLen = 0;
	int i;
	ExtArray<PROC_ID> *jobs = ((job_data_transfer_t *)arg)->jobs;
	char *peer_version = ((job_data_transfer_t *)arg)->peer_version;
	int mode = ((job_data_transfer_t *)arg)->mode;
	int result;
	int old_timeout;
	int cluster, proc;
	
	/* Setup a large timeout; when lots of jobs are being submitted w/ 
	 * large sandboxes, the default is WAY to small...
	 */
	old_timeout = s->timeout(60 * 60 * 8);  

	JobAdsArrayLen = jobs->getlast() + 1;

	for (i=0; i<JobAdsArrayLen; i++) {
		FileTransfer ftrans;
		cluster = (*jobs)[i].cluster;
		proc = (*jobs)[i].proc;
		ClassAd * ad = GetJobAd( cluster, proc );
		if ( !ad ) {
			dprintf( D_ALWAYS, "generalJobFilesWorkerThread(): "
					 "job ad %d.%d not found\n",cluster,proc );
			refuse(s);
			s->timeout(old_timeout);
			return FALSE;
		} else {
			dprintf(D_FULLDEBUG,"generalJobFilesWorkerThread(): "
					"transfer files for job %d.%d\n",cluster,proc);
		}

		dprintf(D_ALWAYS, "The submitting job ad as the FileTransferObject sees it\n");
		dPrintAd(D_ALWAYS, *ad);

			// Create a file transfer object, with schedd as the server
		result = ftrans.SimpleInit(ad, true, true, rsock);
		if ( !result ) {
			dprintf( D_ALWAYS, "generalJobFilesWorkerThread(): "
					 "failed to init filetransfer for job %d.%d \n",
					 cluster,proc );
			refuse(s);
			s->timeout(old_timeout);
			return FALSE;
		}
		if ( peer_version != NULL ) {
			ftrans.setPeerVersion( peer_version );
		}

		// receive sandbox into the schedd The ftrans object is downloading,
		// but the client is uploading to the schedd.
		result = ftrans.DownloadFiles();

		if ( !result ) {
			dprintf( D_ALWAYS, "generalJobFilesWorkerThread(): "
					 "failed to transfer files for job %d.%d \n",
					 cluster,proc );
			refuse(s);
			s->timeout(old_timeout);
			return FALSE;
		}
	}	
		
		
	rsock->end_of_message();

	int answer;

	rsock->encode();

	answer = OK;
	if (!rsock->code(answer)) {
		dprintf(D_ALWAYS, "uploadGeneralJobFilesWorkerThread failed to get response\n");
		answer = ~OK;
	}

	rsock->end_of_message();

	s->timeout(old_timeout);

	if ( peer_version ) {
		free( peer_version );
	}

	if (answer == OK ) {
		return TRUE;
	} else {
		return FALSE;
	}
}



// uploading files to schedd reaper
int
Scheduler::spoolJobFilesReaper(int tid,int exit_status)
{
	ExtArray<PROC_ID> *jobs;
	const char *AttrsToModify[] = { 
		ATTR_JOB_CMD,
		ATTR_JOB_INPUT,
		ATTR_JOB_OUTPUT,
		ATTR_JOB_ERROR,
		ATTR_TRANSFER_INPUT_FILES,
		ATTR_TRANSFER_OUTPUT_FILES,
		ATTR_ULOG_FILE,
		ATTR_X509_USER_PROXY,
		NULL };		// list must end with a NULL


	dprintf(D_FULLDEBUG,"spoolJobFilesReaper tid=%d status=%d\n",
			tid,exit_status);

	time_t now = time(NULL);

		// find the list of jobs which we just finished receiving the files
	spoolJobFileWorkers->lookup(tid,jobs);

	if (!jobs) {
		dprintf(D_ALWAYS,"ERROR - JobFilesReaper no entry for tid %d\n",tid);
		return FALSE;
	}

	if (exit_status == FALSE) {
		dprintf(D_ALWAYS,"ERROR - Staging of job files failed!\n");
		spoolJobFileWorkers->remove(tid);
		delete jobs;
		return FALSE;
	}


	int i,cluster,proc,index;
	char new_attr_value[500];
	char *buf = NULL;
	ExprTree *expr = NULL;
	std::string SpoolSpace;
		// figure out how many jobs we're dealing with
	int len = (*jobs).getlast() + 1;


		// For each job, modify its ClassAd
	for (i=0; i < len; i++) {
		cluster = (*jobs)[i].cluster;
		proc = (*jobs)[i].proc;

		ClassAd *ad = GetJobAd(cluster,proc);
		if (!ad) {
			// didn't find this job ad, must've been removed?
			// just go to the next one
			continue;
		}
		SpooledJobFiles.getJobSpoolPath(ad, SpoolSpace);
		ASSERT(!SpoolSpace.empty());

		BeginTransaction();

			// Backup the original IWD at submit time
		if (buf) free(buf);
		buf = NULL;
		ad->LookupString(ATTR_JOB_IWD,&buf);
		if ( buf ) {
			snprintf(new_attr_value,500,"SUBMIT_%s",ATTR_JOB_IWD);
			SetAttributeString(cluster,proc,new_attr_value,buf);
			free(buf);
			buf = NULL;
		}
			// Modify the IWD to point to the spool space			
		SetAttributeString(cluster,proc,ATTR_JOB_IWD,SpoolSpace.c_str());

			// Backup the original TRANSFER_OUTPUT_REMAPS at submit time
		expr = ad->LookupExpt(ATTR_TRANSFER_OUTPUT_REMAPS);
		snprintf(new_attr_value,500,"SUBMIT_%s",ATTR_TRANSFER_OUTPUT_REMAPS);
		if ( expr ) {
			const char *remap_buf = ExprTreeToString( expr );
			ASSERT(remap_buf);
			SetAttribute(cluster,proc,new_attr_value,remap_buf);
		}
		else if(ad->LookupExpr(new_attr_value)) {
				// SUBMIT_TransferOutputRemaps is defined, but
				// TransferOutputRemaps is not; disable the former,
				// so that when somebody fetches the sandbox, nothing
				// gets remapped.
			SetAttribute(cluster,proc,new_attr_value,"Undefined");
		}
			// Set TRANSFER_OUTPUT_REMAPS to Undefined so that we don't
			// do remaps when the job's output files come back into the
			// spool space. We only want to remap when the submitter
			// retrieves the files.
		SetAttribute(cluster,proc,ATTR_TRANSFER_OUTPUT_REMAPS,"Undefined");

			// Now, for all the attributes listed in 
			// AttrsToModify, change them to be relative to new IWD
			// by taking the basename of all file paths.
		index = -1;
		while ( AttrsToModify[++index] ) {
				// Lookup original value
			if (buf) free(buf);
			buf = NULL;
			ad->LookupString(AttrsToModify[index],&buf);
			if (!buf) {
				// attribute not found, so no need to modify it
				continue;
			}
			if ( nullFile(buf) ) {
				// null file -- no need to modify it
				continue;
			}
				// Create new value - deal with the fact that
				// some of these attributes contain a list of pathnames
			StringList old_paths(buf,",");
			StringList new_paths(NULL,",");
			old_paths.rewind();
			char *old_path_buf;
			bool changed = false;
			const char *base = NULL;
			std::string new_path_buf;
			while ( (old_path_buf=old_paths.next()) ) {
				base = condor_basename(old_path_buf);
				if ( strcmp(base,old_path_buf)!=0 ) {
					sprintf(new_path_buf,
						"%s%c%s",SpoolSpace.c_str(),DIR_DELIM_CHAR,base);
					base = new_path_buf.c_str();
					changed = true;
				}
				new_paths.append(base);
			}
			if ( changed ) {
					// Backup original value
				snprintf(new_attr_value,500,"SUBMIT_%s",AttrsToModify[index]);
				SetAttributeString(cluster,proc,new_attr_value,buf);
					// Store new value
				char *new_value = new_paths.print_to_string();
				ASSERT(new_value);
				SetAttributeString(cluster,proc,AttrsToModify[index],new_value);
				free(new_value);
			}
		}

			// Set ATTR_STAGE_IN_FINISH if not already set.
		int spool_completion_time = 0;
		ad->LookupInteger(ATTR_STAGE_IN_FINISH,spool_completion_time);
		if ( !spool_completion_time ) {
			// The transfer thread specifically slept for 1 second
			// to ensure that the job can't possibly start (and finish)
			// prior to the timestamps on the file.  Unfortunately,
			// we note the transfer finish time _here_.  So we've got 
			// to back off 1 second.
			SetAttributeInt(cluster,proc,ATTR_STAGE_IN_FINISH,now - 1);
		}

			// And now release the job.
		releaseJob(cluster,proc,"Data files spooled",false,false,false,false);
		CommitTransaction();
	}

	daemonCore->Register_Timer( 0, 
						(TimerHandlercpp)&Scheduler::reschedule_negotiator_timer,
						"Scheduler::reschedule_negotiator", this );

	spoolJobFileWorkers->remove(tid);
	delete jobs;
	if (buf) free(buf);
	return TRUE;
}















// download files from schedd
int
Scheduler::downloadJobFiles(int mode, Stream* s)
{
	ReliSock* rsock = (ReliSock*)s;
	int JobAdsArrayLen = 0;
	ExtArray<PROC_ID> *jobs = NULL;
	char *constraint_string = NULL;
	int i;
	static int spool_reaper_id = -1;
	static int transfer_reaper_id = -1;
	PROC_ID a_job;
	int tid;
	char *peer_version = NULL;

		// make sure this connection is authenticated, and we know who
		// the user is.  also, set a timeout, since we don't want to
		// block long trying to read from our client.   
	rsock->timeout( 10 );  
	rsock->decode();

	if( ! rsock->triedAuthentication() ) {
		CondorError errstack;
		if( ! SecMan::authenticate_sock(rsock, WRITE, &errstack) ) {
				// we failed to authenticate, we should bail out now
				// since we don't know what user is trying to perform
				// this action.
				// TODO: it'd be nice to print out what failed, but we
				// need better error propagation for that...
			errstack.push( "SCHEDD", SCHEDD_ERR_SPOOL_FILES_FAILED,
					"Failure to spool job files - Authentication failed" );
			dprintf( D_ALWAYS, "spoolJobFiles() aborting: %s\n",
					 errstack.getFullText().c_str() );
			refuse( s );
			return FALSE;
		}
	}	


	rsock->decode();

	peer_version = NULL;
	if ( !rsock->code(peer_version) ) {
		dprintf( D_ALWAYS,
			 	"spoolJobFiles(): failed to read peer_version\n" );
		refuse(s);
		return FALSE;
	}
		// At this point, we are responsible for deallocating
		// peer_version with free()



	// read constraint string
	if ( !rsock->code(constraint_string) || constraint_string == NULL )
	{
			dprintf( D_ALWAYS, "spoolJobFiles(): "
				 	"failed to read constraint string\n" );
			refuse(s);
			return FALSE;
	}

	jobs = new ExtArray<PROC_ID>;
	ASSERT(jobs);

	time_t now = time(NULL);

	{
	ClassAd * tmp_ad = GetNextJobByConstraint(constraint_string,1);
	JobAdsArrayLen = 0;
	while (tmp_ad) {
		if ( OwnerCheck2(tmp_ad, rsock->getOwner()) )
		{
			(*jobs)[JobAdsArrayLen++] = a_job;
		}
		tmp_ad = GetNextJobByConstraint(constraint_string,0);
	}
	dprintf(D_FULLDEBUG, "Scheduler::spoolJobFiles: "
		"TRANSFER_DATA/WITH_PERMS: %d jobs matched constraint %s\n",
		JobAdsArrayLen, constraint_string);
	if (constraint_string) free(constraint_string);
		// Now set ATTR_STAGE_OUT_START
	for (i=0; i<JobAdsArrayLen; i++) {
			// TODO --- maybe put this in a transaction?
		SetAttributeInt((*jobs)[i].cluster,(*jobs)[i].proc,
						ATTR_STAGE_OUT_START,now);
	}
	}

	rsock->end_of_message();

		// DaemonCore will free the thread_arg for us when the thread
		// exits, but we need to free anything pointed to by
		// job_data_transfer_t ourselves. generalJobFilesWorkerThread()
		// will free 'peer_version' and our reaper will free 'jobs' (the
		// reaper needs 'jobs' for some of its work).
	job_data_transfer_t *thread_arg = (job_data_transfer_t *)malloc( sizeof(job_data_transfer_t) );
	thread_arg->mode = mode;
	thread_arg->peer_version = peer_version;
	thread_arg->jobs = jobs;

	if ( transfer_reaper_id == -1 ) {
		transfer_reaper_id = daemonCore->Register_Reaper(
				"transferJobFilesReaper",
				(ReaperHandlercpp) &Scheduler::transferJobFilesReaper,
				"transferJobFilesReaper",
				this
			);
		}

	// Start a new thread (process on Unix) to do the work
	tid = daemonCore->Create_Thread(
			(ThreadStartFunc) &Scheduler::transferJobFilesWorkerThread,
			(void *)thread_arg,
			s,
			transfer_reaper_id
			);

	if ( tid == FALSE ) {
		free(thread_arg);
		if ( peer_version ) {
			free( peer_version );
		}
		delete jobs;
		refuse(s);
		return FALSE;
	}

		// Place this tid into a hashtable so our reaper can finish up.
	spoolJobFileWorkers->insert(tid, jobs);
	
	return TRUE;
}

// a client is getting files from the schedd
int
Scheduler::transferJobFilesWorkerThread(void *arg, Stream* s)
{
	// a client is getting files from the schedd
	return downloadGeneralJobFilesWorkerThread(arg,s);
}



// get files from the schedd
int
Scheduler::downloadGeneralJobFilesWorkerThread(void *arg, Stream* s)
{
	ReliSock* rsock = (ReliSock*)s;
	int JobAdsArrayLen = 0;
	int i;
	ExtArray<PROC_ID> *jobs = ((job_data_transfer_t *)arg)->jobs;
	char *peer_version = ((job_data_transfer_t *)arg)->peer_version;
	int mode = ((job_data_transfer_t *)arg)->mode;
	int result;
	int old_timeout;
	int cluster, proc;
	
	/* Setup a large timeout; when lots of jobs are being submitted w/ 
	 * large sandboxes, the default is WAY to small...
	 */
	old_timeout = s->timeout(60 * 60 * 8);  

	JobAdsArrayLen = jobs->getlast() + 1;
//	dprintf(D_FULLDEBUG,"TODD spoolJobFilesWorkerThread: JobAdsArrayLen=%d\n",JobAdsArrayLen);

	// if sending sandboxes, first tell the client how many
	// we are about to send.
	dprintf(D_FULLDEBUG, "Scheduler::generalJobFilesWorkerThread: "
		"TRANSFER_DATA/WITH_PERMS: %d jobs to be sent\n", JobAdsArrayLen);
	rsock->encode();
	if ( !rsock->code(JobAdsArrayLen) || !rsock->end_of_message() ) {
		dprintf( D_ALWAYS, "generalJobFilesWorkerThread(): "
				 "failed to send JobAdsArrayLen (%d) \n",
				 JobAdsArrayLen );
		refuse(s);
		return FALSE;
	}

	for (i=0; i<JobAdsArrayLen; i++) {
		FileTransfer ftrans;
		cluster = (*jobs)[i].cluster;
		proc = (*jobs)[i].proc;
		ClassAd * ad = GetJobAd( cluster, proc );
		if ( !ad ) {
			dprintf( D_ALWAYS, "generalJobFilesWorkerThread(): "
					 "job ad %d.%d not found\n",cluster,proc );
			refuse(s);
			s->timeout(old_timeout);
			return FALSE;
		} else {
			dprintf(D_FULLDEBUG,"generalJobFilesWorkerThread(): "
					"transfer files for job %d.%d\n",cluster,proc);
		}

		dprintf(D_ALWAYS, "The submitting job ad as the FileTransferObject sees it\n");
		dPrintAd(D_ALWAYS, *ad);

			// Create a file transfer object, with schedd as the server
		result = ftrans.SimpleInit(ad, true, true, rsock);
		if ( !result ) {
			dprintf( D_ALWAYS, "generalJobFilesWorkerThread(): "
					 "failed to init filetransfer for job %d.%d \n",
					 cluster,proc );
			refuse(s);
			s->timeout(old_timeout);
			return FALSE;
		}
		if ( peer_version != NULL ) {
			ftrans.setPeerVersion( peer_version );
		}

		// Send or receive files as needed
		// send sandbox out of the schedd
		rsock->encode();
		// first send the classad for the job
		result = ad->put(*rsock);
		if (!result) {
			dprintf(D_ALWAYS, "generalJobFilesWorkerThread(): "
				"failed to send job ad for job %d.%d \n",
				cluster,proc );
		} else {
			rsock->end_of_message();
			// and then upload the files
			result = ftrans.UploadFiles();
		}

		if ( !result ) {
			dprintf( D_ALWAYS, "generalJobFilesWorkerThread(): "
					 "failed to transfer files for job %d.%d \n",
					 cluster,proc );
			refuse(s);
			s->timeout(old_timeout);
			return FALSE;
		}
	}	
		
		
	rsock->end_of_message();

	int answer;
	rsock->decode();
	answer = -1;

	if (!rsock->code(answer)) {
		dprintf(D_ALWAYS, "generalJobFilesWorkerThread, failed to get answer from peer\n");
		answer = -1;
	}
	rsock->end_of_message();
	s->timeout(old_timeout);

	if ( peer_version ) {
		free( peer_version );
	}

	if (answer == OK ) {
		return TRUE;
	} else {
		return FALSE;
	}
}




// dowloading files from schedd reaper
int
Scheduler::transferJobFilesReaper(int tid,int exit_status)
{
	ExtArray<PROC_ID> *jobs;
	int i;

	dprintf(D_FULLDEBUG,"transferJobFilesReaper tid=%d status=%d\n",
			tid,exit_status);

		// find the list of jobs which we just finished receiving the files
	spoolJobFileWorkers->lookup(tid,jobs);

	if (!jobs) {
		dprintf(D_ALWAYS,
			"ERROR - transferJobFilesReaper no entry for tid %d\n",tid);
		return FALSE;
	}

	if (exit_status == FALSE) {
		dprintf(D_ALWAYS,"ERROR - Staging of job files failed!\n");
		spoolJobFileWorkers->remove(tid);
		delete jobs;
		return FALSE;
	}

		// For each job, modify its ClassAd
	time_t now = time(NULL);
	int len = (*jobs).getlast() + 1;
	for (i=0; i < len; i++) {
			// TODO --- maybe put this in a transaction?
		SetAttributeInt((*jobs)[i].cluster,(*jobs)[i].proc,ATTR_STAGE_OUT_FINISH,now);
	}

		// Now, deallocate memory
	spoolJobFileWorkers->remove(tid);
	delete jobs;
	return TRUE;
}

#endif /* if 0 */
