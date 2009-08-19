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

#ifndef HASH_H
#define HASH_H

#include "condor_common.h"
#include "condor_debug.h"
#include "MyString.h"

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
	allowDuplicateKeys,   // original (inarguably broken) default behavior
	rejectDuplicateKeys,
	updateDuplicateKeys,
} duplicateKeyBehavior_t;

// IMPORTANT NOTE: Index must be a class on which == works.

template <class Index, class Value>
class HashTable {
 public:
    // the first constructor takes a tableSize that isn't used, it's left in
	// for compatibility reasons
  HashTable( int tableSize,
			 unsigned int (*hashfcn)( const Index &index ),
			 duplicateKeyBehavior_t behavior = allowDuplicateKeys );
    // with this constructor, duplicateKeyBehavior_t is ALWAYS set to
    // rejectDuplicateKeys.  To have it work like updateDuplicateKeys,
    // use replace() instead of insert().
  HashTable( unsigned int (*hashfcn)( const Index &index ));
  void initialize( unsigned int (*hashfcn)( const Index &index ),
			 duplicateKeyBehavior_t behavior );
  HashTable( const HashTable &copy);
  const HashTable& operator=(const HashTable &copy);
  ~HashTable();

  int insert(const Index &index, const Value &value);
  /*
  Replace the old value with a new one.  Returns the old value, or NULL if it
  didn't previously exist.
  */
  int lookup(const Index &index, Value &value) const;
  int lookup(const Index &index, Value* &value) const;
	  // returns 0 if exists, -1 otherwise
  int exists(const Index &index) const;
  int getNext(Index &index, void *current, Value &value,
	      void *&next) const;
  int remove(const Index &index);  
  int getNumElements( ) const { return numElems; }
  int getTableSize( ) const { return tableSize; }
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
  /* Deeply copy the hash table. */
  void copy_deep(const HashTable<Index, Value> &copy);
  /*
  Determines if the table needs to be resized based on the current load factor.
  */
  int needs_resizing();
  /*
  Resize the hash table to the given size, or a default of the next (2^n)-1.
  */
  void resize_hash_table(int newsize = -1);
  /*
  Adds an item, ignoring the possibility of duplicates.  Used in insert() and
  replace() internally.
  */
  int addItem(const Index &index, const Value &value);
#ifdef DEBUGHASH
  void dump();                                  // dump contents of hash table
#endif

  int tableSize;                                // size of hash table
  HashBucket<Index, Value> **ht;                // actual hash table
  unsigned int (*hashfcn)(const Index &index);  // user-provided hash function
  double maxLoadFactor;			// average number of elements per bucket list
  duplicateKeyBehavior_t duplicateKeyBehavior;        // duplicate key behavior
  int currentBucket;
  HashBucket<Index, Value> *currentItem;
  int numElems; // number of elements in the hashtable
};

// The two normal hash table constructors call initialize() to set everything up
// In the first constructor, tableSz is ignored as it is no longer used, it is
// left in for compatability reasons.
template <class Index, class Value>
HashTable<Index,Value>::HashTable( int /* tableSz */,
								   unsigned int (*hashF)( const Index &index ),
								   duplicateKeyBehavior_t behavior ) {
	initialize(hashF, behavior);
}

template <class Index, class Value>
HashTable<Index,Value>::HashTable( unsigned int (*hashF)( const Index &index )) {
	initialize(hashF, rejectDuplicateKeys);
}




// Construct hash table. Allocate memory for hash table and
// initialize its elements.

template <class Index, class Value>
void HashTable<Index,Value>::initialize( unsigned int (*hashF)( const Index &index ),
								         duplicateKeyBehavior_t behavior ) {
  int i;

  hashfcn = hashF;

  maxLoadFactor = 0.8;		// default "table density"

  // You MUST specify a hash function.
  // Try hashFuncInt (int), hashFuncUInt (uint), hashFuncJobIdStr (string of "cluster.proc"),
  // or MyStringHash (MyString)
  ASSERT(hashfcn != 0);

  // if the value for maxLoadFactor is negative or 0, use the default of 50

  // set tableSize before everything else b/c it's needed for the array declarations
  tableSize = 7;

  if (!(ht = new HashBucket<Index, Value>* [tableSize])) {
    EXCEPT("Insufficient memory for hash table");
  }
  for(i = 0; i < tableSize; i++) {
    ht[i] = NULL;
  }
  currentBucket = -1;
  currentItem = 0;
  numElems = 0;
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
    EXCEPT("Insufficient memory for hash table");
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

  }

  // take the rest of the object (it's all shallow data)
  currentBucket = copy.currentBucket;
  numElems = copy.numElems;
  hashfcn = copy.hashfcn;
  duplicateKeyBehavior = copy.duplicateKeyBehavior;
}

// Insert entry into hash table mapping Index to Value.
// Returns 0 if OK, -1 if rejectDuplicateKeys is set (the default for the
// single-argument constructor) and the item already exists.

template <class Index, class Value>
int HashTable<Index,Value>::insert(const Index &index,const  Value &value)
{
  int idx = (int)(hashfcn(index) % tableSize);

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

  addItem(index, value);
  return 0;
}

template <class Index, class Value>
int HashTable<Index,Value>::addItem(const Index &index,const  Value &value) {
  int idx = (int)(hashfcn(index) % tableSize);

  HashBucket<Index, Value> *bucket;

  // don't worry about whether a bucket already exists with this key,
  // just go ahead and insert another one...

  if (!(bucket = new HashBucket<Index, Value>)) {
    EXCEPT("Insufficient memory");
  }

  bucket->index = index;
  bucket->value = value;
  bucket->next = ht[idx];
  ht[idx] = bucket;

#ifdef DEBUGHASH
  dump();
#endif

  numElems++;
   // bucket successfully added, now check to see if the table is too full
  if(needs_resizing()) {
    resize_hash_table();
  }
  return 0;
}

// Check if Index is currently in the hash table. If so, return
// corresponding value and OK status (0). Otherwise return -1.

template <class Index, class Value>
int HashTable<Index,Value>::lookup(const Index &index, Value &value) const
{
  if ( numElems == 0 ) {
	return -1;
  }

  int idx = (int)(hashfcn(index) % tableSize);

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
int HashTable<Index,Value>::lookup(const Index &index, Value* &value ) const
{
  if ( numElems == 0 ) {
	return -1;
  }

  int idx = (int)(hashfcn(index) % tableSize);

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


// Check if Index is currently in the hash table. If so, return
// OK status (0). Otherwise return -1.
template <class Index, class Value>
int HashTable<Index,Value>::exists(const Index &index) const
{
  if ( numElems == 0 ) {
	return -1;
  }

  int idx = (int)(hashfcn(index) % tableSize);

  HashBucket<Index, Value> *bucket = ht[idx];
  while(bucket) {
#ifdef DEBUGHASH
    cerr << "%%  Comparing " << *(long *)&bucket->index
         << " vs. " << *(long *)index << endl;
#endif
    if (bucket->index == index) {
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
/*	This function doesn't appear to be used the the Condor sources,
	no unit test written.
*/
template <class Index, class Value>
int HashTable<Index,Value>::getNext(Index &index, void *current,
				    Value &value, void *&next) const
{
  HashBucket<Index, Value> *bucket;

  if (!current) {
    int idx = (int)(hashfcn(index) % tableSize);
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
  	int idx = (int)(hashfcn(index) % tableSize);

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

  numElems = 0;

  return 0;
}

template <class Index, class Value>
void HashTable<Index,Value>::
startIterations (void)
{
    currentBucket = -1;
	currentItem = 0;
}


template <class Index, class Value>
int HashTable<Index,Value>::
iterate (Value &v)
{
    	// try to get next item in the current bucket chain ...
    if (currentItem) {
        currentItem = currentItem->next;
    
        // ... if successful, return OK
        if (currentItem) {
            v = currentItem->value;
            return 1;
       }
    }

		// the current bucket chain is expended, so find the next non-NULL
		// bucket and continue from there
	do {
		currentBucket++;
		if (currentBucket >= tableSize) {
			// end of hash table ... no more entries
			currentBucket = -1;
			currentItem = 0;

			return 0;
		}
		currentItem = ht[currentBucket];
	} while ( !currentItem );

	v = currentItem->value;
	return 1;
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
    	// try to get next item in the current bucket chain ...
    if (currentItem) {
        currentItem = currentItem->next;
    
        // ... if successful, return OK
        if (currentItem) {
			index = currentItem->index;
            v = currentItem->value;
            return 1;
       }
    }

		// the current bucket chain is expended, so find the next non-NULL
		// bucket and continue from there
	do {
		currentBucket++;
		if (currentBucket >= tableSize) {
			// end of hash table ... no more entries
			currentBucket = -1;
			currentItem = 0;

			return 0;
		}
		currentItem = ht[currentBucket];
	} while ( !currentItem );

	index = currentItem->index;
	v = currentItem->value;
	return 1;
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
}

// Determine if the hash table should be resized and reindexed
template <class Index, class Value>
int HashTable<Index, Value>::needs_resizing() {
	if(((double) numElems / (double) tableSize) >= maxLoadFactor) {
		return 1;
	}
	return 0;
}

// Resize and reindex the hash table
template <class Index, class Value>
void HashTable<Index, Value>::resize_hash_table(int newsize) {
	if(newsize <= 0) {
			// default to next 2^n-1 value
		newsize = tableSize + 1;
		newsize *= 2;
		newsize--;
	}
	HashBucket<Index, Value> **htcopy;
	if (!(htcopy = new HashBucket<Index, Value>* [newsize])) {
		EXCEPT("Insufficient memory for hash table resizing");
	}
	int i;
	for(i = 0; i < newsize; i++) {
		htcopy[i] = NULL;
	}

	HashBucket<Index, Value> *temp = NULL;
	HashBucket<Index, Value> *cur= NULL;
	for( i=0; i<tableSize; i++ ) {
		for( cur=ht[i]; cur; cur=temp ) {
			int idx = (int)(hashfcn(cur->index) % newsize);
				// put it at htcopy[idx]
				// if htcopy[idx] wasn't NULL then put whatever was there as its next value...
				// if it was NULL then set current->next to NULL, so I guess just copy the value no matter what
			temp = cur->next;
			cur->next = htcopy[idx];
			htcopy[idx] = cur;
		}
	}
	delete[] ht;
	ht = htcopy;
	currentItem = 0;
	currentBucket = -1;
	tableSize = newsize;
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
unsigned int hashFuncInt( const int& n );

/// basic hash function for an unpredictable integer key
unsigned int hashFuncLong( const long& n );

/// basic hash function for an unpredictable unsigned integer key
unsigned int hashFuncUInt( const unsigned int& n );

/// hash function for string versions of job id's ("cluster.proc")
unsigned int hashFuncJobIdStr( char* const & key );

/// hash function for char* string
unsigned int hashFuncChars( char const *key );

/// hash function for Mystring string
unsigned int hashFuncMyString( const MyString &key );


#endif // HASH_H
