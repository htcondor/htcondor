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

#include "condor_common.h"
#include "user_log.c++.h"
#include "classad_hashtable.h"
#include "internet.h"

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
		wall_time(0.0), good_time(0.0), cpu_usage(0.0) {}
	int cluster, proc;
	int allocations, kills;
	double wall_time;
	double good_time;
	double cpu_usage;
};

struct HostStatistics {
	HostStatistics(const char executeHost[]) :
		allocations(0), kills(0),
		wall_time(0.0),	good_time(0.0), cpu_usage(0.0) 
		{ strcpy(host, executeHost); }
	char host[128];
	int allocations, kills;
	double wall_time;
	double good_time;
	double cpu_usage;
};

// explicit template instantiation
template class HashTable<HashKey, ExecuteEvent *>;
template class HashBucket<HashKey, ExecuteEvent *>;
template class HashTable<HashKey, CheckpointedEvent *>;
template class HashBucket<HashKey, CheckpointedEvent *>;
template class HashTable<HashKey, JobStatistics *>;
template class HashBucket<HashKey, JobStatistics *>;
template class HashTable<HashKey, HostStatistics *>;
template class HashBucket<HashKey, HostStatistics *>;

HashTable<HashKey, JobStatistics *> Stats(1024, hashFunction);
HashTable<HashKey, HostStatistics *> HStats(1024, hashFunction);
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
				sscanf(argv[i], "%d", &select_cluster);
				char *proc_str = strchr(argv[i], '.');
				if (proc_str) {
					sscanf(proc_str+1, "%d", &select_proc);
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
					exit(1);
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
statsort(const JobStatistics **i, const JobStatistics **j)
{
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
	const char TitleFormat[] =
		"%-15.15s %9.9s %9.9s %9.9s %9.9s %9.9s %7.7s %6.6s\n";
	const char RecordFormat[] =
		"%-15.15s %9.0f %9.0f %9.0f %9.0f %9.0f %6.1f%% %5.1f%%\n";

	// display HostStatistics
	printf(TitleFormat, "", "Wall Time", "Good Time", "CPU Usage",
		   "Avg Alloc", "Avg Lost", "", "");
	printf(TitleFormat, "Host/Job", "(hours)", "(hours)", "(hours)",
		   "(minutes)", "(minutes)", "Goodput", "Util.");
	printf("\n");

	HostStatistics *hs;
	HStats.startIterations();
	while (HStats.iterate(hs) == 1) {
		printf(RecordFormat, hs->host, hs->wall_time/3600, hs->good_time/3600,
			   hs->cpu_usage/3600, hs->wall_time/hs->allocations/60,
			   hs->kills ? (hs->wall_time-hs->good_time)/hs->kills/60 : 0.0,
			   hs->wall_time ? hs->good_time/hs->wall_time*100 : 0.0,
			   hs->good_time ? hs->cpu_usage/hs->good_time*100 : 0.0);
		delete hs;
	}
	HStats.clear();
	printf("\n");

	// display JobStatistics
	JobStatistics **statarray = new (JobStatistics *) [numJobStats];
	JobStatistics *js;
	Stats.startIterations();
	for (int i=0; Stats.iterate(js) == 1; i++) {
		statarray[i] = js;
	}

	qsort(statarray, numJobStats, sizeof(JobStatistics *), statsort);

	int allocations=0, kills=0;
	double wall_time=0.0, good_time=0.0, cpu_usage=0.0;
	char job[40];
	for (int i=0; i < numJobStats; i++) {
		js = statarray[i];
		sprintf(job, "%d.%d", js->cluster, js->proc);
		printf(RecordFormat, job, js->wall_time/3600, js->good_time/3600,
			   js->cpu_usage/3600, js->wall_time/js->allocations/60,
			   js->kills ? (js->wall_time-js->good_time)/js->kills/60 : 0.0,
			   js->wall_time ? js->good_time/js->wall_time*100 : 0.0,
			   js->good_time ? js->cpu_usage/js->good_time*100 : 0.0);
		allocations += js->allocations;
		kills += js->kills;
		wall_time += js->wall_time;
		good_time += js->good_time;
		cpu_usage += js->cpu_usage;
		delete js;
	}
	printf("\n");
	printf(RecordFormat, "Total", wall_time/3600, good_time/3600,
		   cpu_usage/3600, wall_time/allocations/60,
		   kills ? (wall_time-good_time)/kills/60 : 0.0,
		   wall_time ? good_time/wall_time*100 : 0.0,
		   good_time ? cpu_usage/good_time*100 : 0.0);
	Stats.clear();
	delete [] statarray;
}

void
new_record(int cluster, int proc, int evict_time, int wall_time,
		   int good_time, int cpu_usage, char host[], int start_hour,
		   int end_hour, int day)
{
	static bool initialized = false;
	char hash[40];

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
	// Another type of bad record is when cpu_usage is greater than
	// good_time.  This should not be possible.  It may be caused if
	// two instances of the same job are running at the same time.
	if (cpu_usage > good_time) {
		if (debug_mode) {
			fprintf(stderr, "internal error: cpu usage > good time (%d > %d) "
					"for %d.%d!\n", cpu_usage, good_time, cluster, proc);
		}
		return;
	}

	sprintf(hash, "%d.%d", cluster, proc);
	HashKey jobkey(hash);

	JobStatistics *js;
	if (Stats.lookup(jobkey, js) < 0) {
		js = new JobStatistics(cluster, proc);
		Stats.insert(jobkey, js);
		numJobStats++;
	}
	js->allocations++;
	js->kills += (wall_time != good_time);
	js->wall_time += (double)wall_time;
	js->good_time += (double)good_time;
	js->cpu_usage += (double)cpu_usage;

	char ip_addr[128];
	// only use the IP address in the key
	strcpy(ip_addr, host+1);
	for (int i=0; i < 128; i++) {
		if (ip_addr[i] == ':') {
			ip_addr[i] = '\0';
			break;
		}
	}
	HostStatistics *hs;
	HashKey hostkey(ip_addr);
	if (HStats.lookup(hostkey, hs) < 0) {
		struct sockaddr_in sin;
		string_to_sin(host, &sin);
		char *hostname = NULL;
		if (!avoid_dns) {
			hostname = sin_to_hostname(&sin, NULL);
		}
		if (hostname == NULL) {
			hostname = ip_addr;
		}
		hs = new HostStatistics(hostname);
		HStats.insert(hostkey, hs);
		numHostStats++;
	}
	hs->allocations++;
	hs->kills += (wall_time != good_time);
	hs->wall_time += (double)wall_time;
	hs->good_time += (double)good_time;
	hs->cpu_usage += (double)cpu_usage;

	const char TitleFormat[] =
		"%-8.8s %-14.15s %10s %9s %9s %9s %5s %3s %3s\n";
	const char RecordFormat[] =
		"%-8.8s %-15.15s %10d %9d %9d %9d %5d %3d %3d\n";

	if (!totals_only) {
		if (!raw_data && !initialized) {
			printf("\n");
			printf(TitleFormat, "", "", "Evict Time", "Wall Time", "Good Time",
				   "CPU Usage", "Start", "End", "");
			printf(TitleFormat, "Job", "Host", "(time_t)", "(seconds)",
				   "(seconds)", "(seconds)", "(hr.)", "(h)", "Day");
			initialized = true;
		}

		printf(RecordFormat, hash, hs->host, evict_time, wall_time,
			   good_time, cpu_usage, start_hour, end_hour, day);
	}
}

void
read_log(const char *filename, int select_cluster, int select_proc)
{
	ReadUserLog ulog;
	ULogEventOutcome result;
	ULogEvent *event;
	char hash[40];
	HashTable<HashKey, ExecuteEvent *> ExecRecs(1024, hashFunction);
	HashTable<HashKey, CheckpointedEvent *> CkptRecs(1024, hashFunction);

	ulog.initialize(filename);

	while ((result = ulog.readEvent(event)) != ULOG_NO_EVENT) {
		if ((result == ULOG_OK) &&
			(select_cluster == -1 || select_cluster == event->cluster) &&
			(select_proc == -1 || select_proc == event->proc)) {
			switch (event->eventNumber) {
			case ULOG_SUBMIT:
				delete event;
				break;
			case ULOG_EXECUTE:
				sprintf(hash, "%d.%d", event->cluster, event->proc);
				ExecRecs.insert(HashKey(hash), (ExecuteEvent *)event);
				break;
			case ULOG_CHECKPOINTED: {
				sprintf(hash, "%d.%d", event->cluster, event->proc);
				HashKey key(hash);
				// remove any previous ckpt events for this job
				CheckpointedEvent *ckptEvent;
				if (CkptRecs.lookup(key, ckptEvent) >= 0) {
					if (CkptRecs.remove(key) < 0) {
						if (debug_mode) {
							fprintf(stderr, "internal error: hashtable remove "
									"failed for ckpt event %s!\n", hash);
						}
						delete event;
						break;
					}
					delete ckptEvent;
				}
				CkptRecs.insert(HashKey(hash), (CheckpointedEvent *)event);
				break;
			}
			case ULOG_JOB_EVICTED: {
				ExecuteEvent *execEvent;
				JobEvictedEvent *evictEvent = (JobEvictedEvent *)event;
				sprintf(hash, "%d.%d", event->cluster, event->proc);
				HashKey key(hash);
				if (ExecRecs.lookup(key, execEvent) < 0) {
					if (debug_mode) {
						fprintf(stderr,
								"internal error: can't find execute event for "
								"evicted job %s!\n", hash);
					}
					delete event;
					break;
				}
				if (ExecRecs.remove(key) < 0) {
					if (debug_mode) {
						fprintf(stderr,
								"internal error: hashtable remove failed for "
								"%s!\n", hash);
					}
					delete event;
					break;
				}
				time_t start_time, end_time, ckpt_time=0;
				start_time = mktime(&execEvent->eventTime);
				end_time = mktime(&event->eventTime);
				int cpu_usage = 0;
				if (evictEvent->checkpointed) {
					ckpt_time = end_time;
					cpu_usage = evictEvent->run_remote_rusage.ru_utime.tv_sec +
						evictEvent->run_remote_rusage.ru_stime.tv_sec;
				} else {
					CheckpointedEvent *ckptEvent;
					if (CkptRecs.lookup(key, ckptEvent) >= 0) {
						ckpt_time = mktime(&ckptEvent->eventTime);
						cpu_usage =
							ckptEvent->run_remote_rusage.ru_utime.tv_sec +
							ckptEvent->run_remote_rusage.ru_stime.tv_sec;
					}
					if (ckpt_time < start_time) {
						ckpt_time = start_time;
						cpu_usage = 0;
					}
				}
				new_record(event->cluster, event->proc,
						   (int)end_time, (int)end_time-start_time,
						   (int)ckpt_time-start_time, cpu_usage,
						   execEvent->executeHost,
						   execEvent->eventTime.tm_hour,
						   event->eventTime.tm_hour,
						   event->eventTime.tm_wday);
				delete execEvent;
				delete event;
				break;
			}
			case ULOG_JOB_TERMINATED: {
				ExecuteEvent *execEvent;
				JobTerminatedEvent *terminateEvent =
					(JobTerminatedEvent *)event;
				sprintf(hash, "%d.%d", event->cluster, event->proc);
				HashKey key(hash);
				if (ExecRecs.lookup(key, execEvent) < 0) {
					if (debug_mode) {
						fprintf(stderr,
								"internal error: can't find execute event for "
								"terminated job %s!\n", hash);
					}
					delete event;
					break;
				}
				if (ExecRecs.remove(key) < 0) {
					if (debug_mode) {
						fprintf(stderr,
								"internal error: hashtable remove failed "
								"for %s!\n", hash);
					}
					delete event;
					break;
				}
				time_t start_time, end_time;
				start_time = mktime(&execEvent->eventTime);
				end_time = mktime(&event->eventTime);
				if (!evict_only) {
					new_record(event->cluster, event->proc, (int)end_time,
							   (int)end_time-start_time,
							   (int)end_time-start_time,
							   terminateEvent->
							   run_remote_rusage.ru_utime.tv_sec +
							   terminateEvent->
							   run_remote_rusage.ru_stime.tv_sec,
							   execEvent->executeHost,
							   execEvent->eventTime.tm_hour,
							   event->eventTime.tm_hour,
							   event->eventTime.tm_wday);
				}
				delete execEvent;
				delete event;
				break;
			}
			case ULOG_IMAGE_SIZE:
				delete event;
				break;
			case ULOG_EXECUTABLE_ERROR:
			case ULOG_SHADOW_EXCEPTION: {
				ExecuteEvent *execEvent;
				sprintf(hash, "%d.%d", event->cluster, event->proc);
				HashKey key(hash);
				if (ExecRecs.lookup(key, execEvent) < 0) {
					if (debug_mode) {
						fprintf(stderr,
								"internal error: can't find execute event for "
								"job %s!\n", hash);
					}
					delete event;
					break;
				}
				if (ExecRecs.remove(key) < 0) {
					if (debug_mode) {
						fprintf(stderr,
								"internal error: hashtable remove failed for "
								"%s!\n", hash);
					}
					delete event;
					break;
				}
				time_t start_time, end_time, ckpt_time=0;
				start_time = mktime(&execEvent->eventTime);
				end_time = mktime(&event->eventTime);
				int cpu_usage = 0;
				CheckpointedEvent *ckptEvent;
				if (CkptRecs.lookup(key, ckptEvent) >= 0) {
					ckpt_time = mktime(&ckptEvent->eventTime);
					cpu_usage = ckptEvent->run_remote_rusage.ru_utime.tv_sec +
						ckptEvent->run_remote_rusage.ru_stime.tv_sec;
				}
				if (ckpt_time < start_time) {
					ckpt_time = start_time;
					cpu_usage = 0;
				}
				if (!evict_only) {
					new_record(event->cluster, event->proc, (int)end_time,
							   (int)end_time-start_time,
							   (int)ckpt_time-start_time, cpu_usage,
							   execEvent->executeHost,
							   execEvent->eventTime.tm_hour,
							   event->eventTime.tm_hour,
							   event->eventTime.tm_wday);
				}
				delete execEvent;
				delete event;
				break;
			}
			}
		} else {
			delete event;
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
