/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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
#if !defined(_STRING_HASHKEY_H)
#define _STRING_HASHKEY_H

/*
   The HashKey class makes a HashTable key from a char*.  This
   class seems to duplicate functionality in the MyString class.  :-(
*/

#include "HashTable.h"

#include "condor_fix_assert.h"

class HashKey
{
public:
	HashKey() { key = NULL; }
	HashKey(const char *k) { key = strdup(k); }
	HashKey(const HashKey &hk) { key = strdup(hk.key); }
	~HashKey() { if (key) free(key); }
	void sprint(char *);
	HashKey& operator= (const HashKey& from);
	const char* value() { if (key) return key; else return "\0"; };
	friend ostream& operator<< (ostream &out, const HashKey &); 
    friend bool operator== (const HashKey &, const HashKey &);
	friend int hashFunction(const HashKey &key, int numBuckets);
private:
	char *key;
};

int hashFunction(const HashKey &, int);

#endif
