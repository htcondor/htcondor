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
