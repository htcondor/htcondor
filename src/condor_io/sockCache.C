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
#include "sockCache.h"


SocketCache::
SocketCache(int size)
{
	cacheSize = size;
	timeStamp = 0;
	sockCache = new sockEntry[size];
	if (!sockCache)
	{
		EXCEPT ("SocketCache:  Out of memory");
	}

	for (int i = 0; i < size; i++)
		sockCache[i].valid = false;
}


SocketCache::
~SocketCache()
{
	clearCache();
	delete [] sockCache;
}


void SocketCache::
clearCache()
{
	for (int i = 0; i < cacheSize; i++) {
		if (sockCache[i].valid) {
			sockCache[i].sock->close();
			delete sockCache[i].sock;
			sockCache[i].valid = false;
		}	
	}
}


void SocketCache::
invalidateSock (char *sock)
{
	for (int i = 0; i < cacheSize; i++)
	{
		if (sockCache[i].valid && strcmp(sock,sockCache[i].addr) == 0)
		{
			sockCache[i].sock->close();
			delete sockCache[i].sock;
			sockCache[i].valid = false;
		}
	}
}


bool SocketCache::
getReliSock (Sock *&sock, char *addr, int timeOut)
{
	ReliSock	*rSock;
	int			slot;

    for (int i = 0; i < cacheSize; i++)
    {
        if (sockCache[i].valid && strcmp(addr,sockCache[i].addr) == 0)
		{
			sock = sockCache[i].sock;
			sock->timeout(timeOut);
			return true;
		}
	}

	// increment timestamp
	timeStamp++;

	if (!(rSock = new ReliSock))
	{
		return false;
	}

	rSock->timeout(timeOut);
	if (!rSock->connect(addr, 0))
	{
		delete rSock;
		return false;
	}
		
	slot = getCacheSlot();

	sockCache[slot].valid 		= true;
	sockCache[slot].timeStamp 	= timeStamp;
	sockCache[slot].sock 		= rSock;	
	sockCache[slot].sockType	= Stream::reli_sock;
	strcpy(sockCache[slot].addr, addr);

	sock = rSock;
	return true;
}


bool SocketCache::
getSafeSock (Sock *&sock, char *addr, int timeOut)
{
	SafeSock	*sSock;
	int			slot;

    for (int i = 0; i < cacheSize; i++)
    {
        if (sockCache[i].valid && strcmp(addr,sockCache[i].addr) == 0)
		{
			sock = sockCache[i].sock;
			sock->timeout(timeOut);
			return true;
		}
	}

	// increment timestamp
	timeStamp++;

	if (!(sSock = new SafeSock))
	{
		delete sSock;
		return false;
	}

	sSock->timeout(timeOut);
	if (!sSock->connect(addr, 0))
	{
		delete sSock;
		return false;
	}
		
	slot = getCacheSlot();

	sockCache[slot].valid 		= true;
	sockCache[slot].timeStamp 	= timeStamp;
	sockCache[slot].sock 		= sSock;	
	sockCache[slot].sockType	= Stream::safe_sock;
	strcpy(sockCache[slot].addr, addr);

	sock = sSock;
	return true;
}



int SocketCache::
getCacheSlot()
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
	sockCache[oldest].sock->close();
	delete sockCache[oldest].sock;

	return oldest;
}
