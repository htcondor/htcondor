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
#include "condor_io.h"
#include "condor_attributes.h"
#include "condor_version.h"
#include "starter.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "java_detect.h"

extern "C" int exception_cleanup(int,int,char*);	/* Our function called by EXCEPT */

static CStarter StarterObj;
CStarter *Starter = &StarterObj;
ReliSock *syscall_sock = NULL;

static void
usage()
{
	dprintf(D_ALWAYS, "usage: condor_starter initiating_host\n");
	DC_Exit(1);
}

/* DaemonCore interface implementation */


void
printClassAd( void )
{
	printf( "%s = True\n", ATTR_IS_DAEMON_CORE );
	printf( "%s = True\n", ATTR_HAS_FILE_TRANSFER );
	printf( "%s = True\n", ATTR_HAS_MPI );
	printf( "%s = \"%s\"\n", ATTR_VERSION, CondorVersion() );

	config(true);

	ClassAd *ad = java_detect();
	if(ad) {
		int gotone=0;
		float mflops;
		char str[ATTRLIST_MAX_EXPRESSION];

		if(ad->LookupString(ATTR_JAVA_VENDOR,str)) {
			printf("%s = \"%s\"\n",ATTR_JAVA_VENDOR,str);
			gotone++;
		}
		if(ad->LookupString(ATTR_JAVA_VERSION,str)) {
			printf("%s = \"%s\"\n",ATTR_JAVA_VERSION,str);
			gotone++;
		}
		if(ad->LookupFloat(ATTR_JAVA_MFLOPS,mflops)) {
			printf("%s = %f\n", ATTR_JAVA_MFLOPS,mflops);
			gotone++;
		}
		if(gotone>0) printf( "%s = True\n",ATTR_HAS_JAVA);		
		delete ad;
	}
}

void
main_pre_dc_init( int argc, char* argv[] )
{
	if( argc == 2 && strincmp(argv[1],"-cl",3) == MATCH ) {
		printClassAd();
		exit(0);
	}
}


char *mySubSystem = "STARTER";

int
main_init(int argc, char *argv[])
{
	if (argc != 2) {
		dprintf(D_ALWAYS, "argc = %d\n", argc);
		for (int i=0; i < argc; i++) {
			dprintf(D_ALWAYS, "argv[%d] = %s\n", i, argv[i]);
		}
		usage();
	}

#ifdef WIN32
	// On NT, we need to make certain we have a console.
	// This is because we send soft kill via CTRL_C or CTRL_BREAK,
	// and this is done via the console, so we must have one.
	// ... and we do have one, cuz Daemon Core created one for us
	// at startup time.
	// Now we need to do a null call to SetConsoleCtrlHandler.
	// For some reason, sending CTRL_C's does not work until we
	// do this (seems like an NT bug).
	SetConsoleCtrlHandler(NULL, FALSE);
#endif

	// register a cleanup routine to kill our kids in case we EXCEPT
	_EXCEPT_Cleanup = exception_cleanup;

	Stream **socks = daemonCore->GetInheritedSocks();
	if (socks[0] == NULL || 
		socks[1] != NULL || 
		socks[0]->type() != Stream::reli_sock) 
	{
		dprintf(D_ALWAYS, "Failed to inherit remote system call socket.\n");
		DC_Exit(1);
	}
	syscall_sock = (ReliSock *)socks[0];
		/* Set a timeout on remote system calls.  This is needed in
		   case the user job exits in the middle of a remote system
		   call, leaving the shadow blocked.  -Jim B. */
	syscall_sock->timeout(300);

	if(!Starter->Init(argv[1])) {
		dprintf(D_ALWAYS, "Unable to start job.\n");
		DC_Exit(1);
	}

	return 0;
}

int
main_config( bool is_full )
{
	Starter->Config();
	return 0;
}

int
main_shutdown_fast()
{
	if ( Starter->ShutdownFast(0) ) {
		// ShutdownGraceful says it is already finished, because
		// there are no jobs to shutdown.  No need to stick around.
		DC_Exit(0);
	}
	return 0;
}

int
main_shutdown_graceful()
{
	if ( Starter->ShutdownGraceful(0) ) {
		// ShutdownGraceful says it is already finished, because
		// there are no jobs to shutdown.  No need to stick around.
		DC_Exit(0);
	}
	return 0;
}

extern "C" 
int exception_cleanup(int,int,char*)
{
	_EXCEPT_Cleanup = NULL;
	Starter->ShutdownFast(0);
	return 0;
}

void
main_pre_command_sock_init( )
{
}


