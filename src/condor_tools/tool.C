/* 
** Copyright 1986, 1987, 1988, 1989, 1990, 1991 by the Condor Design Team
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the Condor Design Team not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the Condor
** Design Team make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE CONDOR DESIGN TEAM DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE CONDOR DESIGN TEAM BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Author:  Derek Wright
**          University of Wisconsin, Computer Sciences Dept.
** 
*/ 

/*********************************************************************
  Generic condor tool.
*********************************************************************/

#include "condor_common.h"
#include "condor_config.h"
#include "condor_io.h"
#include "my_hostname.h"
#include "get_full_hostname.h"

extern "C" char *get_master_addr(const char *);
extern "C" char *get_schedd_addr(const char *);
extern "C" char *get_startd_addr(const char *);
extern "C" int strincmp( char*, char*, int );

void do_command( char *host );

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
	fprintf( stderr, "Usage: %s [-help] [hostnames]\n", str );
	fprintf( stderr, "  -help gives this usage information.\n" );
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
		fprintf( stderr, "  Error: Unknown command %s\n", str);
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
	}
}
	
int
main( int argc, char *argv[] )
{
	char *fullname, *MyName = argv[0];
	char *cmd_str;

	config( 0 );

	MyName = strrchr( argv[0], DIR_DELIM_CHAR );
	if( !MyName ) {
		MyName = argv[0];
	} else {
		MyName++;
	}
	cmd_str = strchr( MyName, '_');
	if( !cmd_str ) {
		fprintf( stderr, "Error: unknown command %s\n", MyName );
		exit(1);
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
		exit(1);
	}

	if( argc == 1 ) {
		do_command( my_full_hostname() );
		exit( 0 );
	}

	for( argv++; *argv; argv++ ) {
		if( **argv == '-' ) {
			usage( MyName );
		}
		if( (fullname = get_full_hostname(*argv)) == NULL ) {
			fprintf( stderr, "%s: unknown host %s\n", MyName, *argv );
			continue;
		}
		do_command( fullname );
	}
	exit( 0 );
}


void
do_command( char *host )
{
	char		*daemonAddr;
	switch( dt ) {
	case MASTER:
		if ((daemonAddr = get_master_addr(host)) == NULL) {
			fprintf( stderr, "Can't find master address on %s\n", host );
			return;
		}
		break;
	case SCHEDD:
		if ((daemonAddr = get_schedd_addr(host)) == NULL) {
			fprintf( stderr, "Can't find schedd address on %s\n", host );
			return;
		}
		break;
	case STARTD:
		if ((daemonAddr = get_startd_addr(host)) == NULL) {
			fprintf( stderr, "Can't find startd address on %s\n", host );
			return;
		}
		break;
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

	printf( "Sent %s command to %s on %s\n", cmd_to_str(cmd), 
			daemonName, host );
}

extern "C" int SetSyscalls(){}
