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
#ifndef HASH_H
#define HASH_H

#include "condor_common.h"

// a generic hash bucket class

template <class Index, class Value>
class HashBucket {
 public:
  Index       index;                         // stored index
  Value      value;                          // associated value
  HashBucket<Index, Value> *next;            // next node in the hash table
};

// a generic hash table class

template <class Index, class Value>
class HashTable {
 public:
  HashTable(int tableSize,
	    int (*hashfcn)(const Index &index,
			   int numBuckets)); // constructor  
  ~HashTable();                              // destructor

  int insert(const Index &index, const Value &value);
  int lookup(const Index &index, Value &value);
  int lookup(const Index &index, Value* &value);
  int getNext(Index &index, void *current, Value &value,
	      void *&next);
  int remove(const Index &index);  
  int getNumElements( ) { return numElems; }
  int clear();

  void startIterations (void);
  int  iterate (Value &value);
  int  getCurrentKey (Index &index);
  int  iterate (Index &index, Value &value);
    
 private:
#ifdef DEBUGHASH
  void dump();                                  // dump contents of hash table
#endif

  int tableSize;                                // size of hash table
  HashBucket<Index, Value> **ht;                // actual hash table
  int (*hashfcn)(const Index &index, int numBuckets); // user-provided hash function
  int currentBucket;
  HashBucket<Index, Value> *currentItem;
  int *chainsUsed;	// array which says which chains have items; speeds iterating
  int chainsUsedLen;	// index to end of chainsUsed array
  int numElems; // number of elements in the hashtable
  int chainsUsedFreeList;	// head of free list of deleted items in chainsUsed
  int endOfFreeList;
};

// Construct hash table. Allocate memory for hash table and
// initialize its elements.

template <class Index, class Value>
HashTable<Index,Value>::HashTable(int tableSz,
				  int (*hashF)(const Index &index,
					       int numBuckets)) :
	tableSize(tableSz), hashfcn(hashF)
{
  int i,j,k;

  // If tableSize is anything but tiny, round it up to
  // the next prime number if a prime can be found within
  // 35 of the specified size.  Using a prime for tableSize
  // makes for faster lookups assuming the the hash func does
  // a mod on tableSize.
  if ( tableSize > 5 ) {
	  for (i=tableSize;i<tableSize+35;i++) {
			k = i / 2;	// should probably be square root...
			j = 1;
			while ( ++j < k ) {
				// if ( (i / j)*j == i ) {
				if ( (i % j) == 0 ) {
					j = 0;
					break;
				}
			}
			if ( j ) {
				// found a prime
				tableSize = i;
				break;
			}
	  }
  }

  if (!(ht = new HashBucket<Index, Value>* [tableSize])) {
    cerr << "Insufficient memory for hash table" << endl;
    exit(1);
  }
  if (!(chainsUsed = new int[tableSize])) {
    cerr << "Insufficient memory for hash table (chainsUsed array)" << endl;
    exit(1);
  }
  for(i = 0; i < tableSize; i++) {
    ht[i] = NULL;
	chainsUsed[i] = -1;
  }
  currentBucket = -1;
  currentItem = 0;
  chainsUsedLen = 0;
  numElems = 0;
  endOfFreeList = 0 - tableSize - 10;
  chainsUsedFreeList = endOfFreeList;
}

// Insert entry into hash table mapping Index to Value.
// Returns 0 if OK, an error code otherwise.

template <class Index, class Value>
int HashTable<Index,Value>::insert(const Index &index,const  Value &value)
{
  int temp;
  int idx = hashfcn(index, tableSize);
  // do sanity check on return value from hash func
  if ( (idx < 0) || (idx >= tableSize) ) {
    return -1;
  }

  HashBucket<Index, Value> *bucket;
  if (!(bucket = new HashBucket<Index, Value>)) {
    cerr << "Insufficient memory" << endl;
    return -1;
  }

  bucket->index = index;
  bucket->value = value;
  bucket->next = ht[idx];
  if ( !ht[idx] ) {
	// this is the first item we are adding to this chain
	if ( chainsUsedFreeList == endOfFreeList ) {
		chainsUsed[chainsUsedLen++] = idx;
	} else {
		temp = chainsUsedFreeList + tableSize;
		chainsUsedFreeList = chainsUsed[temp];
		chainsUsed[temp] = idx;
	}
  }
  ht[idx] = bucket;

#ifdef DEBUGHASH
  dump();
#endif

  numElems++;
  return 0;
}

// Check if Index is currently in the hash table. If so, return
// corresponding value and OK status (0). Otherwise return -1.

template <class Index, class Value>
int HashTable<Index,Value>::lookup(const Index &index, Value &value)
{
  int idx = hashfcn(index, tableSize);
  // do sanity check on return value from hash func
  if ( (idx < 0) || (idx >= tableSize) ) {
    return -1;
  }

  HashBucket<Index, Value> *bucket = ht[idx];
  while(bucket) {
#ifdef DEBUGHASH
    cerr << "%%  Comparing " << *(long *)&bucket->index
         << " vs. " << *(long *)index << endl;
#endif
    if (bucket->index == index) {
      value = bucket->value;
      return 0;
    }
    bucket = bucket->next;
  }

#ifdef DEBUGHASH
  dump();
#endif

  return -1;
}

// This lookup() is the same as above, but it expects (and returns) a
// _pointer_ reference to the value.  

template <class Index, class Value>
int HashTable<Index,Value>::lookup(const Index &index, Value* &value )
{
  int idx = hashfcn(index, tableSize);
  // do sanity check on return value from hash func
  if ( (idx < 0) || (idx >= tableSize) ) {
    return -1;
  }

  HashBucket<Index, Value> *bucket = ht[idx];
  while(bucket) {
#ifdef DEBUGHASH
    cerr << "%%  Comparing " << *(long *)&bucket->index
         << " vs. " << *(long *)index << endl;
#endif
    if (bucket->index == index) {
      value = (Value *) &(bucket->value);
      return 0;
    }
    bucket = bucket->next;
  }

#ifdef DEBUGHASH
  dump();
#endif

  return -1;
}

// A function which allows duplicate Indices to be retrieved
// iteratively. The first match is returned in next if current
// is NULL. Upon subsequent calls, caller should set
// current = next before calling again. If Index not found,
// returns -1.

template <class Index, class Value>
int HashTable<Index,Value>::getNext(Index &index, void *current,
				    Value &value, void *&next)
{
  HashBucket<Index, Value> *bucket;

  if (!current) {
    int idx = hashfcn(index, tableSize);
  	// do sanity check on return value from hash func
  	if ( (idx < 0) || (idx >= tableSize) ) {
   		return -1;
  	}
    bucket = ht[idx];
  } else {
    bucket = (HashBucket<Index, Value> *)current;
    bucket = bucket->next;
  }

  while(bucket) {
    if (bucket->index == index) {
      value = bucket->value;
      next = bucket;
      return 0;
    }
    bucket = bucket->next;
  }

#ifdef DEBUGHASH
  dump();
#endif

  return -1;
}

// Delete Index entry from hash table. Return OK (0) if index was found.
// Else return -1.

template <class Index, class Value>
int HashTable<Index,Value>::remove(const Index &index)
{
  	int idx = hashfcn(index, tableSize);
	int i;

  	// do sanity check on return value from hash func
  	if ( (idx < 0) || (idx >= tableSize) ) {
   		return -1;
  	}

  	HashBucket<Index, Value> *bucket = ht[idx];
  	HashBucket<Index, Value> *prevBuc = ht[idx];

  	while(bucket) 
	{
    	if (bucket->index == index) 
		{
      		if (bucket == ht[idx]) 
			{
				ht[idx] = bucket->next;

				// if the item being deleted is being iterated, ensure that
				// next iteration returns the object "after" this one
				if (bucket == currentItem)
				{
					currentItem = 0;
					currentBucket --;
				}
			}
      		else
			{
				prevBuc->next = bucket->next;

				// Again, take care of the iterator
				if (bucket == currentItem)
				{
					currentItem = prevBuc;
				}
			}

      		delete bucket;

			if ( !ht[idx] ) {
				// We have removed the last bucket in this chain.
				// Remove this idx from our chainsUsed array.
				for (i=0;i<chainsUsedLen;i++) {
					if ( chainsUsed[i] == idx ) {
						// chainsUsed[i] = chainsUsed[--chainsUsedLen];
						chainsUsed[i] = chainsUsedFreeList;
						chainsUsedFreeList = i - tableSize;
						break;
					}
				}
			}

#			ifdef DEBUGHASH
      		dump();
#			endif

			numElems--;
      		return 0;
    	}

    	prevBuc = bucket;
    	bucket = bucket->next;
  	}

  	return -1;
}

// Clear hash table by deallocating hash buckets in table.

template <class Index, class Value>
int HashTable<Index,Value>::clear()
{
  for(int i = 0; i < tableSize; i++) {
    HashBucket<Index, Value> *tmpBuf = ht[i];
    while(ht[i]) {
      tmpBuf = ht[i];
      ht[i] = ht[i]->next;
      delete tmpBuf;
    }
  }

  chainsUsedLen = 0;
  chainsUsedFreeList = endOfFreeList;

  return 0;
}

template <class Index, class Value>
void HashTable<Index,Value>::
startIterations (void)
{
	int temp, temp1;

    currentBucket = -1;
	currentItem = 0;
						
	// compress the chainsUsed list if needed
	while ( chainsUsedFreeList != endOfFreeList ) {
		temp = chainsUsedFreeList + tableSize;
		chainsUsedFreeList = chainsUsed[temp];
		temp1 = -1;
		while ( temp < chainsUsedLen && temp1 < 0 ) {
			temp1 = chainsUsed[--chainsUsedLen];
		}
		chainsUsed[temp] = temp1;
	}

}


template <class Index, class Value>
int HashTable<Index,Value>::
iterate (Value &v)
{
    // try to get next item in chain ...
    if (currentItem)
    {
        currentItem = currentItem->next;
    
        // ... if successful, return OK
        if (currentItem)
        {
            v = currentItem->value;
            return 1;
       }
    }

	// try next bucket ...
	do {
		currentBucket ++;
	} while ( (currentBucket < chainsUsedLen) && 
			  (chainsUsed[currentBucket] < 0) );

	// must use >= here instead of == since chainsUsedLen could decrement
	// if the user call remove()
	if (currentBucket >= chainsUsedLen)
	{
    	// end of hash table ... no more entries
    	currentBucket = -1;
    	currentItem = 0;

    	return 0;
	}
	else
	{
		currentItem = ht[ chainsUsed[currentBucket] ];
		v = currentItem->value;
		return 1;
	}
}

template <class Index, class Value>
int HashTable<Index,Value>::
getCurrentKey (Index &index)
{
    if (!currentItem) return -1;
    index = currentItem->index;
    return 0;
}


template <class Index, class Value>
int HashTable<Index,Value>::
iterate (Index &index, Value &v)
{
    // try to get next item in chain ...
    if (currentItem)
    {
        currentItem = currentItem->next;
    
        // ... if successful, return OK
        if (currentItem)
        {
            index = currentItem->index;
            v = currentItem->value;
            return 1;
       }
    }

	// try next bucket ...
	do {
		currentBucket ++;
	} while ( (currentBucket < chainsUsedLen) && 
			  (chainsUsed[currentBucket] < 0) );


	// must use >= here instead of == since chainsUsedLen could decrement
	// if the user call remove()
	if (currentBucket >= chainsUsedLen)
	{
    	// end of hash table ... no more entries
    	currentBucket = -1;
    	currentItem = 0;

    	return 0;
	}
	else
	{
		currentItem = ht[ chainsUsed[currentBucket] ];
		index = currentItem->index;
		v = currentItem->value;
		return 1;
	}
}

// Delete hash table by deallocating hash buckets in table and then
// deleting table itself.

template <class Index, class Value>
HashTable<Index,Value>::~HashTable()
{
  clear();
  delete [] ht;
  delete [] chainsUsed;
}

#ifdef DEBUGHASH
// Dump hash table contents.

template <class Index, class Value>
void HashTable<Index,Value>::dump()
{
  for(int i = 0; i < tableSize; i++) {
    HashBucket<Index, Value> *tmpBuf = ht[i];
    int hasStuff = (tmpBuf != NULL);
    if (hasStuff)
      cerr << "%%  Hash value " << i << ": ";
    while(tmpBuf) {
      cerr << tmpBuf->value << " ";
      tmpBuf = ht[i]->next;
    }
    if (hasStuff)
      cerr << endl;
  }
}
#endif // DEBUGHASH

/// basic hash function for an unpredictable integer key
int hashFuncInt( const int& n, int numBuckets );

/// basic hash function for an unpredictable unsigned integer key
int hashFuncUInt( const unsigned int& n, int numBuckets );

/// hash function for string versions of job id's ("cluster.proc")
int hashFuncJobIdStr( const char* & key, int numBuckets );


#endif // HASH_H
