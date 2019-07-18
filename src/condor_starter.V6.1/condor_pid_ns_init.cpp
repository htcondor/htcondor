/***************************************************************
 *
 * Copyright (C) 1990-2013, Condor Team, Computer Sciences Department,
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


#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>


// When running with PID namespaces enabled (via the
// USE_PID_NAMESPACES = true 
// flag, the process that the starter forks (usually the job) runs
// with pid 1, at least to processes within the namespace.  PID 1
// has special responsibilities.  First, all signals sent to it
// (except SIGKILL, SIGSTOP and SIGCONT) are blocked if there is
// no handler installed.  So, if we just ran the job as pid 1, 
// the softkill signal (SIGQUIT) isn't delivered, and after a condor_rm
// the job continues runnning until a hardkill.
//
// Also, the pid 1 acts like the real init wrt reaping orphan processes.
// If a child of this process forks a grandchild, and the child dies, the
// grandchild is reparented to this pid 1, not the ur-pid 1 launched by
// the kernel at boot time.  Thus we need to be careful that we must
// continue to reap processes that may not be direct children of this
// process.  If we don't, they will get re-parented up to the real init
// upon the death of this processes, but I think we lose resource
// information from when the starter calls getrusage on this process.

// To fix these problems, instead of running the job as pid 1, we run
// this simple init like process instead, which turns around and forks
// and execs the job proper.  It registers signal handlers, so that the
// starter can softkill and send other signals to it.  These signals are generally intended for the job, so they are forwarded to the job itself.  That way
// if the job catches SIGQUIT, it can itself perform a graceful shutdown.
//
// If the job itself exits with a signal, we need a way to communicate that back
// up to the starter.  This is done via a file, which is written by this
// process after the job exits.  The starter parses this file to get the
// real exit status and whether the job exitted via a signal.

pid_t child_pid = -1;

// If we receive a signal from the outside world, it was probably intended for our
// child.  Pass it down to the child.
void signal_handler(int signo) {
	if (child_pid > 0) {
		kill(child_pid, signo);
	}
}

void write_status(const char *message, int status) {
	const char *filename = getenv("_CONDOR_PID_NS_INIT_STATUS_FILENAME");
	if (filename) {
		FILE *f = fopen(filename, "w");
		if (f) {
			fprintf(f, "%s: %d\n", message, status);
			fclose(f);
		}
	} 
}

int main(int argc, char **argv, char **envp) {
	
	struct sigaction sa;
	sa.sa_handler = signal_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;

	// Remove the status file, if it exists
	const char *status_filename = getenv("_CONDOR_PID_NS_INIT_STATUS_FILENAME");
	if (status_filename) {
		unlink(status_filename);
	}
	
	// Install the signal handlers, so if we're running as pid 1 they will
	// get delivered.

	sigaction(SIGHUP, &sa, 0);
	sigaction(SIGINT, &sa, 0);
	sigaction(SIGQUIT, &sa, 0);
	sigaction(SIGILL, &sa, 0);
	sigaction(SIGABRT, &sa, 0);
	sigaction(SIGTERM, &sa, 0);
	sigaction(SIGSEGV, &sa, 0);
	sigaction(SIGPIPE, &sa, 0);
	sigaction(SIGUSR1, &sa, 0);
	sigaction(SIGUSR2, &sa, 0);

	
	child_pid = fork();

	if (child_pid == -1) {
		/// fork failed
		// Write nothing to log file, starter knows this means something failed
		// with this tool itself.  There's nothing wrong with the job.
		exit(-1);
	}

	if (child_pid == 0) {
		// in the child
		int i = 0;
		for (i = 0; i < argc - 1; i++) {
			argv[i] = argv[i + 1];
		}
		argv[i] = 0;
		argc--;
		execve(argv[0], argv, envp);

		// We're in trouble if we get here... exec failed
		// maybe bad path, maybe bad interpreter...
		// Must let starter and shadow know that something is wrong with this
		// job, not the machine.
		write_status("ExecFailed", errno);
		exit(0);
	}

	if (child_pid > 0) {
		// in the parent, just wait to see what happens...
		int status;

retry:
		int result = wait(&status);
		if (((result == -1) && (errno == EINTR)) || (result != child_pid)) {
			goto retry; // Signal must have been handled
		}

		// Child has exited.  First see if we failed to exec
		struct stat buf;
		const char *filename = getenv("_CONDOR_PID_NS_INIT_STATUS_FILENAME");
		if (filename) {
			result = stat(filename, &buf);
			if (result != 0) {
				write_status("Exited", status);
			}
		}
		exit(WEXITSTATUS(status));
	}
}
