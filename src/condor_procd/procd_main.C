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
#include "condor_debug.h"
#include "proc_family_monitor.h"
#include "proc_family_server.h"
#include "proc_family_io.h"

#if defined(WIN32)
#include "process_control.WINDOWS.h"
#endif

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

// info about the root process of the family we'll be monitoring
// (set with the two-argument "-P" option)
//
static pid_t root_pid = 0;
static birthday_t root_birthday = 0;

// the maximum number of seconds we'll wait in between
// taking snapshots (one minute by default)
// (set with the "-S" option)
//
static int max_snapshot_interval = 60;

#if defined(WIN32)
// on Windows, we use an external program (condor_softkill.exe)
// to send soft kills to jobs. the path to this program is passed
// via the -K option
//
static char* windows_softkill_binary = NULL;
#endif

static inline void
fail_illegal_option(char* option)
{
	fprintf(stderr,
	        "error: illegal option: %s",
	        option);
	exit(1);
}

static inline void
fail_option_args(char* option, int args_required)
{
	fprintf(stderr,
	        "error: option \"%s\" requires %d arguments",
	        option,
	        args_required);
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

			// root process information
			//
			case 'P':
				if (index + 2 >= argc) {
					fail_option_args("-P", 2);
				}
				index++;
				root_pid = atoi(argv[index]);
				if (root_pid == 0) {
					fprintf(stderr,
					        "error: invalid root process id: %s",
					        argv[index]);
					exit(1);
				}
				index++;
				root_birthday = procd_atob(argv[index]);
				if (root_birthday == 0) {
					fprintf(stderr,
					        "error: invalid root process birthday: %s",
					        argv[index]);
					exit(1);
				}
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
				break;
		}

		index++;
	}

	// now that we're done parsing, enforce required options
	//
	if (local_server_address == NULL) {
		fprintf(stderr, "error: the \"-A\" option is required");
		exit(1);
	}
	if (root_pid == 0 || root_birthday == 0) {
		fprintf(stderr, "error: the \"-P\" option is required");
		exit(1);
	}
}

int
main(int argc, char* argv[])
{
	// close stdin and stdout right away, since we don't use them
	//
	fclose(stdin);
	fclose(stdout);

	// this modifies our static configuration variables based on
	// our command line parameters
	//
	parse_command_line(argc, argv);

	// setup logging if a file was given
	//
	extern FILE* debug_fp;
	if (log_file_name != NULL) {
		debug_fp = safe_fopen_wrapper(log_file_name, "w");
		if (debug_fp == NULL) {
			fprintf(stderr,
			        "error: couldn't open file \"%s\" for logging: %s (%d)",
					log_file_name,
			        strerror(errno),
			        errno);
			exit(1);
		}
	}

	// if a maximum snapshot interval was given, it needs to be either
	// a non-negative number, or -1 for "infinite"
	//
	if (max_snapshot_interval < -1) {
		EXCEPT("error: maximum snapshot interval must be non-negative or -1");
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
	//
	ProcFamilyMonitor monitor(root_pid, root_birthday, max_snapshot_interval);

	// initialize the server for accepting requests from clients. if a local
	// client principal was given, tell the server to accept connections from
	// this principal
	//
	ProcFamilyServer server(monitor, local_server_address);
	if (local_client_principal != NULL) {
		server.set_client_principal(local_client_principal);
	}

	// now that we've initialized the server, close out standard error.
	// this way, calling programs can set up a pipe to block on until
	// we're accepting connections
	//
	fclose(stderr);

	// finally, enter the server's wait loop
	//
	server.wait_loop();

	// cleanup before exiting
	//
	if (debug_fp != NULL) {
		fclose(debug_fp);
	}

	return 0;
}
