/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "baseshadow.h"
#include "shadow.h"
#include "mpishadow.h"
#include "exit.h"
#include "condor_debug.h"
#include "condor_attributes.h"
#include "condor_qmgr.h"

BaseShadow *Shadow;

static void
usage()
{
	printf( "Usage: condor_shadow schedd_addr host"
			 "capability cluster proc\n" );
	exit( JOB_SHADOW_USAGE );
}

extern "C" {
int
ExceptCleanup(int, int, char *buf)
{
  BaseShadow::log_except(buf);
  return 0;
}
}

/* DaemonCore interface implementation */

char *mySubSystem = "SHADOW";

int
dummy_reaper(Service *,int pid,int)
{
	dprintf(D_ALWAYS,"dummy-reaper: pid %d exited; ignored\n",pid);
	return TRUE;
}

int
main_init(int argc, char *argv[])
{
	if (argc != 6) {
		dprintf(D_ALWAYS, "Bad usage, argc = %d\n", argc);
		for (int i=0; i < argc; i++) {
			dprintf(D_ALWAYS, "argv[%d] = %s\n", i, argv[i]);
		}
		usage();
	}

	if (argv[1][0] != '<') {
		EXCEPT("schedd_addr not specified with sinful string.");
	}
	
	if (argv[2][0] != '<') {
		EXCEPT("host not specified with sinful string.");
	}

		/* Start up with condor.condor privileges. */
	set_condor_priv();

	_EXCEPT_Cleanup = ExceptCleanup;

		// Register a do-nothing reaper.  This is just because the
		// file transfer object, which could be instantiated later,
		// registers its own reaper and does an EXCEPT if it gets
		// a reaper ID of 1 (since lots of other daemons have a reaper
		// ID of 1 hard-coded as special... this is bad).
	daemonCore->Register_Reaper("dummy_reaper",
							(ReaperHandler)&dummy_reaper,
							"dummy_reaper",NULL);

		// We have to figure out what sort of shadow to make.
		// That's why so much processing goes on here....

		// Get the jobAd from the schedd:
	ClassAd *jobAd = NULL;
	char schedd_addr[128];
	int cluster, proc;
	strncpy ( schedd_addr, argv[1], 128 );
	cluster = atoi(argv[4]);
	proc = atoi(argv[5]);

		// talk to the schedd to get job ad & set remote host:
	if (!ConnectQ(schedd_addr, SHADOW_QMGMT_TIMEOUT, true)) {
		EXCEPT("Failed to connect to schedd!");
	}
	jobAd = GetJobAd(cluster, proc);
	DisconnectQ(NULL);
	if (!jobAd) {
		dprintf( D_ALWAYS, "errno: %d\n", errno );
		EXCEPT("Failed to get job ad from schedd.");
	}

	int universe;
	if (jobAd->LookupInteger(ATTR_JOB_UNIVERSE, universe) < 0) {
			// change to standard when they work...
		universe = VANILLA;
	}
	
	switch ( universe ) {
	case VANILLA:
	case STANDARD:
		Shadow = new UniShadow();
		break;
	case MPI:
		Shadow = new MPIShadow();
		break;
	case PVM:
		EXCEPT( "PVM...hopefully one day..." );
//		Shadow = new PVMShadow();
		break;
	default:
		dprintf ( D_ALWAYS, "Unknown universe: %d.\n", universe );
		EXCEPT( "Universe not supported" );
	}

	Shadow->init( jobAd, argv[1], argv[2], argv[3], argv[4], argv[5]);

	return 0;
}

int
main_config()
{
	Shadow->config();
	return 0;
}

int
main_shutdown_fast()
{
	Shadow->shutDown(JOB_NOT_CKPTED, 0);
	return 0;
}

int
main_shutdown_graceful()
{
	// should request a graceful shutdown from startd here
	exit(0);	// temporary
	return 0;
}


void
dumpClassad( const char* header, ClassAd* ad, int debug_flag )
{
	if( ! header  ) {
		dprintf( D_ALWAYS, "ERROR: called dumpClassad() w/ NULL header\n" ); 
		return;
	}
	if( ! ad  ) {
		dprintf( D_ALWAYS, "ERROR: called dumpClassad(\"%s\") w/ NULL ad\n", 
				 header );   
		return;
	}
	if( DebugFlags & debug_flag ) {
		dprintf( debug_flag, "*** ClassAd Dump: %s ***\n", header );  
		ad->dPrint( debug_flag );
		dprintf( debug_flag, "--- End of ClassAd ---\n" );
	}
}
