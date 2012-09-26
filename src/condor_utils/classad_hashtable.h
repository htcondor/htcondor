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

#if !defined(_STRING_HASHKEY_H)
#define _STRING_HASHKEY_H

/*
   The HashKey class makes a HashTable key from a char*.  This
   class seems to duplicate functionality in the MyString class.  :-(
*/

//#include <iosfwd>

#include "HashTable.h"
#include "MyString.h"

#include "condor_fix_assert.h"

class HashKey
{
public:
	HashKey() { key = NULL; }
	HashKey(const char *k) { key = strdup(k); }
	HashKey(const HashKey &hk) { key = strdup(hk.key); }
	~HashKey() { if (key) free(key); }
	void sprint(MyString &s);
	HashKey& operator= (const HashKey& from);
	const char* value() { if (key) return key; else return "\0"; };
	//friend std::ostream& operator<< (std::ostream &out, const HashKey &); 
    friend bool operator== (const HashKey &, const HashKey &);
	friend unsigned int hashFunction(const HashKey &key);
private:
	char *key;
};

unsigned int hashFunction(const HashKey &);

/* AttrKey makes a HashTable key from a char*, with case-insensitive
 * hashing/comparison. It's suitable for use with ClassAd attribute
 * names.
 */

class AttrKey
{
public:
	AttrKey() { key = NULL; }
	AttrKey(const char *k) { key = strdup(k); }
	AttrKey(const AttrKey &hk) { key = strdup(hk.key); }
	~AttrKey() { if (key) free(key); }
	AttrKey& operator= (const AttrKey& from);
	const char* value() const { if (key) return key; else return "\0"; };
    friend bool operator== (const AttrKey &, const AttrKey &);
private:
	char *key;
};

unsigned int AttrKeyHashFunction( const AttrKey & );

#endif
