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



ClaimJobResult claim_job(int cluster, int proc, MyString * error_details)
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

	return CJR_OK;
}

ClaimJobResult claim_job(const char * pool_name, const char * schedd_name, int cluster, int proc, MyString * error_details)
{
	// Open a qmgr
	FailObj failobj;
	failobj.SetNames(schedd_name, pool_name);

	DCSchedd schedd(schedd_name,pool_name);
	if( ! schedd.locate() ) {
		failobj.fail("Can't find address of schedd\n");
		return CJR_ERROR;
	}

	CondorError errstack;
	Qmgr_connection * qmgr = ConnectQ(schedd.addr(), 0 /*timeout==default*/, false /*read-only*/, & errstack);
	if( ! qmgr ) {
		failobj.fail("Unable to connect\n%s\n", errstack.getFullText(true));
		return CJR_ERROR;
	}
	failobj.SetQmgr(qmgr);


	//-------
	// Do the actual claim
	ClaimJobResult res = claim_job(cluster, proc, error_details);
	//-------


	// Tear down the qmgr
	if( ! DisconnectQ(qmgr, true /* commit */)) {
		failobj.SetQmgr(0);
		failobj.fail("Failed to commit job claim\n");
		return CJR_ERROR;
	}

	return res;
}



bool submit_job( ClassAd & src, const char * schedd_name, const char * pool_name, int * cluster_out /*= 0*/, int * proc_out /*= 0 */)
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

	ClassAd * adlist[1];
	adlist[0] = &src;
	if( ! schedd.spoolJobFiles(1, adlist, &errstack) ) {
		failobj.fail("Failed to spool job files\n");
		return false;
	}

	if(cluster_out) { *cluster_out = cluster; }
	if(proc_out) { *proc_out = proc; }

	return true;
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

/*
	Push the dirty attributes in src into the queue.  Does _not_ clear
	the dirty attributes.
	Establishes (and tears down) a qmgr connection.
*/
bool push_dirty_attributes(classad::ClassAd & src, const char * schedd_name, const char * pool_name)
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


bool finalize_job(int cluster, int proc, const char * schedd_name, const char * pool_name)
{
	DCSchedd schedd(schedd_name,pool_name);
	if( ! schedd.locate() ) {
		if(!schedd_name) { schedd_name = "local schedd"; }
		if(!pool_name) { pool_name = "local pool"; }
		dprintf(D_ALWAYS, "Unable to find address of %s at %s\n", schedd_name, pool_name);
		printf("Unable to find address of %s at %s\n", schedd_name, pool_name);
		return false;
	}

	MyString constraint;
	constraint.sprintf("(ClusterId==%d&&ProcId==%d)", cluster, proc);
	fprintf(stderr,"%s\n",constraint.Value());


	// Get our sandbox back
	CondorError errstack;
	int jobssent;
	bool success = schedd.receiveJobSandbox(constraint.Value(), &errstack, &jobssent);
	if( ! success ) {
		dprintf(D_ALWAYS, "(%d.%d) Failed to retrieve sandbox.\n", cluster, proc);
		printf("(%d.%d) Failed to retrieve sandbox.\n", cluster, proc);
		return false;
	}

	if(jobssent != 1) {
		dprintf(D_ALWAYS, "(%d.%d) Failed to retrieve sandbox (got %d, expected 1).\n", cluster, proc, jobssent);
		printf("(%d.%d) Failed to retrieve sandbox (got %d, expected 1).\n", cluster, proc, jobssent);
		return false;
	}


	// Yield the job (clear MANAGED).
	Qmgr_connection * qmgr = ConnectQ(schedd.addr(), 0 /*timeout==default*/, false /*read-only*/, & errstack);
	if( ! qmgr ) {
		dprintf(D_ALWAYS, "finalize_job: Unable to connect to schedd\n%s\n", errstack.getFullText(true));
		return false;
	}

	// Check that the job is managed.
	char * managed;
	bool is_managed = false;
	if( GetAttributeStringNew(cluster, proc, ATTR_JOB_MANAGED, &managed) == 0) {
		is_managed = strcmp(managed, MANAGED_EXTERNAL) == 0;
		free(managed);
	}
	if( ! is_managed ) {
		dprintf(D_ALWAYS, "finalize_job: job %d.%d doesn't appear to be managed.\n", cluster, proc);
	} else {
		if( SetAttributeString(cluster, proc, ATTR_JOB_MANAGED, MANAGED_DONE) == -1 ) {
			dprintf(D_ALWAYS, "finalize_job: failed to set %s = %s for %d.%d\n", ATTR_JOB_MANAGED, MANAGED_DONE, cluster, proc);
		}
	}

	if( ! DisconnectQ(qmgr, true /* commit */)) {
		dprintf(D_ALWAYS, "finalize_job: Failed to commit changes\n");
		return false;
	}


	return true;
}
