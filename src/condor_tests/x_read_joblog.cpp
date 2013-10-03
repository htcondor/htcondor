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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#ifdef WIN32
#include "condor_header_features.h"
#include "condor_sys_nt.h"
#else
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
#include "read_user_log.h"

struct hostent *NameEnt;
/*
**#define NUL		"\0";
 */

void usage(char* myname, FILE* puthere)
{
	fprintf(puthere,
"Usage: %s LOGFILE ACTION [number]\n"
"  LOGFILE    The logfile that has its exit status\n"
"  ACTION     Send MACHINE or RETURNVALUE or FULLREMOVE\n"
"				TRACE(read out all events) or\n"
"  				MAXJOBS(read for submitted/executing limits honored)\n"
"  				MAXPOST(read for submitted/executing limits honored)\n"
"  				MAXPRE(read for submitted/executing limits honored)\n"
"  number      (optional) restrict based on the cluster of the job, or cluster.proc.subproc\n"
"  number      (mandatory) MAXJOBS must have a value passed in \n"
"  number      (mandatory) MAXPOST must have a value passed in \n"
"  number      (mandatory) MAXPRE must have a value passed in \n", myname);
}

int main(int argc, char* argv[])
{
    char* logfile = argv[1];
    char* action  = argv[2];
	int jobid = 0;
	int maxjobs = 0;

	char* jobidstr  = 0;

	if( (argc == 3) && (strcmp(action, "MAXJOBS") == 0))
	{
		usage(argv[0], stderr);
		return -1;
	}

	if( argc == 4 )
	{
		jobidstr = argv[3];
		jobid   = atoi(argv[3]); // optional
		maxjobs = jobid;
		// Presumably one of these MAXPOSTs should be something else...
		if( (strcmp(action,"MAXJOBS") == 0) || (strcmp(action,"MAXPOST") == 0) || (strcmp(action,"MAXPOST") == 0) )
		{
			jobid = 0;
		}
		//printf("JOBID = %d\n",maxjobs);
	}

	if( !logfile ) {
		usage(argv[0], stderr);
		return -1;
	}

	if( !action ) {
		usage(argv[0], stderr);
		return -1;
	}

	if( !strncmp(logfile, "-h", 2) || !strcmp(logfile, "--help") ) {
		usage(argv[0], stdout);
		return 0;
	}
	if( !strcmp(action, "RETURNVALUE") || !strcmp(action, "MACHINE") || !strcmp(action, "TRACE") || !strcmp(action, "MAXJOBS") || !strcmp(action, "FULLREMOVE")) 
	{
				;
	}
	else
	{
		printf("Illegal action %s\n", action);
		usage(argv[0], stdout);
		return 1;
	}

	int clust, proc, subproc;
	int current_cluster = 0;
	int running_jobs = 0;
	int running_jobs_hitlimit = 0;
	int returnval = 0;

	if( (!strcmp(action, "TRACE") || !strcmp(action, "RETURNVALUE") ) && jobid)
	{
		//printf("process jobid....\n");
	//if( jobid && strcmp(action, "MAXJOBS"))
		int numscanned = sscanf(jobidstr, "%d.%d.%d", &clust, &proc, &subproc);
		if( numscanned <= 0 ) {
			fprintf(stderr, "Invalid job id: %s\n", jobidstr);
			usage(argv[0], stderr);
			return -1;
		} else if ( numscanned == 1 ) {
			proc = -1;
			subproc = -1;
		} else if ( numscanned == 2 ) {
			subproc = -1;
		}

		//printf("clust %d, proc %d, subproc %d\n", clust, proc, subproc);
	}

	char done = 0;
	char found = 0;
	char evicted = 0;
	char aborted = 0;
    char* hostmachine = NULL;
	char* extractptr = NULL;
	char* placeptr = NULL;
	char machinedotaddr[256];

	if ( access( logfile, F_OK ) != 0 ) {
		fprintf(stderr, "Logfile %s does not exist!\n", logfile);
		return -1;
	}
	ReadUserLog* rul = new ReadUserLog(logfile);
	if( !rul ) {
		fprintf(stderr, "Invalid logfile: %s\n", logfile);
		usage(argv[0], stderr);
		return -1;
	}

    ULogEvent* e = NULL;
	int myjobstatus = 0;
	char *eventtestval;

	while( !done ) {
        ULogEventOutcome outcome = rul->readEvent(e);
		//cout << ULogEventOutcomeNames[outcome] << endl;
		//printf("Outcome from rul->readEvent(e) is %d\n",outcome);
        switch( outcome ) {
		  case ULOG_NO_EVENT:
		  case ULOG_RD_ERROR:
		  case ULOG_UNK_ERROR:
		  case ULOG_MISSED_EVENT:
			done++;
			break;
		  case ULOG_OK:
			//printf("ULOG OK cluster %d\n",e->cluster);
			if( current_cluster != e->cluster && !strcmp(action, "TRACE"))
			{
				current_cluster = e->cluster;
				if(jobid)
				{
					if(e->cluster == clust)
					{
						printf("Examine cluster %d\n",e->cluster);
					}
				}
				else
				{
					printf("Examine cluster %d\n",e->cluster);
				}
			}

			if(jobid) 
			{
				if( !(e->cluster == clust &&
				  	(proc < 0 || e->proc == proc) &&
				  	(subproc < 0 || e->subproc == subproc)) ) {
						printf("skipping cluster %d\n",e->cluster);
					break;
				}
				//cout << "+-" << ULogEventNumberNames[e->eventNumber] << endl;
			}
			if( strcmp(action, "MACHINE") == 0 ) 
			{
				printf("MACHINE\n");
				switch( e->eventNumber ) 
				{
					case ULOG_EXECUTE:
						//printf("Job executed\n");
						//returnval = ((ExecuteEvent*)e)->return_value;
						extractptr = hostmachine = const_cast<char *> ( ((ExecuteEvent*)e)->getExecuteHost());
						placeptr =  machinedotaddr;
						while(*extractptr != ':')
						{
								if(*extractptr == '<')
								{
										extractptr++; // skip
										continue;
								}
								*placeptr++ = *extractptr++;
						}
						*placeptr++ = '\0';
						//printf("Job executed on machine %s %s\n",hostmachine,machinedotaddr);
						printf("%s\n",machinedotaddr);
						return(0);
						break;
					default:
							//printf("Not a EXECUTE event\n");
						break;

				}
			}
			else if( strcmp(action, "TRACE") == 0 )
			{
				//printf("TRACE Looking at event # %d \n",e->eventNumber);
				switch( e->eventNumber ) 
				{
				  case ULOG_SUBMIT:
					printf("Job Submited.........\n");
					break;
				  case ULOG_EXECUTE:
					printf("Job Executed.........\n");
					break;
				  case ULOG_EXECUTABLE_ERROR:
					printf("Job Execution Error.........\n");
					break;
				  case ULOG_CHECKPOINTED:
					printf("Job Checkpointed.........\n");
					break;
				  case ULOG_JOB_EVICTED:
					printf("Job Evicted.........\n");
					break;
				  case ULOG_JOB_TERMINATED:
					printf("Job Terminated.........\n");
					break;
				  case ULOG_IMAGE_SIZE:
					printf("Job Image Size Changed.........\n");
					break;
				  case ULOG_SHADOW_EXCEPTION:
					printf("Job Shadow Exception.........\n");
					break;
				  case ULOG_GENERIC:
					printf("Job Generic.........\n");
					break;
				  case ULOG_JOB_AD_INFORMATION:
				  {
					printf("Job Ad Information.........\n");

					JobAdInformationEvent	*info =
						dynamic_cast<JobAdInformationEvent*>( e );
					if ( !info ) {
						printf( "Event isn't a JobAdInformationEvent!!!\n" );
						break;
					}

					// print a attribute which is always there
					const char *attr;
					attr = "JobStatus";
					if ( !info->LookupInteger(attr, myjobstatus) ) {
						printf( "%s not found!\n", attr );
					}
					else {
						printf("%s is %d\n", attr, myjobstatus);
					}

					attr = "BillString";
					if ( !info->LookupString(attr, &eventtestval) ) {
						printf( "%s not found!\n", attr );
					}
					else {
						printf("%s is '%s'\n", attr, eventtestval );
						free( eventtestval );
					}

					attr = "BillInt";
					if ( !info->LookupInteger(attr, myjobstatus) ) {
						printf( "%s not found!\n", attr );
					}
					else {
						printf("%s is %d\n", attr, myjobstatus);
					}

					attr = "BillBool";
					if ( !info->LookupInteger(attr, myjobstatus) ) {
						printf( "%s not found!\n", attr );
					}
					else {
						printf("%s is %d\n", attr, myjobstatus);
					}
					break;
				  }

				  case ULOG_JOB_ABORTED:
					printf("Job ABORTED by user.........\n");
					break;
				  case ULOG_JOB_SUSPENDED:
					printf("Job Suspended.........\n");
					break;
				  case ULOG_JOB_UNSUSPENDED:
					printf("Job Unsuspended.........\n");
					break;
				  case ULOG_JOB_HELD:
					printf("Job Held.........\n");
					break;
				  case ULOG_JOB_RELEASED:
					printf("Job Released.........\n");
					break;
				  case ULOG_NODE_EXECUTE:
					printf("Job Node Execute.........\n");
					break;
				  case ULOG_NODE_TERMINATED:
					printf("Node Terminated .........\n");
					break;
				  case ULOG_POST_SCRIPT_TERMINATED:
					printf("Post Script Terminated.........\n");
					running_jobs--;
					break;
				  case ULOG_GLOBUS_SUBMIT:
					printf("Job Globus Submit.........\n");
					break;
				  case ULOG_GLOBUS_SUBMIT_FAILED:
					printf("Job Globus Submit Failed.........\n");
					break;
				  case ULOG_GLOBUS_RESOURCE_UP:
					printf("Globus Resource Up.........\n");
					break;
				  case ULOG_GLOBUS_RESOURCE_DOWN:
					printf("Globus Resource Down.........\n");
					break;
				  case ULOG_REMOTE_ERROR:
					printf("Job Remote Error.........\n");
					break;
				  //case ULOG_JOB_DISCONNECTED:
					//printf("Job Disconnected.........\n");
					//break;
				  //case ULOG_JOB_RECONNECTED:
					//printf("Job Reconnected.........\n");
					//break;
				  //case ULOG_JOB_RECONNECT_FAILED:
					//printf("Job Reconnect Failed.........\n");
					//break;
				  default:
					printf("UNKNOWN USER LOG EVENT.........\n");
					break;
				}
			}
			else if( strcmp(action, "MAXJOBS") == 0 )
			{
				//printf("MAXJOBS\n");
				switch( e->eventNumber ) 
				{
				  case ULOG_SUBMIT:
					//printf("Job Submited.........\n");
					running_jobs++;
					if(running_jobs > maxjobs)
					{
						running_jobs_hitlimit++;
					}
					break;
				  case ULOG_POST_SCRIPT_TERMINATED:
					//printf("Post Script Terminated.........\n");
					running_jobs--;
					break;
				  default:
					break;
				}
			}
			else if( strcmp(action, "FULLREMOVE") == 0 )
			{
				//printf("FULLREMOVE\n");
				switch( e->eventNumber ) 
				{
				  case ULOG_SUBMIT:
					//printf("Job Submited.........\n");
					running_jobs++;
					if(running_jobs > maxjobs)
					{
						running_jobs_hitlimit++;
					}
					break;
				  case ULOG_JOB_TERMINATED:
					//printf("Job Terminated.........\n");
					running_jobs--;
					break;
				  case ULOG_JOB_ABORTED:
					//printf("Job ABORTED by user.........\n");
					running_jobs--;
					break;
				  case ULOG_POST_SCRIPT_TERMINATED:
					//printf("Post Script Terminated.........\n");
					running_jobs--;
					break;
				  default:
					break;
				}
			}
			else if( strcmp(action, "RETURNVALUE") == 0 )
			{
				//printf("RETURNVALUE\n");
				switch( e->eventNumber ) 
				{
				  case ULOG_JOB_EVICTED:
					returnval = ((JobEvictedEvent*)e)->return_value;
					//printf("Job Evicted.........\n");
					// allow a later run to return a good value bt 12/13/03
					evicted++;
					found++;
					break;
				  case ULOG_JOB_ABORTED:
					//returnval = ((JobEvictedEvent*)e)->return_value;
					returnval = -99;
					//printf("Job ABORTED by user.........\n");
					// allow a later run to return a good value bt 12/13/03
					aborted++;
					found++;
					done++;
					break;
				  case ULOG_JOB_TERMINATED:
					returnval = ((JobTerminatedEvent*)e)->returnValue;
					done++;
					found++;
					break;
				  case ULOG_NODE_TERMINATED:
					returnval = ((NodeTerminatedEvent*)e)->returnValue;
					done++;
					found++;
					break;
				  case ULOG_POST_SCRIPT_TERMINATED:
					returnval = ((PostScriptTerminatedEvent*)e)->returnValue;
					done++;
					found++;
					break;
				  default:
					break;
				}
			}
			break;
        }
		if( e) delete e;
		e = NULL;
	}

	if( rul ) delete rul;

	if( strcmp(action, "MACHINE") == 0 )
	{
			printf("NONE\n");
			return(-1);
	}

	if( strcmp(action, "TRACE") == 0 )
	{
			printf("DONE\n");
			return(0);
	}

	if( strcmp(action, "MAXJOBS") == 0 )
	{
		if(running_jobs_hitlimit)
		{
			printf("FAILED\n");
			return(1);
		}
		else
		{
			printf("DONE\n");
			return(0);
		}
	}

	if( strcmp(action, "FULLREMOVE") == 0 )
	{
		if(running_jobs)
		{
			printf("FAILED\n");
			return(1);
		}
		else
		{
			printf("DONE...all submitted jobs either ended or were removed\n");
			return(0);
		}
	}

	if( found ) {
		printf("%d\n", returnval);
		return 0;
	} else {
		fprintf(stderr,	"The log file did not contain the return value of "
				"the specified process.\n");
		return -1;
	}
}

