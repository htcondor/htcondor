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

#ifndef HASH_CACHE_H
#define HASH_CACHE_H

#include "HashTable.h"
#include <stdlib.h>

/*
HashCache is a combination hashtable and cache.
Each entry is a mapping from an index type to a value type.
In addition, every object has an expiration time.
Objects are removed from the cache as they expire.
The cache has a limited size, so inserts will also push
out the oldest object, regardless of whether it has expired.
*/

template <class Value>
class HashCacheEntry {
public:
	HashCacheEntry () {
	}
	HashCacheEntry( time_t e, const Value & v ) {
		expires = e;
		value = v;
	}
	time_t expires;
	Value value;
};

template <class Index, class Value>
class HashCache {
public:
	/* Create a cache with this size and hash function */
	HashCache( int size, int (*hash) ( const Index &index, int buckets ) );

	/* Insert an object with this maximum lifetime in seconds. */
	/* If an eviction is necessary, send back the evicted value. */
	/* Return values as in HashTable */
	int insert( const Index & index, const Value & value, time_t lifetime, Value & evicted);

	/* Find an unexpired value with this index */
	/* Return values as in HashTable */
	int lookup( const Index & index, Value & value );

private:
	HashTable<Index,HashCacheEntry<Value> > table;
	int maxsize;

	/* Find the oldest entry and remove it. */
	void evict( Value & evicted );
};


template <class Index, class Value>
HashCache<Index,Value>::HashCache
( int size, int (*hash) ( const Index & index, int buckets ) )
: table(size,hash)
{
	maxsize = size;
}

template <class Index, class Value>
void HashCache<Index,Value>::evict( Value & evicted )
{
	HashCacheEntry<Value> entry;
	Index index;
	time_t lowest_expires=0;
	Index lowest_index;

	table.startIterations();
	while( table.iterate(index,entry) ) {
		if( (lowest_expires==0) || (entry.expires<lowest_expires) ) {
			lowest_expires = entry.expires;
			lowest_index = index;
		}
	}

	table.lookup(lowest_index,entry);
	evicted = entry.value;

	table.remove(lowest_index);
}

template <class Index, class Value>
int HashCache<Index,Value>::insert( const Index & index, const Value & value, time_t lifetime, Value & evicted )
{
	if( table.getNumElements()>=maxsize ) {
		evict(evicted);
	}

	HashCacheEntry<Value> newentry(time(0)+lifetime,value);
	return table.insert(index,newentry);
}

template <class Index, class Value>
int HashCache<Index,Value>::lookup( const Index & index, Value & value )
{
	HashCacheEntry<Value> entry;
	
	if(table.lookup(index,entry)==0) {
		if( entry.expires>time(0) ) {
			value = entry.value;
			return 0;
		} else {
			table.remove(index);
			return -1;
		}
	} else {
		return -1;
	}
}

#endif



































































































