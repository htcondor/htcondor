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
*****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#if !defined( QUERY_H )
#define QUERY_H

#include <vector>
#include <list>
#include <string>
#include "condor_io.h"

BEGIN_NAMESPACE( classad );

class ClassAdCollectionServer;
class ExprTree;
class ClassAd;

class LocalCollectionQuery {
public:
	LocalCollectionQuery( );
	~LocalCollectionQuery( );

	void Bind( ClassAdCollectionServer * );
	bool Query( const string &viewName, ExprTree *constraint=NULL );
	void Clear( );

	void ToFirst( );
	bool IsAtFirst( ) const { return( itr==keys.begin( ) ); }
	bool Current( string &key );
	bool Next( string &key );
	bool Prev( string &key );
	void ToAfterLast( );
	bool IsAfterLast( ) const { return( itr==keys.end( ) ); }

private:
	ClassAdCollectionServer		*server;
	vector<string>				keys;
	vector<string>::iterator	itr;
};


class RemoteCollectionQuery {
public:
	RemoteCollectionQuery( );
	~RemoteCollectionQuery( );

	bool Connect( const string &serverAddr, int port );
	void Disconnect( );

	void SetWantResults( bool b ) { wantResults = b; }
	bool GetWantResults( ) const { return( wantResults ); }
	void SetWantPostlude( bool b ) { wantPostlude = b; }
	bool GetWantPostlude( ) const { return( wantPostlude ); }
	void SetProjectionAttrs( const vector<string> & );
	void GetProjectionAttrs( vector<string> & ) const;
	void SetAccumulate( bool b ) { accumulate = b; }
	bool GetAccumulate( ) const { return( accumulate ); }

	bool PostQuery( const string &viewName, ExprTree *constraint=NULL );
	void ClearResults( );

	bool ToFirst( );
	bool IsAtFirst( ) const;
	bool Next( string &key, ClassAd *&ad );
	bool Current( string &key, ClassAd *&ad );
	bool Prev( string &key, ClassAd *&ad );
	bool ToAfterLast( );
	bool IsAfterLast( ) const;

	ClassAd *GetPostlude( ) { return( &postlude ); }

private:
	ReliSock					serverSock;
	bool						wantResults,wantPostlude,accumulate,finished;
	vector<string>				projectionAttrs;
	list<pair<string,ClassAd*> >results;
	list<pair<string,ClassAd*> >::iterator itr;
	ClassAd						postlude;
	ClassAdParser				parser;
};

END_NAMESPACE // classad

#endif

