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

#include "classad_hashtable.h"

void HashKey::sprint(MyString &s)
{
	s.formatstr("%s", key);
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


unsigned int hashFunction(const HashKey &key)
{
	unsigned int hash = 0;

	const char *p = key.key;
	while (*p) {
		hash = (hash<<5)+hash + (unsigned char)*p;
		p++;
	}

	return hash;
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

unsigned int 
AttrKeyHashFunction (const AttrKey &key)
{
	const char *str = key.value();
	int i = strlen( str ) - 1;
	unsigned int hashVal = 0;
	while (i >= 0) {
		hashVal += (unsigned int)tolower(str[i]);
		i--;
	}
	return hashVal;
}
