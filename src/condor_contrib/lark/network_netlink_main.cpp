
#include "condor_common.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "condor_uid.h"

//#define _GNU_SOURCE
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <signal.h>
#include <linux/if.h>

#include "network_manipulation.h"
#include "network_namespaces.h"

int handle_match(const unsigned char * rule_name, long long bytes_matched, void * unused) {
        if (unused) {}
        printf("Network%s: %lld\n", rule_name, bytes_matched);
	return 0;
}

static int child_post_fork(NetworkNamespaceManager &manager) {

	int rc = 0;
	if (manager.PostForkChild())
	{
		fprintf(stderr, "Post-fork child process failed.\n");
		rc = 4;
		goto finalize_child;
	}

	// Exec out.
	rc = execl("/bin/sh", "sh", "-c", "date; ifconfig -a; route -n; ping 129.93.1.141 -c 10 && curl 129.93.1.141;", (char *)0);
	fprintf(stderr, "Failure to exec /bin/sh. (errno=%d) %s\n", errno, strerror(errno));

finalize_child:
	_exit(rc);
}

int main(int argc, char * argv[])
{
	config();
	dprintf_set_tool_debug("TOOL", 0);
	dprintf(D_FULLDEBUG, "Running network namespace tester in debug mode.\n");

	// Suppress unused param compiler warnings:
	if (argc) {}
	if (argv) {}
	int rc = 0;
	int child_status = 0;
	pid_t fork_pid;

	classad::ClassAd job_ad, machine_ad;

	// Defaults for create / delete scripts.
	// Only done for the test case.
	std::string create_script;
	if (!param(create_script, "NETWORK_NAMESPACE_CREATE_SCRIPT"))
		param_insert("NETWORK_NAMESPACE_CREATE_SCRIPT", "./nat_script.sh");
	if (!param(create_script, "NETWORK_NAMESPACE_DELETE_SCRIPT"))
		param_insert("NETWORK_NAMESPACE_DELETE_SCRIPT", "./nat_delete_script.sh");


	TemporaryPrivSentry sentry(PRIV_ROOT);

	NetworkNamespaceManager manager;

	if (manager.PrepareNetwork("tester", job_ad, machine_ad))
	{
		rc = 1;
		fprintf(stderr, "Failed to prepare network!\n");
		goto finalize;
	}

	if (manager.PreFork())
	{
		rc = 2;
		fprintf(stderr, "Failed to prepare for fork.");
		goto finalize;
	}

	fork_pid = fork();
	if (fork_pid == -1)
	{
		rc = 3;
		fprintf(stderr, "Failed to create a new process. (errno=%d, %s)\n", errno, strerror(errno));
		goto finalize;
	}
	else if (fork_pid == 0)
	{
		return child_post_fork(manager);
	}

	if (manager.PostForkParent(fork_pid))
	{
		fprintf(stderr, "Failure post-fork in parent.\n");
		rc = 4;
		goto finalize;
	}	

	// Wait for the user to exit the child.
	if (waitpid(fork_pid, &child_status, 0) == -1) {
		fprintf(stderr, "Unable to get child %d status. (errno=%d) %s\n", fork_pid, errno, strerror(errno));
		rc = errno;
		goto finalize;
	}
	if (WIFEXITED(child_status)) {
		if ((rc = WEXITSTATUS(child_status))) {
			fprintf(stderr, "Child returned non-zero status %d.\n", rc);
			goto finalize;
		}
		//fprintf(stderr, "Child finished (status=%d)\n", child_status);
	} else {
		fprintf(stderr, "Unable to understand child exit status %d.\n", child_status);
		goto finalize;
	}

	perform_accounting("tester", handle_match, NULL);

finalize:
	if (manager.Cleanup("tester"))
	{
		fprintf(stderr, "Failure when cleaning up test environment.\n");
	}
	else
	{
		fprintf(stderr, "Successfully cleaned up test environment.\n");
	}
	return rc;
}
