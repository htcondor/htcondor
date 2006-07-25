/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#ifndef __SOCK_CACHE_H__
#define __SOCK_CACHE_H__

// due to various insanities with our header files (namely condor_io.h
// and reli_sock.h including each other *sigh*), we can sometimes get
// into trouble if we include one of those here to declare class
// ReliSock.  so, just do the declaration ourselves...
class ReliSock;

class SocketCache
{
public:
		// ctor/dtor
	SocketCache( int size = 16 );
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
