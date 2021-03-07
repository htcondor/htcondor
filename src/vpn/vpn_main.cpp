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
#include "condor_daemon_core.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "subsystem_info.h"
#include "condor_base64.h"
#include "condor_rw.h"
#include "selector.h"
#include "get_daemon_name.h"

#include "vpn_common.h"
#include "vpn_lease_mgr.h"

#include <net/if.h>
#include <net/route.h>
#include <linux/if_tun.h>
#include <linux/sched.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

void main_config();

namespace {
//-------------------------------------------------------------
// Program-wide globals
//
/// Child namespace globals
// File descriptor associated with the tun device in the child.
int g_tun_fd = -1;
// PID of the child namespace process
int g_namespace_pid = -1;
int g_child_reaper = -1;

/// Globals related to boringtun
//
// Socket to communicate with boringtun via the user API.
int g_boringtun_uapi_fd = -1;
// Reaper for the boringtun process
int g_boringtun_reaper = -1;
// Pid of the currently-running boringtun process.
int g_boringtun_pid = -1;
// UDP port for the boringtun process.
int g_boringtun_listen_port = -1;

/// Globals related to slirp4netns
//
// FD to communicate with slirp4netns; process will close if this FD closes.
int g_slirp4netns_exit_fd = -1;
// Reaper for the slirp4netns process
int g_slirp4netns_reaper = -1;
// Pid of the currently-running slirp4netns process.
int g_slirp4netns_pid = -1;

/// Name we will send to the collector.
std::string g_vpn_name;
/// Timer ID for updating the collector.
int g_update_collector_tid = -1;
/// Period, in seconds, for the collector update.
int g_update_interval = 300;
/// Timer ID for scrubbing inactive leases.
int g_lease_tid = -1;

/// State of the IP address management
std::unique_ptr<VPNLeaseMgr> g_lease_mgr;
#define DEFAULT_VPN_NETWORK "10.0.0.0/16"


//-------------------------------------------------------------
int configure_nat()
{
	std::string boringtun_network;
	param(boringtun_network, "VPN_SERVER_NETWORK", DEFAULT_VPN_NETWORK);

	int result_fd[2];
	if (pipe(result_fd) < 0) {
		dprintf(D_ALWAYS, "Failed to create communication pipes.\n");
		return 1;
	}
	auto pid = fork();
	if (pid < 0) {
		dprintf(D_ALWAYS, "Failed to fork to configure the NAT.\n");
		return 1;
	}

	if (pid == 0) {
		close(result_fd[0]);
		int usernsfd = -1, netnsfd = -1;
		std::string netns;
		formatstr(netns, "/proc/%d/ns/net", g_namespace_pid);
		if ((netnsfd = open(netns.c_str(), O_RDONLY)) < 0) {
			dprintf(D_ALWAYS, "Failed to open network namespace %s (errno=%d, %s).\n",
				netns.c_str(), errno, strerror(errno));
			_exit(1);
		}
		std::string userns;
		formatstr(userns, "/proc/%d/ns/user", g_namespace_pid);
		if ((usernsfd = open(userns.c_str(), O_RDONLY)) < 0) {
			dprintf(D_ALWAYS, "Failed to open user namespace %s (errno=%d, %s).\n",
				netns.c_str(), errno, strerror(errno));
			close(netnsfd);
			_exit(1);
		}

		if (setns(usernsfd, CLONE_NEWUSER) < 0) {
			dprintf(D_ALWAYS, "Failed to enter user namespace (errno=%d, %s).\n",
				errno, strerror(errno));
			close(netnsfd); close(usernsfd);
			_exit(1);
		}
		close(usernsfd);

		if (setns(netnsfd, CLONE_NEWNET) < 0) {
			dprintf(D_ALWAYS, "Failed to enter net namespace (errno=%d, %s).\n",
				errno, strerror(errno));
			close(netnsfd);
			_exit(1);
		}
		close(netnsfd);

		std::string command;
		formatstr(command, "iptables -t nat -A POSTROUTING -o slirp0 -s %s -j MASQUERADE", boringtun_network.c_str());
		int retval = system(command.c_str());
		if (retval) {
			dprintf(D_ALWAYS, "Failed to execute iptables command (retval=%d): %s\n", retval, command.c_str());
			_exit(1);
		}

		full_write(result_fd[1], "1", 1);
		_exit(0);
	}
	close(result_fd[1]);
	char result_buf[1];
	auto rval = full_read(result_fd[0], result_buf, 1);
	if (rval <= 0) {
		dprintf(D_ALWAYS, "Child process failed to configure NAT.\n");
		return 1;
	}
	return 0;
}


//-------------------------------------------------------------
std::string load_key_base64(const std::string &param_name)
{
	std::string fname;
	param(fname, param_name.c_str());
	if (fname.empty()) {
		dprintf(D_ALWAYS, "Parameter %s is not set; cannot load key.\n", param_name.c_str());
		return "";
	}

	std::unique_ptr<FILE, decltype(&fclose)> keyfile(
		safe_fopen_no_create(fname.c_str(), "r"),
		&fclose);

	if (!keyfile) {
		dprintf(D_ALWAYS, "Failed to open key file for %s '%s' (errno=%d, %s)\n",
			param_name.c_str(), fname.c_str(), errno, strerror(errno));
		return "";
	}

	std::string base64_key;
	for (std::string line; readLine(line, keyfile.get(), false);) {
		trim(line);
		if (line.empty() || line[0] == '#') continue;
		base64_key = line;
		break;
	}
	if (base64_key.empty()) {
		dprintf(D_ALWAYS, "Key file %s for %s has no key.\n", fname.c_str(), param_name.c_str());
		return "";
	}
	return base64_key;
}


//-------------------------------------------------------------
std::string load_key_hex(const std::string &param_name)
{
	auto base64_key = load_key_base64(param_name);

	return base64_key_to_hex(base64_key);
}


//-------------------------------------------------------------
int configure_boringtun()
{
	std::string key = load_key_hex("VPN_SERVER_KEY");
	if (key.empty()) {
		dprintf(D_ALWAYS, "No VPN_SERVER_KEY available; fatal.\n");
		DC_Exit(1);
	}
	std::string pubkey = load_key_hex("VPN_SERVER_PUBKEY");
	if (pubkey.empty()) {
		dprintf(D_ALWAYS, "No VPN_SERVER_PUBKEY available; fatal.\n");
		DC_Exit(1);
	}

	int udp_port = param_integer("VPN_SERVER_PORT", 0);
	std::string listen_port;
	if (udp_port) {
		formatstr(listen_port, "listen_port=%d\n", udp_port);
	}

	std::string command;
	formatstr(command,
		"set=1\n"
		"%s"
		"private_key=%s\n"
		"\n",
		listen_port.c_str(),
		key.c_str()
	);
	std::unordered_map<std::string, std::string> response_map;
	auto boring_err = boringtun_command(g_boringtun_uapi_fd, command, response_map);

	dprintf(D_FULLDEBUG, "Result of boringtun configuration: %d\n", boring_err);

	command = "get=1\n\n";
	boring_err = boringtun_command(g_boringtun_uapi_fd, command, response_map);
	std::string server_listen_port = response_map["listen_port"];
	try {
		g_boringtun_listen_port = std::stol(server_listen_port);
	} catch (...) {
		dprintf(D_ALWAYS, "Failed to get listen_port from boringtun.\n");
		DC_Exit(1);
	}

	return boring_err;
}


//-------------------------------------------------------------
int register_boringtun_client(const std::string &pubkey, const std::string ipaddr)
{
	std::string command;
	formatstr(command,
		"set=1\n"
		"public_key=%s\n"
		"persistent_keepalive_interval=30\n"
		"allowed_ip=%s/32\n"
		"\n",
		base64_key_to_hex(pubkey).c_str(),
		ipaddr.c_str());

	std::unordered_map<std::string, std::string> response_map;
	auto boring_err = boringtun_command(g_boringtun_uapi_fd, command, response_map);
	dprintf(D_FULLDEBUG, "Result of client %s registration: %d\n", pubkey.c_str(), boring_err);

	return boring_err;
}


//-------------------------------------------------------------
int launch_slirp4netns()
{
	int exit_fd[2];
	if (pipe(exit_fd) < 0) {
		dprintf(D_ALWAYS, "Failed to create exit FD slirp4netns communications (errno=%d, %s).\n",
			errno, strerror(errno));
		DC_Exit(1);
	}
	int ready_fd[2];
	if (pipe(ready_fd) < 0) {
		dprintf(D_ALWAYS, "Failed to create ready FD pipe for slirp4netns (error=%d, %s).\n",
			errno, strerror(errno));
		DC_Exit(1);
	}

	std::string slirp4netns_executable;
	param(slirp4netns_executable, "SLIRP4NETNS", "/usr/bin/slirp4netns");

	std::string cidr;
	param(cidr, "SLIRP4NETNS_CIDR", "192.168.0.0/24");

	std::string namespace_pid;
	formatstr(namespace_pid, "%d", g_namespace_pid);

	std::string exit_fd_str;
	formatstr(exit_fd_str, "%d", exit_fd[0]);

	std::string ready_fd_str;
	formatstr(ready_fd_str, "%d", ready_fd[1]);

	ArgList args;
	args.AppendArg("condor_slirp4netns");
	args.AppendArg("--configure");
	args.AppendArg("--cidr");
	args.AppendArg(cidr);
	args.AppendArg("--exit-fd");
	args.AppendArg(exit_fd_str);
	args.AppendArg("--ready-fd");
	args.AppendArg(ready_fd_str);
	args.AppendArg(namespace_pid);
	args.AppendArg("slirp0");

	Env env;

	int fd_inherit[3];
	fd_inherit[0] = exit_fd[0];
	fd_inherit[1] = ready_fd[1];
	fd_inherit[2] = 0;

	auto pid = daemonCore->Create_Process(
		slirp4netns_executable.c_str(),
		args,
		PRIV_CONDOR,
		g_slirp4netns_reaper,
		false,
		false,
		&env,
		NULL,
		NULL,
		NULL,
		NULL,
		fd_inherit);

	close(exit_fd[0]);
	close(ready_fd[1]);
	if (!pid) {
		dprintf(D_ALWAYS, "Failed to launch slirp4netns; fatal.\n");
		DC_Exit(1);
	}

		// Wait for an indication all is well.
	char ready[1];
	int rval;
	if ((rval = full_read(ready_fd[0], ready, 1) != 1)) {
		dprintf(D_ALWAYS, "slirp4netns failed to initialize; fatal.\n");
		DC_Exit(1);
	}
	if (ready[0] != '1') {
		dprintf(D_ALWAYS, "slirp4netns indicated failure; fatal.\n");
		DC_Exit(1);
	}

	g_slirp4netns_exit_fd = exit_fd[1];
	g_slirp4netns_pid = pid;

	return 0;
}


//-------------------------------------------------------------
int setup_child_namespace(int exit_pipe, int ready_sock)
{
	auto uid = geteuid();
	auto gid = getegid();

		// Create the new namespaces we will work in.
	if (unshare(CLONE_NEWNET|CLONE_NEWUSER) < 0) {
		dprintf(D_ALWAYS, "Failed to create new network and user namespace - fatal!  Check your system configuration to ensure these are both enabled\n");
		return 1;
	}

	if (setup_uidgid_map(0, 0, uid, gid)) {
		dprintf(D_ALWAYS, "Fatal error in setting up new UID / GID.\n");
		return 1;
	}

		// Enable packet forwarding.
	int fd;
	if ((fd = open("/proc/sys/net/ipv4/ip_forward", O_WRONLY)) < 0) {
		dprintf(D_ALWAYS, "Failed to open /proc/sys/net/ipv4/ip_forward (errno=%d, %s).\n",
			errno, strerror(errno));
		return -1;
	}
	char enable[] = "1";
	if (1 != write(fd, enable, 1)) {
		dprintf(D_ALWAYS, "Failed to enable IP forwarding (errno=%d, %s).\n",
			errno, strerror(errno));
		return -1;
	}

		// Setup our new network device and get the corresponding FD.
	auto tun_fd = open_tun("condor0");
	if (tun_fd < 0) {
		return 1;
	}

		// TODO: Eventually, this should go into main_config.
	std::string network_full;
	param(network_full, "VPN_SERVER_NETWORK", DEFAULT_VPN_NETWORK);
	std::string network = network_full.substr(0, network_full.find("/"));
	in_addr_t network_addr = htonl(inet_network(network.c_str()));
	if (!network_addr) {
		dprintf(D_ALWAYS, "Failed to convert server network to IPv4: %s\n", network_full.c_str());
		return 1;
	}
	in_addr_t ipaddr = htonl(ntohl(network_addr) + 2);

	in_addr_t netmask = htonl(inet_network("255.255.255.0"));

	if (configure_tun("condor0", ipaddr, 0, netmask)) {
		dprintf(D_ALWAYS, "Failed to configure TUN network.\n");
		return 1;
	}

		// Pass the TUN filedescriptor back to our parent indicating we're ready.
	if (sendfd(ready_sock, tun_fd) < 0) {
		return 1;
	}

		// The parent never writes into this pipe; hence, the only time
		// the full_read below exits is when the parent exits.  This is
		// effectively an infinite sleep.
	char exit_buf[1];
	full_read(exit_pipe, exit_buf, 1);
	return 0;
}


int reap_child(int pid, int exit_status)
{
	if (pid == g_namespace_pid) {
		dprintf(D_ALWAYS, "Child namespace failed with status %d; VPN needs to be restarted.\n", exit_status);
		DC_Exit(1);
	}
	return 0;
}


int reap_boringtun(int pid, int exit_status)
{
	dprintf(D_ALWAYS, "boringtun process %d exited unexpectedly with status %d; reconfiguring daemon.\n", pid, exit_status);
	if (g_boringtun_pid == pid) {g_boringtun_pid = -1;}
	main_config();
	return 0;
}


int reap_slirp4netns(int pid, int exit_status)
{
		// 
	if (g_slirp4netns_pid == pid && !exit_status) {
		dprintf(D_FULLDEBUG, "slirp4netns parent %d exited & created a daemon.\n",
			pid);
		g_slirp4netns_pid = -1;
		return 0;
	}
	dprintf(D_ALWAYS, "slirp4netns process %d exited unexpectedly with status %d; reconfiguring daemon.\n", pid, exit_status);
	main_config();
	return 0;
}


//-------------------------------------------------------------
void
update_leases() {
	dprintf(D_FULLDEBUG, "Starting maintenance of leases for VPN server.\n");
	if (g_lease_mgr)
		g_lease_mgr->Maintenance();
}

//-------------------------------------------------------------
void
update_collector_ad() {
	dprintf(D_FULLDEBUG, "Starting update of VPN ad in collector.\n");
	ClassAd ad;
	SetMyTypeName(ad, VPN_ADTYPE);
	SetTargetTypeName(ad, "");

	if (g_lease_mgr) {g_lease_mgr->UpdateStats(ad);}

	ad.InsertAttr(ATTR_NAME, g_vpn_name);

	daemonCore->publish(&ad);
	daemonCore->dc_stats.Publish(ad);
	daemonCore->monitor_data.ExportData(&ad);
	daemonCore->sendUpdates(UPDATE_AD_GENERIC, &ad, NULL, true);

	daemonCore->Reset_Timer(g_update_collector_tid, g_update_interval, g_update_interval);
	dprintf(D_FULLDEBUG, "Finished update of VPN ad.\n");
}


//-------------------------------------------------------------
int determine_name()
{
	std::string name_str;
	auto myname = get_mySubSystem()->getLocalName();
	if (myname) {
		auto valid_name = build_valid_daemon_name(myname);
		if (valid_name) {
			g_vpn_name = std::string(valid_name);
			free(valid_name);

			return 1;
		}
	} else {
		auto default_name = default_daemon_name();
		if (!default_name) {
			dprintf(D_ALWAYS, "Failed to determine vpn server local name.\n");
			return 1;
		}
		g_vpn_name = std::string(default_name);
		free(default_name);
	}
	return 0;
}	


//-------------------------------------------------------------
int register_client_failure(ReliSock &rsock, int error_code, const std::string &error_string)
{
	dprintf(D_ALWAYS, "%s\n", error_string.c_str());
	ClassAd respAd;
	respAd.InsertAttr(ATTR_ERROR_CODE, error_code);
	respAd.InsertAttr(ATTR_ERROR_STRING, error_string);

	rsock.encode();
	if (!putClassAd(&rsock, respAd) || !rsock.end_of_message()) {
		dprintf(D_ALWAYS, "Failed to send error response\n");
		return 1;
	}
	return 0;
}


//-------------------------------------------------------------
int register_client(int, Stream *stream)
{
	ReliSock &rsock = *static_cast<ReliSock*>(stream);

	rsock.decode();

	ClassAd reqAd;
	if (!getClassAd(&rsock, reqAd) || !rsock.end_of_message()) {
		dprintf(D_ALWAYS, "Failed to get registration request from client.\n");
		return 1;
	}
	dprintf(D_FULLDEBUG, "Client registration request:\n");
	dPrintAd(D_FULLDEBUG, reqAd);
	std::string client_pubkey;
	if (!reqAd.EvaluateAttrString("ClientPubkey", client_pubkey)) {
		return register_client_failure(rsock, 1, "Registration request missing client pubkey");
	}
	ClassAd respAd;

	std::string base64_pubkey = load_key_base64("VPN_SERVER_PUBKEY");
	if (base64_pubkey.empty()) {
		return register_client_failure(rsock, 3, "VPN server has no pubkey configured");
	}
	respAd.InsertAttr("Pubkey", base64_pubkey);

	CondorError err;
	if (!g_lease_mgr->CreateLease(client_pubkey, respAd, err)) {
		return register_client_failure(rsock, err.code(), err.message());
	}
	std::string host_str;
	respAd.EvaluateAttrString("IPAddr", host_str);

	const char *endpoint = daemonCore->InfoCommandSinfulString();
	condor_sockaddr addr;
	addr.from_sinful(endpoint);
	std::string endpoint_ip;
	formatstr(endpoint_ip, "%s:%d", addr.to_ip_string().c_str(), g_boringtun_listen_port);

	respAd.InsertAttr("Endpoint", endpoint_ip);

	rsock.encode();
	if (!putClassAd(&rsock, respAd) || !rsock.end_of_message()) {
		dprintf(D_ALWAYS, "Failed to send registration response.\n");
		return 1;
	}
	return 0;
}


//-------------------------------------------------------------
int heartbeat_client(int, Stream *stream)
{
	ReliSock &rsock = *static_cast<ReliSock*>(stream);

	rsock.decode();

	ClassAd reqAd;
	if (!getClassAd(&rsock, reqAd) || !rsock.end_of_message()) {
		dprintf(D_ALWAYS, "Failed to get heartbeat request from client.\n");
		return 1;
	}
	dprintf(D_FULLDEBUG, "Client heartbeat request:\n");
	dPrintAd(D_FULLDEBUG, reqAd);
	std::string lease;
	if (!reqAd.EvaluateAttrString("Lease", lease)) {
		return register_client_failure(rsock, 1, "Heartbeat request missing client lease");
	}
	ClassAd respAd;

	CondorError err;
	unsigned lifetime;
	if (!g_lease_mgr->RefreshLease(lease, lifetime, err)) {
		return register_client_failure(rsock, err.code(), err.message());
	}
	respAd.InsertAttr(ATTR_ERROR_CODE, 0);
	respAd.InsertAttr("LeaseLifetime", static_cast<long>(lifetime));

	rsock.encode();
	if (!putClassAd(&rsock, respAd) || !rsock.end_of_message()) {
		dprintf(D_ALWAYS, "Failed to send heartbea response.\n");
		return 1;
	}
	return 0;
}


//-------------------------------------------------------------
int unregister_client(int, Stream *stream)
{
	ReliSock &rsock = *static_cast<ReliSock*>(stream);

	rsock.decode();

	ClassAd reqAd;
	if (!getClassAd(&rsock, reqAd) || !rsock.end_of_message()) {
		dprintf(D_ALWAYS, "Failed to get unregister request from client.\n");
		return 1;
	}
	dprintf(D_FULLDEBUG, "Client unregister request:\n");
	dPrintAd(D_FULLDEBUG, reqAd);
	std::string lease;
	if (!reqAd.EvaluateAttrString("Lease", lease)) {
		return register_client_failure(rsock, 1, "Unregister request missing client lease");
	}
	ClassAd respAd;

	CondorError err;
	if (!g_lease_mgr->RemoveLease(lease, err)) {
		return register_client_failure(rsock, err.code(), err.message());
	}
	respAd.InsertAttr(ATTR_ERROR_CODE, 0);

	rsock.encode();
	if (!putClassAd(&rsock, respAd) || !rsock.end_of_message()) {
		dprintf(D_ALWAYS, "Failed to send unregister response.\n");
		return 1;
	}
	return 0;
}


bool setup_ipam()
{
	std::string vpn_network;
	param(vpn_network, "VPN_SERVER_NETWORK", DEFAULT_VPN_NETWORK);

	auto pos = vpn_network.find("/");
	if (pos == std::string::npos) {
		dprintf(D_ALWAYS, "VPN_SERVER_NETWORK parameter (%s) must be of the form IP / NETMASK.\n", vpn_network.c_str());
		return false;
	}

	in_addr_t network_addr = htonl(inet_network(vpn_network.substr(0, pos).c_str()));
	if (!network_addr) {
		dprintf(D_ALWAYS, "Failed to parse IP address in VPN_SERVER_NETWORK parameter %s.\n", vpn_network.c_str());
		return false;
	}

	int netmask;
	try {
		netmask = std::stol(vpn_network.substr(pos + 1));
	} catch (...) {
		dprintf(D_ALWAYS, "Failed to parse netmask in VPN_SERVER_NETWORK parameter %s.\n", vpn_network.c_str());
		return false;
	}
	if (netmask < 0 || netmask > 32) {
		dprintf(D_ALWAYS, "Invalid netmask (%d) in VPN_SERVER_NETWORK parameter %s.\n", netmask, vpn_network.c_str());
		return false;
	}

	in_addr_t netmask_addr;
	if (netmask == 0) netmask_addr = 0;
	else netmask_addr = ~htonl((1 << (32 - netmask)) - 1);
	network_addr = network_addr & netmask_addr;

	struct in_addr addr;
	addr.s_addr = network_addr;
	std::string network_str(inet_ntoa(addr));
	addr.s_addr = netmask_addr;
	std::string netmask_str(inet_ntoa(addr));
	dprintf(D_FULLDEBUG, "VPN server will use network %s and netmask %s.\n",
		network_str.c_str(), netmask_str.c_str());

	g_lease_mgr.reset(new VPNLeaseMgr(network_addr, netmask_addr, g_boringtun_uapi_fd));

	return true;
}


} // end anonymous namespace


//-------------------------------------------------------------
void main_init(int /* argc */, char * /* argv */ [])
{
	dprintf(D_FULLDEBUG, "main_init() called\n");

	g_child_reaper = daemonCore->Register_Reaper("Child namespace reaper",
		&reap_child,
		"Shuts down daemon on child namespace failure.");
	daemonCore->Set_Default_Reaper(g_child_reaper);

	g_boringtun_reaper = daemonCore->Register_Reaper("boringtun reaper",
		&reap_boringtun,
		"Restarts the boringtun daemon.");

	g_slirp4netns_reaper = daemonCore->Register_Reaper("slirp4netns reaper",
		&reap_slirp4netns,
		"Restarts the slirp4netns daemon.");

	int exit_pipe[2];
	if (pipe(exit_pipe) < 0) {
		dprintf(D_ALWAYS, "Failed to create exit pipe for child namespace\n");
		DC_Exit(1);
	}
	int ready_sock[2];
	if (socketpair(AF_LOCAL, SOCK_STREAM, 0, ready_sock) < 0) {
		dprintf(D_ALWAYS, "Failed to create local socket for FD passing (errno=%d, %s).\n",
			errno, strerror(errno));
		DC_Exit(1);
	}
	pid_t childpid = fork();
	if (childpid < 0) {
		dprintf(D_ALWAYS, "Failed to fork namespace subprocess");
		DC_Exit(1);
	}
	if (childpid == 0) { // Child case.  We do *not* exec
		close(exit_pipe[1]);
		close(ready_sock[0]);
		auto rval = setup_child_namespace(exit_pipe[0], ready_sock[1]);
		DC_Exit(rval);
	}
	close(exit_pipe[0]);
	close(ready_sock[1]);

	g_namespace_pid = childpid;
	g_tun_fd = recvfd(ready_sock[0]);
	if (g_tun_fd < 0) {
		dprintf(D_ALWAYS, "Failed to get TUN fd from child.\n");
		DC_Exit(1);
	}
	dprintf(D_ALWAYS, "Successfully setup network namespace in child PID %d.\n", childpid);

	std::vector<DCpermission> alt_perms;
	alt_perms.push_back(ADVERTISE_STARTD_PERM);
	alt_perms.push_back(ADVERTISE_SCHEDD_PERM);
	alt_perms.push_back(ADVERTISE_MASTER_PERM);
	alt_perms.push_back(NEGOTIATOR);
	daemonCore->Register_Command(VPN_REGISTER, "Register VPN Client",
		(CommandHandler) &register_client, "Register VPN Client",
		DAEMON, D_COMMAND, true, 1, &alt_perms);

	daemonCore->Register_Command(VPN_HEARTBEAT, "Update VPN Client Lease",
		(CommandHandler) &heartbeat_client, "Update VPN Client Lease",
		DAEMON, D_COMMAND, true, 1, &alt_perms);

	daemonCore->Register_Command(VPN_UNREGISTER, "Unregister VPN Client Lease",
		(CommandHandler) &unregister_client, "Unregister VPN Client Lease",
		DAEMON, D_COMMAND, true, 1, &alt_perms);

	main_config();
}

//-------------------------------------------------------------

void 
main_config()
{
	dprintf(D_FULLDEBUG, "main_config() called\n");

	if (determine_name()) {
		if (g_vpn_name.empty()) {
			dprintf(D_ALWAYS, "Failed to determine the local server name.\n");
			DC_Exit(1);
		}
	}
	dprintf(D_FULLDEBUG, "Configuring VPN server %s.\n", g_vpn_name.c_str());

	// Setup boringtun - connectivity for clients
	//
	if (g_boringtun_pid == -1) {
		if (launch_boringtun(g_tun_fd, g_boringtun_reaper, g_boringtun_uapi_fd, g_boringtun_pid)) {
			dprintf(D_ALWAYS, "Failed to launch boringtun; fatal error.\n");
			DC_Exit(1);
		}
		if (configure_boringtun()) {
			dprintf(D_ALWAYS, "Failed to configure boringtun; fatal error.\n");
			DC_Exit(1);
		}
	}

	if (!g_lease_mgr && !setup_ipam()) {
		dprintf(D_ALWAYS, "Failed to setup IP address management.\n");
		DC_Exit(1);
	}
	
	// slirp4netns - connectivity from our netns to the outside
	// world
	//
	if (g_slirp4netns_pid == -1) {
		if (launch_slirp4netns()) {
			dprintf(D_ALWAYS, "Failed to launch slirp4netns; fatal error.\n");
			DC_Exit(1);
		}
		if (configure_nat()) {
			dprintf(D_ALWAYS, "Failed to setup NAT; fatal error.\n");
			DC_Exit(1);
		}
	}

		// TODO: handle re-register!
	g_update_interval = param_integer("VPN_UPDATE_INTERVAL", 300);
	g_update_collector_tid = daemonCore->Register_Timer(
		0, g_update_interval,
		(TimerHandler) update_collector_ad,
		"Update Collector");

	auto lease_interval = param_integer("VPN_LEASE_LIFETIME", 900) / 3;
	g_lease_tid = daemonCore->Register_Timer(
		lease_interval, lease_interval,
		(TimerHandler) update_leases,
		"Lease Update Timer");
}

//-------------------------------------------------------------

void main_shutdown_fast()
{
	dprintf(D_FULLDEBUG, "main_shutdown_fast() called\n");
	DC_Exit(0);
}

//-------------------------------------------------------------

void main_shutdown_graceful()
{
	dprintf(D_FULLDEBUG, "main_shutdown_graceful() called\n");
	DC_Exit(0);
}

//-------------------------------------------------------------

int
main( int argc, char **argv )
{
	set_mySubSystem("VPN_SERVER", SUBSYSTEM_TYPE_VPN );

	dc_main_init = main_init;
	dc_main_config = main_config;
	dc_main_shutdown_fast = main_shutdown_fast;
	dc_main_shutdown_graceful = main_shutdown_graceful;
	return dc_main( argc, argv );
}
