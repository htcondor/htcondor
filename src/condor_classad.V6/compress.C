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

#include "condor_common.h"
#include "condor_classad.h"
#include "compress.h"

using namespace std;

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
                bin->ad->Insert( *ritr, ad->Lookup( *ritr )->Copy( ) );
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
