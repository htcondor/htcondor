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
#include "common.h"
#include "operators.h"
#include "rectangle.h"
#include "sink.h"

Rectangles::
Rectangles( )
{
	cId = pId = rId = -1;
}

Rectangles::
~Rectangles( )
{
	Clear( );
}

void Rectangles::
Clear( )
{
    rpMap.clear( );
    pcMap.clear( );
	crMap.clear( );

    tmpExportedBoxes.clear( );
    tmpImportedBoxes.clear( );

    exportedBoxes.clear( );
    importedBoxes.clear( );

	verifyExported.clear( );
	verifyImported.clear( );

	unexported.clear( );
	unimported.clear( );

	cId = pId = rId = -1;
}

int Rectangles::
NewClassAd( int id )
{
	pcMap[pId]=cId;
	crMap[cId]=rId;
	cId = (id<=cId) ? cId+1 : id;
	return( cId );
}

int Rectangles::
NewPort( int id )
{
	rpMap[rId]=pId;
	pId = (id<=pId) ? pId+1 : id;
	return( pId );
}

int Rectangles::
NewRectangle( int id )
{
	return( rId = (id<=rId) ? rId+1 : id );
}


int Rectangles::
AddUpperBound( const string &attr, Value &val, bool open, bool constraint,
	int rkey )
{
	AllDimensions::iterator	aitr;
	OneDimension::iterator	oitr;
	Interval				i;
	Value					result;
	Value::ValueType		vtl, vtu, vt;
	bool					b, assign;
	AllDimensions			*allDim;
	
		// inserting a constraint or attribute bound?
	allDim = constraint ? &tmpExportedBoxes : &tmpImportedBoxes;

	if( ( aitr = allDim->find( attr ) ) != allDim->end( ) ) {
		if( (oitr=aitr->second.find(rkey<0?rId:rkey) ) != aitr->second.end() ){
			i = oitr->second;
		}
	}
	vtl = i.lower.GetType( );
	vtu = i.upper.GetType( );
	vt = val.GetType( );

		// check for type consistency
	if( ( vt==vtl && vt==vtu ) || 								// same types
			( vtl==Value::UNDEFINED_VALUE && vtu==Value::UNDEFINED_VALUE )||
			( vtl==Value::UNDEFINED_VALUE && vtu==vt ) ||		// tighter upper
			( vtu==Value::UNDEFINED_VALUE && vtl==vt ) ) {		// new upper
			// check for value consistency
		Operation::Operate( Operation::LESS_THAN_OP, val, i.lower, result );
		if( result.IsBooleanValue( b ) && b ) {
			return( INCONSISTENT_VALUE );
		}
	} else {
		return( INCONSISTENT_TYPE );
	}

	if( vtu==Value::UNDEFINED_VALUE ) {
		assign = true;
	} else {
		Operation::Operate( Operation::LESS_THAN_OP, val, i.upper, result );
		if( result.IsBooleanValue( b ) && b ) {
			assign = true;
		} else {
			Operation::Operate( Operation::IS_OP, val, i.upper, result );
			if( result.IsBooleanValue( b ) && b && !i.openUpper && open ) {
				assign = true;
			}
		}
	}
			
	if( assign ) {
		i.upper = val;
		i.openUpper = open;
		(*allDim)[attr][rkey<0?rId:rkey] = i;
	}

	return( NO_ERROR );
}


int Rectangles::
AddLowerBound( const string &attr, Value &val, bool open, bool constraint, 
	int rkey )
{
	AllDimensions::iterator	aitr;
	OneDimension::iterator	oitr;
	Interval				i;
	Value					result;
	Value::ValueType		vtl, vtu, vt;
	bool					b, assign;
	AllDimensions			*allDim;
	
		// inserting a constraint or attribute bound?
	allDim = constraint ? &tmpExportedBoxes : &tmpImportedBoxes;

	if( ( aitr = allDim->find( attr ) ) != allDim->end( ) ) {
		if( (oitr=aitr->second.find(rkey<0?rId:rkey))!=aitr->second.end( ) ) {
			i = oitr->second;
		}
	}
	vtl = i.lower.GetType( );
	vtu = i.upper.GetType( );
	vt = val.GetType( );

		// check for type consistency
	if( ( vt==vtl && vt==vtu ) || 								// same types
			( vtl==Value::UNDEFINED_VALUE && vtu==Value::UNDEFINED_VALUE ) || 
			( vtu==Value::UNDEFINED_VALUE && vtl==vt ) ||		// tighter lower
			( vtl==Value::UNDEFINED_VALUE && vtu==vt ) ) {		// new lower
			// check for value consistency
		Operation::Operate( Operation::GREATER_THAN_OP, val, i.upper, result );
		if( result.IsBooleanValue( b ) && b ) {
			return( INCONSISTENT_VALUE );
		}
	} else {
		return( INCONSISTENT_TYPE );
	}

	if( vtl==Value::UNDEFINED_VALUE ) {
		assign = true;
	} else {
		Operation::Operate( Operation::GREATER_THAN_OP, val, i.lower, result );
		if( result.IsBooleanValue( b ) && b ) {
			assign = true;
		} else {
			Operation::Operate( Operation::IS_OP, val, i.lower, result );
			if( result.IsBooleanValue( b ) && b && !i.openLower && open ) {
				assign = true;
			}
		}
	}
			
	if( assign ) {
		i.lower = val;
		i.openLower = open;
		(*allDim)[attr][rkey<0?rId:rkey] = i;
	}

	return( NO_ERROR );
}

void Rectangles::
Typify( const References &exported, int srec, int erec )
{
	AllDimensions::iterator	aitr;
	OneDimension::iterator	oitr;
	Value::ValueType		vtl, vtu;
	char					indexPrefix;
	References::const_iterator	ritr;

		// find unexported dimensions
	for( ritr = exported.begin( ); ritr != exported.end( ); ritr++ ) {
			// attribute to be exported not in tmpExportedBoxes
		if( (aitr=tmpExportedBoxes.find(*ritr)) == tmpExportedBoxes.end( ) ) {
				// insert as unexported for all rectangles
			for( int i = srec ; i <= erec ; i++ ) {
				unexported[*ritr].insert( i );
			}
		} else {
				// attribute is in exported boxes; all rectangles present?
			for( int i = srec ; i <= erec ; i++ ) {
				if( aitr->second.find( i ) == aitr->second.end( ) ) {
						// insert absent rectangle IDs as unexported
					unexported[*ritr].insert( i );
				}
			}
		}
	}

		// typify all the exported boxes
	for(aitr=tmpExportedBoxes.begin();aitr!=tmpExportedBoxes.end();aitr++){
		for( oitr=aitr->second.begin(); oitr!=aitr->second.end(); oitr++ ) {
			vtl = oitr->second.lower.GetType( );
			vtu = oitr->second.upper.GetType( );
			if( vtl != vtu ) {
					// use the type of the non-undefined value 
				indexPrefix=GetIndexPrefix(vtl==Value::UNDEFINED_VALUE?vtu:vtl);
			} else if( vtl == Value::UNDEFINED_VALUE ) {
					// both endpoints are undefined; use UNDEFINED type
				indexPrefix = GetIndexPrefix( Value::UNDEFINED_VALUE );
			} else {
					// same non-UNDEFINED types on end-points
				indexPrefix = GetIndexPrefix( vtl );
			}
			if( !NormalizeInterval( oitr->second, indexPrefix ) ) {
				printf( "Warning:  Could not normalize interval in rectangle "
							"%d of %s\n", oitr->first, aitr->first.c_str( ) );
				AddExportedVerificationRequest( oitr->first );
				continue;
			}
			exportedBoxes[indexPrefix+(':'+aitr->first)][oitr->first] = 
				oitr->second;
		}
	}
	tmpExportedBoxes.clear( );

		// typify all the imported boxes
	for(aitr=tmpImportedBoxes.begin();aitr!=tmpImportedBoxes.end();aitr++){
		for( oitr=aitr->second.begin(); oitr!=aitr->second.end(); oitr++ ) {
			vtl = oitr->second.lower.GetType( );
			vtu = oitr->second.upper.GetType( );
			if( vtl != vtu ) {
					// use the type of the non-undefined value 
				indexPrefix=GetIndexPrefix(vtl==Value::UNDEFINED_VALUE?vtu:vtl);
			} else if( vtl == Value::UNDEFINED_VALUE ) {
					// both endpoints are undefined; use UNDEFINED type
				indexPrefix = GetIndexPrefix( Value::UNDEFINED_VALUE );
			} else {
					// same non-UNDEFINED types on end-points
				indexPrefix = GetIndexPrefix( vtl );
			}
			if( !NormalizeInterval( oitr->second, indexPrefix ) ) {
				printf( "Warning:  Could not normalize interval in rectangle "
							"%d of %s\n", oitr->first, aitr->first.c_str( ) );
				AddImportedVerificationRequest( aitr->first, oitr->first );
				continue;
			}
			importedBoxes[indexPrefix+(':'+aitr->first)][oitr->first] = 
				oitr->second;
		}
	}
	tmpImportedBoxes.clear( );
}


void Rectangles::
AddExportedVerificationRequest( int rkey )
{
	verifyExported.insert( rkey<0?rId:rkey );
}


void Rectangles::
AddImportedVerificationRequest( const string &attr, int rkey )
{
	verifyImported[attr].insert( rkey<0?rId:rkey );
}


void Rectangles::
AddUnexportedDimension( const string &attr, int rkey )
{
	unexported[attr].insert( rkey<0?rId:rkey );
}


bool Rectangles::
NormalizeInterval( Interval &i, char prefix )
{
	double	dl, du;
	long	il, iu;
	string	s1, s2;
	bool	bl, bu;
	
	switch( prefix ) {
		case 'n': 
			if( !i.lower.IsNumber( dl ) ) dl = -(FLT_MAX);
			if( !i.upper.IsNumber( du ) ) du = FLT_MAX;
			i.lower.SetRealValue( dl );
			i.upper.SetRealValue( du );
			return( true );

		case 't':
			dl = i.lower.IsAbsoluteTimeValue(il) ? (double)il : -(FLT_MAX);
			du = i.lower.IsAbsoluteTimeValue(iu) ? (double)iu : FLT_MAX;
			i.lower.SetRealValue( dl );
			i.upper.SetRealValue( du );
			return( true );

		case 'i':
			dl = i.lower.IsRelativeTimeValue(il) ? (double)il : -(FLT_MAX);
			du = i.lower.IsRelativeTimeValue(iu) ? (double)iu : FLT_MAX;
			i.lower.SetRealValue( dl );
			i.upper.SetRealValue( du );
			return( true );

		case 's':
			if( !i.lower.IsStringValue( s1 ) || !i.upper.IsStringValue( s2 ) ||
					strcasecmp( s1.c_str( ), s2.c_str( ) ) != 0 ) {
				return( false );
			}
			return( true );
			
		case 'b':
			if( !i.lower.IsBooleanValue( bl ) ) bl = false;
			if( !i.upper.IsBooleanValue( bu ) ) bu = true;
			if( !i.openLower && !bl ) {
				if( i.openUpper && bu ) {
					i.openUpper = false;
					bu = false;
				} else if( i.openUpper ) {
					return( false );
				}
			} else if( bu && !i.openUpper ) {
				if( bl ^ i.openLower ) {
					bl = true;
					i.openLower = false;
				} else {
					return( false );
				}
			} else {
				return( false );
			}

			i.lower.SetBooleanValue( bl );
			i.upper.SetBooleanValue( bu );
			return( true );
			
		case 'u':
			return( true );

		default:
			return( false );
	}
}


char Rectangles::
GetIndexPrefix( Value::ValueType vt )
{
	switch( vt ) {
		case Value::INTEGER_VALUE:
		case Value::REAL_VALUE:
			return 'n';
		case Value::STRING_VALUE:
			return 's';
		case Value::BOOLEAN_VALUE:
			return 'b';
		case Value::ABSOLUTE_TIME_VALUE:
			return 't';
		case Value::RELATIVE_TIME_VALUE:
			return 'i';
		case Value::UNDEFINED_VALUE:
			return 'u';
		case Value::ERROR_VALUE:
			return 'e';
		case Value::CLASSAD_VALUE:
			return 'c';
		case Value::LIST_VALUE:
			return 'l';
		default:
			return '!';
	}
}


void Rectangles::
Display( FILE *fp )
{
	AllDimensions::iterator		aitr;
	OneDimension::iterator		oitr;
	DimRectangleMap::iterator	vitr;
	set<int>::iterator			sitr;
	string						lower, upper;
	ClassAdUnParser				unp;
	KeyMap::iterator			kitr;

	fprintf( fp, "ExportedBoxes:\n" );
	for( aitr=exportedBoxes.begin( ); aitr!=exportedBoxes.end( ); aitr++ ) {
		fprintf( fp, "\tDimension: %s\n", aitr->first.c_str( ) );
		for( oitr=aitr->second.begin( ); oitr!=aitr->second.end( ); oitr++ ) {
			lower = "";
			upper = "";
			unp.Unparse( lower, oitr->second.lower );
			unp.Unparse( upper, oitr->second.upper );
			fprintf( fp, "\t  %d: %c%s, %s%c\n", oitr->first, 
				oitr->second.openLower ? '(' : '[', lower.c_str( ), 
				upper.c_str( ), oitr->second.openUpper ? ')' : ']' );
		}
		fputc( '\n', fp );
	}
	fprintf( fp, "ImportedBoxes:\n" );
	for( aitr=importedBoxes.begin( ); aitr!=importedBoxes.end( ); aitr++ ) {
		fprintf( fp, "\tDimension: %s\n", aitr->first.c_str( ) );
		for( oitr=aitr->second.begin( ); oitr!=aitr->second.end( ); oitr++ ) {
            lower = "";
            upper = "";
            unp.Unparse( lower, oitr->second.lower );
            unp.Unparse( upper, oitr->second.upper );
            fprintf( fp, "\t  %d: %c%s, %s%c\n", oitr->first, 
                oitr->second.openLower ? '(' : '[', lower.c_str( ), 
                upper.c_str( ), oitr->second.openUpper ? ')' : ']' );
		}
		fputc( '\n', fp );
	}
	fprintf( fp, "TmpExportedBoxes:\n" );
	for(aitr=tmpExportedBoxes.begin();aitr!=tmpExportedBoxes.end();aitr++){
		fprintf( fp, "\tDimension: %s\n", aitr->first.c_str( ) );
		for( oitr=aitr->second.begin( ); oitr!=aitr->second.end( ); oitr++ ) {
            lower = "";
            upper = "";
            unp.Unparse( lower, oitr->second.lower );
            unp.Unparse( upper, oitr->second.upper );
            fprintf( fp, "\t  %d: %c%s, %s%c\n", oitr->first, 
                oitr->second.openLower ? '(' : '[', lower.c_str( ), 
                upper.c_str( ), oitr->second.openUpper ? ')' : ']' );
		}
		fputc( '\n', fp );
	}
	fprintf( fp, "TmpImportedBoxes:\n" );
	for(aitr=tmpImportedBoxes.begin();aitr!=tmpImportedBoxes.end();aitr++) {
		fprintf( fp, "\tDimension: %s\n", aitr->first.c_str( ) );
		for( oitr=aitr->second.begin( ); oitr!=aitr->second.end( ); oitr++ ) {
			lower = "";
            upper = "";
            unp.Unparse( lower, oitr->second.lower );
            unp.Unparse( upper, oitr->second.upper );
            fprintf( fp, "\t  %d: %c%s, %s%c\n", oitr->first, 
                oitr->second.openLower ? '(' : '[', lower.c_str( ), 
                upper.c_str( ), oitr->second.openUpper ? ')' : ']' );
		}
		fputc( '\n', fp );
	}
	fprintf( fp, "VerifyExported:\n" );
	for( sitr=verifyExported.begin( );sitr!=verifyExported.end( );sitr++ ) {
		fprintf( fp, " %d", *sitr );
	}
	fprintf( fp, "\nVerifyImported:\n" );
	for( vitr=verifyImported.begin( );vitr!=verifyImported.end( );vitr++ ) {
		fprintf( fp, "\tDimension: %s\n\t", vitr->first.c_str( ) );
		for( sitr=vitr->second.begin( ); sitr!=vitr->second.end( ); sitr++ ) {
			fprintf( fp, " %d", *sitr );
		}
		fputc( '\n', fp );
	}
	fprintf( fp, "UnExported:\n" );
	for( vitr=unexported.begin( );vitr!=unexported.end( );vitr++ ) {
		fprintf( fp, "\tDimension: %s\n\t", vitr->first.c_str( ) );
		for( sitr=vitr->second.begin( ); sitr!=vitr->second.end( ); sitr++ ) {
			fprintf( fp, " %d", *sitr );
		}
		fputc( '\n', fp );
	}
	fprintf( fp, "UnImported:\n" );
	for( vitr=unimported.begin( );vitr!=unimported.end( );vitr++ ) {
		fprintf( fp, "\tDimension: %s\n\t", vitr->first.c_str( ) );
		for( sitr=vitr->second.begin( ); sitr!=vitr->second.end( ); sitr++ ) {
			fprintf( fp, " %d", *sitr );
		}
		fputc( '\n', fp );
	}
	fprintf( fp, "KeyMaps:  rpMap\n" );
	for( kitr = rpMap.begin( ); kitr != rpMap.end( ); kitr++ ) {
		fprintf( fp, "%d --> %d\n", kitr->first, kitr->second );
	}
	fprintf( fp, "KeyMaps:  pcMap\n" );
	for( kitr = pcMap.begin( ); kitr != pcMap.end( ); kitr++ ) {
		fprintf( fp, "%d --> %d\n", kitr->first, kitr->second );
	}
}
