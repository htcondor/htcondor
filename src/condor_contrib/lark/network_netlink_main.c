
#define _GNU_SOURCE
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sched.h>
#include <signal.h>
#include <linux/sched.h>
#include <linux/if.h>

#include "network_manipulation.h"

int handle_match(const unsigned char * rule_name, long long bytes_matched, void * unused) {
        if (unused) {}
        printf("Network%s: %lld\n", rule_name, bytes_matched);
	return 0;
}

static int setup_pipes(int *p2c, int *c2p) {
	
	if (pipe(p2c)) {
		fprintf(stderr, "Unable to create the parent-to-child communication pipe. (errno=%d) %s\n", errno, strerror(errno));
		return errno;
	}
	if (pipe(c2p)) {
		fprintf(stderr, "Unable to create the child-to-parent communication pipe. (errno=%d) %s\n", errno, strerror(errno));
		return errno;
	}

	// The child side of both pipes should be set to F_CLOEXEC
	int fd_flags;
	if ((fd_flags = fcntl(p2c[0], F_GETFD, NULL)) == -1) {
		fprintf(stderr, "Failed to get fd flags: (errno=%d) %s\n", errno, strerror(errno));
		return errno;
	}
	if (fcntl(p2c[0], F_SETFD, fd_flags | FD_CLOEXEC) == -1) {
		fprintf(stderr, "Failed to set new fd flags: (errno=%d) %s\n", errno, strerror(errno));
		return errno;
	}
	if ((fd_flags = fcntl(c2p[1], F_GETFD, NULL)) == -1) {
		fprintf(stderr, "Failed to get fd flags: (errno=%d) %s\n", errno, strerror(errno));
		return errno;
	}
	if (fcntl(c2p[1], F_SETFD, fd_flags | FD_CLOEXEC) == -1) {
		fprintf(stderr, "Failed to set new fd flags: (errno=%d) %s\n", errno, strerror(errno));
		return errno;
	}

	return 0;
}

#define SHELL_MAX 512
#define ADDR_MAX 32
static char * create_outer_routing(const char * script, const char * jobid, const char * eth) {
	FILE * fp;
	char * addr = NULL;
	int rc = 0;
	int child_status = 0;

	char script_shell[SHELL_MAX];
	if (snprintf(script_shell, SHELL_MAX, "%s %s %s", script, jobid, eth) >= SHELL_MAX) {
		fprintf(stderr, "Too-long arguments for the routing script: %s %s %s\n", script, jobid, eth);
		return NULL;
	}

	// Execute the script to setup the NAT
	if (!(fp = popen(script_shell, "r"))) {
		fprintf(stderr, "Unable to execute the NAT script. (errno=%d) %s\n", errno, strerror(errno));
		goto finalize;
	}
	addr = (char *)malloc(ADDR_MAX);
	if (!addr) goto finalize;
	if (fgets(addr, ADDR_MAX, fp) == NULL) {
		fprintf(stderr, "Did not read an address from the NAT script.\n");
		rc = 1;
		goto finalize;
	}
	if ((child_status = pclose(fp)) == -1) {
		fprintf(stderr, "Unable to retrieve NAT script status. (errno=%d) %s\n", errno, strerror(errno));
		fp = NULL;
		rc = 1;
		goto finalize;
	}
	fp = NULL;
	if (WIFEXITED(child_status)) {
		if ((rc = WEXITSTATUS(child_status))) {
			fprintf(stderr, "NAT script returned non-zero status %d.\n", rc);
			rc = 1;
			goto finalize;
		}
	} else {
		fprintf(stderr, "Unable to understand child exit status %d.\n", child_status);
		rc = 1;
		goto finalize;
	}
	// inet_pton doesn't like newlines in the address.
	char *first_newline = strchr(addr, '\n');
	if (first_newline) {
		*first_newline = '\0';
	}

finalize:
	if (fp) {
		pclose(fp);
	}
	if (rc && addr) {
		free(addr);
		addr = NULL;
	}
	return addr;
}

static int delete_outer_routing(const char * script, const char * jobid, const char * eth) {
        FILE * fp;
	int child_status = 0;
	int rc = 0;

	char script_shell[SHELL_MAX];
	if (snprintf(script_shell, SHELL_MAX, "%s %s %s", script, jobid, eth) >= SHELL_MAX) {
		fprintf(stderr, "Too-long arguments for the routing script: %s %s %s\n", script, jobid, eth);
		return 1;
	}

	// Execute the script to setup the NAT
	if (!(fp = popen(script_shell, "r"))) {
		fprintf(stderr, "Unable to execute the NAT delete script. (errno=%d) %s\n", errno, strerror(errno));
		rc = errno;
		goto finalize;
	}
	if ((child_status = pclose(fp)) == -1) {
		fprintf(stderr, "Unable to retrieve NAT delete script status. (errno=%d) %s\n", errno, strerror(errno));
		fp = NULL;
		rc = errno;
		goto finalize;
	}
	fp = NULL;
	if (WIFEXITED(child_status)) {
		if ((rc = WEXITSTATUS(child_status))) {
			fprintf(stderr, "NAT delete script returned non-zero status %d.\n", rc);
			goto finalize;
		}
	} else {
		fprintf(stderr, "Unable to understand child exit status %d.\n", child_status);
		rc = 1;
		goto finalize;
	}

finalize:
	if (fp) {
		pclose(fp);
	}
	return rc;
}

struct child_info {
	int p2c[2];
	int c2p[2];
	const char * addr;
	const char * eth;
};

static int child_post_fork(void * info_ptr) {

	struct child_info *info = (struct child_info *)info_ptr;
	close(info->p2c[1]);
	close(info->c2p[0]);

	int rc = 0, sock = -1;
	// TODO: synchronize with parent, wait until it is finished with the socket.
	// Must know we are in our own NETNS first, otherwise there is no veth device!
	int parent_status;
	if (read(info->p2c[0], &parent_status, sizeof(int)) != sizeof(int)) { // TODO make EINTR safe
		fprintf(stderr, "Unable to read status from parent - probable parent failure.\n");
		rc = 1;
		goto finalize_child;
	}
	if (parent_status != 0) {
		fprintf(stderr, "Parent failed to setup the namespace.\n");
		rc = 1;
	}

	// Remount /sys and /proc
	if (mount("proc", "/proc", "proc", 0, 0) == -1) {
		rc = errno;
		fprintf(stderr, "Unable to remount /proc. (errno=%d) %s\n", errno, strerror(errno));
		goto finalize_child;
	}

	if (mount("sysfs", "/sys", "sysfs", 0, 0) == -1) {
		rc = errno;
		fprintf(stderr, "Unable to remount /sys. (errno=%d) %s\n", errno, strerror(errno));
		goto finalize_child;
	}

	// Manipulate our network configuration.
	// Notice that we open a new socket to the kernel - this is because the
	// other socket still talks to the original namespace.
	if ((sock = create_socket()) < 0) {
		fprintf(stderr, "Unable to create socket to talk to kernel for child.\n");
		rc = 1;
		goto finalize_child;
	}
	if (add_address(sock, info->addr, info->eth)) {
		fprintf(stderr, "Unable to add address %s to %s.\n", info->addr, info->eth);
		rc = 1;
		goto finalize_child;
	}
	if (set_status(sock, info->eth, IFF_UP)) {
		fprintf(stderr, "Unable to bring up interface %s.\n", info->eth);
		rc = 3;
		goto finalize_child;
	}
	if (add_local_route(sock, info->addr, info->eth, 24)) {
		fprintf(stderr, "Unable to add local route via %s\n", info->addr);
		rc = 4;
		goto finalize_child;
	}
	if (add_default_route(sock, info->addr)) {
		fprintf(stderr, "Unable to add default route via %s\n", info->addr);
		rc = 2;
		goto finalize_child;
	}
	close(sock);
	sock = -1;

	// Exec out.
	rc = execl("/bin/sh", "sh", "-c", "date; ifconfig -a; route -n; curl 129.93.1.141;", (char *)0);
	fprintf(stderr, "Failure to exec /bin/sh. (errno=%d) %s\n", errno, strerror(errno));

finalize_child:
	if (sock >= 0) {
		close(sock);
	}
	if (write(info->c2p[1], &rc, sizeof(int)) < 0) {
		fprintf(stderr, "Error writing result: %s (errno=%d)\n", strerror(errno), errno);
	}
	close(info->c2p[0]);
	close(info->p2c[0]);
	_exit(rc);
}

#define NUMBER_PAGES 16
int main(int argc, char * argv[]) {

	// Suppress unused param compiler warnings:
	if (argc) {}
	if (argv) {}

	int rc = 0, rc2;
	int sock = -1;
	int created = 0, routed = 0;
	int p2c[2] = {-1, -1};
	int c2p[2] = {-1, -1};
	int child_status = -1;
	char *addr = NULL;
	char * child_stack_ptr, *child_stack = NULL;

	const char * veth0 = "v_external";
	const char * veth1 = "v_internal";

	child_stack = (char *)malloc(NUMBER_PAGES*getpagesize());
	if (!child_stack) {
		fprintf(stderr, "Unable to prepare child stack.\n");
		return 1;
	}
	child_stack_ptr = child_stack + NUMBER_PAGES*getpagesize();

	if (setup_pipes(p2c, c2p)) {
		fprintf(stderr, "Unable to setup synchronization pipes.\n");
		rc = 1;
		goto finalize;
	}

	if ((sock = create_socket()) < 0) {
		fprintf(stderr, "Unable to create socket to talk to kernel.\n");
		rc = 1;
		goto finalize;
	}

	if ((rc = create_veth(sock, veth0, veth1))) {
		fprintf(stderr, "Failed to create veth devices %s/%s.\n", veth0, veth1);
		goto finalize;
	}
	created = 1;

	//printf("Created a new veth device.\n");
	routed = 1;
	if (!(addr = create_outer_routing("./nat_script.sh", "JOBID", veth0))) {
		fprintf(stderr, "Failed to create the routing.\n");
		goto finalize;
	}

	// fork/exec the child process (actually, clone/exec so we can manipulate the namespaces)
	struct child_info info = {
		.addr = addr,
		.eth = veth1,
	};
	info.p2c[0] = p2c[0]; info.p2c[1] = p2c[1];
	info.c2p[0] = c2p[0]; info.c2p[1] = c2p[1];
	// Note: only returns for the parent, not the child.
	pid_t fork_pid = clone(child_post_fork, (void *)child_stack_ptr,
		//CLONE_NEWNS|CLONE_NEWNET|CLONE_NEWPID|CLONE_VM|SIGCHLD,
		CLONE_NEWNS|CLONE_NEWNET|CLONE_NEWPID|SIGCHLD,
		(void *)&info);
	if (fork_pid == -1) {
		rc = 3;
		fprintf(stderr, "Failed to create a new process. (errno=%d) %s\n", errno, strerror(errno));
		goto finalize;
	}
	//fprintf(stderr, "Child PID %d\n", fork_pid);

	// Close out the child end of the pipes.
	close(p2c[0]); p2c[0] = -1;
	close(c2p[1]); c2p[1] = -1;

	if ((rc = set_netns(sock, veth1, fork_pid))) {
		fprintf(stderr, "Failed to set ns\n");
		if (write(p2c[1], &rc, sizeof(int)) < 0) {
			fprintf(stderr, "Failed to write exitcode.\n");
			rc = errno;
		}
		goto finalize;
	}
	if (write(p2c[1], &rc, sizeof(int)) < 0) {
		fprintf(stderr, "Failed to write exit.\n");
		rc = errno;
		goto finalize;
	}
	close(p2c[1]); p2c[1] = -1;

	if ((rc = read(c2p[0], &child_status, sizeof(int))) == sizeof(int)) {
		if (child_status) {
			fprintf(stderr, "Child failed to launch\n");
			rc = child_status;
			goto finalize;
		} else {
			fprintf(stderr, "Error: Child failed to launch, but set 0 status\n");
			rc = 1;
			goto finalize;
		}
	} else {
		//fprintf(stderr, "Child successfully launched (rc=%d)\n", rc);
	}
	close(c2p[0]); c2p[0]= -1;

	// Wait for the user to exit the child.
	if ((rc2 = waitpid(fork_pid, &child_status, 0)) == -1) {
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

	perform_accounting("JOBID", handle_match, NULL);

finalize:
	if (created) {
		//fprintf(stderr, "Trying to delete the created veth device.\n");
		if (delete_veth(sock, veth0)) {
			fprintf(stderr, "Unable to cleanup created device %s.\n", veth0);
			rc = 2;
		}
	}
	if (routed) {
		if (delete_outer_routing("./nat_delete_script.sh", "JOBID", veth0)) {
			fprintf(stderr, "Unable to successfully delete routes for %s\n", "JOBID");
		}
	}
	if (addr) {
		free(addr);
	}
	if (sock != -1) {
		close(sock);
	}
	if (p2c[0] != -1) {
		close(p2c[0]);
	}
	if (p2c[1] != -1) {
		close(p2c[1]);
	}
	if (c2p[0] != -1) {
		close(c2p[0]);
	}
	if (c2p[1] != -1) {
		close(c2p[1]);
	}
	if (child_stack) {
		free(child_stack);
	}
	return rc;
}
