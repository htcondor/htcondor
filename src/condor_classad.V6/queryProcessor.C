#include "condor_common.h"
#include "condor_classad.h"
#include "queryProcessor.h"
#include "intervalTree.h"

QueryProcessor::QueryProcessor( )
{
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

	verifyExported.clear( );
	verifyImported.clear( );
	unexported.clear( );
}


bool
QueryProcessor::InitializeIndexes( Rectangles& r )
{
	AllDimensions::iterator		aitr;
	ClassAdIndex				*index;
	

	ClearIndexes( );

	rectangles = &r;
	for( aitr=r.exportedBoxes.begin(); aitr!=r.exportedBoxes.end(); aitr++ ) {
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


	for( aitr=r.importedBoxes.begin(); aitr!=r.importedBoxes.end(); aitr++ ) {
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

	verifyExported = r.verifyExported;
	verifyImported = r.verifyImported;
	unexported = r.unexported;

	return( true );
}


void QueryProcessor::
PurgeIndexEntries( int rId )
{
	AllDimensions::iterator		aitr;
	OneDimension::iterator		oitr;
	DimRectangleMap::iterator	ditr;
	Indexes::iterator			iitr;

	for( aitr=rectangles->importedBoxes.begin( ); 
			aitr!=rectangles->importedBoxes.end( ); aitr++ ) {
			// find the interval of the rectangle and the relevant index
		if( (iitr=imported.find( aitr->first )) == imported.end( ) ||
			(oitr=aitr->second.find( rId )) == aitr->second.end( ) ) {
			continue;
		}
			// delete from the index
		iitr->second->Delete( rId, oitr->second );
	}


	for( aitr=rectangles->exportedBoxes.begin( ); 
			aitr!=rectangles->exportedBoxes.end( ); aitr++ ) {
			// find the interval of the rectangle and the relevant index
		if( (iitr=exported.find( aitr->first )) == exported.end( ) ||
			(oitr=aitr->second.find( rId )) == aitr->second.end( ) ) {
			continue;
		}
			// delete from the index
		iitr->second->Delete( rId, oitr->second );
	}

	for( ditr=verifyImported.begin( ); ditr!=verifyImported.end( ); ditr++ ) {
		ditr->second.erase( rId );
	}
	verifyExported.erase( rId );
	for( ditr=unexported.begin( ); ditr!=unexported.end( ); ditr++ ) {
		ditr->second.erase( rId );
	}
}


void QueryProcessor::
Display( FILE* fp )
{
	Indexes::iterator			itr;
	DimRectangleMap::iterator	vitr;
	set<int>::iterator			sitr;

	fprintf( fp, "Exported: " );
	for( itr = exported.begin( ); itr != exported.end( ); itr++ ) {
		fprintf( fp, "%s ", itr->first.c_str( ) );
	}
	fprintf( fp, "\nImported:  " );
	for( itr = imported.begin( ); itr != imported.end( ); itr++ ) {
		fprintf( fp, "%s ", itr->first.c_str( ) );
	}
	fprintf( fp, "\nVerifyImported:  " );
	for(vitr = verifyImported.begin( ); vitr != verifyImported.end( ); vitr++){
		fprintf( fp, "%s ", vitr->first.c_str( ) );
	}
	fprintf( fp, "\nVerifyExported:  " );
	for(sitr = verifyExported.begin( ); sitr != verifyExported.end( ); sitr++){
		fprintf( fp, "%d ", *sitr );
	}
	fprintf( fp, "\nUnexported:  " );
	for(vitr = unexported.begin( ); vitr != unexported.end( ); vitr++ ) {
		fprintf( fp, "%s ", vitr->first.c_str( ) );
	}
	fprintf( fp, "\n" );
}


void QueryProcessor::
DumpQState( QueryOutcome &qo, QueryOutcome &qv )
{
	QueryOutcome::iterator	qitr;
	int						size;

	printf( "\tResult:\n" );
	for( qitr=qo.begin( ); qitr!=qo.end( ); qitr++ ) {
		printf( "\t  %d: " , qitr->first );
		size = qitr->second.getlast()+1;
		for( int i = 0 ; i < size ; i++ ) {
			for( int j = 0 ; j < SUINT ; j++ ) {
				if( qitr->second[i]&(unsigned)(1<<j) ) {
					printf( "%d ", (i*SUINT)+j );
				}
			}
		}
		printf( "\n" );
	}
	printf( "\tVerify:\n" );
	for( qitr=qv.begin( ); qitr!=qv.end( ); qitr++ ) {
		printf( "\t  %d: " , qitr->first );
		size = qitr->second.getlast()+1;
		for( int i = 0 ; i < size ; i++ ) {
			for( int j = 0 ; j < SUINT ; j++ ) {
				if( qitr->second[i]&(unsigned)(1<<j) ) {
					printf( "%d ", (i*SUINT)+j );
				}
			}
		}
		printf( "\n" );
	}

}

bool QueryProcessor::
DoQuery( Rectangles &window, KeySet &result, KeySet &verify )
{
	
	AllDimensions::iterator		aitr;
	OneDimension::iterator		oitr;
	Indexes::iterator			iitr;
	ClassAdIndex				*index;
	KeySet						tmpResult;
	KeySet						tmpVerify;
	KeySet						tmpUnconstrained;
	QueryOutcome				qo, qv;
	QueryOutcome::iterator		qitr;
	string						demangled;
	int							wrkey, maxIndex;
	int							n, o;
	DimRectangleMap::iterator	vitr;
	set<int>::iterator			sitr;

		// Phase 0: Initialize 
	maxIndex = (rectangles->rId+1)/SUINT;
	for( int i = 0; i <= window.rId ; i++ ) {
		qo[i].fill( (unsigned int) -1 );// identity for logical AND
		qv[i].fill( 0 );				// identity for logical OR
	}
		// Phase 1: Handle verifyImported attrs in the query window
		//  All object rectangles which place a constraint on a q-vim'd 
		//  attribute must be verified
	//printf( "\nQvim on exported: " );
	for( iitr = exported.begin( ); iitr != exported.end( ); iitr++ ) {
		demangled.assign( iitr->first, 2, iitr->first.size( ) - 2 );
		vitr = window.verifyImported.find(demangled);
		if( vitr != window.verifyImported.end() ){
			//printf( "%s ", demangled.c_str( ) );
				// get all rectangles which place a constraint on the imported
				// attribute
			tmpResult.fill( 0 );
			if( !iitr->second->FilterAll( tmpResult ) ) {
				printf("Error:  Failed when filtering unimported attributes\n");
				return( false );
			}

				// for every query rectangle dependent on that attribute
			for( sitr=vitr->second.begin(); sitr!=vitr->second.end(); sitr++ ){
				for( int i = 0 ; i <= tmpResult.getlast( ) ; i++ ) {
					qo[*sitr][i] &= ~(tmpResult[i]); // blank out from outcome
					qv[*sitr][i] |= tmpResult[i];    // insert into verify
				}
			}
				
		}
	}
	//printf( "\n\n1a. After Qvim on exported:\n" );
	//DumpQState( qo, qv );

		// Phase 1b: Add verifications for o-vex'd rectangles
	for(sitr=verifyExported.begin();sitr!=verifyExported.end(); sitr++){
		n = (*sitr) / SUINT;
		o = (*sitr) % SUINT;
		qo[*sitr][n] &= ~(1<<o);	// blank out from result
		qv[*sitr][n] |= (1<<o);		// add verification
	}
	//printf( ")\n\n1b. After adding verifications for Ovex:\n" );
	//DumpQState( qo, qv );
		

		// Phase 2: Handle verifyExported constraints in the query window
		//   All object rectangles must be verified!
	for( sitr=window.verifyExported.begin();sitr!=window.verifyExported.end(); 
			sitr++ ) {
		qo[*sitr].fill( 0 );	// blank out from outcome
		qv[*sitr].fill( (unsigned) -1 );
	}
	/*
	printf( ")" );

	printf( "\n\n2a. After Qvex on Imported:\n" );
	DumpQState( qo, qv );
	

		// Phase 2b: Also check q-vex on o-vim
	//printf( "\t(Qvex on Ovim: " );
	for( vitr=window.verifyExported.begin(); vitr!=window.verifyExported.end(); 
			vitr++ ){
		if( (ovitr=verifyImported.find(vitr->first)) != verifyImported.end() ){
			// printf( "%s ", vitr->first.c_str( ) );
				// for each q-rectangle requiring vexs
			for( sitr=vitr->second.begin();sitr!=vitr->second.end();sitr++) {
					// go through o-rectangles requiring vims
				for(sitr2=ovitr->second.begin();sitr2!=ovitr->second.end();
						sitr2++){
					n = (*sitr2) / SUINT;
					o = (*sitr2) % SUINT;
					qo[*sitr][n] &= ~(1<<o);	// blank out from result
					qv[*sitr][n] |= (1<<o);		// add verification
				}
			}
		}
	}
	printf( ")\n\n2b. After Qvex on Ovim:\n" );
	DumpQState( qo, qv );
	*/


		// Phase 3: go through dimensions of query rectangle; imported first
	//printf( "\n\n3. Processing query imported:" );
	for( aitr=window.importedBoxes.begin( ); aitr!=window.importedBoxes.end( ); 
			aitr++ ) {
			// get unverified outcome for this dimension; demangle name first
		demangled.assign( aitr->first, 2, aitr->first.size( ) - 2 );
		/*
		printf( "\n\tDimension (%s), Demangled (%s)\n\tVerify Exported: ", 
				aitr->first.c_str( ), demangled.c_str( ) );
		
		tmpVerify.fill( 0 );
		if( ( vitr=verifyExported.find(demangled) ) != verifyExported.end() ) {
			set<int>::iterator	sitr;

			for( sitr=vitr->second.begin(); sitr!=vitr->second.end(); sitr++) {
				//printf( "%d ", *sitr );
				n = *sitr / SUINT;
				o = *sitr % SUINT;
				tmpVerify[n] |= (1<<o);
			}
		}
		*/

			// identify rectangles which do not constrain this dimension
			// So, tmpUnconstrained contains rectangles which pass this query
			// "vacuously"
		//printf( "\n\tUnconstrained: " );
		tmpUnconstrained.fill( 0 );
		if( ( vitr=unexported.find(demangled) ) != unexported.end( ) ) {
			set<int>::iterator	sitr;

			for( sitr=vitr->second.begin(); sitr!=vitr->second.end(); sitr++) {
				//printf( "%d ", *sitr );
				n = *sitr / SUINT;
				o = *sitr % SUINT;
				tmpUnconstrained[n] |= (1<<o);
			}
		}

			// find appropriate index
		if( ( iitr = exported.find( aitr->first ) ) == exported.end( ) ) {
				// no index --- must all be in verify slots
			//printf( "\n\tNo index (using unconstrained and tmpVerify)\n" );
			index = NULL;
		} else {
			index = iitr->second;
		}

			// for every query interval (corresponds to a rectangle) in this 
			// dimension ...
		for( oitr=aitr->second.begin( ); oitr!=aitr->second.end( ); oitr++ ) {
			wrkey = oitr->first;
				// get normal outcome (initialize from unconstrained result)
			tmpResult.fill( 0 );
			for( int i = 0 ; i <= tmpUnconstrained.getlast( ) ; i++ ) {
				tmpResult[i] = tmpUnconstrained[i];
			}
			if( index ) {
				if( !index->Filter( oitr->second, tmpResult ) ) {
					printf( "Error when filtering with index %s\n",
						aitr->first.c_str( ) );
					return( false );
				}
			}

			unsigned int tmp;
			for( int i = 0 ; i <= maxIndex; i++ ) {
					// temporarily hold query result 
				tmp=qo[wrkey][i] & tmpResult[i] & ~qv[wrkey][i] & ~tmpVerify[i];
					// patch verify result 
				qv[wrkey][i] = (qv[wrkey][i]&(tmpResult[i]|tmpVerify[i])) |
								(qo[wrkey][i]&tmpVerify[i]);
					// patch query result
				qo[wrkey][i] = tmp;
			}
		}
		/*
		printf( "\n\tAfter index filtering\n" );
		DumpQState( qo, qv );
		*/
	}

		// Phase 4: now do the same for the exported boxes
	//printf( "\n\n4. Processing query exported:" );
	for( aitr=window.exportedBoxes.begin( ); aitr!=window.exportedBoxes.end( ); 
			aitr++ ) {
			// get unverified outcome for this dimension; demangle name first
		demangled.assign( aitr->first, 2, aitr->first.size( ) - 2 );
		/*
		printf( "\n\tDimension (%s), Demangled (%s)\n\tVerify Imported: ", 
			aitr->first.c_str( ), demangled.c_str( ) );
		*/

		tmpVerify.fill( 0 );
		if( ( vitr=verifyImported.find(demangled) ) != verifyImported.end() ) {
			set<int>::iterator	sitr;

			for( sitr=vitr->second.begin(); sitr!=vitr->second.end(); sitr++) {
				//printf( "%d ", *sitr );
				n = *sitr / SUINT;
				o = *sitr % SUINT;
				tmpVerify[n] |= (1<<o);
			}
		}
			// find appropriate index
		if( ( iitr = imported.find( aitr->first ) ) == imported.end( ) ) {
			//printf( "\n\tNo index (using IDENTITY and tmpVerify)" );
			index = NULL;
		} else {
			index = iitr->second;
		}

			// for every interval in this dimension ...
		for( oitr=aitr->second.begin( ); oitr!=aitr->second.end( ); oitr++ ) {
			wrkey = oitr->first;
				// get normal outcome
			if( index ) {
				tmpResult.fill( 0 );
				if( !index->Filter( oitr->second, tmpResult ) ) {
					printf( "Error when filtering with index %s\n",
						aitr->first.c_str( ) );
					return( false );
				}
			} else {
				tmpResult.fill( (unsigned)-1 );
			}
			//printf( "\n\tFilter %d: ", oitr->first );
			for( int i = 0 ; i < tmpResult.getlast()+1; i++ ) {
				for( int j = 0 ; j < SUINT ; j++ ) {
					if( tmpResult[i]&(unsigned)(1<<j) ) {
						//printf( "%d ", (i*SUINT)+j );
					}
				}
			}


				// patch in normal outcome
			unsigned int tmp;
			for( int i = 0 ; i <= maxIndex; i++ ) {
					// temporarily hold query results
				tmp=qo[wrkey][i] & tmpResult[i] & ~qv[wrkey][i] & ~tmpVerify[i];
					// patch unverified results
				qv[wrkey][i] = (qv[wrkey][i]&(tmpResult[i]|tmpVerify[i])) |
								(qo[wrkey][i]&tmpVerify[i]);
					// patch query results
				qo[wrkey][i] = tmp;
			}
		}
		/*
		printf( "\n\tAfter index filtering\n" );
		DumpQState( qo, qv );
		*/
	}


		// Phase 5:  Process disjuncts
		//   Collapse results of all the q-rectangles into one result and
		//   verify set
	result.fill( 0 );
	for( qitr = qo.begin( ); qitr != qo.end( ); qitr++ ) {
		for( int i = 0 ; i <= qitr->second.getlast( ) ; i++ ) {
			result[i] |= qitr->second[i];
		}
	}
	verify.fill( 0 );
	for( qitr = qv.begin( ); qitr != qv.end( ); qitr++ ) {
		for( int i = 0 ; i <= qitr->second.getlast( ) ; i++ ) {
			verify[i] |= qitr->second[i];
		}
	}
	/*
	printf( "\n5. Collapsing into single result\n\tResult: " );
	for( int i = 0 ; i < result.getlast()+1; i++ ) {
		for( int j = 0 ; j < SUINT ; j++ ) {
			if( result[i]&(unsigned)(1<<j) ) {
				printf( "%d ", (i*SUINT)+j );
			}
		}
	}
	printf( "\n\tVerify: " );
	for( int i = 0 ; i < verify.getlast()+1; i++ ) {
		for( int j = 0 ; j < SUINT ; j++ ) {
			if( verify[i]&(unsigned)(1<<j) ) {
				printf( "%d ", (i*SUINT)+j );
			}
		}
	}
	printf( "\n" );
	*/

		// Phase 6:  Blank out rectangles which constrain unimported attributes
	for( iitr = exported.begin( ); iitr != exported.end( ); iitr++ ) {
		demangled.assign( iitr->first, 2, iitr->first.size( ) - 2 );
		if( window.unimported.find( demangled ) != window.unimported.end( ) ) {
			tmpResult.fill( 0 );
			if( !iitr->second->FilterAll( tmpResult ) ) {
				printf("Error:  Failed when filtering unimported attributes\n");
				return( false );
			}
			for( int i = 0 ; i <= tmpResult.getlast( ) ; i++ ) {
				result[i] &= ~(tmpResult[i]);
				verify[i] &= ~(tmpResult[i]);
			}
		}
	}
	/*
	printf( "\n\n6. Removing rectangles constraining unimported attributes\n\t"
		"Result: " );
	for( int i = 0 ; i < result.getlast()+1; i++ ) {
		for( int j = 0 ; j < SUINT ; j++ ) {
			if( result[i]&(unsigned)(1<<j) ) {
				printf( "%d ", (i*SUINT)+j );
			}
		}
	}
	printf( "\n\tVerify: " );
	for( int i = 0 ; i < verify.getlast()+1; i++ ) {
		for( int j = 0 ; j < SUINT ; j++ ) {
			if( verify[i]&(unsigned)(1<<j) ) {
				printf( "%d ", (i*SUINT)+j );
			}
		}
	}
	*/

		// Phase 7:  Remove verifications for rectangles independently verified
	for( int i = 0 ; i <= maxIndex ; i++ ) {
		verify[i] &= ~result[i];
	}
	/*
	printf("\n\n7. Removing verifications for verified rectangles\n\tVerify: ");
	for( int i = 0 ; i < verify.getlast()+1; i++ ) {
		for( int j = 0 ; j < SUINT ; j++ ) {
			if( verify[i]&(unsigned)(1<<j) ) {
				printf( "%d ", (i*SUINT)+j );
			}
		}
	}
	printf( "\n" );
	*/		
	return( true );
}

bool QueryProcessor::
MapRectangleID( int rId, int &portNum, int &pId, int &cId )
{
	if( !rectangles ) return( false );

	KeyMap::iterator	kitr;

	if( (kitr=rectangles->rpMap.lower_bound(rId))==rectangles->rpMap.end( ) ) {
		return( false );
	}
	pId = kitr->second;

	if( (kitr=rectangles->pcMap.lower_bound(pId))==rectangles->pcMap.end( ) ) {
		return( false );
	}
	cId = kitr->second;
	portNum = kitr->first - pId;

	return( true );
}


bool QueryProcessor::
UnMapClassAdID( int cId, int &brId, int &erId )
{
	if( !rectangles ) return( false );

	KeyMap::iterator	kitr;

	if( (kitr=rectangles->crMap.find( cId ) ) == rectangles->crMap.end( ) ) {
		return( false );
	}
	erId = kitr->second;

	if( (kitr=rectangles->crMap.find( cId-1 ) ) == rectangles->crMap.end( ) ) {
		return( false );
	}
	brId = kitr->second+1;

	return( true );
}


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
	int						n, o;

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
			n = (*nitr) / SUINT;
			o = (*nitr) % SUINT;
			result[n] = result[n] | (1 << o );
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
	int						n, o;
	if( qf ) {
		for( itr = nope.begin( ); itr != nope.end( ); itr++ ) {
			n = (*itr) / SUINT;
			o = (*itr) % SUINT;
			result[n] = result[n] | (1<<o);
		}
	}
	if( qt ) {
		for( itr = yup.begin( ); itr != yup.end( ); itr++ ) {
			n = (*itr) / SUINT;
			o = (*itr) % SUINT;
			result[n] = result[n] | (1<<o);
		}
	}	

	return( true );
}

bool
ClassAdBooleanIndex::FilterAll( KeySet &result )
{
	IndexEntries::iterator	itr;
	int						n, o;

	for( itr = nope.begin( ); itr != nope.end( ) ; itr++ ) {
		n = (*itr) / SUINT;
		o = (*itr) % SUINT;
		result[n] = result[n] | (1<<o);
	}
	for( itr = yup.begin( ); itr != yup.end( ); itr++ ) {
		n = (*itr) / SUINT;
		o = (*itr) % SUINT;
		result[n] = result[n] | (1<<o);
	}
	return( true );
}
