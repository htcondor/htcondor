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
#include "condor_config.h"
#include "condor_distribution.h"
#include "proc_family_client.h"

static int register_family(ProcFamilyClient& pfc, int argc, char* argv[]);
#if defined(LINUX)
static int track_by_associated_gid(ProcFamilyClient& pfc, int argc, 
	char* argv[]);
#endif
#if defined(HAVE_EXT_LIBCGROUP)
static int track_by_associated_cgroup(ProcFamilyClient& pfc, int argc,
	char* argv[]);
#endif
static int get_usage(ProcFamilyClient& pfc, int argc, char* argv[]);
static int dump(ProcFamilyClient& pfc, int argc, char* argv[]);
static int list(ProcFamilyClient& pfc, int argc, char* argv[]);
static int signal_process(ProcFamilyClient& pfc, int argc, char* argv[]);
static int suspend_family(ProcFamilyClient& pfc, int argc, char* argv[]);
static int continue_family(ProcFamilyClient& pfc, int argc, char* argv[]);
static int kill_family(ProcFamilyClient& pfc, int argc, char* argv[]);
static int unregister_family(ProcFamilyClient& pfc, int argc, char* argv[]);
static int snapshot(ProcFamilyClient& pfc, int argc, char* argv[]);
static int quit(ProcFamilyClient& pfc, int argc, char* argv[]);

static void
list_commands()
{
	fprintf(stderr, "  options:\n");
	fprintf(stderr, "    -h\n");
	fprintf(stderr, "    -A <procd address file>\n");
	fprintf(stderr, "  commands:\n");
	fprintf(stderr,
		"    REGISTER_FAMILY <pid> <watcher_pid> <max_snapshot_interval>\n");
#if defined(LINUX)
	fprintf(stderr, "    TRACK_BY_ASSOCIATED_GID <gid> [<pid>]\n");
#endif
#if defined(HAVE_EXT_LIBCGROUP)
	fprintf(stderr, "    TRACK_BY_ASSOCIATED_CGROUP <cgroup> [<pid>]\n");
#endif
	fprintf(stderr, "    GET_USAGE [<pid>]\n");
	fprintf(stderr, "    DUMP [<pid>]\n");
	fprintf(stderr, "    LIST [<pid>]\n");
	fprintf(stderr, "    SIGNAL_PROCESS <signal> [<pid>]\n");
	fprintf(stderr, "    SUSPEND_FAMILY [<pid>]\n");
	fprintf(stderr, "    CONTINUE_FAMILY [<pid>]\n");
	fprintf(stderr, "    KILL_FAMILY [<pid>]\n");
	fprintf(stderr, "    UNREGISTER_FAMILY <pid>\n");
	fprintf(stderr, "    SNAPSHOT\n");
	fprintf(stderr, "    QUIT\n");
}

int
main(int argc, char* argv[])
{
	char *procd_address = NULL;

	if (argc < 2) {
		fprintf(stderr, 
			"Usage: %s <options w/arguments> <command> [<arg> ...]\n",
			argv[0]);
		list_commands();
		return 1;
	}

	int cmd_argc = argc - 1;
	char** cmd_argv = argv + 1;

	// Process the first set of options.
	while(cmd_argv[0] != NULL && cmd_argv[0][0] == '-')
	{
		if (strcmp(cmd_argv[0], "-A") == MATCH) {
			cmd_argc--;
			cmd_argv++;
			if (cmd_argc == 0) {
				fprintf(stderr, "error: -A needs an argument\n");
				list_commands();
				return 1;
			}
			// store the argument to -A as the file we'll use.
			procd_address = cmd_argv[0];
			cmd_argc--;
			cmd_argv++;
			continue;
		} else if (strcmp(cmd_argv[0], "-h") == MATCH) {
			cmd_argc--;
			cmd_argv++;
			fprintf(stderr, 
				"Usage: %s <options w/arguments> <command> [<arg> ...]\n",
				argv[0]);
			list_commands();
			return 1;
			//continue;
		}

		// This is the failure case if we manage to pass all checks above.
		fprintf(stderr, "error: Don't understand option %s\n", cmd_argv[0]);
		list_commands();
		return 1;
	}

	// If there are no command line arguments left, then there is no
	// command specified after the options, which is a failure.
	if (cmd_argc == 0) {
		fprintf(stderr, 
			"Please specify a command.\n"
			"Usage: %s <options w/arguments> <command> [<arg> ...]\n",
			argv[0]);
		list_commands();
		return 1;
	}

	// If a procd address wasn't specified on the command line, see if we
	// have an entry in a config file to use.
	if (procd_address == NULL) {
		fprintf(stderr, "error: procd address (-A) not specified.\n");
		return 1;
	}

	
	ProcFamilyClient pfc;
	if (!pfc.initialize(procd_address)) {
		fprintf(stderr, "error: failed to initialize ProcD client\n");
		return 1;
	}

	// Process this single command we should be performing
	if (strcasecmp(cmd_argv[0], "REGISTER_FAMILY") == 0) {
		return register_family(pfc, cmd_argc, cmd_argv);
	}
#if defined(LINUX)
	else if (strcasecmp(cmd_argv[0], "TRACK_BY_ASSOCIATED_GID") == 0) {
		return track_by_associated_gid(pfc, cmd_argc, cmd_argv);
	}
#endif
#if defined(HAVE_EXT_LIBCGROUP)
	else if (strcasecmp(cmd_argv[0], "TRACK_BY_ASSOCIATED_CGROUP") == 0) {
		return track_by_associated_cgroup(pfc, cmd_argc, cmd_argv);
	}
#endif
	else if (strcasecmp(cmd_argv[0], "GET_USAGE") == 0) {
		return get_usage(pfc, cmd_argc, cmd_argv);
	}
	else if (strcasecmp(cmd_argv[0], "DUMP") == 0) {
		return dump(pfc, cmd_argc, cmd_argv);
	}
	else if (strcasecmp(cmd_argv[0], "LIST") == 0) {
		return list(pfc, cmd_argc, cmd_argv);
	}
	else if (strcasecmp(cmd_argv[0], "SIGNAL_PROCESS") == 0) {
		return signal_process(pfc, cmd_argc, cmd_argv);
	}
	else if (strcasecmp(cmd_argv[0], "SUSPEND_FAMILY") == 0) {
		return suspend_family(pfc, cmd_argc, cmd_argv);
	}
	else if (strcasecmp(cmd_argv[0], "CONTINUE_FAMILY") == 0) {
		return continue_family(pfc, cmd_argc, cmd_argv);
	}
	else if (strcasecmp(cmd_argv[0], "KILL_FAMILY") == 0) {
		return kill_family(pfc, cmd_argc, cmd_argv);
	}
	else if (strcasecmp(cmd_argv[0], "UNREGISTER_FAMILY") == 0) {
		return unregister_family(pfc, cmd_argc, cmd_argv);
	}
	else if (strcasecmp(cmd_argv[0], "SNAPSHOT") == 0) {
		return snapshot(pfc, cmd_argc, cmd_argv);
	}
	else if (strcasecmp(cmd_argv[0], "QUIT") == 0) {
		return quit(pfc, cmd_argc, cmd_argv);
	}
	else {
		fprintf(stderr, "error: invalid command: %s\n", cmd_argv[0]);
		list_commands();
		return 1;
	}
}

static int
register_family(ProcFamilyClient& pfc, int argc, char* argv[])
{
	if (argc != 4) {
		fprintf(stderr,
		        "error: argument synopsis for %s: "
		            "<pid> <watcher_pid> <max_snapshot interval>\n",
		        argv[0]);
		return 1;
	}
	pid_t pid = atoi(argv[1]);
	pid_t watcher = atoi(argv[2]);
	int max_snapshot_interval = atoi(argv[3]);
	bool success;
	if (!pfc.register_subfamily(pid,
	                            watcher,
	                            max_snapshot_interval,
	                            success)) {
		fprintf(stderr, "error: communication error with ProcD\n");
		return 1;
	}
	if (!success) {
		fprintf(stderr,
		        "error: %s command failed with ProcD\n",
		        argv[0]);
		return 1;
	}
	return 0;
}

#if defined(LINUX)
static int
track_by_associated_gid(ProcFamilyClient& pfc, int argc, char* argv[])
{
	if ((argc != 2) && (argc != 3)) {
		fprintf(stderr,
		        "error: argument synopsis for %s: <gid> [<pid>]\n",
		        argv[0]);
		return 1;
	}
	gid_t gid = atoi(argv[1]);
	if (gid == 0) {
		fprintf(stderr, "invalid GID: %s\n", argv[1]);
		return 1;
	}
	pid_t pid = 0;
	if (argc == 3) {
		pid = atoi(argv[2]);
		if (pid <= 0) {
			fprintf(stderr, "error: invalid pid: %s\n", argv[2]);
			return 1;
		}
	}
	bool success;
	if (!pfc.track_family_via_associated_supplementary_group(pid, gid, success)) {
	fprintf(stderr, "error: communication error with ProcD\n");
		return 1;
	}
	if (!success) {
		fprintf(stderr,
		        "error: %s command failed with ProcD\n",
		        argv[0]);
		return 1;
	}
	return 0;
}
#endif

#if defined(HAVE_EXT_LIBCGROUP)
static int
track_by_associated_cgroup(ProcFamilyClient& pfc, int argc, char* argv[])
{
	if ((argc != 2) && (argc != 3)) {
		fprintf(stderr,
			"error: argument synopsis for %s: <cgroup> [<pid>]\n",
			argv[0]);
		return 1;
	}
	const char * cgroup = argv[1];
	pid_t pid = 0;
	if (argc == 3) {
		pid = atoi(argv[2]);
		if (pid <= 0) {
			fprintf(stderr, "error: invalid pid: %s\n", argv[2]);
			return 1;
		}
	}
	bool success;
	if (!pfc.track_family_via_cgroup(pid, cgroup, success)) {
		fprintf(stderr, "error: communication error with ProcD\n");
		return 1;
	}
	if (!success) {
		fprintf(stderr,
			"error: %s command failed with ProcD\n",
			argv[0]);
		return 1;
	}
	return 0;
}
#endif

static int
dump(ProcFamilyClient& pfc, int argc, char* argv[])
{
	if (argc > 2) {
		fprintf(stderr,
		        "error: argument synopsis for %s: [<pid>]\n",
		        argv[0]);
		return 1;
	}
	pid_t pid = 0;
	if (argc == 2) {
		pid = atoi(argv[1]);
		if (pid == 0) {
			fprintf(stderr, "error: invalid pid: %s\n", argv[1]);
			return 1;
		}
	}

	bool response;
	std::vector<ProcFamilyDump> vec;
	if (!pfc.dump(pid, response, vec)) {
		fprintf(stderr, "error: communication error with ProcD\n");
		return 1;
	}
	if (!response) {
		fprintf(stderr,
		        "error: %s command failed with ProcD\n",
		        argv[0]);
		return 1;
	}

	for (size_t i = 0; i < vec.size(); ++i) {
		printf("%u %u %u %d\n",
		       (unsigned)vec[i].parent_root,
		       (unsigned)vec[i].root_pid,
		       (unsigned)vec[i].watcher_pid,
		       (int)vec[i].procs.size());
		for (size_t j = 0; j < vec[i].procs.size(); ++j) {
			printf("%u %u " PROCAPI_BIRTHDAY_FORMAT " %ld %ld\n",
			       (unsigned)vec[i].procs[j].pid,
			       (unsigned)vec[i].procs[j].ppid,
			       vec[i].procs[j].birthday,
			       vec[i].procs[j].user_time,
			       vec[i].procs[j].sys_time);
		}
	}

	return 0;
}

static int
list(ProcFamilyClient& pfc, int argc, char* argv[])
{
	if (argc > 2) {
		fprintf(stderr,
		        "error: argument synopsis for %s: [<pid>]\n",
		        argv[0]);
		return 1;
	}
	pid_t pid = 0;
	if (argc == 2) {
		pid = atoi(argv[1]);
		if (pid == 0) {
			fprintf(stderr, "error: invalid pid: %s\n", argv[1]);
			return 1;
		}
	}

	bool response;
	std::vector<ProcFamilyDump> vec;
	if (!pfc.dump(pid, response, vec)) {
		fprintf(stderr, "error: communication error with ProcD\n");
		return 1;
	}
	if (!response) {
		fprintf(stderr,
		        "error: %s command failed with ProcD\n",
		        argv[0]);
		return 1;
	}

	printf("PID PPID START_TIME USER_TIME SYS_TIME\n");
	for (size_t i = 0; i < vec.size(); ++i) {
		for (size_t j = 0; j < vec[i].procs.size(); ++j) {
			printf("%u %u " PROCAPI_BIRTHDAY_FORMAT " %ld %ld\n",
			       (unsigned)vec[i].procs[j].pid,
			       (unsigned)vec[i].procs[j].ppid,
			       vec[i].procs[j].birthday,
			       vec[i].procs[j].user_time,
			       vec[i].procs[j].sys_time);
		}
	}
	return 0;
}

static int
get_usage(ProcFamilyClient& pfc, int argc, char* argv[])
{
	if (argc > 2) {
		fprintf(stderr,
		        "error: argument synopsis for %s: [<pid>]\n",
		        argv[0]);
		return 1;
	}
	pid_t pid = 0;
	if (argc == 2) {
		pid = atoi(argv[1]);
		if (pid == 0) {
			fprintf(stderr, "error: invalid pid: %s\n", argv[1]);
			return 1;
		}
	}
	ProcFamilyUsage pfu;
	bool success;
	if (!pfc.get_usage(pid, pfu, success)) {
	fprintf(stderr, "error: communication error with ProcD\n");
		return 1;
	}
	if (!success) {
		fprintf(stderr,
		        "error: %s command failed with ProcD\n",
		        argv[0]);
		return 1;
	}
	printf("Number of Processes: %d\n", pfu.num_procs);
	printf("User CPU Time (s): %ld\n", pfu.user_cpu_time);
	printf("System CPU Time (s): %ld\n", pfu.sys_cpu_time);
	printf("CPU Percentage (%%): %f\n", pfu.percent_cpu);
	printf("Maximum Image Size (KB): %lu\n", pfu.max_image_size);
	printf("Total Image Size(KB): %lu\n", pfu.total_image_size);
#if HAVE_PSS
	if (pfu.total_proportional_set_size_available) {
		printf("Proportional Set Size (KB): %lu\n", pfu.total_proportional_set_size);
	}
#endif
	if (pfu.block_read_bytes >= 0)
		printf("Bytes read from block devices (KB): %llu\n", (unsigned long long)(pfu.block_read_bytes/1024));
	if (pfu.block_write_bytes >= 0)
		printf("Bytes written to block devices (KB): %llu\n", (unsigned long long)(pfu.block_write_bytes/1024));
#ifdef LINUX
		printf("Instructions: %llu\n", (unsigned long long) pfu.m_instructions);
#endif
	return 0;
}

int
signal_process(ProcFamilyClient& pfc, int argc, char* argv[])
{
	if ((argc != 2) && (argc != 3)) {
		fprintf(stderr,
		        "error: argument synopsis for %s: <signal> [<pid>]\n",
		        argv[0]);
		return 1;
	}
	int signal = atoi(argv[1]);
	if (signal == 0) {
		fprintf(stderr, "invalid signal: %s\n", argv[1]);
		return 1;
	}
	pid_t pid = 0;
	if (argc == 3) {
		pid = atoi(argv[2]);
		if (pid == 0) {
			fprintf(stderr, "error: invalid pid: %s\n", argv[2]);
			return 1;
		}
	}
	bool success;
	if (!pfc.signal_process(pid, signal, success)) {
		fprintf(stderr, "error: communication error with ProcD\n");
		return 1;
	}
	if (!success) {
		fprintf(stderr,
		        "error: %s command failed with ProcD\n",
		        argv[0]);
		return 1;
	}
	return 0;
}

int
suspend_family(ProcFamilyClient& pfc, int argc, char* argv[])
{
	if (argc > 2) {
		fprintf(stderr,
		        "error: argument synopsis for %s: [<pid>]\n",
		        argv[0]);
		return 1;
	}
	pid_t pid = 0;
	if (argc == 2) {
		pid = atoi(argv[1]);
		if (pid == 0) {
			fprintf(stderr, "error: invalid pid: %s\n", argv[1]);
			return 1;
		}
	}
	bool success;
	if (!pfc.suspend_family(pid, success)) {
		fprintf(stderr, "error: communication error with ProcD\n");
		return 1;
	}
	if (!success) {
		fprintf(stderr,
		        "error: %s command failed with ProcD\n",
		        argv[0]);
		return 1;
	}
	return 0;
}

int
continue_family(ProcFamilyClient& pfc, int argc, char* argv[])
{
	if (argc > 2) {
		fprintf(stderr,
		        "error: argument synopsis for %s: [<pid>]\n",
		        argv[0]);
		return 1;
	}
	pid_t pid = 0;
	if (argc == 2) {
		pid = atoi(argv[1]);
		if (pid == 0) {
			fprintf(stderr, "error: invalid pid: %s\n", argv[1]);
			return 1;
		}
	}
	bool success;
	if (!pfc.continue_family(pid, success)) {
		fprintf(stderr, "error: communication error with ProcD\n");
		return 1;
	}
	if (!success) {
		fprintf(stderr,
		        "error: %s command failed with ProcD\n",
		        argv[0]);
		return 1;
	}
	return 0;
}

int
kill_family(ProcFamilyClient& pfc, int argc, char* argv[])
{
	if (argc > 2) {
		fprintf(stderr,
		        "error: argument synopsis for %s: [<pid>]\n",
		        argv[0]);
		return 1;
	}
	pid_t pid = 0;
	if (argc == 2) {
		pid = atoi(argv[1]);
		if (pid == 0) {
			fprintf(stderr, "error: invalid pid: %s\n", argv[1]);
			return 1;
		}
	}
	bool success;
	if (!pfc.kill_family(pid, success)) {
		fprintf(stderr, "error: communication error with ProcD\n");
		return 1;
	}
	if (!success) {
		fprintf(stderr,
		        "error: %s command failed with ProcD\n",
		        argv[0]);
		return 1;
	}
	return 0;
}

int
unregister_family(ProcFamilyClient& pfc, int argc, char* argv[])
{
	if (argc != 2) {
		fprintf(stderr,
		        "error: argument synopsis for %s: <pid>\n",
		        argv[0]);
		return 1;
	}
	pid_t pid = atoi(argv[1]);
	bool success;
	if (!pfc.unregister_family(pid, success)) {
		fprintf(stderr, "error: communication error with ProcD\n");
		return 1;
	}
	if (!success) {
		fprintf(stderr,
		        "error: %s command failed with ProcD\n",
		        argv[0]);
		return 1;
	}
	return 0;
}

int
snapshot(ProcFamilyClient& pfc, int argc, char* argv[])
{
	if (argc != 1) {
		fprintf(stderr,
		        "error: no arguments required for %s\n",
		        argv[0]);
		return 1;
	}
	bool success;
	if (!pfc.snapshot(success)) {
		fprintf(stderr, "error: communication error with ProcD\n");
		return 1;
	}
	if (!success) {
		fprintf(stderr,
		        "error: %s command failed with ProcD\n",
		        argv[0]);
		return 1;
	}
	return 0;
}

int
quit(ProcFamilyClient& pfc, int argc, char* argv[])
{
	if (argc != 1) {
		fprintf(stderr,
		        "error: no arguments required for %s\n",
		        argv[0]);
		return 1;
	}
	bool success;
	if (!pfc.quit(success)) {
		fprintf(stderr, "error: communication error with ProcD\n");
		return 1;
	}
	if (!success) {
		fprintf(stderr,
		        "error: %s command failed with ProcD\n",
		        argv[0]);
		return 1;
	}
	return 0;
}
