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

class SecMan;
class KeyCacheEntry {
 public:
    KeyCacheEntry(
			char const * id,
			struct sockaddr_in * addr,
			KeyInfo * key,
			ClassAd * policy,
			int expiration
			);
    KeyCacheEntry(const KeyCacheEntry &copy);
    ~KeyCacheEntry();

	const KeyCacheEntry& operator=(const KeyCacheEntry &kc);

    char*                 id();
    struct sockaddr_in *  addr();
    KeyInfo*              key();
    ClassAd*              policy();
    int                   expiration();
	void                  setExpiration(int new_expiration);

 private:

	void delete_storage();
	void copy_storage(const KeyCacheEntry &);

    char *               _id;
    struct sockaddr_in * _addr;
    KeyInfo*             _key;
    ClassAd*             _policy;
    int                  _expiration;
};



class KeyCache {
    friend class SecMan;
public:
	KeyCache(int nbuckets);
	KeyCache(const KeyCache&);
	~KeyCache();
	const KeyCache& operator=(const KeyCache&);

	bool insert(KeyCacheEntry&);
	bool lookup(const char *key_id, KeyCacheEntry*&);
	bool remove(const char *key_id);
	void expire(KeyCacheEntry*);

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
