/*********************************************************************
 *
 * Condor ClassAd library
 * Copyright (C) 1990-2001, CONDOR Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI, and Rajesh Raman.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU Lesser General
 * Public License as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 * USA
 *
 *********************************************************************/

#ifndef __XACTION_H__
#define __XACTION_H__

#if (__GNUC__<3)
#include <hash_map>
#else
#include <ext/hash_map>
#endif

#include <list>
#include <string>
#include "sink.h"
#include "view.h"

BEGIN_NAMESPACE( classad );

class ClassAd;
class ClassAdCollection;
	
class XactionRecord {
public:
	XactionRecord( ) { op = -1; key = ""; rec = 0; }
	bool operator==( const XactionRecord &xrec ) { return false; }
	bool operator< ( const XactionRecord &xrec ) { return false; }

	int 	op;
	string	key;
	ClassAd	*rec;
	ClassAd	*backup;
};

typedef list<XactionRecord> CollectionOpList;

class ServerTransaction {
public:
	ServerTransaction( );
	~ServerTransaction( );

	inline void SetCollectionServer( ClassAdCollection *c ) { server=c; }
	inline void GetCollectionServer( ClassAdCollection *&c ){ c=server; } 
	inline void SetXactionName( const string &n ) { xactionName = n; }
	inline void GetXactionName( string &n ) const { n = xactionName; }
	inline void SetLocalXaction( bool l ) { local = l; }
	inline bool GetLocalXaction( ) const { return( local ); }

	void ClearRecords( );
	void AppendRecord( int, const string &, ClassAd * );
	bool Commit( );
	bool Log( FILE *fp, ClassAdUnParser *unp );
	ClassAd *ExtractErrorCause( );

	enum { ABSENT, ACTIVE, COMMITTED };

private:
	string					xactionName;
	bool					local;
	ClassAdCollection    	*server;
	CollectionOpList		opList;

	int						xactionErrno;
	string					xactionErrMsg;
	ClassAd					*xactionErrCause;
};


class Sock;

class ClientTransaction {
public:
	ClientTransaction( );
	~ClientTransaction( );

	inline void SetServerAddr( const string &a, int p ) { addr=a; port=p; }
	inline void GetServerAddr( string &a, int &p ) const { a=addr; p=port; }
	inline void SetXactionName( const string &n ) { xactionName = n; }
	inline void GetXactionName( string &n ) const { n = xactionName; }
	inline void SetXactionState( char s ) { state = s; }
	inline char GetXactionState( ) const { return( state ); }

	bool LogCommit( FILE *, ClassAdUnParser *unp );
	bool LogAckCommit( FILE *, ClassAdUnParser *unp );
	bool LogAbort( FILE *, ClassAdUnParser *unp );

	enum { ACTIVE, PENDING };

private:
	string	xactionName, addr;
	int		port;
	char	state;
};


END_NAMESPACE

#endif /* __XACTION_H__ */
