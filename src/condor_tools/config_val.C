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
#include "get_full_hostname.h"
#include "daemon_types.h"
#include "internet.h"

char	*MyName;
char	*mySubSystem = NULL;
StringList params;
daemon_t dt = DT_MASTER;
bool	mixedcase = false;

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
			 "   or: %s [options] -set string [string] ...\n",
			 MyName );
	fprintf( stderr,
			 "   or: %s [options] -rset string [string] ...\n",
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
void  SetRemoteParam( char*, char*, char*, char*, ModeType );

int
main( int argc, char* argv[] )
{
	char	*value, *tmp, *foo, *host = NULL;
	char	*addr = NULL, *name = NULL, *pool = NULL;
	int		i;
	bool	ask_a_daemon = false;
	
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
				name = get_daemon_name( argv[i] );
				if( ! name ) {
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
				pool = get_full_hostname( (const char *) argv[i] );
				if( ! pool ) {
					fprintf( stderr, "%s: unknown host %s\n", MyName, 
							 argv[i] );
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
			ask_a_daemon = true;
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
		} else if( match_prefix( argv[i], "-mixedcase" ) ) {
			mixedcase = true;
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

		// We need to do this before we call config().  A) we don't
		// need config() to find this out and B) config() will fail if
		// all the config files aren't setup yet, and we use
		// "condor_config_val -owner" for condor_init.  Derek 9/23/99 
	if( pt == CONDOR_OWNER ) {
		printf( "%s\n", get_condor_username() );
		my_exit( 0 );
	}

		// We need to handle -tilde before we call config() for the
		// same reasons listed above for -owner. -Derek 2/16/00
	if( pt == CONDOR_TILDE ) {
		if( (tmp = get_tilde()) ) {
			printf( "%s\n", tmp );
			my_exit( 0 );
		} else {
			fprintf( stderr, "Error: Specified -tilde but can't find " 
					 "condor's home directory\n" );
			my_exit( 1 );
		}
	}		

		// Want to do this before we try to find the address of a
		// remote daemon, since if there's no -pool option, we need to
		// param() for the COLLECTOR_HOST to contact.
	if( host ) {
		config_host( host );
	} else {
		config();
	}

	if( name || pool || mt != CONDOR_QUERY || dt != DT_MASTER ) {
		ask_a_daemon = true;
	}

	if( ask_a_daemon ) {
		addr = get_daemon_addr( dt, name, pool );
		if( ! addr ) {
			fprintf( stderr, "Can't find address for this %s\n", 
					 daemonString(dt) );
			fprintf( stderr, "Perhaps you need to query another pool.\n" );
			my_exit( 1 );
		}
	}

	params.rewind();

	if( ! params.number() ) {
		usage();
	}

	while( (tmp = params.next()) ) {
		if( mt == CONDOR_SET || mt == CONDOR_RUNTIME_SET ||
			mt == CONDOR_UNSET || mt == CONDOR_RUNTIME_UNSET ) {
			SetRemoteParam( name, addr, pool, tmp, mt );
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
	return 0;
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
				 daemonString(dt), name, addr );
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
				 daemonString(dt), name, addr );
		return NULL;
	}
	if( !s.end_of_message() ) {
		fprintf( stderr, "Can't receive end of message\n" );
		return NULL;
	}
	return val;
}


void
SetRemoteParam( char* name, char* addr, char* pool, char* param_value,
				ModeType mt )
{
	int cmd, rval;
	ReliSock s;
	bool set = false;

		// We need to know two things: what command to send, and (for
		// error messages) if we're setting or unsetting.  Since our
		// bool "set" defaults to false, we only hit the "set = true"
		// statements when we want to, and fall through to also set
		// the right commands in those cases... Derek Wright 9/1/99
	switch (mt) {
	case CONDOR_SET:
		set = true;
	case CONDOR_UNSET:
		cmd = DC_CONFIG_PERSIST;
		break;
	case CONDOR_RUNTIME_SET:
		set = true;
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

		// Now, process our strings for sanity.
	char* param_name = strdup( param_value );
	char* tmp = NULL;

	if( set ) {
		tmp = strchr( param_name, ':' );
		if( ! tmp ) {
			tmp = strchr( param_name, '=' );
		}
		if( ! tmp ) {
			fprintf( stderr, "%s: Can't set configuration value (\"%s\")\n" 
					 "You must specify \"macro_name = value\" or " 
					 "\"expr_name : value\"\n", MyName, param_name );
			my_exit( 1 );
		}
			// If we're still here, we found a ':' or a '=', so, now,
			// chop off everything except the attribute name
			// (including spaces), so we can send that seperately. 
		do {
			*tmp = '\0';
			tmp--;
		} while( *tmp == ' ' );
	} else {
			// Want to do different sanity checking.
		if( (tmp = strchr(param_name, ':')) || 
			(tmp = strchr(param_name, '=')) ) {
			fprintf( stderr, "%s: Can't unset configuration value (\"%s\")\n" 
					 "To unset, you only specify the name of the attribute\n", 
					 MyName, param_name );
			my_exit( 1 );
		}
		tmp = strchr( param_name, ' ' );
		if( tmp ) {
			*tmp = '\0';
		}
	}

		// At this point, in either set or unset mode, param_name
		// should hold a valid name, so do a final check to make sure
		// there are no spaces.
	if( (tmp = strchr(param_name, ' ')) ) {
		fprintf( stderr, 
				 "%s: Error: Configuration variable names cannot contain spaces\n",
				 MyName );
		my_exit( 1 );
	}

	if (!mixedcase) {
		lower_case(param_name);		// make the config name case insensitive
	}

		// We need a version with a newline at the end to make
		// everything cool at the other end.
	char* buf = (char*)malloc( strlen(param_value) + 2 );
	sprintf( buf, "%s\n", param_value );

	s.timeout( 30 );
	if( ! s.connect( addr, 0 ) ) {
		fprintf( stderr, "Can't connect to %s on %s %s\n", 
				 daemonString(dt), name, addr );
		my_exit(1);
	}

	s.encode();
	if( !s.code(cmd) ) {
		fprintf( stderr, "Can't send DC_CONFIG command (%d)\n", cmd );
		my_exit(1);
	}
	if( !s.code(param_name) ) {
		fprintf( stderr, "Can't send config name (%s)\n", param_name );
		my_exit(1);
	}
	if( set ) {
		if( !s.code(param_value) ) {
			fprintf( stderr, "Can't send config setting (%s)\n", param_value );
			my_exit(1);
		}
		if( !s.put('\n') ) {
			fprintf( stderr, "Can't send newline\n" );
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
		my_exit(1);
	}

	s.decode();
	if( !s.code(rval) ) {
		fprintf( stderr, "Can't receive reply from %s on %s %s\n",
				 daemonString(dt), name, addr );
		my_exit(1);
	}
	if( !s.end_of_message() ) {
		fprintf( stderr, "Can't receive end of message\n" );
		my_exit(1);
	}
	if (rval < 0) {
		if (set) {
			fprintf( stderr, "Attempt to set configuration \"%s\" on %s %s "
					 "%s failed.\n",
					 param_value, daemonString(dt), name, addr );
		} else {
			fprintf( stderr, "Attempt to unset configuration \"%s\" on %s %s "
					 "%s failed.\n",
					 param_value, daemonString(dt), name, addr );
		}
		my_exit(1);
	}
	if (set) {
		fprintf( stdout, "Successfully set configuration \"%s\" on %s %s "
				 "%s.\n",
				 param_value, daemonString(dt), name, addr );
	} else {
		fprintf( stdout, "Successfully unset configuration \"%s\" on %s %s "
				 "%s.\n",
				 param_value, daemonString(dt), name, addr );
	}
	free( buf );
	free( param_name );
}
