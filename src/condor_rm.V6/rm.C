/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-2002 CONDOR Team, Computer Sciences Department, 
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

#include "condor_common.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "condor_network.h"
#include "condor_io.h"
#include "sched.h"
#include "alloc.h"
#include "get_daemon_addr.h"
#include "internet.h"
#include "condor_attributes.h"
#include "match_prefix.h"
#include "sig_install.h"
#include "condor_version.h"
#include "condor_ver_info.h"
#include "string_list.h"
#include "daemon.h"
#include "dc_schedd.h"
#include "condor_distribution.h"


char	*MyName;
int mode;
bool All = false;
bool had_error = false;
bool old_messages = false;

DCSchedd* schedd = NULL;

StringList* job_ids = NULL;

	// Prototypes of local interest
void addConstraint(const char *);
void procArg(const char*);
void usage();
void handleAll();
void handleConstraints( void );
ClassAd* doWorkByList( StringList* ids );
void printOldMessages( ClassAd* result_ad, StringList* ids );
void printNewMessages( ClassAd* result_ad, StringList* ids );

bool has_constraint;

MyString global_constraint;
StringList global_id_list;

void
usage()
{
	char word[10];
	switch( mode ) {
	case IDLE:
		sprintf( word, "Releases" );
		break;
	case HELD:
		sprintf( word, "Holds" );
		break;
	case REMOVED:
		sprintf( word, "Removes" );
		break;
	default:
		fprintf( stderr, "ERROR: Unknown mode: %d\n", mode );
		exit( 1 );
		break;
	}
	fprintf( stderr, "Usage: %s [options] [constraints]\n", MyName );
	fprintf( stderr, " where [options] is zero or more of:\n" );
	fprintf( stderr, "  -help               Display this message and exit\n" );
	fprintf( stderr, "  -version            Display version information and exit\n" );
	fprintf( stderr, "  -name schedd_name   Connect to the given schedd\n" );
	fprintf( stderr, "  -pool hostname      Use the given central manager to find daemons\n" );
	fprintf( stderr, "  -addr <ip:port>     Connect directly to the given \"sinful string\"\n" );
	fprintf( stderr, " and where [constraints] is one or more of:\n" );
	fprintf( stderr, "  cluster.proc        %s the given job\n", word );
	fprintf( stderr, "  cluster             %s the given cluster of jobs\n", word );
	fprintf( stderr, "  user                %s all jobs owned by user\n", word );
	fprintf( stderr, "  -all                %s all jobs "
			 "(Cannot be used with other constraints)\n", word );
	exit( 1 );
}


void
version( void )
{
	printf( "%s\n%s\n", CondorVersion(), CondorPlatform() );
	exit( 0 );
}


int
main( int argc, char *argv[] )
{
	char	*arg;
	char	**args = (char **)malloc(sizeof(char *)*(argc - 1)); // args 
	int					nArgs = 0;				// number of args 
	int					i;
	char*	cmd_str;
	char* pool = NULL;
	char* scheddName = NULL;
	char* scheddAddr = NULL;

		// Initialize our global variables
	has_constraint = false;

	myDistro->Init( argc, argv );
	MyName = strrchr( argv[0], DIR_DELIM_CHAR );
	if( !MyName ) {
		MyName = argv[0];
	} else {
		MyName++;
	}

	cmd_str = strchr( MyName, '_');
	if (cmd_str && strcmp(cmd_str, "_hold") == MATCH) {
		mode = HELD;
	} else if( cmd_str && strcmp( cmd_str, "_release" ) == MATCH ) {
		mode = IDLE;
	} else {
		mode = REMOVED;
	}

	config();


	if( argc < 2 ) {
		usage();
	}

#if !defined(WIN32)
	install_sig_handler(SIGPIPE, SIG_IGN );
#endif

	for( argv++; (arg = *argv); argv++ ) {
		if( arg[0] == '-' ) {
			if( ! arg[1] ) {
				usage();
			}
			switch( arg[1] ) {
			case 'd':
				// dprintf to console
				Termlog = 1;
				dprintf_config ("TOOL", 2 );
				break;
			case 'c':
				args[nArgs] = arg;
				nArgs++;
				argv++;
				if( ! *argv ) {
					fprintf( stderr, 
							 "%s: -constraint requires another argument\n", 
							 MyName);
					exit(1);
				}				
				args[nArgs] = *argv;
				nArgs++;
				break;
			case 'a':
				if( arg[2] && arg[2] == 'd' ) {
					argv++;
					if( ! *argv ) {
						fprintf( stderr, 
								 "%s: -addr requires another argument\n", 
								 MyName);
						exit(1);
					}				
					if( is_valid_sinful(*argv) ) {
						scheddAddr = strdup(*argv);
						if( ! scheddAddr ) {
							fprintf( stderr, "Out of Memory!\n" );
							exit(1);
						}
					} else {
						fprintf( stderr, 
								 "%s: \"%s\" is not a valid address\n",
								 MyName, *argv );
						fprintf( stderr, "Should be of the form "
								 "<ip.address.here:port>\n" );
						fprintf( stderr, 
								 "For example: <123.456.789.123:6789>\n" );
						exit( 1 );
					}
					break;
				}
				All = true;
				break;
			case 'n': 
				// use the given name as the schedd name to connect to
				argv++;
				if( ! *argv ) {
					fprintf( stderr, "%s: -name requires another argument\n", 
							 MyName);
					exit(1);
				}				
				if( !(scheddName = get_daemon_name(*argv)) ) { 
					fprintf( stderr, "%s: unknown host %s\n", 
							 MyName, get_host_part(*argv) );
					exit(1);
				}
				break;
			case 'p':
				// use the given name as the central manager to query
				argv++;
				if( ! *argv ) {
					fprintf( stderr, "%s: -pool requires another argument\n", 
							 MyName);
					exit(1);
				}				
				if( pool ) {
					free( pool );
				}
				pool = strdup( *argv );
				break;
			case 'v':
				version();
				break;
			default:
					// This gets hit for "-h", too
				usage();
				break;
			}
		} else {
			if( All ) {
					// If -all is set, there should be no other
					// constraint arguments.
				usage();
			}
			args[nArgs] = arg;
			nArgs++;
		}
	}

	if( ! (All || nArgs) ) {
			// We got no indication of what to remove
		usage();
	}

	old_messages = false;
	char* tmp = param( "TOOLS_PROVIDE_OLD_MESSAGES" );
	if( tmp ) {
		if( tmp[0] == 'T' || tmp[0] == 't' ) {
			old_messages = true;
		}
		free( tmp );
	}

		// We're done parsing args, now make sure we know how to
		// contact the schedd. 
	if( ! scheddAddr ) {
			// This will always do the right thing, even if either or
			// both of scheddName or pool are NULL.
		schedd = new DCSchedd( scheddName, pool );
	} else {
		schedd = new DCSchedd( scheddAddr );
	}
	if( ! schedd->locate() ) {
		fprintf( stderr, "%s: %s\n", MyName, schedd->error() ); 
		exit( 1 );
	}

		// If this schedd doesn't support the new protocol, give a
		// useful error message.
	CondorVersionInfo ver_info( schedd->version(), "SCHEDD" );
	if( ! ver_info.built_since_version(6, 3, 3) ) {
		fprintf( stderr, "The version of the condor_schedd you want to "
				 "communicate with is:\n%s\n", schedd->version() );
		fprintf( stderr, "It is too old to support this version of "
				 "%s:\n%s\n", MyName, CondorVersion() );
		fprintf( stderr, "To use this version of %s you must upgrade "
				 "the\n%s to at least version 6.3.3.\nAborting.\n",
				 MyName, schedd->idStr() ); 
		exit( 1 );
	}

		// Process the args so we do the work.
	if( All ) {
		handleAll();
	} else {
		for(i = 0; i < nArgs; i++) {
			if( match_prefix( args[i], "-constraint" ) ) {
				i++;
				addConstraint( args[i] );
			} else {
				procArg(args[i]);
			}
		}
	}

		// Deal with all the -constraint constraints
	handleConstraints();

		// Finally, do the actual work for all our args which weren't
		// constraints...
	if( job_ids ) {
		ClassAd* result_ad = doWorkByList( job_ids );
		if( old_messages ) {
			printOldMessages( result_ad, job_ids );
		} else {
				// happy day, we can use the new messages! :)
			printNewMessages( result_ad, job_ids );
		}
		delete( result_ad );
	}
	return had_error;
}



// For now, just return true if the constraint worked on at least
// one job, false if not.  Someday, we can fix up the tool to take
// advantage of all the slick info the schedd gives us back about this
// request.  
bool
doWorkByConstraint( const char* constraint )
{
	ClassAd* ad;
	bool rval = true;
	switch( mode ) {
	case IDLE:
		ad = schedd->releaseJobs( constraint, "via condor_release" );
		break;
	case REMOVED:
		ad = schedd->removeJobs( constraint, "via condor_rm" );
		break;
	case HELD:
		ad = schedd->holdJobs( constraint, "via condor_hold" );
		break;
	}
	if( ! ad ) {
		had_error = true;
		rval = false;
	} else {
		int result = FALSE;
		if( !ad->LookupInteger(ATTR_ACTION_RESULT, result) || !result ) {
			had_error = true;
			rval = false;
		}
	}
	return rval;
}


ClassAd*
doWorkByList( StringList* ids )
{
	ClassAd* rval;
	switch( mode ) {
	case IDLE:
		rval = schedd->releaseJobs( ids, "via condor_release" );
		break;
	case REMOVED:
		rval = schedd->removeJobs( ids, "via condor_rm" );
		break;
	case HELD:
		rval = schedd->holdJobs( ids, "via condor_hold" );
		break;
	}
	if( ! rval ) {
		had_error = true;
	} else {
		int result = FALSE;
		if( !rval->LookupInteger(ATTR_ACTION_RESULT, result) || !result ) {
			had_error = true;
		}
	}
	return rval;
}


void
procArg(const char* arg)
{
	int		c, p;								// cluster/proc #
	char*	tmp;

	char constraint[ATTRLIST_MAX_EXPRESSION];

	if(isdigit(*arg))
	// process by cluster/proc #
	{
		c = strtol(arg, &tmp, 10);
		if(c <= 0)
		{
			fprintf(stderr, "Invalid cluster # from %s.\n", arg);
			had_error = true;
			return;
		}
		if(*tmp == '\0')
		// delete the cluster
		{
			sprintf( constraint, "%s == %d", ATTR_CLUSTER_ID, c );
			if( doWorkByConstraint(constraint) ) {
				fprintf( old_messages ? stderr : stdout, 
						 "Cluster %d %s.\n", c,
						 (mode==REMOVED)?"has been marked for removal":
						 (mode==HELD)?"held":"released" );
			} else {
				fprintf( stderr, 
						 "Couldn't find/%s all jobs in cluster %d.\n",
						 (mode==REMOVED)?"remove":(mode==HELD)?"hold":
						 "release", c );
			}
			return;
		}
		if(*tmp == '.')
		{
			p = strtol(tmp + 1, &tmp, 10);
			if(p < 0)
			{
				fprintf( stderr, "Invalid proc # from %s.\n", arg);
				had_error = 1;
				return;
			}
			if(*tmp == '\0')
			// process a proc
			{
				if( ! job_ids ) {
					job_ids = new StringList();
				}
				job_ids->append( arg );
				return;
			}
		}
		fprintf( stderr, "Warning: unrecognized \"%s\" skipped.\n", arg );
		return;
	}
	else if(isalpha(*arg))
	// process by user name
	{
		sprintf( constraint, "%s == \"%s\"", ATTR_OWNER, arg );
		if( doWorkByConstraint(constraint) ) {
			fprintf( stdout, "User %s's job(s) %s.\n", arg,
					 (mode==REMOVED)?"have been marked for removal":
					 (mode==HELD)?"held":"released" );
		} else {
			fprintf( stderr, 
					 "Couldn't find/%s all of user %s's job(s).\n",
					 (mode==REMOVED)?"remove":(mode==HELD)?"hold":
					 "release", arg );
		}
	} else {
		fprintf( stderr, "Warning: unrecognized \"%s\" skipped\n", arg );
	}
}


void
addConstraint( const char *constraint )
{
	static bool has_clause = false;
	if( has_clause ) {
		global_constraint += " && (";
	} else {
		global_constraint += "(";
	}
	global_constraint += constraint;
	global_constraint += ")";

	has_clause = true;
	has_constraint = true;
}


void
handleAll()
{
	char constraint[128];
	sprintf( constraint, "%s >= 0", ATTR_CLUSTER_ID );

	if( doWorkByConstraint(constraint) ) {
		fprintf( stdout, "All jobs %s.\n",
				 (mode==REMOVED)?"marked for removal":
				 (mode==HELD)?"held":"released" );
	} else {
		fprintf( stderr, "Could not %s all jobs.\n",
				 (mode==REMOVED)?"remove":
				 (mode==HELD)?"hold":"release" );

	}
}


void
handleConstraints( void )
{
	if( ! has_constraint ) {
		return;
	}
	const char* tmp = global_constraint.Value();

	if( doWorkByConstraint(tmp) ) {
		fprintf( stdout, "Jobs matching constraint %s %s\n", tmp,
				 (mode==REMOVED)?"have been marked for removal":
				 (mode==HELD)?"held":"released" );
	} else {
		fprintf( stderr, 
				 "Couldn't find/%s all jobs matching constraint %s\n",
				 (mode==REMOVED)?"remove":(mode==HELD)?"hold":"release",
				 tmp );
	}
}


void
printOldFailure( PROC_ID job_id )
{
	fprintf( stderr, "Couldn't find/%s job %d.%d.\n",
			 (mode==REMOVED)?"remove":(mode==HELD)?"hold":"release", 
			 job_id.cluster, job_id.proc );
}


void
printOldMessage( PROC_ID job_id, action_result_t result )
{
	switch( result ) {
	case AR_SUCCESS:
		fprintf( stdout, "Job %d.%d %s.\n", 
				 job_id.cluster, job_id.proc, 
				 (mode==REMOVED)?"marked for removal":
				 (mode==HELD)?"held":"released" );
		break;

	case AR_NOT_FOUND:
		if( mode==IDLE ) {
			fprintf( stderr, "Couldn't access job queue for %d.%d\n", 
					 job_id.cluster, job_id.proc );
			break;
		} 
		printOldFailure( job_id );
		break;

	case AR_PERMISSION_DENIED: 
		printOldFailure( job_id );
		break;

	case AR_BAD_STATUS:
		if( mode == IDLE ) {
			fprintf( stderr, "Job %d.%d not held to be released\n", 
					 job_id.cluster, job_id.proc );
			break;
		} 
		printOldFailure( job_id );
		break;

	case AR_ALREADY_DONE:
			// The old tool allowed you to repeatedly hold or remove
			// the same job over and over again...
		fprintf( stdout, "Job %d.%d %s.\n", 
				 job_id.cluster, job_id.proc, 
				 (mode==REMOVED)?"marked for removal":
				 (mode==HELD)?"held":"released" );
		break;

	case AR_ERROR:
		printOldFailure( job_id );
		break;
	}
}


void
printOldMessages( ClassAd* result_ad, StringList* ids )
{
	char* tmp;
	PROC_ID job_id;
	action_result_t result;

	JobActionResults results;
	results.readResults( result_ad );

	ids->rewind();
	while( (tmp = ids->next()) ) {
		job_id = getProcByString( tmp );
		result = results.getResult( job_id );
		printOldMessage( job_id, result );
	}
}


void
printNewMessages( ClassAd* result_ad, StringList* ids )
{
	char* tmp;
	char* msg;
	PROC_ID job_id;
	bool rval;

	JobActionResults results;
	results.readResults( result_ad );

	ids->rewind();
	while( (tmp = ids->next()) ) {
		job_id = getProcByString( tmp );
		rval = results.getResultString( job_id, &msg );
		if( rval ) {
			fprintf( stdout, "%s\n", msg );
		} else {
			fprintf( stderr, "%s\n", msg );
		}
		free( msg );
	}
}

