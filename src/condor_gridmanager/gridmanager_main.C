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
		"Usage: %s [-f] [-b] [-t] [-p <port>] [-s <schedd addr>] [-o <owern@uid-domain>] [-C <job constraint>] [-S <scratch dir>]\n",
		basename( name ) );
	DC_Exit( 1 );
}

int
main_init( int argc, char ** const argv )
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
