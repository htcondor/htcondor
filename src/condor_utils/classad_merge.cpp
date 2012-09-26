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
#include "condor_classad.h"
#include "classad_merge.h"

void MergeClassAds(ClassAd *merge_into, ClassAd *merge_from, 
				   bool merge_conflicts, bool mark_dirty)
{

	if (!merge_into || !merge_from) {
		return;
	}

	merge_from->ResetName();
	merge_from->ResetExpr();

	const char     *name;
	ExprTree       *expression;

	while ( merge_from->NextExpr(name, expression) ) {

		if (merge_conflicts || !merge_into->LookupExpr(name)) {
			ExprTree  *copy_expression;

			copy_expression = expression->Copy();
			merge_into->Insert(name, copy_expression,false);
			if ( !mark_dirty ) {
				merge_into->SetDirtyFlag(name, false);
			}
		}
	}

	return;
}
