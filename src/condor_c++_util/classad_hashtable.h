#if !defined(_CLASSAD_HASHTABLE_H)
#define _CLASSAD_HASHTABLE_H

#include "HashTable.h"
#include "condor_classad.h"

#include <assert.h>

class HashKey
{
public:
	HashKey() { key = NULL; }
	HashKey(const char *k) { key = strdup(k); }
	HashKey(const HashKey &hk) { key = strdup(hk.key); }
	~HashKey() { if (key) free(key); }
	void sprint(char *);
	HashKey& operator= (const HashKey& from);
	friend ostream& operator<< (ostream &out, const HashKey &); 
    friend bool operator== (const HashKey &, const HashKey &);
	friend int hashFunction(const HashKey &key, int numBuckets);
private:
	char *key;
};

int hashFunction(const HashKey &, int);

typedef HashTable <HashKey, ClassAd *> ClassAdHashTable;

#endif