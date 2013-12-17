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
  drain command-line tool
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
#include "basename.h"
#include "match_prefix.h"

// Global variables
int cmd = 0;
char* pool = NULL;
char* target = NULL;
char* my_name = NULL;
int how_fast = DRAIN_GRACEFUL;
bool resume_on_completion = false;
char *cancel_request_id = NULL;
char *draining_check_expr = NULL;

// pass the exit code through dprintf_SetExitCode so that it knows
// whether to print out the on-error buffer or not.
#define exit(n) (exit)((dprintf_SetExitCode(n==1), n))

// protoypes of interest
void usage( const char* );
void version( void );
void invalid( const char* opt );
void ambiguous( const char* opt );
void another( const char* opt );
void parseArgv( int argc, char* argv[] );

/*********************************************************************
   main()
*********************************************************************/

int
main( int argc, char *argv[] )
{

	myDistro->Init( argc, argv );

	config();
	dprintf_config_tool_on_error(0);
	dprintf_OnExitDumpOnErrorBuffer(stderr);

	parseArgv( argc, argv );

	DCStartd startd( target, pool );

	if( ! startd.locate() ) {
		fprintf( stderr, "ERROR: %s\n", startd.error() );
		exit( 1 );
	}

	bool rval = false;

	if( cmd == DRAIN_JOBS ) {
		std::string request_id;
		rval = startd.drainJobs( how_fast, resume_on_completion, draining_check_expr, request_id );
		if( rval ) {
			printf("Sent request to drain %s\n",startd.name());
		}
	}
	else if( cmd == CANCEL_DRAIN_JOBS ) {
		rval = startd.cancelDrainJobs( cancel_request_id );
		if( rval ) {
			printf("Sent request to cancel draining on %s\n",startd.name());
		}
	}

	if( ! rval ) {
		fprintf( stderr, "Attempt to send %s to startd %s failed\n%s\n",
				 getCommandString(cmd), startd.addr(), startd.error() ); 
		return 1;
	}

	dprintf_SetExitCode(0);
	return 0;
}



/*********************************************************************
   Helper functions used by main()
*********************************************************************/


/*********************************************************************
   Helper functions to parse the command-line 
*********************************************************************/

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
parseArgv( int argc, char* argv[] )
{
	int i;

	my_name = strdup(argv[0]);
	cmd = DRAIN_JOBS;

	for( i=1; i<argc; i++ ) {
		if( match_prefix( argv[i], "-help" ) ) {
			usage(my_name);
		}
		else if( match_prefix( argv[i], "-version" ) ) {
			version();
		}
		else if( match_prefix( argv[i], "-pool" ) ) {
			if( i+1 >= argc ) another(argv[i]);
			pool = strdup(argv[++i]);
		}
		else if( match_prefix( argv[i], "-cancel" ) ) {
			cmd = CANCEL_DRAIN_JOBS;
		}
		else if( match_prefix( argv[i], "-fast" ) ) {
			how_fast = DRAIN_FAST;
		}
		else if( match_prefix( argv[i], "-quick" ) ) {
			how_fast = DRAIN_QUICK;
		}
		else if( match_prefix( argv[i], "-graceful" ) ) {
			how_fast = DRAIN_GRACEFUL;
		}
		else if( match_prefix( argv[i], "-resume-on-completion" ) ) {
			resume_on_completion = true;
		}
		else if( match_prefix( argv[i], "-request-id" ) ) {
			if( i+1 >= argc ) another(argv[i]);
			cancel_request_id = strdup(argv[++i]);
		}
		else if( match_prefix( argv[i], "-check" ) ) {
			if( i+1 >= argc ) another(argv[i]);
			draining_check_expr = strdup(argv[++i]);
		}
        else if( argv[i][0] != '-' ) {
            break;
        }
		else {
			fprintf(stderr,"ERROR: unexpected argument: %s\n", argv[i]);
			exit(2);
		}
	}

    if( i != argc-1 ) {
        fprintf(stderr,"ERROR: must specify one target machine\n");
        exit(2);
    }

	target = strdup(argv[i]);

	if( cmd == DRAIN_JOBS ) {
		if( cancel_request_id ) {
			fprintf(stderr,"ERROR: -request-id may only be used with -cancel\n");
			exit(2);
		}
	}
	if( cmd == CANCEL_DRAIN_JOBS ) {
		if( draining_check_expr ) {
			fprintf(stderr,"ERROR: -check may not be used with -cancel\n");
			exit(2);
		}
	}
}

void
usage( const char *str )
{
	if( ! str ) {
		fprintf( stderr, "Use \"-help\" to see usage information\n" );
		exit( 1 );
	}
	fprintf( stderr, "Usage: %s [OPTIONS] machine\n", str );
	fprintf( stderr, "\nOPTIONS:\n" );
	fprintf( stderr, "-cancel          Stop draining.\n" );
	fprintf( stderr, "-graceful         (the default) Honor MaxVacateTime and MaxJobRetirementTime.\n" );
	fprintf( stderr, "-quick            Honor MaxVacateTime but not MaxJobRetirementTime.\n" );
	fprintf( stderr, "-fast             Honor neither MaxVacateTime nor MaxJobRetirementTime.\n" );
	fprintf( stderr, "-resume-on-completion    When done draining, resume normal operation.\n" );
	fprintf( stderr, "-request-id <id>  Specific request id to cancel (optional).\n" );
	fprintf( stderr, "-check <expr>     Must be true for all slots to be drained or request is aborted.\n" );
	exit( 1 );
}
