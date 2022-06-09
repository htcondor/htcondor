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
#include "read_user_log.h"
#include "format_time.h"
#include "HashTable.h"
#include "internet.h"
#include "condor_distribution.h"
#include "condor_sockaddr.h"
#include "ipv6_hostname.h"

/* 
** Job Record format: cluster.proc evict_time wall_time good_time cpu_usage
** Host Record format: host wall_hours, good_hours, cpu_hours
**                     avg_alloc avg_lost goodput util
** Job Summary format: cluster.proc wall_hours good_hours cpu_hours
**                     avg_alloc avg_lost goodput util
**   evict-time: Unix time_t when job was evicted
**   wall-time: wall-clock seconds for this run
**   good_time: wall-clock seconds of useful run time (i.e., saved in ckpt)
**   cpu-usage: remote cpu usage for this run
**   wall_hours: total wall_time / 3600
**   good_hours: total good_time / 3600
**   cpu_hours: total cpu_usage  / 3600
**   avg_alloc: average allocation time
**   avg_lost: average time lost when killed without a checkpoint
**   goodput: good_time/wall_time
**   util: cpu_usage/wall_time
*/

const char usage[] =
"usage: %s [-total | -raw] [-debug] [-evict] [-j cluster[.proc]] [-all] [-hostname] logfile ...\n"
"\t-help\t\tThis message\n"
"\t-total\t\tOnly display job totals\n"
"\t-raw\t\tDisplay raw data only\n"
"\t-debug\t\tDebug mode\n"
"\t-j\t\tSelect a specific cluster or cluster.proc\n"
"\t-evict\t\tSelect only allocations which ended due to eviction\n"
"\t-all\t\tSelect all clusters and all allocations\n"
"\t-hostname\tDisplay hostname instead of IP address\n";

void read_log(const char *filename, int select_cluster, int select_proc);
void display_stats();

struct JobStatistics {
	JobStatistics(int Cluster, int Proc) :
		cluster(Cluster), proc(Proc), allocations(0), kills(0), 
		wall_time(0), good_time(0), cpu_usage(0) {}
	int cluster, proc;
	int allocations, kills;
	int wall_time;
	int good_time;
	int cpu_usage;
};

struct HostStatistics {
	HostStatistics(const char executeHost[]) :
		allocations(0), kills(0),
		wall_time(0),	good_time(0), cpu_usage(0) 
		{ strcpy_len(host, executeHost, COUNTOF(host)); }
	char host[128];
	int allocations, kills;
	int wall_time;
	int good_time;
	int cpu_usage;
};

// explicit template instantiation

HashTable<std::string, JobStatistics *> Stats(hashFunction);
HashTable<std::string, HostStatistics *> HStats(hashFunction);
int numJobStats = 0;
int numHostStats = 0;
bool totals_only = false;
bool debug_mode = false;
bool evict_only = false;
bool raw_data = false;
bool avoid_dns = true;

int
main(int argc, char *argv[])
{
	bool log_read = false;
	int select_cluster = -1, select_proc = -1;

	if (argc == 1) {
		fprintf(stderr, usage, argv[0]);
		exit(1);
	}

	for (int i=1; i < argc; i++) {
		if (argv[i][0] == '-') {
			switch (argv[i][1]) {
			case 'a':
				select_cluster = select_proc = -1;
				evict_only = false;
				break;
			case 'j': {
				i++;
				if (1 != sscanf(argv[i], "%d", &select_cluster))
					select_cluster = -1;
				char *proc_str = strchr(argv[i], '.');
				if (proc_str) {
					if (1 != sscanf(proc_str+1, "%d", &select_proc))
						select_proc = -1;
				}
				break;
			}
			case 't': {
				totals_only = true;
				break;
			}
			case 'd': {
				debug_mode = true;
				break;
			}
			case 'e': {
				evict_only = true;
				break;
			}
			case 'r': {
				raw_data = true;
				break;
			}
			case 'h': {
				if (argv[i][2] == 'o') {
					avoid_dns = false;
				} else {
					fprintf(stderr, usage, argv[0]);
					exit(0);
				}
				break;
			}
			default:
				fprintf(stderr, usage, argv[0]);
				exit(1);
			}
		} else {
			if (access(argv[i], R_OK) < 0) {
				fprintf(stderr, "Can't access file: %s (errno=%d)\n", argv[i],
						errno);
			} else {
				read_log(argv[i], select_cluster, select_proc);
				log_read = true;
			}
		}
	}

	if (log_read && !raw_data) {
		printf("\n");
		display_stats();
	}

	return 0;
}

int
statsort(const void *vi, const void *vj)
{
	const JobStatistics* const *i;
	const JobStatistics* const *j;
	i = (const JobStatistics* const *)vi;
	j = (const JobStatistics* const *)vj;
	int clustercomp;
	clustercomp = (*i)->cluster - (*j)->cluster;
	if (clustercomp == 0) {
		return (*i)->proc - (*j)->proc;
	}
	return clustercomp;
}

void
display_stats()
{
	int i;

	// display HostStatistics
	printf("%-15.15s %9.9s %9.9s %9.9s %9.9s %9.9s %7.7s %6.6s\n",
		   "Host/Job", "Wall Time", "Good Time", "CPU Usage",
		   "Avg Alloc", "Avg Lost", "Goodput", "Util.");
	printf("\n");

	HostStatistics *hs;
	HStats.startIterations();
	while (HStats.iterate(hs) == 1) {
		printf("%-15.15s ", hs->host);
		printf("%9.9s ", format_time_nosecs(hs->wall_time));
		printf("%9.9s ", format_time_nosecs(hs->good_time));
		printf("%9.9s ", format_time_nosecs(hs->cpu_usage));
		printf("%9.9s ", format_time_nosecs(hs->wall_time/hs->allocations));
		printf("%9.9s ", hs->kills ?
			   format_time_nosecs((hs->wall_time-hs->good_time)/hs->kills) :
			   "0+00:00");
		printf("%6.1f%% %5.1f%%\n",
			   hs->wall_time ?
			   float(hs->good_time)/float(hs->wall_time)*100 : 0.0,
			   hs->good_time ?
			   float(hs->cpu_usage)/float(hs->good_time)*100 : 0.0);
		delete hs;
	}
	HStats.clear();
	printf("\n");

	// display JobStatistics
	JobStatistics **statarray = new JobStatistics* [numJobStats];
	ASSERT(statarray);
	JobStatistics *js;
	Stats.startIterations();
	for (i=0; Stats.iterate(js) == 1; i++) {
		statarray[i] = js;
	}

	qsort(statarray, numJobStats, sizeof(JobStatistics *), statsort);

	int allocations=0, kills=0;
	int wall_time=0, good_time=0, cpu_usage=0;
	char job[40];
	for (i=0; i < numJobStats; i++) {
		js = statarray[i];
		sprintf(job, "%d.%d", js->cluster, js->proc);
		printf("%-15.15s ", job);
		printf("%9.9s ", format_time_nosecs(js->wall_time));
		printf("%9.9s ", format_time_nosecs(js->good_time));
		printf("%9.9s ", format_time_nosecs(js->cpu_usage));
		printf("%9.9s ", format_time_nosecs(js->wall_time/js->allocations));
		printf("%9.9s ", js->kills ?
			   format_time_nosecs((js->wall_time-js->good_time)/js->kills) :
			   "0+00:00");
		printf("%6.1f%% %5.1f%%\n",
			   js->wall_time ?
			   float(js->good_time)/float(js->wall_time)*100 : 0.0,
			   js->good_time ?
			   float(js->cpu_usage)/float(js->good_time)*100 : 0.0);
		allocations += js->allocations;
		kills += js->kills;
		wall_time += js->wall_time;
		good_time += js->good_time;
		cpu_usage += js->cpu_usage;
		delete js;
	}
	printf("\n");
	printf("%-15.15s ", "Total");
	printf("%9.9s ", format_time_nosecs(wall_time));
	printf("%9.9s ", format_time_nosecs(good_time));
	printf("%9.9s ", format_time_nosecs(cpu_usage));
	printf("%9.9s ", allocations ?
		   format_time_nosecs(wall_time/allocations) :
		   "0+00:00");
	printf("%9.9s ", kills ?
		   format_time_nosecs((wall_time-good_time)/kills) :
		   "0+00:00");
	printf("%6.1f%% %5.1f%%\n",
		   wall_time ? float(good_time)/float(wall_time)*100 : 0.0,
		   good_time ? float(cpu_usage)/float(good_time)*100 : 0.0);
	Stats.clear();
	delete [] statarray;
}

void
new_record(int cluster, int proc, int start_time, int evict_time, 
		   int good_time, int cpu_usage, char const *host)
{
	static bool initialized = false;
	char hash[40];
	int wall_time = evict_time-start_time;

	// We detect bad records here.  One cause of bad records is the
	// fact that userlogs timestamps do not contain years, so we
	// always assume we are in the same year, which causes time to run
	// backwards at the turn of the year.
	if (wall_time < 0 ||
		good_time < 0 ||
		cpu_usage < 0) {
		if (debug_mode) {
			fprintf(stderr, "internal error: negative time computed for "
					"%d.%d!\n", cluster, proc);
			fprintf(stderr, "  (records around the turn of "
					"the year are not handled correctly)\n");
		}
		return;
	}

	sprintf(hash, "%d.%d", cluster, proc);

	JobStatistics *js = NULL;
	if (Stats.lookup(hash, js) < 0) {
		js = new JobStatistics(cluster, proc);
		Stats.insert(hash, js);
		numJobStats++;
	}
	js->allocations++;
	js->kills += (wall_time != good_time);
	js->wall_time += wall_time;
	js->good_time += good_time;
	js->cpu_usage += cpu_usage;

	char ip_addr[128];
	// only use the IP address in the key [TODO:IPV6] Parse IPv6 Addr
	strncpy(ip_addr, host+1, sizeof(ip_addr)-1);
	ip_addr[sizeof(ip_addr)-1] = '\0';
	for (int i=0; i < 128; i++) {
		if (ip_addr[i] == ':') {
			ip_addr[i] = '\0';
			break;
		}
	}
	HostStatistics *hs = NULL;
	if (HStats.lookup(ip_addr, hs) < 0) {
		condor_sockaddr addr;
		const char* hostname = NULL;
		std::string hostname_str;
		addr.from_sinful(host);
		if (!avoid_dns) {
			hostname_str = get_hostname(addr);
			hostname = hostname_str.c_str();
		}
		if (hostname == NULL) {
			hostname = ip_addr;
		}
		hs = new HostStatistics(hostname);
		HStats.insert(ip_addr, hs);
		numHostStats++;
	}
	hs->allocations++;
	hs->kills += (wall_time != good_time);
	hs->wall_time += wall_time;
	hs->good_time += good_time;
	hs->cpu_usage += cpu_usage;

	if (!totals_only) {
		if (!raw_data && !initialized) {
			printf("\n");
			printf("%-8.8s %-15.15s %-11.11s %-11.11s %9s %9s %9s\n",
				   "Job", "Host", "Start Time", "Evict Time", "Wall Time",
				   "Good Time", "CPU Usage");
			initialized = true;
		}

		printf("%-8.8s %-15.15s ", hash, hs->host);
		printf("%11s ", format_date(start_time));
		printf("%11s ", format_date(evict_time));
		printf("%9s ", format_time_nosecs(wall_time));
		printf("%9s ", format_time_nosecs(good_time));
		printf("%9s\n", format_time_nosecs(cpu_usage));
	}
}

void
read_log(const char *filename, int select_cluster, int select_proc)
{
	ReadUserLog ulog;
	ULogEventOutcome result;
	ULogEvent *event=NULL;
	char hash[40];
	HashTable<std::string, ExecuteEvent *> ExecRecs(hashFunction);
	HashTable<std::string, CheckpointedEvent *> CkptRecs(hashFunction);
	
	if (ulog.initialize(filename,0,false,true)==false) {
		fprintf(stderr,
			"Error: unable to read log file.\n");
			exit(0);			
			}		
	
	while ((result = ulog.readEvent(event)) != ULOG_NO_EVENT) {
		if ((result == ULOG_OK) &&
			(select_cluster == -1 || select_cluster == event->cluster) &&
			(select_proc == -1 || select_proc == event->proc)) {
			switch (event->eventNumber) {
			case ULOG_SUBMIT:
				delete event;
				break;
			case ULOG_EXECUTE: {
				sprintf(hash, "%d.%d", event->cluster, event->proc);
				// check if we already have an execute event for this job
				ExecuteEvent *execEvent;
				if (ExecRecs.lookup(hash, execEvent) >= 0) {
					// This means we found two execute events for the
					// job not separated by an evict or terminate
					// event.  Which one should we throw out?  If the
					// executeHosts are the same, then we throw out
					// the later event, since the 6.1.15 and 6.1.16
					// shadows logged execute events before every
					// other event, so keeping the first execute event
					// gives correct results for those shadows.
					// Otherwise, we throw out the previous event.
					if (!strcmp(((ExecuteEvent *)event)->getExecuteHost(),
								execEvent->getExecuteHost())) {
						if (debug_mode) {
							fprintf(stderr,
									"warning: discarding execute event "
									"(job %s)\n  found before evict or "
									"termination event for previous execute "
									"event.\n", hash);
						}
						delete event;
						break;
					}
					if (ExecRecs.remove(hash) < 0) {
						if (debug_mode) {
							fprintf(stderr, "internal error: hashtable remove "
									"failed for exec event %s!\n", hash);
						}
						delete event;
						break;
					}
					if (debug_mode) {
						fprintf(stderr, "warning: discarding execute event "
								"(job %s)\n  with no corresponding evict or "
								"termination event.\n", hash);
					}
					delete execEvent;
				}
				ExecRecs.insert(hash, (ExecuteEvent *)event);
				break;
			}
			case ULOG_CHECKPOINTED: {
				sprintf(hash, "%d.%d", event->cluster, event->proc);
				// remove any previous ckpt events for this job
				CheckpointedEvent *ckptEvent;
				if (CkptRecs.lookup(hash, ckptEvent) >= 0) {
					if (CkptRecs.remove(hash) < 0) {
						if (debug_mode) {
							fprintf(stderr, "internal error: hashtable remove "
									"failed for ckpt event %s!\n", hash);
						}
						delete event;
						break;
					}
					delete ckptEvent;
				}
				CkptRecs.insert(hash, (CheckpointedEvent *)event);
				break;
			}
			case ULOG_JOB_EVICTED: {
				ExecuteEvent *execEvent;
				JobEvictedEvent *evictEvent = (JobEvictedEvent *)event;
				sprintf(hash, "%d.%d", event->cluster, event->proc);
				if (ExecRecs.lookup(hash, execEvent) < 0) {
					if (debug_mode) {
						fprintf(stderr,
								"internal error: can't find execute event for "
								"evicted job %s!\n", hash);
					}
					delete event;
					break;
				}
				if (ExecRecs.remove(hash) < 0) {
					if (debug_mode) {
						fprintf(stderr,
								"internal error: hashtable remove failed for "
								"%s!\n", hash);
					}
					delete event;
					break;
				}
				time_t start_time, end_time, ckpt_time=0;
				start_time = execEvent->GetEventclock();
				end_time = event->GetEventclock();
				int cpu_usage = 0;
				if (evictEvent->checkpointed) {
					ckpt_time = end_time;
					cpu_usage = evictEvent->run_remote_rusage.ru_utime.tv_sec +
						evictEvent->run_remote_rusage.ru_stime.tv_sec;
				} else {
					CheckpointedEvent *ckptEvent;
					if (CkptRecs.lookup(hash, ckptEvent) >= 0) {
						ckpt_time = ckptEvent->GetEventclock();
						cpu_usage =
							ckptEvent->run_remote_rusage.ru_utime.tv_sec +
							ckptEvent->run_remote_rusage.ru_stime.tv_sec;
					}
					if (ckpt_time < start_time) {
						ckpt_time = start_time;
						cpu_usage = 0;
					}
				}
				new_record(event->cluster, event->proc, (int)start_time,
						   (int)end_time,
						   (int)ckpt_time-start_time, cpu_usage,
						   execEvent->getExecuteHost());
				delete execEvent;
				delete event;
				break;
			}
			case ULOG_JOB_TERMINATED: {
				ExecuteEvent *execEvent;
				JobTerminatedEvent *terminateEvent =
					(JobTerminatedEvent *)event;
				sprintf(hash, "%d.%d", event->cluster, event->proc);
				if (ExecRecs.lookup(hash, execEvent) < 0) {
					if (debug_mode) {
						fprintf(stderr,
								"internal error: can't find execute event for "
								"terminated job %s!\n", hash);
					}
					delete event;
					break;
				}
				if (ExecRecs.remove(hash) < 0) {
					if (debug_mode) {
						fprintf(stderr,
								"internal error: hashtable remove failed "
								"for %s!\n", hash);
					}
					delete event;
					break;
				}
				time_t start_time, end_time;
				start_time = execEvent->GetEventclock();
				end_time = event->GetEventclock();
				if (!evict_only) {
					new_record(event->cluster, event->proc, (int)start_time,
							   (int)end_time,
							   (int)end_time-start_time,
							   terminateEvent->
							   run_remote_rusage.ru_utime.tv_sec +
							   terminateEvent->
							   run_remote_rusage.ru_stime.tv_sec,
							   execEvent->getExecuteHost());
				}
				delete execEvent;
				delete event;
				break;
			}
			case ULOG_IMAGE_SIZE:
				delete event;
				break;
			case ULOG_EXECUTABLE_ERROR:
			case ULOG_JOB_RECONNECT_FAILED:
			case ULOG_SHADOW_EXCEPTION: {
				ExecuteEvent *execEvent;
				sprintf(hash, "%d.%d", event->cluster, event->proc);
				if (ExecRecs.lookup(hash, execEvent) < 0) {
					if (debug_mode) {
						fprintf(stderr,
								"internal error: can't find execute event for "
								"job %s!\n", hash);
					}
					delete event;
					break;
				}
				if (ExecRecs.remove(hash) < 0) {
					if (debug_mode) {
						fprintf(stderr,
								"internal error: hashtable remove failed for "
								"%s!\n", hash);
					}
					delete event;
					break;
				}
				time_t start_time, end_time, ckpt_time=0;
				start_time = execEvent->GetEventclock();
				end_time = event->GetEventclock();
				int cpu_usage = 0;
				CheckpointedEvent *ckptEvent;
				if (CkptRecs.lookup(hash, ckptEvent) >= 0) {
					ckpt_time = ckptEvent->GetEventclock();
					cpu_usage = ckptEvent->run_remote_rusage.ru_utime.tv_sec +
						ckptEvent->run_remote_rusage.ru_stime.tv_sec;
				}
				if (ckpt_time < start_time) {
					ckpt_time = start_time;
					cpu_usage = 0;
				}
				if (!evict_only) {
					new_record(event->cluster, event->proc, (int)start_time,
							   (int)end_time,
							   (int)ckpt_time-start_time, cpu_usage,
							   execEvent->getExecuteHost());
				}
				delete execEvent;
				delete event;
				break;
			}
			default:
				// ignore the rest of the eveats
				delete event;
				break;
			}
		} else {
			
			if (event!=NULL) delete event;
			if ((result==ULOG_RD_ERROR) || (result == ULOG_UNK_ERROR)) {
				fprintf(stderr,
						"Error: unable to read log file.\n");
				exit(0);
			
			}
		}
	}

	// delete any remaining items in the tables
	ExecuteEvent *execEvent;
	ExecRecs.startIterations();
	while (ExecRecs.iterate(execEvent) == 1) {
		delete execEvent;
	}
	CheckpointedEvent *ckptEvent;
	CkptRecs.startIterations();
	while (CkptRecs.iterate(ckptEvent) == 1) {
		delete ckptEvent;
	}
}
