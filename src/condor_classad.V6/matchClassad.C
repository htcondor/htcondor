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
			"leftMatchesRight = adcd.ad.requirements ;"
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
			"[other=.adcr.ad;my=ad;target=other;super=other;ad=[]]" ) ) ) {
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
			"[other=.adcl.ad;my=ad;target=other;super=other;ad=[]]" ) ) ) {
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
