#ifndef HASH_H
#define HASH_H

#ifdef __GNUG__
#pragma interface
#endif

#include <iostream.h>

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
	    int (*hashfcn)(Index &index,
			   int numBuckets)); // constructor  
  ~HashTable();                              // destructor

  int insert(Index &index, Value &value);
  int lookup(Index &index, Value &value);
  int getNext(Index &index, void *current, Value &value, void *&next);
  int remove(Index &index);  
  int clear();

  void startIterations (void);
  int  getCurrentKey   (Index &);
  int  iterate (Value &);
    
 private:
#ifdef DEBUGHASH
  void dump();                                  // dump contents of hash table
#endif

  int tableSize;                                // size of hash table
  HashBucket<Index, Value> **ht;                // actual hash table
  int (*hashfcn)(Index &index, int numBuckets); // user-provided hash function

  int currentBucket;
  HashBucket<Index, Value> *currentItem;
};

// Construct hash table. Allocate memory for hash table and
// initialize its elements.

template <class Index, class Value>
HashTable<Index,Value>::HashTable(int tableSz,
				  int (*hashF)(Index &index,
					       int numBuckets)) :
	tableSize(tableSz), hashfcn(hashF)
{
  if (!(ht = new HashBucket<Index, Value>* [tableSize])) {
    cerr << "Insufficient memory for hash table" << endl;
    exit(1);
  }
  for(int i = 0; i < tableSize; i++)
    ht[i] = NULL;
}

// Insert entry into hash table mapping Index to Value.
// Returns 0 if OK, an error code otherwise.

template <class Index, class Value>
int HashTable<Index,Value>::insert(Index &index, Value &value)
{
  int idx = hashfcn(index, tableSize);

  HashBucket<Index, Value> *bucket;
  if (!(bucket = new HashBucket<Index, Value>)) {
    cerr << "Insufficient memory" << endl;
    return -1;
  }

  bucket->index = index;
  bucket->value = value;
  bucket->next = ht[idx];
  ht[idx] = bucket;

#ifdef DEBUGHASH
  dump();
#endif

  return 0;
}

// Check if Index is currently in the hash table. If so, return
// corresponding value and OK status (0). Otherwise return -1.

template <class Index, class Value>
int HashTable<Index,Value>::lookup(Index &index, Value &value)
{
  int idx = hashfcn(index, tableSize);

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
int HashTable<Index,Value>::remove(Index &index)
{
  	int idx = hashfcn(index, tableSize);

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
getCurrentKey (Index &index)
{
	if (!currentItem) return -1;
	index = currentItem->index;
	return 0;
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

	do
	{
		// try next bucket ...
		currentBucket ++;
	} while (currentBucket < tableSize && ht[currentBucket] == NULL);

	if (currentBucket == tableSize)
	{
    	// end of hash table ... no more entries
    	currentBucket = -1;
    	currentItem = 0;

    	return 0;
	}
	else
	{
		currentItem = ht[currentBucket];
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
  delete ht;
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
#endif

#endif
