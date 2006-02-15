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
#ifndef HASH_H
#define HASH_H

#include "condor_common.h"
#include "condor_debug.h"
#include "proc.h"	// for PROC_ID

// a generic hash bucket class

template <class Index, class Value>
class HashBucket {
 public:
  Index       index;                         // stored index
  Value      value;                          // associated value
  HashBucket<Index, Value> *next;            // next node in the hash table
};

// a generic hash table class

// various options for what we do when someone tries to insert a new
// bucket with a key (index) that already exists in the table
typedef enum {
	allowDuplicateKeys,   // original (arguably broken) default behavior
	rejectDuplicateKeys,
	updateDuplicateKeys,
} duplicateKeyBehavior_t;

template <class Index, class Value>
class HashTable {
 public:
  HashTable( int tableSize,
			 int (*hashfcn)( const Index &index, int numBuckets ),
			 duplicateKeyBehavior_t behavior = allowDuplicateKeys );
  HashTable( const HashTable &copy);
  const HashTable& operator=(const HashTable &copy);
  ~HashTable();

  int insert(const Index &index, const Value &value);
  int lookup(const Index &index, Value &value);
  int lookup(const Index &index, Value* &value);
  int getNext(Index &index, void *current, Value &value,
	      void *&next);
  int remove(const Index &index);  
  int getNumElements( ) { return numElems; }
  int getTableSize( ) { return tableSize; }
  int clear();

  void startIterations (void);
  int  iterate (Value &value);
  int  getCurrentKey (Index &index);
  int  iterate (Index &index, Value &value);


  /*
  Walk the table, calling walkfunc() on every member.
  If walkfunc() ever returns zero, the walk is stopped.
  Returns true if all walkfuncs() succeed, false
  if stopped. Walk() is provided so that multiple walks
  can be done even if a startIterations() is in progress.
  */
  int walk( int (*walkfunc) ( Value value ) );


 private:
  void copy_deep(const HashTable<Index, Value> &copy);
#ifdef DEBUGHASH
  void dump();                                  // dump contents of hash table
#endif

  int tableSize;                                // size of hash table
  HashBucket<Index, Value> **ht;                // actual hash table
  int (*hashfcn)(const Index &index, int numBuckets); // user-provided hash function
  duplicateKeyBehavior_t duplicateKeyBehavior;        // duplicate key behavior
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
HashTable<Index,Value>::HashTable( int tableSz,
								   int (*hashF)( const Index &index,
												 int numBuckets),
								   duplicateKeyBehavior_t behavior ) :
	tableSize(tableSz), hashfcn(hashF)
{
  int i,j,k;


  // You MUST specify a hash function.
  // Try hashFuncInt (int), hashFuncUInt (uint), hashFuncJobIdStr (string of "cluster.proc"),
  // or MyStringHash (MyString)
  ASSERT(hashfcn != 0);

  // Do not allow tableSize=0 since that is likely to
  // result in a divide by zero in many hash functions.
  // If the user specifies an illegal table size, use
  // a default value of 5.
  if ( tableSize < 1 ) {
	  tableSize = 5;
  }

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
  duplicateKeyBehavior = behavior;
}

// Copy constructor

template <class Index, class Value>
HashTable<Index,Value>::HashTable( const HashTable<Index,Value>& copy ) {
  copy_deep(copy);
}

// Assignment

template <class Index, class Value>
const HashTable<Index,Value>& HashTable<Index,Value>::operator=( const HashTable<Index,Value>& copy ) {
  // don't copy ourself!
  if (this != &copy) {
    clear();
    delete [] ht;
    delete [] chainsUsed;
    copy_deep(copy);
  }

  // return a reference to ourself
  return *this;
}

// Do a deep copy into ourself

template <class Index, class Value>
void HashTable<Index,Value>::copy_deep( const HashTable<Index,Value>& copy ) {
  // we don't need to verify that tableSize is valid and prime, as this will
  // already have been taken care of when the copy was constructed.
  tableSize = copy.tableSize;

  if (!(ht = new HashBucket<Index, Value>* [tableSize])) {
    cerr << "Insufficient memory for hash table" << endl;
    exit(1);
  }
  if (!(chainsUsed = new int[tableSize])) {
    cerr << "Insufficient memory for hash table (chainsUsed array)" << endl;
    exit(1);
  }
  for(int i = 0; i < tableSize; i++) {
    // duplicate this chain
    HashBucket<Index, Value> **our_next = &ht[i];
    HashBucket<Index, Value> *copy_next = copy.ht[i];
    while (copy_next) {
      // copy this bucket
      *our_next = new HashBucket<Index, Value>(*copy_next);

      // if this is the copy's current item, make it ours as well,
	  // but point to _our_ bucket, not the copy's.
      if (copy_next == copy.currentItem) {
        currentItem = *our_next;
      }

      // move to the next item
      our_next = &((*our_next)->next);
      copy_next = copy_next->next;
    }

    // terminate the chain
    *our_next = NULL;

    chainsUsed[i] = copy.chainsUsed[i];
  }

  // take the rest of the object (it's all shallow data)
  currentBucket = copy.currentBucket;
  chainsUsedLen = copy.chainsUsedLen;
  numElems = copy.numElems;
  endOfFreeList = copy.endOfFreeList;
  chainsUsedFreeList = copy.chainsUsedFreeList;
  hashfcn = copy.hashfcn;
  duplicateKeyBehavior = copy.duplicateKeyBehavior;
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
    dprintf( D_ALWAYS, "hashfcn() is broken "
			 "(returned %d when tablesize = %d)!\n", idx, tableSize );
    return -1;
  }

  HashBucket<Index, Value> *bucket;

  // if rejectDuplicateKeys is set and a bucket already exists in the
  // table with this key, return -1

  if ( duplicateKeyBehavior == rejectDuplicateKeys ) {
	  bucket = ht[idx];
	  while (bucket) {
		  if (bucket->index == index) {
			  // found!  return error because rejectDuplicateKeys is set
			  return -1;
		  }
		  bucket = bucket->next;
	  }
  }

  // if updateDuplicateKeys is set and a bucket already exists in the
  // table with this key, update the bucket's value

  else if( duplicateKeyBehavior == updateDuplicateKeys ) {
	  bucket = ht[idx];
	  while( bucket ) {
          if( bucket->index == index ) {
			  bucket->value = value;
			  return 0;
          }
          bucket = bucket->next;
      }
  }

  // don't worry about whether a bucket already exists with this key,
  // just go ahead and insert another one...

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


template <class Index, class Value>
int HashTable<Index,Value>::walk( int (*walkfunc) ( Value value ) )
{
	HashBucket<Index,Value> *current;
	int i;

	for( i=0; i<tableSize; i++ ) {
		for( current=ht[i]; current; current=current->next ) {
			if(!walkfunc( current->value )) return 0;
		}
	}

	return 1;
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
int hashFuncJobIdStr( char* const & key, int numBuckets );

/// hash function for PROC_ID versions of job ids (cluster.proc)
int hashFuncPROC_ID( const PROC_ID &procID, int numBuckets);

/// hash function for char* string
int hashFuncChars(char const *key, int numBuckets);

#endif // HASH_H
