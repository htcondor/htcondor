/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "baseshadow.h"
#include "shadow.h"
#include "mpishadow.h"
#include "parallelshadow.h"
#include "exit.h"
#include "condor_debug.h"
#include "condor_version.h"
#include "condor_attributes.h"
#include "condor_qmgr.h"
#include "shadow_initializer.h"

BaseShadow *Shadow = NULL;
ShadowInitializer *shad_init = NULL;

static void
usage()
{
	printf( "Usage: condor_shadow schedd_addr host capability cluster proc\n" );
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

	/* Get the job ad and figure what kind of shadow to instantiate. */
	shad_init = new ShadowInitializer(argc, argv);
	if (shad_init == NULL)
	{
		EXCEPT("Out of memory in main_init()!");
	}
	shad_init->Bootstrap();
	return 0;
}

int
main_config( bool is_full )
{
	if (Shadow == NULL)
	{
		dprintf(D_ALWAYS,
			"Failed to config shadow because it is of unknown type.\n");
		return 0;
	}
	Shadow->config();
	return 0;
}

int
main_shutdown_fast()
{
	if (Shadow == NULL)
	{
		dprintf(D_ALWAYS,
			"Failed to fast shutdown shadow because it is of unknown type.\n");
		return 0;
	}
	Shadow->shutDown( JOB_NOT_CKPTED );
	return 0;
}

int
main_shutdown_graceful()
{
	Shadow->gracefulShutDown();
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


void
printClassAd( void )
{
	printf( "%s = True\n", ATTR_IS_DAEMON_CORE );
	printf( "%s = True\n", ATTR_HAS_FILE_TRANSFER );
	printf( "%s = True\n", ATTR_HAS_MPI );
	printf( "%s = True\n", ATTR_HAS_JAVA );
	printf( "%s = \"%s\"\n", ATTR_VERSION, CondorVersion() );
}


void
main_pre_dc_init( int argc, char* argv[] )
{
	if( argc == 2 && strincmp(argv[1],"-cl",3) == MATCH ) {
		printClassAd();
		exit(0);
	}
}


void
main_pre_command_sock_init( )
{
}

