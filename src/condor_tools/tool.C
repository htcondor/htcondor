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
#include "daemon_types.h"

#ifndef WIN32
extern "C" int strincmp( char*, char*, int );
#endif

void do_command( char *name );
void version();

// Global variables
int cmd = 0;
daemonType dt;
char* pool = NULL;
int fast = 0;


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
	char* tmp = strchr( str, '_' );
	if( !tmp ) {
		fprintf( stderr, "Usage: %s [command] [-help] [-version] [-pool hostname] [hostnames]\n", str );
	} else {
		fprintf( stderr, "Usage: %s [-help] [-version] [-pool hostname] [hostnames]\n", str );
	}
	fprintf( stderr, "  -help\t\tgives this usage information\n" );
	fprintf( stderr, "  -version\t\tprints the version\n" );
	fprintf( stderr, "  -pool hostname\tuse the given central manager to find daemons\n" );
	fprintf( stderr, "\n  The given command is sent to all hosts specified.\n" ); 
	fprintf( stderr, 
			 "  (if no hostname is specified, the local host is used).\n\n" );

	switch( cmd ) {
	case DAEMONS_ON:
		fprintf( stderr, 
				 "  %s turns on the condor daemons specified in the config file.\n", 
				 str);
		break;
	case DAEMONS_OFF:
		fprintf( stderr, "  %s gracefully turns off any running condor daemons.\n", 
				 str );
		break;
	case MASTER_OFF:
		fprintf( stderr, "  %s %s\n  %s\n", str, 
				 "gracefully shuts down any running condor daemons ",
				 "and causes the condor_master to exit." );
		break;
	case MASTER_OFF_FAST:
		fprintf( stderr, "  %s %s\n  %s\n", str, 
				 "quickly shuts down any running condor daemons ",
				 "and causes the condor_master to exit." );
		break;
	case RESTART:
		fprintf( stderr, "  %s causes the condor_master to restart itself.\n", str );
		break;
	case RECONFIG:
		if( dt == DT_MASTER ) {
			fprintf( stderr, 
					 "  %s causes all condor daemons to reconfigure themselves.\n", 
					 str );
		} else {
				// This is reconfig_schedd
			fprintf( stderr, 
					 "  %s causes the condor_schedd to reconfigure itself.\n", 
					 str );
		}
		break;
	case RESCHEDULE:
		fprintf( stderr, "  %s %s\n  %s\n", str, 
				 "causes the condor_schedd to update the central manager",
				 "and initiate a new negotiation cycle." );
		break;
	case VACATE_CLAIM:
		fprintf( stderr, "  %s %s\n  %s\n", str, 
				 "causes the condor_startd to checkpoint the running job",
				 "on specific (possibly virtual) machines." );
		break;
	case VACATE_ALL_CLAIMS:
		fprintf( stderr, "  %s %s\n  %s\n", str, 
				 "causes the condor_startd to checkpoint any running jobs",
				 "and make them vacate the machine." );
		break;
	case PCKPT_JOB:
		fprintf( stderr, "  %s %s\n  %s%s\n  %s%s\n", str, 
				 "causes the condor_startd to perform a periodic", 
				 "checkpoint on running jobs on specific (possibly virtual) ",
				 "machines.",
				 "The jobs continue to run once ", 
				 "they are done checkpointing." );
		break;
	case PCKPT_ALL_JOBS:
		fprintf( stderr, "  %s %s\n  %s%s\n  %s\n", str, 
				 "causes the condor_startd to perform a periodic", 
				 "checkpoint on any running jobs.  The jobs continue to run ",
				 "once", 
				 "they are done checkpointing." );
		break;
	default:
		fprintf( stderr, "  Valid commands are:\n%s%s",
				 "\toff, on, restart, master_off, reconfig, reschedule,\n",
				 "\treconfig_schedd, vacate, checkpoint\n\n" );
		fprintf( stderr, "  Use \"%s [command] -help\" for more information %s\n", 
				 str, "on a given command." );
		break;
	}
	fprintf(stderr, "\n" );
	exit( 1 );
}

char*
cmd_to_str( int c )
{
	switch( c ) {
	case DAEMONS_OFF:
		return "DAEMONS_OFF";
	case DAEMONS_OFF_FAST:
		return "DAEMONS_OFF_FAST";
	case DAEMONS_ON:
		return "DAEMONS_ON";
	case RECONFIG:
		return "RECONFIG";
	case RESTART:
		return "RESTART";
	case VACATE_CLAIM:
		return "VACATE_CLAIM";
	case VACATE_ALL_CLAIMS:
		return "VACATE_ALL_CLAIMS";
	case PCKPT_JOB:
		return "PCKPT_JOB";
	case PCKPT_ALL_JOBS:
		return "PCKPT_ALL_JOBS";
	case RESCHEDULE:
		return "RESCHEDULE";
	case MASTER_OFF:
		return "MASTER_OFF";
	case MASTER_OFF_FAST:
		return "MASTER_OFF_FAST";
	}

	fprintf(stderr,"Unknown Command (%d) in cmd_to_str()",c);
	exit(1);

	return "UNKNOWN";	// to make C++ happy
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

	config( 0 );

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
		cmd = RECONFIG;
		dt = DT_MASTER;
	} else if( !strcmp( cmd_str, "_restart" ) ) {
		cmd = RESTART;
		dt = DT_MASTER;
	} else if( !strcmp( cmd_str, "_on" ) ) {
		cmd = DAEMONS_ON;
		dt = DT_MASTER;
	} else if( !strcmp( cmd_str, "_off" ) ) {
		cmd = DAEMONS_OFF;
		dt = DT_MASTER;
	} else if( !strcmp( cmd_str, "_master_off" ) ) {
		cmd = MASTER_OFF;
		dt = DT_MASTER;
	} else if( !strcmp( cmd_str, "_reschedule" ) ) {
		cmd = RESCHEDULE;
		dt = DT_SCHEDD;
	} else if( !strcmp( cmd_str, "_reconfig_schedd" ) ) {
		cmd = RECONFIG;
		dt = DT_SCHEDD;
	} else if( !strcmp( cmd_str, "_vacate" ) ) {
		cmd = VACATE_CLAIM;
		dt = DT_STARTD;
	} else if( !strcmp( cmd_str, "_checkpoint" ) ) {
		cmd = PCKPT_JOB;
		dt = DT_STARTD;
	} else {
		fprintf( stderr, "Error: unknown command %s\n", MyName );
		usage( "condor" );
	}
	
		// First, deal with options (begin with '-')
	tmp = argv;
	for( tmp++; *tmp; tmp++ ) {
		if( (*tmp)[0] == '-' ) {
			switch( (*tmp)[1] ) {
			case 'v':
				version();
				break;
			case 'p':
				tmp++;
				if( tmp && *tmp ) {
					if( (foo = get_daemon_name(*tmp)) == NULL ) {
						fprintf( stderr, "%s: unknown host %s\n", MyName, 
								 get_host_part(*tmp) );
						exit( 1 );	
					}
					pool = strdup( foo );
				} else {
					usage( MyName );
				}
				break;
			case 'f':
				fast = 1;
				switch( cmd ) {
				case MASTER_OFF:
					cmd = MASTER_OFF_FAST;
					break;
				case DAEMONS_OFF:
					cmd = DAEMONS_OFF_FAST;
					break;
				default:
					fprintf( stderr, "ERROR: -fast is not valid with %s\n",
							 MyName );
					usage( MyName );
				}
				break;
			case 'a':
				switch( cmd ) {
				case VACATE_CLAIM:
					cmd = VACATE_ALL_CLAIMS;
					break;
				case PCKPT_JOB:
					cmd = PCKPT_ALL_JOBS;
					break;
				default:
					fprintf( stderr, "ERROR: -all is not valid with %s\n",
							 MyName );
					usage( MyName );
				}
				break;
			default:
				usage( MyName );
			}
		}
	}

		// Now, process real args, and ignore - options.
	for( argv++; *argv; argv++ ) {
		switch( (*argv)[0] ) {
		case '-':
				// Some command-line arg we already dealt with
			if( (*argv)[1] == 'p' ) {
					// If it's -pool, we want to skip the next arg, too. 
				argv++;
			}
			continue;
		case '<':
				// This is probably a sinful string, use it
			did_one = TRUE;
			if( is_valid_sinful(*argv) ) {
				do_command( *argv );
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
			do_command( daemonname );
			break;
		}
	}

	if( ! did_one ) {
		do_command( NULL );
	}

	return 0;
}


void
do_command( char *name )
{
	char		*daemonAddr;

	if( name && *name == '<' ) {
			// We were passed a sinful string, use it directly.
		daemonAddr = name;
	} else {
		daemonAddr = get_daemon_addr( dt, name, pool );
	}

	if( ! daemonAddr ) {
		if( name ) {
			fprintf( stderr, "Can't find address for %s %s\n", 
					 daemon_string(dt), name );
			fprintf( stderr, 
					 "Perhaps you need to query another pool.\n" );
		} else {
			fprintf( stderr, "Can't find address of local %s\n",
					 daemon_string(dt) );
		}
		return;
	}

		/* Connect to the daemon */
	ReliSock sock(daemonAddr, 0);
	if(sock.get_file_desc() < 0) {
		fprintf( stderr, "Can't connect to %s on %s (%s)\n", 
				 daemon_string(dt), name, daemonAddr );
		return;
	}

	sock.encode();
	switch(cmd) {
	case VACATE_CLAIM:
	case PCKPT_JOB:
		if( name && *name != '<' && strchr(name, '@')) {
			if( !sock.code(cmd) || !sock.code(name) || !sock.eom() ) {
				fprintf( stderr, "Can't send %s %s command to %s\n", 
						 cmd_to_str(cmd), name, daemon_string(dt) );
				return;
			}
			break;
		}
		// if no name is specified, or if name is a sinful string, we
		// must send VACATE_ALL_CLAIMS or PCKPT_ALL_JOBS instead
		switch(cmd) {
		case VACATE_CLAIM:
			cmd = VACATE_ALL_CLAIMS;
			break;
		case PCKPT_JOB:
			cmd = PCKPT_ALL_JOBS;
			break;
		}
		// no break: we fall through to the default case
	default:
		if( !sock.code(cmd) || !sock.eom() ) {
			fprintf( stderr, "Can't send %s command to %s\n", 
					 cmd_to_str(cmd), daemon_string(dt) );
			return;
		}
	}

	if( name ) {
		if( dt == DT_STARTD ) {
			printf( "Sent %s command to %s on %s\n", cmd_to_str(cmd), 
					daemon_string(dt), get_host_part(name) );
		} else {
			printf( "Sent %s command to %s %s\n", cmd_to_str(cmd), 
					daemon_string(dt), name );
		}
	} else {
		printf( "Sent %s command to local %s\n", cmd_to_str(cmd), 
				daemon_string(dt) );
	}
}


void
version()
{
	printf( "%s\n", CondorVersion() );
	exit(0);
}

extern "C" int SetSyscalls() {return 0;}


