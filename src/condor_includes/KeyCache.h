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
#include "MyString.h"
#include "HashTable.h"
#include "string_list.h"
#include "simplelist.h"
#include "condor_sockaddr.h"

class SecMan;
class KeyCacheEntry {
 public:
    KeyCacheEntry(
			char const * id,
			const condor_sockaddr* addr,
			const KeyInfo * key,
			const ClassAd * policy,
			int expiration,
			int session_lease
			);
    KeyCacheEntry(
			char const * id,
			const condor_sockaddr* addr,
			std::vector<KeyInfo *> key,
			const ClassAd * policy,
			int expiration,
			int session_lease
			);
    KeyCacheEntry(const KeyCacheEntry &copy);
    ~KeyCacheEntry();

	const KeyCacheEntry& operator=(const KeyCacheEntry &kc);

    char*                 id();
	const condor_sockaddr*         addr();
    KeyInfo*              key();
    KeyInfo*              key(Protocol protocol);
    ClassAd*              policy();
    int                   expiration() const;
	char const *          expirationType() const;
	void                  setExpiration(int new_expiration);
	void                  setLingerFlag(bool flag) { _lingering = flag; }
	bool                  getLingerFlag() const { return _lingering; }
	bool                  setPreferredProtocol(Protocol preferred);

	void                  renewLease();
 private:

	void delete_storage();
	void copy_storage(const KeyCacheEntry &);

    char *               _id;
    condor_sockaddr*              _addr;
	std::vector<KeyInfo*> _keys;
    ClassAd*             _policy;
    int                  _expiration;
	int                  _lease_interval;   // max seconds of unused time
	time_t               _lease_expiration; // time of lease expiration
	bool                 _lingering; // true if session only exists
	                                 // to catch lingering communication
	Protocol             _preferred_protocol;
};



class KeyCache {
    friend class SecMan;
public:
	KeyCache();
	KeyCache(const KeyCache&);
	~KeyCache();
	const KeyCache& operator=(const KeyCache&);

	void clear();
	bool insert(KeyCacheEntry&);
	bool lookup(const char *key_id, KeyCacheEntry*&);
	bool remove(const char *key_id);
	void expire(KeyCacheEntry*);
	int  count();

	StringList * getExpiredKeys();
	StringList * getKeysForPeerAddress(char const *addr);
	StringList * getKeysForProcess(char const *parent_unique_id,int pid);

private:
	void copy_storage(const KeyCache &kc);
	void delete_storage();

	typedef HashTable<MyString, SimpleList<KeyCacheEntry *>* > KeyCacheIndex;

	HashTable<MyString, KeyCacheEntry*> *key_table;
	KeyCacheIndex *m_index;

	void addToIndex(KeyCacheEntry *);
	void removeFromIndex(KeyCacheEntry *);
	void addToIndex(KeyCacheIndex *,MyString const &index,KeyCacheEntry *);
	void removeFromIndex(KeyCacheIndex *,MyString const &index,KeyCacheEntry *);
	void makeServerUniqueId(MyString const &parent_id,int server_pid,MyString *result);
};


#endif
