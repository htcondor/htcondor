/* 
** Copyright 1997 by Condor Team
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the copyright holders not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the 
** copyright holders make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE COPYRIGHT HOLDERS DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Author:  Jim Basney (based on Jim Pruyne's QMGMT library)
**
*/ 

#ifdef __GNUG__
#pragma implementation "HashTable.h"
#endif

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
