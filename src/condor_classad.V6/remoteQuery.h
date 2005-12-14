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

