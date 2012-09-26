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

KeyCacheEntry::KeyCacheEntry( char const *id_param, const condor_sockaddr * addr_param, KeyInfo* key_param, ClassAd * policy_param, int expiration_param, int lease_interval ) {
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
		_key = new KeyInfo(*key_param);
	} else {
		_key = NULL;
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
	return _key;
}

ClassAd* KeyCacheEntry::policy() {
	return _policy;
}

int KeyCacheEntry::expiration() {
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

char const *KeyCacheEntry::expirationType() {
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

	if (copy._key) {
		_key = new KeyInfo(*(copy._key));
	} else {
		_key = NULL;
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
}


void KeyCacheEntry::delete_storage() {
	if (_id) {
        free( _id );
	}
	if (_addr) {
	  delete _addr;
	}
	if (_key) {
	  delete _key;
	}
	if (_policy) {
	  delete _policy;
	}
}


KeyCache::KeyCache(int nbuckets) {
	key_table = new HashTable<MyString, KeyCacheEntry*>(nbuckets, MyStringHash, rejectDuplicateKeys);
	m_index = new KeyCacheIndex(MyStringHash);
	dprintf ( D_SECURITY, "KEYCACHE: created: %p\n", key_table );
}

KeyCache::KeyCache(const KeyCache& k) {
	m_index = new KeyCacheIndex(MyStringHash);
	copy_storage(k);
}

KeyCache::~KeyCache() {
	delete_storage();
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
	if (copy.key_table) {
		m_index = new KeyCacheIndex(MyStringHash);
		key_table = new HashTable<MyString, KeyCacheEntry*>(copy.key_table->getTableSize(), MyStringHash, rejectDuplicateKeys);
		dprintf ( D_SECURITY, "KEYCACHE: created: %p\n", key_table );

		// manually iterate all entries from the hash.  they are
		// pointers, and we need to copy that object.
		KeyCacheEntry* key_entry;
		copy.key_table->startIterations();
		while (copy.key_table->iterate(key_entry)) {
			insert(*key_entry);
		}
	} else {
		key_table = NULL;
	}
}


void KeyCache::delete_storage()
{
	if( key_table ) {
			// Delete all entries from the hash, and the table itself
		KeyCacheEntry* key_entry;
		key_table->startIterations();
		while( key_table->iterate(key_entry) ) {
			if( key_entry ) {
				if( IsDebugVerbose(D_SECURITY) ) {
					dprintf( D_SECURITY, "KEYCACHEENTRY: deleted: %p\n", 
							 key_entry );
				}
				delete key_entry;
			}
		}
		if( IsDebugVerbose(D_SECURITY) ) {
			dprintf( D_SECURITY, "KEYCACHE: deleted: %p\n", key_table );
		}
		delete key_table;
		key_table = NULL;
	}
	if( m_index ) {
		MyString index;
		SimpleList<KeyCacheEntry *> *keylist=NULL;

		m_index->startIterations();
		while( m_index->iterate(index,keylist) ) {
			delete keylist;
		}
		m_index->clear();
	}
}


bool KeyCache::insert(KeyCacheEntry &e) {

	// the key_table member is a HashTable which maps
	// MyString's to KeyCacheEntry*'s.  (note the '*')

	// the map_table member is a HashTable which maps
	// MyString's to MyString's.

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
KeyCache::makeServerUniqueId(MyString const &parent_id,int server_pid,MyString *result) {
	ASSERT( result );
	if( parent_id.IsEmpty() || server_pid == 0 ) {
			// If our peer is not a daemon, parent_id will be empty
			// and there is no point in indexing it, because we
			// never query by PID alone.
		return;
	}
	result->formatstr("%s.%d",parent_id.Value(),server_pid);
}

bool KeyCache::lookup(const char *key_id, KeyCacheEntry *&e_ptr) {

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
	MyString parent_id, server_unique_id;
	int server_pid=0;
	MyString server_addr, peer_addr;

	policy->LookupString(ATTR_SEC_SERVER_COMMAND_SOCK, server_addr);
	policy->LookupString(ATTR_SEC_PARENT_UNIQUE_ID, parent_id);
	policy->LookupInteger(ATTR_SEC_SERVER_PID, server_pid);

	if (key->addr())
		peer_addr = key->addr()->to_sinful();
	addToIndex(m_index,peer_addr,key);
	addToIndex(m_index,server_addr,key);

	makeServerUniqueId(parent_id,server_pid,&server_unique_id);
	addToIndex(m_index,server_unique_id,key);
}

void
KeyCache::removeFromIndex(KeyCacheEntry *key)
{
		//remove references to this key from the index
	MyString parent_id, server_unique_id;
	int server_pid=0;
	MyString server_addr, peer_addr;
	ClassAd *policy = key->policy();
	ASSERT( policy );

	policy->LookupString(ATTR_SEC_SERVER_COMMAND_SOCK, server_addr);
	policy->LookupString(ATTR_SEC_PARENT_UNIQUE_ID, parent_id);
	policy->LookupInteger(ATTR_SEC_SERVER_PID, server_pid);

	if (key->addr())
		peer_addr = key->addr()->to_sinful();
	removeFromIndex(m_index,peer_addr,key);
	removeFromIndex(m_index,server_addr,key);

	makeServerUniqueId(parent_id,server_pid,&server_unique_id);
	removeFromIndex(m_index,server_unique_id,key);
}

void
KeyCache::addToIndex(KeyCacheIndex *hash,MyString const &index,KeyCacheEntry *key)
{
	if( index.IsEmpty() ) {
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
KeyCache::removeFromIndex(KeyCacheIndex *hash,MyString const &index,KeyCacheEntry *key)
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
	char* key_id = strdup (e->id());
	time_t key_exp = e->expiration();
	char const *expiration_type = e->expirationType();

	dprintf (D_SECURITY, "KEYCACHE: Session %s %s expired at %s", e->id(), expiration_type, ctime(&key_exp) );

	// remove its reference from the hash table
	remove(key_id);       // This should do it
	dprintf (D_SECURITY, "KEYCACHE: Removed %s from key cache.\n", key_id);

	free( key_id );
}

StringList * KeyCache::getExpiredKeys() {

	// draw the line
    StringList * list = new StringList();
	time_t cutoff_time = time(0);

	// iterate through all entries from the hash
	KeyCacheEntry* key_entry;
    MyString id;
	key_table->startIterations();
	while (key_table->iterate(id, key_entry)) {
		// check the freshness date on that key
		if (key_entry->expiration() && key_entry->expiration() <= cutoff_time) {
            list->append(id.Value());
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
		MyString server_addr,peer_addr;
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
	MyString server_unique_id;
	makeServerUniqueId(parent_unique_id,pid,&server_unique_id);

	SimpleList<KeyCacheEntry*> *keylist=NULL;
	if( m_index->lookup(server_unique_id,keylist)!=0 ) {
		return NULL;
	}
	ASSERT( keylist );

	StringList *keyids = new StringList;
	KeyCacheEntry *key=NULL;

	keylist->Rewind();
	while( keylist->Next(key) ) {
		MyString this_parent_id,this_server_unique_id;
		int this_server_pid=0;
		ClassAd *policy = key->policy();

		policy->LookupString(ATTR_SEC_PARENT_UNIQUE_ID, this_parent_id);
		policy->LookupInteger(ATTR_SEC_SERVER_PID, this_server_pid);
		makeServerUniqueId(this_parent_id,this_server_pid,&this_server_unique_id);

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
