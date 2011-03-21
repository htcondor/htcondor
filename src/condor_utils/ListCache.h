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


#ifndef LIST_CACHE_H
#define LIST_CACHE_H

#include "list.h"
#include <stdlib.h>

/*
ListCache is a combination list and cache.
Each entry in the list has an expiration time.
Objects are removed from the list as they expire.
The cache has a limited size, so inserts will also push
out the oldest object, regardless of whether it has expired.
Lookups on an object cause it to be pulled to the top
of the list.
*/

template <class Value>
class ListCacheEntry {
public:
	ListCacheEntry( time_t e, Value *v ) {
		expires = e;
		value = new Value(*v);
	}
	~ListCacheEntry() {
		delete value;
	}
	time_t expires;
	Value *value;
};

template <class Value>
class ListCache {
public:
	/* Create a cache with this size and hash function */
	ListCache( int size );

	/* Insert an object with this maximum lifetime in seconds. */
	/* Return true on success, false otherwise */
	int insert( Value *value, time_t lifetime );

	/* Find an unexpired value equal to 'search' */
	/* If found, move it to the top of the list */
	/* and return a pointer.  Otherwise, return zero. */
	Value * lookup( Value *search );

	/* Find an unexpired value greater than or equal to 'search' */
	/* If found, move it to the top of the list */
	/* and return a pointer.  Otherwise, return zero. */
	Value * lookup_lte( Value *search );

private:
	List<ListCacheEntry<Value> > list;
	int maxsize;
};


template <class Value>
ListCache<Value>::ListCache( int size )
{
	maxsize = size;
}

template <class Value>
int ListCache<Value>::insert( Value *value, time_t lifetime )
{
	ListCacheEntry<Value> *entry;

	entry = new ListCacheEntry<Value> ( time(0)+lifetime, value );
	
	list.Rewind();
	list.Insert( entry );

	if( list.Length() >= maxsize ) {
		entry = 0;
		while(!list.AtEnd()) {
			entry = list.Next();
		}
		if(entry) {
			delete entry;
			list.DeleteCurrent();
		}
	}

	return 1;
}

template <class Value>
Value * ListCache<Value>::lookup( Value *search )
{
	ListCacheEntry<Value> *entry;
	time_t current;

	current = time(0);

	list.Rewind();
	while( entry = list.Next() ) {
		if( entry->expires < current ) {
			delete entry;
			list.DeleteCurrent();
		} else if( *search == *(entry->value) ) {
			list.DeleteCurrent();
			list.Rewind();
			list.Insert(entry);
			return entry->value;
		}
	}

	return 0;
}

#include "condor_debug.h"

template <class Value>
Value * ListCache<Value>::lookup_lte( Value *search )
{
	ListCacheEntry<Value> *entry;
	time_t current;

	current = time(0);

	list.Rewind();
	while( entry = list.Next() ) {
		if( entry->expires < current ) {
			delete entry;
			list.DeleteCurrent();
		} else if( *search <= *(entry->value) ) {
			list.DeleteCurrent();
			list.Rewind();
			list.Insert(entry);
			return entry->value;
		}
	}

	return 0;
}

#endif



































































































