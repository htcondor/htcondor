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

#include "classad.h"
#include "compress.h"

using namespace std;

namespace classad {

ClassAdBin::ClassAdBin( )
{
    count = 0;
    ad = new ClassAd( );
}

ClassAdBin::~ClassAdBin( )
{
    delete ad;
}

bool
MakeSignature( const ClassAd *ad, const References &refs, string &sig )
{
    References::iterator    itr;
    ExprTree                *expr;
    Value                   val;
    ClassAdUnParser         unp;

    sig = "";
    for( itr=refs.begin( ); itr!= refs.end( ); itr++ ) {
        if((expr=ad->Lookup(*itr))&&expr->GetKind()==ExprTree::LITERAL_NODE){
            unp.Unparse( sig, expr );
        } else {
            return( false );
        }
        sig += ":";
    }

    return( true );
}


bool
Compress( ClassAdCollectionServer *server, LocalCollectionQuery *query,
    const References &refs, CompressedAds &comp, list<ClassAd*> &rest )
{
    string                      key, sig;
    References::const_iterator  ritr;
    ClassAd                     *ad;
    CompressedAds::iterator     citr;
    ClassAdBin                  *bin;

    query->ToFirst( );
    query->Current( key );
    while( !query->IsAfterLast( ) ) {
            // get ad
        if( !( ad = server->GetClassAd( key ) ) ) {
            return( false );
        }

            // get signature of current ad
        if( !MakeSignature( ad, refs, sig ) ) {
                // can't make signature --- can't compress
            rest.push_back( (ClassAd*) ad->Copy( ) );
        }

            // get bin
        if( ( citr = comp.find( sig ) ) == comp.end( ) ) {
                // no bin ... make one
            bin = new ClassAdBin;
            bin->count = 1;

                // make a projected classad 
            for( ritr=refs.begin( ); ritr!=refs.end( ); ritr++ ) {
                bin->ad->Insert( *ritr, ad->Lookup( *ritr )->Copy( ), false );
            }

				// insert bin into container
			comp[sig] = bin;
        } else {
                // increment membership in bin
            bin = citr->second;
            bin->count++;
        }

            // process next ad
        query->Next( key );
    }

    return( true );
}

}
