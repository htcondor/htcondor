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


#include "condor_common.h"
#include "condor_classad.h"
#include "condor_version.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_distribution.h"

#include "read_user_log.h"
#include "file_modified_trigger.h"
#include "wait_for_user_log.h"
#include "match_prefix.h"

#include <set>
/*
XXX XXX XXX WARNING WARNING WARNING
The exit codes in this program are slightly different than
in other Condor tools.  This program should only exit success
if it is positively confirmed that the desired jobs have completed.
Any other exit should indicate EXIT_FAILURE.
*/

/* on Windows, and perhaps other OS's, EXIT_SUCCESS and EXIT_FAILURE are
   defined in stdlib.h as the numbers 0 and 1, respectively. Since they 
   are used differently in this file, we undefine them first. 
*/

#if defined(EXIT_SUCCESS)
	#undef EXIT_SUCCESS
#endif
#if defined(EXIT_FAILURE)
	#undef EXIT_FAILURE
#endif

#define EXIT_SUCCESS exit(0)
#define EXIT_FAILURE exit(1)

#define ANY_NUMBER -1

static void usage( char *cmd )
{
	fprintf(stderr,"\nUse: %s [options] <log-file> [job-number]\nWhere options are:",cmd);
	fprintf(stderr,R"opts(
    -help             Display options
    -version          Display Condor version
    -debug            Show extra debugging info
    -status           Show job start and terminate info
    -echo[:<fmt>]     Echo log events relevant to [job-number]
       optional <fmt> is one or more log format options:
         ISO_DATE     date in Year-Month-Day form
         UTC          echo time as UTC time
         XML          echo in XML log format
         JSON         echo in JSON log format
    -num <number>     Wait for this many jobs to end
                       (default is all jobs)
    -wait <seconds>   Wait no more than this time
                       (default is unlimited)
    -allevents        Continue on even if all jobs have ended.
                      use with -echo to transcribe the whole log
                      cannot be used with -num

This command watches a log file, and indicates when
a specific job (or all jobs mentioned in the log)
have completed or aborted. It returns success if
all such jobs have completed or aborted, and returns
failure otherwise. 

)opts");

	fprintf(stderr,"Examples:\n");
	fprintf(stderr,"    %s logfile\n",cmd);
	fprintf(stderr,"    %s logfile 35\n",cmd);
	fprintf(stderr,"    %s logfile 1406.35\n",cmd);
	fprintf(stderr,"    %s -wait 60 logfile 13.25.3\n",cmd);
	fprintf(stderr,"    %s -num 2 logfile\n",cmd);
	fprintf(stderr,"\nTranscribe an entire log to UTC timestamps:\n    %s -all -echo:UTC logfile\n", cmd);
}

static void version()
{
	printf( "%s\n%s\n", CondorVersion(), CondorPlatform() );
}

static int jobnum_matches( ULogEvent *event, int cluster, int process, int subproc )
{
	return
		( (event->cluster==cluster) || (cluster==ANY_NUMBER) ) &&
		( (event->proc==process)    || (process==ANY_NUMBER) ) &&
		( (event->subproc==subproc) || (subproc==ANY_NUMBER) );
}

int main( int argc, char *argv[] )
{
	int i;
	char *log_file_name = 0;
	char *job_name = 0;
	time_t waittime=0, stoptime=0;
	int minjobs = 0;
	int print_status = false;
	int echo_events = false;
	int dont_wait = false; // set to true when the wait is 0 - read all events then exit.
	int format_opts = 0;
	const char * pcolon;

	myDistro->Init( argc, argv );
	set_priv_initialize(); // allow uid switching if root
	config();

	format_opts = 0;
	auto_free_ptr fmt(param("DEFAULT_USERLOG_FORMAT_OPTIONS"));
	if (fmt) {
		format_opts = ULogEvent::parse_opts(fmt, format_opts);
	}

	for( i=1; i<argc; i++ ) {
		if(!strcmp(argv[i],"-help")) {
			usage(argv[0]);
			EXIT_SUCCESS;
		} else if(!strcmp(argv[i],"-version")) {
			version();
			EXIT_FAILURE;
		} else if(!strcmp(argv[i],"-debug")) {
			// dprintf to console
			dprintf_set_tool_debug("TOOL", 0);
			print_status = false;
		} else if(!strcmp(argv[i],"-status")) {
			if (dprintf_to_term_check()) {
				fprintf(stderr,"-status is implied by -debug\n");
			} else {
				print_status = true;
			}
		} else if(is_dash_arg_colon_prefix(argv[i],"echo", &pcolon, -1)) {
			echo_events = true;
			if (pcolon) {
				format_opts = ULogEvent::parse_opts(pcolon + 1, format_opts);
			}
		} else if (is_dash_arg_prefix(argv[i], "allevents", 3)) {
			if (minjobs) {
				fprintf(stderr, "-allevents cannot be used with -num\n");
				usage(argv[0]);
				EXIT_FAILURE;
			}
			minjobs = INT_MAX; // don't use job exits as a reason to stop
			if ( ! waittime) dont_wait = true;  // stop when we get to the end (unless -wait is also specfied)
		} else if(!strcmp(argv[i],"-wait")) {
			i++;
			if(i>=argc) {
				fprintf(stderr,"-wait requires an argument\n");
				usage(argv[0]);
				EXIT_FAILURE;
			}
			waittime = atoi(argv[i]);
			if (waittime) {
				dont_wait = false;
				stoptime = time(0) + waittime;
				dprintf(D_FULLDEBUG,"Will wait until %s\n",ctime(&stoptime));
			} else {
				dprintf(D_FULLDEBUG,"Will exit at EOF\n");
				dont_wait = true;
			}
		} else if( !strcmp( argv[i], "-num" ) ) {
			i++;
			if( i >= argc ) {
				fprintf( stderr, "-num requires an argument\n" );
				usage( argv[0] );
				EXIT_FAILURE;
			}
			if (minjobs == INT_MAX) {
				fprintf(stderr, "-num cannot be used with -allevents\n");
				usage(argv[0]);
				EXIT_FAILURE;
			}
			minjobs = atoi( argv[i] );
			if( minjobs < 1 ) {
				fprintf( stderr, "-num must be greater than zero\n" );
				usage( argv[0] );
				EXIT_FAILURE;
			}
			dprintf( D_FULLDEBUG, "Will wait until %d jobs end\n", minjobs );
		} else if (log_file_name != NULL && job_name == NULL && !strcmp(argv[i], "-1")) {
			job_name = argv[i]; // treat a bare -1 as 'all clusters'
		} else if(argv[i][0]!='-') {
			if(!log_file_name) {
				log_file_name = argv[i];
			} else if(!job_name) {
				job_name = argv[i];
			} else {
				fprintf(stderr,"Extra argument: %s\n\n",argv[i]);
				usage(argv[0]);
				EXIT_FAILURE;
			}
		} else {
			usage(argv[0]);
			EXIT_FAILURE;
		}
	}

	if( !log_file_name ) {
		usage(argv[0]);
		EXIT_FAILURE;
	}

	int cluster=ANY_NUMBER;
	int process=ANY_NUMBER;
	int subproc=ANY_NUMBER;

	if( job_name ) {
		int fields = sscanf(job_name,"%d.%d.%d",&cluster,&process,&subproc);
		if(fields>=1 && fields<=3) {
			/* number is fine */
		} else {
			fprintf(stderr,"Couldn't understand job number: %s\n",job_name);
			EXIT_FAILURE;
		}
	}

	dprintf(D_FULLDEBUG,"Reading log file %s\n",log_file_name);
	int submitted = 0, aborted = 0, completed = 0;
	std::set<std::string> table;

	// in case we want to echo in XML format
	classad::ClassAdXMLUnParser xmlunp;
	classad::ClassAdJsonUnParser jsonunp;
	if (format_opts & ULogEvent::formatOpt::XML) {
		xmlunp.SetCompactSpacing(false);
	}

	WaitForUserLog wful( log_file_name );
	if(! wful.isInitialized()) {
		fprintf( stderr, "Couldn't open %s: %s\n", log_file_name, strerror(errno) );
		EXIT_FAILURE;
	}

	bool initial_scan = true;
	while( 1 ) {
		int timeout_ms = -1;
		if(! dont_wait) {
			int now = time(NULL);
			if( stoptime && now > stoptime ) {
				printf( "Time expired.\n" );
				EXIT_FAILURE;
			}
			timeout_ms = (stoptime - now) * 1000;
		}

		ULogEventOutcome outcome;
		ULogEvent * event;
		outcome = wful.readEvent( event, initial_scan ? 0 : timeout_ms );
		if( outcome == ULOG_OK ) {
			char key[1024];
			sprintf(key,"%d.%d.%d",event->cluster,event->proc,event->subproc);
			if( jobnum_matches( event, cluster, process, subproc ) ) {
				if (echo_events) {
					std::string event_str;
					if (format_opts & ULogEvent::formatOpt::CLASSAD) {
						ClassAd* ad = event->toClassAd((format_opts & ULogEvent::formatOpt::UTC) != 0);
						if (ad) {
							if (format_opts & ULogEvent::formatOpt::JSON) {
								jsonunp.Unparse(event_str, ad);
								printf("%s\n", event_str.c_str());
							} else {
								xmlunp.Unparse(event_str, ad);
								printf("%s", event_str.c_str());
							}
							delete ad;
						}
					} else {
						event->formatEvent(event_str, format_opts);
						printf("%s...\n", event_str.c_str());
					}
				}

				if(event->eventNumber==ULOG_SUBMIT) {
					dprintf(D_FULLDEBUG,"%s submitted\n",key);
					if (print_status) printf("%s submitted\n", key);
					table.insert(key);
					submitted++;
				} else if(event->eventNumber==ULOG_JOB_TERMINATED) {
					dprintf(D_FULLDEBUG,"%s completed\n",key);
					if (print_status) printf("%s completed\n", key);
					table.erase(key);
					completed++;
				} else if(event->eventNumber==ULOG_JOB_ABORTED) {
					dprintf(D_FULLDEBUG,"%s aborted\n",key);
					if (print_status) printf("%s aborted\n", key);
					table.erase(key);
					aborted++;
				} else if (event->eventNumber==ULOG_EXECUTE) {
					if (print_status) {
						printf("%s executing on host %s\n", key, ((ExecuteEvent*)event)->getExecuteHost());
					}
				} else {
					/* nothing to do */
				}
			}
			delete event;

			if( minjobs && (completed + aborted >= minjobs ) ) {
				printf( "Specifed number of jobs (%d) done.\n", minjobs );
				EXIT_SUCCESS;
			}

			if( table.size() == 0 && submitted > 0 && (!minjobs) && !initial_scan) {
				printf( "All jobs done.\n" );
				EXIT_SUCCESS;
			}
		} else if( outcome == ULOG_NO_EVENT ) {
			if (initial_scan) {
				// we are done with the initial scan when we find no events without waiting.
				initial_scan = false;
				if (table.size() == 0 && !minjobs) {
					printf("All jobs done.\n");
					EXIT_SUCCESS;
				}
			} else if (table.size() == 0 && submitted == 0) {
				if( cluster == ANY_NUMBER ) {
					fprintf(stderr,"This log does not mention any jobs!\n");
				} else {
					fprintf(stderr,"This log does not mention that job!\n");
				}
				if ( ! dont_wait) {
					EXIT_FAILURE;
				}
			}
			if (dont_wait) {
				EXIT_SUCCESS;
			}
		} else if( outcome == ULOG_RD_ERROR ) {
			dprintf( D_FULLDEBUG, "Got ULOG_RD_ERROR, done.\n" );
			EXIT_FAILURE;
		} else if( outcome == ULOG_MISSED_EVENT ) {
			dprintf( D_FULLDEBUG, "Got ULOG_MISSED_EVENT, done.\n" );
			EXIT_FAILURE;
		} else if( outcome == ULOG_UNK_ERROR ) {
			dprintf( D_FULLDEBUG, "Got ULOG_UNK_ERROR, done.\n" );
			EXIT_FAILURE;
		} else {
			EXCEPT( "Unknown result reading user log." );
		}
	}

	EXIT_FAILURE;
	return 1; /* meaningless, but it makes Windows happy */
}
