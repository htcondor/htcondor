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

extern "C" int strincmp( char*, char*, int );

void do_command( char *name );
void version();
int	is_valid_sinful( char *sinful );

enum daemonType {MASTER, SCHEDD, STARTD};

// Global variables
int cmd = 0;
daemonType dt;
char* daemonName;
char* daemonNames[] = {
	"master",
	"schedd",
	"startd"
};

void
usage( char *str )
{
	char* tmp = strchr( str, '_' );
	if( !tmp ) {
		fprintf( stderr, "Usage: %s [command] [-help] [-version] [hostnames]\n", str );
	} else {
		fprintf( stderr, "Usage: %s [-help] [version] [hostnames]\n", str );
	}
	fprintf( stderr, "  -help gives this usage information.\n" );
	fprintf( stderr, "  -version prints the version.\n" );
	fprintf( stderr, "  The given command is sent to all hosts specified.\n" ); 
	fprintf( stderr, 
			 "  (if no hostname is specified, the local host is used).\n\n" );

	switch( cmd ) {
	case DAEMONS_ON:
		fprintf( stderr, 
				 "  %s turns on the condor daemons specified in the config file.\n", 
				 str);
		break;
	case DAEMONS_OFF:
		fprintf( stderr, "  %s turns off any currently running condor daemons.\n", 
				 str );
		break;
	case MASTER_OFF:
		fprintf( stderr, "  %s %s\n  %s\n", str, 
				 "shuts down any currently running condor daemons ",
				 "and causes the condor_master to exit." );
		break;
	case RESTART:
		fprintf( stderr, "  %s causes the condor_master to restart itself.\n", str );
		break;
	case RECONFIG:
		if( dt == MASTER ) {
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
	case VACATE_ALL_CLAIMS:
		fprintf( stderr, "  %s %s\n  %s\n", str, 
				 "causes the condor_startd to checkpoint any running jobs",
				 "and make them vacate the machine." );
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
	case DAEMONS_ON:
		return "DAEMONS_ON";
	case RECONFIG:
		return "RECONFIG";
	case RESTART:
		return "RESTART";
	case VACATE_ALL_CLAIMS:
		return "VACATE_ALL_CLAIMS";
	case PCKPT_ALL_JOBS:
		return "PCKPT_ALL_JOBS";
	case RESCHEDULE:
		return "RESCHEDULE";
	case MASTER_OFF:
		return "MASTER_OFF";
	}
}
	
int
main( int argc, char *argv[] )
{
	char *daemonname, *daemonaddr, *MyName = argv[0];
	char *cmd_str, **tmp;
	int size, did_one = FALSE;

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
		dt = MASTER;
		daemonName = daemonNames[0];
	} else if( !strcmp( cmd_str, "_restart" ) ) {
		cmd = RESTART;
		dt = MASTER;
		daemonName = daemonNames[0];
	} else if( !strcmp( cmd_str, "_on" ) ) {
		cmd = DAEMONS_ON;
		dt = MASTER;
		daemonName = daemonNames[0];
	} else if( !strcmp( cmd_str, "_off" ) ) {
		cmd = DAEMONS_OFF;
		dt = MASTER;
		daemonName = daemonNames[0];
	} else if( !strcmp( cmd_str, "_master_off" ) ) {
		cmd = MASTER_OFF;
		dt = MASTER;
		daemonName = daemonNames[0];
	} else if( !strcmp( cmd_str, "_reschedule" ) ) {
		cmd = RESCHEDULE;
		dt = SCHEDD;
		daemonName = daemonNames[1];
	} else if( !strcmp( cmd_str, "_reconfig_schedd" ) ) {
		cmd = RECONFIG;
		dt = SCHEDD;
		daemonName = daemonNames[1];
	} else if( !strcmp( cmd_str, "_vacate" ) ) {
		cmd = VACATE_ALL_CLAIMS;
		dt = STARTD;
		daemonName = daemonNames[2];
	} else if( !strcmp( cmd_str, "_checkpoint" ) ) {
		cmd = PCKPT_ALL_JOBS;
		dt = STARTD;
		daemonName = daemonNames[2];
	} else {
		fprintf( stderr, "Error: unknown command %s\n", MyName );
		usage( "condor" );
	}
	
		// First, check for -help or -version:
	tmp = argv;
	for( tmp++; *tmp; tmp++ ) {
		if( (*tmp)[0] == '-' ) {
			if( (*tmp)[1] == 'v' ) {
				version();
			} else {
				usage( MyName );
			}
		}
	}

		// Now, process real args, and ignore - options.
	for( argv++; *argv; argv++ ) {
		switch( (*argv)[0] ) {
		case '-':
				// Some command-line arg we already dealt with
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
	exit( 0 );
}


void
do_command( char *name )
{
	char		*daemonAddr;

	if( name && *name == '<' ) {
			// We were passed a sinful string, use it directly.
		daemonAddr = name;
	} else {
		switch( dt ) {
		case MASTER:
			if ((daemonAddr = get_master_addr(name)) == NULL) {
				if( name ) {
					fprintf( stderr, "Can't find address of master %s\n", name );
				} else {
					fprintf( stderr, "Can't find address of local master\n" );
				}
				return;
			}
			break;
		case SCHEDD:
			if ((daemonAddr = get_schedd_addr(name)) == NULL) {
				if( name ) {
					fprintf( stderr, "Can't find address of schedd %s\n", name );
				} else {
					fprintf( stderr, "Can't find address of local schedd\n" );
				}
				return;
			}
			break;
		case STARTD:
			if( name ) {
				if( (daemonAddr = get_startd_addr(get_host_part(name))) == NULL ) {
					fprintf( stderr, "Can't find startd address on %s\n", 
							 get_host_part(name) );
					return;
				} 
			} else {
				if( (daemonAddr = get_startd_addr(NULL)) == NULL ) {
					fprintf( stderr, "Can't find address of local startd\n" );
					return;
				}
			}
			break;
		}
	}

		/* Connect to the daemon */
	ReliSock sock(daemonAddr, 0);
	if(sock.get_file_desc() < 0) {
		fprintf( stderr, "Can't connect to %s (%s)\n", daemonName, 
				 daemonAddr );
		return;
	}

	sock.encode();
	if( !sock.code(cmd) || !sock.eom() ) {
		fprintf( stderr, "Can't send %s command to %s\n", cmd_to_str(cmd), 
				 daemonName );
		return;
	}

	if( name ) {
		if( dt == STARTD ) {
			printf( "Sent %s command to %s on %s\n", cmd_to_str(cmd), 
					daemonName, get_host_part(name) );
		} else {
			printf( "Sent %s command to %s %s\n", cmd_to_str(cmd), 
					daemonName, name );
		}
	} else {
		printf( "Sent %s command to local %s\n", cmd_to_str(cmd), 
				daemonName );
	}
}


void
version()
{
	printf( "%s\n", CondorVersion() );
	exit(0);
}

int
is_valid_sinful( char *sinful )
{
	char* tmp;
	if( !sinful ) return FALSE;
	if( !(sinful[0] == '<') ) return FALSE;
	if( !(tmp = strchr(sinful, ':')) ) return FALSE;
	if( !(tmp = strrchr(sinful, '>')) ) return FALSE;
	return TRUE;
}


extern "C" int SetSyscalls(){}


