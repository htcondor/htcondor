#ifndef __COLLHASH_H__
#define __COLLHASH_H__

#include <iostream.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include "condor_classad.h"

#include "HashTable.h"

// this is the tuple that we will be hashing on
class HashKey
{
    public:

    char name    [64];
    char ip_addr [16];

	void   sprint (char *);
	friend ostream& operator<< (ostream &out, const HashKey &); 
    friend bool operator== (HashKey &, HashKey &);
};

// the hash functions
int hashFunction (HashKey &, int);
int hashOnName   (HashKey &, int);

// type for the hash tables ...
typedef HashTable <HashKey, ClassAd *> CollectorHashTable;

// functions to make the hashkeys
bool makeStartdAdHashKey (HashKey &, ClassAd *, sockaddr_in *);
bool makeScheddAdHashKey (HashKey &, ClassAd *, sockaddr_in *);
bool makeMasterAdHashKey (HashKey &, ClassAd *, sockaddr_in *);
bool makeCkptSrvrAdHashKey (HashKey &, ClassAd *, sockaddr_in *);

#endif __COLLHASH_H__





