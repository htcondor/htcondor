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

namespace classad {

MatchClassAd::
MatchClassAd()
{
	lCtx = rCtx = NULL;
	lad = rad = NULL;
	ladParent = radParent = NULL;
	symmetric_match = NULL;
	right_matches_left = NULL;
	left_matches_right = NULL;
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

	// don't need this and just slows down matchmaking
	this->DisableDirtyTracking();

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

	// Insert the Ad resolver but also lookup before using b/c
	// there are no gaurentees not to collide.
	Insert( "lCtx", lCtx );
	Insert( "rCtx", rCtx );

	symmetric_match = Lookup("symmetricMatch");
	right_matches_left = Lookup("rightMatchesLeft");
	left_matches_right = Lookup("leftMatchesRight");

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
	ladParent = ad ? ad->GetParentScope( ) : (ClassAd*)NULL;
	if( ad ) {
		if( !Insert( "LEFT", ad ) ) {
			lad = NULL;
			ladParent = NULL;
			Delete( "LEFT" );
			return false;
		}
		
		// For the ability to efficiently reference the ad via
		// .LEFT, it is inserted in the top match ad, but we want
		// its parent scope to be the context ad.
		lCtx->SetParentScope(this);
		lad->SetParentScope(lCtx);

		if ( _useOldClassAdSemantics ) {
			lad->alternateScope = rad;
		}
	} else {
		Delete( "LEFT" );
	}
	if ( rad && _useOldClassAdSemantics ) {
		rad->alternateScope = lad;
	}
	return true;
}


bool MatchClassAd::
ReplaceRightAd( ClassAd *ad )
{
	rad = ad;
	radParent = ad ? ad->GetParentScope( ) : (ClassAd*)NULL;
	if( ad ) {
		if( !Insert( "RIGHT", ad ) ) {
			rad = NULL;
			radParent = NULL;
			Delete( "RIGHT" );
			return false;
		}
		
	
		// For the ability to efficiently reference the ad via
		// .RIGHT, it is inserted in the top match ad, but we want
		// its parent scope to be the context ad.
		rCtx->SetParentScope(this);
		rad->SetParentScope(rCtx);

		if ( _useOldClassAdSemantics ) {
			rad->alternateScope = lad;
		}
	} else {
		Delete( "RIGHT" );
	}
	if ( lad && _useOldClassAdSemantics ) {
		lad->alternateScope = rad;
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
		if ( _useOldClassAdSemantics && rad ) {
			lad->alternateScope = NULL;
			rad->alternateScope = NULL;
		}
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
		if ( _useOldClassAdSemantics && lad ) {
			lad->alternateScope = NULL;
			rad->alternateScope = NULL;
		}
	}
	radParent = NULL;
	rad = NULL;
	return( ad );
}

void MatchClassAd::
SetLeftAlias( const std::string &name )
{
	CaseIgnEqStr attr_cmp;
	if ( attr_cmp( name.c_str(), lAlias.c_str() ) ) {
		return;
	}
	// TODO Use ClassAd::Remove() to allow reuse of existing ExprTrees?
	if ( !lAlias.empty() ) {
		lCtx->Delete( name );
		rCtx->Delete( name );
	}
	lAlias = name;
	if ( !lAlias.empty() ) {
		// TODO make reference in right ad point directly at '.LEFT.rCtx.ad'?
		lCtx->Insert( lAlias, AttributeReference::MakeAttributeReference( NULL, "ad", false ) );
		rCtx->Insert( lAlias, AttributeReference::MakeAttributeReference( NULL, "LEFT", true ) );
	}
}

void MatchClassAd::
SetRightAlias( const std::string &name )
{
	CaseIgnEqStr attr_cmp;
	if ( attr_cmp( name.c_str(), rAlias.c_str() ) ) {
		return;
	}
	// TODO Use ClassAd::Remove() to allow reuse of existing ExprTrees?
	if ( !rAlias.empty() ) {
		lCtx->Delete( name );
		rCtx->Delete( name );
	}
	rAlias = name;
	if ( !rAlias.empty() ) {
		// TODO make reference in left ad point directly at '.RIGHT.rCtx.ad'?
		lCtx->Insert( rAlias, AttributeReference::MakeAttributeReference( NULL, "RIGHT", true ) );
		rCtx->Insert( rAlias, AttributeReference::MakeAttributeReference( NULL, "ad", false ) );
	}
}

bool MatchClassAd::
OptimizeRightAdForMatchmaking( ClassAd *ad, std::string *error_msg, const std::string &left_alias, const std::string &right_alias )
{
	return MatchClassAd::OptimizeAdForMatchmaking( ad, true, error_msg, left_alias, right_alias );
}

bool MatchClassAd::
OptimizeLeftAdForMatchmaking( ClassAd *ad, std::string *error_msg, const std::string &left_alias, const std::string &right_alias )
{
	return MatchClassAd::OptimizeAdForMatchmaking( ad, false, error_msg, left_alias, right_alias );
}

bool MatchClassAd::
OptimizeAdForMatchmaking( ClassAd *ad, bool is_right, std::string *error_msg, const std::string &left_alias, const std::string &right_alias )
{
	if( ad->Lookup("my") ||
		ad->Lookup("target") ||
		ad->Lookup("other") ||
		( !left_alias.empty() && ad->Lookup(left_alias) ) ||
		( !right_alias.empty() && ad->Lookup(right_alias) ) ||
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
	if ( !_useOldClassAdSemantics ) {
		Value me;
		ExprTree * pLit;
		me.SetClassAdValue( ad );
		ad->Insert("my",(pLit=Literal::MakeLiteral(me)));
	}

		// insert "target" and "other" into this ad so references can be
		// _partially_ flattened to the more efficient .RIGHT or .LEFT
	char const *other = is_right ? "LEFT" : "RIGHT";
	ExprTree *target = AttributeReference::MakeAttributeReference(NULL,other,true);
	ExprTree *t2 = AttributeReference::MakeAttributeReference(NULL,other,true);
	
	ad->Insert("target",target);
	ad->Insert("other",t2);
	if ( !left_alias.empty() ) {
		if ( is_right ) {
			ad->Insert( left_alias, AttributeReference::MakeAttributeReference( NULL, "LEFT", true ) );
		} else {
			ad->Insert( left_alias, AttributeReference::MakeAttributeReference( NULL, "self", false ) );
		}
	}
	if ( !right_alias.empty() ) {
		if ( is_right ) {
			ad->Insert( right_alias, AttributeReference::MakeAttributeReference( NULL, "self", false ) );
		} else {
			ad->Insert( right_alias, AttributeReference::MakeAttributeReference( NULL, "RIGHT", true ) );
		}
	}


	ExprTree *flat_requirements = NULL;
	Value flat_val;

	if( ad->FlattenAndInline(requirements,flat_val,flat_requirements) ) {
		if( !flat_requirements ) {
				// flattened to a value
			flat_requirements = Literal::MakeLiteral(flat_val);
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
					delete flat_requirements;
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
	// TODO The failure cases above should run this cleanup code
	if ( !_useOldClassAdSemantics ) {
		ad->Delete("my");
	}
	ad->Delete("other"); 
	ad->Delete("target");
	if ( !left_alias.empty() ) {
		ad->Delete( left_alias );
	}
	if ( !right_alias.empty() ) {
		ad->Delete( right_alias );
	}

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

bool MatchClassAd::
EvalMatchExpr(ExprTree *match_expr)
{
	Value val;
	if( !match_expr ) {
		return false;
	}

	if( EvaluateExpr( match_expr, val ) ) {
		bool result = false;
		if( val.IsBooleanValueEquiv( result ) ) {
			return result;
		}
		long long int_result = 0;
		if( val.IsIntegerValue( int_result ) ) {
			return int_result != 0;
		}
	}
	return false;
}

bool MatchClassAd::
symmetricMatch()
{
	return EvalMatchExpr( symmetric_match );
}

bool MatchClassAd::
rightMatchesLeft()
{
	return EvalMatchExpr( right_matches_left );
}

bool MatchClassAd::
leftMatchesRight()
{
	return EvalMatchExpr( left_matches_right );
}

} // classad
