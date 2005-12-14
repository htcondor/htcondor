/*********************************************************************
 *
 * Condor ClassAd library
 * Copyright (C) 1990-2003, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI and Rajesh Raman.
 *
 * This source code is covered by the Condor Public License, which can
 * be found in the accompanying LICENSE file, or online at
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
 *********************************************************************/

#ifndef __XACTION_H__
#define __XACTION_H__

#include <list>
#include <string>
#include "sink.h"
#include "view.h"

BEGIN_NAMESPACE( classad )

class ClassAd;
class ClassAdCollection;
	
class XactionRecord {
public:
	XactionRecord( ) { op = -1; key = ""; rec = 0; }
	bool operator==( const XactionRecord &xrec ) const { return false; }
	bool operator< ( const XactionRecord &xrec ) const { return false; }

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


class Sock;

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


END_NAMESPACE

#endif /* __XACTION_H__ */
