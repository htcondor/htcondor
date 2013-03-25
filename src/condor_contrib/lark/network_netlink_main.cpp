
#include "condor_common.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "condor_uid.h"

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
#include <getopt.h>
#include <time.h>

#include "lark_attributes.h"
#include "network_manipulation.h"
#include "network_namespaces.h"
#include "network_adapter.unix.h"

int handle_match(const unsigned char * rule_name, long long bytes_matched, void * unused) {
        if (unused) {}
        printf("Network%s: %lld\n", rule_name, bytes_matched);
	return 0;
}

static struct timespec g_starttime;

static int child_post_fork(lark::NetworkNamespaceManager &manager) {

	int rc = 0;
	int diffms;
	if (manager.PostForkChild())
	{
		fprintf(stderr, "Post-fork child process failed.\n");
		rc = 4;
		goto finalize_child;
	}
	struct timespec finishtime;
	clock_gettime(CLOCK_MONOTONIC, &finishtime);
	diffms = 1000*(finishtime.tv_sec - g_starttime.tv_sec) + (finishtime.tv_nsec - g_starttime.tv_nsec)/1e6;
	fprintf(stdout, "Total startup time: %d ms\n", diffms);

	// Exec out.
	rc = execl("/bin/sh", "sh", "-c", "date; ip link list; ip addr list; ip route; iptables -L -v ; ping 129.93.1.141 -c 5 && curl 129.93.1.141; exit 0", (char *)0);
	fprintf(stderr, "Failure to exec /bin/sh. (errno=%d) %s\n", errno, strerror(errno));

finalize_child:
	_exit(rc);
}

int main(int argc, char * argv[])
{

	// Suppress unused param compiler warnings:
	int rc = 0;
	int child_status = 0;
	pid_t fork_pid;

	classad::ClassAd job_ad;
	classad_shared_ptr<classad::ClassAd> machine_ad_ptr(new classad::ClassAd);

	int verbose_flag = 0;
	int static_address_flag = 0;
	int bridge_flag = 0;
	int help_flag = 0;
	int c;
	std::string static_internal_mac, static_external_mac;
	std::string static_internal_address, static_external_address, bridge_interface;

	while (1) {
		static struct option long_options[] =
		{
			{"verbose", no_argument, &verbose_flag, 1},
			{"bridge",  no_argument, &bridge_flag, 1},
			{"internal_address", required_argument, 0, 'i'},
			{"external_address", required_argument, 0, 'e'},
			{"internal_mac", required_argument, 0, 'm'},
			{"external_mac", required_argument, 0, 'n'},
			{"bridge_interface", required_argument, 0, 'b'},
			{0, 0, 0, 0}
		};
		int option_index = 0;
		c = getopt_long(argc, argv, "m:n:e:i:b:", long_options, &option_index);

		if (c == -1) {
			break;
		}

		switch (c) {
		case 0:
			break;
		case 'm':
			static_internal_mac = optarg;
		case 'n':
			static_external_mac = optarg;
		case 'i':
			static_address_flag = 1;
			static_internal_address = optarg;
			break;
		case 'e':
			static_address_flag = 1;
			static_external_address = optarg;
			break;
		case 'b':
			bridge_interface = optarg;
			break;
		case '?':
			help_flag = 1;
			break;
		default:
			printf("Unknown option: %c\n", c);
			help_flag = 1;
			break;
		}
	}
	if (help_flag) {
		printf("Usage: lark_network_namespace_tester [options]\n");
		printf("Options:\n");
		printf(" --verbose: Increase verbosity of output.\n");
		printf(" --bridge:  Bridge job device (default is NAT).\n");
		printf(" -i,--internal_address IPV4_ADDRESS: Internal address to use (default is to auto-detect)\n");
		printf(" -e,--external_address IPV4_ADDRESS: External address to use (default is to auto-detect)\n");
		printf(" -m,--internal_mac MAC: Internal MAC address to use (default is to create a random MAC)\n");
		printf(" -n,--external_mac MAC: External MAC address to use (default is to create a random MAC)\n");
		printf(" -b,--bridge_interface IFACE: System interface to use as a bridge (required with the --bridge flag).\n");
		return 0;
	}

	machine_ad_ptr->InsertAttr(ATTR_NETWORK_TYPE, bridge_flag ? "bridge" : "nat");
	if (bridge_flag && bridge_interface.length()) {
		machine_ad_ptr->InsertAttr(ATTR_BRIDGE_DEVICE, bridge_interface);
	}

	if (static_address_flag) {
		machine_ad_ptr->InsertAttr(ATTR_ADDRESS_TYPE, "static");
		machine_ad_ptr->InsertAttr(ATTR_INTERNAL_ADDRESS_IPV4, static_internal_address);
		machine_ad_ptr->InsertAttr(ATTR_EXTERNAL_ADDRESS_IPV4, static_external_address);
	} else {
		machine_ad_ptr->InsertAttr(ATTR_ADDRESS_TYPE, bridge_flag ? "dhcp" : "local");
	}

	if (static_internal_mac.size()) {
		machine_ad_ptr->InsertAttr(ATTR_INTERNAL_MAC, static_internal_mac);
	}
	if (static_external_mac.size()) {
		machine_ad_ptr->InsertAttr(ATTR_EXTERNAL_MAC, static_external_mac);
	}

	config();
	if (verbose_flag) {
		param_insert("TOOL_DEBUG", "D_FULLDEBUG");
	}
	dprintf_set_tool_debug("TOOL", 0);
	dprintf(D_FULLDEBUG, "Running network namespace tester in debug mode.\n");

	machine_ad_ptr->InsertAttr(ATTR_NETWORK_ACCOUNTING, true);

	// Defaults for create / delete scripts.
	// Only done for the test case.
	std::string create_script;
	if (!param(create_script, "NETWORK_NAMESPACE_CREATE_SCRIPT"))
		param_insert("NETWORK_NAMESPACE_CREATE_SCRIPT", "./lark_setup_script.sh");
	if (!param(create_script, "NETWORK_NAMESPACE_DELETE_SCRIPT"))
		param_insert("NETWORK_NAMESPACE_DELETE_SCRIPT", "./lark_cleanup_script.sh");

	classad::PrettyPrint pp;
	std::string ad_str;
	pp.Unparse(ad_str, machine_ad_ptr.get());
	dprintf(D_ALWAYS, "ClassAd contents: %s\n", ad_str.c_str());

	TemporaryPrivSentry sentry(PRIV_ROOT);

	clock_gettime(CLOCK_MONOTONIC, &g_starttime);

	lark::NetworkNamespaceManager &manager = lark::NetworkNamespaceManager::GetManager();

	if (manager.PrepareNetwork("tester", job_ad, machine_ad_ptr))
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
