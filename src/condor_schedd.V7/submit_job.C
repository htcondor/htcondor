#include "condor_common.h"
#include "submit_job.h"
#include "condor_classad.h"
#include "condor_qmgr.h"
#include "dc_schedd.h"
#include "condor_ver_info.h"
#include "condor_attributes.h"



bool submit_job( ClassAd & src, const char * schedd_name, const char * pool_name, int * cluster_out /*= 0*/, int * proc_out /*= 0 */)
{
		// Simplify my error handling and reporting code
	struct FailObj {
		FailObj() : cluster(-1), proc(-1), qmgr(0) { }
		MyString scheddtitle;
		int cluster;
		int proc;
		Qmgr_connection * qmgr;
		void fail( const char * fmt, ...) {
			if(cluster != -1 && qmgr) {
				DestroyCluster(cluster);
			}
			if(qmgr) { DisconnectQ( qmgr, false /* don't commit */); }

			MyString msg;
			msg += "ERROR (";
			msg += scheddtitle;
			msg += ") ";

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
			fprintf(stderr, "%s", msg.Value());
		}
	} failobj;

	MyString scheddtitle = "local schedd";
	if(schedd_name) {
		scheddtitle = "schedd ";
		scheddtitle += schedd_name;
	}
	failobj.scheddtitle = scheddtitle;

	DCSchedd schedd(schedd_name,pool_name);
	if( ! schedd.locate() ) {
		failobj.fail("Can't find address of %s\n", scheddtitle.Value());
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
	failobj.qmgr = qmgr;

	int cluster = NewCluster();
	if( cluster < 0 ) {
		failobj.fail("Failed to create a new cluster (%d)\n", cluster);
		return false;
	}
	failobj.cluster = cluster;

	int proc = NewProc(cluster);
	if( proc < 0 ) {
		failobj.fail("Failed to create a new proc (%d)\n", proc);
		return false;
	}
	failobj.proc = proc;

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
		leaveinqueue.sprintf("%s == %d && (%s =?= UNDEFINED || %s == 0 || ((CurrentTime - %s) < %d))",
			ATTR_JOB_STATUS,
			COMPLETED,
			ATTR_COMPLETION_DATE,
			ATTR_COMPLETION_DATE,
			ATTR_COMPLETION_DATE,
			60 * 60 * 24 * 10);

		src.Assign(ATTR_JOB_LEAVE_IN_QUEUE, leaveinqueue.Value());
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
		failobj.qmgr = 0;
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
