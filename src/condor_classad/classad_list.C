/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
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

