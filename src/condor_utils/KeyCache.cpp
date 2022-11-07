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
#include "internet.h"

KeyCacheEntry::KeyCacheEntry( const std::string& id_param, const std::string& addr_param, const KeyInfo* key_param, const ClassAd * policy_param, int expiration_param, int lease_interval )
	: _id(id_param)
	, _addr(addr_param)
{
	if (key_param) {
		_keys.push_back(new KeyInfo(*key_param));
		_preferred_protocol = key_param->getProtocol();
	} else {
		_preferred_protocol = CONDOR_NO_PROTOCOL;
	}

	if (policy_param) {
		_policy = new ClassAd(*policy_param);
	} else {
		_policy = NULL;
	}

	_expiration = expiration_param;
	_lease_interval = lease_interval;
	_lease_expiration = 0;
	_lingering = false;
	renewLease();
}

// NOTE: In this constructor, we assume ownership of the KeyInfo objects
//   in the vector. In the single-KeyInfo constructor, we copy it.
KeyCacheEntry::KeyCacheEntry( const std::string& id_param, const std::string& addr_param, std::vector<KeyInfo*> key_param, const ClassAd * policy_param, int expiration_param, int lease_interval )
	: _id(id_param)
	, _addr(addr_param)
{
	_keys = key_param;
	if (_keys.empty()) {
		_preferred_protocol = CONDOR_NO_PROTOCOL;
	} else {
		_preferred_protocol = _keys[0]->getProtocol();
	}

	if (policy_param) {
		_policy = new ClassAd(*policy_param);
	} else {
		_policy = NULL;
	}

	_expiration = expiration_param;
	_lease_interval = lease_interval;
	_lease_expiration = 0;
	_lingering = false;
	renewLease();
}

KeyCacheEntry::KeyCacheEntry(const KeyCacheEntry& copy) 
{
	copy_storage(copy);
}

KeyCacheEntry::~KeyCacheEntry() {
	delete_storage();
}

const KeyCacheEntry& KeyCacheEntry::operator=(const KeyCacheEntry &copy) {
	if (this != &copy) {
		delete_storage();
		copy_storage(copy);
	}
	return *this;
}

KeyInfo* KeyCacheEntry::key() {
	return key(_preferred_protocol);
}

KeyInfo* KeyCacheEntry::key(Protocol protocol) {
	for ( auto key: _keys ) {
		if ( key->getProtocol() == protocol ) {
			return key;
		}
	}
	return NULL;
}

ClassAd* KeyCacheEntry::policy() {
	return _policy;
}

int KeyCacheEntry::expiration() const {
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

void KeyCacheEntry::setExpiration(int new_expiration) {
	_expiration = new_expiration;
}

bool KeyCacheEntry::setPreferredProtocol(Protocol preferred)
{
	for (auto key : _keys) {
		if (key->getProtocol() == preferred) {
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

void KeyCacheEntry::copy_storage(const KeyCacheEntry &copy) {
	_id = copy._id;
	_addr = copy._addr;

	for (auto key : copy._keys) {
		_keys.push_back(new KeyInfo(*key));
	}

	if (copy._policy) {
		_policy = new ClassAd(*(copy._policy));
	} else {
		_policy = NULL;
	}

	_expiration = copy._expiration;
	_lease_interval = copy._lease_interval;
	_lease_expiration = copy._lease_expiration;
	_lingering = copy._lingering;
	_preferred_protocol = copy._preferred_protocol;
}


void KeyCacheEntry::delete_storage() {
	for (auto key : _keys) {
		delete key;
	}
	if (_policy) {
	  delete _policy;
	}
}


KeyCache::KeyCache() {
	key_table = new HashTable<std::string, KeyCacheEntry*>(hashFunction);
	dprintf ( D_SECURITY|D_FULLDEBUG, "KEYCACHE: created: %p\n", key_table );
}

KeyCache::KeyCache(const KeyCache& k) {
	key_table = new HashTable<std::string, KeyCacheEntry*>(hashFunction);
	copy_storage(k);
}

KeyCache::~KeyCache() {
	delete_storage();
	delete key_table;
}
	    
const KeyCache& KeyCache::operator=(const KeyCache& k) {
	if (this != &k) {
		delete_storage();
		copy_storage(k);
	}
	return *this;
}


void KeyCache::copy_storage(const KeyCache &copy) {
	dprintf ( D_SECURITY|D_FULLDEBUG, "KEYCACHE: created: %p\n", key_table );

	// manually iterate all entries from the hash.  they are
	// pointers, and we need to copy that object.
	KeyCacheEntry* key_entry;
	copy.key_table->startIterations();
	while (copy.key_table->iterate(key_entry)) {
		insert(*key_entry);
	}
}


void KeyCache::delete_storage()
{
	if( key_table ) {
			// Delete all entries from the hash
		KeyCacheEntry* key_entry;
		key_table->startIterations();
		while( key_table->iterate(key_entry) ) {
			if( key_entry ) {
				delete key_entry;
			}
		}
		key_table->clear();
		//dprintf( D_SECURITY|D_FULLDEBUG, "KEYCACHE: deleted: %p\n", key_table );
	}
}


void KeyCache::clear()
{
	delete_storage();
}

bool KeyCache::insert(KeyCacheEntry &e) {

	// the key_table member is a HashTable which maps
	// std::string's to KeyCacheEntry*'s.  (note the '*')

	// create a new entry
	KeyCacheEntry *new_ent = new KeyCacheEntry(e);

	// stick a pointer to the entry in the table
	// NOTE: HashTable's insert returns ZERO on SUCCESS!!!
	bool retval = (key_table->insert(new_ent->id(), new_ent) == 0);

	if (!retval) {
		// key was not inserted... delete
		delete new_ent;
	}

	return retval;
}

bool KeyCache::lookup(const char *key_id, KeyCacheEntry *&e_ptr) {
	// A NULL key_id is not valid
	if (!key_id) return false;

	// use a temp pointer so that e_ptr is not modified
	// if a match is not found

	KeyCacheEntry *tmp_ptr = NULL;

	// NOTE: HashTable's lookup returns ZERO on SUCCESS
	bool res = (key_table->lookup(key_id, tmp_ptr) == 0);

	if (res) {
		// hand over the pointer
		e_ptr = tmp_ptr;
	}

	return res;
}

bool KeyCache::remove(const char *key_id) {
	// A NULL key_id is not valid
	if (!key_id) return false;

	// to remove a key:
	// you first need to do a lookup, so we can get the pointer to delete.
	KeyCacheEntry *tmp_ptr = NULL;

	// NOTE: HashTable's lookup returns ZERO on SUCCESS
	bool res = (key_table->lookup(key_id, tmp_ptr) == 0);

	if (res) {
		// ** HEY **
		// key_id could be pointing to the string tmp_ptr->id.  so, we'd
		// better finish using key_id *before* we delete tmp_ptr.
		res = (key_table->remove(key_id) == 0);
		delete tmp_ptr;
	}

	return res;
}

void KeyCache::expire(KeyCacheEntry *e) {
	time_t key_exp = e->expiration();
	char const *expiration_type = e->expirationType();

	dprintf (D_SECURITY|D_FULLDEBUG, "KEYCACHE: Session %s %s expired at %s\n", e->id().c_str(), expiration_type, ctime(&key_exp) );

	// remove its reference from the hash table
	remove(e->id().c_str());       // This should do it
}

StringList * KeyCache::getExpiredKeys() {

	// draw the line
    StringList * list = new StringList();
	time_t cutoff_time = time(0);

	// iterate through all entries from the hash
	KeyCacheEntry* key_entry;
	std::string id;
	key_table->startIterations();
	while (key_table->iterate(id, key_entry)) {
		// check the freshness date on that key
		if (key_entry->expiration() && key_entry->expiration() <= cutoff_time) {
            list->append(id.c_str());
			//expire(key_entry);
		}
	}
    return list;
}

int KeyCache::count() {
	ASSERT( key_table );
	return key_table->getNumElements();
}
