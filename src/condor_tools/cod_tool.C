/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-2003 CONDOR Team, Computer Sciences Department, 
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

 

/*********************************************************************
  COD command-line tool
*********************************************************************/

#include "condor_common.h"
#include "condor_distribution.h"
#include "condor_attributes.h"
#include "command_strings.h"
#include "enum_utils.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "condor_version.h"
#include "get_full_hostname.h"
#include "get_daemon_addr.h"
#include "internet.h"
#include "daemon.h"
#include "dc_startd.h"
#include "sig_install.h"


// Global variables
int cmd = 0;
char* addr = NULL;
char* name = NULL;
char* pool = NULL;
char* target = NULL;
char* my_name = NULL;
char* claim_id = NULL;
char* classad_path = NULL;
char* requirements = NULL;
char* job_keyword = NULL;
FILE* CA_PATH = NULL;
int cluster_id = -1;
int proc_id = -1;
int timeout = -1;
bool needs_id = true;
VacateType vacate_type = VACATE_GRACEFUL;


// protoypes of interest
void usage( char* );
void version( void );
void invalid( char* opt );
void ambiguous( char* opt );
void another( char* opt );
void parseCOpt( char* opt, char* arg );
void parsePOpt( char* opt, char* arg );
void parseArgv( int argc, char* argv[] );
int getCommandFromArgv( int argc, char* argv[] );
void printOutput( ClassAd* reply, DCStartd* startd );
void fillRequestAd( ClassAd* );
void fillActivateAd( ClassAd* );


/*********************************************************************
   main()
*********************************************************************/

int
main( int argc, char *argv[] )
{

#ifndef WIN32
	// Ignore SIGPIPE so if we cannot connect to a daemon we do not
	// blowup with a sig 13.
	install_sig_handler(SIGPIPE, SIG_IGN );
#endif

	myDistro->Init( argc, argv );

	config();

	cmd = getCommandFromArgv( argc, argv );
	
	parseArgv( argc, argv );

	DCStartd startd( target, pool );

	if( needs_id ) {
		assert( claim_id );
		startd.setClaimId( claim_id );
	}

	if( ! startd.locate() ) {
		fprintf( stderr, "ERROR: %s\n", startd.error() );
		exit( 1 );
	}

	bool rval;
	ClassAd reply;
	ClassAd ad;

	switch( cmd ) {
	case CA_REQUEST_CLAIM:
		fillRequestAd( &ad );
		rval = startd.requestClaim( CLAIM_COD, &ad, &reply, timeout );
		break;
	case CA_ACTIVATE_CLAIM:
		fillActivateAd( &ad );
		rval = startd.activateClaim( &ad, &reply, timeout );
		break;
	case CA_SUSPEND_CLAIM:
		rval = startd.suspendClaim( &reply, timeout );
		break;
	case CA_RESUME_CLAIM:
		rval = startd.resumeClaim( &reply, timeout );
		break;
	case CA_DEACTIVATE_CLAIM:
		rval = startd.deactivateClaim( vacate_type, &reply, timeout );
		break;
	case CA_RELEASE_CLAIM:
		rval = startd.releaseClaim( vacate_type, &reply, timeout );
		break;
	}

	if( ! rval ) {
		fprintf( stderr, "Attempt to send %s to startd %s failed\n%s\n",
				 getCommandString(cmd), startd.addr(), startd.error() ); 
		return 1;
	}

	printOutput( &reply, &startd );
	return 0;
}



/*********************************************************************
   Helper functions used by main()
*********************************************************************/


void
printOutput( ClassAd* reply, DCStartd* startd )
{

	char* claim_id = NULL;
	char* last_state = NULL;

	if( cmd == CA_REQUEST_CLAIM ) {
		reply->LookupString( ATTR_CLAIM_ID, &claim_id );
	}

	if( CA_PATH ) {
		reply->fPrint( CA_PATH );
		fclose( CA_PATH );
		printf( "Successfully sent %s to startd at %s\n",
				getCommandString(cmd), startd->addr() ); 
		printf( "Result ClassAd written to %s\n", classad_path );
		if( cmd == CA_REQUEST_CLAIM ) {
			printf( "ID of new claim is: \"%s\"\n", claim_id );
			free( claim_id );
		}
		return;
	}

	if( cmd == CA_REQUEST_CLAIM ) {
		fprintf( stderr, "Successfully sent %s to startd at %s\n", 
				 getCommandString(cmd), startd->addr() ); 
		fprintf( stderr, "WARNING: You did not specify "
				 "-classad_path, printing to STDOUT\n" );
		reply->fPrint( stdout );
		fprintf( stderr, "ID of new claim is: \"%s\"\n", claim_id );
		free( claim_id );
		return;
	}

	printf( "Successfully sent %s to startd at %s\n",
			getCommandString(cmd), startd->addr() );

	switch( cmd ) {
	case CA_REQUEST_CLAIM:
		EXCEPT( "Already handled CA_REQUEST_CLAIM!" );
		break;

	case CA_RELEASE_CLAIM:
		if( reply->LookupString(ATTR_LAST_CLAIM_STATE, &last_state) ) { 
			printf( "State of claim when it was released: \"%s\"\n",
					last_state );
			free( last_state );
		} else {
			fprintf( stderr, "Warning: "
					 "reply ClassAd did not contain attribute \"%s\"\n",
					 ATTR_LAST_CLAIM_STATE );
		}
		break;
	default:
			// nothing else yet to print
		break;
	}
}


void
fillRequirements( ClassAd* req )
{
	MyString jic_req;
	jic_req = ATTR_HAS_JIC_LOCAL_CONFIG;
	jic_req += "==TRUE";

	MyString require;
	require = ATTR_REQUIREMENTS;
	require += '=';

	if( requirements ) {
		require += '(';
		require += requirements;
		require += ")&&(";
		require += jic_req;
		require += ')';
	} else {
		require += jic_req;
	}
	if( ! req->Insert(require.Value()) ) {
		fprintf( stderr, "ERROR: can't parse requirements '%s'\n",
				 requirements );
		exit( 1 );
	}
}


void
fillRequestAd( ClassAd* req )
{
	fillRequirements( req );
}


void
fillActivateAd( ClassAd* req )
{
	fillRequirements( req );

	MyString line;
	if( cluster_id >= 0 ) {
		line = ATTR_CLUSTER_ID;
		line += '=';
		line += cluster_id;
		req->Insert( line.Value() );
	}
	if( proc_id >= 0 ) {
		line = ATTR_PROC_ID;
		line += '=';
		line += proc_id;
		req->Insert( line.Value() );
	}
	if( job_keyword ) {
		line = ATTR_JOB_KEYWORD;
		line += "=\"";
		line += job_keyword;
		line += '"';
		req->Insert( line.Value() );
	}
}


/*********************************************************************
   Helper functions to parse the command-line 
*********************************************************************/

int
getCommandFromArgv( int argc, char* argv[] )
{
	char* cmd_str = NULL;
	int size;

	my_name = strrchr( argv[0], DIR_DELIM_CHAR );
	if( !my_name ) {
		my_name = argv[0];
	} else {
		my_name++;
	}

		// See if there's an '-' in our name, if not, append argv[1]. 
	cmd_str = strchr( my_name, '_');
	if( !cmd_str ) {

			// If there's no argv[1], print usage.
		if( ! argv[1] ) { usage( my_name ); }

			// If argv[1] begins with '-', print usage, don't append.
		if( argv[1][0] == '-' ) { 
				// The one exception is if we got a "cod -v", we
				// should print the version, not give an error.
			if( argv[1][1] == 'v' ) {
				version();
			} else {
				usage( my_name );
			}
		}
		size = strlen( argv[1] );
		my_name = (char*)malloc( size + 5 );
		sprintf( my_name, "cod %s", argv[1] );
		cmd_str = my_name+4;
		argv++; argc--;
	} else {
		cmd_str++;
	}
		// Figure out what kind of tool we are.
	if( !strcmp( cmd_str, "request" ) ) {
			// this is the only one that doesn't require a claim id 
		needs_id = false;
		return CA_REQUEST_CLAIM;
	} else if( !strcmp( cmd_str, "release" ) ) {
		return CA_RELEASE_CLAIM;
	} else if( !strcmp( cmd_str, "activate" ) ) {
		return CA_ACTIVATE_CLAIM;
	} else if( !strcmp( cmd_str, "deactivate" ) ) {
		return CA_DEACTIVATE_CLAIM;
	} else if( !strcmp( cmd_str, "suspend" ) ) {
		return CA_SUSPEND_CLAIM;
	} else if( !strcmp( cmd_str, "resume" ) ) {
		return CA_RESUME_CLAIM;
	} else {
		fprintf( stderr, "ERROR: unknown command %s\n", my_name );
		usage( "cod" );
	}
	return -1;
}


void
version()
{
	printf( "%s\n%s\n", CondorVersion(), CondorPlatform() );
	exit( 0 );
}

void
invalid( char* opt )
{
	fprintf( stderr, "%s: '%s' is invalid\n", my_name, opt );
	usage( my_name );
}


void
ambiguous( char* opt )
{
	fprintf( stderr, "%s: '%s' is ambiguous\n", my_name, opt ); 
	usage( my_name );
}


void
another( char* opt )
{
	fprintf( stderr, "%s: '%s' requires another argument\n", my_name,
			 opt ); 
	usage( my_name );
}


void
parseCOpt( char* opt, char* arg )
{
 		// first, try to understand what the option is they passed,
		// given what kind of cod command we want to send.  -cluster
		// is only valid for activate, so if we're not doing that, we
		// can assume they want -classadpath

	char _cpath[] = "-classad";
	char _clust[] = "-cluster";
	char* target = NULL;

		// we already checked these, make sure we're not crazy
	assert( opt[0] == '-' && opt[1] == 'c' );  

	if( cmd == CA_ACTIVATE_CLAIM ) {
		if( ! (opt[2] && opt[3]) ) {
			ambiguous( opt );
		}
		if( opt[2] != 'l' ) {
			invalid( opt );
		}
		switch( opt[3] ) {
		case 'a':
			if( strncmp(_cpath, opt, strlen(opt)) ) {
				invalid( opt );
			} 
			target = _cpath;
			break;

		case 'u':
			if( strncmp(_clust, opt, strlen(opt)) ) {
				invalid( opt );
			} 
			target = _clust;
			break;

		default:
			invalid( opt );
			break;
		}
	} else { 
		if( strncmp(_cpath, opt, strlen(opt)) ) {
			invalid( opt );
		}
		target = _cpath;
	}

		// now, make sure we got the arg
	if( ! arg ) {
		another( target );
	}
	if( target == _clust ) {
			// we can check like that, since we're setting target to
			// point to it, so we don't have to do a strcmp().
		cluster_id = atoi( arg );
	} else {
		classad_path = strdup( arg );
	}
}


void
parsePOpt( char* opt, char* arg )
{
 		// first, try to understand what the option is they passed,
		// given what kind of cod command we want to send.  -proc
		// is only valid for activate, so if we're not doing that, we
		// can assume they want -pool

	char _pool[] = "-pool";
	char _proc[] = "-proc";
	char* target = NULL;

		// we already checked these, make sure we're not crazy
	assert( opt[0] == '-' && opt[1] == 'p' );  

	if( cmd == CA_ACTIVATE_CLAIM ) {
		if( ! opt[2] ) {
			ambiguous( opt );
		}
		switch( opt[2] ) {
		case 'o':
			if( strncmp(_pool, opt, strlen(opt)) ) {
				invalid( opt );
			} 
			target = _pool;
			break;

		case 'r':
			if( strncmp(_proc, opt, strlen(opt)) ) {
				invalid( opt );
			} 
			target = _proc;
			break;

		default:
			invalid( opt );
			break;
		}
	} else { 
		if( strncmp(_pool, opt, strlen(opt)) ) {
			invalid( opt );
		} 
		target = _pool;
	}

		// now, make sure we got the arg
	if( ! arg ) {
		another( target );
	}

	if( target == _pool ) {
		pool = get_full_hostname( (const char *)(arg) );
		if( !pool ) {
			fprintf( stderr, "%s: unknown host %s\n", my_name, arg );
			exit( 1 );
		}
	} else {
		proc_id = atoi( arg );
	}
}


void
parseArgv( int argc, char* argv[] )
{
	char** tmp = argv;

	for( tmp++; *tmp; tmp++ ) {
		if( (*tmp)[0] != '-' ) {
				// If it doesn't start with '-', skip it
			continue;
		}
		switch( (*tmp)[1] ) {

				// // // // // // // // // // // // // // // //
				// Shared options that make sense to all cmds 
				// // // // // // // // // // // // // // // //

		case 'v':
			if( strncmp("-version", *tmp, strlen(*tmp)) ) {
				invalid( *tmp );
			} 
			version();
			break;

		case 'h':
			if( strncmp("-help", *tmp, strlen(*tmp)) ) {
				invalid( *tmp );
			} 
			usage( my_name );
			break;

		case 'd':
			if( strncmp("-debug", *tmp, strlen(*tmp)) ) {
				invalid( *tmp );
			} 
			Termlog = 1;
			dprintf_config ("TOOL", 2);
			break;

		case 'a':
			if( strncmp("-address", *tmp, strlen(*tmp)) ) {
				invalid( *tmp );
			} 
			tmp++;
			if( ! (tmp && *tmp) ) {
				another( "-address" );
			}
			if( ! is_valid_sinful(*tmp) ) {
                fprintf( stderr, "%s: '%s' is not a valid address\n",
						 my_name, *tmp );
				exit( 1 );
			}
			addr = strdup( *tmp ); 
			break;

		case 'n':
			if( strncmp("-name", *tmp, strlen(*tmp)) ) {
				invalid( *tmp );
			} 
			tmp++;
			if( ! (tmp && *tmp) ) {
				another( "-name" );
			}
			name = get_daemon_name( *tmp );
			if( ! name ) {
                fprintf( stderr, "%s: unknown host %s\n", my_name, 
                         get_host_part(*tmp) );
				exit( 1 );
			}
			break;

				// // // // // // // // // // // // // // // //
				// Switches that only make sense to some cmds 
				// // // // // // // // // // // // // // // //

		case 'f':
			if( !((cmd == CA_RELEASE_CLAIM) || 
				  (cmd == CA_DEACTIVATE_CLAIM)) )
			{
				invalid( *tmp );
			}
			if( strncmp("-fast", *tmp, strlen(*tmp)) ) {
				invalid( *tmp );
			} 
			vacate_type = VACATE_FAST;
			break;

		case 'r':
			if( !((cmd == CA_REQUEST_CLAIM) || 
				  (cmd == CA_ACTIVATE_CLAIM)) )
			{
				invalid( *tmp );
			}
			if( strncmp("-requirements", *tmp, strlen(*tmp)) ) {
				invalid( *tmp );
			} 
			tmp++;
			if( ! (tmp && *tmp) ) {
				another( "-requirements" );
			}
			requirements = strdup( *tmp );
			break;

		case 'i':
			if( cmd == CA_REQUEST_CLAIM ) {
				invalid( *tmp );
			}
			if( strncmp("-id", *tmp, strlen(*tmp)) ) {
				invalid( *tmp );
			} 
			tmp++;
			if( ! (tmp && *tmp) ) {
				another( "-id" );
			}
			claim_id = strdup( *tmp );
			break;

		case 'k':
			if( cmd != CA_ACTIVATE_CLAIM ) {
				invalid( *tmp );
			}
			if( strncmp("-keyword", *tmp, strlen(*tmp)) ) {
				invalid( *tmp );
			} 
			tmp++;
			if( ! (tmp && *tmp) ) {
				another( "-keyword" );
			}
			job_keyword = strdup( *tmp );
			break;

				// // // // // // // // // // // // // // // // // // 
				// P and C are complicated, since they are ambiguous
				// in the case of activate, but not others.  so, they
				// have their own methods to make it easier to
				// understand what the hell's going on. :)
				// // // // // // // // // // // // // // // // // //

		case 't':
			if( strncmp("-timeout", *tmp, strlen(*tmp)) ) {
				invalid( *tmp );
			} 
			tmp++;
			if( ! (tmp && *tmp) ) {
				another( "-timeout" );
			}
			timeout = atoi( *tmp );
			break;

		case 'p':
			parsePOpt( tmp[0], tmp[1] );
			tmp++;
			break;

		case 'c':
			parseCOpt( tmp[0], tmp[1] );
			tmp++;
			break;

		default:
			invalid( *tmp );

		}
	}

		// Now that we're done parsing, make sure it all makes sense

	if( needs_id && ! claim_id ) {
		fprintf( stderr,  "ERROR: You must specify a ClaimID with "
				 "-id for %s\n", my_name );
		usage( my_name );
	}

	if( addr && name ) {
		fprintf( stderr, 
				 "ERROR: You cannot specify both -name and -address\n" );
		usage( my_name );
	}

	if( addr ) {
		target = addr;
	} else if( name ) {
		target = name;
	} else { 
			// local startd
		target = NULL;
	}

	if( cmd == CA_ACTIVATE_CLAIM && ! job_keyword ) { 
		fprintf( stderr,
				 "ERROR: You must specify -keyword for %s\n",
				 my_name );
		usage( my_name );
		
	}

	if( classad_path ) { 
		CA_PATH = fopen( classad_path, "w" );
		if( !CA_PATH ) {
			fprintf( stderr, 
					 "ERROR: failed to open '%s': errno %d (%s)\n",
					 classad_path, errno, strerror(errno) );
			exit( 1 );
		}
	}
}


void
printCmd( int cmd )
{
	switch( cmd ) {
	case CA_REQUEST_CLAIM:
		fprintf( stderr, "    request\t\tCreate a new COD claim\n" );
		break;
	case CA_ACTIVATE_CLAIM:
		fprintf( stderr, 
				 "    activate\t\tStart a job on a given claim\n" );
		break;
	case CA_DEACTIVATE_CLAIM:
		fprintf( stderr, "    deactivate\t\tKill the current job, but "
				 "keep the claim\n" );
		break;
	case CA_RELEASE_CLAIM:
		fprintf( stderr, "    release\t\tRelinquish a claim, and kill "
				 "any running job\n" ); 
		break;
	case CA_SUSPEND_CLAIM:
		fprintf( stderr, "    suspend\t\tSuspend the job on a given "
				 "claim\n" ); 
		break;
	case CA_RESUME_CLAIM:
		fprintf( stderr, "    resume\t\tResume the job on a given "
				 "claim\n" ); 
		break;
	}
}
		

void
printFast( void ) 
{
	fprintf( stderr, "    -fast\t\tQuickly kill any running job\n" );
}


void
usage( char *str )
{
	bool has_cmd_opt = true;
	bool needs_id = true;

	if( ! str ) {
		fprintf( stderr, "Use \"-help\" to see usage information\n" );
		exit( 1 );
	}
	if( !cmd ) {
		fprintf( stderr, "Usage: %s [command] [options]\n", str );
		fprintf( stderr, "Where [command] is one of:\n" );
		printCmd( CA_REQUEST_CLAIM );
		printCmd( CA_RELEASE_CLAIM );
		printCmd( CA_ACTIVATE_CLAIM );
		printCmd( CA_DEACTIVATE_CLAIM );
		printCmd( CA_SUSPEND_CLAIM );
		printCmd( CA_RESUME_CLAIM );
		fprintf( stderr, "use %s [command] -help for more "
				 "information on a given command\n", str ); 
		exit( 1 );
	}

	fprintf( stderr, "Usage: %s [target] [general-opts]", str );

	switch( cmd ) {
	case CA_REQUEST_CLAIM:
		needs_id = false;
		break;
	case CA_SUSPEND_CLAIM:
	case CA_RESUME_CLAIM:
		has_cmd_opt = false;
		break;
	}

	if( has_cmd_opt ) {
		fprintf( stderr, " [command-opts]" );
	} 
	if( needs_id ) {
		fprintf( stderr, " -id ClaimId" );
	}
	fprintf( stderr, "\n" );

	printCmd( cmd );
	
	fprintf( stderr, "\nWhere [target] can be zero or one of:\n" );
	fprintf( stderr, "    -name hostname\tContact the startd on the "
			 "given host\n" ); 
	fprintf( stderr, "    -pool hostname\tUse the given central manager "
			 "to find the startd\n\t\t\trequested with -name\n" );
	fprintf( stderr, "    -addr <addr:port>\tContact the startd at the " 
			 "given \"sinful string\"\n" );
	fprintf( stderr, "    (If no targets are specified, "
			 "the local host is used)\n" );

	fprintf( stderr, "\nWhere [general-opts] can be zero or more of:\n" );
	fprintf( stderr, "    -help\t\tGive this usage information\n" );
	fprintf( stderr, "    -debug\t\tPrint verbose debugging information\n" );
	fprintf( stderr, "    -version\t\tPrint the version\n" );
	fprintf( stderr, "    -timeout N\t\tTimeout all network "
			 "operations after N seconds\n" );
	fprintf( stderr, "    -classad file\tPrint the reply ClassAd to "
			 "the given file\n" );

	if( has_cmd_opt ) {
		fprintf( stderr,
 				 "\nWhere [command-opts] can be zero or more of:\n" );
	}

	switch( cmd ) {

	case CA_REQUEST_CLAIM:
		fprintf( stderr, "    -requirements expr\tFind a resource "
				 "that matches the boolean expression\n" );
		break;

	case CA_ACTIVATE_CLAIM:
		fprintf( stderr, "    -keyword string\tUse the keyword to "
				 "find the job in the config file\n" );
		fprintf( stderr, "    -cluster N\t\tStart the job with the "
				 "given cluster ID\n" );
		fprintf( stderr, "    -proc N\t\tStart the job with the "
				 "given proc ID\n" );
		fprintf( stderr, "    -requirements expr\tFind a starter "
				 "that matches the boolean expression\n" );
		break;

	case CA_RELEASE_CLAIM:
	case CA_DEACTIVATE_CLAIM:
		printFast();
		break;
	}

	fprintf(stderr, "\n" );
	exit( 1 );
}


