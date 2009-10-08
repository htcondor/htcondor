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

static bool _evalBool(ClassAd *ad, ExprTree *tree)
{
	EvalResult result;
	char * constraint = NULL;

	// Evaluate constraint with ad in the target scope so that constraints
	// have the same semantics as the collector queries.  --RR
	if (!tree->EvalTree(NULL, ad, &result)) {
		tree->PrintToNewStr(&constraint);
		if (constraint) {
			dprintf(D_ALWAYS, "can't evaluate constraint: %s\n", constraint);
			free(constraint);
			constraint = NULL;
		}
		return false;
	}
	if (result.type == LX_INTEGER) {
		return (bool)result.i;  // Nominal exit point
	}

	tree->PrintToNewStr(&constraint);
	if (constraint) {
		dprintf(D_ALWAYS, "constraint (%s) does not evaluate to bool\n",
				constraint);
		free(constraint);
		constraint = NULL;
	}
	return false;
}

// Count ads in list that meet constraint.
int ClassAdList::
Count( ExprTree *constraint )
{
    ClassAd *ad = NULL;
    int matchCount  = 0;

    // Check for null constraint.
    if ( constraint == NULL ) {
        return 0;
    }

    Rewind();
    while( (ad = Next() ) ) {
        if ( _evalBool( ad, constraint) ) {
            matchCount++;
        }
    }
    return matchCount;
}

