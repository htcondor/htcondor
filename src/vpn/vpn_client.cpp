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
#include "dc_vpn.h"
#include "vpn_common.h"

#include <dlfcn.h>

#include "wireguard_ffi.h"

namespace {

struct x25519_key (*g_x25519_secret_key)() = nullptr;
struct x25519_key (*g_x25519_public_key)(struct x25519_key) = nullptr;
const char * (*g_x25519_key_to_base64)(struct x25519_key) = nullptr;
void (*g_x25519_key_to_str_free)(const char *) = nullptr;

bool load_boringtun()
{
	void *dl_hdl;

	if ((dl_hdl = dlopen("libboringtun.so", RTLD_LAZY)) == NULL ||
		!(g_x25519_secret_key = (struct x25519_key (*)())dlsym(dl_hdl, "x25519_secret_key")) ||
		!(g_x25519_public_key = (struct x25519_key (*)(struct x25519_key))dlsym(dl_hdl, "x25519_public_key")) ||
		!(g_x25519_key_to_base64 = (const char *(*)(struct x25519_key))dlsym(dl_hdl, "x25519_key_to_base64")) ||
		!(g_x25519_key_to_str_free = (void (*)(const char *))dlsym(dl_hdl, "x25519_key_to_str_free"))
	) {
		fprintf(stderr, "Failed to open boringtun library: %s\n", dlerror());
		return false;
	}
	return true;
}


//-------------------------------------------------------------
int setup_child_namespace(int ready_sock, int parent_ready_fd,
	in_addr_t ipaddr, in_addr_t gw, in_addr_t netmask,
	int argc, char *argv[])
{
        //auto euid = geteuid();
        //auto egid = getegid();

                // Create the new namespaces we will work in.
        if (unshare(CLONE_NEWNET|CLONE_NEWUSER) < 0) {
                dprintf(D_ALWAYS, "Failed to create new network and user namespace - fatal!  Check your system configuration to ensure these are both enabled\n");
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


int reap_boringtun(int /*pid*/, int /*exit_status*/)
{
	return 0;
}


//-------------------------------------------------------------
int launch_vpn_client(int argc, char *argv[], const std::string &ipaddr_str,
	const std::string &netmask_str, const std::string & gwaddr_str,
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
	printf("VPN client will use netmask %s.\n", netmask_str.c_str());
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

	int reaper = daemonCore->Register_Reaper("Boringtun reaper", reap_boringtun, "Boringtun reaper");

	if (launch_boringtun(tun_fd, reaper, uapi_fd, boringtun_pid)) {
		fprintf(stderr, "Failed to launch boringtun process.\n");
		return 1;
	}

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

	int child_status;
	while (true) {
		int rval = waitpid(pid, &child_status, 0);
		if (rval == -1 && errno != EINTR) {break;}
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
	fprintf(stderr, "Usage: %s [-name NAME] [-pool POOL] -- <COMMAND>"
		"Launches COMMAND inside a VPN hosted by the specified VPN server.\n"
		"Options:\n"
		"    -name    <NAME>     Find a VPN daemon with this name\n"
		"    -pool    <POOL>     Query this collector\n",
		argv0);
}


}

void main_init(int argc, char *argv[]) {

	if (!load_boringtun()) {
		fprintf(stderr, "Failed to load libboringtun; necessary for %s.\n", argv[0]);
		DC_Exit(1);
	}

	myDistro->Init( argc, argv );
	set_priv_initialize();
	config();

	std::string pool;
	std::string name;

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
		else if (is_dash_arg_prefix(argv[i], "debug", 1)) {
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

	DCVPN vpn(name.empty() ? nullptr : name.c_str(),
		pool.empty() ? nullptr : pool.c_str());

	if (!vpn.locate(Daemon::LOCATE_FOR_LOOKUP)) {
		fprintf(stderr, "Couldn't locate VPN (pool=%s, name=%s)\n",
			pool.c_str(), name.c_str());
		exit(1);
	}

	struct x25519_key privkey = (*g_x25519_secret_key)();

	struct x25519_key pubkey = (*g_x25519_public_key)(privkey);

	auto base64_pubkey = (*g_x25519_key_to_base64)(pubkey);
	std::string base64_pubkey_str(base64_pubkey);
	(*g_x25519_key_to_str_free)(base64_pubkey);

	auto base64_key = (*g_x25519_key_to_base64)(privkey);
	std::string base64_key_str(base64_key);
	(*g_x25519_key_to_str_free)(base64_key);

	CondorError err;
	std::string ipaddr;
	std::string netmask;
	std::string gwaddr;
	std::string base64_server_pubkey;
	std::string server_endpoint;
	if (!vpn.registerClient(base64_pubkey_str, ipaddr, netmask, gwaddr,
		base64_server_pubkey, server_endpoint, err))
	{
		fprintf(stderr, "Failed to register client: %s\n", err.getFullText().c_str());
		exit(1);
	}

	DC_Exit(launch_vpn_client(argc - command_index, argv + command_index, ipaddr, netmask, gwaddr, base64_server_pubkey, server_endpoint, base64_key_str));
}


void main_config() {}
void main_shutdown_fast() {}
void main_shutdown_graceful() {}

int
main( int argc, char **argv )
{
	set_mySubSystem("TOOL", SUBSYSTEM_TYPE_TOOL);

	dc_main_init = main_init;
	dc_main_config = main_config;
	dc_main_shutdown_fast = main_shutdown_fast;
	dc_main_shutdown_graceful = main_shutdown_graceful;
	return dc_main( argc, argv );
}
