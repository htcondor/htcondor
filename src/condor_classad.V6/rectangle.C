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
#include "operators.h"
#include "rectangle.h"

Rectangles::
Rectangles( )
{
}

Rectangles::
~Rectangles( )
{
	boxes.clear( );
}


bool Rectangles::
AddUpperBound( int key, const string &attr, const double &val, bool open )
{
	AllDimensions::iterator	aitr;
	OneDimension::iterator	oitr;
	NumericInterval			i;

	i.lower = -(FLT_MAX);
	i.upper = FLT_MAX;
	i.openLower = false;
	i.openUpper = false;

	if( ( aitr = boxes.find( attr ) ) != boxes.end( ) ) {
		if( ( oitr = aitr->second.find( key ) ) != aitr->second.end( ) ) {
			i = oitr->second;
		}
	}

	if( val < i.upper || ( val==i.upper && !i.openUpper && open ) ) {
		i.upper = val;
		i.openUpper = open;
	}


	boxes[attr][key] = i;
	return( true );
}


bool Rectangles::
AddLowerBound( int key, const string &attr, const double &val, bool open )
{
	AllDimensions::iterator	aitr;
	OneDimension::iterator	oitr;
	NumericInterval			i;

	i.lower = -(FLT_MAX);
	i.upper = FLT_MAX;

	if( ( aitr = boxes.find( attr ) ) != boxes.end( ) ) {
		if( ( oitr = aitr->second.find( key ) ) != aitr->second.end( ) ) {
			i = oitr->second;
		}
	}

	if( val > i.lower || ( val==i.lower && !i.openLower && open ) ) {
		i.lower = val;
		i.openLower = open;
	}


	boxes[attr][key] = i;
	return( true );
}


void Rectangles::
Display( FILE *fp )
{
	AllDimensions::iterator	aitr;
	OneDimension::iterator	oitr;

	for( aitr=boxes.begin( ); aitr != boxes.end( ); aitr++ ) {
		fprintf( fp, "Dimension: %s\n", aitr->first.c_str( ) );
		for( oitr=aitr->second.begin( ); oitr!=aitr->second.end( ); oitr++ ) {
			fprintf( fp, "  %d: %c%g, %g%c\n", oitr->first, 
				oitr->second.openLower ? '(' : '[', oitr->second.lower,
				oitr->second.upper, oitr->second.openUpper ? ')' : ']' );
		}
		fputc( '\n', fp );
	}
}
