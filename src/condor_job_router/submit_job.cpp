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
#include "classad_newold.h"
#include "condor_uid.h"
#include "condor_event.h"
#include "write_user_log.h"
#include "condor_email.h"
#include "condor_arglist.h"
#include "format_time.h"
#include "set_user_priv_from_ad.h"
#define WANT_CLASSAD_NAMESPACE
#undef open
#include "classad/classad_distribution.h"
#include "set_user_from_ad.h"

	// Simplify my error handling and reporting code
class FailObj {
public:
	FailObj() : cluster(-1), proc(-1), qmgr(0), save_error_msg(0) { }

	void SetCluster(int c) { cluster = c; }
	void SetProc(int p) { proc = p; }

	void SetSaveErrorMsg(MyString *s) { save_error_msg = s; }

	void SetNames(const char * schedd_name, const char * pool_name) {
		if(schedd_name) {
			names = "schedd ";
			names += schedd_name;
		} else {
			names = "local schedd";
		}
		names += " at ";
		if(pool_name) {
			names = "pool ";
			names += pool_name;
		} else {
			names = "local pool";
		}
	}

	void SetQmgr(Qmgr_connection * q) { qmgr = q; }

	void fail( const char * fmt, ...) {
		if(cluster != -1 && qmgr) {
			DestroyCluster(cluster);
		}
		if(qmgr) { DisconnectQ( qmgr, false /* don't commit */); }

		MyString msg;
		msg = "ERROR ";
		if(names.Length()) {
			msg += "(";
			msg += names;
			msg += ") ";
		}

		if(cluster != -1) {
			msg += "(";
			msg += cluster;
			if(proc != -1) {
				msg += ".";
				msg += proc;
			}
			msg += ") ";
		}
		va_list args;
		va_start(args,fmt);
		msg.vsprintf_cat(fmt,args);
		va_end(args);


		if( save_error_msg ) {
			*save_error_msg = msg.Value();
		}
		else {
			dprintf(D_ALWAYS, "%s", msg.Value());
		}
	}
private:
	MyString names;
	int cluster;
	int proc;
	Qmgr_connection * qmgr;
	MyString *save_error_msg;
};


static priv_state set_user_priv_from_ad(classad::ClassAd const &ad)
{
	set_user_from_ad(ad);
	return set_user_priv();
}


ClaimJobResult claim_job(int cluster, int proc, MyString * error_details, const char * my_identity)
{
	ASSERT(cluster > 0);
	ASSERT(proc >= 0);

	// Check that the job's status is still IDLE
	int status;
	if( GetAttributeInt(cluster, proc, ATTR_JOB_STATUS, &status) == -1) {
		if(error_details) {
			error_details->sprintf("Encountered problem reading current %s for %d.%d", ATTR_JOB_STATUS, cluster, proc); 
		}
		return CJR_ERROR;
	}
	if(status != IDLE) {
		if(error_details) {
			error_details->sprintf("Job %d.%d isn't idle, is %s (%d)", cluster, proc, getJobStatusString(status), status); 
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
				error_details->sprintf("Job %d.%d is already managed by another process", cluster, proc); 
			}
			return CJR_BUSY;
		}
	}
	// else: No Managed attribute?  Assume it's schedd managed (ok) and carry on.
	
	// No one else has a claim.  Claim it ourselves.
	if( SetAttributeString(cluster, proc, ATTR_JOB_MANAGED, MANAGED_EXTERNAL) == -1 ) {
		if(error_details) {
			error_details->sprintf("Encountered problem setting %s = %s", ATTR_JOB_MANAGED, MANAGED_EXTERNAL); 
		}
		return CJR_ERROR;
	}

	if(my_identity) {
		if( SetAttributeString(cluster, proc, ATTR_JOB_MANAGED_MANAGER, my_identity) == -1 ) {
			if(error_details) {
				error_details->sprintf("Encountered problem setting %s = %s", ATTR_JOB_MANAGED, MANAGED_EXTERNAL); 
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
		failobj.fail("Unable to connect\n%s\n", errstack.getFullText(true));
		return NULL;
	}
	failobj.SetQmgr(qmgr);
	return qmgr;
}

static Qmgr_connection *open_job(ClassAd &job,DCSchedd &schedd,FailObj &failobj)
{
		// connect to the q as the owner of this job
	MyString effective_owner;
	job.LookupString(ATTR_OWNER,effective_owner);

	return open_q_as_owner(effective_owner.Value(),schedd,failobj);
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


static ClaimJobResult claim_job_with_current_privs(const char * pool_name, const char * schedd_name, int cluster, int proc, MyString * error_details, const char * my_identity,classad::ClassAd const &job)
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
			error_details->sprintf("Failed to commit job claim for schedd %s in pool %s",
				schedd_name ? schedd_name : "local schedd",
				pool_name ? pool_name : "local pool");
		}
		return CJR_ERROR;
	}

	return res;
}

ClaimJobResult claim_job(classad::ClassAd const &ad, const char * pool_name, const char * schedd_name, int cluster, int proc, MyString * error_details, const char * my_identity)
{
	priv_state priv = set_user_priv_from_ad(ad);

	ClaimJobResult result = claim_job_with_current_privs(pool_name,schedd_name,cluster,proc,error_details,my_identity,ad);

	set_priv(priv);
	return result;
}




bool yield_job(bool done, int cluster, int proc, MyString * error_details, const char * my_identity, bool release_on_hold, bool *keep_trying) {
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
			error_details->sprintf("Job %d.%d is not managed!", cluster, proc); 
		}
		*keep_trying = false;
		return false;
	}

	if(my_identity) {
		char * manager;
		if( GetAttributeStringNew(cluster, proc, ATTR_JOB_MANAGED_MANAGER, &manager) >= 0) {
			if(strcmp(manager, my_identity) != 0) {
				if(error_details) {
					error_details->sprintf("Job %d.%d is managed by '%s' instead of expected '%s'", cluster, proc, manager, my_identity);
				}
				free(manager);
				*keep_trying = false;
				return false;
			}
			free(manager);
		}
	}

	const char * newsetting = done ? MANAGED_DONE : MANAGED_SCHEDD;
	if( SetAttributeString(cluster, proc, ATTR_JOB_MANAGED, newsetting) == -1 ) {
		if(error_details) {
			error_details->sprintf("Encountered problem setting %s = %s", ATTR_JOB_MANAGED, newsetting); 
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
	bool done, int cluster, int proc, MyString * error_details,
	const char * my_identity, bool release_on_hold, bool *keep_trying,
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
	bool res = yield_job(done, cluster, proc, error_details, my_identity, release_on_hold, keep_trying);
	//-------


	// Tear down the qmgr
	failobj.SetQmgr(0);
	if( ! DisconnectQ(qmgr, true /* commit */)) {
		failobj.fail("Failed to commit job claim\n");
		if(error_details && res) {
			error_details->sprintf("Failed to commit job claim for schedd %s in pool %s",
				schedd_name ? schedd_name : "local schedd",
				pool_name ? pool_name : "local pool");
		}
		return false;
	}

	return res;
}


bool yield_job(classad::ClassAd const &ad,const char * pool_name,
	const char * schedd_name, bool done, int cluster, int proc,
	MyString * error_details, const char * my_identity, 
        bool release_on_hold, bool *keep_trying)
{
	bool success;
	priv_state priv = set_user_priv_from_ad(ad);

	success = yield_job_with_current_privs(pool_name,schedd_name,done,cluster,proc,error_details,my_identity,release_on_hold,keep_trying,ad);

	set_priv(priv);

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

	}

		// we want the job to hang around (taken from condor_submit.V6/submit.C)
	MyString leaveinqueue;
	leaveinqueue.sprintf("%s == %d", ATTR_JOB_STATUS, COMPLETED);
	src.AssignExpr(ATTR_JOB_LEAVE_IN_QUEUE, leaveinqueue.Value());

	ExprTree * tree;
	const char *lhstr = 0;
	const char *rhstr = 0;
	src.ResetExpr();
	while( src.NextExpr(lhstr, tree) ) {
		rhstr = ExprTreeToString( tree );
		if( !lhstr || !rhstr) { 
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
			failobj.fail("Failed to spool job files: %s\n",errstack.getFullText(true));
			return false;
		}
	}

	if(cluster_out) { *cluster_out = cluster; }
	if(proc_out) { *proc_out = proc; }

	return true;
}

bool submit_job( ClassAd & src, const char * schedd_name, const char * pool_name, bool is_sandboxed, int * cluster_out /*= 0*/, int * proc_out /*= 0 */)
{
	bool success;
	priv_state priv = set_user_priv_from_ad(src);

	success = submit_job_with_current_priv(src,schedd_name,pool_name,is_sandboxed,cluster_out,proc_out);

	set_priv(priv);

	return success;
}

bool submit_job( classad::ClassAd & src, const char * schedd_name, const char * pool_name, bool is_sandboxed, int * cluster_out /*= 0*/, int * proc_out /*= 0 */)
{
	ClassAd src2;
	if( ! new_to_old(src, src2) ) {
		dprintf(D_ALWAYS, "submit_job failed to convert job ClassAd from new to old form\n");
		return false;
	}
	return submit_job(src2, schedd_name, pool_name, is_sandboxed, cluster_out, proc_out);
}

/*
	Push the dirty attributes in src into the queue.  Does _not_ clear
	the dirty attributes (use ClassAd::ClearAllDirtyFlags() if you want).
	Assumes the existance of an open qmgr connection (via ConnectQ).
*/
bool push_dirty_attributes(int cluster, int proc, ClassAd & src)
{
	src.ResetExpr();
	const char *lhstr = 0;
	const char *rhstr = 0;
	ExprTree * tree;
	while( src.NextDirtyExpr(lhstr, tree) ) {
		rhstr = ExprTreeToString( tree );
		if( !lhstr || !rhstr) { 
			dprintf(D_ALWAYS,"(%d.%d) push_dirty_attributes: Problem processing classad\n", cluster, proc);
			return false;
		}
		dprintf(D_FULLDEBUG, "Setting %s = %s\n", lhstr, rhstr);
		if( SetAttribute(cluster, proc, lhstr, rhstr) == -1 ) {
			dprintf(D_ALWAYS,"(%d.%d) push_dirty_attributes: Failed to set %s = %s\n", cluster, proc, lhstr, rhstr);
			return false;
		}
	}
	return true;
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
	ClassAd src2;
	if( ! new_to_old(src, src2) ) {
		dprintf(D_ALWAYS, "push_dirty_attributes failed to convert job ClassAd from new to old form\n");
		return false;
	}
	return push_dirty_attributes(cluster, proc, src2);
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

	MyString constraint;
	constraint.sprintf("(ClusterId==%d&&ProcId==%d)", cluster, proc);


	if( is_sandboxed ) {
			// Get our sandbox back
		int jobssent;
		CondorError errstack;
		bool success = schedd.receiveJobSandbox(constraint.Value(), &errstack, &jobssent);
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

bool finalize_job(classad::ClassAd const &ad,int cluster, int proc, const char * schedd_name, const char * pool_name, bool is_sandboxed)
{
	bool success;
	priv_state priv = set_user_priv_from_ad(ad);

	success = finalize_job_with_current_privs(ad,cluster,proc,schedd_name,pool_name,is_sandboxed);

	set_priv(priv);
	return success;
}

static bool remove_job_with_current_privs(int cluster, int proc, char const *reason, const char * schedd_name, const char * pool_name, MyString &error_desc)
{
	DCSchedd schedd(schedd_name,pool_name);
	bool success = true;
	CondorError errstack;

	if( ! schedd.locate() ) {
		if(!schedd_name) { schedd_name = "local schedd"; }
		if(!pool_name) { pool_name = "local pool"; }
		dprintf(D_ALWAYS, "Unable to find address of %s at %s\n", schedd_name, pool_name);
		error_desc.sprintf("Unable to find address of %s at %s", schedd_name, pool_name);
		return false;
	}

	MyString constraint;
	constraint.sprintf("(ClusterId==%d&&ProcId==%d)", cluster, proc);
	ClassAd *result_ad;

	result_ad = schedd.removeJobs(constraint.Value(), reason, &errstack, AR_LONG);

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

bool remove_job(classad::ClassAd const &ad, int cluster, int proc, char const *reason, const char * schedd_name, const char * pool_name, MyString &error_desc)
{
	bool success;
	priv_state priv = set_user_priv_from_ad(ad);

	success = remove_job_with_current_privs(cluster,proc,reason,schedd_name,pool_name,error_desc);

	set_priv(priv);
	return success;
}

bool InitializeUserLog( classad::ClassAd const &job_ad, UserLog *ulog, bool *no_ulog )
{
	int cluster, proc;
	std::string owner;
	std::string userLogFile;
	std::string domain;
	std::string gjid;
	bool use_xml = false;

	ASSERT(ulog);
	ASSERT(no_ulog);

	userLogFile[0] = '\0';
	job_ad.EvaluateAttrString( ATTR_ULOG_FILE, userLogFile );
	if ( userLogFile[0] == '\0' ) {
		// User doesn't want a log
		*no_ulog = true;
		return true;
	}
	*no_ulog = false;

	job_ad.EvaluateAttrString(ATTR_OWNER,owner);
	job_ad.EvaluateAttrInt( ATTR_CLUSTER_ID, cluster );
	job_ad.EvaluateAttrInt( ATTR_PROC_ID, proc );
	job_ad.EvaluateAttrString( ATTR_NT_DOMAIN, domain );
	job_ad.EvaluateAttrBool( ATTR_ULOG_USE_XML, use_xml );
	job_ad.EvaluateAttrString( ATTR_GLOBAL_JOB_ID, gjid );

	if(!ulog->initialize(owner.c_str(), domain.c_str(), userLogFile.c_str(), cluster, proc, 0, gjid.c_str())) {
		return false;
	}
	ulog->setUseXML( use_xml );
	return true;
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
		event->run_remote_rusage.ru_utime.tv_sec = (int)real_val;
		event->total_remote_rusage.ru_utime.tv_sec = (int)real_val;
	}
	if ( job_ad.EvaluateAttrReal( ATTR_JOB_REMOTE_SYS_CPU, real_val ) ) {
		event->run_remote_rusage.ru_stime.tv_sec = (int)real_val;
		event->total_remote_rusage.ru_stime.tv_sec = (int)real_val;
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
	UserLog ulog;
	bool no_ulog = false;

	if(!InitializeUserLog(ad,&ulog,&no_ulog)) {
		dprintf( D_FULLDEBUG,
				 "(%d.%d) Unable to open user log (event %d)\n",
				 event.cluster, event.proc, event.eventNumber );
		return false;
	}
	if(no_ulog) {
		return true;
	}

	int rc = ulog.writeEvent((ULogEvent *)&event);

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



// The following is copied from gridmanager/basejob.C
// TODO: put the code into a shared file.

void
EmailTerminateEvent(ClassAd * job_ad, bool exit_status_known)
{
	if ( !job_ad ) {
		dprintf(D_ALWAYS, 
				"email_terminate_event called with invalid ClassAd\n");
		return;
	}

	int cluster, proc;
	job_ad->LookupInteger( ATTR_CLUSTER_ID, cluster );
	job_ad->LookupInteger( ATTR_PROC_ID, proc );

	int notification = NOTIFY_COMPLETE; // default
	job_ad->LookupInteger(ATTR_JOB_NOTIFICATION,notification);

	switch( notification ) {
		case NOTIFY_NEVER:    return;
		case NOTIFY_ALWAYS:   break;
		case NOTIFY_COMPLETE: break;
		case NOTIFY_ERROR:    return;
		default:
			dprintf(D_ALWAYS, 
				"Condor Job %d.%d has unrecognized notification of %d\n",
				cluster, proc, notification );
				// When in doubt, better send it anyway...
			break;
	}

	char subjectline[50];
	sprintf( subjectline, "Condor Job %d.%d", cluster, proc );
	FILE * mailer =  email_user_open( job_ad, subjectline );

	if( ! mailer ) {
		// Is message redundant?  Check email_user_open and euo's children.
		dprintf(D_ALWAYS, 
			"email_terminate_event failed to open a pipe to a mail program.\n");
		return;
	}

		// gather all the info out of the job ad which we want to 
		// put into the email message.
	MyString JobName;
	job_ad->LookupString( ATTR_JOB_CMD, JobName );

	MyString Args;
	ArgList::GetArgsStringForDisplay(job_ad,&Args);
	
	/*
	// Not present.  Probably doesn't make sense for Globus
	int had_core = FALSE;
	job_ad->LookupBool( ATTR_JOB_CORE_DUMPED, had_core );
	*/

	int q_date = 0;
	job_ad->LookupInteger(ATTR_Q_DATE,q_date);
	
	float remote_sys_cpu = 0.0;
	job_ad->LookupFloat(ATTR_JOB_REMOTE_SYS_CPU, remote_sys_cpu);
	
	float remote_user_cpu = 0.0;
	job_ad->LookupFloat(ATTR_JOB_REMOTE_USER_CPU, remote_user_cpu);
	
	int image_size = 0;
	job_ad->LookupInteger(ATTR_IMAGE_SIZE, image_size);
	
	/*
	int shadow_bday = 0;
	job_ad->LookupInteger( ATTR_SHADOW_BIRTHDATE, shadow_bday );
	*/
	
	float previous_runs = 0;
	job_ad->LookupFloat( ATTR_JOB_REMOTE_WALL_CLOCK, previous_runs );
	
	time_t arch_time=0;	/* time_t is 8 bytes some archs and 4 bytes on other
						   archs, and this means that doing a (time_t*)
						   cast on & of a 4 byte int makes my life hell.
						   So we fix it by assigning the time we want to
						   a real time_t variable, then using ctime()
						   to convert it to a string */
	
	time_t now = time(NULL);

	fprintf( mailer, "Your Condor job %d.%d \n", cluster, proc);
	if ( JobName.Length() ) {
		fprintf(mailer,"\t%s %s\n",JobName.Value(),Args.Value());
	}
	if(exit_status_known) {
		fprintf(mailer, "has ");

		int int_val;
		if( job_ad->LookupBool(ATTR_ON_EXIT_BY_SIGNAL, int_val) ) {
			if( int_val ) {
				if( job_ad->LookupInteger(ATTR_ON_EXIT_SIGNAL, int_val) ) {
					fprintf(mailer, "exited with the signal %d.\n", int_val);
				} else {
					fprintf(mailer, "exited with an unknown signal.\n");
					dprintf( D_ALWAYS, "(%d.%d) Job ad lacks %s.  "
						 "Signal code unknown.\n", cluster, proc, 
						 ATTR_ON_EXIT_SIGNAL);
				}
			} else {
				if( job_ad->LookupInteger(ATTR_ON_EXIT_CODE, int_val) ) {
					fprintf(mailer, "exited normally with status %d.\n",
						int_val);
				} else {
					fprintf(mailer, "exited normally with unknown status.\n");
					dprintf( D_ALWAYS, "(%d.%d) Job ad lacks %s.  "
						 "Return code unknown.\n", cluster, proc, 
						 ATTR_ON_EXIT_CODE);
				}
			}
		} else {
			fprintf(mailer,"has exited.\n");
			dprintf( D_ALWAYS, "(%d.%d) Job ad lacks %s.  ",
				 cluster, proc, ATTR_ON_EXIT_BY_SIGNAL);
		}
	} else {
		fprintf(mailer,"has exited.\n");
	}

	/*
	if( had_core ) {
		fprintf( mailer, "Core file is: %s\n", getCoreName() );
	}
	*/

	arch_time = q_date;
	fprintf(mailer, "\n\nSubmitted at:        %s", ctime(&arch_time));
	
	double real_time = now - q_date;
	arch_time = now;
	fprintf(mailer, "Completed at:        %s", ctime(&arch_time));
	
	fprintf(mailer, "Real Time:           %s\n", 
			format_time((int)real_time));


	fprintf( mailer, "\n" );
	
		// TODO We don't necessarily have this information even if we do
		//   have the exit status
	if( exit_status_known ) {
		fprintf(mailer, "Virtual Image Size:  %d Kilobytes\n\n", image_size);
	}

	double rutime = remote_user_cpu;
	double rstime = remote_sys_cpu;
	double trtime = rutime + rstime;
	/*
	double wall_time = now - shadow_bday;
	fprintf(mailer, "Statistics from last run:\n");
	fprintf(mailer, "Allocation/Run time:     %s\n",format_time(wall_time) );
	*/
		// TODO We don't necessarily have this information even if we do
		//   have the exit status
	if( exit_status_known ) {
		fprintf(mailer, "Remote User CPU Time:    %s\n", format_time((int)rutime) );
		fprintf(mailer, "Remote System CPU Time:  %s\n", format_time((int)rstime) );
		fprintf(mailer, "Total Remote CPU Time:   %s\n\n", format_time((int)trtime));
	}

	/*
	double total_wall_time = previous_runs + wall_time;
	fprintf(mailer, "Statistics totaled from all runs:\n");
	fprintf(mailer, "Allocation/Run time:     %s\n",
			format_time(total_wall_time) );

	// TODO: Can we/should we get this for Globus jobs.
		// TODO: deal w/ total bytes <- obsolete? in original code)
	float network_bytes;
	network_bytes = bytesSent();
	fprintf(mailer, "\nNetwork:\n" );
	fprintf(mailer, "%10s Run Bytes Received By Job\n", 
			metric_units(network_bytes) );
	network_bytes = bytesReceived();
	fprintf(mailer, "%10s Run Bytes Sent By Job\n",
			metric_units(network_bytes) );
	*/

	email_close(mailer);
}

bool EmailTerminateEvent( classad::ClassAd const &ad )
{
	ClassAd old_ad;
	classad::ClassAd ad_copy = ad;
	if(!new_to_old(ad_copy,old_ad)) {
		dprintf(D_ALWAYS, "EmailTerminateEvent failed to convert job ClassAd from new to old form\n");
		return false;
	}

	EmailTerminateEvent( &old_ad, true );
	return true;
}
