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


#include "condor_common.h"
#include "gmr.h"

GMR::GMR( )
{
    parentAd = NULL;
    key = -1;
}


GMR::~GMR( )
{
    if( parentAd ) {
        delete parentAd;
    }
#if defined(DEBUG)
    printf( "Deleted GMR %d (in dtor)\n", key );
#endif
}


GMR *
GMR::MakeGMR( int key, classad::ClassAd *ad )
{
    const classad::ExprTree          *expr;
    classad::ClassAd                 *portAd;
    const classad::ExprList     		*el;
    classad::ExprListIterator        eli;
    Dependencies::iterator  dit;
    classad::Value                   val;
    GMR::Port               port;
    classad::ClassAdIterator         citr;
    std::string                  attr;
    Ports::iterator         portItr;
    GMR                     *gmr = new GMR( );

    if( !gmr ) return( NULL );
    gmr->parentAd = ad;

        // get the port list
    if( !ad->EvaluateAttr( "Ports", val ) ) {
        printf( "Failed to get 'Ports' attribute\n");
        delete gmr;
        return( NULL );
    }
    if( !val.IsListValue( el ) ) {
        printf( "'Ports' attribute not a list\n" );
        delete gmr;
        return( NULL );
    }

        // insert basic port info 
    for( eli.Initialize( el ) ; !eli.IsAfterLast( ); eli.NextExpr( ) ) {
        if(!(expr=eli.CurrentExpr())||expr->GetKind()!=classad::ExprTree::CLASSAD_NODE){
            printf( "Port is not a classad\n" );
            delete gmr;
            return( NULL );
        }
        portAd = (classad::ClassAd*)expr;

        port.portAd = portAd;
        if( !portAd->EvaluateAttrString( "label", port.label ) ) {
            printf( "Bad or missing port label\n" );
            delete gmr;
            return( NULL );
        }

            // check if this port has already been declared
        for( portItr=gmr->ports.begin(); portItr!=gmr->ports.end(); portItr++ ){
            if( strcasecmp( portItr->label.c_str( ), port.label.c_str( ) )==0 ){
                printf( "Port %s already declared\n", port.label.c_str( ) );
                delete gmr;
                return( NULL );

           }
        }

            // get external references of all expressions in port ad
        citr.Initialize( *portAd );
        for( ; !citr.IsAfterLast( ) ; citr.NextAttribute( attr, expr ) ) {
            citr.CurrentAttribute( attr, expr );
            if( !ad->GetExternalReferences( expr, port.dependencies, false ) ){
                printf( "Failed to get external refs for %s in port %s\n",
                    attr.c_str( ), port.label.c_str( ) );
                delete gmr;
                return( NULL );
            }
        }

            // make sure that the dependencies are not forwards
        for(dit=port.dependencies.begin();dit!=port.dependencies.end();dit++) {
                // dependency on self?
            if( strcasecmp( dit->c_str( ), port.label.c_str( ) ) == 0 ) {
                continue;
            }
                // must be dependent on some previous port
            for(portItr=gmr->ports.begin();portItr!=gmr->ports.end();portItr++){
                if( strcasecmp( dit->c_str( ), portItr->label.c_str( ) )==0 ) {
                    break;
                }
            }
            if( portItr == gmr->ports.end( ) ) {
                printf( "Illegal reference/dependency %s in port %s\n",
                    dit->c_str( ), port.label.c_str( ) );
                delete gmr;
                return( NULL );
            }
        }

            // stuff port into port list
        gmr->ports.push_back( port );
    }

    gmr->key = key;
    //printf( "Created GMR with key %d\n", key );
    return( gmr );
}


bool
GMR::GetExternalReferences( classad::References &refs )
{
    classad::ClassAd         ctx, *pctx;
    classad::PortReferences  prefs;
    classad::References      tmp;
    Ports::iterator itr;
    classad::ExprTree        *tree;

    ctx.Insert( "_ad", parentAd );
    for( itr = ports.begin( ); itr != ports.end( ); itr++ ) {
            // insert dummy context and register as a valid target scope
        pctx = new classad::ClassAd( );
        ctx.Insert( itr->label, pctx );
        prefs[pctx] = tmp;

            // find external references of port
        if( !( tree = itr->portAd->Lookup( "Requirements" ) ) ) continue;
        if( !( itr->portAd->GetExternalReferences( tree, prefs ) ) ) {
            ctx.Remove( "_ad" );
            return( false );
        }
    }

        // accumulate all external references in refs
    classad::PortReferences::iterator    prItr;
    classad::References::iterator        rItr;
    for( prItr = prefs.begin( ); prItr != prefs.end( ); prItr++ ) {
        for( rItr=prItr->second.begin( ); rItr!=prItr->second.end( ); rItr++ ){
            refs.insert( *rItr );
        }
    }

    ctx.Remove( "_ad" );
    return( true );
}

/*
bool
GMR::AddRectangles( classad::Rectangles &r, 
const classad::References &impRefs, 
	const classad::References &expRefs )
{
    Ports::iterator     pitr;
    classad::ExprTree            *tree;
	int					srec, erec;

    r.NewClassAd( );
    for( pitr = ports.begin( ); pitr != ports.end( ); pitr++ ) {
        r.NewPort( );
		srec = r.rId+1;
        if( !( tree = pitr->portAd->Lookup( ATTR_REQUIREMENTS ) ) ||
                !pitr->portAd->AddRectangle( tree, r, pitr->label, impRefs ) ) {
            return( false );
        }
		erec = r.rId;
		r.Typify( expRefs, srec, erec );
    }

    return( true );
}


bool
GMR::AddRectangles( int portNum, Rectangles &r,
const classad::References &impRefs, 
	const References &expRefs )
{
    Port		*port;
    classad::ExprTree    *tree;
	int			srec, erec;

	if( portNum < 0 || (unsigned)portNum >= ports.size( ) ) return( false );
	port = &(ports[portNum]);

    r.NewClassAd( );
    r.NewPort( );
	srec = r.rId+1;
    if( !( tree = port->portAd->Lookup( ATTR_REQUIREMENTS ) ) ||
    		!port->portAd->AddRectangle( tree, r, port->label, impRefs ) ) {
        return( false );
    }
	erec = r.rId;
	r.Typify( expRefs, srec, erec );

    return( true );
}
*/


void
GMR::Display( FILE *fp )
{
    Ports::iterator         itr;
    Dependencies::iterator  dit;

    for( itr = ports.begin( ); itr != ports.end( ); itr++ ) {
        fprintf( fp, "Port %s: Depends on ", itr->label.c_str( ) );
        for(dit=itr->dependencies.begin();dit!=itr->dependencies.end();dit++){
            fprintf( fp, " %s", dit->c_str( ) );
        }
        putc( '\n', fp );
    }
}
