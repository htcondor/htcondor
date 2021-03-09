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

	// Simplify my error handling and reporting code
class FailObj {
public:
	FailObj() : cluster(-1), proc(-1), qmgr(0), save_error_msg(0) { }

	void SetCluster(int c) { cluster = c; }
	void SetProc(int p) { proc = p; }

	void SetSaveErrorMsg(std::string *s) { save_error_msg = s; }

	void SetNames(const char * schedd_name, const char * pool_name) {
		if(schedd_name) {
			names = "schedd ";
			names += schedd_name;
		} else {
			names = "local schedd";
		}
		names += " at ";
		if(pool_name) {
			names += "pool ";
			names += pool_name;
		} else {
			names += "local pool";
		}
	}

	void SetQmgr(Qmgr_connection * q) { qmgr = q; }

	void fail( const char * fmt, ...) {
		if(cluster != -1 && qmgr) {
			DestroyCluster(cluster);
		}
		if(qmgr) { DisconnectQ( qmgr, false /* don't commit */); }

		std::string msg;
		msg = "ERROR ";
		if(names.length()) {
			msg += "(";
			msg += names;
			msg += ") ";
		}

		if(cluster != -1) {
			msg += "(";
			msg += std::to_string( cluster );
			if(proc != -1) {
				msg += ".";
				msg += std::to_string( proc );
			}
			msg += ") ";
		}
		va_list args;
		va_start(args,fmt);
		vformatstr_cat(msg,fmt,args);
		va_end(args);


		if( save_error_msg ) {
			*save_error_msg = msg;
		}
		else {
			dprintf(D_ALWAYS, "%s", msg.c_str());
		}
	}
private:
	std::string names;
	int cluster;
	int proc;
	Qmgr_connection * qmgr;
	std::string *save_error_msg;
};


ClaimJobResult claim_job(int cluster, int proc, std::string * error_details, const char * my_identity)
{
	ASSERT(cluster > 0);
	ASSERT(proc >= 0);

	// Check that the job's status is still IDLE
	int status;
	if( GetAttributeInt(cluster, proc, ATTR_JOB_STATUS, &status) == -1) {
		if(error_details) {
			formatstr(*error_details, "Encountered problem reading current %s for %d.%d", ATTR_JOB_STATUS, cluster, proc); 
		}
		return CJR_ERROR;
	}
	if(status != IDLE) {
		if(error_details) {
			formatstr(*error_details, "Job %d.%d isn't idle, is %s (%d)", cluster, proc, getJobStatusString(status), status); 
		}
		return CJR_BUSY;
	}

	// Check that the job is still managed by the schedd
	char * managed;
	if( GetAttributeStringNew(cluster, proc, ATTR_JOB_MANAGED, &managed) >= 0) {
		bool ok = strcmp(managed, MANAGED_SCHEDD) == 0;
		free(managed);
		if( ! ok ) {
			if(error_details) {
				formatstr(*error_details, "Job %d.%d is already managed by another process", cluster, proc); 
			}
			return CJR_BUSY;
		}
	}
	// else: No Managed attribute?  Assume it's schedd managed (ok) and carry on.
	
	// No one else has a claim.  Claim it ourselves.
	if( SetAttributeString(cluster, proc, ATTR_JOB_MANAGED, MANAGED_EXTERNAL) == -1 ) {
		if(error_details) {
			formatstr(*error_details, "Encountered problem setting %s = %s", ATTR_JOB_MANAGED, MANAGED_EXTERNAL); 
		}
		return CJR_ERROR;
	}

	if(my_identity) {
		if( SetAttributeString(cluster, proc, ATTR_JOB_MANAGED_MANAGER, my_identity) == -1 ) {
			if(error_details) {
				formatstr(*error_details, "Encountered problem setting %s = %s", ATTR_JOB_MANAGED, MANAGED_EXTERNAL); 
			}
			return CJR_ERROR;
		}
	}

	return CJR_OK;
}

static Qmgr_connection *open_q_as_owner(char const *effective_owner,DCSchedd &schedd,FailObj &failobj)
{
	CondorError errstack;
	Qmgr_connection * qmgr = ConnectQ(schedd.addr(), 0 /*timeout==default*/, false /*read-only*/, & errstack, effective_owner, schedd.version());
	if( ! qmgr ) {
		failobj.fail("Unable to connect\n%s\n", errstack.getFullText(true).c_str());
		return NULL;
	}
	failobj.SetQmgr(qmgr);
	return qmgr;
}

static Qmgr_connection *open_job(ClassAd &job,DCSchedd &schedd,FailObj &failobj)
{
		// connect to the q as the owner of this job
	std::string effective_owner;
	job.LookupString(ATTR_OWNER,effective_owner);

	return open_q_as_owner(effective_owner.c_str(),schedd,failobj);
}

static Qmgr_connection *open_job(classad::ClassAd const &job,DCSchedd &schedd,FailObj &failobj)
{
		// connect to the q as the owner of this job
	std::string effective_owner;
	job.EvaluateAttrString(ATTR_OWNER,effective_owner);

	return open_q_as_owner(effective_owner.c_str(),schedd,failobj);
}

static Qmgr_connection *open_job(classad::ClassAd const &job,const char *schedd_name, const char *pool_name,FailObj &failobj)
{
	failobj.SetNames(schedd_name,pool_name);
	DCSchedd schedd(schedd_name,pool_name);
	if( ! schedd.locate() ) {
		failobj.fail("Can't find address of schedd\n");
		return NULL;
	}

	return open_job(job,schedd,failobj);
}


static ClaimJobResult claim_job_with_current_privs(const char * pool_name, const char * schedd_name, int cluster, int proc, std::string * error_details, const char * my_identity,classad::ClassAd const &job)
{
	// Open a qmgr
	FailObj failobj;
	failobj.SetSaveErrorMsg( error_details );

	Qmgr_connection * qmgr = open_job(job,schedd_name,pool_name,failobj);
	if( !qmgr ) {
		return CJR_ERROR;
	}


	//-------
	// Do the actual claim
	ClaimJobResult res = claim_job(cluster, proc, error_details, my_identity);
	//-------


	// Tear down the qmgr
	failobj.SetQmgr(0);
	if( ! DisconnectQ(qmgr, true /* commit */)) {
		failobj.fail("Failed to commit job claim\n");
		if(error_details && res == CJR_OK) {
			formatstr(*error_details, "Failed to commit job claim for schedd %s in pool %s",
				schedd_name ? schedd_name : "local schedd",
				pool_name ? pool_name : "local pool");
		}
		return CJR_ERROR;
	}

	return res;
}

ClaimJobResult claim_job(classad::ClassAd const &ad, const char * pool_name, const char * schedd_name, int cluster, int proc, std::string * error_details, const char * my_identity, bool target_is_sandboxed)
{
	priv_state priv = set_user_priv_from_ad(ad);

	ClaimJobResult result = claim_job_with_current_privs(pool_name,schedd_name,cluster,proc,error_details,my_identity,ad);

	set_priv(priv);
	uninit_user_ids();

		// chown the src job sandbox to the user if appropriate
	if( result == CJR_OK && !target_is_sandboxed ) {
		if( SpooledJobFiles::jobRequiresSpoolDirectory(&ad) ) {
			if( !SpooledJobFiles::createJobSpoolDirectory(&ad,PRIV_USER) ) {
				if( error_details ) {
					formatstr(*error_details, "Failed to create/chown source job spool directory to the user.");
				}
				yield_job(ad,pool_name,schedd_name,true,cluster,proc,error_details,my_identity,false);
				return CJR_ERROR;
			}
		}
	}

	return result;
}




bool yield_job(bool done, int cluster, int proc, classad::ClassAd const &job_ad, std::string * error_details, const char * my_identity, bool target_is_sandboxed, bool release_on_hold, bool *keep_trying) {
	ASSERT(cluster > 0);
	ASSERT(proc >= 0);

	bool junk_keep_trying;
	if(!keep_trying) keep_trying = &junk_keep_trying;
	*keep_trying = true;

	// Check that the job is still managed by us
	bool is_managed = false;
	char * managed;
	if( GetAttributeStringNew(cluster, proc, ATTR_JOB_MANAGED, &managed) >= 0) {
		is_managed = strcmp(managed, MANAGED_EXTERNAL) == 0;
		free(managed);
	}
	if( ! is_managed ) {
		if(error_details) {
			formatstr(*error_details, "Job %d.%d is not managed!", cluster, proc); 
		}
		*keep_trying = false;
		return false;
	}

	if(my_identity) {
		char * manager;
		if( GetAttributeStringNew(cluster, proc, ATTR_JOB_MANAGED_MANAGER, &manager) >= 0) {
			if(strcmp(manager, my_identity) != 0) {
				if(error_details) {
					formatstr(*error_details, "Job %d.%d is managed by '%s' instead of expected '%s'", cluster, proc, manager, my_identity);
				}
				free(manager);
				*keep_trying = false;
				return false;
			}
			free(manager);
		}
	}

		// chown the src job sandbox back to condor is appropriate
	if( !target_is_sandboxed ) {
		if( SpooledJobFiles::jobRequiresSpoolDirectory(&job_ad) ) {
			SpooledJobFiles::chownSpoolDirectoryToCondor(&job_ad);
		}
	}

	const char * newsetting = done ? MANAGED_DONE : MANAGED_SCHEDD;
	if( SetAttributeString(cluster, proc, ATTR_JOB_MANAGED, newsetting) == -1 ) {
		if(error_details) {
			formatstr(*error_details, "Encountered problem setting %s = %s", ATTR_JOB_MANAGED, newsetting); 
		}
		return false;
	}
		// Wipe the manager (if present).
	SetAttributeString(cluster, proc, ATTR_JOB_MANAGED_MANAGER, "");

	int status;
	if( GetAttributeInt(cluster, proc, ATTR_JOB_STATUS, &status) != -1) {
		if(status == REMOVED) {
			int rc = DestroyProc(cluster, proc);
			if(rc < 0) {
				dprintf(D_ALWAYS,"yield_job: failed to remove %d.%d: rc=%d\n",cluster,proc,rc);
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

	return true;
}




static bool yield_job_with_current_privs(
	const char * pool_name, const char * schedd_name,
	bool done, int cluster, int proc, std::string * error_details,
	const char * my_identity, bool target_is_sandboxed, bool release_on_hold, bool *keep_trying,
	classad::ClassAd const &job)
{
	// Open a qmgr
	FailObj failobj;
	failobj.SetSaveErrorMsg( error_details );

	bool junk_keep_trying;
	if(!keep_trying) keep_trying = &junk_keep_trying;
	*keep_trying = true;

	Qmgr_connection *qmgr = open_job(job,schedd_name,pool_name,failobj);
	if( !qmgr ) {
		return false;
	}

	//-------
	// Do the actual yield
	bool res = yield_job(done, cluster, proc, job, error_details, my_identity, target_is_sandboxed, release_on_hold, keep_trying);
	//-------


	// Tear down the qmgr
	failobj.SetQmgr(0);
	if( ! DisconnectQ(qmgr, true /* commit */)) {
		failobj.fail("Failed to commit job claim\n");
		if(error_details && res) {
			formatstr(*error_details, "Failed to commit job claim for schedd %s in pool %s",
				schedd_name ? schedd_name : "local schedd",
				pool_name ? pool_name : "local pool");
		}
		return false;
	}

	return res;
}


bool yield_job(classad::ClassAd const &ad,const char * pool_name,
	const char * schedd_name, bool done, int cluster, int proc,
	std::string * error_details, const char * my_identity, bool target_is_sandboxed,
        bool release_on_hold, bool *keep_trying)
{
	bool success;
	priv_state priv = set_user_priv_from_ad(ad);

	success = yield_job_with_current_privs(pool_name,schedd_name,done,cluster,proc,error_details,my_identity,target_is_sandboxed,release_on_hold,keep_trying,ad);

	set_priv(priv);
	uninit_user_ids();

	return success;
}


static bool submit_job_with_current_priv( ClassAd & src, const char * schedd_name, const char * pool_name, bool is_sandboxed, int * cluster_out /*= 0*/, int * proc_out /*= 0 */)
{
	FailObj failobj;
	failobj.SetNames(schedd_name, pool_name);

	DCSchedd schedd(schedd_name,pool_name);
	if( ! schedd.locate() ) {
		failobj.fail("Can't find address of schedd\n");
		return false;
	}
	// TODO Consider: condor_submit has to fret about storing a credential on Win32.

	Qmgr_connection * qmgr = open_job(src,schedd,failobj);
	if( !qmgr ) {
		return false;
	}

	// Starting in 8.5.8, schedd clients can't set X509-related attributes
	// other than the name of the proxy file.
	std::set<std::string, classad::CaseIgnLTStr> filter_attrs;
	CondorVersionInfo ver_info( schedd.version() );
	if ( ver_info.built_since_version( 8, 5, 8 ) ) {
		 filter_attrs.insert( ATTR_X509_USER_PROXY_SUBJECT );
		 filter_attrs.insert( ATTR_X509_USER_PROXY_EXPIRATION );
		 filter_attrs.insert( ATTR_X509_USER_PROXY_EMAIL );
		 filter_attrs.insert( ATTR_X509_USER_PROXY_VONAME );
		 filter_attrs.insert( ATTR_X509_USER_PROXY_FIRST_FQAN );
		 filter_attrs.insert( ATTR_X509_USER_PROXY_FQAN );
	}
	filter_attrs.insert( ATTR_TOKEN_SUBJECT );
	filter_attrs.insert( ATTR_TOKEN_ISSUER );
	filter_attrs.insert( ATTR_TOKEN_GROUPS );
	filter_attrs.insert( ATTR_TOKEN_SCOPES );
	filter_attrs.insert( ATTR_TOKEN_ID );

	int cluster = NewCluster();
	if( cluster < 0 ) {
		failobj.fail("Failed to create a new cluster (%d)\n", cluster);
		return false;
	}
	failobj.SetCluster(cluster);

	int proc = NewProc(cluster);
	if( proc < 0 ) {
		failobj.fail("Failed to create a new proc (%d)\n", proc);
		return false;
	}
	failobj.SetProc(proc);

	src.Assign(ATTR_PROC_ID, proc);
	src.Assign(ATTR_CLUSTER_ID, cluster);
	src.Assign(ATTR_Q_DATE, (int)time(0));
	src.Assign(ATTR_ENTERED_CURRENT_STATUS, (int)time(0));

	// Things to set because we want to spool/sandbox input files
	if( is_sandboxed ) {
		// we need to submit on hold (taken from condor_submit.V6/submit.C)
		src.Assign(ATTR_JOB_STATUS, 5); // 5==HELD
		src.Assign(ATTR_HOLD_REASON, "Spooling input data files");
		src.Assign(ATTR_HOLD_REASON_CODE, CONDOR_HOLD_CODE_SpoolingInput);

			// See the comment in the function body of ExpandInputFileList
			// for an explanation of what is going on here.
		std::string transfer_input_error_msg;
		if( !FileTransfer::ExpandInputFileList( &src, transfer_input_error_msg ) ) {
			failobj.fail("%s\n",transfer_input_error_msg.c_str());
			return false;
		}
	}

		// we want the job to hang around (taken from condor_submit.V6/submit.C)
	std::string leaveinqueue;
	formatstr(leaveinqueue, "%s == %d", ATTR_JOB_STATUS, COMPLETED);
	src.AssignExpr(ATTR_JOB_LEAVE_IN_QUEUE, leaveinqueue.c_str());

	ExprTree * tree;
	const char *lhstr = 0;
	const char *rhstr = 0;
	for( auto itr = src.begin(); itr != src.end(); itr++ ) {
		lhstr = itr->first.c_str();
		tree = itr->second;
		if ( filter_attrs.find( lhstr ) != filter_attrs.end() ) {
			continue;
		}
		rhstr = ExprTreeToString( tree );
		if( !rhstr) { 
			failobj.fail("Problem processing classad\n");
			return false;
		}
		if( SetAttribute(cluster, proc, lhstr, rhstr) == -1 ) {
			failobj.fail("Failed to set %s = %s\n", lhstr, rhstr);
			return false;
		}
	}

	failobj.SetQmgr(0);
	if( ! DisconnectQ(qmgr, true /* commit */)) {
		failobj.fail("Failed to commit job submission\n");
		return false;
	}

	if( is_sandboxed ) {
		CondorError errstack;
		ClassAd * adlist[1];
		adlist[0] = &src;
		if( ! schedd.spoolJobFiles(1, adlist, &errstack) ) {
			failobj.fail("Failed to spool job files: %s\n",errstack.getFullText(true).c_str());
			return false;
		}
	}

	schedd.reschedule();

	if(cluster_out) { *cluster_out = cluster; }
	if(proc_out) { *proc_out = proc; }

	return true;
}

bool submit_job(const std::string & owner, const std::string &domain, ClassAd & src, const char * schedd_name, const char * pool_name, bool is_sandboxed, int * cluster_out /*= 0*/, int * proc_out /*= 0 */)
{
	bool success;

	if (!init_user_ids(owner.c_str(), domain.c_str()))
	{
		dprintf(D_ALWAYS, "Failed in init_user_ids(%s,%s)\n",
			owner.c_str(),
			domain.c_str());
		return false;
	}
	TemporaryPrivSentry sentry(PRIV_USER);

	success = submit_job_with_current_priv(src,schedd_name,pool_name,is_sandboxed,cluster_out,proc_out);

	uninit_user_ids();

	return success;
}

/*
	Push the dirty attributes in src into the queue.  Does _not_ clear
	the dirty attributes.
	Assumes the existance of an open qmgr connection (via ConnectQ).
*/
bool push_dirty_attributes(classad::ClassAd & src)
{
	int cluster;
	if( ! src.EvaluateAttrInt(ATTR_CLUSTER_ID, cluster) ) {
		dprintf(D_ALWAYS, "push_dirty_attributes: job lacks a cluster\n");
		return false;
	}
	int proc;
	if( ! src.EvaluateAttrInt(ATTR_PROC_ID, proc) ) {
		dprintf(D_ALWAYS, "push_dirty_attributes: job lacks a proc\n");
		return false;
	}
	const char *rhstr = 0;
	ExprTree * tree;
	for( classad::ClassAd::dirtyIterator it = src.dirtyBegin();
		 it != src.dirtyEnd(); ++it) {

		rhstr = NULL;
		tree = src.Lookup( *it );
		if ( tree ) {
			rhstr = ExprTreeToString( tree );
		}
		if( !rhstr) { 
			dprintf(D_ALWAYS,"(%d.%d) push_dirty_attributes: Problem processing classad\n", cluster, proc);
			return false;
		}
		dprintf(D_FULLDEBUG, "Setting %s = %s\n", it->c_str(), rhstr);
		if( SetAttribute(cluster, proc, it->c_str(), rhstr) == -1 ) {
			dprintf(D_ALWAYS,"(%d.%d) push_dirty_attributes: Failed to set %s = %s\n", cluster, proc, it->c_str(), rhstr);
			return false;
		}
	}
	return true;
}

static bool push_dirty_attributes_with_current_priv(classad::ClassAd & src, const char * schedd_name, const char * pool_name)
{
	FailObj failobj;
	Qmgr_connection *qmgr = open_job(src,schedd_name,pool_name,failobj);
	if( !qmgr ) {
		return false;
	}

	bool ret = push_dirty_attributes(src);

	failobj.SetQmgr(0);
	if( ! DisconnectQ(qmgr, ret /* commit */)) {
		dprintf(D_ALWAYS, "push_dirty_attributes: Failed to commit changes\n");
		return false;
	}

	return ret;
}

/*
	Push the dirty attributes in src into the queue.  Does _not_ clear
	the dirty attributes.
	Establishes (and tears down) a qmgr connection.
*/
bool push_dirty_attributes(classad::ClassAd & src, const char * schedd_name, const char * pool_name)
{
	bool success;
	priv_state priv = set_user_priv_from_ad(src);

	success = push_dirty_attributes_with_current_priv(src,schedd_name,pool_name);

	set_priv(priv);
	uninit_user_ids();
	return success;
}

/*
	Update src in the queue so that it ends up looking like dest.
    This handles attribute deletion as well as change of value.
	Assumes the existance of an open qmgr connection (via ConnectQ).
*/
bool push_classad_diff(classad::ClassAd & src,classad::ClassAd & dest)
{
	int cluster;
	if( ! src.EvaluateAttrInt(ATTR_CLUSTER_ID, cluster) ) {
		dprintf(D_ALWAYS, "push_classad_diff: job lacks a cluster\n");
		return false;
	}
	int proc;
	if( ! src.EvaluateAttrInt(ATTR_PROC_ID, proc) ) {
		dprintf(D_ALWAYS, "push_classad_diff: job lacks a proc\n");
		return false;
	}

	classad::References attrs;

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
				dprintf(D_ALWAYS,"(%d.%d) push_classad_diff: Problem processing classad attribute %s\n", cluster, proc, attr);
				return false;
			}
			src_rhstr = rhstr;
		}

		dest_tree = dest.Lookup( attr );
		if ( dest_tree ) {
			char const *rhstr = ExprTreeToString( dest_tree );
			if( !rhstr) { 
				dprintf(D_ALWAYS,"(%d.%d) push_classad_diff: Problem processing classad attribute %s\n", cluster, proc, attr);
				return false;
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
			dprintf(D_ALWAYS,"(%d.%d) push_classad_diff: Failed to set %s = %s\n", cluster, proc, attr, dest_rhstr.c_str());
			return false;
		}
	}
	return true;
}

static bool push_classad_diff_with_current_priv(classad::ClassAd & src, classad::ClassAd & dest, const char * schedd_name, const char * pool_name)
{
	FailObj failobj;
	Qmgr_connection *qmgr = open_job(src,schedd_name,pool_name,failobj);
	if( !qmgr ) {
		return false;
	}

	bool ret = push_classad_diff(src,dest);

	failobj.SetQmgr(0);
	if( ! DisconnectQ(qmgr, ret /* commit */)) {
		dprintf(D_ALWAYS, "push_dirty_attributes: Failed to commit changes\n");
		return false;
	}

	return ret;
}

/*
	Update src in the queue so that it ends up looking like dest.
    This handles attribute deletion as well as change of value.
	Establishes (and tears down) a qmgr connection.
*/
bool push_classad_diff(classad::ClassAd & src, classad::ClassAd & dest, const char * schedd_name, const char * pool_name)
{
	bool success;
	priv_state priv = set_user_priv_from_ad(src);

	success = push_classad_diff_with_current_priv(src,dest,schedd_name,pool_name);

	set_priv(priv);
	uninit_user_ids();
	return success;
}

static bool finalize_job_with_current_privs(classad::ClassAd const &job,int cluster, int proc, const char * schedd_name, const char * pool_name, bool is_sandboxed)
{
	FailObj failobj;
	failobj.SetNames(schedd_name,pool_name);

	DCSchedd schedd(schedd_name,pool_name);
	if( ! schedd.locate() ) {
		if(!schedd_name) { schedd_name = "local schedd"; }
		if(!pool_name) { pool_name = "local pool"; }
		dprintf(D_ALWAYS, "Unable to find address of %s at %s\n", schedd_name, pool_name);
		return false;
	}

	std::string constraint;
	formatstr(constraint, "(ClusterId==%d&&ProcId==%d)", cluster, proc);


	if( is_sandboxed ) {
			// Get our sandbox back
		int jobssent;
		CondorError errstack;
		bool success = schedd.receiveJobSandbox(constraint.c_str(), &errstack, &jobssent);
		if( ! success ) {
			dprintf(D_ALWAYS, "(%d.%d) Failed to retrieve sandbox.\n", cluster, proc);
			return false;
		}

		if(jobssent != 1) {
			dprintf(D_ALWAYS, "(%d.%d) Failed to retrieve sandbox (got %d, expected 1).\n", cluster, proc, jobssent);
			return false;
		}
	}

	// Yield the job (clear MANAGED).
	Qmgr_connection * qmgr = open_job(job,schedd,failobj);
	if(!qmgr) {
		return false;
	}

	if( SetAttribute(cluster, proc, ATTR_JOB_LEAVE_IN_QUEUE, "FALSE") == -1 ) {
		dprintf(D_ALWAYS, "finalize_job: failed to set %s = %s for %d.%d\n", ATTR_JOB_LEAVE_IN_QUEUE, "FALSE", cluster, proc);
	}

	failobj.SetQmgr(0);
	if( ! DisconnectQ(qmgr, true /* commit */)) {
		dprintf(D_ALWAYS, "finalize_job: Failed to commit changes\n");
		return false;
	}


	return true;
}

bool finalize_job(const std::string & owner, const std::string &domain, classad::ClassAd const &ad,int cluster, int proc, const char * schedd_name, const char * pool_name, bool is_sandboxed)
{
	bool success;

	if (!init_user_ids(owner.c_str(), domain.c_str()))
	{
		dprintf(D_ALWAYS, "Failed in init_user_ids(%s,%s)\n",
			owner.c_str(),
			domain.c_str());
		return false;
	}
	TemporaryPrivSentry sentry(PRIV_USER);

	success = finalize_job_with_current_privs(ad,cluster,proc,schedd_name,pool_name,is_sandboxed);

	uninit_user_ids();
	return success;
}

static bool remove_job_with_current_privs(int cluster, int proc, char const *reason, const char * schedd_name, const char * pool_name, std::string &error_desc)
{
	DCSchedd schedd(schedd_name,pool_name);
	bool success = true;
	CondorError errstack;

	if( ! schedd.locate() ) {
		if(!schedd_name) { schedd_name = "local schedd"; }
		if(!pool_name) { pool_name = "local pool"; }
		dprintf(D_ALWAYS, "Unable to find address of %s at %s\n", schedd_name, pool_name);
		formatstr(error_desc, "Unable to find address of %s at %s", schedd_name, pool_name);
		return false;
	}

	std::string id_str;
	formatstr(id_str, "%d.%d", cluster, proc);
	StringList job_ids(id_str.c_str());
	ClassAd *result_ad;

	result_ad = schedd.removeJobs(&job_ids, reason, &errstack, AR_LONG);

	PROC_ID job_id;
	job_id.cluster = cluster;
	job_id.proc = proc;

	char *err_str = NULL;
	if(result_ad) {
		JobActionResults results(AR_LONG);
		results.readResults(result_ad);
		success = results.getResultString(job_id,&err_str);
	}
	else {
		error_desc = "No result ClassAd returned by schedd.removeJobs()";
		success = false;
	}

	if(!success && err_str) {
		error_desc = err_str;
	}
	if(err_str) free(err_str);
	if(result_ad) delete result_ad;

	return success;
}

bool remove_job(classad::ClassAd const &ad, int cluster, int proc, char const *reason, const char * schedd_name, const char * pool_name, std::string &error_desc)
{
	bool success;
	priv_state priv = set_user_priv_from_ad(ad);

	success = remove_job_with_current_privs(cluster,proc,reason,schedd_name,pool_name,error_desc);

	set_priv(priv);
	uninit_user_ids();
	return success;
}

bool InitializeAbortedEvent( JobAbortedEvent *event, classad::ClassAd const &job_ad )
{
	int cluster, proc;
	char removeReason[256];

		// This code is copied from gridmanager basejob.C with a small
		// amount of refactoring.
		// TODO: put this code in a common place.

	job_ad.EvaluateAttrInt( ATTR_CLUSTER_ID, cluster );
	job_ad.EvaluateAttrInt( ATTR_PROC_ID, proc );

	dprintf( D_FULLDEBUG, 
			 "(%d.%d) Writing abort record to user logfile\n",
			 cluster, proc );

	removeReason[0] = '\0';
	job_ad.EvaluateAttrString( ATTR_REMOVE_REASON, removeReason,
						   sizeof(removeReason) - 1 );

	event->setReason( removeReason );
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

	return true;
}

bool InitializeHoldEvent( JobHeldEvent *event, classad::ClassAd const &job_ad )
{
	int cluster, proc;
	char holdReason[256];

		// This code is copied from gridmanager basejob.C with a small
		// amount of refactoring.
		// TODO: put this code in a common place.

	job_ad.EvaluateAttrInt( ATTR_CLUSTER_ID, cluster );
	job_ad.EvaluateAttrInt( ATTR_PROC_ID, proc );

	dprintf( D_FULLDEBUG, 
			 "(%d.%d) Writing hold record to user logfile\n",
			 cluster, proc );

	holdReason[0] = '\0';
	job_ad.EvaluateAttrString( ATTR_REMOVE_REASON, holdReason,
						   sizeof(holdReason) - 1 );

	event->setReason( holdReason );
	return true;
}

bool WriteEventToUserLog( ULogEvent const &event, classad::ClassAd const &ad )
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

	int rc = ulog.writeEvent(const_cast<ULogEvent *>(&event));

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
