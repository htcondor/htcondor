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

 

/***********************************************************************
*
* Print declarations from the condor config files, condor_config and
* condor_config.local.  Declarations in files specified by
* LOCAL_CONFIG_FILE override those in the global config file, so this
* prints the same declarations found by condor programs using the
* config() routine.
*
* In addition, the configuration of remote machines can be queried.
* You can specify either a name or sinful string you want to connect
* to, what kind of daemon you want to query, and what pool to query to
* find the specified daemon.
*
***********************************************************************/

#include "condor_common.h"
#include "condor_config.h"
#include "condor_io.h"
#include "condor_uid.h"
#include "match_prefix.h"
#include "string_list.h"
#include "get_daemon_addr.h"
#include "daemon_types.h"

char	*MyName;
char	*mySubSystem = NULL;
StringList params;
daemonType dt = DT_MASTER;

// The pure-tools (PureCoverage, Purify, etc) spit out a bunch of
// stuff to stderr, which is where we normally put our error
// messages.  To enable config_val.test to produce easily readable
// output, even with pure-tools, we just write everything to stdout.  
#if defined( PURE_DEBUG ) 
#	define stderr stdout
#endif

enum PrintType {CONDOR_OWNER, CONDOR_TILDE, CONDOR_NONE};
enum ModeType {CONDOR_QUERY, CONDOR_SET, CONDOR_UNSET,
			   CONDOR_RUNTIME_SET, CONDOR_RUNTIME_UNSET};


// On some systems, the output from config_val sometimes doesn't show
// up unless we explicitly flush before we exit.
void
my_exit( int status )
{
	fflush( stdout );
	fflush( stderr );
	exit( status );
}


void
usage()
{
	fprintf( stderr, "Usage: %s [options] variable [variable] ...\n", MyName );
	fprintf( stderr,
			 "   or: %s [options] -set variable value [variable value] ...\n",
			 MyName );
	fprintf( stderr,
			 "   or: %s [options] -rset variable value [variable value] ...\n",
			 MyName );
	fprintf( stderr, "   or: %s [options] -unset variable [variable] ...\n",
			 MyName );
	fprintf( stderr, "   or: %s [options] -runset variable [variable] ...\n",
			 MyName );
	fprintf( stderr, "   or: %s [options] -tilde\n", MyName );
	fprintf( stderr, "   or: %s [options] -owner\n", MyName );
	fprintf( stderr, "\n   Valid options are:\n" );
	fprintf( stderr, "   -name daemon_name\t(query the specified daemon for its configuration)\n" );
	fprintf( stderr, "   -pool hostname\t(use the given central manager to find daemons)\n" );
	fprintf( stderr, "   -address <ip:port>\t(connect to the given ip/port)\n" );
	fprintf( stderr, "   -host hostname\t(connect to the given hostname)\n" );
	fprintf( stderr, "   -set\t\t\t(set a persistent config file expression)\n" );
	fprintf( stderr, "   -rset\t\t(set a runtime config file expression\n" );

	fprintf( stderr, "   -unset\t\t(unset a persistent config file expression)\n" );
	fprintf( stderr, "   -runset\t\t(unset a runtime config file expression)\n" );

	fprintf( stderr, "   -master\t\t(query the master [default])\n" );
	fprintf( stderr, "   -schedd\t\t(query the schedd)\n" );
	fprintf( stderr, "   -startd\t\t(query the startd)\n" );
	fprintf( stderr, "   -collector\t\t(query the collector)\n" );
	fprintf( stderr, "   -negotiator\t\t(query the negotiator)\n" );
	fprintf( stderr, "   -tilde\t\t(return the path to the Condor home directory)\n" );
	fprintf( stderr, "   -owner\t\t(return the owner of the condor_config_val process)\n" );
	my_exit( 1 );
}


char* GetRemoteParam( char*, char*, char*, char* );
void  SetRemoteParam( char*, char*, char*, char*, char*, ModeType );

main( int argc, char* argv[] )
{
	char	*value, *tmp, *host = NULL;
	char	*addr = NULL, *name = NULL, *pool = NULL;
	int		i;
	
	PrintType pt = CONDOR_NONE;
	ModeType mt = CONDOR_QUERY;

	MyName = argv[0];

	for( i=1; i<argc; i++ ) {
		if( match_prefix( argv[i], "-host" ) ) {
			if( argv[i + 1] ) {
				host = strdup( argv[++i] );
			} else {
				usage();
			}
		} else if( match_prefix( argv[i], "-name" ) ) {
			if( argv[i + 1] ) {
				i++;
				if( (tmp = get_daemon_name(argv[i])) ) { 
					name = strdup( tmp );
				} else {
					fprintf( stderr, "%s: unknown host %s\n", MyName, 
							 get_host_part(argv[i]) );
					my_exit( 1 );
				}
			} else {
				usage();
			}
		} else if( match_prefix( argv[i], "-address" ) ) {
			if( argv[i + 1] ) {
				i++;
				if( is_valid_sinful(argv[i]) ) {
					addr = strdup( argv[i] );
				} else {
					fprintf( stderr, "%s: invalid address %s\n"
							 "Address must be of the form \"<111.222.333.444:555>\n"
							 "   where 111.222.333.444 is the ip address and"
							 "555 is the port\n  you wish to connect to (the"
							 "punctuation is important.\n", MyName, argv[i] );
					my_exit( 1 );
				}
			} else {
				usage();
			}
		} else if( match_prefix( argv[i], "-pool" ) ) {
			if( argv[i + 1] ) {
				i++;
				if( (tmp = get_daemon_name(argv[i])) ) {
					pool = strdup( tmp );
				} else {
					fprintf( stderr, "%s: unknown host %s\n", MyName, 
							 get_host_part(argv[i]) );
					my_exit( 1 );
				}
			} else {
				usage();
			}
		} else if( match_prefix( argv[i], "-owner" ) ) {
			pt = CONDOR_OWNER;
		} else if( match_prefix( argv[i], "-tilde" ) ) {
			pt = CONDOR_TILDE;
		} else if( match_prefix( argv[i], "-master" ) ) {
			dt = DT_MASTER;
		} else if( match_prefix( argv[i], "-schedd" ) ) {
			dt = DT_SCHEDD;
		} else if( match_prefix( argv[i], "-startd" ) ) {
			dt = DT_STARTD;
		} else if( match_prefix( argv[i], "-collector" ) ) {
			dt = DT_COLLECTOR;
		} else if( match_prefix( argv[i], "-negotiator" ) ) {
			dt = DT_NEGOTIATOR;
		} else if( match_prefix( argv[i], "-set" ) ) {
			mt = CONDOR_SET;
		} else if( match_prefix( argv[i], "-unset" ) ) {
			mt = CONDOR_UNSET;
		} else if( match_prefix( argv[i], "-rset" ) ) {
			mt = CONDOR_RUNTIME_SET;
		} else if( match_prefix( argv[i], "-runset" ) ) {
			mt = CONDOR_RUNTIME_UNSET;
		} else if( match_prefix( argv[i], "-" ) ) {
			usage();
		} else {
			params.append( strdup( argv[i] ) );
		}
	}

		// If we didn't get told what subsystem we should use, set it
		// to "TOOL".
	if( !mySubSystem ) {
		mySubSystem = strdup( "TOOL" );
	}

		// Want to do this before we try to find the address of a
		// remote daemon, since if there's no -pool option, we need to
		// param() for the COLLECTOR_HOST to contact.
	if( host ) {
		config_host( 0, host );
	} else {
		config( 0 );
	}

	if( name || pool || mt != CONDOR_QUERY ) {
		addr = get_daemon_addr( dt, name, pool );
		if( ! addr ) {
			fprintf( stderr, "Can't find address for %s %s\n", 
					 daemon_string(dt), name );
			fprintf( stderr, "Perhaps you need to query another pool.\n" );
			my_exit( 1 );
		}
	}

	if( pt == CONDOR_TILDE ) {
		if( (tmp = get_tilde()) ) {
			printf( "%s\n", tmp );
			my_exit( 0 );
		} else {
			fprintf( stderr,
					 "Error: Specified -tilde but can't find %s\n",
					 "condor's home directory." );
			my_exit( 1 );
		}
	}		

	if( pt == CONDOR_OWNER ) {
		printf( "%s\n", get_condor_username() );
		my_exit( 0 );
	}

	params.rewind();

	if( ! params.number() ) {
		usage();
	}

	while( (tmp = params.next()) ) {
		if( mt == CONDOR_SET || mt == CONDOR_RUNTIME_SET ) {
			value = params.next();
			SetRemoteParam( name, addr, pool, tmp, value, mt );
		} else if( mt == CONDOR_UNSET || mt == CONDOR_RUNTIME_UNSET ) {
			SetRemoteParam( name, addr, pool, tmp, "", mt );
		} else {
			if( name || pool || addr ) {
				value = GetRemoteParam( name, addr, pool, tmp );
			} else {
				value = param( tmp );
			}
			if( value == NULL ) {
				fprintf(stderr, "Not defined: %s\n", tmp);
				my_exit( 1 );
			} else {
				printf("%s\n", value);
				free( value );
			}
		}
	}
	my_exit( 0 );
}


char*
GetRemoteParam( char* name, char* addr, char* pool, char* param_name ) 
{
	ReliSock s;
	s.timeout( 30 );
	char	*val = NULL;
	int 	cmd = CONFIG_VAL;
	
	if( !name ) {
		name = "";
	}

	if( ! s.connect( addr, 0 ) ) {
		fprintf( stderr, "Can't connect to %s on %s %s\n", 
				 daemon_string(dt), name, addr );
		my_exit(1);
	}

	s.encode();
	if( !s.code(cmd) ) {
		fprintf( stderr, "Can't send command CONFIG_VAL (%d)\n", cmd );
		return NULL;
	}
	if( !s.code(param_name) ) {
		fprintf( stderr, "Can't send request (%s)\n", param_name );
		return NULL;
	}
	if( !s.end_of_message() ) {
		fprintf( stderr, "Can't send end of message\n" );
		return NULL;
	}

	s.decode();
	if( !s.code(val) ) {
		fprintf( stderr, "Can't receive reply from %s on %s %s\n",
				 daemon_string(dt), name, addr );
		return NULL;
	}
	if( !s.end_of_message() ) {
		fprintf( stderr, "Can't receive end of message\n" );
		return NULL;
	}
	return val;
}


void
SetRemoteParam( char* name, char* addr, char* pool, char* param_name,
				char* param_value, ModeType mt )
{
	int cmd, rval;
	ReliSock s;

	s.timeout( 30 );

	switch (mt) {
	case CONDOR_SET:
	case CONDOR_UNSET:
		cmd = DC_CONFIG_PERSIST;
		break;
	case CONDOR_RUNTIME_SET:
	case CONDOR_RUNTIME_UNSET:
		cmd = DC_CONFIG_RUNTIME;
		break;
	default:
		fprintf( stderr, "Unknown command type %d\n", (int)mt );
		my_exit( 1 );
	}

	if( !name ) {
		name = "";
	}

	if( ! s.connect( addr, 0 ) ) {
		fprintf( stderr, "Can't connect to %s on %s %s\n", 
				 daemon_string(dt), name, addr );
		my_exit(1);
	}

	s.encode();
	if( !s.code(cmd) ) {
		fprintf( stderr, "Can't send DC_CONFIG command (%d)\n", cmd );
		my_exit(1);
	}
	if( !s.code(param_name) ) {
		fprintf( stderr, "Can't send param name (%s)\n", param_name );
		my_exit(1);
	}
	char *buf = NULL;
	if (param_value && param_value[0]) {
		buf = (char *)malloc(strlen(param_name)+strlen(param_value)+5);
		sprintf(buf, "%s : %s\n", param_name, param_value);
		if( !s.code(buf) ) {
			fprintf( stderr, "Can't send config setting (%s)\n", buf );
			free(buf);
			my_exit(1);
		}
	} else {
		if( !s.put("") ) {
			fprintf( stderr, "Can't send config setting\n" );
			my_exit(1);
		}
	}
	if( !s.end_of_message() ) {
		fprintf( stderr, "Can't send end of message\n" );
		if (buf) free(buf);
		my_exit(1);
	}

	s.decode();
	if( !s.code(rval) ) {
		fprintf( stderr, "Can't receive reply from %s on %s %s\n",
				 daemon_string(dt), name, addr );
		if (buf) free(buf);
		my_exit(1);
	}
	if( !s.end_of_message() ) {
		fprintf( stderr, "Can't receive end of message\n" );
		if (buf) free(buf);
		my_exit(1);
	}
	if (buf) buf[strlen(buf)-1] = '\0';	// remove newline
	if (rval < 0) {
		if (buf) {
			fprintf( stderr, "Attempt to set configuration \"%s\" on %s %s "
					 "%s failed.\n",
					 buf, daemon_string(dt), name, addr );
			free(buf);
		} else {
			fprintf( stderr, "Attempt to unset configuration \"%s\" on %s %s "
					 "%s failed.\n",
					 param_name, daemon_string(dt), name, addr );
		}
		my_exit(1);
	}
	if (buf) {
		fprintf( stderr, "Successfully set configuration \"%s\" on %s %s "
				 "%s.\n",
				 buf, daemon_string(dt), name, addr );
		free(buf);
	} else {
		fprintf( stderr, "Successfully unset configuration \"%s\" on %s %s "
				 "%s.\n",
				 param_name, daemon_string(dt), name, addr );
	}
}
