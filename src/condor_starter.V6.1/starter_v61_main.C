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
#include "condor_debug.h"
#include "condor_config.h"

#include "starter.h"
#include "java_detect.h"

#include "jic_shadow.h"
#include "jic_local_config.h"


extern "C" int exception_cleanup(int,int,char*);	/* Our function called by EXCEPT */
JobInfoCommunicator* parseArgs( int argc, char* argv [] );

static CStarter StarterObj;
CStarter *Starter = &StarterObj;

// this appears at the bottom of this file:
extern "C" int display_dprintf_header(FILE *fp);
static char* dprintf_header = NULL;

int my_argc;
char** my_argv;

static void
usage()
{
	dprintf(D_ALWAYS, "argc = %d\n", my_argc);
	for( int i=0; i < my_argc; i++ ) {
		dprintf( D_ALWAYS, "argv[%d] = %s\n", i, my_argv[i] );
	}
	dprintf(D_ALWAYS, "usage: condor_starter initiating_host\n");
	dprintf(D_ALWAYS, "   or: condor_starter -job_keyword keyword\n");
	dprintf(D_ALWAYS, "                      -job_cluster number\n");
	dprintf(D_ALWAYS, "                      -job_proc    number\n");
	dprintf(D_ALWAYS, "                      -job_subproc number\n");
//	dprintf(D_ALWAYS, "   or: condor_starter -localfile filename\n");
	DC_Exit(1);
}

/* DaemonCore interface implementation */


void
printClassAd( void )
{
	printf( "%s = \"%s\"\n", ATTR_VERSION, CondorVersion() );
	printf( "%s = True\n", ATTR_IS_DAEMON_CORE );
	printf( "%s = True\n", ATTR_HAS_FILE_TRANSFER );
	printf( "%s = True\n", ATTR_HAS_MPI );

		/*
		  Attributes describing what kinds of Job Info Communicators
		  this starter has.  This is mostly for COD, but someday might
		  be useful to other people, too.  There's no need to
		  advertise the fact we've got a JICShadow, since all starters
		  always have and will be able to communicate with a shadow...
		*/

	printf( "%s = True\n", ATTR_HAS_JIC_LOCAL_CONFIG );


		// Java stuff
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
	if( argc == 2 && strincmp(argv[1],"-cla",4) == MATCH ) {
		printClassAd();
		exit(0);
	}
}


char *mySubSystem = "STARTER";

int
main_init(int argc, char *argv[])
{
	my_argc = argc;
	my_argv = argv;

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

	JobInfoCommunicator* jic = NULL;

		// now, based on the command line args, figure out what kind
		// of JIC we need...
	if( argc < 2 ) {
		usage();
	}

	jic = parseArgs( argc, argv );

	if( ! jic ) {
			// we couldn't figure out what to do...
		usage();
	}
	if( !Starter->Init(jic) ) {
		dprintf(D_ALWAYS, "Unable to start job.\n");
		DC_Exit(1);
	}

	return 0;
}


void
invalid( char* opt )
{
	dprintf( D_ALWAYS, "Command-line option '%s' is invalid\n", opt ); 
	usage();
}


void
ambiguous( char* opt )
{
	dprintf( D_ALWAYS, "Command-line option '%s' is ambiguous\n", opt ); 
	usage();
}


void
another( char* opt )
{
	dprintf( D_ALWAYS, 
			 "Command-line option '%s' requires another argument\n", opt ); 
	usage();
}


JobInfoCommunicator*
parseArgs( int argc, char* argv [] )
{
	JobInfoCommunicator* jic = NULL;
	char* job_keyword = NULL; 
	int job_cluster = -1;
	int job_proc = -1;
	int job_subproc = -1;
	char* shadow_host = NULL;

	bool warn_multi_keyword = false;
	bool warn_multi_cluster = false;
	bool warn_multi_proc = false;
	bool warn_multi_subproc = false;

	char *opt, *arg;
	int opt_len;

	char _jobkeyword[] = "-job_keyword";
	char _jobcluster[] = "-job_cluster";
	char _jobproc[] = "-job_proc";
	char _jobsubproc[] = "-job_subproc";
	char _header[] = "-header";
	char* target = NULL;

	ASSERT( argc >= 2 );
	
	char** tmp = argv;
	for( tmp++; *tmp; tmp++ ) {
		target = NULL;
		opt = tmp[0];
		arg = tmp[1];
		opt_len = strlen( opt );

		if( opt[0] != '-' ) {
				// this must be a hostname...
			shadow_host = strdup( opt );
			continue;
		}

		if( ! strncmp(opt, _header, opt_len) ) { 
			if( ! arg ) {
				another( _header );
			}
			dprintf_header = strdup( arg );
			DebugId = display_dprintf_header;
			tmp++;	// consume the arg so we don't get confused 
			continue;
		}

		if( strncmp( "-job_", opt, MIN(opt_len,5)) ) {
			invalid( opt );
		}
		if( opt_len < 6 ) {
			ambiguous( opt );
		}
		switch( opt[5] ) {

		case 'c':
			if( strncmp(_jobcluster, opt, opt_len) ) {
				invalid( opt );
			} 
			target = _jobcluster;
			break;

		case 'k':
			if( strncmp(_jobkeyword, opt, opt_len) ) {
				invalid( opt );
			} 
			target = _jobkeyword;
			break;

		case 'p':
			if( strncmp(_jobproc, opt, opt_len) ) {
				invalid( opt );
			} 
			target = _jobproc;
			break;

		case 's':
			if( strncmp(_jobsubproc, opt, opt_len) ) {
				invalid( opt );
			} 
			target = _jobsubproc;
			break;

		default:
			invalid( opt );
			break;

		}
			// now, make sure we got the arg
		if( ! arg ) {
			another( target );
		} else {
				// consume it for the purposes of the for() loop
			tmp++;
		}
		if( target == _jobkeyword ) {
				// we can check like that, since we're setting target to
				// point to it, so we don't have to do a strcmp().
			if( job_keyword ) {
				warn_multi_keyword = true;
				free( job_keyword );
			}
			job_keyword = strdup( arg );
		} else if( target == _jobcluster ) {
			if( job_cluster >= 0 ) {
				warn_multi_cluster = true;
			}
			job_cluster = atoi( arg );
			if( job_cluster < 0 ) {
				dprintf( D_ALWAYS, 
						 "ERROR: Invalid value for '%s': \"%s\"\n",
						 _jobcluster, arg );
				usage();
			}
		} else if( target == _jobproc ) {
			if( job_proc >= 0 ) {
				warn_multi_proc = true;
			}
			job_proc = atoi( arg );
			if( job_proc < 0 ) {
				dprintf( D_ALWAYS, 
						 "ERROR: Invalid value for '%s': \"%s\"\n",
						 _jobproc, arg );
				usage();
			}
		} else if( target == _jobsubproc ) {
			if( job_subproc >= 0 ) {
				warn_multi_subproc = true;
			}
			job_subproc = atoi( arg );
			if( job_subproc < 0 ) {
				dprintf( D_ALWAYS, 
						 "ERROR: Invalid value for '%s': \"%s\"\n",
						 _jobsubproc, arg );
				usage();
			}
		} else {
				// Should never get here, since we'll hit usage above
				// if we don't know what target option we're doing...
			EXCEPT( "Programmer error in parsing arguments" );
		}
	}

	if( warn_multi_keyword ) {
		dprintf( D_ALWAYS, "WARNING: "
				 "multiple '%s' options given, using \"%s\"\n",
				 _jobkeyword, job_keyword );
	}
	if( warn_multi_cluster ) {
		dprintf( D_ALWAYS, "WARNING: "
				 "multiple '%s' options given, using \"%d\"\n",
				 _jobcluster, job_cluster );
	}
	if( warn_multi_proc ) {
		dprintf( D_ALWAYS, "WARNING: "
				 "multiple '%s' options given, using \"%d\"\n",
				 _jobproc, job_proc );
	}
	if( warn_multi_subproc ) {
		dprintf( D_ALWAYS, "WARNING: "
				 "multiple '%s' options given, using \"%d\"\n",
				 _jobsubproc, job_subproc );
	}

	if( shadow_host ) {
		if( job_keyword ) {
			dprintf( D_ALWAYS, "You cannot use '%s' and specify a "
					 "shadow host\n" );
			usage();
		}
		jic = new JICShadow( shadow_host );
		free( shadow_host );
		shadow_host = NULL;
		return jic;
	}

	if( ! job_keyword ) {
		dprintf( D_ALWAYS, "ERROR: You must specify '%s'\n",
				 _jobkeyword ); 
		usage();
	}

		// If the user didn't specify it, use -1 for cluster and/or
		// proc, and the JIC subclasses will know they weren't on
		// the command-line.
	jic = new JICLocalConfig( job_keyword, job_cluster, job_proc, 
							  job_subproc );
	free( job_keyword );
	return jic;
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


extern "C" 
int
display_dprintf_header(FILE *fp)
{
	if( dprintf_header ) {
		fprintf( fp, "%s ", dprintf_header );
	}
	return TRUE;
}
