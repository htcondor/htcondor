#include "condor_common.h"
#include "condor_debug.h"
#include "sockCache.h"

static char _FileName_[] = __FILE__;

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
}


void SocketCache::
clearCache()
{
	for (int i = 0; i < cacheSize; i++)
	{
		if (sockCache[i].valid);
		{
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


ReliSock *SocketCache::
getReliSock (char *sock, int timeOut)
{
	ReliSock	*rSock;
	int			slot;

	// increment timestamp
	timeStamp++;

	if (!(rSock = new ReliSock))
	{
		delete rSock;
		return NULL;
	}

	rSock->timeout(timeOut);
	if (!rSock->connect(sock, 0))
	{
		delete rSock;
		return NULL;
	}
		
	slot = getCacheSlot();

	sockCache[slot].valid 		= true;
	sockCache[slot].timeStamp 	= timeStamp;
	sockCache[slot].sock 		= rSock;	
	sockCache[slot].sockType	= Stream::reli_sock;
	strcpy(sockCache[slot].addr, sock);

	return rSock;
}


SafeSock *SocketCache::
getSafeSock (char *sock, int timeOut)
{
	SafeSock	*sSock;
	int			slot;

	// increment timestamp
	timeStamp++;

	if (!(sSock = new SafeSock))
	{
		delete sSock;
		return NULL;
	}

	sSock->timeout(timeOut);
	if (!sSock->connect(sock, 0))
	{
		delete sSock;
		return NULL;
	}
		
	slot = getCacheSlot();

	sockCache[slot].valid 		= true;
	sockCache[slot].timeStamp 	= timeStamp;
	sockCache[slot].sock 		= sSock;	
	sockCache[slot].sockType	= Stream::safe_sock;
	strcpy(sockCache[slot].addr, sock);

	return sSock;
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

	return i;
}
