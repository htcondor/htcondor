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



































































































