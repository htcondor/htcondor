/***************************************************************
 *
 * Copyright (C) 2021, Condor Team, Computer Sciences Department,
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
#include "condor_daemon_core.h"
#include "condor_config.h"
#include "condor_distribution.h"
#include "match_prefix.h"
#include "subsystem_info.h"
#include "selector.h"
#include "dc_vpn.h"
#include "vpn_common.h"


namespace {


//-------------------------------------------------------------
int setup_child_namespace(int ready_sock, int parent_ready_fd,
	in_addr_t ipaddr, in_addr_t gw, in_addr_t netmask,
	int argc, char *argv[])
{
	auto uid = geteuid();
	auto gid = getegid();

                // Create the new namespaces we will work in.
        if (unshare(CLONE_NEWNET|CLONE_NEWUSER) < 0) {
                dprintf(D_ALWAYS, "Failed to create new network and user namespace - fatal!  Check your system configuration to ensure these are both enabled\n");
                return 1;
        }

	if (setup_uidgid_map(-1, -1, uid, gid)) {
		dprintf(D_ALWAYS, "Failed to set UID/GID mapping in the child namespace.\n");
		return 1;
	}

	int tun_fd;
	if ((tun_fd = open_tun("condor0")) < 0) {
		return 1;
	}

	if (configure_tun("condor0", ipaddr, gw, netmask)) {
		dprintf(D_ALWAYS, "Failed to configure the TUN device.\n");
		return 1;
	}

	if (sendfd(ready_sock, tun_fd) < 0) {
		fprintf(stderr, "Failed to send TUN fd to parent.\n");
		return 1;
	}

	char parent_buf[1];
	if (1 != full_read(parent_ready_fd, parent_buf, 1)) {
		dprintf(D_ALWAYS, "Failed to get go-ahead from parent.\n");
		return 1;
	}

	std::vector<char *> child_argv;
	child_argv.resize(argc + 1, nullptr);
	for (int idx = 0; idx < argc; idx++) {
		child_argv[idx] = argv[idx];
	}

	return execv(child_argv[0], &child_argv[0]);
}


struct child_info {
	pid_t boringtun_pid;
	pid_t main_pid;
	int boringtun_pipe_r;
	int boringtun_pipe_w;
	int main_pipe_r;
	int main_pipe_w;
};

struct child_info g_child_info;

void reap_sigaction(int signum, siginfo_t *info, void * /*context*/)
{
	if (signum != SIGCHLD) {return;} // Internal logic error...

	int write_pipe = -1;
	if (info->si_pid == g_child_info.boringtun_pid) {
		write_pipe = g_child_info.boringtun_pipe_w;
	} else if (info->si_pid == g_child_info.main_pid) {
		write_pipe = g_child_info.main_pipe_w;
	}
	if (write_pipe == -1) {return;}

	full_write(write_pipe, "1", 1);
}


//-------------------------------------------------------------
int launch_vpn_client(int argc, char *argv[], DCVPN &vpn, std::string lease, unsigned lease_interval, const std::string &ipaddr_str,
	const std::string &netmask_str, const std::string & /*network_str*/, const std::string & gwaddr_str,
	const std::string &base64_server_pubkey, const std::string &server_endpoint,
	const std::string &base64_client_privkey)
{
	in_addr_t ipaddr = htonl(inet_network(ipaddr_str.c_str()));
	if (!ipaddr) {
		fprintf(stderr, "Failed to parse IP address: %s\n", ipaddr_str.c_str());
		return 1;
	}
	in_addr_t netmask = htonl(inet_network(netmask_str.c_str()));
	if (!netmask) {
		fprintf(stderr, "Failed to parse network address: %s\n", netmask_str.c_str());
		return 1;
	}
	//printf("VPN client will use netmask %s.\n", netmask_str.c_str());
	in_addr_t gw = htonl(inet_network(gwaddr_str.c_str()));
	if (!gw) {
		fprintf(stderr, "Failed to parse gateway IP address: %s\n", gwaddr_str.c_str());
		return 1;
	}

	int ready_sock[2];
	if (socketpair(AF_LOCAL, SOCK_STREAM, 0, ready_sock) < 0) {
		fprintf(stderr, "Failed to create new communications pipe (errno=%d, %s)\n", errno, strerror(errno));
		return 1;
	}
	int parent_ready_fd[2];
	if (pipe(parent_ready_fd) < 0) {
		fprintf(stderr, "Failed to create new communications pipe (errno=%d, %s)\n", errno, strerror(errno));
		return 1;
	}

	int boringtun_signal_pipe[2], main_signal_pipe[2];
	if (((pipe2(boringtun_signal_pipe, O_CLOEXEC) < 0) ||
		pipe2(main_signal_pipe, O_CLOEXEC) < 0))
	{
		fprintf(stderr, "Failed to create internal signal pipe (errno=%d, %s)\n", errno, strerror(errno));
		return 1;
	}
	g_child_info.boringtun_pipe_r = boringtun_signal_pipe[0];
	g_child_info.boringtun_pipe_w = boringtun_signal_pipe[1];
	g_child_info.main_pipe_r = main_signal_pipe[0];
	g_child_info.main_pipe_w = main_signal_pipe[1];
	struct sigaction act;
	memset(&act, '\0', sizeof(act));
	act.sa_sigaction = &reap_sigaction;
	act.sa_flags = SA_SIGINFO|SA_NOCLDSTOP;
	if (sigaction(SIGCHLD, &act, nullptr) < 0) {
		fprintf(stderr, "Failed to setup new signal handler.\n");
		return 1;
	}

	int pid = fork();
	if (pid < 0) {
		fprintf(stderr, "Failed to fork new sub-process (errno=%d, %s).\n", errno, strerror(errno));
	}
	if (pid == 0) {
		int rval = setup_child_namespace(ready_sock[1], parent_ready_fd[0], ipaddr, gw, netmask, argc, argv);
		_exit(rval);
	}
	close(ready_sock[1]);
	close(parent_ready_fd[0]);

	int tun_fd = recvfd(ready_sock[0]);

	int uapi_fd;
	pid_t boringtun_pid;

	if (launch_boringtun(tun_fd, -1, uapi_fd, boringtun_pid)) {
		fprintf(stderr, "Failed to launch boringtun process.\n");
		return 1;
	}
	g_child_info.boringtun_pid = boringtun_pid;

	std::string private_key_hex = base64_key_to_hex(base64_client_privkey);
	std::string server_pubkey_hex = base64_key_to_hex(base64_server_pubkey);
	std::string command;
	formatstr(command, "set=1\n"
		"private_key=%s\n"
		"public_key=%s\n"
		"endpoint=%s\n"
		"allowed_ip=0.0.0.0/0\n"
		"\n",
		private_key_hex.c_str(),
		server_pubkey_hex.c_str(),
		server_endpoint.c_str());

	std::unordered_map<std::string, std::string> response_map;
	auto boring_err = boringtun_command(uapi_fd, command, response_map);
	dprintf(D_FULLDEBUG, "Results of server %s registration: %d\n", base64_server_pubkey.c_str(), boring_err);

	if (boring_err) {return boring_err;}

	if (1 != full_write(parent_ready_fd[1], "1", 1)) {
		fprintf(stderr, "Failed to give child a go-ahead\n");
	}

	g_child_info.main_pid = pid;

	Selector selector;
	selector.add_fd(g_child_info.main_pipe_r, Selector::IO_READ);
	selector.add_fd(g_child_info.boringtun_pipe_r, Selector::IO_READ);
	selector.set_timeout(lease_interval);
	while (!selector.has_ready()) {
		selector.execute();
		if (selector.timed_out()) {
			CondorError err;
			if (!vpn.heartbeat(lease, lease_interval, err)) {
				fprintf(stderr, "Failed to renew lease: %s\n", err.getFullText().c_str());
			}
		}
	}

	int child_status;
	if (selector.fd_ready(g_child_info.boringtun_pipe_r, Selector::IO_READ)) {
		while (true) {
			int rval = waitpid(g_child_info.boringtun_pid, &child_status, 0);
			if (rval > 0 || (rval == -1 && errno != EINTR)) {break;}
		}
		fprintf(stderr, "Boringtun failed with exit status %d.\n", child_status);
		kill(pid, SIGKILL);
	}
	CondorError err;
	if (!vpn.cancel(lease, err))
		fprintf(stderr, "Failed to destroy remote lease: %s\n", err.getFullText().c_str());

	while (true) {
		int rval = waitpid(pid, &child_status, 0);
		if (rval > 0 || (rval == -1 && errno != EINTR)) {break;}
	}

	if (WIFEXITED(child_status)) {_exit(child_status);}
	else {
		// send the same signal to ourselve; loop until we receive it.
		kill(getpid(), WTERMSIG(child_status));
		while (true) {sleep(1);}
	}
	return 0;
}

//-------------------------------------------------------------
void print_usage(const char *argv0)
{
	fprintf(stderr, "Usage: %s [-name NAME] [-pool POOL] -- <COMMAND>\n"
		"Launches COMMAND inside a VPN hosted by the specified VPN server.\n"
		"Options:\n"
		"    -name       <NAME>     Find a VPN daemon with this name\n"
		"    -constraint <CONST>    Query constraint for finding a VPN daemon\n"
		"    -pool       <POOL>     Query this collector\n",
		argv0);
}


}

int main(int argc, char *argv[]) {

	myDistro->Init( argc, argv );
	set_priv_initialize();
	config();

	std::string pool;
	std::string name;
	std::string constraint;

	int command_index = -1;
	for (int i = 1; i < argc; i++) {
		if (is_dash_arg_prefix(argv[i], "pool", 1)) {
			i++;
			if (!argv[i]) {
				fprintf(stderr, "%s: -pool requires a pool name argument.\n", argv[0]);
				exit(1);
			}
			pool = argv[i];
		}
		else if (is_dash_arg_prefix(argv[i], "name", 1)) {
			i++;
			if (!argv[i]) {
				fprintf(stderr, "%s: -name requires a daemon name argument.\n", argv[0]);
				exit(1);
			}
			name = argv[i];
		}
		else if (is_dash_arg_prefix(argv[i], "constraint", 1)) {
			i++;
			if (!argv[i]) {
				fprintf(stderr, "%s: -constraint requires an argument.\n", argv[0]);
				exit(1);
			}
			constraint = argv[i];
		} else if (is_dash_arg_prefix(argv[i], "debug", 1)) {
			dprintf_set_tool_debug("TOOL", 0);
		}
		else if (is_dash_arg_prefix(argv[i], "help", 1)) {
			print_usage(argv[0]);
			exit(1);
		}
		else if (!strcmp("--", argv[i])) {
			command_index = i + 1;
			break;
		}
		else {
			fprintf(stderr, "%s: Invalid command line argument: %s\n", argv[0], argv[i]);
			print_usage(argv[0]);
			exit(1);
		}
	}
	if (command_index == -1) {
		fprintf(stderr, "Must end command in '-- COMMAND'\n");
		exit(1);
	}

	std::string addr;
	if (!constraint.empty()) {
		if (!name.empty()) {
			fprintf(stderr, "Error! -constraint and -name option are incompatible.\n");
			exit(1);
		}
		CondorQuery query(VPN_AD);
		query.addANDConstraint("MyType == \"VPN\"");
		query.addANDConstraint(constraint.c_str());
		query.setLocationLookup("foo");

		std::unique_ptr<CollectorList> collectors(CollectorList::create(pool.empty() ? nullptr : pool.c_str()));
		CondorError errstack;
		ClassAdList ads;
		if (collectors->query(query, ads) != Q_OK) {
			fprintf(stderr, "Failed to query for VPN servers.\n");
			exit(1);
		}
		ads.Open();
		ClassAd *scan;
		std::vector<std::string> vpn_ads;
		while ((scan = ads.Next())) {
			std::string addr;
			if (!scan->EvaluateAttrString(ATTR_MY_ADDRESS, addr)) {
				continue;
			}
			vpn_ads.emplace_back(addr);
		}
		addr = vpn_ads[get_random_int_insecure() % vpn_ads.size()];
	}

	DCVPN vpn(addr.empty() ? (name.empty() ? nullptr : name.c_str()) : addr.c_str(),
		addr.empty() ? (pool.empty() ? nullptr : pool.c_str()) : "");

	if (!vpn.locate(Daemon::LOCATE_FOR_LOOKUP)) {
		fprintf(stderr, "Couldn't locate VPN (pool=%s, name=%s)\n",
			pool.c_str(), name.c_str());
		exit(1);
	}

	struct x25519_key privkey;
	if (!vpn_generate_x25519_secret_key(privkey)) {
		fprintf(stderr, "Failed to generate secret key.\n");
		exit(1);
	}

	struct x25519_key pubkey;
	if (!vpn_generate_x25519_pubkey(privkey, pubkey)) {
		fprintf(stderr, "Failed to generate public key.\n");
		exit(1);
	}

	auto base64_pubkey = vpn_x25519_key_to_base64(pubkey);
	std::string base64_pubkey_str(base64_pubkey);
	vpn_x25519_key_to_str_free(base64_pubkey);

	auto base64_key = vpn_x25519_key_to_base64(privkey);
	std::string base64_key_str(base64_key);
	vpn_x25519_key_to_str_free(base64_key);

	CondorError err;
	std::string ipaddr;
	std::string netmask;
	std::string network;
	std::string gwaddr;
	std::string base64_server_pubkey;
	std::string server_endpoint;
	std::string lease;
	unsigned lease_interval;
	if (!vpn.registerClient(base64_pubkey_str, ipaddr, netmask, network, gwaddr,
		base64_server_pubkey, server_endpoint, lease, lease_interval, err))
	{
		fprintf(stderr, "Failed to register client: %s\n", err.getFullText().c_str());
		exit(1);
	}

	return launch_vpn_client(argc - command_index, argv + command_index, vpn, lease, lease_interval, ipaddr, netmask, network, gwaddr, base64_server_pubkey, server_endpoint, base64_key_str);
}
