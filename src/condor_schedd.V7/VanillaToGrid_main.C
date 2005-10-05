#include "condor_common.h"
#include "condor_config.h"
#include "condor_classad.h"
#include "VanillaToGrid.h"
#include "submit_job.h"
#include "classad_newold.h"
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





int main(int argc, char **argv) 
{
	if(argc != 3) {
		die("Usage:\n"
			"  %s filename 'gridurl'\n"
			"     filename - filename of classad to process\n"
			"     girdurl - Unified grid URL\n");
	}

	config(); // Initialize param.

	// Load old classad string.
	MyString s_oldad = slurp_file(argv[1]);
	// Convert to old class ad.
	ClassAd oldad((char *)s_oldad.Value(),'\n');

	classad::ClassAd newad;
	if( ! old_to_new(oldad, newad) ) {
		die("Failed to parse class ad\n");
	}

	//====================================================================
	// Do something interesting:
	VanillaToGrid::vanillaToGrid(&newad, argv[2]);
	//====================================================================

	ClassAd oldadout;
	new_to_old(newad, oldadout); 

	int cluster,proc;
	if( ! submit_job( oldadout, 0, 0, &cluster, &proc ) ) {
		fprintf(stderr, "Failed to submit job\n");
	}
	printf("Successfully submitted %d.%d\n",cluster,proc);

	// Convert to old classad string
	MyString out;
	oldadout.sPrint(out);

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
