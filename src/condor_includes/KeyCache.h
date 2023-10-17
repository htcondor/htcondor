/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
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


#ifndef CONDOR_KEYCACHE_H_INCLUDE
#define CONDOR_KEYCACHE_H_INCLUDE

#include "condor_common.h"
#include "condor_classad.h"
#include "CryptKey.h"

class KeyCacheEntry {
 public:
    KeyCacheEntry(
			const std::string& id,
			const std::string& addr,
			std::vector<KeyInfo> key,
			const ClassAd & policy,
			time_t expiration,
			int session_lease
			);

	const std::string&    id() const { return _id; }
	const std::string&    addr() const { return _addr; }
    KeyInfo*              key();
    KeyInfo*              key(Protocol protocol);
    ClassAd*              policy();
    time_t                expiration() const;
	char const *          expirationType() const;
	void                  setExpiration(time_t new_expiration);
	void                  setLingerFlag(bool flag) { _lingering = flag; }
	bool                  getLingerFlag() const { return _lingering; }
	bool                  setPreferredProtocol(Protocol preferred);
	void                  setLastPeerVersion(const std::string& version) { _last_peer_version = version; }
	std::string           getLastPeerVersion() const { return _last_peer_version; }

	void                  renewLease();
 private:

	std::string           _id;
	std::string           _addr;
	std::vector<KeyInfo>  _keys;
    ClassAd              _policy;
    time_t               _expiration;
	int                  _lease_interval;   // max seconds of unused time
	time_t               _lease_expiration; // time of lease expiration
	bool                 _lingering; // true if session only exists
	                                 // to catch lingering communication
	Protocol             _preferred_protocol;
	std::string          _last_peer_version;
};


using KeyCache = std::map<std::string, KeyCacheEntry, std::less<>>;


#endif
