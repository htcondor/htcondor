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
#include "condor_common.h"
#include "condor_debug.h"
#include "condor_io.h"

SocketCache::SocketCache( int size )
{
	cacheSize = size;
	timeStamp = 0;
	sockCache = new sockEntry[size];
	if( !sockCache ) {
		EXCEPT( "SocketCache: Out of memory" );
	}
	for( int i = 0; i < size; i++ ) {
		initEntry( &(sockCache[i]) );
	}
}


SocketCache::~SocketCache()
{
	clearCache();
	delete [] sockCache;
}


void
SocketCache::resize( int size )
{
	if( size == cacheSize ) {
			// nothing to do!
		return;
	}
	if( size < cacheSize ) {
			// we don't gracefully support this yet.  if we really
			// supported this in a SocketCache where each socket is
			// registered with DaemonCore, we'd have to have a way to
			// deal with canceling the sockets...
		dprintf( D_ALWAYS, 
				 "ERROR: Cannot shrink a SocketCache with resize()\n" );
		return;
	}
	dprintf( D_FULLDEBUG, "Resizing SocketCache - old: %d new: %d\n",
			 cacheSize, size ); 
	sockEntry* new_cache = new sockEntry[size];
	int i;
	for( i=0; i<size; i++ ) {
		if( i<cacheSize && sockCache[i].valid ) {
			new_cache[i].valid = true;
			new_cache[i].sock = sockCache[i].sock;
			new_cache[i].timeStamp = sockCache[i].timeStamp;
			strcpy( new_cache[i].addr, sockCache[i].addr );
		} else {
			initEntry( &(new_cache[i]) );
		}
	}
	delete [] sockCache;
	cacheSize = size;
	sockCache = new_cache;
}


void
SocketCache::clearCache()
{
	for( int i = 0; i < cacheSize; i++ ) {
		invalidateEntry( i );
	}
}


void
SocketCache::invalidateSock( const char* addr )
{
	for( int i = 0; i < cacheSize; i++ ) {
		if( sockCache[i].valid && strcmp(addr,sockCache[i].addr) == 0 ) {
			invalidateEntry(i);
		}
	}
}


ReliSock* 
SocketCache::findReliSock( const char *addr )
{
    for( int i = 0; i < cacheSize; i++ ) {
        if( sockCache[i].valid && strcmp(addr,sockCache[i].addr) == 0 ) {
			return sockCache[i].sock;
		}
	}
	return NULL;
}


void
SocketCache::addReliSock( const char* addr, ReliSock* rsock )
{
	int slot = getCacheSlot();
	sockCache[slot].valid 		= true;
	sockCache[slot].timeStamp 	= timeStamp;
	sockCache[slot].sock 		= rsock;	
	strcpy(sockCache[slot].addr, addr);
}


bool
SocketCache::isFull( void )
{
	for( int i = 0; i < cacheSize; i++ ) {
		if( ! sockCache[i].valid ) {
			return false;
		}
	}
	return true;
}


int
SocketCache::size( void )
{
	return cacheSize;
}


int
SocketCache::getCacheSlot()
{
	int time	= INT_MAX;
	int	oldest  = -1;
	int	i;

	// increment time stamp
	timeStamp++;

	// find empty entry, or oldest entry
	for (i = 0; i < cacheSize; i++)
	{
		if (!sockCache[i].valid)
		{
			dprintf (D_FULLDEBUG, "SocketCache:  Found unused slot %d\n", i);
			return i;
		}

		if (time > sockCache[i].timeStamp)
		{
			oldest 	= i;
			time 	= sockCache[i].timeStamp;
		}
	}

	// evict the oldest
	dprintf (D_FULLDEBUG, "SocketCache:  Evicting old connection to %s\n", 
				sockCache[oldest].addr);
	invalidateEntry( oldest );
	return oldest;
}


void
SocketCache::invalidateEntry( int i )
{
	if( sockCache[i].valid ) {
		sockCache[i].sock->close();
		delete sockCache[i].sock;
	}
	initEntry( &(sockCache[i]) );
}


void
SocketCache::initEntry( sockEntry* entry )
{
	entry->valid = false;
	entry->addr[0] = '\0';
	entry->timeStamp = 0;
	entry->sock = NULL;
}
