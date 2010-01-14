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

static char const *ATTR_UNOPTIMIZED_REQUIREMENTS = "UnoptimizedRequirements";

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
			"[symmetricMatch = RIGHT.requirements && LEFT.requirements ;"
			"leftMatchesRight = RIGHT.requirements ;"
			"rightMatchesLeft = LEFT.requirements ;"
			"leftRankValue = LEFT.rank ;"
			"rightRankValue = RIGHT.rank]" ) ) ) {
		Clear( );
		lCtx = NULL;
		rCtx = NULL;
		return( false );
	}
	Update( *upd );
	delete upd;

		// In the following, global-scope references to LEFT and RIGHT
		// are used to make things slightly more efficient.  That means
		// that this match ad should not be nested inside other ads.
		// Also for effiency, the left and right ads are actually
		// inserted as .LEFT and .RIGHT (ie at the top level) but
		// their parent scope is set to the lCtx and rCtx ads as
		// though they were inserted as lCtx.ad and rCtx.ad.  This way,
		// the most direct way to reference the ads is through the
		// top-level global names .LEFT and .RIGHT.

		// the left context
	if( !( lCtx = parser.ParseClassAd( 
			"[other=.RIGHT;target=.RIGHT;my=.LEFT;ad=.LEFT]" ) ) ) {
		Clear( );
		lCtx = NULL;
		rCtx = NULL;
		return( false );
	}

		// the right context
	if( !( rCtx = parser.ParseClassAd( 
			"[other=.LEFT;target=.LEFT;my=.RIGHT;ad=.RIGHT]" ) ) ) {
		delete lCtx;
		lCtx = rCtx = NULL;
		return( false );
	}

		// insert the left and right contexts
	Insert( "lCtx", lCtx );
	Insert( "rCtx", rCtx );

	if( !adl ) {
		adl = new ClassAd();
	}
	if( !adr ) {
		adr = new ClassAd();
	}
	ReplaceLeftAd( adl );
	ReplaceRightAd( adr );

	return( true );
}


bool MatchClassAd::
ReplaceLeftAd( ClassAd *ad )
{
	lad = ad;
	ladParent = lad ? lad->GetParentScope( ) : (ClassAd*)NULL;
	if( ad ) {
		if( !Insert( "LEFT", ad ) ) {
			return false;
		}
			// For the ability to efficiently reference the ad via
			// .LEFT, it is inserted in the top match ad, but we want
			// its parent scope to be the context ad.
		lad->SetParentScope( lCtx );
	}
	return true;
}


bool MatchClassAd::
ReplaceRightAd( ClassAd *ad )
{
	rad = ad;
	radParent = rad ? rad->GetParentScope( ) : (ClassAd*)NULL;
	if( ad ) {
		if( !Insert( "RIGHT", ad ) ) {
			return false;
		}
			// For the ability to efficiently reference the ad via
			// .RIGHT, it is inserted in the top match ad, but we want
			// its parent scope to be the context ad.
		rad->SetParentScope( rCtx );
	}
	return true;
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
	Remove( "LEFT" );
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
	Remove( "RIGHT" );
	if( rad ) {
		rad->SetParentScope( radParent );
	}
	radParent = NULL;
	rad = NULL;
	return( ad );
}

bool MatchClassAd::
OptimizeRightAdForMatchmaking( ClassAd *ad, std::string *error_msg )
{
	return OptimizeAdForMatchmaking( ad, true, error_msg );
}

bool MatchClassAd::
OptimizeLeftAdForMatchmaking( ClassAd *ad, std::string *error_msg )
{
	return OptimizeAdForMatchmaking( ad, false, error_msg );
}

bool MatchClassAd::
OptimizeAdForMatchmaking( ClassAd *ad, bool is_right, std::string *error_msg )
{
	if( ad->Lookup("my") ||
		ad->Lookup("target") ||
		ad->Lookup("other") ||
		ad->Lookup(ATTR_UNOPTIMIZED_REQUIREMENTS) )
	{
		if( error_msg ) {
			*error_msg = "Optimization of matchmaking requirements failed, because ad already contains one of my, target, other, or UnoptimizedRequirements.";
		}
		return false;
	}

	ExprTree *requirements = ad->Lookup(ATTR_REQUIREMENTS);
	if( !requirements ) {
		if( error_msg ) {
			*error_msg = "No requirements found in ad to be optimized.";
		}
		return false;
	}

		// insert "my" into this ad so that references that use it
		// can be flattened
	Value me;
	me.SetClassAdValue( ad );
	ad->Insert("my",Literal::MakeLiteral(me));

		// insert "target" and "other" into this ad so references can be
		// _partially_ flattened to the more efficient .RIGHT or .LEFT
	char const *other = is_right ? "LEFT" : "RIGT";
	ExprTree *target =
		AttributeReference::MakeAttributeReference(NULL,other,true);
	ad->Insert("target",target);
	ad->Insert("other",target);


	ExprTree *flat_requirements = NULL;
	Value flat_val;

	if( ad->FlattenAndInline(requirements,flat_val,flat_requirements) ) {
		if( !flat_requirements ) {
				// flattened to a value
			flat_requirements = classad::Literal::MakeLiteral(flat_val);
		}
		if( flat_requirements ) {
				// save original requirements
			ExprTree *orig_requirements = ad->Remove(ATTR_REQUIREMENTS);
			if( orig_requirements ) {
				if( !ad->Insert(ATTR_UNOPTIMIZED_REQUIREMENTS,orig_requirements) )
				{
						// Now we have no requirements.  Very bad!
					if( error_msg ) {
						*error_msg = "Failed to rename original requirements.";
					}
					delete orig_requirements;
					return false;
				}
			}

				// insert new flattened requirements
			if( !ad->Insert(ATTR_REQUIREMENTS,flat_requirements) ) {
				if( error_msg ) {
					*error_msg = "Failed to insert optimized requirements.";
				}
				delete flat_requirements;
				return false;
			}
		}
	}

		// After flatenning, no references should remain to MY or TARGET.
		// Even if there are, those can be resolved by the context ads, so
		// we don't need to leave these attributes in the ad.
	ad->Delete("my");
	ad->Remove("other"); // this is a pointer to same object as target
	ad->Delete("target");

	return true;
}

bool MatchClassAd::
UnoptimizeAdForMatchmaking( ClassAd *ad )
{
	ExprTree *orig_requirements = ad->Remove(ATTR_UNOPTIMIZED_REQUIREMENTS);
	if( orig_requirements ) {
		if( !ad->Insert(ATTR_REQUIREMENTS,orig_requirements) ) {
			return false;
		}
	}
	return true;
}

END_NAMESPACE // classad
