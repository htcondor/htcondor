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

#include "classad_hashtable.h"

void HashKey::sprint(char *s)
{
	strcpy(s, key);
}

HashKey& HashKey::operator= (const HashKey& from)
{
	if (this->key)
		free(this->key);
	this->key = strdup(from.key);
	return *this;
}

bool operator==(const HashKey &lhs, const HashKey &rhs)
{
	return (strcmp(lhs.key, rhs.key) == 0);
}

ostream &operator<<(ostream &out, const HashKey &hk)
{
	out << "Hashkey: " << hk.key << endl;
	return out;
}

int hashFunction(const HashKey &key, int numBuckets)
{
	unsigned int bkt = 0;
	int i;

	if (key.key) {
		for (i = 0; key.key[i]; bkt += key.key[i++]);
		bkt %= numBuckets;
	}

	return bkt;
}


AttrKey& AttrKey::operator= (const AttrKey& from)
{
	if (this->key)
		free(this->key);
	this->key = strdup(from.key);
	return *this;
}

bool operator==(const AttrKey &lhs, const AttrKey &rhs)
{
	return (strcasecmp(lhs.key, rhs.key) == 0);
}

int 
AttrKeyHashFunction (const AttrKey &key, int numBuckets)
{
	const char *str = key.value();
	int i = strlen( str ) - 1, hashVal = 0;
	while (i >= 0) {
		hashVal += tolower(str[i]);
		i--;
	}
	return (hashVal % numBuckets);
}
