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

#include "condor_common.h"
#include "condor_attributes.h"
#include "common.h"
#include "collectionServer.h"
#include "query.h"

BEGIN_NAMESPACE( classad )

// --- local collection query implementation begins ---

LocalCollectionQuery::
LocalCollectionQuery( )
{
	itr = keys.end( );
	server = NULL;
}

LocalCollectionQuery::
~LocalCollectionQuery( )
{
}


void LocalCollectionQuery::
Bind( ClassAdCollectionServer *s )
{
	server = s;
}


bool LocalCollectionQuery::
Query( const string &viewName, ExprTree *expr )
{
	ViewRegistry::iterator	vri;
	ViewMembers::iterator	vmi;
	MatchClassAd			mad;
	View					*view;
	ClassAd					*ad; 
	const ClassAd			*parent;
	string					key;
	bool					match;

		// get the view to query
	if( !server || ( vri = server->viewRegistry.find( viewName ) ) == 
			server->viewRegistry.end( ) ) {
		return( false );
	}
	view = vri->second;

	if( expr ) {
			// setup evluation environment if a constraint was supplied
		parent = expr->GetParentScope( );
		if( !( ad=mad.GetLeftAd() ) || !ad->Insert(ATTR_REQUIREMENTS,expr ) ) {
			expr->SetParentScope( parent );
			return( false );
		}
	}
	keys.clear( );


		// iterate over the view members
	for( vmi=view->viewMembers.begin(); vmi!=view->viewMembers.end(); vmi++ ) {
			// ... and insert keys into local list in same order
		vmi->GetKey( key );

		if( expr ) {
				// if a constraint was supplied, make sure its satisfied
			ad = server->GetClassAd( key );
			mad.ReplaceRightAd( ad );
			if( mad.EvaluateAttrBool( "RightMatchesLeft", match ) && match ) {
				keys.push_back( key );
			}
			mad.RemoveRightAd( );
		} else {
			keys.push_back( key );
		}
	}

		// initialize local iterator
	itr = keys.begin( );

		// clean up and return
	if( expr ) {
		expr->SetParentScope( parent );
	}
	return( true );
}


void LocalCollectionQuery::
ToFirst( )
{
	itr = keys.begin( );
}


bool LocalCollectionQuery::
Next( string &key )
{
	itr++;
	if( itr == keys.end( ) ) {
		return( false );
	}
	key = *itr;
	return( true );
}


bool LocalCollectionQuery::
Current( string &key )
{
	if( itr != keys.end( ) ) {
		key = *itr;
		return( true );
	}
	return( false );
}

bool LocalCollectionQuery::
Prev( string &key )
{
	if( itr == keys.begin( ) ) {
		return( false );
	}
	itr--;
	key = *itr;
	return( true );
}


void LocalCollectionQuery::
ToAfterLast( )
{
	itr = keys.end( );
}

// --- local collection query implementation ends ---




// --- remote collection query implementation begins ---
RemoteCollectionQuery::
RemoteCollectionQuery( )
{
	wantResults = true;
	wantPostlude= false;
	accumulate	= false;
	itr = results.end( );
}


RemoteCollectionQuery::
~RemoteCollectionQuery( )
{
	ClearResults( );
}


bool RemoteCollectionQuery::
Connect( const string &serverAddr, int port )
{
	serverSock.close( );
	serverSock.encode( );
	if( !serverSock.connect( (char*)serverAddr.c_str( ), port ) || 
		!serverSock.put((int)ClassAdCollectionInterface::ClassAdCollOp_Connect)
		|| !serverSock.eom() ){
		CondorErrno = ERR_CONNECT_FAILED;
		CondorErrMsg = "failed to connect to collection server";
		return( false );
	}
	return( true );
}

void RemoteCollectionQuery::
Disconnect( )
{
	serverSock.encode( );
	serverSock.put( (int)ClassAdCollectionInterface::ClassAdCollOp_Disconnect );
	serverSock.eom( );
	serverSock.close( );
}


void RemoteCollectionQuery::
SetProjectionAttrs( const vector<string> &attrs )
{
	vector<string>::const_iterator itr;
	projectionAttrs.clear( );
	for( itr = attrs.begin( ); itr != attrs.end( ); itr++ ) {
		projectionAttrs.push_back( *itr );
	}
}


void RemoteCollectionQuery::
GetProjectionAttrs( vector<string> &attrs ) const
{
	vector<string>::const_iterator itr;
	attrs.clear( );
	for( itr = projectionAttrs.begin( ); itr != projectionAttrs.end( ); itr++ ){
		attrs.push_back( *itr );
	}
}


bool RemoteCollectionQuery::
PostQuery( const string &viewName, ExprTree *constraint )
{
	vector<string>::iterator	sitr;
	vector<ExprTree*>			projAttrs;
	ExprTree					*tree;
	Value						val;
	ClassAd						query;

		// first construct the query ...
	ClearResults( );
	query.InsertAttr( "WantResults", wantResults );
	query.InsertAttr( "WantPostlude", wantPostlude );
	query.InsertAttr( "ViewName", viewName );
	if( constraint ) {
		query.Insert( ATTR_REQUIREMENTS, constraint );
	} else {
		query.InsertAttr( ATTR_REQUIREMENTS, true );
	}
	for( sitr=projectionAttrs.begin( ); sitr!=projectionAttrs.end( ); sitr++ ) {
		val.SetStringValue( *sitr );
		if( !(tree = Literal::MakeLiteral( val ) ) ) {
			vector<ExprTree*>::iterator	pitr;
			CondorErrno = ERR_MEM_ALLOC_FAILED;
			CondorErrMsg = "";
			for( pitr=projAttrs.begin( ); pitr!=projAttrs.end( ); pitr++ ) {
				delete *pitr;
			} 
			return( false );
		}
		projAttrs.push_back( tree );
	}
	query.Insert( "ProjectionAttrs", ExprList::MakeExprList( projAttrs ) );

		// send the query to the server
	ClassAdUnParser	unp;
	string			buffer;

	serverSock.encode( );
	unp.Unparse( buffer, &query );
	if(!serverSock.put((int)ClassAdCollectionInterface::ClassAdCollOp_QueryView)
		|| !serverSock.put((char*)buffer.c_str( )) || !serverSock.eom( ) ) {
		CondorErrno = ERR_COMMUNICATION_ERROR;
		CondorErrMsg = "failed to send query to collection server";
		return( false );
	}

		// get ready to receive response
	finished = false;
	itr = results.begin( );
	return( true );
}


void RemoteCollectionQuery::
ClearResults( )
{
	for( itr = results.begin( ); itr != results.end( ); itr++ ) {
		delete itr->second;
	}
	results.clear( );
	postlude.Clear( );
	finished = false;
}


bool RemoteCollectionQuery::
ToFirst( )
{
	if( !accumulate ) return( false );

	itr = results.begin( );
	return( true );
}


bool RemoteCollectionQuery::
IsAtFirst( ) const
{
	if( !accumulate ) return( false );
	return( results.begin( ) == itr );
}


bool RemoteCollectionQuery::
Next( string &key, ClassAd *& ad )
{
		// if we're iterating over already downloaded results ...
	if( itr != results.end( ) ) {
		itr++;
		key = itr->first;
		ad = itr->second;
		return( true );
	}

		// itr==results.end( ); are we already done? 
	if( finished ) {
		return( false );	// done
	}

		// not done; need to download next item
	string	buffer;
	char	*tmp=NULL;
	serverSock.decode( );
	if( !serverSock.get( tmp ) ) {
		if( tmp ) free( tmp );
		CondorErrno = ERR_COMMUNICATION_ERROR;
		CondorErrMsg = "failed to retrieve result from collection server";
		return( false );
	}
	buffer = tmp;
	free( tmp );

		// is server done with sending results?
	if( strcmp( (char*)buffer.c_str( ), "<done>" ) == 0 ) {
		finished = true;
			// get postlude if necessary
		tmp = NULL;
		if( wantPostlude ) {
			if( !serverSock.get( tmp ) || !serverSock.eom( ) ) {
				if( tmp ) free( tmp );
				CondorErrno = ERR_COMMUNICATION_ERROR;
				CondorErrMsg = "failed to receive query postlude from server";
				return( false );
			}
			buffer = tmp;
			free( tmp );
			postlude.Clear( );
			if( !parser.ParseClassAd( buffer, postlude ) ) {
				CondorErrMsg += "; failed to parse query postlude";
				return( false );
			}
		} else if( !serverSock.eom( ) ) {
				// no postlude
			CondorErrno = ERR_COMMUNICATION_ERROR;
			CondorErrMsg = "failed to receive eom() from server";
			return( false );
		}
			// done with results, so return false
		return( false );
	}

		// not done; buffer contains an ad
	ClassAd *tmpAd=NULL;
	if( !( tmpAd = parser.ParseClassAd( buffer ) ) ) {
		CondorErrMsg += "; failed to parse query result";
		return( false );
	}
	tmpAd->EvaluateAttrString( ATTR_KEY, key );
	ad = (ClassAd*)tmpAd->Remove( ATTR_AD );
	delete tmpAd;

		// if accumulating, add <key,ad> to the results list
	if( accumulate ) {
		results.push_back( make_pair( key, ad ) );
	}

		// done
	return( true );
}


bool RemoteCollectionQuery::
Current( string &key, ClassAd *& ad )
{
	if( !accumulate ) return( false );

	if( itr != results.end( ) ) {
		key = itr->first;
		ad = itr->second;
		return( true );;
	}

	return( false );
}


bool RemoteCollectionQuery::
Prev( string &key, ClassAd *& ad )
{
	if( !accumulate ) return( false );

		// already at beginning
	if( itr == results.begin( ) ) {
		return( false );
	}

	itr--;
	key = itr->first;
	ad = itr->second;
	return( true );
}


bool RemoteCollectionQuery::
ToAfterLast( )
{
	if( !accumulate ) return( false );

	itr = results.end( );
	return( true );
}


bool RemoteCollectionQuery::
IsAfterLast( ) const
{
	if( !accumulate ) return( false );
	return( results.end( ) == itr );
}


END_NAMESPACE
