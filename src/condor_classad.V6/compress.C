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
#include "condor_classad.h"
#include "compress.h"

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
            rest.push_back( ad->Copy( ) );
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
