/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
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
