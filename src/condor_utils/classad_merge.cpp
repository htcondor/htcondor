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
				   bool merge_conflicts, bool mark_dirty,
				   bool keep_clean_when_possible)
{

	if (!merge_into || !merge_from) {
		return;
	}

	bool was_dirty_tracking = merge_into->SetDirtyTracking(mark_dirty);

	const char     *name;
	ExprTree       *expression;

	for ( auto itr = merge_from->begin(); itr != merge_from->end(); itr++ ) {

		name = itr->first.c_str();
		expression = itr->second;
		if (merge_conflicts || !merge_into->LookupExpr(name)) {
			if( keep_clean_when_possible ) {
				char *from_expr = NULL;
				char *to_expr = NULL;
				bool equiv=false;

				if( (from_expr=sPrintExpr(*merge_from,name)) &&
					(to_expr=sPrintExpr(*merge_into,name)) )
				{
					if( from_expr && to_expr && strcmp(from_expr,to_expr)==0 ) {
						equiv=true;
					}
				}
				if( from_expr ) { free( from_expr ); from_expr = NULL; }
				if( to_expr ) { free( to_expr ); to_expr = NULL; }

				if( equiv ) {
					continue;
				}
			}

			ExprTree *copy_expression = expression->Copy();
			merge_into->Insert(name, copy_expression);
		}
	}

	merge_into->SetDirtyTracking(was_dirty_tracking);
	return;
}

void MergeClassAdsCleanly(ClassAd *merge_into, ClassAd *merge_from)
{
	return MergeClassAds(merge_into,merge_from,true,true,true);
}


int MergeClassAdsIgnoring(ClassAd *merge_into, ClassAd *merge_from, const AttrNameSet & ignore, bool mark_dirty /*=true*/)
{
	if (!merge_into || !merge_from) {
		return 0;
	}

	bool was_dirty_tracking = merge_into->SetDirtyTracking(mark_dirty);

	int cMerged = 0; // count of merged items
	const char *name;
	ExprTree   *expression;
	for ( auto itr = merge_from->begin(); itr != merge_from->end(); itr++ ) {

		name = itr->first.c_str();
		expression = itr->second;
		// don't merge attributes if the name is in the ignore list.
		if (ignore.find(name) != ignore.end())
			continue;

		ExprTree  *copy_expression = expression->Copy();
		merge_into->Insert(name, copy_expression);
		++cMerged;
	}

	merge_into->SetDirtyTracking(was_dirty_tracking);
	return cMerged;
}

