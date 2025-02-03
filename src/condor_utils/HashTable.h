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

#include <utility>

// a generic hash bucket class

template <class Index, class Value>
class HashBucket {
 public:
  Index       index;                         // stored index
  Value      value;                          // associated value
  HashBucket<Index, Value> *next;            // next node in the hash table
};

template <class Index, class Value> class HashTable;

// Note that the HashIterator only works if both Index and Value are
// pointer types or can be assigned the value NULL.  wenger 2017-03-08
template< class Index, class Value >
class HashIterator 
{
public:
	HashIterator(const HashIterator &original) {
		m_parent = original.m_parent;
		m_idx = original.m_idx;
		m_cur = original.m_cur;
		m_parent->register_iterator(this);
	}

	~HashIterator() {
		m_parent->remove_iterator(this);
	}

	// Manually define all that the deprecated std::iterator used to do for us
	using iterator_category = std::input_iterator_tag;
	using value_type = std::pair<Index, Value>;
	using difference_type = std::ptrdiff_t;

	std::pair<Index, Value> operator *() const {
		if (m_cur) {
			return std::make_pair(m_cur->index, m_cur->value);
		}  
		return std::make_pair<Index, Value>({},{});
	}

	std::pair<Index, Value> operator ->() const {
		if (m_cur) {
			return std::make_pair(m_cur->index, m_cur->value);
		}  
		return std::make_pair<Index, Value>({},{});
	}

	/*
	 * Move the iterator forward by one entry in the hashtable.
	 * Unlike the '++' operator, this has no side-effects outside
	 * this object.
	 */
	void advance() {
		if (m_idx == -1) { return; }
		if (m_cur) m_cur = m_cur->next;
		while (!m_cur) {
			if (m_idx == m_parent->tableSize-1) {
				m_idx = -1;
				break;
			} else {
				m_cur = m_parent->ht[++m_idx];
			}
		}
	}

	HashIterator operator++(int) {
		// Note the copy constructor has the side-effect of
		// registering a new iterator with the parent.  Do not
		// call this from within the parent table itself.
		HashIterator<Index,Value> result = *this;
		advance();
		return result;
	}

	bool operator==(const HashIterator &rhs) const {
		if (this->m_parent != rhs.m_parent) {
			return false;
		}
		if (this->m_idx != rhs.m_idx) {
			return false;
		}
		if (this->m_cur != rhs.m_cur) {
			return false;
		}
		return true;
	}

private:
	friend class HashTable<Index, Value>;

	HashIterator(HashTable<Index, Value> *parent, int idx)
	  : m_parent(parent), m_idx(idx), m_cur(NULL)
	{
		if (idx == -1) return;
		m_cur = m_parent->ht[m_idx];
		while (!m_cur) {
			if (m_idx == m_parent->tableSize-1) {
				m_idx = -1;
				break;
			} else {
				m_cur = m_parent->ht[++m_idx];
			}
		}
		m_parent->register_iterator(this);
	}

	HashTable<Index, Value> *m_parent;
	int m_idx;
	HashBucket<Index, Value> *m_cur;
};

// a generic hash table class

// IMPORTANT NOTE: Index must be a class on which == works.

template <class Index, class Value>
class HashTable {
 public:
  typedef HashIterator<Index, Value> iterator;
  friend class HashIterator<Index, Value>;

  HashTable( size_t (*hashfcn)( const Index &index ) );
  HashTable( const HashTable &copy);
  const HashTable& operator=(const HashTable &copy);
  ~HashTable();

  int insert(const Index &index, const Value &value, bool update = false);
  int lookup(const Index &index, Value &value) const;
  int lookup(const Index &index, Value* &value) const;
	  // returns 0 if exists, -1 otherwise
  int exists(const Index &index) const;
  int remove(const Index &index);  
  int getNumElements( ) const { return numElems; }
  int getTableSize( ) const { return tableSize; }
  int clear();

  void startIterations (void);
  int  iterate (Value &value);
  int  getCurrentKey (Index &index);
  int  iterate (Index &index, Value &value);
  int  iterate_nocopy(const Index ** pindex, Value ** pvalue);
  int  iterate_stats(int & ix_bucket, int & ix_item);

  iterator begin() {return iterator(this, 0);}
  iterator end() {return iterator(this, -1);}

  /*
  Walk the table, calling walkfunc() on every member.
  If walkfunc() ever returns zero, the walk is stopped.
  Returns true if all walkfuncs() succeed, false
  if stopped. Walk() is provided so that multiple walks
  can be done even if a startIterations() is in progress.
  */
  int walk( int (*walkfunc) ( Value value ) );


 private:
  void register_iterator(iterator* it);
  void remove_iterator(iterator* it);

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
#ifdef DEBUGHASH
  void dump();                                  // dump contents of hash table
#endif

  int tableSize;                                // size of hash table
  int numElems; // number of elements in the hashtable
  HashBucket<Index, Value> **ht;                // actual hash table
  size_t (*hashfcn)(const Index &index);  // user-provided hash function
  double maxLoadFactor;			// average number of elements per bucket list
  int currentBucket;
  HashBucket<Index, Value> *currentItem;
  std::vector<iterator*> activeIterators;
};

// The two normal hash table constructors call initialize() to set everything up
// In the first constructor, tableSz is ignored as it is no longer used, it is
// left in for compatability reasons.
template <class Index, class Value>
HashTable<Index,Value>::HashTable( size_t (*hashF)( const Index &index ) ) {
  int i;

  hashfcn = hashF;

  maxLoadFactor = 0.8;		// default "table density"

  // You MUST specify a hash function.
  // Try hashFuncInt (int), hashFuncUInt (uint), hashFuncJobIdStr (string of "cluster.proc"),
  // or hashFunction(<string type>)
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
  currentBucket = -1; // no current bucket
  currentItem = 0; // no current item
  numElems = 0;
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

// Register an iterator
template <class Index, class Value>
void
HashTable<Index,Value>::register_iterator(typename HashTable<Index,Value>::iterator* it) {
	activeIterators.push_back(it);
}

template <class Index, class Value>
void
HashTable<Index,Value>::remove_iterator(typename HashTable<Index,Value>::iterator* dead_it) {
	typename std::vector<iterator*>::iterator it;
	for (it = activeIterators.begin();
		it != activeIterators.end();
		++it)
	{
		if (dead_it == *it) {
			activeIterators.erase(it);
			break;
		}
	}
	if(needs_resizing()) {
		resize_hash_table();
	}
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
  currentItem = 0; // Ensure set, even if unset/invalid in source copy.
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
  maxLoadFactor = copy.maxLoadFactor;
}

// Insert entry into hash table mapping Index to Value.
// Returns 0 if OK, -1 if update is false (the default)
// and the item already exists.

template <class Index, class Value>
int HashTable<Index,Value>::insert(const Index &index,const  Value &value, bool update)
{
  size_t idx = hashfcn(index) % tableSize;

  HashBucket<Index, Value> *bucket;

  bucket = ht[idx];
  while( bucket ) {
    if( bucket->index == index ) {
      // This key is already in the table, decide what to do about that
      if ( update ) {
        //  update the value in the table
        bucket->value = value;
        return 0;
      } else {
        // reject as a duplicate
        return -1;
      }
    }
    bucket = bucket->next;
  }

  // This is a new key, add it
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

  size_t idx = hashfcn(index) % tableSize;

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

  size_t idx = hashfcn(index) % tableSize;

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

  size_t idx = hashfcn(index) % tableSize;

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

// Delete Index entry from hash table. Return OK (0) if index was found.
// Else return -1.

template <class Index, class Value>
int HashTable<Index,Value>::remove(const Index &index)
{
  	size_t idx = hashfcn(index) % tableSize;

  	HashBucket<Index, Value> *bucket = ht[idx];
  	HashBucket<Index, Value> *prevBuc = ht[idx];

  	while(bucket) 
	{
    	if (bucket->index == index) 
		{
      		if (bucket == ht[idx]) 
			{
				// The item we're deleting is the first one for
				// this index.

				ht[idx] = bucket->next;

				// if the item being deleted is being iterated, ensure that
				// next iteration returns the object "after" this one
				if (bucket == currentItem)
				{
					currentItem = 0;
						// -1 means no current bucket.  (Change here
						// fixes gittrac #6177.  wenger 2017-03-16)
					if (--currentBucket < 0) currentBucket = -1;
				}
			}
      		else
			{
				// The item we're deleting is NOT the first one for
				// this index.

				prevBuc->next = bucket->next;

				// Again, take care of the iterator
				if (bucket == currentItem)
				{
					currentItem = prevBuc;
				}
			}

			// Invalidate all active iterators that point to this object.
			for (typename std::vector<iterator*>::iterator it=activeIterators.begin();
				it != activeIterators.end();
				it++)
			{
				if (bucket == (*it)->m_cur)
				{
					// These iterators must move forward!  The current iterator may be dereferenced
					// before being incremented.  Hence, it must point at a valid object and it must
					// not return a value already seen
					(*it)->advance();
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

	// Change all existing iterators to point at the end.
	for (typename std::vector<iterator*>::iterator it=activeIterators.begin();
		it != activeIterators.end();
		it++)
	{
		(*it)->m_idx = -1;
		(*it)->m_cur = NULL;
	}

  numElems = 0;

  return 0;
}

template <class Index, class Value>
void HashTable<Index,Value>::
startIterations (void)
{
		// No current bucket.
    currentBucket = -1;
		// No current item.
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
int HashTable<Index,Value>::
iterate_nocopy (const Index **pindex, Value ** pv)
{
    // try to get next item in the current bucket chain ...
    if (currentItem) {
        currentItem = currentItem->next;
    
        // ... if successful, return OK
        if (currentItem) {
            *pindex = &currentItem->index;
            *pv = &currentItem->value;
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

	*pindex = &currentItem->index;
	*pv = &currentItem->value;
	return 1;
}

// this iterator helps to gather statistics about the hashtable fill ratio
template <class Index, class Value>
int HashTable<Index,Value>::
iterate_stats (int & ix_bucket, int & ix_item)
{
	// try to get next item in the current bucket chain ...
	if (currentItem) {
		currentItem = currentItem->next;

		// ... if successful, return OK
		if (currentItem) {
			++ix_item;
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
			ix_bucket = -1;
			ix_item = tableSize;
			return 0;
		}
		currentItem = ht[currentBucket];
	} while ( !currentItem );

	ix_bucket = currentBucket;
	ix_item = 0;
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
		// Resizing destroys active iterators.
	if (activeIterators.size()) return 0;
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
			size_t idx = hashfcn(cur->index) % newsize;
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
size_t hashFuncInt( const int& n );

/// basic hash function for an unpredictable integer key
size_t hashFuncLong( const long& n );

/// basic hash function for an unpredictable unsigned integer key
size_t hashFuncUInt( const unsigned int& n );

/// hash functions for a string
size_t hashFunction( char const *key );
size_t hashFunction( const std::string &key );
size_t hashFunction( const YourString &key );

size_t hashFunction( const YourStringNoCase &key );
size_t hashFunction( const istring &key );

/// hash function for a pointer
size_t hashFuncVoidPtr( void* const & pv );

#endif // HASH_H
