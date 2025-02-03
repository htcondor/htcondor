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

#ifdef WIN32
#include "condor_header_features.h"
#include "condor_sys_nt.h"
#else
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include "read_user_log.h"
#include "classad/classad_distribution.h"

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
	char read_error = 0;
	char found = 0;
    const char* hostmachine = NULL;
	const char* extractptr = NULL;
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
	std::string eventtestval;

	while( !done ) {
        ULogEventOutcome outcome = rul->readEvent(e);
		//cout << ULogEventOutcomeNames[outcome] << endl;
		//printf("Outcome from rul->readEvent(e) is %d\n",outcome);
        switch( outcome ) {
		  case ULOG_RD_ERROR:
			printf("RD_ERROR\n");
			read_error++;
			done++;
			break;
		  case ULOG_NO_EVENT:
			done++;
			break;
		  default:
		  case ULOG_UNK_ERROR:
		  case ULOG_MISSED_EVENT:
			printf("ULOG_outcome_%d\n", outcome);
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
				//cout << "+-" << e->eventName() << endl;
			}
			if( strcmp(action, "MACHINE") == 0 ) 
			{
				printf("MACHINE\n");
				switch( e->eventNumber ) 
				{
					case ULOG_EXECUTE:
						//printf("Job executed\n");
						//returnval = ((ExecuteEvent*)e)->return_value;
						extractptr = hostmachine = ((ExecuteEvent*)e)->getExecuteHost();
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
					{
						NodeTerminatedEvent *info = dynamic_cast<NodeTerminatedEvent*>(e);
						if (!info) {
							printf("Event isn't a NodeTerminatedEvent!!!\n");
							break;
						}

						// print a attribute which is always there
						classad::ClassAd * ad = reinterpret_cast<classad::ClassAd*>(info->toClassAd(true));
						if ( ! ad) { printf("toClassAd failed!\n"); break; }

						int rmem = -1, memu = -1, mem = -1;
						if (!ad->EvaluateAttrNumber("Memory", mem)) {
							printf("\tMemory not found!\n");
						}
						if (!ad->EvaluateAttrNumber("MemoryUsage", memu)) {
							printf("\tMemoryUsage not found!\n");
						}
						if (!ad->EvaluateAttrNumber("RequestMemory", rmem)) {
							printf("\tRequestMemory not found!\n");
						}
						printf("\tMemory : requested %d, got %d, used %d\n", rmem, mem, memu);
						delete ad;
					}
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
				  case ULOG_JOB_DISCONNECTED:
					printf("Job Disconnected.........\n");
					break;
				  case ULOG_JOB_RECONNECTED:
					printf("Job Reconnected.........\n");
					break;
				  case ULOG_JOB_RECONNECT_FAILED:
					printf("Job Reconnect Failed.........\n");
					break;
				  case ULOG_GRID_RESOURCE_UP:
					printf("Grid Resource Up.........\n");
					break;
				  case ULOG_GRID_RESOURCE_DOWN:
					printf("Grid Resource Down.........\n");
					break;
				  case ULOG_GRID_SUBMIT:
					printf("Job Submitted remotely.........\n");
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
						printf( "\t%s not found!\n", attr );
					}
					else {
						printf("\t%s is %d\n", attr, myjobstatus);
					}

					attr = "BillString";
					if ( !info->LookupString(attr, eventtestval) ) {
						printf( "\t%s not found!\n", attr );
					}
					else {
						printf("\t%s is \"%s\"\n", attr, eventtestval.c_str());
					}

					attr = "BillInt";
					if ( !info->LookupInteger(attr, myjobstatus) ) {
						printf( "\t%s not found!\n", attr );
					}
					else {
						printf("\t%s is %d\n", attr, myjobstatus);
					}

					attr = "BillBool";
					if ( !info->LookupInteger(attr, myjobstatus) ) {
						printf( "\t%s not found!\n", attr );
					}
					else {
						printf("\t%s is %s\n", attr, myjobstatus ? "true" : "false");
					}
					break;
				  }
				  case ULOG_JOB_STATUS_UNKNOWN:
					printf("Job Status Unknown.........\n");
					break;
				  case ULOG_JOB_STATUS_KNOWN:
					printf("Job Status Known.........\n");
					break;
				  case ULOG_JOB_STAGE_IN:
					printf("Job performing stage-in.........\n");
					break;
				  case ULOG_JOB_STAGE_OUT:
					printf("Job performing stage-out.........\n");
					break;
				  case ULOG_ATTRIBUTE_UPDATE:
					printf("Attribute updated.........\n");
					break;
				  case ULOG_PRESKIP:
					printf("PRE_SKIP event for DAGMan.........\n");
					break;
				  case ULOG_CLUSTER_SUBMIT:
					printf("Cluster submitted.........\n");
					break;
				  case ULOG_CLUSTER_REMOVE:
					printf("Cluster removed.........\n");
					break;
				  case ULOG_FACTORY_PAUSED:
					printf("Factory paused.........\n");
					break;
				  case ULOG_FACTORY_RESUMED:
					printf("Factory resumed.........\n");
					break;
				  case ULOG_NONE:
					printf("None (try again later).........\n");
					break;
				  case ULOG_FILE_TRANSFER:
					printf("File transfer.........\n");
					break;

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
					found++;
					break;
				  case ULOG_JOB_ABORTED:
					//returnval = ((JobEvictedEvent*)e)->return_value;
					returnval = -99;
					//printf("Job ABORTED by user.........\n");
					// allow a later run to return a good value bt 12/13/03
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
		if (read_error) {
			printf("DONE (read error)\n");
		} else {
			printf("DONE\n");
		}
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
		if(running_jobs > 0)
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

