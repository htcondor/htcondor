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

#include "common.h"
#include "collectionBase.h"
#include "query.h"

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
