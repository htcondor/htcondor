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

#ifndef CONDOR_KEYCACHE_H_INCLUDE
#define CONDOR_KEYCACHE_H_INCLUDE

#include "condor_common.h"
#include "condor_classad.h"
#include "CryptKey.h"
#include "MyString.h"
#include "HashTable.h"
#include "string_list.h"

class SecMan;
class KeyCacheEntry {
 public:
    KeyCacheEntry(
			char * id,
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

private:
	void copy_storage(const KeyCache &kc);
	void delete_storage();

	HashTable<MyString, KeyCacheEntry*> *key_table;
};


#endif
