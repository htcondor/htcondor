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
#include "condor_config.h"
#include "condor_debug.h"
#include "condor_arglist.h"
#include "env.h"
#include "condor_daemon_core.h"
#include "condor_rw.h"
#include "selector.h"
#include "condor_base64.h"

#include <linux/if_tun.h>
#include <net/route.h>
#include <net/if.h>


//-------------------------------------------------------------
int open_tun(const char *tunname)
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
        strncpy(ifr.ifr_name, tunname, sizeof(ifr.ifr_name) - 1);
        if (ioctl(fd, TUNSETIFF, (void *)&ifr) < 0) {
                dprintf(D_ALWAYS, "Failed to create the new TUN device %s (errno=%d, %s).\n",
                        tunname, errno, strerror(errno));
                close(fd);
                return -1;
        }
        return fd;
}


//-------------------------------------------------------------
int launch_boringtun(int tun_fd, int boringtun_reaper, int &uapi_fd, pid_t &boringtun_pid)
{
        int uapi_sock[2];
        if (socketpair(AF_LOCAL, SOCK_STREAM, 0, uapi_sock) < 0) {
                dprintf(D_ALWAYS, "Failed to create local socket for boringtun communications (errno=%d, %s).\n",
                        errno, strerror(errno));
                return 1;
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
        formatstr(wg_tun_fd, "%d", tun_fd);

        Env boringtun_env;
        boringtun_env.SetEnv("WG_UAPI_FD", wg_uapi_fd);
        boringtun_env.SetEnv("WG_TUN_FD", wg_tun_fd);
        boringtun_env.SetEnv("WG_LOG_LEVEL", log_level);
                // Disables attempts at dropping privilege.
        boringtun_env.SetEnv("WG_SUDO", "1");

        int fd_inherit[3];
        fd_inherit[0] = tun_fd;
        fd_inherit[1] = uapi_sock[1];
        fd_inherit[2] = 0;

        auto pid = daemonCore->Create_Process(
                boringtun_executable.c_str(),
                boringtun_args,
                PRIV_CONDOR,
                boringtun_reaper,
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
                uapi_fd = uapi_sock[0];
                boringtun_pid = pid;
                return 0;
        } else {
                close(uapi_sock[0]);
                return 1;
        }
}


//-------------------------------------------------------------
/// Configure TUN devide
int configure_tun(const char *tunname, in_addr_t ipaddr, in_addr_t gw, in_addr_t netmask)
{
	auto sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0) {
		dprintf(D_ALWAYS, "Unable to open TUN FD (errno=%d, %s).\n", errno, strerror(errno));
		return -1;
	}

        // set loopback device to UP
	struct ifreq ifr_lo;
	memset(&ifr_lo, 0, sizeof(struct ifreq));
	strncpy(ifr_lo.ifr_name, "lo", sizeof(ifr_lo.ifr_name) - 1);
	ifr_lo.ifr_flags = IFF_UP | IFF_RUNNING;
	if (ioctl(sockfd, SIOCSIFFLAGS, &ifr_lo) < 0) {
		dprintf(D_ALWAYS, "Cannot set loopback device to up (errno=%d, %s).\n",
			errno, strerror(errno));
		return -1;
	}

	struct ifreq ifr;
	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_flags = IFF_UP | IFF_RUNNING;
	strncpy(ifr.ifr_name, tunname, sizeof(ifr.ifr_name) - 1);

	if (ioctl(sockfd, SIOCSIFFLAGS, &ifr) < 0) {
		dprintf(D_ALWAYS, "Cannot set TUN device %s up (errno=%d, %s)\n",
			tunname, errno, strerror(errno));
		return -1;
	}

	int mtu = param_integer("VPN_MTU", 1500);
	ifr.ifr_mtu = mtu;
	if (ioctl(sockfd, SIOCSIFMTU, &ifr) < 0) {
		dprintf(D_ALWAYS, "Cannot set MTU for TUN device %s.\n", tunname);
		return -1;
	}

	struct sockaddr_in *sai = (struct sockaddr_in *)&ifr.ifr_addr;
	sai->sin_family = AF_INET;
	sai->sin_port = 0;
	in_addr my_addr;
	my_addr.s_addr = ipaddr;
	sai->sin_addr = my_addr;

	if (ioctl(sockfd, SIOCSIFADDR, &ifr) < 0) {
		dprintf(D_ALWAYS, "Cannot set device address (errno=%d, %s).\n", errno, strerror(errno));
		return -1;
	}

	my_addr.s_addr = netmask;
	sai->sin_addr = my_addr;
	if (ioctl(sockfd, SIOCSIFNETMASK, &ifr) < 0) {
		dprintf(D_ALWAYS, "Cannot set netmask for %s (errno=%d, %s).\n",
			tunname, errno, strerror(errno));
		return -1;
	}

	if (gw) {
		my_addr.s_addr = gw;
		struct rtentry route;
		memset(&route, 0, sizeof(route));
		sai = (struct sockaddr_in *)&route.rt_gateway;
		sai->sin_family = AF_INET;
		sai->sin_addr = my_addr;
		sai = (struct sockaddr_in *)&route.rt_dst;
		sai->sin_family = AF_INET;
		sai->sin_addr.s_addr = INADDR_ANY;
		sai = (struct sockaddr_in *)&route.rt_genmask;
		sai->sin_family = AF_INET;
		sai->sin_addr.s_addr = INADDR_ANY;

		route.rt_flags = RTF_UP | RTF_GATEWAY;
		route.rt_metric = 0;
		char *tunname_dup = strdup(tunname);
		route.rt_dev = tunname_dup;

		if (ioctl(sockfd, SIOCADDRT, &route) < 0) {
			dprintf(D_ALWAYS, "Failed to set default route.\n");
			free(tunname_dup);
			return -1;
		}
		free(tunname_dup);
	}
	return 0;
}


//-------------------------------------------------------------
std::string base64_key_to_hex(const std::string &base64_key)
{
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
int boringtun_command(int uapi_fd, const std::string &command, std::unordered_map<std::string, std::string> &response_map)
{       
        int command_bytes = condor_write("boringtun UAPI",
                uapi_fd, command.c_str(),
                command.size(), 20);
        if (command_bytes != static_cast<int>(command.size())) {
                dprintf(D_ALWAYS, "Failed to send configuration command to boringtun"
                        " (errno=%d, %s).\n", errno, strerror(errno));
                return errno;
        }
	//dprintf(D_ALWAYS, "boringtun_command: %s\n", command.c_str());
    
        // Wait for response message.
        Selector selector;
        selector.add_fd(uapi_fd, Selector::IO_READ);
        selector.set_timeout(20);
        selector.execute();
        if (selector.timed_out()) {
                dprintf(D_ALWAYS, "boringtun did not send back response.\n");
                return 1;
        }
        
        char command_resp[1024];
        auto resp_bytes = condor_read("boringtun UAPI", uapi_fd,
                command_resp, 1024, 20, 0, true);
        if (resp_bytes <= 0) {
                dprintf(D_ALWAYS, "Failed to get response back from borington\n");
                return 1;
        }
        
        std::string response(command_resp, resp_bytes);
	//dprintf(D_ALWAYS, "boringtun_response: %s\n", response.c_str());
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
                auto key = response_str.substr(0, pos);
                auto val = response_str.substr(pos + 1);
                response_map[key] = val;
		dprintf(D_FULLDEBUG, "boringtun returned key=%s, val=%s\n", key.c_str(), val.c_str());
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
int recvfd(int sock)
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
