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
#include "condor_classad.h"
#include "queryProcessor.h"
#include "intervalTree.h"
#include "../gm-oo/gangster-indexed.h"

using namespace std;

BEGIN_NAMESPACE(classad)

int QueryProcessor::numQueries = 0;
//extern KeySet givenAway;

QueryProcessor::QueryProcessor( )
{
	rectangles = NULL;
	summarize  = true;
}


QueryProcessor::~QueryProcessor( )
{
	ClearIndexes( );
}


void
QueryProcessor::ClearIndexes( )
{
	Indexes::iterator			itr;

	for( itr = imported.begin( ); itr != imported.end( ); itr++ ) {
		delete itr->second;
	}
	imported.clear( );

	for( itr = exported.begin( ); itr != exported.end( ); itr++ ) {
		delete itr->second;
	}
	exported.clear( );
}


bool
QueryProcessor::InitializeIndexes( Rectangles& r, bool useSummaries )
{
	AllDimensions::iterator		aitr;
	ClassAdIndex				*index;
	Rectangles					*rec = useSummaries ? &summaries : &r;
	
	ClearIndexes( );

	summarize = useSummaries;
	rectangles = &r;

		// summarize the rectangles
	if( useSummaries ) {
		if( !summaries.Summarize( r, reps, consts, constRepMap ) ) {
			printf( "Failed to summarize\n" );
			return( false );
		}
		summaries.Complete( true );
	}
	r.Complete( true );

	for(aitr=rec->exportedBoxes.begin();aitr!=rec->exportedBoxes.end();aitr++) {
		if( (aitr->first)[1] != ':' ) return( false );
		switch( (aitr->first)[0] ) {
			case 'n':
			case 'i':
			case 't':
				index = ClassAdNumericIndex::MakeNumericIndex( aitr->second );
				break;
					
			case 'b':
			case 'u':
				index = ClassAdBooleanIndex::MakeBooleanIndex( aitr->second );
				break;

			case 's':
				index = ClassAdStringIndex::MakeStringIndex( aitr->second );
				break;

			default:
				printf( "Error:  Unknown type prefix %c\n", (aitr->first)[0] );
				return( false );
		}
		if( !index ) {
			ClearIndexes( );
			return( false );
		}
		exported[aitr->first] = index;
	}


	for(aitr=rec->importedBoxes.begin();aitr!=rec->importedBoxes.end();aitr++) {
		if( (aitr->first)[1] != ':' ) return( false );
		switch( (aitr->first)[0] ) {
			case 'n':
			case 'i':
			case 't':
				index = ClassAdNumericIndex::MakeNumericIndex( aitr->second );
				break;
					
			case 'b':
			case 'u':
				index = ClassAdBooleanIndex::MakeBooleanIndex( aitr->second );
				break;

			case 's':
				index = ClassAdStringIndex::MakeStringIndex( aitr->second );
				break;

			default:
				printf( "Error:  Unknown type prefix %c\n", (aitr->first)[0] );
				return( false );
		}
		if( !index ) {
			ClearIndexes( );
			return( false );
		}
		imported[aitr->first] = index;
	}

	return( true );
}


void QueryProcessor::
PurgeRectangle( int rId )
{
	AllDimensions::iterator		aitr;
	OneDimension::iterator		oitr;
	Indexes::iterator			iitr;
	KeyMap::iterator			kitr;
	Constituents::iterator		citr;
	int							repId;
	Rectangles					*rec = summarize ? &summaries : rectangles;

	if( summarize ) {
			// first find representative
		if( ( kitr = constRepMap.find( rId ) ) == constRepMap.end( ) ) {
				// enh??
			printf( "Error: No entry for constituent!\n" );
			exit( 1 );
			return;	
		}
		repId = kitr->second;
			// remove constituent from representative
		if( ( citr = consts.find( repId ) ) == consts.end( ) ) {
			printf( "Error: No representative for constituent!\n" );
			exit( 1 );
			return;
		}
		
		citr->second.erase( rId );
			// and from the rectangle object
		rectangles->PurgeRectangle( rId );	
			// any constituents left?
		if( !citr->second.empty( ) ) {
				// done
			return;
		}
	} else {
		repId = rId;
	}
	
	
	//givenAway.Insert( repId );

		// first, the imported stuff; indexed imported ...
	for(aitr=rec->importedBoxes.begin();aitr!=rec->importedBoxes.end();aitr++){
			// find the interval of the rectangle and the relevant index
		if( (iitr=imported.find( aitr->first )) == imported.end( ) ||
			(oitr=aitr->second.find( repId )) == aitr->second.end( ) ) {
			continue;
		}
			// delete from the index
		if( !iitr->second->Delete( repId, oitr->second ) ) {
			printf( "Error:  Failed delete from imported index %s\n",
				iitr->first.c_str( ) );
		}
	}

		// next, the exported stuff; indexed exported
	for(aitr=rec->exportedBoxes.begin();aitr!=rec->exportedBoxes.end();aitr++){
			// find the interval of the rectangle and the relevant index
		if( (iitr=exported.find( aitr->first )) == exported.end( ) ||
			(oitr=aitr->second.find( repId )) == aitr->second.end( ) ) {
			continue;
		}
			// delete from the index
		if( !iitr->second->Delete( repId, oitr->second ) ) {
			printf( "Error:  Failed delete from exported index %s\n",
				iitr->first.c_str( ) );
		}
	}

		// kill rep 
	rec->PurgeRectangle( repId );
}


void QueryProcessor::
Display( FILE* fp )
{
	Indexes::iterator			itr;

	fprintf( fp, "Exported: " );
	for( itr = exported.begin( ); itr != exported.end( ); itr++ ) {
		fprintf( fp, "%s ", itr->first.c_str( ) );
	}
	fprintf( fp, "\nImported:  " );
	for( itr = imported.begin( ); itr != imported.end( ); itr++ ) {
		fprintf( fp, "%s ", itr->first.c_str( ) );
	}
}


void QueryProcessor::
DumpQState( QueryOutcome &qo )
{
	QueryOutcome::iterator	qitr;
	KeySet::iterator		kitr;
	int						elt;

	printf( "\tResult:\n" );
	for( qitr=qo.begin( ); qitr!=qo.end( ); qitr++ ) {
		printf( "\t  %d: " , qitr->first );
		kitr.Initialize(qitr->second);
		while( kitr.Next(elt) ) {
			printf( "%d ", elt );
		}
		printf( "\n" );
	}
}


bool QueryProcessor::
DoQuery( Rectangles &window, KeySet &result )
{
	AllDimensions::iterator		aitr;
	OneDimension::iterator		oitr;
	Indexes::iterator			iitr;
	KeySet						deviantExportedSet;
	KeySet						tmpResult;
	QueryOutcome				qo;
	string						demangled;
	DimRectangleMap::iterator	ditr, daxitr;
	KeySet::iterator			ksitr;
	int							tmp;
	Rectangles					*rec = summarize ? &summaries : rectangles;

	numQueries++;

	//printf( "Doing query\n" );
		// Phase 0: Initialize 
	for( int i = 0; i <= window.rId ; i++ ) {
		qo[i].MakeUniversal( );
	}

		// Phase 1:  Process imported intervals
	for( aitr=window.importedBoxes.begin( ); aitr!=window.importedBoxes.end( );
			aitr++ ) {

		demangled.assign( aitr->first, 2, aitr->first.size( ) - 2 );
		ditr = rec->unexported.find( demangled );

		for( oitr=aitr->second.begin( ); oitr!=aitr->second.end( ); oitr++ ) {
			tmpResult.Clear( );
				// include all rectangles whose constraints are satisfied
			if( ( iitr = exported.find( aitr->first ) ) != exported.end( ) ) {
				if( !iitr->second->Filter( oitr->second, tmpResult ) ) {
					printf( "Failed exported index lookup\n" );
					return( false );
				}
			}

				// include deviant exported rectangles and all rectangles
				// which do not constrain this attribute (if any)
			if( ditr == rec->unexported.end( ) ) {
				qo[oitr->first].IntersectWithUnionOf( tmpResult, 
					rec->deviantExported );
			} else {
				qo[oitr->first].IntersectWithUnionOf( tmpResult, ditr->second,
					rec->deviantExported );
			}
		}
	}

		// Phase 2:  Remove candidates which constrain absent attributes
	for(ditr=window.unimported.begin();ditr!=window.unimported.end();ditr++) {
			// find all rectangles which constrain this particular attribute
		daxitr = rec->allExported.find( ditr->first );
		if( daxitr == rec->allExported.end( ) ) continue;
			// remove this set from the solution for each query rectangle
		ksitr.Initialize( ditr->second );
		while( ksitr.Next( tmp ) ) {
			qo[tmp].Subtract( daxitr->second );
		}
	}

		// Phase 3:  Process exported dimensions
	for( aitr=window.exportedBoxes.begin( ); aitr!=window.exportedBoxes.end( );
			aitr++ ) {
			// need to include rectangles which are deviant on this attribute
		demangled.assign( aitr->first, 2, aitr->first.size( ) - 2 );
		ditr = rec->deviantImported.find( demangled );

		for( oitr=aitr->second.begin( ); oitr!=aitr->second.end( ); oitr++ ) {
			tmpResult.Clear( );
				// include all rectangles which satisfy this constraint
			if( ( iitr = imported.find( aitr->first ) ) != imported.end( ) ) {
				if( !iitr->second->Filter( oitr->second, tmpResult ) ) {
					printf( "Failed imported index lookup\n" );
					return( false );
				}
			}
				// check if there are relevant deviant imported rectangles
			if( ditr == rec->deviantImported.end( ) ) {
					// ... if not, just intersect with filtered results
				qo[oitr->first].Intersect( tmpResult );
			} else {
					// ... otherwise, include deviant imported rectangles
				qo[oitr->first].IntersectWithUnionOf(ditr->second,tmpResult);
			}
		}
	}

		// Phase 4:  Aggregate results for all query rectangles
	result.Clear( );
	for( int i = 0 ; i <= window.rId ; i++ ) {
		result.Unify( qo[i] );
	}

		// done
	return( true );
}

bool QueryProcessor::
MapRectangleID( int srId, int &rId, int &portNum, int &pId, int &cId )
{
	if( summarize ) {
		Constituents::iterator  citr;
		set<int>::iterator      sitr;

		if( ( citr = consts.find( srId ) ) == consts.end( ) ) {
			return( false );
		}
		if( citr->second.empty( ) ) {
			return( false );
		}
		
			// When we map to a classad, we check if the classad is already in
			// in the bundle.  If it is, we try for another representative in 
			// the same constituency.  If we run out, return false ; the caller 
			// will try the next valid representative
		for( sitr=citr->second.begin( ); sitr!=citr->second.end( ); sitr++ ) {
			rId = *sitr;
			if( rectangles->MapRectangleID( rId, portNum, pId, cId ) && 
				Gangster::keysInBundle.find(cId)==Gangster::keysInBundle.end()){
				return( true );
			}
		}
	} else {
		rId = srId;
		return(rectangles->MapRectangleID( rId, portNum, pId, cId ) &&
				Gangster::keysInBundle.find(cId)==Gangster::keysInBundle.end());
	}

    return( false );
}


bool QueryProcessor::
UnMapClassAdID( int cId, int &brId, int &erId )
{
	return(rectangles ? rectangles->UnMapClassAdID( cId, brId, erId ) : false);
}

//----------------------------------------------------------------------------

Query::
Query( )
{
	op = IDENT;
	rectangles = NULL;
	left = right = NULL;
}

Query::
~Query( )
{
	if( rectangles ) delete rectangles;
	if( left ) delete left;
	if( right ) delete right;
}

Query *Query::
MakeQuery( Rectangles *r )
{
	Query *q = new Query( );
	q->rectangles = r;
	return( q );
}

Query *Query::
MakeConjunctQuery( Query *q1, Query *q2 )
{
	Query *q = new Query( );
	q->op = AND;
	q->left = q1;
	q->right = q2;
	return( q );
}

Query *Query::
MakeDisjunctQuery( Query *q1, Query *q2 )
{
	Query *q = new Query( );
	q->op = OR;
	q->left = q1;
	q->right = q2;
	return( q );
}

bool Query::
RunQuery( QueryProcessor &qp, KeySet &result )
{
	switch( op ) {
		case IDENT: {
			return( qp.DoQuery( *rectangles, result ) );
		}

		case AND: {
			KeySet	tmp;
			if( !left->RunQuery( qp, result ) || !right->RunQuery( qp, tmp ) ) {
				return( false );
			}
			result.Intersect( tmp );
			return( true );
		}

		case OR: 
			return( left->RunQuery(qp,result) && right->RunQuery(qp,result) );

		default:
			return( false );
	}
}

//----------------------------------------------------------------------------

// index implementations
ClassAdIndex::ClassAdIndex( )
{
}

ClassAdIndex::~ClassAdIndex( )
{
}


//-----------------------------------------------------------------------------
// numeric index; uses interval trees

ClassAdNumericIndex::ClassAdNumericIndex( )
{
	intervalTree = NULL;
}


ClassAdNumericIndex::~ClassAdNumericIndex( )
{
	if( intervalTree ) delete intervalTree;
}

ClassAdNumericIndex *
ClassAdNumericIndex::MakeNumericIndex( OneDimension &dim )
{
	ClassAdNumericIndex	*index = new ClassAdNumericIndex( );
	if( !index ) return( NULL );
	index->intervalTree = IntervalTree::MakeIntervalTree( dim );
	return( index );
}

bool
ClassAdNumericIndex::Delete( int rkey, const Interval &interval )
{
	return( intervalTree && intervalTree->DeleteInterval( rkey, interval ) );
}

bool
ClassAdNumericIndex::Filter( const Interval &interval, KeySet &result ) 
{
	return( intervalTree && intervalTree->WindowQuery( interval, result ) );
}

bool
ClassAdNumericIndex::FilterAll( KeySet &result )
{
	Interval	interval;
	interval.lower.SetRealValue( -(FLT_MAX) );
	interval.upper.SetRealValue( FLT_MAX );
	return( intervalTree && intervalTree->WindowQuery( interval, result ) );
}

//-----------------------------------------------------------------------------
// string index; uses STL map

ClassAdStringIndex::ClassAdStringIndex( )
{
}

ClassAdStringIndex::~ClassAdStringIndex( )
{
}

ClassAdStringIndex *
ClassAdStringIndex::MakeStringIndex( OneDimension &dim )
{
	ClassAdStringIndex		*si = new ClassAdStringIndex( );
	OneDimension::iterator	itr;
	StringIndex::iterator	sitr;
	string					str;

	if( !si ) return( NULL );
	for( itr = dim.begin( ); itr != dim.end( ); itr++ ) {
		itr->second.lower.IsStringValue( str );
		if( ( sitr = si->stringIndex.find( str ) ) == si->stringIndex.end( ) ) {
			IndexEntries	ie;
			ie.insert( itr->first );
			si->stringIndex[str] = ie;
		} else {
			sitr->second.insert( itr->first );
		}
	}
	return( si );
}


bool
ClassAdStringIndex::Delete( int rkey, const Interval &interval )
{
	string					str;
	StringIndex::iterator	sitr;

	if( !interval.lower.IsStringValue( str ) ) return( false );
	if( ( sitr = stringIndex.find( str ) ) == stringIndex.end( ) ) {
		return( false );
	}
	sitr->second.erase( rkey );
	return( true );
}


bool
ClassAdStringIndex::Filter( const Interval &interval, KeySet &result )
{
	string					str;
	StringIndex::iterator	bitr, eitr;
	IndexEntries::iterator	nitr;

		// figure out iterator position for lower end of interval
	if( !interval.lower.IsStringValue( str ) ) {
		bitr = stringIndex.begin( );
	} else if( interval.openLower ) {
		bitr = stringIndex.upper_bound( str );
	} else {
		bitr = stringIndex.lower_bound( str );
	}

		// figure out iterator position for upper end of interval
	if( !interval.upper.IsStringValue( str ) ) {
		eitr = stringIndex.end( );
	} else {
		if( ( eitr = stringIndex.find( str ) ) != stringIndex.end( ) ) {
			if( !interval.openUpper ) {
				eitr++;
			}
		} else {
			eitr = stringIndex.upper_bound( str );
		}
	}

		// sweep from lower position to upper position
	for(; bitr != eitr; bitr++ ) {
			// get all keys stored in the these entries
		for( nitr=bitr->second.begin( ); nitr!=bitr->second.end( ); nitr++ ) {
			result.Insert( *nitr );
		}
	}

	return( true );
}

bool
ClassAdStringIndex::FilterAll( KeySet &result )
{
	Interval	interval;
	interval.lower.SetUndefinedValue( );
	interval.upper.SetUndefinedValue( );
	return( Filter( interval, result ) );
}


//-----------------------------------------------------------------------------
// boolean index

ClassAdBooleanIndex::ClassAdBooleanIndex( )
{
}

ClassAdBooleanIndex::~ClassAdBooleanIndex( )
{
}

ClassAdBooleanIndex*
ClassAdBooleanIndex::MakeBooleanIndex( OneDimension &dim )
{
	ClassAdBooleanIndex	*bi = new ClassAdBooleanIndex( );
	OneDimension::iterator	itr;
	bool					b1, b2;

	if( !bi ) return( NULL );
	for( itr = dim.begin( ); itr != dim.end( ); itr++ ) {
		if( itr->second.lower.IsBooleanValue( b1 ) &&
				itr->second.upper.IsBooleanValue( b2 ) ) {
			if(  b1 ||  b2 ) bi->yup.insert( itr->first ); 
			if( !b1 || !b2 ) bi->nope.insert( itr->first );
		}
	}

	return( bi );
}


bool
ClassAdBooleanIndex::Delete( int rkey, const Interval& )
{
	yup.erase( rkey );
	nope.erase( rkey );
	return( true );
}


bool
ClassAdBooleanIndex::Filter( const Interval &interval, KeySet &result )
{
	bool	ol, l, ou, u;
	bool	qf, qt;

		// figure out which index "entries" to query
	ol = interval.openLower;
	interval.lower.IsBooleanValue( l );
	interval.upper.IsBooleanValue( u );
	ou = interval.openUpper;
	if( !ol && !l ) {
		if( !u && !ou ) {
			qf = true;
		} else if( u && !ou ) {
			qf = qt = true;
		} else if( u && ou ) {
			qf = true;
		} 
	} else if( u && !ou ) {
		if( ol ^ l ) {
			qt = true;
		} 
	}
			
	if( !qt && !qf ) return( true );

		// do the queries
	IndexEntries::iterator	itr;
	if( qf ) {
		for( itr = nope.begin( ); itr != nope.end( ); itr++ ) {
			result.Insert( *itr );
		}
	}
	if( qt ) {
		for( itr = yup.begin( ); itr != yup.end( ); itr++ ) {
			result.Insert( *itr );
		}
	}	

	return( true );
}

bool
ClassAdBooleanIndex::FilterAll( KeySet &result )
{
	IndexEntries::iterator	itr;

	for( itr = nope.begin( ); itr != nope.end( ) ; itr++ ) {
		result.Insert( *itr );
	}
	for( itr = yup.begin( ); itr != yup.end( ); itr++ ) {
		result.Insert( *itr );
	}
	return( true );
}


END_NAMESPACE
