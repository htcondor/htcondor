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

/*

WARNING: The "addr" for the SocketCache is an opaque identifier.  SocketCache
does not intepret it in any way. (Nor will it, ever.)

SocketCache _does_ use it for log messages, so it should be something
meaningful.  The Sinful string for the "main" address is ideal.  This would
ideally be the first address in the list of sinful strings found in
ATTR_PUBLIC_NETWORK_IP_ADDR.

What you use doesn't matter, _so long as you are consistent_.  If you pass
different addr strings for the same host, you'll get multiple connections.  So
long as you always use the same attribute, you'll be fine.

Why this warning?  When CCB is involved, a single host could have multiple IP
addresses to contact it at.  Which one we use to contact the host may change
over time.  If you use the address you connect with _at the moment_ as your
addr here, you could end up with multiple sockets.

*/
class SocketCache
{
public:
		// ctor/dtor
	SocketCache( int size = DEFAULT_SOCKET_CACHE_SIZE );
	~SocketCache();

	void resize( int size );

	void clearCache( void );
	void invalidateSock( const char* );
	void invalidateSock(const std::string &sockAddr) {invalidateSock(sockAddr.c_str());}

	ReliSock*	findReliSock( const char* addr );
	ReliSock*	findReliSock(const std::string &addr ) {return findReliSock(addr.c_str());}
	void		addReliSock( const char* addr, ReliSock* rsock );
	void		addReliSock( const std::string &addr, ReliSock* rsock ) {addReliSock(addr.c_str(), rsock);}

	bool	isFull( void );
	int		size( void ) const;

private:

	struct sockEntry
	{
		bool		valid;
		MyString	addr;
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
