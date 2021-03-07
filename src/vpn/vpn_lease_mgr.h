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

#include <string>
#include <unordered_map>
#include <time.h>
#include <arpa/inet.h>

// Forward decl's
namespace classad {
	class ClassAd;
}

class CondorError;

///
// VPNLeaseMgr is designed to be a singleton that manages the status of
// client leases that are connected to the server..
//

class VPNLeaseMgr {
public:
	VPNLeaseMgr(in_addr_t network, in_addr_t netmask, int boringtun_sock);

		// Create a new lease; return the lease ID and update a given ClassAd
		// with the lease information to send back to the client.
	bool CreateLease(const std::string &pub, classad::ClassAd &ad, CondorError &err);

		// Refresh the lease's timeout internally.
	bool RefreshLease(const std::string &id, unsigned &lifetime, CondorError &err);

		// Remove lease from the pool
	bool RemoveLease(const std::string &id, CondorError &err);

		// Periodic maintenance
	void Maintenance();

		// Stats for daemon ClassAd
	void UpdateStats(classad::ClassAd &ad);
private:
	in_addr_t NextIP();

// State for IP address management
	in_addr_t m_offset{100};
	in_addr_t m_network;
	in_addr_t m_netmask;

// UAPI Connection to a boringtun tunnel
	int m_boringtun_sock{-1};

// Lifetime of the leases
	unsigned int m_lease_lifetime{900};

// Lease hash structures
	struct Lease {
		time_t m_expiry{0};
		std::string m_pubkey;
		in_addr_t m_ip{0};
	};

	std::unordered_map<uint64_t, Lease> m_id_to_lease_map;
	std::unordered_map<in_addr_t, uint64_t> m_ip_to_id_map;

// Statistics
	uint64_t m_lifetime_clients_created{0};
	uint64_t m_lifetime_clients_removed{0};
	uint64_t m_lifetime_clients_expired{0};
	uint64_t m_lifetime_client_heartbeats{0};
};
