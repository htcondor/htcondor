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

#include "compat_classad_util.h"
#include "classad_oldnew.h"

/* TODO This function needs to be tested.
 */
int Parse(const char*str, MyString &name, classad::ExprTree*& tree, int*pos)
{
	classad::ClassAdParser parser;
	classad::ClassAd *newAd;

		// We don't support the pos argument at the moment.
	if ( pos ) {
		*pos = 0;
	}

		// String escaping is different between new and old ClassAds.
		// We need to convert the escaping from old to new style before
		// handing the expression to the new ClassAds parser.
	std::string newAdStr = "[";
	for ( int i = 0; str[i] != '\0'; i++ ) {
		if ( str[i] == '\\' && 
			 ( str[i + 1] != '"' ||
			   str[i + 1] == '"' &&
			   ( str[i + 2] == '\0' || str[i + 2] == '\n' ||
				 str[i + 2] == '\r') ) ) {
			newAdStr.append( 1, '\\' );
		}
		newAdStr.append( 1, str[i] );
	}
	newAdStr += "]";
	newAd = parser.ParseClassAd( newAdStr );
	if ( newAd == NULL ) {
		tree = NULL;
		return 1;
	}
	if ( newAd->size() != 1 ) {
		delete newAd;
		tree = NULL;
		return 1;
	}
	
	classad::ClassAd::iterator itr = newAd->begin();
	name = itr->first.c_str();
	tree = itr->second->Copy();
	delete newAd;
	return 0;
}

/* TODO This function needs to be tested.
 */
int ParseClassAdRvalExpr(const char*s, classad::ExprTree*&tree, int*pos)
{
	classad::ClassAdParser parser;
	std::string str = s;
	if ( parser.ParseExpression( str, tree, true ) ) {
		return 0;
	} else {
		tree = NULL;
		if ( pos ) {
			*pos = 0;
		}
		return 1;
	}
}

/* TODO This function needs to be tested.
 */
const char *ExprTreeToString( classad::ExprTree *expr )
{
	static std::string buffer;
	classad::ClassAdUnParser unparser;

	unparser.SetOldClassAd( true );
	unparser.Unparse( buffer, expr );

	return buffer.c_str();
}

static compat_classad::ClassAd *empty_ad = NULL;

/* TODO This function needs to be written.
 */
bool EvalBool(compat_classad::ClassAd *ad, const char *constraint)
{
	static classad::ExprTree *tree = NULL;
	static char * saved_constraint = NULL;
	compat_classad::EvalResult result;
	bool constraint_changed = true;

	if ( empty_ad == NULL ) {
		empty_ad = new compat_classad::ClassAd;
	}

	if ( saved_constraint ) {
		if ( strcmp(saved_constraint,constraint) == 0 ) {
			constraint_changed = false;
		}
	}

	if ( constraint_changed ) {
		// constraint has changed, or saved_constraint is NULL
		if ( saved_constraint ) {
			free(saved_constraint);
			saved_constraint = NULL;
		}
		if ( tree ) {
			delete tree;
			tree = NULL;
		}
		if ( ParseClassAdRvalExpr( constraint, tree ) != 0 ) {
			dprintf( D_ALWAYS,
				"can't parse constraint: %s\n", constraint );
			return false;
		}
		saved_constraint = strdup( constraint );
	}

	// Evaluate constraint with ad in the target scope so that constraints
	// have the same semantics as the collector queries.  --RR
	if ( !EvalExprTree( tree, empty_ad, ad, &result ) ) {
		dprintf( D_ALWAYS, "can't evaluate constraint: %s\n", constraint );
		return false;
	}
	if ( result.type == compat_classad::LX_INTEGER ) {
		return (bool)result.i;
	}
	dprintf( D_ALWAYS, "constraint (%s) does not evaluate to bool\n",
		constraint );
	return false;
}

/* TODO This function needs to be written.
 */
bool EvalBool(compat_classad::ClassAd *ad, classad::ExprTree *tree)
{
	compat_classad::EvalResult result;

	if ( empty_ad == NULL ) {
		empty_ad = new compat_classad::ClassAd;
	}

	// Evaluate constraint with ad in the target scope so that constraints
	// have the same semantics as the collector queries.  --RR
	if ( !EvalExprTree( tree, empty_ad, ad, &result ) ) {        
		return false;
	}

	if ( result.type == compat_classad::LX_INTEGER ) {
		return (bool)result.i;
	}

	return false;
}

bool ClassAdsAreSame( compat_classad::ClassAd *ad1, compat_classad::ClassAd * ad2, StringList *ignored_attrs, bool verbose )
{
	classad::ExprTree *ad1_expr, *ad2_expr;
	const char* attr_name;
	ad2->ResetExpr();
	bool found_diff = false;
	while( ad2->NextExpr(attr_name, ad2_expr) && ! found_diff ) {
		if( ignored_attrs && ignored_attrs->contains_anycase(attr_name) ) {
			if( verbose ) {
				dprintf( D_FULLDEBUG, "ClassAdsAreSame(): skipping \"%s\"\n",
						 attr_name );
			}
			continue;
		}
		ad1_expr = ad1->LookupExpr( attr_name );
		if( ! ad1_expr ) {
				// no value for this in ad1, the ad2 value is
				// certainly different
			if( verbose ) {
				dprintf( D_FULLDEBUG, "ClassAdsAreSame(): "
						 "ad2 contains %s and ad1 does not\n", attr_name );
			}
			found_diff = true;
			break;
		}
		if( ad1_expr->SameAs( ad2_expr ) ) {
			if( verbose ) {
				dprintf( D_FULLDEBUG, "ClassAdsAreSame(): value of %s in "
						 "ad1 matches value in ad2\n", attr_name );
			}
		} else {
			if( verbose ) {
				dprintf( D_FULLDEBUG, "ClassAdsAreSame(): value of %s in "
						 "ad1 is different than in ad2\n", attr_name );
			}
			found_diff = true;
			break;
		}
	}
	return ! found_diff;
}

int EvalExprTree( classad::ExprTree *expr, compat_classad::ClassAd *source,
				  compat_classad::ClassAd *target, compat_classad::EvalResult *result )
{
	int rc = TRUE;
	if ( !expr || !source || !result ) {
		return FALSE;
	}

	classad::Value val;
	const classad::ClassAd *old_scope = expr->GetParentScope();
	classad::MatchClassAd *mad = NULL;

	expr->SetParentScope( source );
	if ( target && target != source ) {
		mad = new classad::MatchClassAd( source, target );
	}
	if ( source->EvaluateExpr( expr, val ) ) {
		switch ( val.GetType() ) {
		case classad::Value::ERROR_VALUE:
			result->type = compat_classad::LX_ERROR;
			break;
		case classad::Value::UNDEFINED_VALUE:
			result->type = compat_classad::LX_UNDEFINED;
			break;
		case classad::Value::BOOLEAN_VALUE: {
			result->type = compat_classad::LX_BOOL;
			bool v;
			val.IsBooleanValue( v );
			result->i = v ? 1 : 0;
			break;
		}
		case classad::Value::INTEGER_VALUE:
			result->type = compat_classad::LX_INTEGER;
			val.IsIntegerValue( result->i );
			break;
		case classad::Value::REAL_VALUE: {
			result->type = compat_classad::LX_FLOAT;
			double d;
			val.IsRealValue( d );
			result->f = d;
			break;
		}
		case classad::Value::STRING_VALUE: {
			result->type = compat_classad::LX_STRING;
			std::string s;
			val.IsStringValue( s );
			result->s = strnewp( s.c_str() );
			break;
		}
		default:
			rc = FALSE;
		}
	} else {
		rc = FALSE;
	}

	if ( mad ) {
		mad->RemoveLeftAd();
		mad->RemoveRightAd();
		delete mad;
	}
	expr->SetParentScope( old_scope );

	return rc;
}

bool IsAMatch( compat_classad::ClassAd *ad1, compat_classad::ClassAd *ad2 )
{
	return IsAHalfMatch(ad1, ad2) && IsAHalfMatch(ad2, ad1);
}

bool IsAHalfMatch( compat_classad::ClassAd *my, compat_classad::ClassAd *target )
{
	static classad::ExprTree *reqsTree = NULL;
	compat_classad::EvalResult *val;	
	
	if( stricmp(target->GetMyTypeName(),my->GetTargetTypeName()) &&
	    stricmp(my->GetTargetTypeName(),ANY_ADTYPE) )
	{
		return false;
	}

	if ((val = new compat_classad::EvalResult) == NULL)
	{
		EXCEPT("Out of memory -- quitting");
	}

	if ( reqsTree == NULL ) {
		ParseClassAdRvalExpr ("MY.Requirements", reqsTree);
	}
	EvalExprTree( reqsTree, my, target, val );
	if (!val || val->type != compat_classad::LX_INTEGER)
	{
		delete val;
		return false;
	}
	else
	if (!val->i)
	{
		delete val;
		return false;
	}

	delete val;
	return true;
}

void AttrList_setPublishServerTime( bool publish )
{
	AttrList_setPublishServerTimeMangled( publish );
}
