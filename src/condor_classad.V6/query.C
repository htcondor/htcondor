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

#include "common.h"
#include "collectionBase.h"
#include "query.h"

using namespace std;

BEGIN_NAMESPACE( classad )

// --- local collection query implementation begins ---

LocalCollectionQuery::
LocalCollectionQuery( )
{
	itr = keys.end( );
	collection = NULL;
	return;
}

LocalCollectionQuery::
~LocalCollectionQuery( )
{
	return;
}


void LocalCollectionQuery::
Bind( ClassAdCollection *c )
{
	collection = c;
	return;
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

    parent = NULL;

	// get the view to query
	if( !collection || ( vri = collection->viewRegistry.find( viewName ) ) == 
			collection->viewRegistry.end( ) ) {
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
			ad = collection->GetClassAd( key );
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
		ad = mad.GetLeftAd();
		if (ad != NULL) {
			ad->Remove(ATTR_REQUIREMENTS);
		}
	}
	return( true );
}


void LocalCollectionQuery::
ToFirst(void)
{
	itr = keys.begin( );
	return;
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
ToAfterLast(void)
{
	itr = keys.end( );
	return;
}

// --- local collection query implementation ends ---

END_NAMESPACE
