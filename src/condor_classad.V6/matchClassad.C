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
#include "source.h"
#include "matchClassad.h"

BEGIN_NAMESPACE( classad )

MatchClassAd::
MatchClassAd()
{
	lCtx = rCtx = NULL;
	lad = rad = NULL;
	ladParent = radParent = NULL;
	InitMatchClassAd( NULL, NULL );
}


MatchClassAd::
MatchClassAd( ClassAd *adl, ClassAd *adr ) : ClassAd()
{
	lad = rad = lCtx = rCtx = NULL;
	ladParent = radParent = NULL;
	InitMatchClassAd( adl, adr );
}


MatchClassAd::
~MatchClassAd()
{
}


MatchClassAd* MatchClassAd::
MakeMatchClassAd( ClassAd *adl, ClassAd *adr )
{
	return( new MatchClassAd( adl, adr ) );
}

bool MatchClassAd::
InitMatchClassAd( ClassAd *adl, ClassAd *adr )
{
	ClassAdParser parser;

		// clear out old info
	Clear( );
	lad = rad = NULL;
	lCtx = rCtx = NULL;

		// convenience expressions
	ClassAd *upd;
	if( !( upd = parser.ParseClassAd( 
			"[symmetricMatch = leftMatchesRight && rightMatchesLeft ;"
			"leftMatchesRight = adcr.ad.requirements ;"
			"rightMatchesLeft = adcl.ad.requirements ;"
			"leftRankValue = adcl.ad.rank ;"
			"rightRankValue = adcr.ad.rank]" ) ) ) {
		Clear( );
		lCtx = NULL;
		rCtx = NULL;
		return( false );
	}
	Update( *upd );
	delete upd;

		// stash parent scopes
	ladParent = adl ? adl->GetParentScope( ) : (ClassAd*)NULL;
	radParent = adr ? adr->GetParentScope( ) : (ClassAd*)NULL;

		// the left context
	if( !( lCtx = parser.ParseClassAd( 
			"[other=.adcr.ad;my=ad;target=other;ad=[]]" ) ) ) {
		Clear( );
		lCtx = NULL;
		rCtx = NULL;
		return( false );
	}
	if( adl ) {
		lCtx->Insert( "ad", adl );
	} else {
		Value val;
		lCtx->EvaluateAttr( "ad", val );
		val.IsClassAdValue( adl );
	}

		// the right context
	if( !( rCtx = parser.ParseClassAd( 
			"[other=.adcl.ad;my=ad;target=other;ad=[]]" ) ) ) {
		delete lCtx;
		lCtx = rCtx = NULL;
		return( false );
	}
	if( adr ) {
		rCtx->Insert( "ad", adr );
	} else {
		Value val;
		rCtx->EvaluateAttr( "ad", val );
		val.IsClassAdValue( adr );
	}

		// insert the left and right contexts
	Insert( "adcl", lCtx );
	Insert( "adcr", rCtx );

	lad = adl;
	rad = adr;

	return( true );
}


bool MatchClassAd::
ReplaceLeftAd( ClassAd *ad )
{
	lad = ad;
	ladParent = lad ? lad->GetParentScope( ) : (ClassAd*)NULL;
	return( ad ? lCtx->Insert( "ad", ad ) : true );
}


bool MatchClassAd::
ReplaceRightAd( ClassAd *ad )
{
	rad = ad;
	radParent = rad ? rad->GetParentScope( ) : (ClassAd*)NULL;
	return( ad ? rCtx->Insert( "ad", ad ) : true );
}


ClassAd *MatchClassAd::
GetLeftAd()
{
	return( lad );
}


ClassAd *MatchClassAd::
GetRightAd()
{
	return( rad );
}


ClassAd *MatchClassAd::
GetLeftContext( )
{
	return( lCtx );
}


ClassAd *MatchClassAd::
GetRightContext( )
{
	return( rCtx );
}


ClassAd *MatchClassAd::
RemoveLeftAd( )
{
	ClassAd *ad = lad;
	lCtx->Remove( "ad" );
	if( lad ) {
		lad->SetParentScope( ladParent );
	}
	ladParent = NULL;
	lad = NULL;
	return( ad );
}


ClassAd *MatchClassAd::
RemoveRightAd( )
{
	ClassAd	*ad = rad;
	rCtx->Remove( "ad" );
	if( rad ) {
		rad->SetParentScope( radParent );
	}
	radParent = NULL;
	rad = NULL;
	return( ad );
}

END_NAMESPACE // classad
