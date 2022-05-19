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

KeyCacheEntry::KeyCacheEntry( char const *id_param, const condor_sockaddr * addr_param, const KeyInfo* key_param, const ClassAd * policy_param, int expiration_param, int lease_interval ) {
	if (id_param) {
		_id = strdup(id_param);
	} else {
		_id = NULL;
	}

	if (addr_param) {
		_addr = new condor_sockaddr(*addr_param);
	} else {
		_addr = NULL;
	}

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
KeyCacheEntry::KeyCacheEntry( char const *id_param, const condor_sockaddr * addr_param, std::vector<KeyInfo*> key_param, const ClassAd * policy_param, int expiration_param, int lease_interval ) {
	if (id_param) {
		_id = strdup(id_param);
	} else {
		_id = NULL;
	}

	if (addr_param) {
		_addr = new condor_sockaddr(*addr_param);
	} else {
		_addr = NULL;
	}

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

char* KeyCacheEntry::id() {
	return _id;
}

const condor_sockaddr*  KeyCacheEntry::addr() {
	return _addr;
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
	if (copy._id) {
		_id = strdup(copy._id);
	} else {
		_id = NULL;
	}

	if (copy._addr) {
    	_addr = new condor_sockaddr(*(copy._addr));
	} else {
    	_addr = NULL;
	}

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
	if (_id) {
        free( _id );
	}
	if (_addr) {
	  delete _addr;
	}
	for (auto key : _keys) {
		delete key;
	}
	if (_policy) {
	  delete _policy;
	}
}


KeyCache::KeyCache() {
	key_table = new HashTable<std::string, KeyCacheEntry*>(hashFunction);
	m_index = new KeyCacheIndex(hashFunction);
	dprintf ( D_SECURITY|D_FULLDEBUG, "KEYCACHE: created: %p\n", key_table );
}

KeyCache::KeyCache(const KeyCache& k) {
	key_table = new HashTable<std::string, KeyCacheEntry*>(hashFunction);
	m_index = new KeyCacheIndex(hashFunction);
	copy_storage(k);
}

KeyCache::~KeyCache() {
	delete_storage();
	delete key_table;
	delete m_index;
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
	if( m_index ) {
		std::string index;
		SimpleList<KeyCacheEntry *> *keylist=NULL;

		m_index->startIterations();
		while( m_index->iterate(index,keylist) ) {
			delete keylist;
		}
		m_index->clear();
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
	else {
		addToIndex(new_ent);
	}

	return retval;
}

void
KeyCache::makeServerUniqueId(std::string const &parent_id, int server_pid, std::string& result) {
	if( parent_id.empty() || server_pid == 0 ) {
			// If our peer is not a daemon, parent_id will be empty
			// and there is no point in indexing it, because we
			// never query by PID alone.
		return;
	}
	formatstr(result, "%s.%d", parent_id.c_str(), server_pid);
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

void
KeyCache::addToIndex(KeyCacheEntry *key)
{
		// update our index
	ClassAd *policy = key->policy();
	std::string parent_id;
	std::string server_unique_id;
	int server_pid=0;
	std::string server_addr, peer_addr;

	policy->LookupString(ATTR_SEC_SERVER_COMMAND_SOCK, server_addr);
	policy->LookupString(ATTR_SEC_PARENT_UNIQUE_ID, parent_id);
	policy->LookupInteger(ATTR_SEC_SERVER_PID, server_pid);

	if (key->addr())
		peer_addr = key->addr()->to_sinful();
	addToIndex(m_index,peer_addr,key);
	addToIndex(m_index,server_addr,key);

	makeServerUniqueId(parent_id, server_pid, server_unique_id);
	addToIndex(m_index,server_unique_id,key);
}

void
KeyCache::removeFromIndex(KeyCacheEntry *key)
{
		//remove references to this key from the index
	std::string parent_id;
	std::string server_unique_id;
	int server_pid=0;
	std::string server_addr, peer_addr;
	ClassAd *policy = key->policy();
	ASSERT( policy );

	policy->LookupString(ATTR_SEC_SERVER_COMMAND_SOCK, server_addr);
	policy->LookupString(ATTR_SEC_PARENT_UNIQUE_ID, parent_id);
	policy->LookupInteger(ATTR_SEC_SERVER_PID, server_pid);

	if (key->addr())
		peer_addr = key->addr()->to_sinful();
	removeFromIndex(m_index,peer_addr,key);
	removeFromIndex(m_index,server_addr,key);

	makeServerUniqueId(parent_id, server_pid, server_unique_id);
	removeFromIndex(m_index,server_unique_id,key);
}

void
KeyCache::addToIndex(KeyCacheIndex *hash,std::string const &index,KeyCacheEntry *key)
{
	if( index.empty() ) {
		return;
	}
	ASSERT( key );

	SimpleList<KeyCacheEntry *> *keylist=NULL;
	if( hash->lookup(index,keylist)!=0 ) {
		keylist = new SimpleList<KeyCacheEntry *>;
		ASSERT( keylist );
		bool inserted = hash->insert(index,keylist)==0;
		ASSERT(inserted);
	}
	bool appended = keylist->Append(key);
	ASSERT( appended );
}

void
KeyCache::removeFromIndex(KeyCacheIndex *hash,std::string const &index,KeyCacheEntry *key)
{
	SimpleList<KeyCacheEntry *> *keylist=NULL;
	if( hash->lookup(index,keylist)!=0 ) {
		return;
	}
	bool deleted = keylist->Delete(key);
	ASSERT( deleted );

	if( keylist->Length() == 0 ) {
		delete keylist;
		bool removed = hash->remove(index)==0;
		ASSERT( removed );
	}
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
		removeFromIndex( tmp_ptr );

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

	dprintf (D_SECURITY|D_FULLDEBUG, "KEYCACHE: Session %s %s expired at %s\n", e->id(), expiration_type, ctime(&key_exp) );

	// remove its reference from the hash table
	remove(e->id());       // This should do it
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

StringList *
KeyCache::getKeysForPeerAddress(char const *addr)
{
	if( !addr || !*addr ) {
		return NULL;
	}
	SimpleList<KeyCacheEntry*> *keylist=NULL;
	if( m_index->lookup(addr,keylist)!=0 ) {
		return NULL;
	}
	ASSERT( keylist );

	StringList *keyids = new StringList;
	KeyCacheEntry *key=NULL;

	keylist->Rewind();
	while( keylist->Next(key) ) {
		std::string server_addr,peer_addr;
		ClassAd *policy = key->policy();

		policy->LookupString(ATTR_SEC_SERVER_COMMAND_SOCK, server_addr);
		if (key->addr())
			peer_addr = key->addr()->to_sinful();
			// addr should match either the server command socket
			// or the peer client address associated with this entry.
			// If not, then something is horribly wrong with our index.
		ASSERT( server_addr == addr || peer_addr == addr );

		keyids->append(key->id());
	}
	return keyids;
}

StringList *
KeyCache::getKeysForProcess(char const *parent_unique_id,int pid)
{
	std::string server_unique_id;
	makeServerUniqueId(parent_unique_id, pid, server_unique_id);

	SimpleList<KeyCacheEntry*> *keylist=NULL;
	if( m_index->lookup(server_unique_id,keylist)!=0 ) {
		return NULL;
	}
	ASSERT( keylist );

	StringList *keyids = new StringList;
	KeyCacheEntry *key=NULL;

	keylist->Rewind();
	while( keylist->Next(key) ) {
		std::string this_parent_id;
		std::string this_server_unique_id;
		int this_server_pid=0;
		ClassAd *policy = key->policy();

		policy->LookupString(ATTR_SEC_PARENT_UNIQUE_ID, this_parent_id);
		policy->LookupInteger(ATTR_SEC_SERVER_PID, this_server_pid);
		makeServerUniqueId(this_parent_id, this_server_pid, this_server_unique_id);

			// If server id of key in index does not match server id
			// we are looking up, something is horribly wrong with
			// the index.
		ASSERT( this_server_unique_id == server_unique_id );

		keyids->append(key->id());
	}
	return keyids;
}

int KeyCache::count() {
	ASSERT( key_table );
	return key_table->getNumElements();
}
