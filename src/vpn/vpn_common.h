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

#pragma once

#ifdef LINUX

#include "wireguard_ffi.h"

#include <string>
#include <unordered_map>

// Create a new network device, tunname, of type TUN.
// Returns non-zero on failure
int open_tun(const char *tunname);

/// Launch boringtun, listening on a given FD.
//
// Input args:
// - tun_fd: File descriptor associated with a TUN device.
// - boringtun_reaper: DaemonCore process reaper ID to associate with boringtun's process.
//
// Output args:
// - uapi_fd: FD associated with the boringtun API.
// - boringtun_pid: PID of the boringtun process.
//
// Returns non-zero on failure.
int launch_boringtun(int tun_fd, int boringtun_reaper, int &uapi_fd, pid_t &boringtun_pid);

/// Send a command to a boringtun process on a given user API socket.
int boringtun_command(int uapi_fd, const std::string &command, std::unordered_map<std::string, std::string> &response_map);

/// Configure TUN devide
int configure_tun(const char *tunname, in_addr_t ipaddr, in_addr_t gw, in_addr_t netmask);

/// Helper function - turn a base64-encoded key into a hex-encoded key
std::string base64_key_to_hex(const std::string &base64_key);

/// Send a file descriptor to a connected socket.
int sendfd(int sock, int fd);

/// Receive a FD from a socket.
int recvfd(int sock);

/// Setup UID mapping inside the user namespace.
//    Makes it so `id` returns a desired value.
//  - req_uid: The requested UID; this is the apparent UID of the process in the namespace after mapping.
//  - req_gid: The requested GID
//  - parent_uid / parent_gid: The UID / GID in the parent namespace
//  - returns: non-zero on failure.
//  - The special value of `-1` for req_uid / req_gid indicates to map to the UID/GID in the parent namespace.
int setup_uidgid_map(int req_uid, int req_gid, int parent_uid, int parent_gid);

/// Generate a X25519 private key
bool vpn_generate_x25519_secret_key(struct x25519_key &privkey);

/// Derive a X25519 public key from a private key
bool vpn_generate_x25519_pubkey(struct x25519_key &privkey, struct x25519_key &pubbkey);

/// Convert a X25519 key to base64
const char *vpn_x25519_key_to_base64(struct x25519_key &pubkey);

/// Free memory from the base64 representation of a X25519 key
void vpn_x25519_key_to_str_free(const char *base64_pubkey);

#endif // LINUX
