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


#include "classad/common.h"
#include "classad/source.h"
#include "classad/matchClassad.h"

using namespace std;

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
			"[other=adcr.ad;my=ad;target=other;ad=[]]" ) ) ) {
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
			"[other=adcl.ad;my=ad;target=other;ad=[]]" ) ) ) {
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
