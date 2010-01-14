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


#if !defined( QUERY_H )
#define QUERY_H

#include <vector>
#include <list>
#include <string>
#include "condor_io.h"

BEGIN_NAMESPACE( classad )

class ClassAdCollectionServer;
class ExprTree;
class ClassAd;

class RemoteCollectionQuery {
public:
	RemoteCollectionQuery( );
	~RemoteCollectionQuery( );

	bool Connect( const std::string &serverAddr, int port );
	void Disconnect( );

	void SetWantResults( bool b ) { wantResults = b; }
	bool GetWantResults( ) const { return( wantResults ); }
	void SetWantPostlude( bool b ) { wantPostlude = b; }
	bool GetWantPostlude( ) const { return( wantPostlude ); }
	void SetProjectionAttrs( const std::vector<std::string> & );
	void GetProjectionAttrs( std::vector<std::string> & ) const;
	void SetAccumulate( bool b ) { accumulate = b; }
	bool GetAccumulate( ) const { return( accumulate ); }

	bool PostQuery( const std::string &viewName, ExprTree *constraint=NULL );
	void ClearResults( );

	bool ToFirst( );
	bool IsAtFirst( ) const;
	bool Next( std::string &key, ClassAd *&ad );
	bool Current( std::string &key, ClassAd *&ad );
	bool Prev( std::string &key, ClassAd *&ad );
	bool ToAfterLast( );
	bool IsAfterLast( ) const;

	ClassAd *GetPostlude( ) { return( &postlude ); }

private:
	ReliSock					serverSock;
	bool						wantResults,wantPostlude,accumulate,finished;
	std::vector<std::string>				projectionAttrs;
	std::list<std::pair<std::string,ClassAd*> >results;
	std::list<std::pair<std::string,ClassAd*> >::iterator itr;
	ClassAd						postlude;
	ClassAdParser				parser;
};

END_NAMESPACE // classad

#endif

