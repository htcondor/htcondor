/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "KeyCache.h"
#include "CryptKey.h"

KeyCacheEntry::KeyCacheEntry( char *id, struct sockaddr_in * addr, KeyInfo* key, ClassAd * policy, int expiration) {
	if (id) {
		_id = strdup(id);
	} else {
		_id = NULL;
	}

	if (addr) {
		_addr = new struct sockaddr_in(*addr);
	} else {
		_addr = NULL;
	}

	if (key) {
		_key = new KeyInfo(*key);
	} else {
		_key = NULL;
	}

	if (policy) {
		_policy = new ClassAd(*policy);
	} else {
		_policy = NULL;
	}

	_expiration = expiration;
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

struct sockaddr_in *  KeyCacheEntry::addr() {
	return _addr;
}

KeyInfo* KeyCacheEntry::key() {
	return _key;
}

ClassAd* KeyCacheEntry::policy() {
	return _policy;
}

int KeyCacheEntry::expiration() {
	return _expiration;
}

void KeyCacheEntry::copy_storage(const KeyCacheEntry &copy) {
	if (copy._id) {
		_id = strdup(copy._id);
	} else {
		_id = NULL;
	}

	if (copy._addr) {
    	_addr = new struct sockaddr_in(*(copy._addr));
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
	dprintf ( D_SECURITY, "KEYCACHE: created: %p\n", key_table );
}

KeyCache::KeyCache(const KeyCache& k) {
	copy_storage(k);
}

KeyCache::~KeyCache() {
	delete_storage();
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
		key_table = new HashTable<MyString, KeyCacheEntry*>(copy.key_table->getTableSize(), MyStringHash, rejectDuplicateKeys);
		dprintf ( D_SECURITY, "KEYCACHE: created: %p\n", key_table );

		// manually iterate all entries from the hash.  they are
		// pointers, and we need to copy that object.
		KeyCacheEntry* key_entry;
		copy.key_table->startIterations();
		while (copy.key_table->iterate(key_entry)) {
			KeyCacheEntry *tmp_ent = new KeyCacheEntry(*key_entry);
			key_table->insert(tmp_ent->id(), tmp_ent);
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
				if( DebugFlags & D_FULLDEBUG ) {
					dprintf( D_SECURITY, "KEYCACHEENTRY: deleted: %p\n", 
							 key_entry );
				}
				delete key_entry;
			}
		}
		delete key_table;
		if( DebugFlags & D_FULLDEBUG ) {
			dprintf( D_SECURITY, "KEYCACHE: deleted: %p\n", key_table );
		}
		key_table = NULL;
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

	return retval;
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

bool KeyCache::remove(const char *key_id) {
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
	char* key_id = strdup (e->id());
	time_t key_exp = e->expiration();

	dprintf (D_SECURITY, "KEYCACHE: Session %s expired at %s", e->id(), ctime(&key_exp) );

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

