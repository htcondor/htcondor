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

 

/*********************************************************************
  Generic condor tool.
*********************************************************************/

#include "condor_common.h"
#include "condor_config.h"
#include "condor_version.h"
#include "condor_io.h"
#include "my_hostname.h"
#include "get_daemon_addr.h"
#include "internet.h"
#include "get_full_hostname.h"
#include "daemon_types.h"
#include "sig_install.h"
#include "command_strings.h"

void doCommand( char *name );
void version();
void namePrintf( FILE* stream, char* name, char* str, ... );
void handleAll();
void doSquawk( char *addr );
int handleSquawk( char *line, char *addr );
int doSquawkReconnect( char *addr );
void squawkHelp( char *token );
int  printAdToFile(ClassAd *ad, char* filename);

// Global variables
int cmd = 0;
daemon_t dt = DT_NONE;
char* pool = NULL;
int fast = 0;
int all = 0;
char* subsys = NULL;
int takes_subsys = 0;
int cmd_set = 0;

// The pure-tools (PureCoverage, Purify, etc) spit out a bunch of
// stuff to stderr, which is where we normally put our error
// messages.  To enable condor.test to produce easily readable
// output, even with pure-tools, we just write everything to stdout.  
#if defined( PURE_DEBUG ) 
#	define stderr stdout
#endif

void
usage( char *str )
{
	if( ! str ) {
		fprintf( stderr, "Use \"-help\" to see usage information\n" );
		exit( 1 );
	}

	char* tmp = strchr( str, '_' );
	if( !tmp ) {
		fprintf( stderr, "Usage: %s [command] ", str );
	} else {
		fprintf( stderr, "Usage: %s ", str );
	}

	fprintf( stderr, "[general-options] [targets]" );
	if( takes_subsys ) {
		fprintf( stderr, " [subsystem]" );
	}
	fprintf( stderr, "\nwhere [general-options] can be zero or more of:\n" );
	fprintf( stderr, "    -help\t\tgives this usage information\n" );
	fprintf( stderr, "    -version\t\tprints the version\n" );
	fprintf( stderr, 
			 "    -pool hostname\tuse the given central manager to find daemons\n" );
	if( cmd == DAEMONS_OFF || cmd == DAEMON_OFF || cmd == RESTART ) {
		fprintf( stderr, "    -graceful\t\tgracefully shutdown daemons %s\n", 
				 "(the default)" );
		fprintf( stderr, "    -fast\t\tquickly shutdown daemons\n" );
	}
	if( cmd == VACATE_CLAIM ) {
		fprintf( stderr, 
				 "    -graceful\t\tgracefully vacate the jobs (the default)\n" );
		fprintf( stderr, 
				 "    -fast\t\tquickly vacate the jobs (no checkpointing)\n" );
	}

	fprintf( stderr, "where [targets] can be zero or more of:\n" );
#if 0
		// This isn't supported yet, so don't mention it now.
	fprintf( stderr, 
			 "  -all\t\t\tall hosts in your pool (overrides other targets)\n" );
#endif
	fprintf( stderr, "    hostname\t\tgiven host\n" );
	fprintf( stderr, "    <ip.address:port>\tgiven \"sinful string\"\n" );
	fprintf( stderr,
			 "  (for compatibility with other Condor tools, you can also use:)\n" );
	fprintf( stderr, "    -name hostname\tgiven host\n" );
	fprintf( stderr, "    -addr <addr:port>\tgiven \"sinful string\"\n" );
	fprintf( stderr, "  (if no targets are specified, the local host is used)\n" );
	if( takes_subsys ) {
		fprintf( stderr, "where [subsystem] can be one of:\n" );
		if( cmd == DAEMONS_OFF || cmd == DAEMON_OFF ) {
			fprintf( stderr, "    -master\n" );
		} else {
			fprintf( stderr, "    -master\t\t(the default)\n" );
		}
		fprintf( stderr, "    -startd\n" );
		fprintf( stderr, "    -schedd\n" );
		fprintf( stderr, "    -collector\n" );
		fprintf( stderr, "    -negotiator\n" );
		fprintf( stderr, "    -kbdd\n" );
	}
	fprintf( stderr, "\n" );

	switch( cmd ) {
	case DAEMONS_ON:
		fprintf( stderr, 
				 "  %s turns on the condor daemons specified in the config file.\n", 
				 str);
		break;
	case DAEMONS_OFF:
	case DC_OFF_GRACEFUL:
		fprintf( stderr, "  %s turns off the specified daemon.\n", 
				 str );
		fprintf( stderr, 
				 "  If no subsystem is given, everything except the master is shut down.\n" );
		break;
	case RESTART:
		fprintf( stderr, "  %s causes specified daemon to restart itself.\n", str );
		fprintf( stderr, 
				 "  If sent to the master, all daemons on that host will restart.\n" );
		break;

	case DC_RECONFIG:
		fprintf( stderr, 
				 "  %s causes the specified daemon to reconfigure itself.\n", 
				 str );
		fprintf( stderr, 
				 "  If sent to the master, all daemons on that host will reconfigure.\n" );
		break;
	case RESCHEDULE:
		fprintf( stderr, "  %s %s\n  %s\n", str, 
				 "causes the condor_schedd to update the central manager",
				 "and initiate a new negotiation cycle." );
		break;
	case VACATE_CLAIM:
		fprintf( stderr, 
				 "  %s causes the condor_startd to vacate the running\n"
				 "  job(s) on specific machines.  If you specify a virtual machine\n"
				 "  (for example, \"vm1@hostname\"), only that machine will be\n"
				 "  vacated.  If you specify just a hostname, all jobs running under\n"
				 "  any virtual machines at that host will be vacated.  By default,\n"
				 "  the jobs will be checkpointed (if possible), though if you\n"
				 "  specify the -fast option, they will be immediately killed.\n",
				 str );
		break;
	case PCKPT_JOB:
		fprintf( stderr, "  %s %s\n  %s%s\n  %s%s\n", str, 
				 "causes the condor_startd to perform a periodic", 
				 "checkpoint on running jobs on specific (possibly virtual) ",
				 "machines.",
				 "The jobs continue to run once ", 
				 "they are done checkpointing." );
		break;
	case SQUAWK:
		fprintf( stderr, "  %s\n"
				 "is a developer-only command used to talk to daemons.", 
				 str );
		break;
	default:
		fprintf( stderr, "  Valid commands are:\n%s%s",
				 "\toff, on, restart, reconfig, reschedule, ",
				 "vacate, checkpoint\n\n" );
		fprintf( stderr, "  Use \"%s [command] -help\" for more information %s\n", 
				 str, "on a given command." );
		break;
	}
	fprintf(stderr, "\n" );
	exit( 1 );
}


char*
cmdToStr( int c )
{
	switch( c ) {
	case DAEMONS_OFF:
		return "Kill-All-Daemons";
	case DAEMONS_OFF_FAST:
		return "Kill-All-Daemons-Fast";
	case DAEMON_OFF:
	case DC_OFF_GRACEFUL:
		return "Kill-Daemon";
	case DAEMON_OFF_FAST:
	case DC_OFF_FAST:
		return "Kill-Daemon-Fast";
	case DAEMONS_ON:
		return "Spawn-All-Daemons";
	case DAEMON_ON:
		return "Spawn-Daemon";
	case RESTART:
		return "Restart";
	case VACATE_CLAIM:
		return "Vacate-Claim";
	case VACATE_CLAIM_FAST:
		return "Vacate-Claim-Fast";
	case VACATE_ALL_CLAIMS:
		return "Vacate-All-Claims";
	case VACATE_ALL_FAST:
		return "Vacate-All-Claims-Fast";
	case PCKPT_JOB:
		return "Checkpoint-Job";
	case PCKPT_ALL_JOBS:
		return "Checkpoint-All-Jobs";
	case RESCHEDULE:
		return "Reschedule";
	case DC_RECONFIG:
		return "Reconfig";
	case SQUAWK:
		return "Squawk";
	}
	fprintf( stderr, "Unknown Command (%d) in cmdToStr()\n", c );
	exit(1);

	return "UNKNOWN";	// to make C++ happy
}
	

void
subsys_check( char* MyName )
{
	if( ! takes_subsys ) {
		fprintf( stderr, 
				 "ERROR: Can't specify a subsystem flag with %s.\n",  
				 MyName );
		usage( MyName );
	}
	if( dt ) {
		fprintf( stderr, "ERROR: can only specify one subsystem flag.\n" );
		usage( MyName );
	}
	subsys = (char*)1;
}


int
main( int argc, char *argv[] )
{
	char *daemonname, *MyName = argv[0];
	char *cmd_str, **tmp, *foo;
	int size, did_one = FALSE;

#ifndef WIN32
	// Ignore SIGPIPE so if we cannot connect to a daemon we do not
	// blowup with a sig 13.
	install_sig_handler(SIGPIPE, SIG_IGN );
#endif

	config();

	MyName = strrchr( argv[0], DIR_DELIM_CHAR );
	if( !MyName ) {
		MyName = argv[0];
	} else {
		MyName++;
	}
		// See if there's an '-' in our name, if not, append argv[1]. 
	cmd_str = strchr( MyName, '_');
	if( !cmd_str ) {

			// If there's no argv[1], print usage.
		if( ! argv[1] ) { usage( MyName ); }

			// If argv[1] begins with '-', print usage, don't append.
		if( argv[1][0] == '-' ) { 
				// The one exception is if we got a "condor -v", we
				// should print the version, not give an error.
			if( argv[1][1] == 'v' ) {
				version();
			} else {
				usage( MyName );
			}
		}
		size = strlen( argv[1] );
		MyName = (char*)malloc( size + 8 );
		sprintf( MyName, "condor_%s", argv[1] );
		cmd_str = MyName+6;
		argv++; argc--;
	}
		// Figure out what kind of tool we are.
	if( !strcmp( cmd_str, "_reconfig" ) ) {
		cmd = DC_RECONFIG;
		takes_subsys = 1;
	} else if( !strcmp( cmd_str, "_restart" ) ) {
		cmd = RESTART;
		takes_subsys = 1;
	} else if( !strcmp( cmd_str, "_off" ) ) {
		cmd = DAEMONS_OFF;
		takes_subsys = 1;
	} else if( !strcmp( cmd_str, "_on" ) ) {
		cmd = DAEMONS_ON;
		takes_subsys = 1;
	} else if( !strcmp( cmd_str, "_master_off" ) ) {
		fprintf( stderr, "WARNING: condor_master_off is depricated.\n" );
		fprintf( stderr, "\t Use: \"condor_off -master\" instead.\n" );
		cmd = DC_OFF_GRACEFUL;
		dt = DT_MASTER;
		takes_subsys = 0;
	} else if( !strcmp( cmd_str, "_reschedule" ) ) {
		cmd = RESCHEDULE;
	} else if( !strcmp( cmd_str, "_reconfig_schedd" ) ) {
		fprintf( stderr, "WARNING: condor_reconfig_schedd is depricated.\n" );
		fprintf( stderr, "\t Use: \"condor_reconfig -schedd\" instead.\n" );
		cmd = DC_RECONFIG;
		dt = DT_SCHEDD;
	} else if( !strcmp( cmd_str, "_vacate" ) ) {
		cmd = VACATE_CLAIM;
	} else if( !strcmp( cmd_str, "_checkpoint" ) ) {
		cmd = PCKPT_JOB;
	} else if ( !strcmp( cmd_str, "_squawk" ) ) {
		cmd = SQUAWK;
		takes_subsys = 1;
	} else {
		fprintf( stderr, "ERROR: unknown command %s\n", MyName );
		usage( "condor" );
	}
	
		// First, deal with options (begin with '-')
	tmp = argv;
	for( tmp++; *tmp; tmp++ ) {
		if( (*tmp)[0] != '-' ) {
				// If it doesn't start with '-', skip it
			continue;
		}
		switch( (*tmp)[1] ) {
		case 'v':
			version();
			break;
		case 'h':
			usage( MyName );
			break;
		case 'p':
			tmp++;
			if( tmp && *tmp ) {
				if( (pool = get_full_hostname((const char *)(*tmp))) == NULL ) {
					fprintf( stderr, "%s: unknown host %s\n", MyName, *tmp );
					exit( 1 );	
				}
			} else {
				fprintf( stderr, "ERROR: -pool requires another argument\n" );
				usage( NULL );
			}
			break;
		case 'f':
			fast = 1;
			switch( cmd ) {
			case DAEMONS_OFF:
			case DC_OFF_GRACEFUL:
			case RESTART:
			case VACATE_CLAIM:
				break;
			default:
				fprintf( stderr, "ERROR: \"-fast\" is not valid with %s\n",
						 MyName );
				usage( NULL );
			}
			break;
		case 'g':
			fast = 0;
			break;
		case 'a':
			if( (*tmp)[2] ) {
				switch( (*tmp)[2] ) {
				case 'd':
						// We got a -addr, make sure we've got 
						// something else after it
					tmp++;
					if( ! (tmp && *tmp) ) {
						fprintf( stderr, 
								 "ERROR: -addr requires another argument\n" ); 
						usage( NULL );
					}
					break;
				case 'l':
						// We got a "-all", remember that
					all = 1;
					break;
				default:
					fprintf( stderr, 
							 "ERROR: unknown parameter: \"%s\"\n",
							 *tmp ); 
					usage( NULL );
					break;
				}
			} else {
					// Until "-all" works, "-a" is not ambiguous,
					// just treat it as "-addr".
#if 0
				fprintf( stderr, 
						 "ERROR: ambiguous parameter: \"%s\"\n",
						 *tmp ); 
				fprintf( stderr, 
						 "Please specify \"-addr\" or \"-all\"\n" );
				usage( NULL );
#else
				tmp++;
				if( ! (tmp && *tmp) ) {
					fprintf( stderr, 
							 "ERROR: -addr requires another argument\n" ); 
					usage( NULL );
				}
#endif
			}
			break;
		case 'n':
			if( (*tmp)[2] ) {
				switch( (*tmp)[2] ) {
				case 'a': 
						// We got a "-name", make sure we've got 
						// something else after it
					tmp++;
					if( ! (tmp && *tmp) ) {
						fprintf( stderr, 
								 "ERROR: -name requires another argument\n" );
						usage( NULL );
					}
					break;
				case 'e':
						// We got a "-negotiator"
					subsys_check( MyName );
					dt = DT_NEGOTIATOR;
					break;
				default:
					fprintf( stderr, 
							 "ERROR: invalid option: \"%s\"\n",
							 *tmp );  
					usage( NULL );
					break;
				}
			} else {
				fprintf( stderr, 
						 "ERROR: ambiguous option: \"%s\"\n",
						 *tmp ); 
				fprintf( stderr, 
						 "Please specify \"-name\" or \"-negotiator\"\n" );
				usage( NULL );
			}
			break;
		case 'm':
			subsys_check( MyName );
			dt = DT_MASTER;
			break;
		case 'c':
			if( (*tmp)[2] ) {
				switch( (*tmp)[2] ) {
				case 'm': 
						// We got a "-cmd", make sure we've got 
						// something else after it
					tmp++;
					if( tmp && *tmp ) {
						cmd = atoi( *tmp );
						cmd_set = 1;
						if( !cmd ) {
							fprintf( stderr, 
									 "ERROR: invalid argument to -cmd (\"%s\")\n",
									 *tmp );
							exit( 1 );
						}
					} else {
						fprintf( stderr, 
								 "ERROR: -cmd requires another argument\n" ); 
						exit( 1 );
					}
					break;
				case 'o':
					subsys_check( MyName );
					dt = DT_COLLECTOR;
					break;
				default:
					fprintf( stderr, 
							 "ERROR: unknown parameter: \"%s\"\n",
							 *tmp );  
					usage( NULL );
					break;
				}
			} else {
					// Since -cmd is a developer-only, hidden
					// option, just treat "-c" as "-collector". 
#if 0
				fprintf( stderr, 
						 "ERROR: ambiguous parameter: \"%s\"\n",
						 *tmp ); 
				usage( NULL );
#else
				subsys_check( MyName );
				dt = DT_COLLECTOR;
#endif
			}
			break;
		case 'k':
			subsys_check( MyName );
			dt = DT_KBDD;
			break;
		case 's':
			subsys_check( MyName );
			if( (*tmp)[2] ) {
				switch( (*tmp)[2] ) {
				case 'c':
					dt = DT_SCHEDD;
					break;
				case 't':
					dt = DT_STARTD;
					break;
				default: 
					fprintf( stderr, 
							 "ERROR: invalid subsystem argument \"%s\"\n",
							 *tmp );
					usage( NULL );
					break;
				}
			} else {
				fprintf( stderr, 
						 "ERROR: ambiguous subsystem argument \"%s\"\n",
						 *tmp );
				fprintf( stderr, 
						 "Please specify \"-startd\" or \"-schedd\"\n" );
				usage( NULL );
			}
			break;
		default:
			fprintf( stderr, "ERROR: invalid argument \"%s\"\n",
					 *tmp );
			usage( MyName );
		}
	}

		// If we don't know what our dt should be, it should probably
		// be DT_MASTER, with a few exceptions.
	if( !dt ) {
		switch( cmd ) {
		case VACATE_CLAIM:
		case PCKPT_JOB:
			dt = DT_STARTD;
			break;
		case RESCHEDULE:
			dt = DT_SCHEDD;
			break;
		default:
			dt = DT_MASTER;
			break;
		}
	}

	if( all ) {
			// If we were told -all, we can just ignore any other
			// options and send the specifed command to every machine
			// in the pool.
		handleAll();
		return 0;
	}

	if( subsys ) {
		subsys = (char*)daemonString( dt );
	}

		// Now, process real args, and ignore - options.
	for( argv++; *argv; argv++ ) {
		switch( (*argv)[0] ) {
		case '-':
				// Some command-line arg we already dealt with
			if( (*argv)[1] == 'p' || 
				((*argv)[1] == 'c' && (*argv)[2] == 'm') ) {
					// If it's -pool or -cmd, we want to skip the next
					// arg, too.
				argv++;
			}
			continue;
		case '<':
				// This is probably a sinful string, use it
			did_one = TRUE;
			if( is_valid_sinful(*argv) ) {
				doCommand( *argv );
			} else {
				fprintf( stderr, "Address %s is not valid.\n", *argv );
				fprintf( stderr, "Should be of the form <ip.address.here:port>.\n" );
				fprintf( stderr, "For example: <123.456.789.123:6789>\n" );
				continue;
			}
			break;
		default:
				// This is probably a daemon name, use it.
			did_one = TRUE;
			if( (daemonname = get_daemon_name(*argv)) == NULL ) {
				fprintf( stderr, "%s: unknown host %s\n", MyName, 
						 get_host_part(*argv) );
				continue;
			}
			doCommand( daemonname );
			delete [] daemonname;
			daemonname = NULL;
			break;
		}
	}

	if( ! did_one ) {
		doCommand( NULL );
	}

	return 0;
}


#define RESTORE \
dt = old_dt; \
cmd = old_cmd

void
doCommand( char *name )
{
	char		*addr = NULL;
	int 		sinful = 0, done = 0;
	daemon_t 	old_dt = dt;
	int			old_cmd = cmd;

		// See if we were passed a sinful string, and if so, use it. 
	if( name && *name == '<' ) {
		addr = name;
		sinful = 1;
		dt = DT_NONE;
	} 

		// DAEMONS_OFF has some special cases to handle:
	if( cmd == DAEMONS_OFF ) {
			// if we were passed a sinful string, use the DC version. 
		if( sinful ) {
			cmd = DC_OFF_GRACEFUL;
		}
		if( subsys ) {
				// If we were told the subsys, we need a different
				// cmd, and we want to send it to the master. 
				// If we were told to use the master, we want to send
				// a DC_OFF_GRACEFUL, not a DAEMON_OFF 
			if( old_dt == DT_MASTER ) {
				cmd = DC_OFF_GRACEFUL;
			} else {
				cmd = DAEMON_OFF;
			}
			dt = DT_MASTER;
		}
	}
	if( cmd == DAEMONS_ON && subsys && (old_dt != DT_MASTER) ) {
			// If we were told the subsys, so we need a different
			// cmd, and we want to send it to the master. 
		cmd = DAEMON_ON;
		dt = DT_MASTER;
	}
	if( cmd == RESTART && subsys && old_dt != DT_MASTER ) {
			// We're trying to restart something and we were told a
			// specific subsystem to restart.  So, just send a DC_OFF
			// to that daemon, and the master will restart for us. 
			// Note: we don't want to do this if we were told to
			// restart the master, since if we send this, the master
			// itself will exit, which isn't what we want.
		cmd = DC_OFF_GRACEFUL;
	}


	if( ! addr ) {
		addr = get_daemon_addr( dt, name, pool );
	}
	if( ! addr ) {
		namePrintf( stderr, name, "Can't find address for" );
		fprintf( stderr, "Perhaps you need to query another pool.\n" ); 
		RESTORE;
		return;
	}

		/* Squawking does its own connect... */
	if ( cmd == SQUAWK ) {
		doSquawk( addr );
		printf ( "Bye!\n" );
		done = 1;
		RESTORE;
		return;
	}

		/* Connect to the daemon */
	ReliSock sock;
	if(!sock.connect(addr)) {
		namePrintf( stderr, name, "Can't connect to" );
		RESTORE;
		return;
	}

	sock.encode();
	switch(cmd) {
	case VACATE_CLAIM:
		if( fast ) {
			cmd = VACATE_CLAIM_FAST;
		}
		// If no name is specified, or if name is a sinful string or
		// hostname, we must send VACATE_ALL_CLAIMS.
		if ( name && !sinful && strchr(name, '@') ) {
			if( !sock.code(cmd) || !sock.code(name) || !sock.eom() ) {
				namePrintf( stderr, name, "Can't send %s command to", 
							 cmdToStr(cmd) );
				RESTORE;
				return;
			} else {
				done = 1;
			}
		} else {
			if( fast ) {
				cmd = VACATE_ALL_FAST;
			} else {
				cmd = VACATE_ALL_CLAIMS;
			}
		}
		break;
	case PCKPT_JOB:
		// If no name is specified, or if name is a sinful string or
		// hostname, we must send PCKPT_ALL_JOBS.
		if( name && !sinful && strchr(name, '@') ) {
			if( !sock.code(cmd) || !sock.code(name) || !sock.eom() ) {
				namePrintf( stderr, name, "Can't send %s command to", 
							 cmdToStr(cmd) );
				RESTORE;
				return;
			} else {
				done = 1;
			}
		} else {
			cmd = PCKPT_ALL_JOBS;
		}
		break;
	case DAEMON_OFF:
			// if -fast is used, we need to send a different command.
		if( fast ) {
			cmd = DAEMON_OFF_FAST;
		}
		if( !sock.code(cmd) || !sock.code(subsys) || !sock.eom() ) {
			namePrintf( stderr, name, "Can't send %s command to", 
						 cmdToStr(cmd) );
			RESTORE;
			return;
		} else {
			done = 1;
		}
		break;
	case DAEMON_ON:
		if( !sock.code(cmd) || !sock.code(subsys) || !sock.eom() ) {
			namePrintf( stderr, name, "Can't send %s command to", 
						 cmdToStr(cmd) );
			RESTORE;
			return;
		} else {
			done = 1;
		}
		break;
	case DAEMONS_OFF:
			// if -fast is used, we need to send a different command.
		if( fast ) {
			cmd = DAEMONS_OFF_FAST;
		}
		break;
	case DC_OFF_GRACEFUL:
			// if -fast is used, we need to send a different command.
		if( fast ) {
			cmd = DC_OFF_FAST;
		}
		break;
	default:
		break;
	}

	if( !done ) {
		if( !sock.code(cmd) || !sock.eom() ) {
			namePrintf( stderr, name, "Can't send %s command to", 
						 cmdToStr(cmd) );
			RESTORE;
			return;
		}
	}
	if( cmd == DAEMON_ON || cmd == DAEMON_OFF || cmd == DAEMON_OFF_FAST 
		|| ((cmd == DC_OFF_GRACEFUL || cmd == DC_OFF_FAST) && 
		 old_dt == DT_MASTER) ) {
		if( sinful ) {
			namePrintf( stdout, name, "Sent \"%s\" command to", 
						 cmdToStr(cmd), daemonString(old_dt) );
		} else {
			namePrintf( stdout, name, "Sent \"%s\" command for \"%s\" to", 
						 cmdToStr(cmd), daemonString(old_dt) );
		}
	} else if( old_cmd == RESTART ) {
		printf( "Sent \"%s", cmdToStr(old_cmd) );
		if( fast ) {
			printf( "-Fast" );
		}
		namePrintf( stdout, name, "\" command to", cmdToStr(old_cmd) );
	} else if( cmd_set ) {
		namePrintf( stdout, name, "Sent command \"%d\" to", cmd );
	} else {
		namePrintf( stdout, name, "Sent \"%s\" command to", cmdToStr(cmd) );
	}
	RESTORE;
}


void
version()
{
	printf( "%s\n%s\n", CondorVersion(), CondorPlatform() );
	exit( 0 );
}


/*
  This function takes in a name string, which either holds a hostname,
  a sinful string, or is NULL.  It also takes a format string and
  option arguments.  The format string and its args are printed to the
  specified FILE*, followed by the appropriate descriptive string
  depending on the value of name, and dt (the daemon type) we're
  using. 
*/
void
namePrintf( FILE* stream, char* name, char* str, ... )
{
	va_list args;
	va_start( args, str );
	vfprintf( stream, str, args );
	va_end( args );

	if( name ) {
		if( *name == '<' ) {
				// sinful string,
			if( dt ) {
				fprintf( stream, " %s at %s\n", daemonString(dt), name );			
			} else {
				fprintf( stream, " daemon at %s\n", name );			
			}
		} else {
				// a real name
			assert( dt ); 
			fprintf( stream, " %s %s\n", daemonString(dt), name );			
		}
	} else {
			// local daemon
		assert( dt );
		fprintf( stream, " local %s\n", daemonString(dt) );
	}
}

// Want to send a command to all hosts in the pool.
void
handleAll()
{
		// Todo *grin*
	fprintf( stderr, "ERROR: The -all option is not yet implemented\n" );
	exit( 1 );
}

void
doSquawk( char *address ) {

		/* making own addr here; memory management in tool confusing. */
	char line[256], addr[256];
	int i, done = FALSE;
	strcpy( addr, address );
	
	while ( !done ) {
		printf ( "> " );
		fflush ( stdout );

		if ( !fgets( line, 256, stdin ) ) {
			done = TRUE;
		} else {
			for ( i=0 ; line[i] != '\0' ; i++ ) {
				line[i] = tolower ( line[i] );
			}
			if ( line[i-1] == '\n' ) { 
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

	token = strtok( line, " " );

	if ( !token ) return TRUE;
	
	switch ( token[0] ) {
	case 'e': /* exit */
	case 'q': /* quit */
		return FALSE;
		
	case 'd': { /* dump state */
		ReliSock sock;
		if ( !sock.connect( addr ) ) {
			printf ( "Problems connecting to %s\n", addr );
			return TRUE;
		}
		sock.encode();
		sock.put( DUMP_STATE );
		sock.eom();
		sock.decode();

		ClassAd ad;
		ad.initFromStream( sock );
		printAdToFile( &ad, NULL );
		
		return TRUE;
	}
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
		
		signal = get_command_num ( token );
		if ( signal == -1 ) {
			printf ( "Signal %s not known.\n", token );
			return TRUE;
		}
		
		ReliSock sock;
		if ( !sock.connect( addr ) ) {
			printf ( "Problems connecting to %s\n", addr );
			return TRUE;
		}
		sock.encode();
		sock.put( DC_RAISESIGNAL );
		sock.code( signal );
		sock.eom();
		
		return TRUE;
	}
	case 'c': { /* Send a DC Command */
		token = strtok( NULL, " " );
		if ( !token ) {
			printf ( "You must specify a command to send.\n" );
			return TRUE;
		}
		
		command = get_command_num ( token );
		if ( command == -1 ) {
			printf ( "Command %s not known.\n", token );
			return TRUE;
		}

		ReliSock sock;
		if ( !sock.connect( addr ) ) {
			printf ( "Problems connecting to %s\n", addr );
			return TRUE;
		}
		sock.encode();
		sock.code( command );
		while ( token = strtok( NULL, " " ) ) {
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
		sock.eom();

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
			squawkHelp( token );
			return TRUE;
		}
			/* Generic help falls thru to here: */
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
		
		if( (pool = get_full_hostname((const char *)(tmp))) == NULL ) {
			fprintf( stderr, "Unknown host %s\n", tmp );
			return FALSE;
		} else {
			printf ( "Using pool %s.\n", tmp );
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
	char *tmp = get_daemon_addr( dt, hostname, pool );
	if ( !tmp ) {
		printf ( "Failed to contact daemon.\n" );
		return FALSE;
	}
	strcpy ( addr, tmp );
	delete [] hostname;
	
	return TRUE;	
}

void squawkHelp( char *token ) {
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

int 
printAdToFile(ClassAd *ad, char* filename) {

	FILE *fp;

	if ( filename ) {
	    if ( (fp = fopen(filename,"a")) == NULL ) {
			printf ( "ERROR appending to %s.\n", filename );
			return FALSE;
		}
	} else {
		fp = stdout;
	}

    if (!ad->fPrint(fp)) {
        printf( "ERROR - failed to write ad to file.\n" );
        if ( filename ) fclose(fp);
        return FALSE;
    }
    fprintf(fp,"***\n");   // separator
	
	if ( filename ) {
		fclose(fp);
	} 		

    return TRUE;
}


extern "C" int SetSyscalls() {return 0;}




