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
				   bool merge_conflicts)
{

	if (!merge_into || !merge_from) {
		return;
	}

	merge_from->ResetName();
	merge_from->ResetExpr();

	while (1) {
		const char     *name;
		ExprTree       *expression;

		name       = merge_from->NextNameOriginal();
		expression = merge_from->NextExpr();

		if (name == NULL || expression == NULL) {
			break;
		} else {
			if (merge_conflicts || !merge_into->Lookup(name)) {
				ExprTree  *copy_expression;

				copy_expression = expression->DeepCopy();
				merge_into->Insert(copy_expression);
			}
		}
	}

	return;
}
