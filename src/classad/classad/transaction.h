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


#ifndef __CLASSAD_TRANSACTION_H__
#define __CLASSAD_TRANSACTION_H__

#include <list>
#include <string>
#include "classad/sink.h"
#include "classad/view.h"

class Sock;

namespace classad {

class ClassAd;
class ClassAdCollection;
	
class XactionRecord {
public:
	XactionRecord( ) { op = -1; key = ""; rec = 0; }
	bool operator==( const XactionRecord & ) const { return false; }
	bool operator< ( const XactionRecord & ) const { return false; }

	int 	op;
	std::string	key;
	ClassAd	*rec;
	ClassAd	*backup;
};

typedef std::list<XactionRecord> CollectionOpList;

class ServerTransaction {
public:
	ServerTransaction( );
	~ServerTransaction( );

	inline void SetCollectionServer( ClassAdCollection *c ) { server=c; }
	inline void GetCollectionServer( ClassAdCollection *&c ){ c=server; } 
	inline void SetXactionName( const std::string &n ) { xactionName = n; }
	inline void GetXactionName(std::string &n ) const { n = xactionName; }
	inline void SetLocalXaction( bool l ) { local = l; }
	inline bool GetLocalXaction( ) const { return( local ); }

	void ClearRecords( );
	void AppendRecord( int, const std::string &, ClassAd * );
	bool Commit( );
	bool Log( FILE *fp, ClassAdUnParser *unp );
	ClassAd *ExtractErrorCause( );

	enum { ABSENT, ACTIVE, COMMITTED };

private:
	std::string				xactionName;
	bool					local;
	ClassAdCollection    	*server;
	CollectionOpList		opList;

	int						xactionErrno;
	std::string				xactionErrMsg;
	ClassAd					*xactionErrCause;
};


class ClientTransaction {
public:
	ClientTransaction( );
	~ClientTransaction( );

	inline void SetServerAddr( const std::string &a, int p ) { addr=a; port=p; }
	inline void GetServerAddr( std::string &a, int &p ) const { a=addr; p=port; }
	inline void SetXactionName( const std::string &n ) { xactionName = n; }
	inline void GetXactionName( std::string &n ) const { n = xactionName; }
	inline void SetXactionState( char s ) { state = s; }
	inline char GetXactionState( ) const { return( state ); }

	bool LogCommit( FILE *, ClassAdUnParser *unp );
	bool LogAckCommit( FILE *, ClassAdUnParser *unp );
	bool LogAbort( FILE *, ClassAdUnParser *unp );

	enum { ACTIVE, PENDING };

private:
	std::string	xactionName, addr;
	int		port;
	char	state;
};


}

#endif /* __CLASSAD_TRANSACTION_H__ */
