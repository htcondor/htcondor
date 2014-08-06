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
#include "HashTable.h"
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
	fprintf(stderr,"\nUse: %s [options] <log-file> [job-number]\n",cmd);
	fprintf(stderr,"Where options are:\n");
	fprintf(stderr,"    -help             Display options\n");
	fprintf(stderr,"    -version          Display Condor version\n");
	fprintf(stderr,"    -debug            Show extra debugging info\n");
	fprintf(stderr,"    -status           Show job start and terminate info\n");
	fprintf(stderr,"    -echo             Echo log events relevant to [job-number]\n");
	fprintf(stderr,"    -num <number>     Wait for this many jobs to end\n");
	fprintf(stderr,"                       (default is all jobs)\n");
	fprintf(stderr,"    -wait <seconds>   Wait no more than this time\n");
	fprintf(stderr,"                       (default is unlimited)\n\n");

	fprintf(stderr,"This command watches a log file, and indicates when\n");
	fprintf(stderr,"a specific job (or all jobs mentioned in the log)\n");
	fprintf(stderr,"have completed or aborted.  It returns success if\n");
	fprintf(stderr,"all such jobs have completed or aborted, and returns\n");
	fprintf(stderr,"failure otherwise.\n\n");

	fprintf(stderr,"Examples:\n");
	fprintf(stderr,"    %s logfile\n",cmd);
	fprintf(stderr,"    %s logfile 35\n",cmd);
	fprintf(stderr,"    %s logfile 1406.35\n",cmd);
	fprintf(stderr,"    %s -wait 60 logfile 13.25.3\n",cmd);
	fprintf(stderr,"    %s -num 2 logfile\n",cmd);
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
	int debug_print_rescue = false;

	myDistro->Init( argc, argv );
	config();

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
		} else if(!strcmp(argv[i],"-echo")) {
			echo_events = true;
		} else if(!strcmp(argv[i],"-wait")) {
			i++;
			if(i>=argc) {
				fprintf(stderr,"-wait requires an argument\n");
				usage(argv[0]);
				EXIT_FAILURE;
			}
			waittime = atoi(argv[i]);
			stoptime = time(0) + waittime;
			dprintf(D_FULLDEBUG,"Will wait until %s\n",ctime(&stoptime));
		} else if( !strcmp( argv[i], "-num" ) ) {
			i++;
			if( i >= argc ) {
				fprintf( stderr, "-num requires an argument\n" );
				usage( argv[0] );
				EXIT_FAILURE;
			}
			minjobs = atoi( argv[i] );
			if( minjobs < 1 ) {
				fprintf( stderr, "-num must be greater than zero\n" );
				usage( argv[0] );
				EXIT_FAILURE;
			}
			dprintf( D_FULLDEBUG, "Will wait until %d jobs end\n", minjobs );
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
	int submitted, aborted, completed, flagged;
	FILE *sec_fp = NULL;
	int pos, nPos;
rescue :
	submitted=0;
	aborted=0;
	completed=0;
	flagged = 0;
	ReadUserLog log ;
	HashTable<MyString,MyString> table(127,MyStringHash);
	
	if(log.initialize(log_file_name,false,false,true)) {
		sec_fp = safe_fopen_wrapper_follow(log_file_name, "r", 0644);
		fseek (sec_fp, 0, SEEK_END);
		pos = ftell(sec_fp); 
		nPos = pos;	if (debug_print_rescue) printf("begin:%d ", nPos);
		while(1) {
			fseek(sec_fp, 0, SEEK_END);
			int tmp_pos = ftell(sec_fp);
			
			ULogEventOutcome outcome;
			ULogEvent *event;
			outcome = log.readEvent(event);
			
			if(outcome==ULOG_OK) {
				flagged = 0;
				pos = nPos = tmp_pos;
				if (debug_print_rescue) printf("top:%d ", nPos);

				char key[1024];
				sprintf(key,"%d.%d.%d",event->cluster,event->proc,event->subproc);
				MyString str(key);
				if( jobnum_matches( event, cluster, process, subproc ) ) {

					if (echo_events) {
						event->putEvent(stdout);
						printf("...\n");
					}

					if(event->eventNumber==ULOG_SUBMIT) {
						dprintf(D_FULLDEBUG,"%s submitted\n",key);
						if (print_status) printf("%s submitted\n", key);
						table.insert(str,str);
						submitted++;
					} else if(event->eventNumber==ULOG_JOB_TERMINATED) {
						dprintf(D_FULLDEBUG,"%s completed\n",key);
						if (print_status) printf("%s completed\n", key);
						table.remove(str);
						completed++;
					} else if(event->eventNumber==ULOG_JOB_ABORTED) {
						dprintf(D_FULLDEBUG,"%s aborted\n",key);
						if (print_status) printf("%s aborted\n", key);
						table.remove(str);
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
			} else {
				// did something change in the file since our last visit?
				fseek(sec_fp, 0, SEEK_END);
				nPos = ftell(sec_fp);
				if (flagged == 1) {
					fclose(sec_fp);
					dprintf(D_FULLDEBUG, "INFO: File %s changed but userLog reader could not read another event. We are reinitializing userLog reader. \n", log_file_name);
					if (debug_print_rescue) printf("rescue:%d ", nPos);
					if (print_status) printf("<reinitializing userLog reader>\n");
					// reinitialize the user log, we ended up here a second time 
					goto rescue;
				}
				if ( nPos != pos ){
					if (debug_print_rescue) printf("lagging:%d!=%d ", nPos, pos);
					pos = nPos;
					// we do not want to retry every time we are in a waiting sleep cycle, therefore flag a change
					flagged = 1;
				}
				
				dprintf(D_FULLDEBUG,"%d submitted %d completed %d aborted %d remaining\n",submitted,completed,aborted,submitted-completed-aborted);
				if(table.getNumElements()==0) {
					if(submitted>0) {
						if( !minjobs ) {
							printf("All jobs done.\n");
							EXIT_SUCCESS;
						}
					} else {
						if(cluster==ANY_NUMBER) {
							fprintf(stderr,"This log does not mention any jobs!\n");
						} else {
							fprintf(stderr,"This log does not mention that job!\n");
						}
						EXIT_FAILURE;
					}
				} else if(stoptime && time(0)>stoptime) {
					printf("Time expired.\n");
					EXIT_FAILURE;
				} else {
					time_t sleeptime;

					if(stoptime) {
						sleeptime = stoptime-time(0);
					} else {
						sleeptime = 5;
					}

					if(sleeptime>5) {
						sleeptime = 5;
					} else if(sleeptime<1) {
						sleeptime = 1;
					}

					log.synchronize();
					dprintf(D_FULLDEBUG,"No more events, sleeping for %ld seconds\n", (long)sleeptime);
					sleep(sleeptime);
					
				}
			}
		}
		fclose(sec_fp);
	} else {
		fprintf(stderr,"Couldn't open %s: %s\n",log_file_name,strerror(errno));
	}
	EXIT_FAILURE;
	
	return 1; /* meaningless, but it makes Windows happy */
}
