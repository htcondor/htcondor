#include "condor_common.h"
#include "submit_job.h"
#include "condor_classad.h"
#include "condor_qmgr.h"
#include "dc_schedd.h"
#include "condor_ver_info.h"
#include "condor_attributes.h"
#include "proc.h"
#include "classad_newold.h"
#define WANT_NAMESPACES
#include "classad_distribution.h"
#include "condor_uid.h"

	// Simplify my error handling and reporting code
class FailObj {
public:
	FailObj() : cluster(-1), proc(-1), qmgr(0) { }

	void SetCluster(int c) { cluster = c; }
	void SetProc(int p) { proc = p; }

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
		dprintf(D_ALWAYS, "%s", msg.Value());
	}
private:
	MyString names;
	int cluster;
	int proc;
	Qmgr_connection * qmgr;
};


static priv_state set_user_priv_from_ad(ClassAd const &ad)
{
	char *owner = NULL;
	char *domain = NULL;
	if( !ad.LookupString(ATTR_OWNER,&owner) ) {
		EXCEPT("Failed to find %s in job ad.\n",ATTR_OWNER);
	}
	if( !ad.LookupString(ATTR_NT_DOMAIN,&domain) ) {
		domain = strdup("");
	}

	if( !init_user_ids(owner,domain) ) {
		EXCEPT("Failed in init_user_ids(%s,%s)\n",owner ? owner : "(nil)",domain ? domain : "(nil)");
	}

	free(owner);
	free(domain);

	return set_user_priv();
}

static priv_state set_user_priv_from_ad(classad::ClassAd const &ad)
{
	std::string owner;
	std::string domain;
	if( !ad.EvaluateAttrString(ATTR_OWNER,owner) ) {
		EXCEPT("Failed to find %s in job ad.\n",ATTR_OWNER);
	}
	ad.EvaluateAttrString(ATTR_NT_DOMAIN,domain);

	if( !init_user_ids(owner.c_str(),domain.c_str()) ) {
		EXCEPT("Failed in init_user_ids(%s,%s)\n",owner.c_str(),domain.c_str());
	}

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
	if( GetAttributeStringNew(cluster, proc, ATTR_JOB_MANAGED, &managed) == 0) {
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

static ClaimJobResult claim_job_with_current_privs(const char * pool_name, const char * schedd_name, int cluster, int proc, MyString * error_details, const char * my_identity)
{
	// Open a qmgr
	FailObj failobj;
	failobj.SetNames(schedd_name, pool_name);

	DCSchedd schedd(schedd_name,pool_name);
	if( ! schedd.locate() ) {
		failobj.fail("Can't find address of schedd\n");
		if(error_details) {
			error_details->sprintf("Can't find address of schedd %s in pool %s",
				schedd_name ? schedd_name : "local schedd",
				pool_name ? pool_name : "local pool");
		}
		return CJR_ERROR;
	}

	CondorError errstack;
	Qmgr_connection * qmgr = ConnectQ(schedd.addr(), 0 /*timeout==default*/, false /*read-only*/, & errstack);
	if( ! qmgr ) {
		failobj.fail("Unable to connect\n%s\n", errstack.getFullText(true));
		if(error_details) {
			error_details->sprintf("Can't connect to schedd %s in pool %s (%s)",
				schedd_name ? schedd_name : "local schedd",
				pool_name ? pool_name : "local pool",
				errstack.getFullText(true));
		}
		return CJR_ERROR;
	}
	failobj.SetQmgr(qmgr);


	//-------
	// Do the actual claim
	ClaimJobResult res = claim_job(cluster, proc, error_details, my_identity);
	//-------


	// Tear down the qmgr
	if( ! DisconnectQ(qmgr, true /* commit */)) {
		failobj.SetQmgr(0);
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

	ClaimJobResult result = claim_job_with_current_privs(pool_name,schedd_name,cluster,proc,error_details,my_identity);

	set_priv(priv);
	return result;
}




bool yield_job(bool done, int cluster, int proc, MyString * error_details, const char * my_identity, bool *keep_trying) {
	ASSERT(cluster > 0);
	ASSERT(proc >= 0);

	bool junk_keep_trying;
	if(!keep_trying) keep_trying = &junk_keep_trying;
	*keep_trying = true;

	// Check that the job is still managed by us
	bool is_managed = false;
	char * managed;
	if( GetAttributeStringNew(cluster, proc, ATTR_JOB_MANAGED, &managed) == 0) {
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
		if( GetAttributeStringNew(cluster, proc, ATTR_JOB_MANAGED_MANAGER, &manager) == 0) {
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
	}

	return true;
}




static bool yield_job_with_current_privs(const char * pool_name, const char * schedd_name,
	bool done, int cluster, int proc, MyString * error_details, const char * my_identity, bool *keep_trying) {
	// Open a qmgr
	FailObj failobj;
	failobj.SetNames(schedd_name, pool_name);

	bool junk_keep_trying;
	if(!keep_trying) keep_trying = &junk_keep_trying;
	*keep_trying = true;

	DCSchedd schedd(schedd_name,pool_name);
	if( ! schedd.locate() ) {
		failobj.fail("Can't find address of schedd\n");
		if(error_details) {
			error_details->sprintf("Can't find address of schedd %s in pool %s",
				schedd_name ? schedd_name : "local schedd",
				pool_name ? pool_name : "local pool");
		}
		return false;
	}

	CondorError errstack;
	Qmgr_connection * qmgr = ConnectQ(schedd.addr(), 0 /*timeout==default*/, false /*read-only*/, & errstack);
	if( ! qmgr ) {
		failobj.fail("Unable to connect\n%s\n", errstack.getFullText(true));
		if(error_details) {
			error_details->sprintf("Can't connect to schedd %s in pool %s (%s)",
				schedd_name ? schedd_name : "local schedd",
				pool_name ? pool_name : "local pool",
				errstack.getFullText(true));
		}
		return false;
	}
	failobj.SetQmgr(qmgr);


	//-------
	// Do the actual yield
	bool res = yield_job(done, cluster, proc, error_details, my_identity, keep_trying);
	//-------


	// Tear down the qmgr
	if( ! DisconnectQ(qmgr, true /* commit */)) {
		failobj.SetQmgr(0);
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
	MyString * error_details, const char * my_identity, bool *keep_trying)
{
	bool success;
	priv_state priv = set_user_priv_from_ad(ad);

	success = yield_job_with_current_privs(pool_name,schedd_name,done,cluster,proc,error_details,my_identity,keep_trying);

	set_priv(priv);

	return success;
}


static bool submit_job_with_current_priv( ClassAd & src, const char * schedd_name, const char * pool_name, int * cluster_out /*= 0*/, int * proc_out /*= 0 */)
{
	FailObj failobj;
	failobj.SetNames(schedd_name, pool_name);

	DCSchedd schedd(schedd_name,pool_name);
	if( ! schedd.locate() ) {
		failobj.fail("Can't find address of schedd\n");
		return false;
	}
	// TODO Consider: condor_submit has to fret about storing a credential on Win32.

		// Does the schedd support the new unified syntax for grid universe
		// jobs (i.e. GridResource and GridJobId used for all types)?
		// If not,  we can't send the job over.
	CondorVersionInfo vi( schedd.version() );
	if( ! vi.built_since_version(6,7,11) ) {
		failobj.fail("Schedd is too old of version and can not handle new Grid unified syntax\n"); // TODO: List remote version number
		return false;
	}

	CondorError errstack;
	Qmgr_connection * qmgr = ConnectQ(schedd.addr(), 0 /*timeout==default*/, false /*read-only*/, & errstack);
	if( ! qmgr ) {
		failobj.fail("Unable to connect\n%s\n", errstack.getFullText(true));
		return false;
	}
	failobj.SetQmgr(qmgr);

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

	// Things to set because we want to spool/sandbox input files
	{
		// we need to submit on hold (taken from condor_submit.V6/submit.C)
		src.Assign(ATTR_JOB_STATUS, 5); // 5==HELD
		src.Assign(ATTR_HOLD_REASON, "Spooling input data files");

		// we want the job to hang around (taken from condor_submit.V6/submit.C)
		MyString leaveinqueue;
		leaveinqueue.sprintf("%s == %d", ATTR_JOB_STATUS, COMPLETED);
		/*
		leaveinqueue.sprintf("%s == %d && (%s =?= UNDEFINED || %s == 0 || ((CurrentTime - %s) < %d))",
			ATTR_JOB_STATUS,
			COMPLETED,
			ATTR_COMPLETION_DATE,
			ATTR_COMPLETION_DATE,
			ATTR_COMPLETION_DATE,
			60 * 60 * 24 * 10);
		*/

		src.AssignExpr(ATTR_JOB_LEAVE_IN_QUEUE, leaveinqueue.Value());
	}

	ExprTree * tree;
	src.ResetExpr();
	while( (tree = src.NextExpr()) ) {
		char *lhstr = 0;
		char *rhstr = 0;
		ExprTree * lhs = 0;
		ExprTree * rhs = 0;
		if( (lhs = tree->LArg()) ) { lhs->PrintToNewStr (&lhstr); }
		if( (rhs = tree->RArg()) ) { rhs->PrintToNewStr (&rhstr); }
		if( !lhs || !rhs || !lhstr || !rhstr) { 
			failobj.fail("Problem processing classad\n");
			return false;
		}
		if( SetAttribute(cluster, proc, lhstr, rhstr) == -1 ) {
			failobj.fail("Failed to set %s = %s\n", lhstr, rhstr);
			return false;
		}
		free(lhstr);
		free(rhstr);
	}

	if( ! DisconnectQ(qmgr, true /* commit */)) {
		failobj.SetQmgr(0);
		failobj.fail("Failed to commit job submission\n");
		return false;
	}

	failobj.SetQmgr(0);
	ClassAd * adlist[1];
	adlist[0] = &src;
	if( ! schedd.spoolJobFiles(1, adlist, &errstack) ) {
		failobj.fail("Failed to spool job files: %s\n",errstack.getFullText(true));
		return false;
	}

	if(cluster_out) { *cluster_out = cluster; }
	if(proc_out) { *proc_out = proc; }

	return true;
}

bool submit_job( ClassAd & src, const char * schedd_name, const char * pool_name, int * cluster_out /*= 0*/, int * proc_out /*= 0 */)
{
	bool success;
	priv_state priv = set_user_priv_from_ad(src);

	success = submit_job_with_current_priv(src,schedd_name,pool_name,cluster_out,proc_out);

	set_priv(priv);

	return success;
}

bool submit_job( classad::ClassAd & src, const char * schedd_name, const char * pool_name, int * cluster_out /*= 0*/, int * proc_out /*= 0 */)
{
	ClassAd src2;
	if( ! new_to_old(src, src2) ) {
		dprintf(D_ALWAYS, "submit_job failed to convert job ClassAd from new to old form\n");
		return false;
	}
	return submit_job(src2, schedd_name, pool_name, cluster_out, proc_out);
}

/*
	Push the dirty attributes in src into the queue.  Does _not_ clear
	the dirty attributes (use ClassAd::ClearAllDirtyFlags() if you want).
	Assumes the existance of an open qmgr connection (via ConnectQ).
*/
bool push_dirty_attributes(int cluster, int proc, ClassAd & src)
{
	src.ResetExpr();
	ExprTree * tree;
	while( (tree = src.NextDirtyExpr()) ) {
		char *lhstr = 0;
		char *rhstr = 0;
		ExprTree * lhs = 0;
		ExprTree * rhs = 0;
		if( (lhs = tree->LArg()) ) { lhs->PrintToNewStr (&lhstr); }
		if( (rhs = tree->RArg()) ) { rhs->PrintToNewStr (&rhstr); }
		if( !lhs || !rhs || !lhstr || !rhstr) { 
			dprintf(D_ALWAYS,"(%d.%d) push_dirty_attributes: Problem processing classad\n", cluster, proc);
			return false;
		}
		dprintf(D_FULLDEBUG, "Setting %s = %s\n", lhstr, rhstr);
		if( SetAttribute(cluster, proc, lhstr, rhstr) == -1 ) {
			dprintf(D_ALWAYS,"(%d.%d) push_dirty_attributes: Failed to set %s = %s\n", cluster, proc, lhstr, rhstr);
			return false;
		}
		free(lhstr);
		free(rhstr);
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
	MyString scheddtitle = "local schedd";
	if(schedd_name) {
		scheddtitle = "schedd ";
		scheddtitle += schedd_name;
	}

	DCSchedd schedd(schedd_name,pool_name);
	if( ! schedd.locate() ) {
		dprintf(D_ALWAYS, "push_dirty_attributes: Can't find address of %s\n", scheddtitle.Value());
		return false;
	}

	CondorError errstack;
	Qmgr_connection * qmgr = ConnectQ(schedd.addr(), 0 /*timeout==default*/, false /*read-only*/, & errstack);
	if( ! qmgr ) {
		dprintf(D_ALWAYS, "push_dirty_attributes: Unable to connect to %s\n%s\n", scheddtitle.Value(), errstack.getFullText(true));
		return false;
	}

	bool ret = push_dirty_attributes(src);

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

static bool finalize_job_with_current_privs(int cluster, int proc, const char * schedd_name, const char * pool_name)
{
	DCSchedd schedd(schedd_name,pool_name);
	if( ! schedd.locate() ) {
		if(!schedd_name) { schedd_name = "local schedd"; }
		if(!pool_name) { pool_name = "local pool"; }
		dprintf(D_ALWAYS, "Unable to find address of %s at %s\n", schedd_name, pool_name);
		return false;
	}

	MyString constraint;
	constraint.sprintf("(ClusterId==%d&&ProcId==%d)", cluster, proc);


	// Get our sandbox back
	CondorError errstack;
	int jobssent;
	bool success = schedd.receiveJobSandbox(constraint.Value(), &errstack, &jobssent);
	if( ! success ) {
		dprintf(D_ALWAYS, "(%d.%d) Failed to retrieve sandbox.\n", cluster, proc);
		return false;
	}

	if(jobssent != 1) {
		dprintf(D_ALWAYS, "(%d.%d) Failed to retrieve sandbox (got %d, expected 1).\n", cluster, proc, jobssent);
		return false;
	}


	// Yield the job (clear MANAGED).
	Qmgr_connection * qmgr = ConnectQ(schedd.addr(), 0 /*timeout==default*/, false /*read-only*/, & errstack);
	if( ! qmgr ) {
		dprintf(D_ALWAYS, "finalize_job: Unable to connect to schedd\n%s\n", errstack.getFullText(true));
		return false;
	}

	if( SetAttribute(cluster, proc, ATTR_JOB_LEAVE_IN_QUEUE, "FALSE") == -1 ) {
		dprintf(D_ALWAYS, "finalize_job: failed to set %s = %s for %d.%d\n", ATTR_JOB_LEAVE_IN_QUEUE, "FALSE", cluster, proc);
	}

	if( ! DisconnectQ(qmgr, true /* commit */)) {
		dprintf(D_ALWAYS, "finalize_job: Failed to commit changes\n");
		return false;
	}


	return true;
}

bool finalize_job(classad::ClassAd const &ad,int cluster, int proc, const char * schedd_name, const char * pool_name)
{
	bool success;
	priv_state priv = set_user_priv_from_ad(ad);

	success = finalize_job_with_current_privs(cluster,proc,schedd_name,pool_name);

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
