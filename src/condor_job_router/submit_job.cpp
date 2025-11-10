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
#include "submit_job.h"
#include "condor_classad.h"
#include "condor_qmgr.h"
#include "dc_schedd.h"
#include "condor_ver_info.h"
#include "condor_attributes.h"
#include "proc.h"
#include "condor_uid.h"
#include "condor_event.h"
#include "write_user_log.h"
#include "condor_email.h"
#include "condor_arglist.h"
#include "format_time.h"
#include "set_user_priv_from_ad.h"
#include "classad/classad_distribution.h"
#include "file_transfer.h"
#include "exit.h"
#include "condor_holdcodes.h"
#include "spooled_job_files.h"
#include "classad_helpers.h"
#include "condor_config.h"
#include "submit_job.h"


ClaimJobResult claim_job(classad::ClassAd const &ad, const ScheddContactInfo & scci, int cluster, int proc, std::string& error_details, const char * my_identity)
{
	ClaimJobResult result = CJR_ERROR;
	int status = 0;
	char* managed = nullptr;

	DCSchedd schedd(scci.name, scci.pool, scci.address_file);
	if( ! schedd.locate() ) {
		error_details = "Can't find address of schedd";
		return CJR_ERROR;
	}

	std::string effective_owner;
	ad.EvaluateAttrString(ATTR_USER, effective_owner);

	CondorError errstack;
	Qmgr_connection * qmgr = ConnectQ(schedd, 0 /*timeout==default*/, false /*read-only*/, & errstack, effective_owner.c_str());
	if( ! qmgr ) {
		error_details = "Unable to connect: " + errstack.getFullText(true);
		return CJR_ERROR;
	}

	// Check that the job's status is still IDLE
	if( GetAttributeInt(cluster, proc, ATTR_JOB_STATUS, &status) == -1) {
		formatstr(error_details, "Encountered problem reading current %s for %d.%d", ATTR_JOB_STATUS, cluster, proc); 
		goto done;
	}
	if(status != IDLE) {
		formatstr(error_details, "Job %d.%d isn't idle, is %s (%d)", cluster, proc, getJobStatusString(status), status); 
		result = CJR_BUSY;
		goto done;
	}

	// Check that the job is still managed by the schedd
	if( GetAttributeStringNew(cluster, proc, ATTR_JOB_MANAGED, &managed) >= 0) {
		bool ok = strcmp(managed, MANAGED_SCHEDD) == 0;
		free(managed);
		if( ! ok ) {
			formatstr(error_details, "Job %d.%d is already managed by another process", cluster, proc); 
			result = CJR_BUSY;
			goto done;
		}
	}
	// else: No Managed attribute?  Assume it's schedd managed (ok) and carry on.

	// No one else has a claim.  Claim it ourselves.
	if( SetAttributeString(cluster, proc, ATTR_JOB_MANAGED, MANAGED_EXTERNAL) == -1 ) {
		formatstr(error_details, "Encountered problem setting %s = %s", ATTR_JOB_MANAGED, MANAGED_EXTERNAL); 
		goto done;
	}

	if(my_identity) {
		if( SetAttributeString(cluster, proc, ATTR_JOB_MANAGED_MANAGER, my_identity) == -1 ) {
			formatstr(error_details, "Encountered problem setting %s = %s", ATTR_JOB_MANAGED, MANAGED_EXTERNAL); 
			goto done;
		}
	}

	result = CJR_OK;

 done:
	// Tear down the qmgr
	if (qmgr) {
		bool commit = result == CJR_OK;
		if (! DisconnectQ(qmgr, commit)) {
			error_details = "Failed to commit job claim";
			result = CJR_ERROR;
		}
	}

	return result;
}


bool yield_job(classad::ClassAd const &ad, const ScheddContactInfo & scci,
	bool done, int cluster, int proc,
	std::string& error_details, const char * my_identity,
        bool release_on_hold, bool *keep_trying)
{
	bool success = false;
	const char * newsetting = nullptr;
	int status = 0;

	if (keep_trying) *keep_trying = true;

	DCSchedd schedd(scci.name, scci.pool, scci.address_file);
	if( ! schedd.locate() ) {
		error_details = "Can't find address of schedd";
		return false;
	}

	std::string effective_owner;
	ad.EvaluateAttrString(ATTR_USER, effective_owner);

	CondorError errstack;
	Qmgr_connection * qmgr = ConnectQ(schedd, 0 /*timeout==default*/, false /*read-only*/, & errstack, effective_owner.c_str());
	if( ! qmgr ) {
		error_details = "Unable to connect: " + errstack.getFullText(true);
		return false;
	}

	// Check that the job is still managed by us
	bool is_managed = false;
	char * managed;
	if( GetAttributeStringNew(cluster, proc, ATTR_JOB_MANAGED, &managed) >= 0) {
		is_managed = strcmp(managed, MANAGED_EXTERNAL) == 0;
		free(managed);
	}
	if( ! is_managed ) {
		formatstr(error_details, "Job %d.%d is not managed!", cluster, proc); 
		if (keep_trying) *keep_trying = false;
		goto done;
	}

	if(my_identity) {
		char * manager;
		if( GetAttributeStringNew(cluster, proc, ATTR_JOB_MANAGED_MANAGER, &manager) >= 0) {
			if(strcmp(manager, my_identity) != 0) {
				formatstr(error_details, "Job %d.%d is managed by '%s' instead of expected '%s'", cluster, proc, manager, my_identity);
				free(manager);
				if (keep_trying) *keep_trying = false;
				goto done;;
			}
			free(manager);
		}
	}

	newsetting = done ? MANAGED_DONE : MANAGED_SCHEDD;
	if( SetAttributeString(cluster, proc, ATTR_JOB_MANAGED, newsetting) == -1 ) {
		formatstr(error_details, "Encountered problem setting %s = %s", ATTR_JOB_MANAGED, newsetting); 
		goto done;
	}
		// Wipe the manager (if present).
	SetAttributeString(cluster, proc, ATTR_JOB_MANAGED_MANAGER, "");

	status = 0;
	if( GetAttributeInt(cluster, proc, ATTR_JOB_STATUS, &status) != -1) {
		if(status == REMOVED) {
			int rc = DestroyProc(cluster, proc);
			if(rc < 0) {
				dprintf(D_ERROR,"yield_job: failed to remove %d.%d: rc=%d\n",cluster,proc,rc);
				// Do not treat this as a failure of yield_job().
			}
		}
		else if(status != IDLE && status != COMPLETED) {
			int hold_mirrored = 0;
			if(status == HELD && release_on_hold == true) {
					// If held state is mirrored from target job, then
					// undo it.  Otherwise, leave it alone.
				GetAttributeInt(cluster, proc, ATTR_HOLD_COPIED_FROM_TARGET_JOB, &hold_mirrored);
				if( hold_mirrored ) {
					DeleteAttribute(cluster, proc, ATTR_HOLD_COPIED_FROM_TARGET_JOB );
				}
			}
			if(status != HELD || hold_mirrored) {
					// Return job to idle state.
				SetAttributeInt(cluster, proc, ATTR_JOB_STATUS, IDLE);
			}
		}
	}

	success = true;

 done:
	// Tear down the qmgr
	if (qmgr) {
		bool commit = success == true;
		if (! DisconnectQ(qmgr, commit)) {
			error_details = "Failed to commit job claim";
			success = false;
		}
	}

	return success;
}


bool submit_job(const std::string & owner, const std::string &domain, ClassAd & src, const ScheddContactInfo & scci, bool is_sandboxed, int * cluster_out /*= 0*/, int * proc_out /*= 0 */)
{
	bool success = false;
	int cluster = -1;
	int proc = -1;
	std::string leaveinqueue;
	bool put_in_proc = false;
	ExprTree * tree = nullptr;
	const char *lhstr = 0;
	const char *rhstr = 0;

	DCSchedd schedd(scci.name, scci.pool, scci.address_file);
	if( ! schedd.locate() ) {
		dprintf(D_ERROR, "Can't find address of schedd\n");
		return false;
	}

	// TODO: fix this to use Job owner/user instead of OS user
	// When USERREC_NAME_IS_FULLY_QUALIFIED in enabled in the schedd
	// we must pass a fully qualified name to ConnectQ.
	// But using the OSuser name here wrong and will only work as
	// long as the schedd cannot separate OSuser from job Owner.
	// The reason we can't just use the auth identity of the socket
	// to place jobs is that in most cases it will be condor@family
	std::string user(owner);
	user += '@';
	#ifdef WIN32
	user += domain;
	#else
	// TODO: use schedd2's value of UID_DOMAIN instead.
	auto_free_ptr uid_domain(param("UID_DOMAIN"));
	user += uid_domain.ptr();
	#endif

	CondorError errstack;
	Qmgr_connection * qmgr = ConnectQ(schedd, 0 /*timeout==default*/, false /*read-only*/, & errstack, user.c_str());
	if( !qmgr ) {
		dprintf(D_ERROR, "Unable to connect to schedd: %s\n", errstack.getFullText(true).c_str());
		return false;
	}

	// Schedd clients can't set X509- or token-related attributes
	// other than the name of the proxy file.
	std::set<std::string, classad::CaseIgnLTStr> filter_attrs;
	filter_attrs.insert( ATTR_X509_USER_PROXY_SUBJECT );
	filter_attrs.insert( ATTR_X509_USER_PROXY_EXPIRATION );
	filter_attrs.insert( ATTR_X509_USER_PROXY_EMAIL );
	filter_attrs.insert( ATTR_X509_USER_PROXY_VONAME );
	filter_attrs.insert( ATTR_X509_USER_PROXY_FIRST_FQAN );
	filter_attrs.insert( ATTR_X509_USER_PROXY_FQAN );
	filter_attrs.insert( ATTR_TOKEN_SUBJECT );
	filter_attrs.insert( ATTR_TOKEN_ISSUER );
	filter_attrs.insert( ATTR_TOKEN_GROUPS );
	filter_attrs.insert( ATTR_TOKEN_SCOPES );
	filter_attrs.insert( ATTR_TOKEN_ID );

	std::set<std::string, classad::CaseIgnLTStr> proc_attrs;
	proc_attrs.insert(ATTR_PROC_ID);
	proc_attrs.insert(ATTR_JOB_STATUS);

	cluster = NewCluster();
	if( cluster < 0 ) {
		dprintf(D_ERROR, "Failed to create a new cluster (%d)\n", cluster);
		goto submit_done;
	}

	proc = NewProc(cluster);
	if( proc < 0 ) {
		dprintf(D_ERROR, "Failed to create a new proc (%d)\n", proc);
		goto submit_done;
	}

	src.Assign(ATTR_PROC_ID, proc);
	src.Assign(ATTR_CLUSTER_ID, cluster);
	src.Assign(ATTR_Q_DATE, time(nullptr));
	src.Assign(ATTR_ENTERED_CURRENT_STATUS, time(nullptr));

	// Things to set because we want to spool/sandbox input files
	if( is_sandboxed ) {
		// we need to submit on hold (taken from condor_submit.V6/submit.C)
		src.Assign(ATTR_JOB_STATUS, 5); // 5==HELD
		src.Assign(ATTR_HOLD_REASON, "Spooling input data files");
		src.Assign(ATTR_HOLD_REASON_CODE, CONDOR_HOLD_CODE::SpoolingInput);

			// See the comment in the function body of ExpandInputFileList
			// for an explanation of what is going on here.
		std::string transfer_input_error_msg;
		if( !FileTransfer::ExpandInputFileList( &src, transfer_input_error_msg ) ) {
			dprintf(D_ERROR, "%s\n", transfer_input_error_msg.c_str());
			goto submit_done;
		}
	}

		// we want the job to hang around (taken from condor_submit.V6/submit.C)
	formatstr(leaveinqueue, "%s == %d", ATTR_JOB_STATUS, COMPLETED);
	src.AssignExpr(ATTR_JOB_LEAVE_IN_QUEUE, leaveinqueue.c_str());

	for( auto itr = src.begin(); itr != src.end(); itr++ ) {
		lhstr = itr->first.c_str();
		tree = itr->second;
		if ( filter_attrs.find( lhstr ) != filter_attrs.end() ) {
			continue;
		}
		put_in_proc = proc_attrs.find(lhstr) != proc_attrs.end();
		rhstr = ExprTreeToString( tree );
		if( !rhstr) { 
			dprintf(D_ERROR, "Problem processing classad\n");
			goto submit_done;
		}
		if( SetAttribute(cluster, put_in_proc ? proc : -1, lhstr, rhstr) == -1 ) {
			dprintf(D_ERROR, "Failed to set %s = %s\n", lhstr, rhstr);
			goto submit_done;
		}
	}

	success = true;

 submit_done:
	bool commit = success;
	if( qmgr && ! DisconnectQ(qmgr, commit, &errstack)) {
		dprintf(D_ERROR, "Failed to commit job submission : %s\n", errstack.getFullText(true).c_str());
		return false;
	}
	if (!success) {
		return false;
	}
	if ( ! errstack.empty()) {
		dprintf(D_ALWAYS, "job submmission warning : %s\n", errstack.getFullText(true).c_str());
		errstack.clear();
	}

	if( is_sandboxed ) {
		// TODO Once job User and OsUser can diverge, we need to authenticate
		//   as the User but be in PRIV_USER as the OsUser for file access.
		TemporaryPrivSentry sentry;
		if (!init_user_ids(owner.c_str(), domain.c_str())) {
			dprintf(D_ERROR, "Failed in init_user_ids(%s,%s)\n",
					owner.c_str(), domain.c_str());
			return false;
		}
		set_user_priv();

		ClassAd * adlist[1];
		adlist[0] = &src;
		if( ! schedd.spoolJobFiles(1, adlist, &errstack) ) {
			dprintf(D_ERROR, "Failed to spool job files: %s\n", errstack.getFullText(true).c_str());
			return false;
		}
	}

	schedd.reschedule();

	if(cluster_out) { *cluster_out = cluster; }
	if(proc_out) { *proc_out = proc; }

	return true;
}

/*
	Push the dirty attributes in src into the queue.  Does _not_ clear
	the dirty attributes.
*/
bool push_dirty_attributes(classad::ClassAd & src, const ScheddContactInfo & scci)
{
	bool success = false;

	DCSchedd schedd(scci.name, scci.pool, scci.address_file);
	if( ! schedd.locate() ) {
		dprintf(D_ERROR, "Can't find address of schedd\n");
		return false;
	}

	std::string effective_owner;
	src.EvaluateAttrString(ATTR_USER, effective_owner);

	CondorError errstack;
	Qmgr_connection * qmgr = ConnectQ(schedd, 0 /*timeout==default*/, false /*read-only*/, & errstack, effective_owner.c_str());
	if( ! qmgr ) {
		dprintf(D_ERROR, "Unable to connect: %s\n", errstack.getFullText(true).c_str());
		return false;
	}

	int cluster;
	int proc;
	const char *rhstr = 0;
	ExprTree * tree;
	if( ! src.EvaluateAttrInt(ATTR_CLUSTER_ID, cluster) ) {
		dprintf(D_ERROR, "push_dirty_attributes: job lacks a cluster\n");
		goto done;
	}
	if( ! src.EvaluateAttrInt(ATTR_PROC_ID, proc) ) {
		dprintf(D_ERROR, "push_dirty_attributes: job lacks a proc\n");
		goto done;
	}
	if( src.IsAttributeDirty(ATTR_JOB_STATUS) ) {
		int schedd_status = IDLE;
		if( GetAttributeInt(cluster, proc, ATTR_JOB_STATUS, &schedd_status) < 0 ) {
			dprintf(D_ERROR, "(%d.%d) push_dirty_attributes: Failed to get %s\n", cluster, proc, ATTR_JOB_STATUS);
			goto done;
		}
		if( schedd_status == REMOVED || schedd_status == COMPLETED ) {
			dprintf(D_FULLDEBUG, "Not altering %s away from REMOVED\n", ATTR_JOB_STATUS);
			src.MarkAttributeClean(ATTR_JOB_STATUS);
		}
	}
	for( classad::ClassAd::dirtyIterator it = src.dirtyBegin();
		 it != src.dirtyEnd(); ++it) {

		rhstr = NULL;
		tree = src.Lookup( *it );
		if ( tree ) {
			rhstr = ExprTreeToString( tree );
		}
		if( !rhstr) { 
			dprintf(D_ERROR, "(%d.%d) push_dirty_attributes: Problem processing classad\n", cluster, proc);
			goto done;
		}
		dprintf(D_FULLDEBUG, "Setting %s = %s\n", it->c_str(), rhstr);
		if( SetAttribute(cluster, proc, it->c_str(), rhstr) == -1 ) {
			dprintf(D_ERROR, "(%d.%d) push_dirty_attributes: Failed to set %s = %s\n", cluster, proc, it->c_str(), rhstr);
			goto done;
		}
	}

	success = true;

 done:
	if (qmgr) {
		bool commit = success;
		if (! DisconnectQ(qmgr, commit)) {
			dprintf(D_ERROR, "push_dirty_attributes: Failed to commit changes\n");
			success = false;
		}
	}

	return success;
}

/*
	Update src in the queue so that it ends up looking like dest.
    This handles attribute deletion as well as change of value.
*/
bool push_classad_diff(classad::ClassAd & src, classad::ClassAd & dest, const ScheddContactInfo & scci)
{
	bool success = false;

	DCSchedd schedd(scci.name, scci.pool, scci.address_file);
	if( ! schedd.locate() ) {
		dprintf(D_ERROR, "Can't find address of schedd\n");
		return false;
	}

	std::string effective_owner;
	src.EvaluateAttrString(ATTR_USER, effective_owner);

	CondorError errstack;
	Qmgr_connection * qmgr = ConnectQ(schedd, 0 /*timeout==default*/, false /*read-only*/, & errstack, effective_owner.c_str());
	if( ! qmgr ) {
		dprintf(D_ERROR, "Unable to connect: %s\n", errstack.getFullText(true).c_str());
		return false;
	}

	int cluster;
	int proc;
	classad::References attrs;

	if( ! src.EvaluateAttrInt(ATTR_CLUSTER_ID, cluster) ) {
		dprintf(D_ERROR, "push_classad_diff: job lacks a cluster\n");
		goto done;
	}
	if( ! src.EvaluateAttrInt(ATTR_PROC_ID, proc) ) {
		dprintf(D_ERROR, "push_classad_diff: job lacks a proc\n");
		goto done;
	}

	for( classad::ClassAd::iterator src_it = src.begin();
		 src_it != src.end(); ++src_it)
	{
		attrs.insert( src_it->first );
	}
	for( classad::ClassAd::iterator dest_it = dest.begin();
		 dest_it != dest.end(); ++dest_it)
	{
		attrs.insert( dest_it->first );
	}

	for ( classad::References::iterator attrs_itr = attrs.begin();
		  attrs_itr != attrs.end(); attrs_itr++ )
	{
		std::string src_rhstr;
		std::string dest_rhstr;
		ExprTree * src_tree;
		ExprTree * dest_tree;

		char const *attr = attrs_itr->c_str();

		src_tree = src.Lookup( attr );
		if ( src_tree ) {
			char const *rhstr = ExprTreeToString( src_tree );
			if( !rhstr) {
				dprintf(D_ERROR, "(%d.%d) push_classad_diff: Problem processing classad attribute %s\n", cluster, proc, attr);
				goto done;
			}
			src_rhstr = rhstr;
		}

		dest_tree = dest.Lookup( attr );
		if ( dest_tree ) {
			char const *rhstr = ExprTreeToString( dest_tree );
			if( !rhstr) {
				dprintf(D_ERROR, "(%d.%d) push_classad_diff: Problem processing classad attribute %s\n", cluster, proc, attr);
				goto done;
			}
			dest_rhstr = rhstr;
			if( dest_rhstr == src_rhstr ) {
				continue;
			}
		}
		else {
			dprintf(D_FULLDEBUG, "Deleting %s\n", attr);
			DeleteAttribute(cluster, proc, attr);
			continue;
		}

		dprintf(D_FULLDEBUG, "Setting %s = %s\n", attr, dest_rhstr.c_str());
		if( SetAttribute(cluster, proc, attr, dest_rhstr.c_str()) == -1 ) {
			dprintf(D_ERROR, "(%d.%d) push_classad_diff: Failed to set %s = %s\n", cluster, proc, attr, dest_rhstr.c_str());
			goto done;
		}
	}

	success = true;

 done:
	if (qmgr) {
		bool commit = success;
		if (! DisconnectQ(qmgr, commit)) {
			dprintf(D_ERROR, "push_dirty_attributes: Failed to commit changes\n");
			success = false;
		}
	}

	return success;
}

bool finalize_job(const std::string & owner, const std::string &domain, classad::ClassAd const &ad,int cluster, int proc, const ScheddContactInfo & scci, bool is_sandboxed)
{
	bool success = false;
	std::string effective_owner;
	CondorError errstack;
	Qmgr_connection * qmgr = nullptr;

	DCSchedd schedd(scci.name, scci.pool, scci.address_file);
	if( ! schedd.locate() ) {
		dprintf(D_ERROR, "Can't find address of schedd\n");
		return false;
	}

	if( is_sandboxed ) {
		// TODO Once job User and OsUser can diverge, we need to authenticate
		//   as the User but be in PRIV_USER as the OsUser for file access.
		TemporaryPrivSentry sentry;
		if (!init_user_ids(owner.c_str(), domain.c_str())) {
			dprintf(D_ERROR, "Failed in init_user_ids(%s,%s)\n",
					owner.c_str(), domain.c_str());
			return false;
		}
		set_user_priv();

		std::string constraint;
		formatstr(constraint, "(ClusterId==%d&&ProcId==%d)", cluster, proc);

			// Get our sandbox back
		int jobssent;
		CondorError errstack;
		bool success = schedd.receiveJobSandbox(constraint.c_str(), &errstack, &jobssent);
		if( ! success ) {
			dprintf(D_ERROR, "(%d.%d) Failed to retrieve sandbox.\n", cluster, proc);
			goto done;
		}

		if(jobssent != 1) {
			dprintf(D_ERROR, "(%d.%d) Failed to retrieve sandbox (got %d, expected 1).\n", cluster, proc, jobssent);
			goto done;
		}
	}

	// Yield the job (clear MANAGED).
	ad.EvaluateAttrString(ATTR_USER, effective_owner);

	qmgr = ConnectQ(schedd, 0 /*timeout==default*/, false /*read-only*/, & errstack, effective_owner.c_str());
	if( ! qmgr ) {
		dprintf(D_ERROR, "Unable to connect: %s\n", errstack.getFullText(true).c_str());
		goto done;
	}

	if( SetAttribute(cluster, proc, ATTR_JOB_LEAVE_IN_QUEUE, "FALSE") == -1 ) {
		dprintf(D_ERROR, "finalize_job: failed to set %s = %s for %d.%d\n", ATTR_JOB_LEAVE_IN_QUEUE, "FALSE", cluster, proc);
		goto done;
	}

	success = true;

 done:
	if (qmgr) {
		bool commit = success;
		if (! DisconnectQ(qmgr, commit)) {
			dprintf(D_ERROR, "finalize_job: Failed to commit changes\n");
			success = false;
		}
	}

	return success;
}

bool remove_job(int cluster, int proc, char const *reason, const ScheddContactInfo & scci, std::string &error_desc)
{
	bool success = false;
	CondorError errstack;
 
	DCSchedd schedd(scci.name, scci.pool, scci.address_file);
	if( ! schedd.locate() ) {
		dprintf(D_ERROR, "Can't find address of schedd\n");
		return false;
	}

	std::string id_str;
	formatstr(id_str, "%d.%d", cluster, proc);
	std::vector<std::string> job_ids = {id_str};
	ClassAd *result_ad;

	result_ad = schedd.removeJobs(job_ids, reason, &errstack, AR_LONG);

	PROC_ID job_id;
	job_id.cluster = cluster;
	job_id.proc = proc;

	char *err_str = NULL;
	if(result_ad) {
		JobActionResults results(AR_LONG);
		results.readResults(result_ad);
		success = results.getResultString(job_id, &err_str);
	}
	else {
		error_desc = "No result ClassAd returned by schedd.removeJobs()";
		success = false;
	}

	if(!success && err_str) {
		error_desc = err_str;
	}
	free(err_str);
	delete result_ad;

	return success;
}

bool InitializeAbortedEvent( JobAbortedEvent *event, classad::ClassAd const &job_ad )
{
	int cluster, proc;

		// This code is copied from gridmanager basejob.C with a small
		// amount of refactoring.
		// TODO: put this code in a common place.

	job_ad.EvaluateAttrInt( ATTR_CLUSTER_ID, cluster );
	job_ad.EvaluateAttrInt( ATTR_PROC_ID, proc );

	dprintf( D_FULLDEBUG, 
			 "(%d.%d) Writing abort record to user logfile\n",
			 cluster, proc );

	std::string reasonstr;
	job_ad.EvaluateAttrString(ATTR_REMOVE_REASON, reasonstr);
	event->setReason(reasonstr);

	return true;
}

bool InitializeTerminateEvent( TerminatedEvent *event, classad::ClassAd const &job_ad )
{
	int cluster, proc;

		// This code is copied from gridmanager basejob.C with a small
		// amount of refactoring.
		// TODO: put this code in a common place.

	job_ad.EvaluateAttrInt( ATTR_CLUSTER_ID, cluster );
	job_ad.EvaluateAttrInt( ATTR_PROC_ID, proc );

	dprintf( D_FULLDEBUG, 
			 "(%d.%d) Writing terminate record to user logfile\n",
			 cluster, proc );

	struct rusage r;
	memset( &r, 0, sizeof( struct rusage ) );

#if !defined(WIN32)
	event->run_local_rusage = r;
	event->run_remote_rusage = r;
	event->total_local_rusage = r;
	event->total_remote_rusage = r;
#endif /* WIN32 */
	event->sent_bytes = 0;
	event->recvd_bytes = 0;
	event->total_sent_bytes = 0;
	event->total_recvd_bytes = 0;

	bool exited_by_signal;
	if( job_ad.EvaluateAttrBool(ATTR_ON_EXIT_BY_SIGNAL, exited_by_signal) ) {
		int int_val;
		if( exited_by_signal ) {
			if( job_ad.EvaluateAttrInt(ATTR_ON_EXIT_SIGNAL, int_val) ) {
				event->signalNumber = int_val;
				event->normal = false;
			} else {
				dprintf( D_ALWAYS, "(%d.%d) Job ad lacks %s.  "
						 "Signal code unknown.\n", cluster, proc, 
						 ATTR_ON_EXIT_SIGNAL);
				event->normal = false;
					// TODO What about event->signalNumber?
			}
		} else {
			if( job_ad.EvaluateAttrInt(ATTR_ON_EXIT_CODE, int_val) ) {
				event->normal = true;
				event->returnValue = int_val;
			} else {
				dprintf( D_ALWAYS, "(%d.%d) Job ad lacks %s.  "
						 "Return code unknown.\n", cluster, proc, 
						 ATTR_ON_EXIT_CODE);
				event->normal = false;
					// TODO What about event->signalNumber?
			}
		}
	} else {
		dprintf( D_ALWAYS,
				 "(%d.%d) Job ad lacks %s.  Final state unknown.\n",
				 cluster, proc, ATTR_ON_EXIT_BY_SIGNAL);
		event->normal = false;
			// TODO What about event->signalNumber?
	}

	double real_val = 0;
	if ( job_ad.EvaluateAttrReal( ATTR_JOB_REMOTE_USER_CPU, real_val ) ) {
		event->run_remote_rusage.ru_utime.tv_sec = (time_t)real_val;
		event->total_remote_rusage.ru_utime.tv_sec = (time_t)real_val;
	}
	if ( job_ad.EvaluateAttrReal( ATTR_JOB_REMOTE_SYS_CPU, real_val ) ) {
		event->run_remote_rusage.ru_stime.tv_sec = (time_t)real_val;
		event->total_remote_rusage.ru_stime.tv_sec = (time_t)real_val;
	}

	setEventUsageAd(job_ad, &event->pusageAd);

	return true;
}

bool InitializeHoldEvent( JobHeldEvent *event, classad::ClassAd const &job_ad )
{
	int cluster, proc;

		// This code is copied from gridmanager basejob.C with a small
		// amount of refactoring.
		// TODO: put this code in a common place.

	job_ad.EvaluateAttrInt( ATTR_CLUSTER_ID, cluster );
	job_ad.EvaluateAttrInt( ATTR_PROC_ID, proc );

	dprintf( D_FULLDEBUG, 
			 "(%d.%d) Writing hold record to user logfile\n",
			 cluster, proc );

	std::string reasonstr;
	job_ad.EvaluateAttrString(ATTR_REMOVE_REASON, reasonstr);
	event->setReason(reasonstr);

	return true;
}

bool WriteEventToUserLog( ULogEvent &event, classad::ClassAd const &ad )
{
	WriteUserLog ulog;

	if ( ! ulog.initialize(ad, true) ) {
		dprintf( D_FULLDEBUG,
				 "(%d.%d) Unable to open user log (event %d)\n",
				 event.cluster, event.proc, event.eventNumber );
		return false;
	}
	if ( ! ulog.willWrite() ) {
		return true;
	}

	int rc = ulog.writeEvent(&event, &ad);

	if (!rc) {
		dprintf( D_FULLDEBUG,
				 "(%d.%d) Unable to update user log (event %d)\n",
				 event.cluster, event.proc, event.eventNumber );
		return false;
	}

	return true;
}

bool WriteTerminateEventToUserLog( classad::ClassAd const &ad )
{
	JobTerminatedEvent event;

	if(!InitializeTerminateEvent(&event,ad)) {
		return false;
	}

	return WriteEventToUserLog( event, ad );
}

bool WriteAbortEventToUserLog( classad::ClassAd const &ad )
{
	JobAbortedEvent event;

	if(!InitializeAbortedEvent(&event,ad)) {
		return false;
	}

	return WriteEventToUserLog( event, ad );
}

bool WriteHoldEventToUserLog( classad::ClassAd const &ad )
{
	JobHeldEvent event;
	if(!InitializeHoldEvent(&event,ad))
	{
		return false;
	}

	return WriteEventToUserLog( event, ad );
}

bool WriteExecuteEventToUserLog( classad::ClassAd const &ad )
{
	int cluster;
	int proc;
	ad.EvaluateAttrInt( ATTR_CLUSTER_ID, cluster );
	ad.EvaluateAttrInt( ATTR_PROC_ID, proc );

	dprintf( D_FULLDEBUG,
			 "(%d.%d) Writing execute record to user logfile\n",
			 cluster, proc );

	ExecuteEvent event;

	std::string routed_id;
	ad.EvaluateAttrString( "RoutedToJobId", routed_id );
	event.setExecuteHost( routed_id.c_str() );

	return WriteEventToUserLog( event, ad );
}

bool WriteEvictEventToUserLog( classad::ClassAd const &ad )
{
	int cluster;
	int proc;
	ad.EvaluateAttrInt( ATTR_CLUSTER_ID, cluster );
	ad.EvaluateAttrInt( ATTR_PROC_ID, proc );

	dprintf( D_FULLDEBUG,
			 "(%d.%d) Writing evict record to user logfile\n",
			 cluster, proc );
	JobEvictedEvent event;

	return WriteEventToUserLog( event, ad );
}


// The following is copied from gridmanager/basejob.C
// TODO: put the code into a shared file.

void
EmailTerminateEvent(ClassAd * job_ad, bool   /*exit_status_known*/)
{
	if ( !job_ad ) {
		dprintf(D_ALWAYS, 
				"email_terminate_event called with invalid ClassAd\n");
		return;
	}

	Email email;
	email.sendExit(job_ad, JOB_EXITED);
}

bool EmailTerminateEvent( classad::ClassAd const &ad )
{
	ClassAd old_ad = ad;

	EmailTerminateEvent( &old_ad, true );
	return true;
}
