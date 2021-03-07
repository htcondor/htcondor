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
#include "condor_config.h"
#include "stl_string_utils.h"
#include "classad/classad.h"
#include "CondorError.h"
#include "condor_crypt.h"
#include "condor_base64.h"
#include "vpn_common.h"
#include "vpn_lease_mgr.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <sstream>

VPNLeaseMgr::VPNLeaseMgr(in_addr_t network, in_addr_t netmask, int boringtun_sock)
	: m_network(network),
	  m_netmask(netmask),
	  m_boringtun_sock(boringtun_sock),
	  m_lease_lifetime(param_integer("VPN_LEASE_LIFETIME", 900))
{}


in_addr_t
VPNLeaseMgr::NextIP()
{
	in_addr_t result = htonl(ntohl(m_network) + (m_offset & ~ntohl(m_netmask)));
	m_offset += 1;

		// Avoid the GW address
	if (result == htonl(ntohl(m_network) + 2)) {return NextIP();}
		// Avoid the network address
	if (result == m_network) {return NextIP();}
		// Avoid the broadcast address.
	in_addr_t broadcast_addr = htonl((ntohl(m_network) & ntohl(m_netmask)) + ~ntohl(m_netmask));
	if (result == broadcast_addr) {return NextIP();}

	return result;
}


bool
VPNLeaseMgr::CreateLease(const std::string &base64_pubkey, classad::ClassAd &respAd, CondorError & err)
{
        struct in_addr gw_addr;
        gw_addr.s_addr = htonl(ntohl(m_network) + 1);
        struct in_addr host_addr;
	std::unordered_map<in_addr_t, uint64_t>::const_iterator iter;
		// Keep looking for an IP address until we find a free one.
	in_addr_t first_ip = NextIP();
	host_addr.s_addr = first_ip;
	iter = m_ip_to_id_map.find(host_addr.s_addr);
	while (iter != m_ip_to_id_map.end()) {
        	host_addr.s_addr = NextIP();
		if (host_addr.s_addr == first_ip) {
			dprintf(D_ALWAYS, "IPAM: Out of IP addresses!\n");
			err.pushf("LEASE", 2, "Out of assignable IP addresses");
			return false;
		}
		iter = m_ip_to_id_map.find(host_addr.s_addr);
	};

        std::string gw_str(inet_ntoa(gw_addr));
        std::string host_str(inet_ntoa(host_addr));
        struct in_addr tmpaddr;
        tmpaddr.s_addr = m_network;
        std::string net_str = inet_ntoa(tmpaddr);
        tmpaddr.s_addr = m_netmask;
        std::string netmask_str = inet_ntoa(tmpaddr);

        respAd.InsertAttr("IpAddr", host_str);
        respAd.InsertAttr("GW", gw_str);
        respAd.InsertAttr("Network", net_str);
        respAd.InsertAttr("Netmask", netmask_str);
	respAd.InsertAttr("LeaseLifetime", static_cast<int>(m_lease_lifetime));

	std::string command;
	formatstr(command,
		"set=1\n"
		"public_key=%s\n"
		"persistent_keepalive_interval=30\n"
		"allowed_ip=%s/32\n"
		"\n",
		base64_key_to_hex(base64_pubkey).c_str(),
		host_str.c_str());

        std::unordered_map<std::string, std::string> response_map;
	dprintf(D_FULLDEBUG, "Attempting client registration:\n%s\n", command.c_str());
        auto boring_err = boringtun_command(m_boringtun_sock, command, response_map);
        dprintf(D_FULLDEBUG, "Result of client %s registration with boringtun: %d\n", base64_pubkey.c_str(), boring_err);
	if (boring_err) {
		err.pushf("BORINGTUN", boring_err, "Client registration with boringtun failed: %s", strerror(boring_err));
		return false;
	}

	union {
		unsigned char rand_bytes[8];
		uint64_t lease_id;
	} lease_id;

	std::unique_ptr<unsigned char, decltype(&free)> new_rand_bytes(Condor_Crypt_Base::randomKey(8), &free);
	memcpy(lease_id.rand_bytes, new_rand_bytes.get(), sizeof(lease_id));

	Lease lease;
	lease.m_expiry = time(NULL) + m_lease_lifetime;
	lease.m_pubkey = base64_pubkey;
	lease.m_ip = host_addr.s_addr;

	auto rval = m_id_to_lease_map.insert(std::pair<uint64_t, Lease>(lease_id.lease_id, lease));
	if (!rval.second) {
		err.pushf("LEASE", 1, "Internal error: generated lease already existed");
		return false;
	}
	m_ip_to_id_map.insert(std::pair<in_addr_t, uint64_t>(host_addr.s_addr, lease_id.lease_id));

	std::unique_ptr<char, decltype(&free)> base64_lease(
		condor_base64_encode(lease_id.rand_bytes, sizeof(lease_id), false),
		&free);

	respAd.InsertAttr("Lease", base64_lease.get());

	m_lifetime_clients_created++;

	return true;
}


bool
VPNLeaseMgr::RefreshLease(const std::string &base64_lease_id, unsigned &lifetime, CondorError &err)
{
	unsigned char *binary_key = nullptr;
	int binary_key_len = 0;
	condor_base64_decode(base64_lease_id.c_str(), &binary_key, &binary_key_len, false);
	if (binary_key_len != 8) {
		err.push("LEASE", 3, "Invalid lease ID length");
		return false;
	}

	union {
		unsigned char lease_bytes[8];
		uint64_t lease_id;
	} lease_id;

	memcpy(lease_id.lease_bytes, binary_key, 8);

	auto iter = m_id_to_lease_map.find(lease_id.lease_id);
	if (iter == m_id_to_lease_map.end()) {
		err.pushf("LEASE", 4, "Unknown lease: %s", base64_lease_id.c_str());
		return false;
	}

	iter->second.m_expiry = time(NULL) + m_lease_lifetime;
	lifetime = m_lease_lifetime;

	m_lifetime_client_heartbeats++;

	return true;
}


bool
VPNLeaseMgr::RemoveLease(const std::string &base64_lease_id, CondorError &err)
{
	unsigned char *binary_key = nullptr;
	int binary_key_len = 0;
	condor_base64_decode(base64_lease_id.c_str(), &binary_key, &binary_key_len, false);
	if (binary_key_len != 8) {
		err.push("LEASE", 3, "Invalid lease ID length");
		return false;
	}

	union {
		unsigned char lease_bytes[8];
		uint64_t lease_id;
	} lease_id;

	memcpy(lease_id.lease_bytes, binary_key, 8);

	auto iter = m_id_to_lease_map.find(lease_id.lease_id);
	if (iter == m_id_to_lease_map.end()) {
		err.pushf("LEASE", 4, "Unknown lease: %s", base64_lease_id.c_str());
		return false;
	}
	in_addr_t ip = iter->second.m_ip;
	std::string pubkey(std::move(iter->second.m_pubkey));

	m_id_to_lease_map.erase(iter);

	auto iter2 = m_ip_to_id_map.find(ip);
	if (iter2 != m_ip_to_id_map.end()) {m_ip_to_id_map.erase(iter2);}

	std::string command;
	formatstr(command, "set=1\n"
		"public_key=%s\n"
		"remove=true\n"
		"\n",
		base64_key_to_hex(pubkey).c_str());

	std::unordered_map<std::string, std::string> response_map;
	dprintf(D_FULLDEBUG, "Attempting client removal:\n%s\n", command.c_str());
	auto boring_err = boringtun_command(m_boringtun_sock, command.c_str(), response_map);
	dprintf(D_FULLDEBUG, "Result of client removal with boringtun: %d\n", boring_err);
	if (boring_err) {
		dprintf(D_ALWAYS, "Failed to remove public key %s from boringtun\n", pubkey.c_str());
	}

	m_lifetime_clients_removed++;

	return true;
}


void
VPNLeaseMgr::Maintenance()
{
	m_lease_lifetime = param_integer("VPN_LEASE_LIFETIME", 900);

	std::vector<uint64_t> expired_leases;
	auto now = time(NULL);
	for (const auto & value : m_id_to_lease_map)
	{
		if (value.second.m_expiry < now) {
			expired_leases.push_back(value.first);
		}
	}

	std::vector<std::string> pubkeys;
	for (auto id : expired_leases) {
		dprintf(D_FULLDEBUG, "Removing expired lease %lu\n", id);
		auto iter = m_id_to_lease_map.find(id);
		auto ip = iter->second.m_ip;
		pubkeys.emplace_back(std::move(iter->second.m_pubkey));
		if (iter != m_id_to_lease_map.end()) {m_id_to_lease_map.erase(iter);}
		auto iter2 = m_ip_to_id_map.find(ip);
		if (iter2 != m_ip_to_id_map.end()) {m_ip_to_id_map.erase(iter2);}

		m_lifetime_clients_expired++;
	}

	if (!pubkeys.empty()) {
		std::stringstream ss;
		ss << "set=1\n";
		for (const auto &pubkey : pubkeys) {
			ss << "public_key=" << base64_key_to_hex(pubkey).c_str() << std::endl;
			ss << "remove=true\n";
		}
		ss << "\n";

		std::unordered_map<std::string, std::string> response_map;
		dprintf(D_FULLDEBUG, "Attempting client removal:\n%s\n", ss.str().c_str());
		auto boring_err = boringtun_command(m_boringtun_sock, ss.str(), response_map);
		dprintf(D_FULLDEBUG, "Result of client removal for maintenance with boringtun: %d\n", boring_err);
		if (boring_err) {
			dprintf(D_ALWAYS, "Failed to remove %lu public keys from boringtun\n", pubkeys.size());
		}
	}
}


void
VPNLeaseMgr::UpdateStats(classad::ClassAd &ad)
{
	ad.InsertAttr("LifetimeClientsCreated", static_cast<long long>(m_lifetime_clients_created));
	ad.InsertAttr("LifetimeClientsRemoved", static_cast<long long>(m_lifetime_clients_removed));
	ad.InsertAttr("LifetimeClientsExpired", static_cast<long long>(m_lifetime_clients_expired));
	ad.InsertAttr("LifetimeClientHeartbeats", static_cast<long long>(m_lifetime_client_heartbeats));
	ad.InsertAttr("ClientsActive", static_cast<long>(m_ip_to_id_map.size()));
}
