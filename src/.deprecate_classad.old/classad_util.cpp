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
#include "condor_debug.h"
#include "condor_classad.h"
#include "condor_classad_util.h"
#include "condor_adtypes.h"
#include "MyString.h"

bool EvalBool(AttrList* ad, ExprTree *tree)
{
  EvalResult result;

  // Evaluate constraint with ad in the target scope so that constraints
  // have the same semantics as the collector queries.  --RR
  if (!tree->EvalTree(NULL, ad, &result)) {        
    return false;
  }
        
  if (result.type == LX_INTEGER) {
    return (bool)result.i;
  }
         
  return false;
}

bool EvalBool(ClassAd *ad, const char *constraint)
{
	static ExprTree *tree = NULL;
	static char * saved_constraint = NULL;
	EvalResult result;
	bool constraint_changed = true;

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
		if (ParseClassAdRvalExpr(constraint, tree) != 0) {
			dprintf(D_ALWAYS,
				"can't parse constraint: %s\n", constraint);
			return false;
		}
		saved_constraint = strdup(constraint);
	}

	// Evaluate constraint with ad in the target scope so that constraints
	// have the same semantics as the collector queries.  --RR
	if (!tree->EvalTree(NULL, ad, &result)) {
		dprintf(D_ALWAYS, "can't evaluate constraint: %s\n", constraint);
		return false;
	}
	if (result.type == LX_INTEGER) {
		return (bool)result.i;
	}
	dprintf(D_ALWAYS, "constraint (%s) does not evaluate to bool\n",
		constraint);
	return false;
}

bool
ClassAdsAreSame( ClassAd* ad1, ClassAd* ad2, StringList* ignored_attrs,
				 bool verbose )
{
	ExprTree *ad1_expr, *ad2_expr;
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
		if( *ad1_expr == *ad2_expr ) {
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
