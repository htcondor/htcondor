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

#ifndef __SOCK_CACHE_H__
#define __SOCK_CACHE_H__

// due to various insanities with our header files (namely condor_io.h
// and reli_sock.h including each other *sigh*), we can sometimes get
// into trouble if we include one of those here to declare class
// ReliSock.  so, just do the declaration ourselves...
class ReliSock;

#define DEFAULT_SOCKET_CACHE_SIZE 16

class SocketCache
{
public:
		// ctor/dtor
	SocketCache( int size = DEFAULT_SOCKET_CACHE_SIZE );
	~SocketCache();

	void resize( int size );

	void clearCache( void );
	void invalidateSock( const char* );

	ReliSock*	findReliSock( const char* addr );
	void		addReliSock( const char* addr, ReliSock* rsock );

	bool	isFull( void );
	int		size( void );

private:

	struct sockEntry
	{
		bool		valid;
		char 		addr[32];
		ReliSock	*sock;
		int			timeStamp;
	};

	int getCacheSlot( void );
	void invalidateEntry( int i );
	void initEntry( sockEntry* entry );

	int			timeStamp;
	sockEntry 	*sockCache;
	int			cacheSize;
};
			
		
#endif//__SOCK_CACHE_H__
