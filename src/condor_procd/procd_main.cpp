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
#include "condor_debug.h"
#include "proc_family_monitor.h"
#include "proc_family_server.h"
#include "proc_family_io.h"

#if defined(WIN32)
#include "process_control.WINDOWS.h"
#endif

#if !defined(WIN32)
#include <syslog.h>
#endif

extern int log_size;

// our "local server address"
// (set with the "-A" option)
//
static char* local_server_address = NULL;

// the client prinical who will be allowed to connect to us
// (if not given, only root/SYSTEM will be allowed access).
// this string will be a SID on Windows and a UID on UNIX
//
static char* local_client_principal = NULL;

// log file (no logging by default)
// (set with the "-L" option)
//
static char* log_file_name = NULL;

// the maximum number of seconds we'll wait in between
// taking snapshots (one minute by default)
// (set with the "-S" option)
//
static int max_snapshot_interval = 60;

#if defined(LINUX)
// a range of group IDs that can be used to track process
// families by placing them in their supplementary group
// lists
//
static gid_t min_tracking_gid = 0;
static gid_t max_tracking_gid = 0;
#endif

#if defined(WIN32)
// on Windows, we use an external program (condor_softkill.exe)
// to send soft kills to jobs. the path to this program is passed
// via the -K option
//
static char* windows_softkill_binary = NULL;
#endif

// Determines if the procd should assign GIDs to family groups internally using
// the range denoted by -G, or if there will be an external method like
// gidd_alloc (which is used by OSG) in which case the associated gid must
// exist within the rang denoted by -G. This is set by the -E command line
// argument. If -E is present then this is true.
static bool use_external_gid_association = false;

// Controls the root pid of the tree that the procd is monotoring.  If not set,
// it defaults to the procd's parent. Set by -P on the command line.
static pid_t root_pid = 0;
// This is set to true if -P was seen on the command line.
static bool root_pid_specified = false;

static void
usage(void)
{
	fprintf(stderr, "Usage:\n"
	"  -h                     This usage message.\n"
	"  -D                     Wait for debugger.\n"
	"  -A <address-file>      Path to address file for client communication.\n"
	"  -C <principal>         The UID owner of the address file.\n"
	"  -L <log-file>          Path to the output logfile.\n"
	"  -E                     When specified with -G, the procd_ctl tool\n"
	"                         can be used to associate a gid with the pid\n"
	"                         family.\n"
	"  -P <pid>               If specified, then the procd will manage the\n"
	"                         process family rooted at this pid. If this pid\n"
	"                         dies the procd will continue running.  If not\n"
	"                         specified, then the procd's parent, which must\n"
	"                         not be process 1 on unix, is monitored. If\n"
	"                         the parent process dies, the condor_procd will\n"
	"                         exit.\n"
	"  -S <seconds>           Process snapshot interval.\n"
	"  -G <min-gid> <max-gid> If -E is not specified, then self-allocate gids\n"
	"                         out of this range for process family tracking.\n"
	"                         If -E is specified then procd_ctl must be used\n"
	"                         to allocate gids which must then be in this\n"
	"                         range.\n");
}

static inline void
fail_illegal_option(char* option)
{
	fprintf(stderr,
	        "error: illegal option: %s\n",
	        option);
	usage();
	exit(1);
}

static inline void
fail_option_args(const char* option, int args_required)
{
	fprintf(stderr,
	        "error: option \"%s\" requires %d %s\n",
	        option,
	        args_required,
			args_required==1?"argument":"arguments");
	usage();
	exit(1);
}


static void
parse_command_line(int argc, char* argv[])
{
	int index = 1;
	while (index < argc) {

		// first, make sure the first char of the option is '-'
		// and that there is at least one more char after that
		//
		if (argv[index][0] != '-' || argv[index][1] == '\0') {
			fail_illegal_option(argv[index]);
		}

		// now switch on the option
		//
		switch(argv[index][1]) {

			// DEBUG: stop ourselves so a debugger can
			// attach if "-D" is given
			//
			case 'D':
				sleep(30);
				break;

			// local server address
			//
			case 'A':
				if (index + 1 >= argc) {
					fail_option_args("-A", 1);
				}
				index++;
				local_server_address = argv[index];
				break;

			// local client principal
			//
			case 'C':
				if (index + 1 >= argc) {
					fail_option_args("-C", 1);
				}
				index++;
				local_client_principal = argv[index];
				break;

			// log file name
			//
			case 'L':
				if (index + 1 >= argc) {
					fail_option_args("-L", 1);
				}
				index++;
				log_file_name = argv[index];
				break;
			
			// log file size till rotation
			//
			case 'R':
				if (index + 1 >= argc) {
					fail_option_args("-R", 1);
				}
				index++;
				log_size = atoi(argv[index]);
				// set a 64k floor on the log size, to avoid excessive rotation
				if (log_size > 0) { log_size = MAX(log_size, 65536); }
				break;
				
			// maximum snapshot interval
			//
			case 'S':
				if (index + 1 >= argc) {
					fail_option_args("-S", 1);
				}
				index++;
				max_snapshot_interval = atoi(argv[index]);
				break;
			
			// Should the procd utilize an externel protocol for assigning
			// gids to families, or an internal protocol where the procd
			// figures it out itself?
			case 'E':
				use_external_gid_association = true;
				break;

			// If this isn't specified, use an algorithm to find the parent
			// pid of the procd and track that. Otherwise, this is the pid
			// of the family the procd is tracking.
			case 'P':
				if (index + 1 >= argc) {
					fail_option_args("-P", 1);
				}
				index++;
				root_pid = atoi(argv[index]);
				if (root_pid == 0) {
					EXCEPT("The procd can not track pid 0.");
				}
#if !defined(WIN32)
				// We can't track init since that escalates certain
				// privileges in certain situations.
				if (root_pid == 1) {
					EXCEPT("The procd will not track pid 1 as the family.");
				}
#endif
				root_pid_specified = true;
				break;

#if defined(LINUX)
			// tracking group ID range
			//
			case 'G':
				if (index + 2 >= argc) {
					fail_option_args("-G", 2);
				}
				index++;
				min_tracking_gid = (gid_t)atoi(argv[index]);
				index++;
				max_tracking_gid = (gid_t)atoi(argv[index]);
				break;
#endif

#if defined(WIN32)
			// windows condor_softkill.exe binary path
			//
			case 'K':
				if (index + 1 >= argc) {
					fail_option_args("-K", 1);
				}
				index++;
				windows_softkill_binary = argv[index];
				break;
#endif
			// default case
			//
			default:
				fail_illegal_option(argv[index]);
				usage();
				exit(EXIT_FAILURE);
				break;
		}

		index++;
	}

	// now that we're done parsing, enforce required options
	//
	if (local_server_address == NULL) {
		fprintf(stderr, "error: the \"-A\" option is required\n");
		usage();
		exit(EXIT_FAILURE);
	}
}

static void
get_parent_info(pid_t& parent_pid, birthday_t& parent_birthday)
{
	procInfo* own_pi = NULL;
	procInfo* parent_pi = NULL;

	int ignored;
	if (ProcAPI::getProcInfo(getpid(), own_pi, ignored) != PROCAPI_SUCCESS) {
		fprintf(stderr, "error: getProcInfo failed on own PID\n");
		exit(1);
	}
	if (ProcAPI::getProcInfo(own_pi->ppid, parent_pi, ignored) != PROCAPI_SUCCESS) {
		fprintf(stderr, "error: getProcInfo failed on parent PID\n");
		exit(1);
	}

	parent_pid = parent_pi->pid;
	parent_birthday = parent_pi->birthday;

	delete own_pi;
	delete parent_pi;
}

int
main(int argc, char* argv[])
{
	// close stdin and stdout right away, since we don't use them
	//
	if (freopen(NULL_FILE, "r", stdin) == NULL ||
		freopen(NULL_FILE, "w", stdout) == NULL) {
		fprintf(stderr, "Failed to reopen stdin and stdout, errno=%d (%s)\n",
				errno, strerror(errno));
		exit(1);
	}

	// this modifies our static configuration variables based on
	// our command line parameters
	//
	parse_command_line(argc, argv);

	// Determine who is the root of the process tree we should monitor.
	// Either it will be the parent of the procd, or whatever was told to us
	// on the command line with -P.
	birthday_t root_birthday;
	if (root_pid_specified == false) { 
		get_parent_info(root_pid, root_birthday);
	} else {
		procInfo* pi = NULL;
		int ignored;
		int status = ProcAPI::getProcInfo(root_pid, pi, ignored);
		if (status != PROCAPI_SUCCESS) {
				if (pi != NULL) {
					delete pi;
				}
				fprintf(stderr,
					"getProcInfo failed on root PID %u\n",
					 (unsigned)root_pid);
				EXCEPT("getProcInfo failed on root PID %u",
				(unsigned)root_pid);
		}
		root_birthday = pi->birthday;
		delete pi;
	}

	// setup logging if a file was given
	//
	extern FILE* debug_fp;
	extern char* debug_fn;

	if (log_file_name && !strcmp(log_file_name, "SYSLOG")) {
#if !defined(WIN32)
		openlog(NULL, LOG_NOWAIT|LOG_PID, LOG_DAEMON);
		debug_fn = log_file_name;
#endif
	} else if (log_file_name != NULL) {
		debug_fn = log_file_name;
		debug_fp = safe_fopen_wrapper_follow(log_file_name, "a");
		if (debug_fp == NULL) {
			fprintf(stderr,
			        "error: couldn't open file \"%s\" for logging: %s (%d)\n",
					log_file_name,
			        strerror(errno),
			        errno);
			exit(1);
		}
		dprintf(D_ALWAYS, "***********************************\n");
		dprintf(D_ALWAYS, "* condor_procd STARTING UP\n");
#if defined(WIN32)
		dprintf(D_ALWAYS, "* PID = %lu\n", 
			(unsigned long)::GetCurrentProcessId());
#if defined _M_X64
		dprintf(D_ALWAYS, "* ARCH = x64\n");
#endif
#else
		dprintf(D_ALWAYS, "* PID = %lu\n", (unsigned long)getpid());
		dprintf(D_ALWAYS, "* UID = %lu\n", (unsigned long)getuid());
		dprintf(D_ALWAYS, "* GID = %lu\n", (unsigned long)getgid());
#endif
		dprintf(D_ALWAYS, "***********************************\n");
	}

	// if a maximum snapshot interval was given, it needs to be either
	// a non-negative number, or -1 for "infinite"
	//
	if (max_snapshot_interval < -1) {
		fprintf(stderr,
		        "error: maximum snapshot interval must be non-negative or -1\n");
		exit(1);
	}

#if defined(WIN32)
	// on Windows, we need to tell our "process control" module what binary
	// to use for sending WM_CLOSE messages
	//
	if (windows_softkill_binary != NULL) {
		set_windows_soft_kill_binary(windows_softkill_binary);
	}
#endif

	// initialize the "engine" for tracking process families
	// If we specified a root pid, that means we don't want to except if it
	// dies. If we didn't specify a root pid, it means the procd's parent
	// is the watcher pid, and if it (the master 99.99% of the time) dies, we
	// also want to go away.
	//
	if (root_pid_specified == true) {
		dprintf(D_ALWAYS, 
			"Procd has no watcher pid and will not die if pid %lu dies.\n",
			(unsigned long) root_pid);
	} else {
		dprintf(D_ALWAYS, 
			"Procd has a watcher pid and will die if pid %lu dies.\n",
			(unsigned long) root_pid);
	}

	ProcFamilyMonitor monitor(root_pid, 
								root_birthday, 
								max_snapshot_interval,
								root_pid_specified ? false : true);

#if defined(LINUX)
	// if a "-G" option was given, enable group ID tracking in the
	// monitor
	//
	if (min_tracking_gid != 0) {
		if (min_tracking_gid > max_tracking_gid) {
			fprintf(stderr,
			        "invalid group range given: %u - %u\n",
			        min_tracking_gid,
			        max_tracking_gid);
			exit(1);
		}

		// If we want to use an external gid association mechanism, then
		// presumably the gidd_alloc tool and the procd_ctl tool will
		// be used to find an open gid and associate it with a procfamily.
		// Otherwise, we do self-allocation of the gids out of the specified
		// mix/max range of gids.
		monitor.enable_group_tracking(min_tracking_gid, 
			max_tracking_gid,
			use_external_gid_association ? false : true);
	}
#endif

#if defined(HAVE_EXT_LIBCGROUP)
	monitor.enable_cgroup_tracking();
#endif

	// initialize the server for accepting requests from clients
	//
	ProcFamilyServer server(monitor, local_server_address);

	// specify the client that we'll be accepting connections from. note
	// that passing NULL may have special meaning here: for example on
	// UNIX we'll check to see if we were invoked as a setuid root program
	// and if so use our real UID as the client principal
	//
	server.set_client_principal(local_client_principal);

	// now that we've initialized the server, close out standard error.
	// this way, calling programs can set up a pipe to block on until
	// we're accepting connections
	//
	if ( freopen(NULL_FILE, "w", stderr) == NULL ) {
		fprintf( stderr, "Failed to reopen stderr, errno=%d (%s)\n",
				 errno, strerror(errno) );
		exit( 1 );
	}

	// finally, enter the server's wait loop
	//
	server.wait_loop();

	return 0;
}
