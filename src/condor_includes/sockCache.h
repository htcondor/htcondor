#ifndef __SOCK_CACHE_H__
#define __SOCK_CACHE_H__

#include "condor_io.h"

class SocketCache
{
	public:
		// ctor/dtor
		SocketCache(int=16);
		~SocketCache();

		void clearCache(void);
		void invalidateSock(char *);
		bool getReliSock(Sock *&, char *, int=30);
		bool getSafeSock(Sock *&, char *, int=30);		

	private:
		int 	 getCacheSlot();

		struct sockEntry
		{
			bool		valid;
			char 		addr[32];
			Sock		*sock;
			int			timeStamp;

			Stream::stream_type sockType;
		};
		int			timeStamp;
		sockEntry 	*sockCache;
		int			cacheSize;
};
			
		
#endif//__SOCK_CACHE_H__
