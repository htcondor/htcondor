/***************************************************************
 *
 * Copyright (C) 2020, Condor Team, Computer Sciences Department,
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

#include <net/if.h>
#include <net/route.h>
#include <linux/if_tun.h>
#include <linux/sched.h>

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

/// Globals related to slirp4netns
//
// FD to communicate with slirp4netns; process will close if this FD closes.
int g_slirp4netns_exit_fd = -1;
// Reaper for the slirp4netns process
int g_slirp4netns_reaper = -1;
// Pid of the currently-running slirp4netns process.
int g_slirp4netns_pid = -1;

//-------------------------------------------------------------
// Send a file descriptor, fd, to the Unix socket, sock.
int sendfd(int sock, int fd)
{
	ssize_t rc;
	struct msghdr msg;
	struct cmsghdr *cmsg;
	char cmsgbuf[CMSG_SPACE(sizeof(fd))];
	struct iovec iov;
	char dummy = '\0';
	memset(&msg, 0, sizeof(msg));
	iov.iov_base = &dummy;
	iov.iov_len = 1;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = cmsgbuf;
	msg.msg_controllen = sizeof(cmsgbuf);
	cmsg = CMSG_FIRSTHDR(&msg);
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;
	cmsg->cmsg_len = CMSG_LEN(sizeof(fd));
	memcpy(CMSG_DATA(cmsg), &fd, sizeof(fd));
	msg.msg_controllen = cmsg->cmsg_len;
	if ((rc = sendmsg(sock, &msg, 0)) < 0) {
		dprintf(D_ALWAYS, "Failed to send file descriptor to parent (errno=%d, %s).\n",
			errno, strerror(errno));
	}
	return rc;
}


//-------------------------------------------------------------
// Receive a file descriptor (return value) from a Unix socket pair.
static int recvfd(int sock)
{
	int fd;
	ssize_t rc;
	struct msghdr msg;
	struct cmsghdr *cmsg;
	char cmsgbuf[CMSG_SPACE(sizeof(fd))];
	struct iovec iov;
	char dummy = '\0';
	memset(&msg, 0, sizeof(msg));
	iov.iov_base = &dummy;
	iov.iov_len = 1;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = cmsgbuf;
	msg.msg_controllen = sizeof(cmsgbuf);
	if ((rc = recvmsg(sock, &msg, 0)) < 0) {
		dprintf(D_ALWAYS, "Failed to receive a message from socket (errno=%d, %s)\n",
			errno, strerror(errno));
		return (int)rc;
	}
	if (rc == 0) {
		dprintf(D_ALWAYS, "Received an empty message instead of a file descriptor.\n");
		return -1;
	}
	cmsg = CMSG_FIRSTHDR(&msg);
	if (cmsg == NULL || cmsg->cmsg_type != SCM_RIGHTS) {
		dprintf(D_ALWAYS, "Message did not contain a file descriptor.\n");
		return -1;
	}
	memcpy(&fd, CMSG_DATA(cmsg), sizeof(fd));
	return fd;
}


//-------------------------------------------------------------
int open_tun(const char *tapname)
{
	int fd;
	if ((fd = open("/dev/net/tun", O_RDWR)) < 0) {
		dprintf(D_ALWAYS, "Unable to open /dev/net/tun device (errno=%d, %s).\n",
			errno, strerror(errno));
		return fd;
	}
	struct ifreq ifr;
	memset(&ifr, 0, sizeof(ifr));
		// Ask for a TUN device with no extra packet headers and enable
		// the multi-queue reading.
	ifr.ifr_flags = IFF_TUN | IFF_NO_PI | IFF_MULTI_QUEUE;
	strncpy(ifr.ifr_name, tapname, sizeof(ifr.ifr_name) - 1);
	if (ioctl(fd, TUNSETIFF, (void *)&ifr) < 0) {
		dprintf(D_ALWAYS, "Failed to create the new TUN device %s (errno=%d, %s).\n",
			tapname, errno, strerror(errno));
		close(fd);
		return -1;
	}
	return fd;
}


//-------------------------------------------------------------
int configure_nat()
{
	std::string boringtun_network;
	param(boringtun_network, "VPN_SERVER_NETWORK", "10.0.3.0/24");

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
int launch_boringtun()
{
	int uapi_sock[2];
	if (socketpair(AF_LOCAL, SOCK_STREAM, 0, uapi_sock) < 0) {
		dprintf(D_ALWAYS, "Failed to create local socket for boringtun communications (errno=%d, %s).\n",
			errno, strerror(errno));
		DC_Exit(1);
	}

	std::string boringtun_executable;
	param(boringtun_executable, "BORINGTUN", "/usr/bin/boringtun");

	std::string log_level;
	param(log_level, "BORINGTUN_DEBUG", "info");

	ArgList boringtun_args;
	boringtun_args.AppendArg("condor_boringtun");
	boringtun_args.AppendArg("-f");
	boringtun_args.AppendArg("condor0");

	std::string wg_uapi_fd;
	formatstr(wg_uapi_fd, "%d", uapi_sock[1]);
	std::string wg_tun_fd;
	formatstr(wg_tun_fd, "%d", g_tun_fd);

	Env boringtun_env;
	boringtun_env.SetEnv("WG_UAPI_FD", wg_uapi_fd);
	boringtun_env.SetEnv("WG_TUN_FD", wg_tun_fd);
	boringtun_env.SetEnv("WG_LOG_LEVEL", log_level);
		// Disables attempts at dropping privilege.
	boringtun_env.SetEnv("WG_SUDO", "1");

	int fd_inherit[3];
	fd_inherit[0] = g_tun_fd;
	fd_inherit[1] = uapi_sock[1];
	fd_inherit[2] = 0;

	auto pid = daemonCore->Create_Process(
		boringtun_executable.c_str(),
		boringtun_args,
		PRIV_CONDOR,
		g_boringtun_reaper,
		false,
		false,
		&boringtun_env,
		NULL,
		NULL,
		NULL,
		NULL,
		fd_inherit);

	close(uapi_sock[1]);
	if (pid) {
		g_boringtun_uapi_fd = uapi_sock[0];
		g_boringtun_pid = pid;
		return 0;
	} else {
		close(uapi_sock[0]);
		return 1;
	}
}


//-------------------------------------------------------------
std::string load_key_hex(const std::string &param_name)
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

	unsigned char *binary_key = nullptr;
	int binary_key_len = 0;
	condor_base64_decode(base64_key.c_str(), &binary_key, &binary_key_len, false);

	std::string hex_key;
	for (int index = 0; index < binary_key_len; index++) {
		formatstr_cat(hex_key, "%02x", static_cast<int>(binary_key[index]));
	}
	return hex_key;
}

//-------------------------------------------------------------
int boringtun_command(const std::string &command, std::unordered_map<std::string, std::string> response_map)
{
	int command_bytes = condor_write("boringtun UAPI",
		g_boringtun_uapi_fd, command.c_str(),
		command.size(), 20);
	if (command_bytes != static_cast<int>(command.size())) {
		dprintf(D_ALWAYS, "Failed to send configuration command to boringtun"
			" (errno=%d, %s).\n", errno, strerror(errno));
		return errno;
	}

	// Wait for response message.
	Selector selector;
	selector.add_fd( g_boringtun_uapi_fd, Selector::IO_READ );
	selector.set_timeout(20);
	selector.execute();
	if (selector.timed_out()) {
		dprintf(D_ALWAYS, "boringtun did not send back response.\n");
		return 1;
	}

	char command_resp[1024];
	auto resp_bytes = condor_read("boringtun UAPI", g_boringtun_uapi_fd,
		command_resp, 1024, 20, 0, true);
	if (resp_bytes <= 0) {
		dprintf(D_ALWAYS, "Failed to get response back from borington\n");
		return 1;
	}

	std::string response(command_resp, resp_bytes);
	StringList response_list(response, "\n");
	response_list.rewind();
	const char *response_line;
	int boring_errno = -1;
	while ( (response_line=response_list.next()) ) {
		std::string response_str(response_line);
		auto pos = response_str.find("=");
		if (pos == std::string::npos) {
			dprintf(D_FULLDEBUG, "boringtun sent an invalid line: %s\n", response_line);
		}
		auto key = response.substr(0, pos);
		auto val = response.substr(pos + 1);
		response_map[key] = val;
		if (key == "errno") {
			try {
				boring_errno = std::stol(val);
			} catch (...) {
				dprintf(D_ALWAYS, "boringtun sent an invalid errno: %s\n", val.c_str());
				return 1;
			}
		}
	}
	if (boring_errno < 0) {
		dprintf(D_ALWAYS, "boringtun did not respond with an errno.\n");
	}
	return boring_errno;
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
	auto boring_err = boringtun_command(command, response_map);

	dprintf(D_FULLDEBUG, "Result of boringtun configuration: %d\n", boring_err);
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
	auto euid = geteuid();
	auto egid = getegid();

		// Create the new namespaces we will work in.
	if (unshare(CLONE_NEWNET|CLONE_NEWUSER) < 0) {
		dprintf(D_ALWAYS, "Failed to create new network and user namespace - fatal!  Check your system configuration to ensure these are both enabled\n");
		return 1;
	}

		// Give ourselves root privileges in the new namespace; needed
		// to manipulate the iptables.
	auto setgroups_fd = open("/proc/self/setgroups", O_WRONLY);
	if (setgroups_fd < 0) {
		dprintf(D_ALWAYS, "Unable to open setgroups file (errno=%d, %s)\n",
			errno, strerror(errno));
		return 1;
	}
	char deny[] = "deny";
	if (4 != full_write(setgroups_fd, deny, 4)) {
		dprintf(D_ALWAYS, "Unable to set setgroups policy (errno=%d, %s)\n",
			errno, strerror(errno));
		return 1;
	}
	close(setgroups_fd);

	std::string uid_map;
	formatstr( uid_map, "0 %d 1", euid);
	auto uid_map_fd = open("/proc/self/uid_map", O_WRONLY);
	if (uid_map_fd < 0) {
		dprintf(D_ALWAYS, "Unable to open uid_map (errno=%d, %s)\n",
			errno, strerror(errno));
		return 1;
	}
	if (static_cast<int>(uid_map.size()) != full_write(uid_map_fd, uid_map.c_str(), uid_map.size())) {
		dprintf(D_ALWAYS, "Unable to create UID map (errno=%d, %s)\n",
			errno, strerror(errno));
		return 1;
	}
	close(uid_map_fd);

	std::string gid_map;
	formatstr( gid_map, "0 %d 1", egid);
	auto gid_map_fd = open("/proc/self/gid_map", O_WRONLY);
	if (gid_map_fd < 0) {
		dprintf(D_ALWAYS, "Unable to open gid_map (errno=%d, %s)\n",
			errno, strerror(errno));
		return 1;
	}
	if (static_cast<int>(gid_map.size()) != full_write(gid_map_fd, gid_map.c_str(), gid_map.size())) {
		dprintf(D_ALWAYS, "Unable to create GID map (errno=%d, %s)\n",
			errno, strerror(errno));
		return 1;
	}
	close(gid_map_fd);

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

	main_config();
}

//-------------------------------------------------------------

void 
main_config()
{
	dprintf(D_FULLDEBUG, "main_config() called\n");

	// Setup boringtun - connectivity for clients
	//
	if (g_boringtun_pid == -1) {
		if (launch_boringtun()) {
			dprintf(D_ALWAYS, "Failed to launch boringtun; fatal error.\n");
			DC_Exit(1);
		}
		if (configure_boringtun()) {
			dprintf(D_ALWAYS, "Failed to configure boringtun; fatal error.\n");
			DC_Exit(1);
		}
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
	set_mySubSystem("VPN", SUBSYSTEM_TYPE_DAEMON );

	dc_main_init = main_init;
	dc_main_config = main_config;
	dc_main_shutdown_fast = main_shutdown_fast;
	dc_main_shutdown_graceful = main_shutdown_graceful;
	return dc_main( argc, argv );
}
