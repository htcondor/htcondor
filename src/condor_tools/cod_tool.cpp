/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


 

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
#include "condor_string.h"
#include "get_daemon_name.h"
#include "internet.h"
#include "daemon.h"
#include "dc_startd.h"
#include "dc_collector.h"
#include "sig_install.h"
#include "basename.h"
#include "globus_utils.h"

// Global variables
int cmd = 0;
char* addr = NULL;
char* name = NULL;
DCCollector* pool = NULL;
char* target = NULL;
char* my_name = NULL;
char* claim_id = NULL;
char* classad_path = NULL;
char* requirements = NULL;
char* job_keyword = NULL;
char* jobad_path = NULL;
char* proxy_file = NULL;
FILE* CA_PATH = NULL;
FILE* JOBAD_PATH = NULL;
int cluster_id = -1;
int proc_id = -1;
int timeout = -1;
int lease_time = -1;
bool needs_id = true;
VacateType vacate_type = VACATE_GRACEFUL;

// protoypes of interest
PREFAST_NORETURN void usage( const char*, int iExitCode=1 );
void version( void );
void invalid( const char* opt );
void ambiguous( const char* opt );
void another( const char* opt );
void parseCOpt( char* opt, char* arg );
void parsePOpt( char* opt, char* arg );
void parseArgv( int argc, char* argv[] );
int getCommandFromArgv( int argc, char* argv[] );
void printOutput( ClassAd* reply, DCStartd* startd );
void fillRequestAd( ClassAd* );
void fillActivateAd( ClassAd* );
bool dumpAdIntoRequest( ClassAd* );

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

	DCStartd startd( target, pool ? pool->addr() : NULL );

	if( needs_id ) {
		assert( claim_id );
		startd.setClaimId( claim_id );
	}

	if( ! startd.locate() ) {
		fprintf( stderr, "ERROR: %s\n", startd.error() );
		exit( 1 );
	}

	bool rval = FALSE;
	int irval;
	ClassAd reply;
	ClassAd ad;

	switch( cmd ) {
	case CA_REQUEST_CLAIM:
		fillRequestAd( &ad );
		rval = startd.requestClaim( CLAIM_COD, &ad, &reply, timeout );
		break;
	case CA_ACTIVATE_CLAIM:
		fillActivateAd( &ad );
		irval = startd.activateClaim( &ad, &reply, timeout );
		rval = (irval == OK);
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
	case CA_RENEW_LEASE_FOR_CLAIM:
		rval = startd.renewLeaseForClaim( &reply, timeout );
		break;
	case DELEGATE_GSI_CRED_STARTD:
		irval = startd.delegateX509Proxy( proxy_file, 0, NULL );
		rval = (irval == OK);
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

	char* claimid = NULL;
	char* last_state = NULL;

	if( cmd == CA_REQUEST_CLAIM ) {
		reply->LookupString( ATTR_CLAIM_ID, &claimid );
	}

	if( CA_PATH ) {
		reply->fPrint( CA_PATH );
		fclose( CA_PATH );
		printf( "Successfully sent %s to startd at %s\n",
				getCommandString(cmd), startd->addr() ); 
		printf( "Result ClassAd written to %s\n", classad_path );
		if( cmd == CA_REQUEST_CLAIM ) {
			printf( "ID of new claim is: \"%s\"\n", claimid );
			free( claimid );
		}
		return;
	}

	if( cmd == CA_REQUEST_CLAIM ) {
		fprintf( stderr, "Successfully sent %s to startd at %s\n", 
				 getCommandString(cmd), startd->addr() ); 
		fprintf( stderr, "WARNING: You did not specify "
				 "-classad, printing to STDOUT\n" );
		reply->fPrint( stdout );
		fprintf( stderr, "ID of new claim is: \"%s\"\n", claimid );
		free( claimid );
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
	MyString jic_req = "TARGET.";
	if (jobad_path) {
		jic_req += ATTR_HAS_JIC_LOCAL_STDIN;
	} else {
		jic_req += ATTR_HAS_JIC_LOCAL_CONFIG;
	}
	jic_req += "==TRUE";

	MyString require;
	require = ATTR_REQUIREMENTS;
	require += "=(";

	if (requirements) {
		require += requirements;
		require += ")&&(";
	}
	int slot_id = 0;
	if (name) {
		if (sscanf(name, "slot%d@", &slot_id) != 1) { 
			slot_id = 0;
		}
	}
	if (slot_id > 0) {
		require += "TARGET.";
		require += ATTR_SLOT_ID;
		require += "==";
		require += slot_id;
		require += ")&&(";
	}
	else if (param_boolean("ALLOW_VM_CRUFT", false)) {
		int vm_id = 0;
		if (name) {
			if (sscanf(name, "vm%d@", &vm_id) != 1) { 
				vm_id = 0;
			}
		}
		if (vm_id > 0) {
			require += "TARGET.";
			require += ATTR_VIRTUAL_MACHINE_ID;
			require += "==";
			require += vm_id;
			require += ")&&(";
		}
	}

	require += jic_req;
	require += ')';

	if( ! req->Insert(require.Value()) ) {
		fprintf( stderr, "ERROR: can't parse requirements '%s'\n",
				 requirements );
		exit( 1 );
	}

#if !defined(WANT_OLD_CLASSADS)
		// The user may have entered some references to machine attributes
		// without explicitly specifying the TARGET scope.
	req->AddTargetRefs( TargetMachineAttrs );
#endif

}


void
fillRequestAd( ClassAd* req )
{
	fillRequirements( req );

	if( lease_time != -1 ) {
		req->Assign( ATTR_JOB_LEASE_DURATION, lease_time );
	}
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
	if( jobad_path ) {
		line = ATTR_HAS_JOB_AD;
		line += '=';
		line += "TRUE";
		req->Insert( line.Value() );
		dumpAdIntoRequest( req );
	}
}


bool
dumpAdIntoRequest( ClassAd* req )
{
	bool read_something = false;
    char buf[1024];
	char *tmp;
	while( fgets(buf, 1024, JOBAD_PATH) ) {
        read_something = true;
		chomp( buf );
			// skip any white space
		tmp = &buf[0];
		while( *tmp && isspace(*tmp)) {
			tmp++;
		}
		if( *tmp && *tmp == '#' ) {
				// skip comments
			continue;
		}
		if( *tmp ) {
				// if we got this far and there's still data, try to
				// insert it into our classad...
			if( ! req->Insert(tmp) ) {
				fprintf( stderr, "Failed to insert \"%s\" into ClassAd, "
						 "ignoring this line\n", tmp );
			}
		}
    }
	fclose( JOBAD_PATH );

#if !defined(WANT_OLD_CLASSADS)
		// The user may have entered some references to machine attributes
		// without explicitly specifying the TARGET scope.
	req->AddTargetRefs( TargetMachineAttrs );
#endif

	return read_something;
}



/*********************************************************************
   Helper functions to parse the command-line 
*********************************************************************/

int
getCommandFromArgv( int argc, char* argv[] )
{
	char* cmd_str = NULL;
	int size, baselen;
	char* base = strdup( condor_basename(argv[0]) ); 

		// See if there's an '-' in our name, if not, append argv[1].
	cmd_str = strrchr( base, '_');

		// now, make sure we're not looking at "condor_cod" or
		// something.  if cmd_str is pointing at cod, we want to move
		// beyond that...
	if( cmd_str && !strncasecmp(cmd_str, "_cod", 4) ) {
		if( cmd_str[4] ) {
			cmd_str = &cmd_str[4];
		} else {
			cmd_str = NULL;
		}
	}
		// finally, see what we've got after that...
	if( !cmd_str ) {

			// If there's no argv[1], print usage.
		if( ! argv[1] ) { usage( base ); }

			// If argv[1] begins with '-', print usage, don't append.
		if( argv[1][0] == '-' ) { 
				// The one exception is if we got a "cod -v", we
				// should print the version, not give an error.
			if( argv[1][1] == 'v' ) {
				version();
			} if( argv[1][1] == 'h' ) {
			      usage( base, 0 );
			} else {
				usage( base );
			}
		}
		size = strlen(argv[1]);
		baselen = strlen(base);
			// we also need to store the space and the '\0'.
		my_name = (char*)malloc( size + baselen + 2 );
		ASSERT( my_name != NULL );
		sprintf( my_name, "%s %s", base, argv[1] );
			// skip past the basename and the space...
		cmd_str = my_name+baselen+1;
		free( base );
		argv++; argc--;
	} else {
		my_name = base;
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
	} else if( !strcmp( cmd_str, "renew" ) ) {
		return CA_RENEW_LEASE_FOR_CLAIM;
	} else if( !strcmp( cmd_str, "delegate_proxy" ) ) {
		return DELEGATE_GSI_CRED_STARTD;
	} else {
		fprintf( stderr, "ERROR: unknown command \"%s\"\n", my_name );
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
invalid( const char* opt )
{
	fprintf( stderr, "%s: '%s' is invalid\n", my_name, opt );
	usage( my_name );
}


void
ambiguous( const char* opt )
{
	fprintf( stderr, "%s: '%s' is ambiguous\n", my_name, opt ); 
	usage( my_name );
}


void
another( const char* opt )
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
		// can assume they want -classad

	char _cpath[] = "-classad";
	char _clust[] = "-cluster";
	char* parse_target = NULL;

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
			parse_target = _cpath;
			break;

		case 'u':
			if( strncmp(_clust, opt, strlen(opt)) ) {
				invalid( opt );
			} 
			parse_target = _clust;
			break;

		default:
			invalid( opt );
			break;
		}
	} else { 
		if( strncmp(_cpath, opt, strlen(opt)) ) {
			invalid( opt );
		}
		parse_target = _cpath;
	}

		// now, make sure we got the arg
	if( ! arg ) {
		another( parse_target );
	}
	if( parse_target == _clust ) {
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
	char* parse_target = NULL;

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
			parse_target = _pool;
			break;

		case 'r':
			if( strncmp(_proc, opt, strlen(opt)) ) {
				invalid( opt );
			} 
			parse_target = _proc;
			break;

		default:
			invalid( opt );
			break;
		}
	} else { 
		if( strncmp(_pool, opt, strlen(opt)) ) {
			invalid( opt );
		} 
		parse_target = _pool;
	}

		// now, make sure we got the arg
	if( ! arg ) {
		another( parse_target );
	}

	if( parse_target == _pool ) {
		pool = new DCCollector( arg );
		if( ! pool->addr() ) {
			fprintf( stderr, "%s: %s\n", my_name, pool->error() );
			exit( 1 );
		}
	} else {
		proc_id = atoi( arg );
	}
}


void
parseArgv( int  /*argc*/, char* argv[] )
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
			usage( my_name, 0);
			break;

		case 'd':
			if( strncmp("-debug", *tmp, strlen(*tmp)) ) {
				invalid( *tmp );
			} 
			dprintf_set_tool_debug("TOOL", 0);
			break;

		case 'a':
			if( cmd != CA_REQUEST_CLAIM ) {
				invalid( *tmp );
			}
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
			if( cmd != CA_REQUEST_CLAIM ) {
				invalid( *tmp );
			}
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

		case 'j':
			if( cmd != CA_ACTIVATE_CLAIM ) {
				invalid( *tmp );
			}
			if( strncmp("-jobad", *tmp, strlen(*tmp)) ) {
				invalid( *tmp );
			} 
			tmp++;
			if( ! (tmp && *tmp) ) {
				another( "-jobad" );
			}
			jobad_path = strdup( *tmp );
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

		case 'l':
			if( strncmp("-lease", *tmp, strlen(*tmp)) == 0 ) {
				if( cmd != CA_REQUEST_CLAIM ) {
					invalid( *tmp );
				}
				tmp++;
				if( ! (tmp && *tmp) ) {
					another( "-lease" );
				}
				lease_time = atoi( *tmp );
			}
			else {
				invalid( *tmp );
			}
			break;

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

	    case 'x':
			if( strncmp("-x509proxy", *tmp, strlen(*tmp)) ) {
				invalid( *tmp );
			} 
			tmp++;
			if( ! (tmp && *tmp) ) {
				another( "-x509proxy" );
			}
			proxy_file = *tmp;
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
	} else if( claim_id ) {
			// This is the last resort, because claim ids are
			// no longer considered to be the correct place to
			// get the startd's address.
		target = getAddrFromClaimId( claim_id );
	} else { 
			// local startd
		target = NULL;
	}

	if( cmd == CA_ACTIVATE_CLAIM && ! (job_keyword || jobad_path) ) { 
		fprintf( stderr,
				 "ERROR: You must specify -keyword or -jobad for %s\n",
				 my_name );
		usage( my_name );
	}

	if (cmd == DELEGATE_GSI_CRED_STARTD && !proxy_file) {
		proxy_file = get_x509_proxy_filename();
		if (!proxy_file) {
			fprintf( stderr,
					 "\nERROR: can't determine proxy filename to delegate\n" );
			exit(1);
		}
	}

	if( jobad_path ) {
		if( ! strcmp(jobad_path, "-") ) {
			JOBAD_PATH = stdin;
		} else {
			JOBAD_PATH = safe_fopen_wrapper_follow( jobad_path, "r" );
			if( !JOBAD_PATH ) {
				fprintf( stderr,
						 "ERROR: failed to open '%s': errno %d (%s)\n",
						 jobad_path, errno, strerror(errno) );
				exit( 1 );
			}
		}
	}

	if( classad_path ) { 
		CA_PATH = safe_fopen_wrapper_follow( classad_path, "w" );
		if( !CA_PATH ) {
			fprintf( stderr, 
					 "ERROR: failed to open '%s': errno %d (%s)\n",
					 classad_path, errno, strerror(errno) );
			exit( 1 );
		}
	}
}


void
printCmd( int command )
{
	switch( command ) {
	case CA_REQUEST_CLAIM:
		fprintf( stderr, "   request\t\tCreate a new COD claim\n" );
		break;
	case CA_ACTIVATE_CLAIM:
		fprintf( stderr, 
				 "   activate\t\tStart a job on a given claim\n" );
		break;
	case CA_DEACTIVATE_CLAIM:
		fprintf( stderr, "   deactivate\t\tKill the current job, but "
				 "keep the claim\n" );
		break;
	case CA_RELEASE_CLAIM:
		fprintf( stderr, "   release\t\tRelinquish a claim, and kill "
				 "any running job\n" ); 
		break;
	case CA_SUSPEND_CLAIM:
		fprintf( stderr, "   suspend\t\tSuspend the job on a given claim\n" ); 
		break;
	case CA_RESUME_CLAIM:
		fprintf( stderr, "   resume\t\tResume the job on a given claim\n" ); 
		break;
	case CA_RENEW_LEASE_FOR_CLAIM:
		fprintf( stderr, "   renew\t\tRenew the lease for this claim\n");
		break;
	case DELEGATE_GSI_CRED_STARTD:
		fprintf( stderr, "   delegate_proxy\tDelegate an x509 proxy to the "
				 "claim\n" );
		break;
	}
}
		

void
printFast( void ) 
{
	fprintf( stderr, "   -fast\t\tQuickly kill any running job\n" );
}


void
usage( const char *str, int iExitCode )
{
	bool has_cmd_opt = true;
	bool needsID = true;

	if( ! str ) {
		fprintf( stderr, "Use \"-help\" to see usage information\n" );
		exit( iExitCode );
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
		printCmd( CA_RENEW_LEASE_FOR_CLAIM );
		printCmd( DELEGATE_GSI_CRED_STARTD );
		fprintf( stderr, "use %s [command] -help for more "
				 "information on a given command\n", str ); 
		exit( iExitCode );
	}

	switch( cmd ) {
	case CA_REQUEST_CLAIM:
		needsID = false;
		break;
	case CA_SUSPEND_CLAIM:
	case CA_RESUME_CLAIM:
		has_cmd_opt = false;
		break;
	}

	fprintf( stderr, "Usage: %s %s[target] [general-opts]%s\n", str,
			 needsID ? " [claimid] " : "",
			 has_cmd_opt ? " [command-opts]" : "");

	printCmd( cmd );
	
	if( needsID ) { 
		fprintf( stderr, "\nWhere [claimid] must be specified as:\n");
		fprintf( stderr, "   -id ClaimId\t\tAct on the given COD claim\n" );
		fprintf( stderr, "   (The startd address may be inferred from this in most cases, but it is better\n"
				         "to specify the address explicitly.\n");
	}

	fprintf( stderr, "\nWhere [target] can be zero or one of:\n" );
	fprintf( stderr, "   -name hostname\tContact the startd on the "
			 "given host\n" ); 
	fprintf( stderr, "   -pool hostname\tUse the given central manager "
			 "to find the startd\n\t\t\trequested with -name\n" );
	fprintf( stderr, "   -addr <ip_addr:port>\tContact the startd at " 
			 "the given \"sinful string\"\n" );
	fprintf( stderr, "   (If no target or claimid is specified, the local "
			 "host is used)\n" );

	fprintf( stderr, "\nWhere [general-opts] can either be one of:\n" );
	fprintf( stderr, "   -help\t\tPrint this usage information and exit\n" );
	fprintf( stderr, "   -version\t\tPrint the version of this tool and "
			 "exit\n" );
	fprintf( stderr, " or it may be zero or more of:\n" );
	fprintf( stderr, "   -debug\t\tPrint verbose debugging information\n" );
	fprintf( stderr, "   -classad file\tPrint the reply ClassAd to "
			 "the given file\n" );
	fprintf( stderr, "   -timeout N\t\tTimeout all network "
			 "operations after N seconds\n" );

	if( has_cmd_opt ) {
		fprintf( stderr,
 				 "\nWhere [command-opts] can be zero or more of:\n" );
	}

	switch( cmd ) {

	case CA_REQUEST_CLAIM:
		fprintf( stderr, "   -requirements expr\tFind a resource "
				 "that matches the boolean expression\n" );
		fprintf( stderr, "   -lease N\t\tNumber of seconds to wait for lease renewal\n");
		break;

	case CA_ACTIVATE_CLAIM:
		fprintf( stderr, "   -keyword string\tUse the keyword to "
				 "find the job in the config file\n" );
		fprintf( stderr, "   -jobad filename\tUse the ClassAd in filename "
				 "to define the job\n" );
		fprintf( stderr, "\t\t\t(if filename is \"-\", read from STDIN)\n" );
		fprintf( stderr, "   -cluster N\t\tStart the job with the "
				 "given cluster ID\n" );
		fprintf( stderr, "   -proc N\t\tStart the job with the "
				 "given proc ID\n" );
		fprintf( stderr, "   -requirements expr\tFind a starter "
				 "that matches the boolean expression\n" );
		break;

	case CA_RELEASE_CLAIM:
	case CA_DEACTIVATE_CLAIM:
		printFast();
		break;

	case DELEGATE_GSI_CRED_STARTD:
		fprintf( stderr, "   -x509proxy filename\tDelegate the proxy in the "
				 "specified file\n" );
		break;
	}

	fprintf(stderr, "\n" );
	exit( iExitCode );
}


