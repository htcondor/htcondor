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

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_io.h"

SocketCache::SocketCache( int cSize )
{
	cacheSize = cSize;
	timeStamp = 0;
	sockCache = new sockEntry[cSize];
	if( !sockCache ) {
		EXCEPT( "SocketCache: Out of memory" );
	}
	for( int i = 0; i < cSize; i++ ) {
		initEntry( &(sockCache[i]) );
	}
}


SocketCache::~SocketCache()
{
	clearCache();
	delete [] sockCache;
}


void
SocketCache::resize( int newSize )
{
	if( newSize == cacheSize ) {
			// nothing to do!
		return;
	}
	if( newSize < cacheSize ) {
			// we don't gracefully support this yet.  if we really
			// supported this in a SocketCache where each socket is
			// registered with DaemonCore, we'd have to have a way to
			// deal with canceling the sockets...
		dprintf( D_ALWAYS, 
				 "ERROR: Cannot shrink a SocketCache with resize()\n" );
		return;
	}
	dprintf( D_FULLDEBUG, "Resizing SocketCache - old: %d new: %d\n",
			 cacheSize, newSize ); 
	sockEntry* new_cache = new sockEntry[newSize];
	int i;
	for( i=0; i<newSize; i++ ) {
		if( i<cacheSize && sockCache[i].valid ) {
			new_cache[i].valid = true;
			new_cache[i].sock = sockCache[i].sock;
			new_cache[i].timeStamp = sockCache[i].timeStamp;
			new_cache[i].addr = sockCache[i].addr;
		} else {
			initEntry( &(new_cache[i]) );
		}
	}
	delete [] sockCache;
	cacheSize = newSize;
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
		if( sockCache[i].valid && addr == sockCache[i].addr ) {
			invalidateEntry(i);
		}
	}
}


ReliSock* 
SocketCache::findReliSock( const char *addr )
{
    for( int i = 0; i < cacheSize; i++ ) {
        if( sockCache[i].valid && addr == sockCache[i].addr ) {
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
	sockCache[slot].addr = addr;
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
SocketCache::size( void ) const
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
			 sockCache[oldest].addr.c_str());
	if(oldest != -1) {
		invalidateEntry( oldest );
	}
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
	entry->addr = "";
	entry->timeStamp = 0;
	entry->sock = NULL;
}
