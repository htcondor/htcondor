#include "condor_common.h"
#include "condor_config.h"
#include "condor_new_classads.h"
#include "condor_classad.h"
#include "condor_qmgr.h"
#include "condor_attributes.h"
#include "dc_schedd.h"
#include "condor_ver_info.h"
#include "VanillaToGrid.h"
#define WANT_NAMESPACES
#include "classad_distribution.h"



void die(const char * s) {
	printf("FATAL ERROR: %s\n", s);
	exit(1);
}

MyString slurp_file(const char * filename) {
	int fd = open(filename, O_RDONLY);
	if(fd == -1) {
		die("failed to open input");
	}
	MyString s;
	char buf[1024];
	while(true) {
		int bytes = read(fd, buf, sizeof(buf));
		for(int i = 0; i < bytes; ++i) {
			s += buf[i];
		}
		if(bytes != sizeof(buf)) {
			break;
		}
	}
	return s;
}

void write_file(const char * filename, const char * data) {
	FILE * f = fopen(filename, "w");
	if( ! f ) {
		die("Failed to open output");
	}
	fprintf(f, "%s\n", data);
	fclose(f);
}

/* 
- src - The classad to submit.  The ad will be modified based on the needs of
  the submission.  This will include adding QDate, ClusterId and ProcId as well as putting the job on hold (for spooling)

- schedd_name - Name of the schedd to send the job to, as specified in the Name
  attribute of the schedd's classad.  Can be NULL to indicate "local schedd"

- pool_name - hostname (and possible port) of collector where schedd_name can
  be found.  Can be NULL to indicate "whatever COLLECTOR_HOST in my
  condor_config file claims".

*/
bool submit_job( ClassAd & src, const char * schedd_name, const char * pool_name )
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

	printf("Successfully submitted %d.%d\n",cluster,proc);
	return true;
}





int main(int argc, char **argv) 
{
	if(argc != 3) {
		die("Usage:\n"
			"  %s filename 'gridurl'\n"
			"     filename - filename of classad to process\n"
			"     girdurl - Unified grid URL\n");
	}

	config(); // Initialize param.

	// How hard can it be to take a file containing an old classad (in string
	// form) and turn it into a new classad object?

	// Load old classad string.
	MyString s_oldad = slurp_file(argv[1]);
	// Convert to old class ad.
	ClassAd oldad((char *)s_oldad.Value(),'\n');

	// Convert to new classad string
	NewClassAdUnparser oldtonew;
	oldtonew.SetOutputType(true);
	oldtonew.SetOutputTargetType(true);
	MyString s_newad;
	oldtonew.Unparse(&oldad, s_newad);

	// Convert to new classad
	classad::ClassAdParser parser;
	classad::ClassAd newad;
	if( ! parser.ParseClassAd(s_newad.Value(), newad, true) )
	{
		die("Failed to parse class ad\n");
	}


	// Finally, a new classad.


	//====================================================================
	// Do something interesting:
	VanillaToGrid::vanillaToGrid(&newad, "condor adesmet@puffin.cs.wisc.edu puffin.cs.wisc.edu");
	//====================================================================


	// Convert back to new classad string
	classad::ClassAdUnParser unparser;
	std::string s_newadout;
	unparser.Unparse(s_newadout, &newad);

	// Convert to old classad
	NewClassAdParser newtoold;
	ClassAd * newad2 = newtoold.ParseClassAd(s_newadout.c_str());

	if( ! submit_job( *newad2, 0, 0 ) ) {
		fprintf(stderr, "Failed to submit job\n");
	}

	// Convert to old classad string
	MyString out;
	newad2->sPrint(out);

	delete newad2;

	//printf("%s\n", out.Value());

	return 0;
}





// The following is stolen from condor_submit.V6/submit.C
// When this gets folded into the schedd.V7 proper, it will be
// moot since we'll be a full blown DaemonCore daemon.

/************************************
	The following are dummy stubs for the DaemonCore class to allow
	tools using DCSchedd to link.  DaemonCore is brought in
	because of the FileTransfer object.
	These stub functions will become obsoluete once we start linking
	in the real DaemonCore library with the tools, -or- once
	FileTransfer is broken down.
*************************************/
#include "../condor_daemon_core.V6/condor_daemon_core.h"
	DaemonCore* daemonCore = NULL;
	int DaemonCore::Kill_Thread(int) { return 0; }
//char * DaemonCore::InfoCommandSinfulString(int) { return NULL; }
	int DaemonCore::Register_Command(int,char*,CommandHandler,char*,Service*,
		DCpermission,int) {return 0;}
	int DaemonCore::Register_Reaper(char*,ReaperHandler,
		char*,Service*) {return 0;}
	int DaemonCore::Create_Thread(ThreadStartFunc,void*,Stream*,
		int) {return 0;}
	int DaemonCore::Suspend_Thread(int) {return 0;}
	int DaemonCore::Continue_Thread(int) {return 0;}
//	int DaemonCore::Register_Reaper(int,char*,ReaperHandler,ReaperHandlercpp,
//		char*,Service*,int) {return 0;}
//	int DaemonCore::Register_Reaper(char*,ReaperHandlercpp,
//		char*,Service*) {return 0;}
//	int DaemonCore::Register_Command(int,char*,CommandHandlercpp,char*,Service*,
//		DCpermission,int) {return 0;}
/**************************************/
