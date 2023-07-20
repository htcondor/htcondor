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


#include "classad/common.h"
#include "classad/collectionBase.h"
#include "classad/query.h"

using std::string;
using std::vector;
using std::pair;


namespace classad {

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
Query( const string &viewName, ExprTree *expr, bool two_way_matching )
{
	ViewRegistry::iterator	vri;
	ViewMembers::iterator	vmi;
	MatchClassAd			mad;
	View					*view;
	ClassAd					*ad=0; 
	const ClassAd			*parent;
	string					key;
	bool					match;
    bool                    given_classad=false;
    
	if (expr && expr->isClassad(&ad))
	{
		given_classad = true;
	}

    parent = NULL;

	// get the view to query
	if( !collection || ( vri = collection->viewRegistry.find( viewName ) ) == 
			collection->viewRegistry.end( ) ) {
		return( false );
	}
	view = vri->second;

	if( expr ) {
        if (given_classad) {
            mad.ReplaceLeftAd(ad);
        } else {
            // setup evluation environment if a constraint was supplied
            parent = expr->GetParentScope( );
            if( !( ad=mad.GetLeftAd() ) || !ad->Insert(ATTR_REQUIREMENTS,expr) ) {
                expr->SetParentScope( parent );
                return( false );
            }
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
                bool add_key;
                if (!given_classad || !two_way_matching) {
                    add_key = true;
                } else if (mad.EvaluateAttrBool( "LeftMatchesRight", match ) && match) {
                    add_key = true;
                } else {
                    add_key = false;
                }
                if (add_key) {   
                    keys.push_back( key );
                }
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
        if (given_classad) {
            mad.RemoveLeftAd();
        }
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

}
