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
#include "condor_debug.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "basename.h"

#include "globus_utils.h"

#include "gridmanager.h"


char *mySubSystem = "GRIDMANAGER";	// used by Daemon Core

char *myUserName = NULL;

// this appears at the bottom of this file
extern "C" int display_dprintf_header(FILE *fp);

void
usage( char *name )
{
	dprintf( D_ALWAYS, 
		"Usage: %s [-f] [-b] [-t] [-p <port>] [-s <schedd addr>] [-o <owern@uid-domain>] [-x <x509_user_proxy>] [-C <job constraint>] [-S <scratch dir>]\n",
		basename( name ) );
	DC_Exit( 1 );
}

int
main_init( int argc, char **argv )
{

	dprintf(D_FULLDEBUG,
		"Welcome to the all-singing, all dancing, \"amazing\" GridManager!\n");

	// handle specific command line args
	int i = 1;
	while ( i < argc ) {
		if ( argv[i][0] != '-' )
			usage( argv[0] );

		switch( argv[i][1] ) {
		case 'C':
			if ( argc <= i + 1 )
				usage( argv[0] );
			ScheddJobConstraint = strdup( argv[i + 1] );
			i++;
			break;
		case 's':
			// don't check parent for schedd addr. use this one instead
			if ( argc <= i + 1 )
				usage( argv[0] );
			ScheddAddr = strdup( argv[i + 1] );
			i++;
			break;
		case 'S':
			if ( argc <= i + 1 )
				usage( argv[0] );
			GridmanagerScratchDir = strdup( argv[i + 1] );
			i++;
			break;
		case 'x':
			// Use a different user_proxy;
			if ( argc <= i + 1 )
				usage( argv[0] );
			X509Proxy = strdup( argv[i + 1] );
			useDefaultProxy = false;
			i++;
			break;
		case 'o':
			// use a different owner name; this is useful if
			// Condor-G is running as non-root, and yet
			// different users are allowed to submit to the schedd.
			if ( argc <= i + 1 )
				usage( argv[0] );
			myUserName = strdup( argv[i + 1] );
			i++;
			break;
		default:
			usage( argv[0] );
			break;
		}

		i++;
	}

	// Setup dprintf to display pid
	DebugId = display_dprintf_header;

	Init();
	Register();

	return TRUE;
}

int
main_config( bool is_full )
{
	Reconfig();
	return TRUE;
}

int
main_shutdown_fast()
{
	DC_Exit(0);
	return TRUE;	// to satisfy c++
}

int
main_shutdown_graceful()
{
	DC_Exit(0);
	return TRUE;	// to satify c++
}

void
main_pre_dc_init( int argc, char* argv[] )
{
}

void
main_pre_command_sock_init( )
{
}

// This function is called by dprintf - always display our pid in our
// log entries. 
extern "C" 
int
display_dprintf_header(FILE *fp)
{
	static pid_t mypid = 0;

	if (!mypid) {
		mypid = daemonCore->getpid();
	}

	fprintf( fp, "[%ld] ", (long)mypid );

	return TRUE;
}
