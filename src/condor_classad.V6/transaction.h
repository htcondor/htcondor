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

#if !defined( XACTION_H )
#define XACTION_H

#include <hash_map>
#include <list>
#include <string>
#include "sink.h"
#include "view.h"

BEGIN_NAMESPACE( classad );

class ClassAd;
class ClassAdCollectionServer;
	
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

	inline void SetCollectionServer( ClassAdCollectionServer *c ) { server=c; }
	inline void GetCollectionServer( ClassAdCollectionServer *&c ){ c=server; } 
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
	ClassAdCollectionServer	*server;
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

#endif XACTION_H
