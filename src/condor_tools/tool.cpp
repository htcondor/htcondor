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
  Generic condor tool.
*********************************************************************/

#include "condor_common.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "condor_version.h"
#include "condor_io.h"
#include "basename.h"
#include "my_hostname.h"
#include "get_daemon_name.h"
#include "internet.h"
#include "daemon.h"
#include "dc_collector.h"
#include "daemon_types.h"
#include "sig_install.h"
#include "command_strings.h"
#include "match_prefix.h"
#include "condor_distribution.h"
#include "condor_query.h"
#include "daemon_list.h"
#include "my_username.h"


void computeRealAction( void );
bool resolveNames( std::vector<Daemon>& daemon_list, std::vector<std::string>* name_list, std::vector<std::string>* unresolved_names_list );
void doCommand( Daemon& d );
int doCommands(int argc,const char *argv[],const char *MyName, std::vector<std::string> & unresolved_names);
void version();
void handleAll();
void doSquawk( const char *addr );
int handleSquawk( char *line, char *addr );
int doSquawkReconnect( char *addr );
bool printAdToFile(ClassAd *ad, char* filename);
int strncmp_auto(const char *s1, const char *s2);
void PREFAST_NORETURN usage( const char *str, int iExitCode=1, FILE* out=nullptr );

// Global variables
int cmd = 0;
int real_cmd = 0;
daemon_t dt = DT_NONE;
daemon_t real_dt = DT_NONE;
DCCollector* pool = NULL;
bool fast = false;
bool peaceful_shutdown = false;
bool force_shutdown = false;
bool full = false;
bool all = false;
const char* constraint = NULL;
std::string annexString;
const char* subsys = NULL;
const char* exec_program = NULL;
bool can_exec = false;
bool can_drain = false;
bool dash_drain = false;
bool dash_quick = false;
const char * drain_reason = nullptr;
const char * draining_check_expr = nullptr;
const char * draining_start_expr = nullptr;
bool dash_fast = false;
bool dash_graceful = false;
bool dash_peaceful = false;

int takes_subsys = 0;
int cmd_set = 0;
const char *subsys_arg = NULL;
bool IgnoreMissingDaemon = false;

bool all_good = true;

std::set<std::string> addresses_sent;

static const char * drain_cmd_verb(int cmd)
{
	switch (cmd) {
	case SET_SHUTDOWN_PROGRAM: return "shutdown";
	case DAEMONS_OFF: return "shutdown";
	case DC_RECONFIG_FULL: return "reconfig";
	case RESTART: return "restart";
	default: break;
	}
	return "proceeding";
}

void
usage( const char *myname, int iExitCode, FILE* out )
{
	if ( ! out) out = stderr;
	if ( ! myname ) {
		fprintf( out, "Use \"-help\" to see usage information\n" );
		exit( 1 );
	}

	if( !strchr( myname, '_' ) ) {
		fprintf( out, "Usage: %s [command] ", myname );
	} else if (cmd == SET_SHUTDOWN_PROGRAM) {
		fprintf( out, "Usage: %s -exec <name> ", myname );
	} else {
		fprintf( out, "Usage: %s ", myname );
	}

	fprintf( out, "[general-options]%s [targets]%s\n",
		can_drain ? " [drain-options]" : "",
		takes_subsys ? " [daemon]" : "");

	fprintf( out, "\nwhere [general-options] can be zero or more of:\n" );
	fprintf( out, "    -help\t\tgives this usage information\n" );
	fprintf( out, "    -version\t\tprints the version\n" );
	fprintf( out, "    -pool <hostname>\tuse the given central manager to find daemons\n" );
	if (can_exec) {
		fprintf( out, "    -exec <name>\tTell the master to run the program is has configured as\n"
				      "                \tMASTER_SHUTDOWN_<name> on next %s\n", drain_cmd_verb(cmd));
	}
	if( cmd == DAEMONS_OFF || cmd == DAEMON_OFF || cmd == RESTART ) {
		fprintf( out, "    -graceful\t\tThe default. If jobs are running, wait for up to the configured \n\t\t\tgrace period for them to finish, then exit\n");
		fprintf( out, "    -fast\t\tquickly shutdown daemons, immediately evicting any running jobs\n" );
		fprintf( out, "    -peaceful\t\twait indefinitely for jobs to finish before shutdown\n" );
		fprintf( out, "    -force-graceful\tupgrade a peaceful shutdown to a graceful shutdown\n" );
	}
	if( cmd == VACATE_CLAIM ) {
		fprintf( out,
				 "    -graceful\t\tgracefully vacate the jobs (the default)\n" );
		fprintf( out,
				 "    -fast\t\tquickly vacate the jobs (no checkpointing)\n" );
	}
	if (can_drain) {
		fprintf( out, "    -drain\t\tdrain before %s. (target must be 9.12 or later)\n", drain_cmd_verb(cmd) );
		fprintf( out, "\nwhere [drain-options] modify -drain and are zero or more of: \n"
			"    -reason <text>    While draining, advertise <text> as the reason.\n"
			"    -request-id <id>  Specific request id to cancel (optional).\n"
			"    -check <expr>     Must be true for all slots to be drained or request is aborted.\n"
			"    -start <expr>     Change START expression to this while draining.\n");
		fprintf( out,
			"    -deadline <expr>  How long to drain before %s\n", drain_cmd_verb(cmd));
	}
	fprintf( out, "\nwhere [targets] can be zero or more of:\n" );
	fprintf( out, 
			 "    -all\t\tall hosts in your pool (overrides other targets)\n" );
	fprintf( out,
			 "    -annex-name <name>\tall annex hosts in the named annex\n" );
	fprintf( out,
			 "    -annex-slots\tall annex hosts in your pool\n" );
	fprintf( out, "    hostname\t\tgiven host\n" );
	fprintf( out, "    <ip.address:port>\tgiven \"sinful string\"\n" );
	fprintf( out,
			 "  (for compatibility with other Condor tools, you can also use:)\n" );
	fprintf( out, "    -name name\tgiven host\n" );
	fprintf( out, "    -constraint constraint\tconstraint\n" );
	fprintf( out, "    -addr <addr:port>\tgiven \"sinful string\"\n" );
	fprintf( out, "  (if no targets are specified, the local host is used)\n" );
	if( takes_subsys ) {
		fprintf( out, "\nwhere [daemon] can be one of:\n" );
		fprintf( out,
			 "    -daemon <name>\tspecify the target daemon by name.\n" );
		fprintf( out,
			"    Or use one of the following options to select one of the standard daemons\n");
		if( cmd == DAEMONS_OFF || cmd == DAEMON_OFF ) {
			fprintf( out, "    -master\n" );
		} else {
			fprintf( out, "    -master\t\t(the default)\n" );
		}
		fprintf( out, "    -startd\n" );
		fprintf( out, "    -schedd\n" );
		fprintf( out, "    -collector\n" );
		fprintf( out, "    -negotiator\n" );
		fprintf( out, "    -kbdd\n" );
	}
	fprintf( out, "\n" );

	switch( cmd ) {
	case DAEMONS_ON:
		fprintf( out,
				 "  %s turns on the condor daemons specified in the config file.\n", 
				 myname);
		break;
	case DAEMONS_OFF:
	case DC_OFF_GRACEFUL:
		fprintf( out, "  %s turns off the specified daemon.\n", 
				 myname );
		fprintf( out,
				 "  If no daemon is given, everything except the master is shut down.\n" );
		break;
	case RESTART:
		fprintf( out, "  %s causes specified daemon to restart itself.\n", myname );
		fprintf( out,
				 "  If sent to the master, all daemons on that host will restart.\n" );
		break;
	case SET_SHUTDOWN_PROGRAM:
		fprintf( out, "  %s causes the master to run a pre-configured program when it exits.\n", myname );
		break;

	case DC_RECONFIG_FULL:
		fprintf( out,
				 "  %s causes the specified daemon to reconfigure itself.\n", 
				 myname );
		fprintf( out,
				 "  If sent to the master, all daemons on that host will reconfigure.\n" );
		break;
	case RESCHEDULE:
		fprintf( out, "  %s %s\n  %s\n", myname, 
				 "causes the condor_schedd to update the central manager",
				 "and initiate a new negotiation cycle." );
		break;
	case VACATE_CLAIM:
		fprintf( out,
				 "  %s causes the condor_startd to vacate the running\n"
				 "  job(s) on specific machines.  If you specify a slot\n"
				 "  (for example, \"slot1@hostname\"), only that slot will be\n"
				 "  vacated.  If you specify just a hostname, all jobs running under\n"
				 "  any slots at that host will be vacated.  By default,\n"
				 "  the jobs will be checkpointed (if possible), though if you\n"
				 "  specify the -fast option, they will be immediately killed.\n",
				 myname );
		break;
	default:
		fprintf( out, "  Valid commands are:\n%s%s",
				 "\toff, on, restart, reconfig, reschedule, ",
				 "vacate, checkpoint, set_shutdown\n\n" );
		fprintf( out, "  Use \"%s [command] -help\" for more information %s\n",
				 myname, "on a given command." );
		break;
	}
	fprintf(out, "\n" );
	exit( iExitCode );
}


void
pool_target_usage( void )
{
	fprintf( stderr, "ERROR: You have asked to find a machine in "
			 "another pool (with\n"
			 "the -pool option) but you have not specified which machine.\n"
			 "Please also use -addr, -name, or list the name(s).\n"
			 "For more information, use -help.\n" );
	exit( 1 );
}


const char*
cmdToStr( int c )
{
	switch( c ) {
	case DAEMONS_OFF:
		return "Kill-All-Daemons";
	case DAEMONS_OFF_FAST:
		return "Kill-All-Daemons-Fast";
	case DAEMONS_OFF_PEACEFUL:
		return "Kill-All-Daemons-Peacefully";
	case DAEMONS_OFF_FLEX:
	case MASTER_OFF:
		return "Shutdown-Daemons";
	case DAEMON_OFF:
	case DC_OFF_GRACEFUL:
	case DC_OFF_FORCE:
		return "Kill-Daemon";
	case DAEMON_OFF_FAST:
	case DC_OFF_FAST:
		return "Kill-Daemon-Fast";
	case DAEMON_OFF_PEACEFUL:
	case DC_OFF_PEACEFUL:
		return "Kill-Daemon-Peacefully";
	case DC_SET_PEACEFUL_SHUTDOWN:
		return "Set-Peaceful-Shutdown";
	case DC_SET_FORCE_SHUTDOWN:
		return "Set-Force-Shutdown";
	case DAEMONS_ON:
		return "Spawn-All-Daemons";
	case DAEMON_ON:
		return "Spawn-Daemon";
	case RESTART:
		if( fast ) {
			return "Restart-Fast";
		} else if( peaceful_shutdown ) {
			return "Restart-Peaceful";
		} else {
			return "Restart";
		}
	case VACATE_CLAIM:
		return "Vacate-Claim";
	case VACATE_CLAIM_FAST:
		return "Vacate-Claim-Fast";
	case VACATE_ALL_CLAIMS:
		return "Vacate-All-Claims";
	case VACATE_ALL_FAST:
		return "Vacate-All-Claims-Fast";
	case RESCHEDULE:
		return "Reschedule";
	case DC_RECONFIG_FULL:
		return "Reconfig";
	case SET_SHUTDOWN_PROGRAM:
		return "Set-Shutdown-Program";
	}
	fprintf( stderr, "Unknown Command (%d) in cmdToStr()\n", c );
	exit(1);

	return "UNKNOWN";	// to make C++ happy
}
	

void
subsys_check( const char* MyName )
{
	if( ! takes_subsys ) {
		fprintf( stderr, 
				 "ERROR: Can't specify a daemon flag with %s.\n",  
				 MyName );
		usage( MyName );
	}
	if( dt ) {
		fprintf( stderr, "ERROR: can only specify one daemon flag.\n" );
		usage( MyName );
	}
	subsys = (char*)1;
}

bool skipAfterAnnex = false;

static void
another(  const char* opt )
{
	fprintf( stderr, "ERROR: %s requires another argument\n", opt );
	usage( NULL );
}

int
main( int argc, const char *argv[] )
{
	int rc;
	int got_name_or_addr = 0;
	std::vector<std::string> unresolved_names;
	const char * pcolon;

#ifndef WIN32
	// Ignore SIGPIPE so if we cannot connect to a daemon we do not
	// blowup with a sig 13.
	install_sig_handler(SIGPIPE, SIG_IGN );
#endif

	set_priv_initialize(); // allow uid switching if root
	config();

	const char *MyName = condor_basename(argv[0]);

		// See if there's an '-' in our name, if not, append argv[1]. 
	const char *cmd_str = strchr( MyName, '_');
	if( !cmd_str ) {
			// If argv[1] is version, we know what to do, otherwise print usage
		if (argv[1] && is_dash_arg_prefix(argv[1], "version")) {
			version(); // this calls exit(0)
		}
		usage( MyName );
	}

		// Figure out what kind of tool we are.
        // We use strncmp instead of strcmp because 
        // we want to work on windows when invoked as 
        // condor_reconfig.exe, not just condor_reconfig
	if( !strncmp_auto( cmd_str, "_reconfig" ) ) {
		cmd = DC_RECONFIG_FULL;
		takes_subsys = 1;
		can_drain = 1;
	} else if( !strncmp_auto( cmd_str, "_restart" ) ) {
		cmd = RESTART;
		takes_subsys = 1;
		can_drain = 1;
		can_exec = 1; // TODO: make this work
	} else if( !strncmp_auto( cmd_str, "_off" ) ) {
		cmd = DAEMONS_OFF;
		takes_subsys = 1;
		can_drain = 1;
		can_exec = 1; // TODO: make this work
	} else if( !strncmp_auto( cmd_str, "_on" ) ) {
		cmd = DAEMONS_ON;
		takes_subsys = 1;
	} else if( !strncmp_auto( cmd_str, "_reschedule" ) ) {
		cmd = RESCHEDULE;
	} else if( !strncmp_auto( cmd_str, "_vacate" ) ) {
		cmd = VACATE_CLAIM;
	} else if ( !strncmp_auto( cmd_str, "_set_shutdown" ) ) {
		cmd = SET_SHUTDOWN_PROGRAM;
		can_exec = 1;
	} else {
		fprintf( stderr, "ERROR: unknown command %s\n", MyName );
		usage( "condor" );
	}

	std::vector<const char *> names_and_addrs; names_and_addrs.reserve(argc+1);

		// First, deal with options (begin with '-')
	for (int i = 1; i < argc; ++i) {
		if (*argv[i] != '-') {
				// If it doesn't start with '-', skip it
			++got_name_or_addr;
			names_and_addrs.push_back(argv[i]);
			continue;
		}
		if (is_dash_arg_prefix(argv[i], "version")) {
			version();
		}
		else if (is_dash_arg_prefix(argv[i], "help")) {
			usage( MyName, 0 );
		}
		else if (is_dash_arg_prefix(argv[i], "peaceful", 2)) {
				dash_peaceful = true;
				peaceful_shutdown = true;
				fast = false;
				force_shutdown = false;
				switch( cmd ) {
				case DAEMONS_OFF:
				case DC_OFF_GRACEFUL:
				case RESTART:
					break;
				default:
					fprintf( stderr, "ERROR: \"-peaceful\" "
							 "is not valid with %s\n", MyName );
					usage( NULL );
				}
		}
		else if (is_dash_arg_prefix(argv[i], "pool", 1)) {
				if (argv[i+1]) {
					pool = new DCCollector(argv[++i]);
					if( ! pool->addr() ) {
						fprintf( stderr, "%s: %s\n", MyName, pool->error() );
						exit( 1 );
					}
				} else {
					fprintf( stderr, "ERROR: -pool requires another argument\n" );
					usage( NULL );
				}
		}
		else if (is_dash_arg_prefix(argv[i], "preen", -1)) {
				subsys_check(MyName);
				dt = DT_GENERIC;
				subsys_arg = "preen";
		}
		else if (is_dash_arg_prefix(argv[i], "full", 2)) {
					// CRUFT: -full is a deprecated argument to
					//   condor_reconfig. It was removed in 7.5.3.
					if( cmd != DC_RECONFIG_FULL ) {
						fprintf( stderr, "ERROR: \"-full\" "
								 "is not valid with %s\n", MyName );
						usage( NULL );
					}
		}
		else if (is_dash_arg_prefix(argv[i], "fast", 2)) {
					dash_fast = true;
					fast = true;
					peaceful_shutdown = false;
					force_shutdown = false;
					switch( cmd ) {
					case DAEMONS_OFF:
					case DC_OFF_GRACEFUL:
					case RESTART:
					case VACATE_CLAIM:
						break;
					default:
						fprintf( stderr, "ERROR: \"-fast\" "
								 "is not valid with %s\n", MyName );
						usage( NULL );
					}
		}
		else if (is_dash_arg_prefix(argv[i], "force-graceful", 2)) {
					fast = false;
					peaceful_shutdown = false;
					force_shutdown = true;
					switch( cmd ) {
					case DAEMONS_OFF:
						break;
					default:
						fprintf( stderr, "ERROR: \"-force-graceful\" "
								 "is not valid with %s\n", MyName );
						usage( NULL );
					}
		}
		else if (is_dash_arg_prefix(argv[i], "f", -1)) {
				fprintf( stderr, 
						 "ERROR: ambiguous parameter: \"-f\"\n"
						 "Please specify \"-full\" or \"-fast\"\n" );
				usage( NULL );
		}
		else if (is_dash_arg_colon_prefix(argv[i], "debug", &pcolon, 1)) {
				dprintf_set_tool_debug("TOOL", (pcolon && pcolon[1]) ? pcolon+1 : nullptr);
		}
		else if (is_dash_arg_prefix(argv[i], "daemon", 2)) {
				subsys_check( MyName );
					// We got a "-daemon", make sure we've got 
					// something else after it
				if (argv[i+1]) {
					subsys_arg = argv[++i];
					dt = stringToDaemonType(subsys_arg);
					if( dt == DT_NONE ) {
						dt = DT_GENERIC;
					}
				} else {
					fprintf( stderr, 
							 "ERROR: -daemon requires another argument\n" ); 
					usage( NULL );
					exit( 1 );
				}
		}
		else if (is_dash_arg_prefix(argv[i], "drain", 2) && can_drain) {
			dash_drain = true;
		}
		else if (is_dash_arg_prefix(argv[i], "quick", 1) && can_drain) {
			dash_quick = true;
		}
		else if (is_dash_arg_prefix(argv[i], "reason", 3) && dash_drain) {
			if (i+1 >= argc) another(argv[i]);
			drain_reason = argv[++i];
		}
		else if (is_dash_arg_prefix(argv[i], "check", 2) && dash_drain) {
			if( i+1 >= argc ) another(argv[i]);
			draining_check_expr = argv[++i];
		}
		else if( is_dash_arg_prefix( argv[i], "start", 5 ) && dash_drain) {
			if( i+1 >= argc ) another(argv[i]);
			draining_start_expr = argv[++i];
		}
		else if (is_dash_arg_prefix(argv[i], "exec", 2)) {
			// TODO: allow -exec with condor_off and condor_restart even if -drain is not specified.
			if ( cmd != SET_SHUTDOWN_PROGRAM && ! dash_drain) {
				fprintf( stderr,
						 "ERROR: \"-exec\" is not valid with %s%s\n", MyName,
						dash_drain ? "" : " without -drain"
						);
				usage( NULL );
				break;
			}
			if( i+1 >= argc ) another(argv[i]);
			exec_program = argv[++i];
			printf( "Set exec to %s\n", exec_program );
		}
		else if (is_dash_arg_prefix(argv[i], "graceful", 1)) {
				dash_graceful = true;
				fast = false;
				peaceful_shutdown = false;
		}
		else if (is_dash_arg_prefix(argv[i], "addr", 2)) {
						// We got a -addr, make sure we've got 
						// something else after it
					if (! argv[i+1] || *argv[i+1] == '-') {
						fprintf( stderr, 
								 "ERROR: -addr requires another argument\n" ); 
						usage( NULL );
					}
					++got_name_or_addr;
					names_and_addrs.push_back(argv[++i]);
		}
		else if (is_dash_arg_prefix(argv[i], "all", 2)) {
						// We got a "-all", remember that
					all = true;
		}
		else if (is_dash_arg_prefix(argv[i], "a", -1)) {
				fprintf( stderr, 
						 "ERROR: ambiguous parameter: \"-a\"\n"
						 "Please specify \"-addr\" or \"-all\"\n" );
				usage( NULL );
		}
		else if (starts_with(argv[i], "-annex")) {
					// We got -annex*, all of which default to the master.
					if( cmd == DAEMONS_OFF ) {
						subsys_check( MyName );
						dt = DT_MASTER;
					}

					const char * option = argv[i];
					const char * argument = NULL;
					if( argv[i+1] ) { argument = argv[i+1]; }

					if( strcmp( option, "-annex-name" ) == 0 ) {
						if( argument ) {
							formatstr( annexString, ATTR_ANNEX_NAME " =?= \"%s\"", argument );
							skipAfterAnnex = true;
							++i;
						} else {
							fprintf( stderr, "ERROR: -annex-name requires an annex name\n" );
							usage( NULL );
						}
					} else if( strcmp( option, "-annex-slots" ) == 0 ) {
						annexString = "IsAnnex";
					} else {
						if( argument && argument[0] != '-' ) {
							formatstr( annexString, ATTR_ANNEX_NAME " =?= \"%s\"", argument );
							skipAfterAnnex = true;
							++i;
						} else {
							annexString = "IsAnnex";
						}
					}

					if( constraint && (! annexString.empty()) ) {
						formatstr( annexString, "(%s) && (%s)", constraint, annexString.c_str() );
					}
					constraint = annexString.c_str();
		}
		else if (is_dash_arg_prefix(argv[i], "name", 2)) {
						// We got a "-name", make sure we've got 
						// something else after it
					if (! argv[i+1] || *argv[i+1] == '-') {
						fprintf( stderr, 
								 "ERROR: -name requires another argument\n" );
						usage( NULL );
					}
					++got_name_or_addr;
					names_and_addrs.push_back(argv[++i]);
		}
		else if (is_dash_arg_prefix(argv[i], "negotiator", 2)) {
					subsys_check( MyName );
					dt = DT_NEGOTIATOR;
		}
		else if (is_dash_arg_prefix(argv[i], "n", -1)) {
				fprintf( stderr, 
						 "ERROR: ambiguous parameter: \"-n\"\n"
						 "Please specify \"-name\" or \"-negotiator\"\n" );
				usage( NULL );
		}
		else if (is_dash_arg_prefix(argv[i], "master", 1)) {
			subsys_check( MyName );
			dt = DT_MASTER;
		}
		else if (is_dash_arg_prefix(argv[i], "cmd", 2)) {
						// We got a "-cmd", make sure we've got 
						// something else after it
					if( argv[i+1] ) {
						cmd = atoi( argv[++i] );
						cmd_set = 1;
						if( !cmd ) {
							fprintf( stderr, 
									 "ERROR: invalid argument to -cmd (\"%s\")\n",
									 argv[i] );
							exit( 1 );
						}
					} else {
						fprintf( stderr, 
								 "ERROR: -cmd requires another argument\n" ); 
						exit( 1 );
					}
		}
		else if (is_dash_arg_prefix(argv[i], "constraint", 3)) {
							// We got a "-constraint", make sure we've got
							// something else after it
							if (constraint) {
								fprintf(stderr, "ERROR: only one -constraint argument is allowed\n");
								exit (1);
							}
							if( argv[i+1] ) {
								constraint = argv[++i];
							} else {
								fprintf( stderr, "ERROR: -constraint requires another argument\n" );
								usage( NULL );
							}
		}
		else if (is_dash_arg_prefix(argv[i], "collector", 3)) {
							subsys_check( MyName );
							dt = DT_COLLECTOR;
		}
		else if (is_dash_arg_prefix(argv[i], "co", -1)) {
						fprintf( stderr,
							"ERROR: ambigous parameter: \"-co\"\n"
							"Please specify \"-collector\" or \"-constraint\"\n" );
						usage( NULL );
		}
		else if (is_dash_arg_prefix(argv[i], "kbdd", 1)) {
			subsys_check( MyName );
			dt = DT_KBDD;
		}
		else if (is_dash_arg_prefix(argv[i], "schedd", 2)) {
			subsys_check( MyName );
			dt = DT_SCHEDD;
		}
		else if (is_dash_arg_prefix(argv[i], "startd", 2)) {
			subsys_check( MyName );
			dt = DT_STARTD;
		}
		else if (is_dash_arg_prefix(argv[i], "subsystem", 2)) {
						// We got a "-subsystem", make sure we've got 
						// something else after it
					if( argv[i+1] ) {
						subsys_check( MyName );
						subsys_arg = argv[++i];
						dt = stringToDaemonType(subsys_arg);
						if( dt == DT_NONE ) {
							dt = DT_GENERIC;
						}
					} else {
						fprintf( stderr, 
							 "ERROR: -subsystem requires another argument\n" );
						usage( NULL );
						exit( 1 );
					}
		}
		else if (is_dash_arg_prefix(argv[i], "s", -1)) {
				fprintf( stderr, 
						 "ERROR: ambiguous argument \"-s\"\n"
				"Please specify \"-subsystem\", \"-startd\" or \"-schedd\"\n" );
				usage( NULL );
		}
		else {
			fprintf( stderr, "ERROR: invalid argument \"%s\"\n",
					 argv[i] );
			usage( MyName );
		}
	}

	if (constraint && got_name_or_addr) {
		fprintf (stderr,
			"ERROR: use of -constraint or -annex conflicts with other arguments containing names or addresses.\n"
			"You can change the constraint to select daemons by name by adding\n"
			"  (NAME == \"daemon-name\" || MACHINE == \"daemon-name\")\n"
			"to your constraint.\n");
		usage(NULL);
	}

	// add a null name at the end like argv
	names_and_addrs.push_back(nullptr);

		// it's not always obvious what daemon we want to talk to and
		// what command we want to send.  for example, with
		// condor_off, even if the target daemon is a startd, we
		// really need to send something to the master, instead.  so,
		// this method figures out what kind of daemon we need to
		// communicate with (regardless of what kind of daemon we're
		// going to be acting on) and what command we need to send.
	computeRealAction();

		// While we're parsing command-line args, when we notice a
		// daemon flag, we just set subsys to non-NULL to know we
		// found it, and record the actual daemon with "dt".  Now
		// that we know the true target daemon type for whatever
		// command we're using, want the real string to use.
	if( subsys ) {
		if( (dt == DT_ANY || dt == DT_GENERIC) && subsys_arg ) {
			subsys = subsys_arg; 
		} else { 
			subsys = daemonString( dt );
		}
	}

	// For SET_SHUTDOWN_PROGRAM, we require -exec
	if (  (SET_SHUTDOWN_PROGRAM == cmd) && (NULL == exec_program) ) {
		fprintf( stderr, "ERROR: \"-exec\" required for %s\n", MyName );
		usage( NULL );
	}


	if (takes_subsys) {
		// for these commands, double check that the current configuration files
		// can be parsed by daemons running as 'condor' before running the command
		// note that check_config_file_access does nothing if can_switch_ids() is false.
		std::vector<std::string> errFiles;
		if ( ! check_config_file_access("condor", errFiles)) {
			fprintf(stderr, "ERROR: the following configuration files cannot be read by user 'condor'. Aborting command\n");
			for (auto& file: errFiles) {
				fprintf(stderr, "\t%s\n", file.c_str());
			}
			exit(1);
		}
	}

	// If we are sending peaceful daemon shutdown/restart commands to
	// the master (i.e. we want no timeouts resulting in killing),
	// then we have to deal here with the fact that the master only
	// transmits a graceful (not peaceful) signal to its children.  In
	// anticipation of this, we need to turn on peaceful mode in the
	// relavent children.  Currently, only the startd and schedd have
	// special peaceful behavior.

	if( ! dash_drain && (peaceful_shutdown || force_shutdown ) && real_dt == DT_MASTER ) {
		if( (real_cmd == DAEMONS_OFF) ||
			(real_cmd == DAEMON_OFF && subsys && !strcmp(subsys,"startd")) ||
			(real_cmd == DAEMON_OFF && subsys && !strcmp(subsys,"schedd")) ||
			(real_cmd == DC_OFF_GRACEFUL) || (real_cmd == DC_OFF_FORCE) || 
			(real_cmd == RESTART)) {

			// Temporarily override globals so we can send a different command.
			daemon_t orig_real_dt = real_dt;
			int orig_real_cmd = real_cmd;
			int orig_cmd = cmd;
			bool orig_IgnoreMissingDaemon = IgnoreMissingDaemon;

			if( peaceful_shutdown ) {
				cmd = real_cmd = DC_SET_PEACEFUL_SHUTDOWN;
			} else if( force_shutdown ) {
				cmd = real_cmd = DC_SET_FORCE_SHUTDOWN;
			}
			// do not abort if the child daemon is not there, because
			// A) we have no reason to beleave that it _should_ be there
			// B) if it should be there, the user will get an error when
			//    the real command is sent following this peaceful command
			IgnoreMissingDaemon = true;

			if( !subsys || !strcmp(subsys,"startd") ) {
				real_dt = DT_STARTD;
				rc = doCommands(got_name_or_addr,&names_and_addrs[0],MyName,unresolved_names);
				if(rc) return rc;
			}
			if( !subsys || !strcmp(subsys,"schedd") ) {
				real_dt = DT_SCHEDD;
				rc = doCommands(got_name_or_addr,&names_and_addrs[0],MyName,unresolved_names);
				if(rc) return rc;
			}

			// Restore globals.
			real_dt = orig_real_dt;
			real_cmd = orig_real_cmd;
			cmd = orig_cmd;
			IgnoreMissingDaemon = orig_IgnoreMissingDaemon;
		}
	}

	rc = doCommands(got_name_or_addr,&names_and_addrs[0],MyName,unresolved_names);

	if ( ! unresolved_names.empty()) {
		for (const auto& name: unresolved_names) {
			fprintf( stderr, "Can't find address for %s\n", name.c_str());
		}
		fprintf( stderr, "Perhaps you need to query another pool.\n" );
		if ( ! rc) rc = 1;
	}

	return rc;
}

int
doCommands(int /*argc*/, const char * argv[], const char *MyName, std::vector<std::string> & unresolved_names)
{
	std::vector<std::string> names;
	std::vector<std::string> addrs;
	std::vector<Daemon> daemons;
	char *daemonname;
	bool found_one = false;

	if( all || (constraint!=NULL) ) {
			// If we were told -all, we can just ignore any other
			// options and send the specifed command to every machine
			// in the pool.
		handleAll();
		return 0;
	}

	// the incoming "argv" is just the arguments that are names and/or addresses
	// it used to be the full argv passed to main, but that changed in 9.12
	// We still call it argv to avoid a lot of code churn
	for(; *argv; argv++ ) {
		switch( (*argv)[0] ) {
		case '<':
				// This is probably a sinful string, use it
			found_one = true;
			if( is_valid_sinful(*argv) ) {
				addrs.emplace_back( *argv );
			} else {
				fprintf( stderr, "Address %s is not valid.\n", *argv );
				fprintf( stderr, "Should be of the form <ip.address.here:port>.\n" );
				fprintf( stderr, "For example: <123.456.789.123:6789>\n" );
				all_good = false;
				continue;
			}
			break;
		default:
				// This is probably a daemon name, use it.
			found_one = true;
			if( (daemonname = get_daemon_name(*argv)) == NULL ) {
				fprintf( stderr, "%s: unknown host %s\n", MyName, 
						 get_host_part(*argv) );
				all_good = false;
				continue;
			}
			// if daemonname is NULL, we "continue" above and never reach here,
			// so we know daemonname points to something.  however, it is
			// possibly it will point to an empty string, if the name failed to
			// resolve.  in this case, we'll just add the unresolved value from
			// argv, in case that is the "name" of the daemon but it doesn't
			// resolve to a hostname (definitely possible with NAT'd nodes, in
			// EC2, or a number of other scenarios.)
			if (*daemonname) {
				names.emplace_back( daemonname );
			} else {
				names.emplace_back( *argv );
			}
			free( daemonname );
			daemonname = NULL;
			break;
		}
	}

	if( ! found_one ) {
		if( pool ) {
				// Evil, they specified a valid pool but didn't give a
				// real target for what daemon they want to send this
				// command to.  We need to print an error and die,
				// instead of just sending the command to the local
				// machine. 
			pool_target_usage();
		}
			// if they didn't specify any targets, and aren't trying
			// to talk to a remote pool, they just want to send their
			// command to the local host. 
		Daemon local_d( real_dt, NULL );
		if( real_dt == DT_GENERIC ) {
			local_d.setSubsystem( subsys );
		}
		if( ! local_d.locate() ) {
			if( IgnoreMissingDaemon ) {
				return 0;
			}
			fprintf( stderr, "Can't find address for %s\n", local_d.idStr() );
			fprintf( stderr, "Perhaps you need to query another pool.\n" ); 
			return 1;
		}
		doCommand( local_d );
		return all_good ? 0 : 1;
	}

		// If we got here, there were some targets specified on the
		// command line.  that we know all the targets, resolve any
		// names, with a single query to the collector...
	if( ! resolveNames(daemons, &names, &unresolved_names) ) {
		fprintf( stderr, "ERROR: Failed to resolve daemon names, aborting\n" );
		exit( 1 );
	}

	for (const auto& addr : addrs) {
		if( subsys ) {
			daemons.emplace_back( dt, addr.c_str(), nullptr );
		} else {
			daemons.emplace_back( DT_ANY, addr.c_str(), nullptr );
		}
	}

		// Now, send commands to all the daemons we know about.
	for (auto& d : daemons) {
		if( real_dt == DT_GENERIC ) {
			d.setSubsystem( subsys );
		}
		doCommand( d );
	}
	return all_good ? 0 : 1;
}


void
computeRealAction( void )
{
	real_cmd = -1;
	switch( cmd ) {

	case DC_RECONFIG_FULL:
		if (dash_drain) {
			real_dt = DT_MASTER;
			real_cmd = DAEMONS_OFF_FLEX;
		}
		break;

	case RESTART:
		if (dash_drain) {
			real_dt = DT_MASTER;
			real_cmd = DAEMONS_OFF_FLEX;
		} else if (subsys && dt != DT_MASTER) {
				// We're trying to restart something and we were told
				// a specific daemon to restart.  So, just send a
				// DC_OFF to that daemon, and the master will restart
				// for us.  Note: we don't want to do this if we were
				// told to restart the master, since if we send this,
				// the master itself will exit, which we don't want.
			real_cmd = DC_OFF_GRACEFUL;
		}
		break;

	case DAEMONS_OFF:
		if (dash_drain) {
			real_dt = DT_MASTER;
			real_cmd = DAEMONS_OFF_FLEX;
		}  else if (subsys) {
				// If we were told the subsys, we need a different
				// cmd, and we want to send it to the master. 
				// If we were told to use the master, we want to send
				// a DC_OFF_GRACEFUL, not a DAEMON_OFF 
			if( dt == DT_MASTER ) {
				real_cmd = DC_OFF_GRACEFUL;
				if( force_shutdown ) {
					real_cmd = DC_OFF_FORCE;
				}
			} else {
				real_cmd = DAEMON_OFF;
			}
			real_dt = DT_MASTER;
		}
		break;

	case DAEMONS_ON:
			// regardless of the target, we always want to send this
			// to the master...
		real_dt = DT_MASTER;
		if( subsys && dt != DT_MASTER ) {
				// If we were told the subsys (and it's not the
				// master), we need a different cmd, and we want to
				// send it to the master.
			real_cmd = DAEMON_ON;
		}
		break;

	case RESCHEDULE:
		dt = real_dt = DT_SCHEDD;
		break;

	case VACATE_CLAIM:
		dt = real_dt = DT_STARTD;
		break;

	case SET_SHUTDOWN_PROGRAM:
		real_dt = DT_MASTER;
		break;
	}

	if( real_cmd < 0 ) {
			// if it hasn't been set yet, set it to cmd, since there's
			// nothing special going on...
		real_cmd = cmd;
	}

		// if we still don't know who we're trying to talk to yet, it
		// should be the master, since that's the default for most
		// commands. 
	if( real_dt == DT_NONE ) {
		if( dt ) {
			real_dt = dt;
		} else {
			real_dt = DT_MASTER;
		}
	}
}


bool
resolveNames( std::vector<Daemon>& daemon_list, std::vector<std::string>* name_list, std::vector<std::string>* unresolved_names )
{
	char* tmp = NULL;
	const char* host = NULL;
	bool had_error = false;

		// NULL name_list means we want to do -all and query for
		// everything.  However, if there's a name_list, but it's
		// empty, it means we have no work to do.
	if( name_list && name_list->empty() ) {
		return true;
	}

	AdTypes	adtype = MASTER_AD;
	switch( real_dt ) {
	case DT_MASTER:
		adtype = MASTER_AD;
		break;
	case DT_STARTD:
		adtype = STARTD_AD;
		break;
	case DT_SCHEDD:
		adtype = SCHEDD_AD;
		break;
	case DT_CLUSTER:
		adtype = CLUSTER_AD;
		break;
	case DT_COLLECTOR:
		adtype = COLLECTOR_AD;
		break;
	case DT_NEGOTIATOR:
		adtype = NEGOTIATOR_AD;
		break;
	case DT_CREDD:
		adtype = CREDD_AD;
		break;
	case DT_GENERIC:
		adtype = GENERIC_AD;
		break;
	case DT_HAD:
		adtype = HAD_AD;
		break;
	default:
			// TODO: can we do better than this?
		fprintf( stderr, "Unrecognized daemon type while resolving names\n" );
		usage( NULL );
	}

	const char* pool_addr = pool ? pool->addr() : NULL;
	CondorQuery query(adtype);
	ClassAd* ad;
	ClassAdList ads;

	CondorError errstack;
	QueryResult q_result;

	if (adtype == GENERIC_AD) {
		query.setGenericQueryType(subsys);
	}
	if (constraint!=NULL) {
	  query.addANDConstraint(constraint);
	}
	query.setLocationLookup("tool", false);
	query.addExtraAttribute(ATTR_SEND_PRIVATE_ATTRIBUTES, "true");

	if (pool_addr) {
		q_result = query.fetchAds(ads, pool_addr, &errstack);
	} else {
		CollectorList * collectors = CollectorList::create();
		q_result = collectors->query (query, ads);
		delete collectors;
	}
	if( q_result != Q_OK ) {
		fprintf( stderr, "%s\n", errstack.getFullText(true).c_str() );
		fprintf( stderr, "ERROR: can't connect to %s\n",
				 pool ? pool->idStr() : "local collector" );
		had_error = true;
	} else if( ads.Length() <= 0 ) { 
			// TODO make this message better
		if( IgnoreMissingDaemon ) {
			return true;
		}
		fprintf( stderr, "Found no ClassAds when querying pool (%s)\n",
				 pool ? pool->name() : "local" );
		had_error = true;
	}
	if( had_error ) {
		if( ! name_list ) {
		  if ( constraint!=NULL ) {
		    fprintf( stderr, "Can't find addresses for %s's for constraint '%s'\n", 
			     real_dt ? daemonString(real_dt) : "daemon", constraint );
		  } else {
		    fprintf( stderr, "Can't find addresses for %s's for -all\n", 
			     real_dt ? daemonString(real_dt) : "daemon" );
		  }
		} else {
			for (const auto& name: *name_list) {
				fprintf( stderr, "Can't find address for %s %s\n", 
						 real_dt ? daemonString(real_dt) : "daemon", name.c_str() );
			}
		}
		fprintf( stderr, "Perhaps you need to query another pool.\n" );
		exit( 1 );
	}


	if( ! name_list ) {
			// no list of names to check, just store everything.
		ads.Rewind();
		while( (ad = ads.Next()) ) {
			daemon_list.emplace_back(ad, real_dt, pool_addr);
		}
		return true;
	}

		// now we have to compare the list of names we're looking for
		// against the list of ads we got back.  two linked lists
		// *sigh*.  for now, we just do it the stupid, evil O(N^2)
		// way.  if this ever turns out to be a problem a) people
		// should use "-all" (which is only O(N)), or we could make
		// this smarter.  For now, we do it the slow way since we're
		// looking for a specific list of names and we need to make
		// sure we find them all.
	for (const auto& name : *name_list) {
		bool found_it = false;
		if (unresolved_names && contains(*unresolved_names, name))
			continue;
		ads.Rewind();
		while( !found_it && (ad = ads.Next()) ) {
				// we only want to use the special ATTR_MACHINE hack
				// to locate a startd if the query string we were
				// given (which we're holding in the variable "name")
				// does NOT contain an '@' character... if it does, it
				// means the end user is trying to find a specific
				// startd (e.g. glide-in, a certain slot, etc).
			if( real_dt == DT_STARTD && ! strchr(name.c_str(), '@') ) {
				host = get_host_part( name.c_str() );
				ad->LookupString( ATTR_MACHINE, &tmp );
				dprintf (D_FULLDEBUG, "TOOL: checking startd (%s,%s,%s)\n", name.c_str(),host,tmp);
				if( ! tmp ) {
						// weird, malformed ad.
						// should we print a warning?
					continue;
				}
				if( strcasecmp(tmp, host) ) {		// no match
					free( tmp );
					tmp = NULL;
					continue;
				}

					/*
					  Because we need to keep track of what the user
					  gave us on the command line (whether it was just
					  a hostname, or a qualified daemon name), we want
					  to store back whatever we're using for the
					  "name" into the daemon object we create.  this
					  way, we won't get confused when dealing with SMP
					  startds, or daemons with multiple names if the
					  user just specified a hostname.  This also saves
					  us when the user specifies a given slot on an SMP,
					  and we match with whatever slot from the same host
					  we hit first.  By inserting the name we're
					  looking for, we just ensure we've got the right
					  slot (since all we really care about is the
					  version and the ip/port, and those will
					  obviously be the same for all slots)
					*/
				ad->Assign( ATTR_NAME, name);
				daemon_list.emplace_back( ad, real_dt, pool_addr );
				found_it = true;
				free( tmp );
				tmp = NULL;

			} else {  // daemon type != DT_STARTD or there's an '@'
					// everything else always uses ATTR_NAME
				ad->LookupString( ATTR_NAME, &tmp );
				if( ! tmp ) {
						// weird, malformed ad.
						// should we print a warning?
					continue;
				}
				if( strchr(tmp, '@') ) {
					// could be slot1@foo@something, or slot1@hostname.
					// for this comparison we strip off the first part.
					host = 1 + strchr(tmp, '@');
				} else {
					host = tmp;
				}
				dprintf (D_FULLDEBUG, "TOOL: checking %s (%s,%s,%s)\n",
				         real_dt ? daemonString(real_dt) : "daemon", name.c_str(), host, tmp);

					/* look for a couple variations */
				if( ! strcasecmp(name.c_str(), host) || ! strcasecmp(name.c_str(), tmp) ) {
						/* See comment above, "Because we need..." */
					ad->Assign( ATTR_NAME, name);
					daemon_list.emplace_back( ad, real_dt, pool_addr );
					found_it = true;
				}
				free( tmp );
				tmp = NULL;
			} // daemon type
		} // while( each ad from the collector )

		if( !found_it ) {
			fprintf( stderr, "Can't find address for %s %s\n",
					 daemonString(real_dt), name.c_str() );
			if (unresolved_names) {
				unresolved_names->emplace_back(name);
			} else {
				had_error = true;
			}
		}
	} // while( each name we were given ) 

	if( had_error ) {
		fprintf( stderr,
				 "Perhaps you need to query another pool.\n" );
		all_good = false;
	}
	return true;
}


void
doCommand( Daemon& d )
{
	bool done = false;
	int	my_cmd = real_cmd;
	CondorError errstack;
	bool error = true;
	bool want_reply = false;
	const char* name;
	bool is_local;
	daemon_t d_type;

	ClassAd cmdAd;
	if (real_cmd == DAEMONS_OFF_FLEX) {
		if ( ! d.version() || ! CondorVersionInfo(d.version()).built_since_version(9,11,0)) {
			fprintf(stderr, "condor_master must be version 9.12 or later for this command\n");
			all_good = false;
			return;
		}
		if (dash_drain) {
			want_reply = true;
			cmdAd.Assign("Drain", "STARTDS");
			cmdAd.Assign(ATTR_HOW_FAST, fast ? DRAIN_FAST : (dash_quick ? DRAIN_QUICK : DRAIN_GRACEFUL));
			// master will choose what to do on completion...
			//cmdAd.Assign(ATTR_RESUME_ON_COMPLETION, (cmd==DC_RECONFIG_FULL) ? DRAIN_NOTHING_ON_COMPLETION : DRAIN_EXIT_ON_COMPLETION);
			if (drain_reason) {
				cmdAd.Assign(ATTR_DRAIN_REASON, drain_reason);
			} else {
				auto_free_ptr username(my_username());
				if (! username) username.set(strdup("command"));
				std::string reason("by "); reason += username.ptr();
				cmdAd.Assign(ATTR_DRAIN_REASON, reason);
			}
			if (draining_check_expr) cmdAd.AssignExpr(ATTR_CHECK_EXPR, draining_check_expr);
			if (draining_start_expr) cmdAd.AssignExpr(ATTR_START_EXPR, draining_start_expr);
		}
		if (exec_program) {
			cmdAd.Assign("ShutdownTask", exec_program);
			want_reply = true;
			if (cmd == DAEMONS_OFF) cmd = MASTER_OFF; // use MASTER_OFF so the master exits also
		} else if (dt == DT_MASTER) {
			if (cmd == DAEMONS_OFF) cmd = MASTER_OFF; // use MASTER_OFF so the master exits also
		}
		cmdAd.Assign("WantReply", want_reply);
		cmdAd.Assign("Command", cmd);
	}

	do {
		// Grab some info about the daemon which is frequently used.
		name = d.name();
		d_type = d.type();
		is_local = d.isLocal();
		ReliSock sock;
		done = false;
		my_cmd = real_cmd;

		// If we're trying to send either vacate or checkpoint we have
		// two possible commands to send, the machine-wide version and
		// the per-claim version.  we have to treat the per-claim one
		// as a special case in a couple of places, so figure that out
		// now and use this bool in the rest of this function.  if
		// we're doing a -all, if all we know is a sinful string, if
		// it's a local vacate, or if we only have a hostname but no
		// "slotX@...", we want to send a machine-wide command, not the
		// per-claim one.
		bool is_per_claim_startd_cmd = false;
		if( real_cmd == VACATE_CLAIM ) {
			if( !all && d_type != DT_ANY && !is_local &&
				(name && strchr(name, '@')) )
			{
				is_per_claim_startd_cmd = true;
			}
		}

		// in general, we never want to send the same command to the
		// same address.  the only exception is if we're doing a
		// per-claim startd command.  in that case, we should honor
		// the requests even though we're talking to the same address,
		// since we'll send the claim-id after the command and it
		// won't be duplication of effort.
		if( ! is_per_claim_startd_cmd ) {
			// If we fail to insert the address, then it's already present
			// in the set, which means we've already contacted the daemon.
			auto [it, success] = addresses_sent.insert(d.addr());
			if (!success) {
				return;
			}
		}

		/* Connect to the daemon */
		if( sock.connect(d.addr()) ) {
			error = false;
//			break;
		} else {
			continue;
		}

		switch(real_cmd) {
		case VACATE_CLAIM:
			if( is_per_claim_startd_cmd ) {
					// we've got a specific slot, so send the claim after
					// the command.
				if( fast ) {
					my_cmd = VACATE_CLAIM_FAST;
				}
				if (!d.startCommand(my_cmd, &sock, 0, &errstack)) {
					fprintf(stderr, "ERROR\n%s\n", errstack.getFullText(true).c_str());
				}
				if( !sock.put(name) || !sock.end_of_message() ) {
					fprintf( stderr, "Can't send %s command to %s\n", 
								 cmdToStr(my_cmd), d.idStr() );
					all_good = false;
					return;
				} else {
					done = true;
				}
			} else {
				if( fast ) {
					my_cmd = VACATE_ALL_FAST;
				} else {
					my_cmd = VACATE_ALL_CLAIMS;
				}
			}
			break;

		case DAEMON_OFF:
				// if -fast is used, we need to send a different command.
			if( fast ) {
				my_cmd = DAEMON_OFF_FAST;
			} else if( peaceful_shutdown ) {
				my_cmd = DAEMON_OFF_PEACEFUL;
			}
			if( !d.startCommand( my_cmd, &sock, 0, &errstack) ) {
				fprintf( stderr, "ERROR\n%s\n", errstack.getFullText(true).c_str() );
			}
			if( !sock.put( subsys ) || !sock.end_of_message() ) {
				fprintf( stderr, "Can't send %s command to %s\n",
							cmdToStr(my_cmd), d.idStr() );
				all_good = false;
				return;
			} else {
				done = true;
			}
			break;

		case DAEMON_ON:
			if( !d.startCommand(my_cmd, &sock, 0, &errstack) ) {
				fprintf( stderr, "ERROR\n%s\n", errstack.getFullText(true).c_str() );
			}
			if( !sock.put( subsys ) || !sock.end_of_message() ) {
				fprintf( stderr, "Can't send %s command to %s\n",
						 cmdToStr(my_cmd), d.idStr() );
				all_good = false;
				return;
			} else {
				done = true;
			}
			break;

		case RESTART:
			if( peaceful_shutdown ) {
				my_cmd = RESTART_PEACEFUL;
			}
			break;

		case DAEMONS_OFF_FLEX: {
			ClassAd replyAd;
			if (!d.startCommand(my_cmd, &sock, 0, &errstack)) {
				fprintf(stderr, "ERROR\n%s\n", errstack.getFullText(true).c_str());
			}
			if (!putClassAd(&sock, cmdAd) || !sock.end_of_message()) {
				fprintf(stderr, "Can't send %s command to %s\n",
					cmdToStr(my_cmd), d.idStr());
				all_good = false;
				return;
			} else {
				done = true;
			}
			if (want_reply) {
				replyAd.Clear();
				sock.decode();
				if ( ! getClassAd(&sock, replyAd) || ! sock.end_of_message()) {
					fprintf(stderr, "No response for %s command to %s\n",
						cmdToStr(cmd), d.idStr());
				} else {
					// got a reply, what do we do with it?
					std::string buf;
					fprintf(stdout, "Response to %s command from %s\n%s\n",
						cmdToStr(cmd), d.idStr(), formatAd(buf, replyAd, "\t"));
				}
			}

		} break;

		case DAEMONS_OFF:
				// if -fast is used, we need to send a different command.
			if( fast ) {
				my_cmd = DAEMONS_OFF_FAST;
			} else if( peaceful_shutdown ) {
				my_cmd = DAEMONS_OFF_PEACEFUL;
			}
			if( d_type != DT_MASTER ) {
 					// if we're trying to send this to anything other than
 					// a master (for example, if we were just given a
 					// sinful string and we don't know the daemon type)
 					// we've got to send a different command. 
				if( fast ) {
					my_cmd = DC_OFF_FAST;
				} else if( peaceful_shutdown ) {
					my_cmd = DC_OFF_PEACEFUL;
				} else if( force_shutdown ) {
					my_cmd = DC_OFF_FORCE;
				} else {
					my_cmd = DC_OFF_GRACEFUL;
				}
			}
			break;

		case DC_OFF_GRACEFUL:
				// if -fast is used, we need to send a different command.
			if( fast ) {
				my_cmd = DC_OFF_FAST;
			} else if( peaceful_shutdown ) {
				my_cmd = DC_OFF_PEACEFUL;
			}
			break;

		case DC_RECONFIG_FULL:
				// Nothing to do
			break;
		case SET_SHUTDOWN_PROGRAM:
		{
			if( !d.startCommand(my_cmd, &sock, 0, &errstack) ) {
				fprintf( stderr, "ERROR\n%s\n", errstack.getFullText(true).c_str() );
			}
			if( !sock.put( exec_program ) || !sock.end_of_message() ) {
				fprintf( stderr, "Can't send %s command to %s\n",
						 cmdToStr(my_cmd), d.idStr() );
				all_good = false;
				return;
			} else {
				done = true;
			}
			break;
		}

		default:
			break;
		}

		if( !done ) {
			if( !d.sendCommand(my_cmd, &sock, 0, &errstack) ) {
				fprintf( stderr, "ERROR\n%s\n", errstack.getFullText(true).c_str() );
				fprintf( stderr, "Can't send %s command to %s\n",
							 cmdToStr(my_cmd), d.idStr() );
				all_good = false;
				return;
			}
		}
	
			// for the purposes of reporting, if what we're trying to do
			// is a restart, we want to print that here.  so, set my_cmd
			// back to RESTART.
		if( cmd == RESTART ) {
			my_cmd = RESTART;
		}

			// now, print out the right thing depending on what we did
		if( my_cmd == DAEMON_ON || my_cmd == DAEMON_OFF || 
				my_cmd == DAEMON_OFF_FAST || ((my_cmd == DC_OFF_GRACEFUL ||
				my_cmd == DC_OFF_FAST || my_cmd == DC_OFF_FORCE) &&
				real_dt == DT_MASTER) ) {
			if( d_type == DT_ANY ) {
				printf( "Sent \"%s\" command to %s\n",
						cmdToStr(my_cmd), d.idStr() );
			} else {
				printf( "Sent \"%s\" command for \"%s\" to %s\n",
						cmdToStr(my_cmd), 
						subsys_arg ? subsys_arg : daemonString(dt),
						d.idStr() );
			}
		} else if( d_type == DT_STARTD && all ) {
				// we want to special case printing about this, since
				// we're doing a machine-wide command...
			printf( "Sent \"%s\" command to startd %s\n", cmdToStr(my_cmd),
					d.fullHostname() );
		} else if( cmd_set ) {
			printf( "Sent command \"%d\" to %s\n", my_cmd, d.idStr() );
		} else {
			printf( "Sent \"%s\" command to %s\n", cmdToStr(my_cmd), d.idStr() );
		}
		sock.close();
	} while( d.nextValidCm() );
	if( error ) {
		fprintf( stderr, "Can't connect to %s\n", d.idStr() );
		all_good = false;
		return;
	}
}

void
version()
{
	printf( "%s\n%s\n", CondorVersion(), CondorPlatform() );
	exit( 0 );
}


// Want to send a command to all hosts in the pool or
// modulo the constraint, of course
void
handleAll()
{
	std::vector<Daemon> daemons;

	if( ! resolveNames(daemons, NULL, NULL) ) {
	  if ( constraint!=NULL ) {
	    fprintf( stderr, "ERROR: Failed to find daemons matching the constraint\n" );
	  } else {
	    fprintf( stderr, "ERROR: Failed to find daemons for -all\n" );
	  }
	  exit( 1 );
	}

		// Now, send commands to all the daemons we know about.
	for (auto& d : daemons) {
		if( real_dt == DT_GENERIC ) {
			d.setSubsystem( subsys );
		}
		doCommand( d );
	}
}


void
doSquawk( const char *address ) {

		/* making own addr here; memory management in tool confusing. */
	char line[256], addr[256];
	int i, done = FALSE;
	strcpy_len( addr, address, COUNTOF(addr) );
	
	while ( !done ) {
		printf ( "> " );
		fflush ( stdout );

		if ( !fgets( line, 256, stdin ) ) {
			done = TRUE;
		} else {
			for ( i=0 ; line[i] != '\0' ; i++ ) {
				line[i] = tolower ( line[i] );
			}
			if ( i > 0 && line[i-1] == '\n' ) { 
				line[i-1] = '\0';
			}
			done = ! handleSquawk( line, addr );
		}
	}
}

int
handleSquawk( char *line, char *addr ) {
	
	char *token;
	int command, signal;
    ReliSock sock;

    if ( !sock.connect( addr ) ) {
        printf ( "Problems connecting to %s\n", addr );
        return TRUE;
    }

        
	token = strtok( line, " " );

	if ( !token ) return TRUE;
	
	switch ( token[0] ) {
	case 'e': /* exit */
	case 'q': /* quit */
		return FALSE;
		
	case 's':  { /* Send a DC signal */
		token = strtok( NULL, " " );
		if ( !token ) {
			printf ( "You must specify a signal to send.\n" );
			return TRUE;
		}
		if ( strncmp( token, "dc_", 3 ) ) {
			printf ( "The signal must be a daemoncore signal "
					 "(starts with DC_).\n" );
			return TRUE;
		}
		
		signal = getCommandNum( token );
		if ( signal == -1 ) {
			printf ( "Signal %s not known.\n", token );
			return TRUE;
		}
		
		Daemon d( DT_ANY, addr );
		CondorError errstack;
		if (!d.startCommand (DC_RAISESIGNAL, &sock, 0, &errstack)) {
			fprintf( stderr, "ERROR\n%s\n", errstack.getFullText(true).c_str() );
		}

		sock.encode();
		if (!sock.code(signal) || !sock.end_of_message()) {
			fprintf(stderr, "Failed to send signal to %s\n", d.idStr());
		}
		
		return TRUE;
	}
	case 'c': { /* Send a DC Command */
		token = strtok( NULL, " " );
		if ( !token ) {
			printf ( "You must specify a command to send.\n" );
			return TRUE;
		}
		
		command = getCommandNum( token );
		if ( command == -1 ) {
			printf ( "Command %s not known.\n", token );
			return TRUE;
		}

		Daemon d( DT_ANY, addr );
		CondorError errstack;
		if (!d.startCommand ( command, &sock, 0, &errstack)) {
			fprintf( stderr, "%s\n", errstack.getFullText(true).c_str() );
		}
		sock.encode();
		while( (token = strtok(NULL, " ")) ) {
			if ( isdigit(token[0]) ) {
				int dig = atoi( token );
				if ( !sock.code( dig ) ) {
					printf ( "Error coding %d.\n", dig );
					return TRUE;
				}
			} else {
				if ( !sock.code( token ) ) {
					printf ( "Error coding %s.\n", token );
					return TRUE;
				}
			}
		}
		sock.end_of_message();

		return TRUE;
	}
	case 'r':
		if ( doSquawkReconnect( addr ) ) {
			printf ( "Now talking to %s.\n", addr );
		}
		return TRUE;

	case '\n':
		return TRUE;

	case 'h': /* help */
		if ( ( token = strtok( NULL, " " )) != NULL ) {
			return TRUE;
		}
			/* Generic help falls thru to here: */
			//@fallthrough@
	default:
		printf( "Valid commands are \"help\", \"signal\", \"command\"," );
		printf( "\"reconnect\",\n\"dump\" (state into a ClassAd) and" );
		printf( "\"quit\".\n");
		return TRUE;
	} /* switch */		

	return TRUE; /* for a happy C++ compiler */
}

int
doSquawkReconnect( char *addr ) {
	
	char *token;
	
	if ( (token = strtok( NULL, " " )) == NULL ) {
		printf ( "You must specify a daemon to connect to...\n" );
		return FALSE;
	}
	
	if ( token[0] == '<' ) {
		strcpy( addr, token );
		return TRUE;
	}
	
		/* get -pool if it exists */
	if ( token[0] == '-' && token[1] == 'p' ) {
		if ( (token = strtok( NULL, " " )) == NULL ) {
			printf ( "-pool needs another argument...\n" );
			return FALSE;
		}
		char *tmp = token;
		if ( (token = strtok( NULL, " " )) == NULL ) {
			printf ( "Need more arguments.\n" );
			return FALSE;
		}
		
		if ( pool ) delete pool;
		pool = new DCCollector( tmp );
		if( ! pool->addr() ) {
			fprintf( stderr, "%s\n", pool->error() );
			return FALSE;
		} else {
			printf ( "Using pool %s.\n", pool->name() );
		}
	}
	
		/* get hostname */
	char *hostname;
	if( ( hostname = get_daemon_name(token)) == NULL ) {
		printf( "Unknown host %s\n", get_host_part(token) );
		return FALSE;
	}
	
	if ( (token = strtok( NULL, " " )) == NULL ) {
		dt = DT_MASTER;
	} else {
		switch ( token[1] ) {  /* skip '-' */
		case 's':
			if ( token[2] == 't' ) dt = DT_STARTD;
			else if ( token[2] == 'c' ) dt = DT_SCHEDD;
			else dt = DT_MASTER;
			break;
		case 'c':
			dt = DT_COLLECTOR;
			break;
		case 'm':
			dt = DT_MASTER;
			break;
		case 'n':
			dt = DT_NEGOTIATOR;
			break;
		case 'k':
			dt = DT_KBDD;
			break;
		default:
			dt = DT_MASTER;
		}
	}
	Daemon d( dt, hostname, pool ? pool->addr() : NULL );
	if( real_dt == DT_GENERIC ) {
		d.setSubsystem( subsys );
	}
	if( ! d.locate(Daemon::LOCATE_FOR_LOOKUP) ) {
		printf ( "Failed to contact daemon.\n" );
		free( hostname );
		return FALSE;
	}
	strcpy ( addr, d.addr() );
	free( hostname );
	
	return TRUE;	
}

void squawkHelp( const char *token ) {
	switch( token[0] ) {
	case 's': 
		printf ( "Send a daemoncore signal.\n" ); 
		break;
	case 'c': 
		printf ( "Send a daemoncore command.  Each word after "
				 "the command\nwill also be sent.  If you send "
				 "a number, it will be sent\nas an integer.\n" );
		break;
	case 'r':
		printf( "Connect to a different daemon.  The format is:\n" );
		printf( "reconnect <host:port> or\n" );
		printf( "reconnect [-pool poolname] hostname [-deamontype]\n");
		break;
	case 'e':
	case 'q':
		printf( "Terminates this squawking session.\n" );
		break;
	default:
		printf ( "Don't know command %s.\n", token );
	}
}

bool
printAdToFile(ClassAd *ad, char* filename) {

	FILE *fp;

	if ( filename ) {
	    if ( (fp = safe_fopen_wrapper_follow(filename,"ab")) == NULL ) {
			printf ( "ERROR appending to %s. could not open file.\n", filename );
			return FALSE;
		}
	} else {
		fp = stdout;
	}

	std::string buf;
	formatAd(buf, *ad);
	buf += "****\n";    // separator

	int result = fputs(buf.c_str(), fp);
	if (result < 0) {
        printf( "ERROR - failed to write ad to file. errno=%d\n", errno );
    }
	
	if ( filename ) {
		fclose(fp);
	}

	return !(result<0);
}


int strncmp_auto(const char *s1, const char *s2)
{
    return strncasecmp(s1, s2, strlen(s2));
}
