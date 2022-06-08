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
#include "internet.h"
#include "daemon.h"
#include "dc_startd.h"
#include "dc_collector.h"
#include "basename.h"
#include "match_prefix.h"

// Global variables
int cmd = 0;
const char* pool = NULL;
const char* target = NULL;
const char* my_name = NULL;
int how_fast = DRAIN_GRACEFUL;
int on_completion = DRAIN_NOTHING_ON_COMPLETION;
const char * drain_reason = NULL;
const char *cancel_request_id = NULL;
const char *draining_check_expr = NULL;
const char *draining_start_expr = NULL;
int dash_verbose = 0;

// pass the exit code through dprintf_SetExitCode so that it knows
// whether to print out the on-error buffer or not.
#define exit(n) (exit)((dprintf_SetExitCode(n==1), n))

// protoypes of interest
void usage( const char* );
void version( void );
void invalid( const char* opt );
void ambiguous( const char* opt );
void another( const char* opt );
void conflict(const char * opt, int completion);
const char * on_completion_name(int completion);
void parseArgv( int argc, const char* argv[] );

/*********************************************************************
   main()
*********************************************************************/

int
main( int argc, const char *argv[] )
{

	set_priv_initialize(); // allow uid switching if root
	config();
	dprintf_config_tool_on_error(0);
	dprintf_OnExitDumpOnErrorBuffer(stderr);

	parseArgv( argc, argv );

	DCStartd startd( target, pool );

	if( ! startd.locate(Daemon::LOCATE_FOR_ADMIN) ) {
		fprintf( stderr, "ERROR: %s\n", startd.error() );
		exit( 1 );
	}

	// -exit-on-completion and -restart-on-completion only work with startd's that are 8.9.12 or later
	if ((on_completion > DRAIN_RESUME_ON_COMPLETION) && startd.version()) {
		CondorVersionInfo verinfo(startd.version());
		if ( ! verinfo.built_since_version(8,9,12)) {
			fprintf(stderr, "ERROR: %s does not support %s\n%s\n",
				startd.name(), on_completion_name(on_completion), startd.version());
			exit(1);
		}
	}

	bool rval = false;

	if( cmd == DRAIN_JOBS ) {
		std::string request_id;
		rval = startd.drainJobs( how_fast, drain_reason, on_completion, draining_check_expr, draining_start_expr, request_id );
		if( rval ) {
			printf("Sent request to drain the startd %s with %s. This only "
				"affects the single startd; any other startds running on the "
				"same host will not be drained.\n", startd.addr(), 
				startd.name());
			if (dash_verbose && ! request_id.empty()) { printf("\tRequest id: %s\n", request_id.c_str()); }
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

const char * on_completion_name(int completion)
{
	const char * const opts[] = { "-resume-on-completion", "-exit-on-completion", "-restart-on-completion", "on-completion" };
	int ix = completion - 1;
	if (ix < 0 || ix > 3) ix = 3;
	return opts[ix];
}

void
conflict(const char * opt, int completion)
{
	fprintf(stderr, "%s: '%s' conflicts with previous use of '%s'\n", my_name, opt, on_completion_name(completion));
	usage(my_name);
}

void
parseArgv( int argc, const char* argv[] )
{
	int i;

	my_name = argv[0];
	cmd = DRAIN_JOBS;

	for( i=1; i<argc; i++ ) {
		if (is_dash_arg_prefix(argv[i], "help", 1)) {
			usage(my_name);
		}
		else if (is_dash_arg_prefix(argv[i], "version", 1)) {
			version();
		}
		else if (is_dash_arg_prefix(argv[i], "verbose", 4)) {
			dash_verbose = 1;
		}
		else if (is_dash_arg_prefix( argv[i], "pool", 1)) {
			if( i+1 >= argc ) another(argv[i]);
			pool = argv[++i];
		}
		else if (is_dash_arg_prefix(argv[i], "cancel", 1)) {
			cmd = CANCEL_DRAIN_JOBS;
		}
		else if (is_dash_arg_prefix(argv[i], "fast", 1)) {
			how_fast = DRAIN_FAST;
		}
		else if (is_dash_arg_prefix(argv[i], "quick", 1)) {
			how_fast = DRAIN_QUICK;
		}
		else if (is_dash_arg_prefix(argv[i], "graceful", 1)) {
			how_fast = DRAIN_GRACEFUL;
		}
		else if (is_dash_arg_prefix(argv[i], "resume-on-completion", 1)) {
			if (on_completion) conflict(argv[i], on_completion);
			on_completion = DRAIN_RESUME_ON_COMPLETION;
		}
		else if (is_dash_arg_prefix(argv[i], "exit-on-completion", 2)) {
			if (on_completion) conflict(argv[i], on_completion);
			on_completion = DRAIN_EXIT_ON_COMPLETION;
		}
		else if (is_dash_arg_prefix(argv[i], "restart-on-completion", 4)) {
			if (on_completion) conflict(argv[i], on_completion);
			on_completion = DRAIN_RESTART_ON_COMPLETION;
		}
		else if (is_dash_arg_prefix(argv[i], "reason", 3)) {
			if (i+1 >= argc) another(argv[i]);
			drain_reason = argv[++i];
		}
		else if (is_dash_arg_prefix( argv[i], "request-id", 3)) {
			if( i+1 >= argc ) another(argv[i]);
			cancel_request_id = argv[++i];
		}
		else if (is_dash_arg_prefix(argv[i], "check", 2)) {
			if( i+1 >= argc ) another(argv[i]);
			draining_check_expr = argv[++i];
		}
		else if( is_dash_arg_prefix( argv[i], "start", 5 ) ) {
			if( i+1 >= argc ) another(argv[i]);
			draining_start_expr = argv[++i];
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

	target = argv[i];

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
		if (on_completion) {
			fprintf(stderr, "ERROR: cannot use an -on-completion option with -cancel\n");
			exit(2);
		}
		if (drain_reason) {
			fprintf(stderr, "ERROR: cannot use -reason with -cancel\n");
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
	fprintf( stderr, "-cancel           Stop draining.\n" );
	fprintf( stderr, "-graceful         (the default) Honor MaxVacateTime and MaxJobRetirementTime.\n" );
	fprintf( stderr, "-quick            Honor MaxVacateTime but not MaxJobRetirementTime.\n" );
	fprintf( stderr, "-fast             Honor neither MaxVacateTime nor MaxJobRetirementTime.\n" );
	fprintf( stderr, "-reason <text>    While draining, advertise <text> as the reason.\n" );
	fprintf( stderr, "-resume-on-completion    When done draining, resume normal operation.\n" );
	fprintf( stderr, "-exit-on-completion      When done draining, STARTD should exit and not restart.\n" );
	fprintf( stderr, "-restart-on-completion   When done draining, STARTD should restart.\n" );
	fprintf( stderr, "-request-id <id>  Specific request id to cancel (optional).\n" );
	fprintf( stderr, "-check <expr>     Must be true for all slots to be drained or request is aborted.\n" );
	fprintf( stderr, "-start <expr>     Change START expression to this while draining.\n" );
	exit( 1 );
}
