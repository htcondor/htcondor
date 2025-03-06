/***************************************************************
 *
 * Copyright (C) 1990-2010, Condor Team, Computer Sciences Department,
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


#include "condor_common.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "condor_io.h"
#include "get_daemon_name.h"
#include "internet.h"
#include "condor_attributes.h"
#include "match_prefix.h"
#include "sig_install.h"
#include "condor_version.h"
#include "condor_ver_info.h"
#include "daemon.h"
#include "dc_schedd.h"
#include "dc_collector.h"
#include "condor_distribution.h"
#include "CondorError.h"
#include "str_isxxx.h"
#include "enum_utils.h"


char	*MyName;
char 	*actionReason = NULL;
char    *holdReasonSubCode = NULL;
JobAction mode;
bool had_error = false;
bool dash_long = false;
bool dash_totals = false;

DCSchedd* schedd = NULL;

std::vector<std::string> job_ids;

	// Prototypes of local interest
typedef enum {
	CT_NONE = FALSE,
	CT_ALL,
	CT_CLUSTER,
	CT_USER,
	CT_SIMPLE,
	CT_COMPLEX,
} CONSTRAINT_TYPE;
void addConstraint(const char * constraint, CONSTRAINT_TYPE type=CT_COMPLEX);
void addUserConstraint(const char * user);
void addClusterConstraint(int clust);
void procArg(const char*);
void usage(int iExitCode=1);
void handleConstraints( void );
ClassAd* doWorkByList(const std::vector<std::string>& ids, CondorError * errstack);
void printNewMessages(ClassAd* result_ad, const std::vector<std::string>& ids);
bool mayUserForceRm( );

CONSTRAINT_TYPE has_constraint = CT_NONE;
std::string global_constraint;
CONSTRAINT_TYPE has_usercluster = CT_NONE;
std::string usercluster_constraint;

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
	
	case JA_SUSPEND_JOBS:
		return past ? "suspended" : "suspend";
		break;
		
	case JA_CONTINUE_JOBS:
		return past ? "continued" : "continue";
		break;

	case JA_REMOVE_JOBS:
		return past ? "marked for removal" : "remove";
		break;

	case JA_REMOVE_X_JOBS:
		return past ? "removed locally (remote state unknown)" : "remove";
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
usage(int iExitCode)
{
	char word[32];
	snprintf( word, sizeof(word), "%s", getJobActionString(mode) );
	fprintf( stderr, "Usage: %s [options] [constraints]\n", MyName );
	fprintf( stderr, " where [options] is zero or more of:\n" );
	fprintf( stderr, "  -help               Display this message and exit\n" );
	fprintf( stderr, "  -version            Display version information and exit\n" );
	fprintf( stderr, "  -long               Display full result classad\n" );
	fprintf( stderr, "  -totals             Display success/failure totals\n" );
	fprintf( stderr, "  -debug              Display debugging information while running\n" );

	fprintf( stderr, "  -name schedd_name   Connect to the given schedd\n" );
	fprintf( stderr, "  -pool hostname      Use the given central manager to find daemons\n" );
	fprintf( stderr, "  -addr <ip:port>     Connect directly to the given \"sinful string\"\n" );
	if( mode == JA_REMOVE_JOBS || mode == JA_REMOVE_X_JOBS ) {
		fprintf( stderr, "  -reason reason      Use the given RemoveReason\n");
	} else if( mode == JA_RELEASE_JOBS ) {
		fprintf( stderr, "  -reason reason      Use the given ReleaseReason\n");
	} else if( mode == JA_HOLD_JOBS ) {
		fprintf( stderr, "  -reason reason      Use the given HoldReason\n");
		fprintf( stderr, "  -subcode number     Set HoldReasonSubCode\n");
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
	fprintf( stderr, " and where [constraints] is one of:\n" );
	fprintf( stderr, "  cluster.proc        %s the given job\n", word );
	fprintf( stderr, "  cluster             %s the given cluster of jobs\n", word );
	fprintf( stderr, "  user                %s all jobs owned by user\n", word );
	fprintf( stderr, "  -constraint expr    %s all jobs matching the boolean expression\n", word );
	fprintf( stderr, "  -all                %s all jobs "
			 "(cannot be used with other constraints)\n", word );
	exit( iExitCode );
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
	char*	cmd_str;
	DCCollector* pool = NULL;
	char* scheddName = NULL;
	char* scheddAddr = NULL;
	const char * pcolon;

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

	if (cmd_str && strncasecmp( cmd_str, "_hold", strlen("_hold") ) == MATCH) { 
		mode = JA_HOLD_JOBS;
	} else if ( cmd_str && 
			strncasecmp( cmd_str, "_release", strlen("_release") ) == MATCH ) {
		mode = JA_RELEASE_JOBS;
	} else if ( cmd_str && 
			strncasecmp( cmd_str, "_suspend", strlen("_suspend") ) == MATCH ) {
		mode = JA_SUSPEND_JOBS;
	} else if ( cmd_str && 
			strncasecmp( cmd_str, "_continue", strlen("_continue") ) == MATCH ) {
		mode = JA_CONTINUE_JOBS;
	}else if ( cmd_str && 
			strncasecmp( cmd_str, "_rm", strlen("_rm") ) == MATCH ) {
		mode = JA_REMOVE_JOBS;
	} else if( cmd_str && ! strncasecmp(cmd_str, "_vacate_job",
									strlen("_vacate_job")) ) {  
		mode = JA_VACATE_JOBS;
	} else {
		// don't know what mode we're using, so bail.
		fprintf( stderr, "Unrecognized command name, \"%s\"\n", MyName ); 
		usage();
	}

	set_priv_initialize(); // allow uid switching if root
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
			if (is_dash_arg_colon_prefix(arg, "debug", &pcolon, 1)) {
				// dprintf to console
				dprintf_set_tool_debug("TOOL", (pcolon && pcolon[1]) ? pcolon+1 : nullptr);
			} else if (is_dash_arg_prefix(arg, "long", 1)) {
				dash_long = true;
			} else if (is_dash_arg_prefix(arg, "totals", 3)) {
				dash_totals = true;
			} else if (is_dash_arg_prefix(arg, "constraint", 1)) {
				++argv; arg = *argv;
				if( ! arg || *arg == '-') {
					fprintf( stderr, 
							 "%s: -constraint requires another argument\n", 
							 MyName);
					exit(1);
				}
				addConstraint(*argv, CT_SIMPLE);
			} else if (is_dash_arg_prefix(arg, "all", 1)) {
				if (has_constraint || has_usercluster) {
					fprintf( stderr, "%s: can't mix -all with other constraints\n", MyName);
					exit(1);
				}
				addConstraint(ATTR_CLUSTER_ID " >= 0", CT_ALL);
			} else if (is_dash_arg_prefix(arg, "addr", 2)) {
				++argv; arg = *argv;
				if( ! arg || *arg == '-') {
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
			} else if (is_dash_arg_prefix(arg, "reason", 1)) {
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
			} else if (is_dash_arg_prefix(arg, "subcode", 1)) {
				argv++;
				if( ! *argv ) {
					fprintf( stderr, 
							 "%s: -subcode requires another argument\n", 
							 MyName);
					exit(1);
				}		
				char *end = NULL;
				long code = strtol(*argv,&end,10);
				if( code == LONG_MIN || !end || *end || end==*argv ) {
					fprintf( stderr, "Invalid -subcode %s!\n", *argv );
					exit(1);
				}
				holdReasonSubCode = strdup(*argv);
				ASSERT( holdReasonSubCode );
			} else if (is_dash_arg_prefix(arg, "forcex", (mode == JA_REMOVE_JOBS) ? 1 : 2)) {
				if( mode == JA_REMOVE_JOBS ) {
					mode = JA_REMOVE_X_JOBS;
				} else {
                    fprintf( stderr, 
                             "-forcex is only valid with condor_rm\n" );
					usage();
				}
			} else if (is_dash_arg_prefix(arg, "fast", (mode == JA_VACATE_JOBS) ? 1 : 2)) {
				if( mode == JA_VACATE_JOBS ) {
					mode = JA_VACATE_FAST_JOBS;
				} else {
                    fprintf( stderr, 
                             "-fast is only valid with condor_vacate_job\n" );
					usage();
				}
			} else if (is_dash_arg_prefix(arg, "name", 1)) {
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
			} else if (is_dash_arg_prefix(arg, "pool", 1)) {
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
			} else if (is_dash_arg_prefix(arg, "version", 1)) {
				version();
			} else if (is_dash_arg_prefix(arg, "help", 1)) {
				usage(0);
			} else {
				fprintf( stderr, "Unrecognized option: %s\n", arg ); 
				usage();
			}
		} else {
			if (has_constraint == CT_ALL) {
					// If -all is set, there should be no other
					// constraint arguments.
				usage();
			}
			procArg(arg);
		}
	}

	if( ! has_constraint && ! has_usercluster && job_ids.empty() ) {
			// We got no indication of what to act on
		fprintf( stderr, "You did not specify any jobs\n" ); 
		usage();
	}

		// Pick the default reason if the user didn't specify one
	if( actionReason == NULL ) {
		switch( mode ) {
		case JA_RELEASE_JOBS:
			actionReason = strdup("via condor_release");
			break;
		case JA_REMOVE_X_JOBS:
			actionReason = strdup("via condor_rm -forcex");
			break;
		case JA_REMOVE_JOBS:
			actionReason = strdup("via condor_rm");
			break;
		case JA_HOLD_JOBS:
			actionReason = strdup("via condor_hold");
			break;
		case JA_SUSPEND_JOBS:
			actionReason = strdup("via condor_suspend");
			break;
		case JA_CONTINUE_JOBS:
			actionReason = strdup("via condor_continue");
			break;
		default:
			actionReason = NULL;
		}
	}

		// We're done parsing args, now make sure we know how to
		// contact the schedd. 
	if (scheddAddr) {
		schedd = new DCSchedd( scheddAddr );
	} else if (scheddName && ! pool) {
		// if there was a name specified, we always want to look that up in the collector
		// because SCHEDD_NAME and SCHEDD_ADDRESS_FILE don't necessary refer to the same schedd
		// which the DCSchedd constructor doesn't handle correctly if there is no pool
		CollectorList * colist = CollectorList::create();

		// construct a query for the given schedd name
		char *daemonname = get_daemon_name(scheddName);
		if ( ! daemonname) {
			fprintf( stderr, "Error: unknown schedd %s\n", get_host_part(scheddName));
			exit(1);
		}
		std::string constr(daemonname); constr.insert(0, ATTR_NAME " == \""); constr.append("\"");
		free(daemonname);
		CondorQuery query(SCHEDD_AD);
		query.addORConstraint (constr.c_str());

		ClassAdList schedList;
		QueryResult qres = colist->query (query, schedList);
		delete colist;
		schedList.Rewind();
		ClassAd * schedAd = schedList.Next();
		if (qres != Q_OK || ! schedAd) {
			fprintf( stderr, "Error: cannot get address of schedd %s\n", get_host_part(scheddName));
			exit(1);
		}

		schedd = new DCSchedd(*schedAd, pool ? pool->addr() : NULL );
	} else {
			// This will always do the right thing, even if either or
			// both of scheddName or pool are NULL.
		schedd = new DCSchedd( scheddName, pool ? pool->addr() : NULL );
	}
	if( ! schedd->locate() ) {
		fprintf( stderr, "%s: %s\n", MyName, schedd->error() ); 
		exit( 1 );
	}

		// Special case for condor_rm -forcex: a configuration
		// setting can disable this functionality.  The real
		// validation is done in the schedd, but we can catch
		// the most common cases here and give a useful error
		// message.
	if(mode == JA_REMOVE_X_JOBS) {
		if( mayUserForceRm() == false) {
			fprintf( stderr, "Remove aborted. condor_rm -forcex has been disabled by the administrator.\n" );
			exit( 1 );
		}
	}

		// do the actual work for fully qualified job ids
	if (!job_ids.empty()) {
		if (has_constraint) {
			fprintf(stderr, "Warning, -constraint is ignored for fully qualified job ids\n");
		}
		CondorError errstack;
		ClassAd* result_ad = doWorkByList( job_ids, &errstack );
		if (had_error) {
			fprintf( stderr, "%s\n", errstack.getFullText(true).c_str() );
		}
		printNewMessages( result_ad, job_ids );
		delete( result_ad );
	}

		// do the work for all constraint based commands, including usernames and clusters
	handleConstraints();

		// If releasing jobs, and no errors happened, do a 
		// reschedule command now.
	if ( mode == JA_RELEASE_JOBS && had_error == false ) {
		Daemon  my_schedd(DT_SCHEDD, NULL, NULL);
		CondorError errstack;
		if (!my_schedd.sendCommand(RESCHEDULE, Stream::safe_sock, 0, &errstack)) {
			fprintf( stderr, "%s\n", errstack.getFullText(true).c_str() );
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
	ClassAd* ad = 0;
	int total_jobs = -1;
	bool rval = true;
	action_result_type_t result_type = (dash_long && ! dash_totals) ? AR_LONG : AR_TOTALS;
	switch( mode ) {
	case JA_RELEASE_JOBS:
		ad = schedd->releaseJobs( constraint, actionReason, errstack, result_type );
		break;
	case JA_REMOVE_X_JOBS:
		ad = schedd->removeXJobs( constraint, actionReason, errstack, result_type );
		break;
	case JA_VACATE_JOBS:
		ad = schedd->vacateJobs( constraint, VACATE_GRACEFUL, errstack, result_type );
		break;
	case JA_VACATE_FAST_JOBS:
		ad = schedd->vacateJobs( constraint, VACATE_FAST, errstack, result_type );
		break;
	case JA_REMOVE_JOBS:
		ad = schedd->removeJobs( constraint, actionReason, errstack, result_type );
		break;
	case JA_HOLD_JOBS:
		ad = schedd->holdJobs( constraint, actionReason, holdReasonSubCode, errstack, result_type );
		break;
	case JA_SUSPEND_JOBS:
		ad = schedd->suspendJobs( constraint, actionReason, errstack, result_type );
		break;
	case JA_CONTINUE_JOBS:
		ad = schedd->continueJobs( constraint, actionReason, errstack, result_type );
		break;
	default:
		EXCEPT( "impossible: unknown mode in doWorkByConstraint" );
	}
	if( ! ad ) {
		had_error = true;
		rval = false;
	} else {
		if (dash_long) {
			fPrintAd (stdout, *ad);
			fputc ('\n', stdout);
		} else if (dash_totals) {
			JobActionResults jar;
			jar.readResults(ad);
			fprintf(stdout, "%d Succeeded, %d Failed, %d Not Found, %d Bad Status, %d Already Done, %d Permission Denied\n"
				, jar.numSuccess()
				, jar.numError()
				, jar.numNotFound()
				, jar.numBadStatus()
				, jar.numAlreadyDone()
				, jar.numPermissionDenied());
		}
		int result = FALSE;
		if( !ad->LookupInteger(ATTR_ACTION_RESULT, result) ) {
			had_error = true;
			rval = false;
		}
		else if( !result ) {
			// There were no ads acted upon, but that doesn't
			// mean there was an error.  It's possible the schedd
			// had no jobs
			if( !ad->LookupInteger(ATTR_TOTAL_JOB_ADS, total_jobs) || total_jobs > 0 ) {
				had_error = true;
			} else {
				// There were no jobs in the queue, so add a
				// more meaningful error message
				errstack->push(MyName, 0, "There are no jobs in the queue");
			}
			rval = false;
		}
		delete ad;
	}
	return rval;
}


ClassAd*
doWorkByList( const std::vector<std::string>& ids, CondorError *errstack )
{
	ClassAd* rval = 0;
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
		rval = schedd->holdJobs( ids, actionReason, holdReasonSubCode, errstack );
		break;
	case JA_SUSPEND_JOBS:
		rval = schedd->suspendJobs( ids, actionReason, errstack );
		break;
	case JA_CONTINUE_JOBS:
		rval = schedd->continueJobs( ids, actionReason, errstack );
		break;
	default:
		EXCEPT( "impossible: unknown mode in doWorkByList" );
	}
	if( ! rval ) {
		had_error = true;
	} else {
		if (dash_long) {
			fPrintAd (stdout, *rval);
			fputc ('\n', stdout);
		} else if (dash_totals) {
			JobActionResults jar;
			jar.readResults(rval);
			fprintf(stdout, "%d Succeeded, %d Failed, %d Not Found, %d Bad Status, %d Already Done, %d Permission Denied\n"
				, jar.numSuccess()
				, jar.numError()
				, jar.numNotFound()
				, jar.numBadStatus()
				, jar.numAlreadyDone()
				, jar.numPermissionDenied());
		}
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

	if( str_isint(arg) || str_isreal(arg,true) )
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
			addClusterConstraint(c);
			return;
		}
		if(*tmp == '.')
		{
			p = strtol(tmp + 1, &tmp, 10);
			if(p < 0)
			{
				fprintf( stderr, "Invalid proc # from %s.\n", arg);
				had_error = true;
				return;
			}
			if(*tmp == '\0')
			// process a proc
			{
				job_ids.emplace_back( arg );
				return;
			}
		}
		fprintf( stderr, "Warning: unrecognized \"%s\" skipped.\n", arg );
		return;
	}
	// process by user name
	else {
		addUserConstraint(arg);
	}
}

void
addClusterConstraint(int clust)
{
	if ( ! has_usercluster) {
		formatstr(usercluster_constraint,ATTR_CLUSTER_ID " == %d", clust);
		has_usercluster = CT_CLUSTER;
	} else {
		formatstr_cat(usercluster_constraint," || " ATTR_CLUSTER_ID " == %d", clust);
		has_usercluster = CT_COMPLEX;
	}
}

void
addUserConstraint(const char * user)
{
	if ( ! has_usercluster) {
		formatstr(usercluster_constraint,ATTR_OWNER " == \"%s\"", user);
		has_usercluster = CT_USER;
	} else {
		formatstr_cat(usercluster_constraint," || " ATTR_OWNER " == \"%s\"", user);
		has_usercluster = CT_COMPLEX;
	}
}

void
addConstraint(const char *constraint, CONSTRAINT_TYPE ct_type)
{
	if (CT_NONE == has_constraint || CT_ALL == has_constraint) {
		global_constraint = constraint;
		has_constraint = ct_type;
	} else if (CT_COMPLEX == has_constraint) {
		formatstr_cat(global_constraint," && (%s)", constraint);
	} else {
		// if we get here, has_constraint is CT_CLUSTER, CT_USER or CT_SIMPLE,
		// wrap current (single) constraint with (), and && with new constraint.
		// and promote constraint type to complex.
		std::string one = global_constraint;
		formatstr(global_constraint,"(%s) && (%s)", one.c_str(), constraint);
		has_constraint = CT_COMPLEX;
	}
}

bool
mayUserForceRm( )
{
	const char * PARAM_ALLOW_FORCE_RM = "ALLOW_FORCE_RM";
	char* tmp = param( PARAM_ALLOW_FORCE_RM );
	if( tmp == NULL) {
		// Not present.  Assume TRUE (old behavior).
		return true;
	}

	ClassAd tmpAd;
	const char * TESTNAME = "test";
	if(tmpAd.AssignExpr(TESTNAME, tmp) == FALSE) {
		// Error parsing, most likely.  Warn and assume TRUE.
		fprintf(stderr, "The configuration setting %s may be invalid.  Treating as TRUE.\n", PARAM_ALLOW_FORCE_RM);
		free(tmp);
		return true;
	}

	free(tmp);

	bool is_okay = false;
	if(tmpAd.LookupBool(TESTNAME, is_okay)) {
		return is_okay;
	} else {
		// Something went wrong.  May be undefined because
		// we need a job classad.  Assume TRUE.
		return true;
	}
}


void
handleConstraints( void )
{
	if (has_usercluster) {
		addConstraint(usercluster_constraint.c_str(), has_usercluster);
	}
	if ( ! has_constraint) {
		return;
	}

	const char* tmp = global_constraint.c_str();
	std::string cmsg;
	switch(has_constraint) {
		case CT_USER:
			formatstr(cmsg," of user %s", strstr(tmp, " == ")+4);
			break;
		case CT_CLUSTER:
			formatstr(cmsg," in cluster %s", strstr(tmp, " == ")+4);
			break;
		case CT_ALL:
			cmsg = "";
			break;
		default:
			formatstr(cmsg," matching constraint (%s)", tmp);
			break;
	}

	CondorError errstack;
	if( doWorkByConstraint(tmp, &errstack) ) {
		fprintf(stdout, "All jobs%s have been %s\n", cmsg.c_str(), actionWord(mode,true));
	} else {
		fprintf( stderr, "%s\n", errstack.getFullText(true).c_str() );
		if (had_error) {
			fprintf(stderr, "Couldn't find/%s all jobs%s\n", actionWord(mode,false), cmsg.c_str());
		}
	}
}


void
printNewMessages(ClassAd* result_ad, const std::vector<std::string>& ids)
{
	char* msg;
	PROC_ID job_id;
	bool rval;

	JobActionResults results;
	results.readResults( result_ad );

	for (auto& tmp: ids) {
		job_id = getProcByString( tmp.c_str() );
		rval = results.getResultString( job_id, &msg );
		if( rval ) {
			fprintf( stdout, "%s\n", msg );
		} else {
			fprintf( stderr, "%s\n", msg );
		}
		free( msg );
	}
}
