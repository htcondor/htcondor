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


#include "condor_common.h"
#include "KeyCache.h"
#include "CryptKey.h"
#include "condor_attributes.h"
//#include "internet.h"

KeyCacheEntry::KeyCacheEntry( const std::string& id_param, const std::string& addr_param, std::vector<KeyInfo> key_param, const ClassAd& policy_param, time_t expiration_param, int lease_interval )
	: _id(id_param)
	, _addr(addr_param)
	, _keys(key_param)
	, _policy(policy_param)
	, _expiration(expiration_param)
	, _lease_interval(lease_interval)
	, _lease_expiration(0)
	, _lingering(false)
{
	if (_keys.empty()) {
		_preferred_protocol = CONDOR_NO_PROTOCOL;
	} else {
		_preferred_protocol = _keys[0].getProtocol();
	}

	renewLease();
}

KeyInfo* KeyCacheEntry::key() {
	return key(_preferred_protocol);
}

KeyInfo* KeyCacheEntry::key(Protocol protocol) {
	for ( auto& key: _keys ) {
		if ( key.getProtocol() == protocol ) {
			return &key;
		}
	}
	return NULL;
}

ClassAd* KeyCacheEntry::policy() {
	return &_policy;
}

time_t KeyCacheEntry::expiration() const {
		// Return the sooner of the two expiration timestamps.
		// A 0 timestamp indicates no expiration.
	if( _expiration ) {
		if( _lease_expiration ) {
			if( _lease_expiration < _expiration ) {
				return _lease_expiration;
			}
		}
		return _expiration;
	}
	return _lease_expiration;
}

char const *KeyCacheEntry::expirationType() const {
	if( _lease_expiration && (_lease_expiration < _expiration || !_expiration) ) {
		return "lease";
	}
	if( _expiration ) {
		return "lifetime";
	}
	return "";
}

void KeyCacheEntry::setExpiration(time_t new_expiration) {
	_expiration = new_expiration;
}

bool KeyCacheEntry::setPreferredProtocol(Protocol preferred)
{
	for (auto& key : _keys) {
		if (key.getProtocol() == preferred) {
			_preferred_protocol = preferred;
			return true;
		}
	}
	return false;
}

void KeyCacheEntry::renewLease() {
	if( _lease_interval ) {
		_lease_expiration = time(0) + _lease_interval;
	}
}

