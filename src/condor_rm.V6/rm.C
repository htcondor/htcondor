/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "condor_network.h"
#include "condor_io.h"
#include "get_daemon_name.h"
#include "internet.h"
#include "condor_attributes.h"
#include "match_prefix.h"
#include "sig_install.h"
#include "condor_version.h"
#include "condor_ver_info.h"
#include "string_list.h"
#include "daemon.h"
#include "dc_schedd.h"
#include "dc_collector.h"
#include "condor_distribution.h"
#include "CondorError.h"


char	*MyName;
char 	*actionReason = NULL;
JobAction mode;
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
ClassAd* doWorkByList( StringList* ids, CondorError * errstack );
void printOldMessages( ClassAd* result_ad, StringList* ids );
void printNewMessages( ClassAd* result_ad, StringList* ids );

bool has_constraint;

MyString global_constraint;
StringList global_id_list;

const char* 
actionWord( JobAction action, bool past )
{
	switch( action ) {
	case JA_RELEASE_JOBS:
		return past ? "released" : "release";
		break;

	case JA_HOLD_JOBS:
		return past ? "held" : "hold";
		break;

	case JA_REMOVE_JOBS:
	case JA_REMOVE_X_JOBS:
		return past ? "removed" : "remove";
		break;

	case JA_VACATE_JOBS:
		return past ? "vacated" : "vacate";
		break;

	case JA_VACATE_FAST_JOBS:
		return past ? "fast-vacated" : "fast-vacate";
		break;

	default:
		fprintf( stderr, "ERROR: Unknown action: %d\n", action );
		exit( 1 );
		break;
	}
	return NULL;
}


void
usage()
{
	char word[32];
	sprintf( word, getJobActionString(mode) );
	fprintf( stderr, "Usage: %s [options] [constraints]\n", MyName );
	fprintf( stderr, " where [options] is zero or more of:\n" );
	fprintf( stderr, "  -help               Display this message and exit\n" );
	fprintf( stderr, "  -version            Display version information and exit\n" );

// i'm not sure we want -debug documented.  if we change our minds, we
// should just uncomment the next line
//	fprintf( stderr, "  -debug              Display debugging information while running\n" );

	fprintf( stderr, "  -name schedd_name   Connect to the given schedd\n" );
	fprintf( stderr, "  -pool hostname      Use the given central manager to find daemons\n" );
	fprintf( stderr, "  -addr <ip:port>     Connect directly to the given \"sinful string\"\n" );
	if( mode == JA_REMOVE_JOBS || mode == JA_REMOVE_X_JOBS ) {
		fprintf( stderr, "  -reason reason      Use the given RemoveReason\n");
	} else if( mode == JA_RELEASE_JOBS ) {
		fprintf( stderr, "  -reason reason      Use the given ReleaseReason\n");
	} else if( mode == JA_HOLD_JOBS ) {
		fprintf( stderr, "  -reason reason      Use the given HoldReason\n");
	}

	if( mode == JA_REMOVE_JOBS || mode == JA_REMOVE_X_JOBS ) {
		fprintf( stderr,
				     "  -forcex             Force the immediate local removal of jobs in the X state\n"
		         "                      (only affects jobs already being removed)\n" );
	}
	if( mode == JA_VACATE_JOBS || mode == JA_VACATE_FAST_JOBS ) {
		fprintf( stderr,
				     "  -fast               Use a fast vacate (hardkill)\n" );
	}
	fprintf( stderr, " and where [constraints] is one or more of:\n" );
	fprintf( stderr, "  cluster.proc        %s the given job\n", word );
	fprintf( stderr, "  cluster             %s the given cluster of jobs\n", word );
	fprintf( stderr, "  user                %s all jobs owned by user\n", word );
	fprintf( stderr, "  -constraint expr    %s all jobs matching the boolean expression\n", word );
	fprintf( stderr, "  -all                %s all jobs "
			 "(cannot be used with other constraints)\n", word );
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
	DCCollector* pool = NULL;
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

	// we match modes based on characters after the '_'. This means
	// 'condor_hold.exe' or 'condor_hold_wrapped' are all legal argv[0]'s
	// for condor_hold.

	if (cmd_str && strncmp( cmd_str, "_hold", strlen("_hold") ) == MATCH) { 

		mode = JA_HOLD_JOBS;

	} else if ( cmd_str && 
			strncmp( cmd_str, "_release", strlen("_release") ) == MATCH ) {

		mode = JA_RELEASE_JOBS;

	} else if ( cmd_str && 
			strncmp( cmd_str, "_rm", strlen("_rm") ) == MATCH ) {

		mode = JA_REMOVE_JOBS;

	} else if( cmd_str && ! strncmp(cmd_str, "_vacate_job",
									strlen("_vacate_job")) ) {  

		mode = JA_VACATE_JOBS;

	} else {
		// don't know what mode we're using, so bail.
		fprintf( stderr, "Unrecognized command name, \"%s\"\n", MyName ); 
		usage();
	}

	config();


	if( argc < 2 ) {
			// We got no indication of what to act on
		fprintf( stderr, "You did not specify any jobs\n" ); 
		usage();
	}

#if !defined(WIN32)
	install_sig_handler(SIGPIPE, SIG_IGN );
#endif

	for( argv++; (arg = *argv); argv++ ) {
		if( arg[0] == '-' ) {
            if (match_prefix(arg, "-debug")) {
				// dprintf to console
				Termlog = 1;
				dprintf_config ("TOOL", 2 );
            } else if (match_prefix(arg, "-constraint")) {
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
            } else if (match_prefix(arg, "-all")) {
                All = true;
            } else if (match_prefix(arg, "-addr")) {
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
                        fprintf( stderr, "Out of memory!\n" );
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
			} else if (match_prefix(arg, "-reason")) {
				argv++;
				if( ! *argv ) {
					fprintf( stderr, 
							 "%s: -reason requires another argument\n", 
							 MyName);
					exit(1);
				}		
				actionReason = strdup(*argv);		
				if( ! actionReason ) {
					fprintf( stderr, "Out of memory!\n" );
					exit(1);
				}
            } else if (match_prefix(arg, "-forcex")) {
				if( mode == JA_REMOVE_JOBS ) {
					mode = JA_REMOVE_X_JOBS;
				} else {
                    fprintf( stderr, 
                             "-forcex is only valid with condor_rm\n" );
					usage();
				}
            } else if (match_prefix(arg, "-fast")) {
				if( mode == JA_VACATE_JOBS ) {
					mode = JA_VACATE_FAST_JOBS;
				} else {
                    fprintf( stderr, 
                             "-fast is only valid with condor_vacate_job\n" );
					usage();
				}
            } else if (match_prefix(arg, "-name")) {
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
            } else if (match_prefix(arg, "-pool")) {
				// use the given name as the central manager to query
				argv++;
				if( ! *argv ) {
					fprintf( stderr, "%s: -pool requires another argument\n", 
							 MyName);
					exit(1);
				}				
				if( pool ) {
					delete pool;
				}
				pool = new DCCollector( *argv );
				if( ! pool->addr() ) {
					fprintf( stderr, "%s: %s\n", MyName, pool->error() );
					exit(1);
				}
            } else if (match_prefix(arg, "-version")) {
				version();
            } else if (match_prefix(arg, "-help")) {
				usage();
            } else {
				fprintf( stderr, "Unrecognized option: %s\n", arg ); 
				usage();
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
			// We got no indication of what to act on
		fprintf( stderr, "You did not specify any jobs\n" ); 
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

		// Pick the default reason if the user didn't specify one
	if( actionReason == NULL ) {
		switch( mode ) {
		case JA_RELEASE_JOBS:
			actionReason = "via condor_release";
			break;
		case JA_REMOVE_X_JOBS:
			actionReason = "via condor_rm -forcex";
			break;
		case JA_REMOVE_JOBS:
			actionReason = "via condor_rm";
			break;
		case JA_HOLD_JOBS:
			actionReason = "via condor_hold";
			break;
		default:
			actionReason = NULL;
		}
	}

		// We're done parsing args, now make sure we know how to
		// contact the schedd. 
	if( ! scheddAddr ) {
			// This will always do the right thing, even if either or
			// both of scheddName or pool are NULL.
		schedd = new DCSchedd( scheddName, pool ? pool->addr() : NULL );
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
		CondorError errstack;
		ClassAd* result_ad = doWorkByList( job_ids, &errstack );
		if (had_error) {
			fprintf( stderr, "%s\n", errstack.getFullText(true) );
		}
		if( old_messages ) {
			printOldMessages( result_ad, job_ids );
		} else {
				// happy day, we can use the new messages! :)
			printNewMessages( result_ad, job_ids );
		}
		delete( result_ad );
	}

		// If releasing jobs, and no errors happened, do a 
		// reschedule command now.
	if ( mode == JA_RELEASE_JOBS && had_error == false ) {
		Daemon  my_schedd(DT_SCHEDD, NULL, NULL);
		CondorError errstack;
		if (!my_schedd.sendCommand(RESCHEDULE, Stream::safe_sock, 0, &errstack)) {
			fprintf( stderr, "%s\n", errstack.getFullText(true) );
		}
	}

	return had_error;
}



// For now, just return true if the constraint worked on at least
// one job, false if not.  Someday, we can fix up the tool to take
// advantage of all the slick info the schedd gives us back about this
// request.  
bool
doWorkByConstraint( const char* constraint, CondorError * errstack )
{
	ClassAd* ad;
	bool rval = true;
	switch( mode ) {
	case JA_RELEASE_JOBS:
		ad = schedd->releaseJobs( constraint, actionReason, errstack );
		break;
	case JA_REMOVE_X_JOBS:
		ad = schedd->removeXJobs( constraint, actionReason,
								  errstack );
		break;
	case JA_VACATE_JOBS:
		ad = schedd->vacateJobs( constraint, VACATE_GRACEFUL, errstack );
		break;
	case JA_VACATE_FAST_JOBS:
		ad = schedd->vacateJobs( constraint, VACATE_FAST, errstack );
		break;
	case JA_REMOVE_JOBS:
		ad = schedd->removeJobs( constraint, actionReason, errstack );
		break;
	case JA_HOLD_JOBS:
		ad = schedd->holdJobs( constraint, actionReason, errstack );
		break;
	default:
		EXCEPT( "impossible: unknown mode in doWorkByConstraint" );
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
doWorkByList( StringList* ids, CondorError *errstack )
{
	ClassAd* rval;
	switch( mode ) {
	case JA_RELEASE_JOBS:
		rval = schedd->releaseJobs( ids, actionReason, errstack );
		break;
	case JA_REMOVE_X_JOBS:
		rval = schedd->removeXJobs( ids, actionReason, errstack );
		break;
	case JA_VACATE_JOBS:
		rval = schedd->vacateJobs( ids, VACATE_GRACEFUL, errstack );
		break;
	case JA_VACATE_FAST_JOBS:
		rval = schedd->vacateJobs( ids, VACATE_FAST, errstack );
		break;
	case JA_REMOVE_JOBS:
		rval = schedd->removeJobs( ids, actionReason, errstack );
		break;
	case JA_HOLD_JOBS:
		rval = schedd->holdJobs( ids, actionReason, errstack );
		break;
	default:
		EXCEPT( "impossible: unknown mode in doWorkByList" );
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
			CondorError errstack;
			sprintf( constraint, "%s == %d", ATTR_CLUSTER_ID, c );
			if( doWorkByConstraint(constraint, &errstack) ) {
				fprintf( old_messages ? stderr : stdout, 
						 "Cluster %d %s.\n", c,
						 (mode == JA_REMOVE_JOBS) ?
						 "has been marked for removal" :
						 (mode == JA_REMOVE_X_JOBS) ?
						 "has been removed locally (remote state unknown)" :
						 actionWord(mode,true) );
			} else {
				fprintf( stderr, "%s\n", errstack.getFullText(true) );
				fprintf( stderr, 
						 "Couldn't find/%s all jobs in cluster %d.\n",
						 actionWord(mode,false), c );
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
		CondorError errstack;
		sprintf( constraint, "%s == \"%s\"", ATTR_OWNER, arg );
		if( doWorkByConstraint(constraint, &errstack) ) {
			fprintf( stdout, "User %s's job(s) %s.\n", arg,
					 (mode == JA_REMOVE_JOBS) ?
					 "have been marked for removal" :
					 (mode == JA_REMOVE_X_JOBS) ?
					 "have been removed locally (remote state unknown)" :
					 actionWord(mode,true) );
		} else {
			fprintf( stderr, "%s\n", errstack.getFullText(true) );
			fprintf( stderr, 
					 "Couldn't find/%s all of user %s's job(s).\n",
					 actionWord(mode,false), arg );
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

	CondorError errstack;
	if( doWorkByConstraint(constraint, &errstack) ) {
		fprintf( stdout, "All jobs %s.\n",
				 (mode == JA_REMOVE_JOBS) ?
				 "marked for removal" :
				 (mode == JA_REMOVE_X_JOBS) ?
				 "removed locally (remote state unknown)" :
				 actionWord(mode,true) );
	} else {
		fprintf( stderr, "%s\n", errstack.getFullText(true) );
		fprintf( stderr, "Could not %s all jobs.\n",
				 actionWord(mode,false) );
	}
}


void
handleConstraints( void )
{
	if( ! has_constraint ) {
		return;
	}
	const char* tmp = global_constraint.Value();

	CondorError errstack;
	if( doWorkByConstraint(tmp, &errstack) ) {
		fprintf( stdout, "Jobs matching constraint %s %s\n", tmp,
				 (mode == JA_REMOVE_JOBS) ?
				 "have been marked for removal" :
				 (mode == JA_REMOVE_X_JOBS) ?
				 "have been removed locally (remote state unknown)" :
				 actionWord(mode,true) );

	} else {
		fprintf( stderr, "%s\n", errstack.getFullText(true) );
		fprintf( stderr, 
				 "Couldn't find/%s all jobs matching constraint %s\n",
				 actionWord(mode,false), tmp );
	}
}


void
printOldFailure( PROC_ID job_id )
{
	fprintf( stderr, "Couldn't find/%s job %d.%d.\n",
			 actionWord(mode,false), job_id.cluster, job_id.proc );
}


void
printOldMessage( PROC_ID job_id, action_result_t result )
{
	switch( result ) {
	case AR_SUCCESS:
		fprintf( stdout, "Job %d.%d %s.\n", 
				 job_id.cluster, job_id.proc, 
				 (mode == JA_REMOVE_JOBS) ?
				 "marked for removal" :
				 (mode == JA_REMOVE_X_JOBS) ?
				 "removed locally (remote state unknown)" :
				 actionWord(mode,true) );
		break;

	case AR_NOT_FOUND:
		if( mode==JA_RELEASE_JOBS ) {
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
		if( mode == JA_RELEASE_JOBS ) {
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
				 (mode == JA_REMOVE_JOBS) ?
				 "marked for removal" :
				 (mode == JA_REMOVE_X_JOBS) ?
				 "removed locally (remote state unknown)" :
				 actionWord(mode,true) );
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

/************************************
	The following are dummy stubs for the DaemonCore class to allow
	tools using DCSchedd to link.  DaemonCore is brought in
	because of the FileTransfer object.
	These stub functions will become obsoluete once we start linking
	in the real DaemonCore library with the tools, -or- once
	FileTransfer is broken down.
*************************************/
#include "../condor_daemon_core.V6/condor_daemon_core.h"
	DaemonCore* daemonCore = NULL;
	int DaemonCore::Kill_Thread(int) { return 0; }
//	char * DaemonCore::InfoCommandSinfulString(int) { return NULL; }
	int DaemonCore::Register_Command(int,char*,CommandHandler,char*,Service*,
		DCpermission,int) {return 0;}
	int DaemonCore::Register_Reaper(char*,ReaperHandler,
		char*,Service*) {return 0;}
	int DaemonCore::Create_Thread(ThreadStartFunc,void*,Stream*,
		int) {return 0;}
	int DaemonCore::Suspend_Thread(int) {return 0;}
	int DaemonCore::Continue_Thread(int) {return 0;}
//	int DaemonCore::Register_Reaper(int,char*,ReaperHandler,ReaperHandlercpp,
//		char*,Service*,int) {return 0;}
//	int DaemonCore::Register_Reaper(char*,ReaperHandlercpp,
//		char*,Service*) {return 0;}
//	int DaemonCore::Register_Command(int,char*,CommandHandlercpp,char*,Service*,
//		DCpermission,int) {return 0;}
/**************************************/
