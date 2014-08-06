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


#ifndef CLASSAD_MERGE_H
#define CLASSAD_MERGE_H

#include "condor_common.h"
#include "condor_classad.h"

/** Merge one ClassAd into another.
 *  @param merge_into The ClassAd that is the recipient of attributes
 *  @param merge_from The ClassAd that is giving the attributes
 *  @param merge_conflicts true to copy all attributes, false if you 
 *         don't want to replace attributes that already exist in the
 *         merge_into classad	
 *  @param mark_dirty true to mark merged attributes as dirty, false
 *         to mark them as clean
 *  @param keep_clean_when_possible - when true, compare values for each
 *         attribute to be merged from the source into the destination,
 *         and only merge if different.
 */
void MergeClassAds(ClassAd *merge_into, ClassAd *merge_from, 
				   bool merge_conflicts, bool mark_dirty = true, bool keep_clean_when_possible = false);

/** Like MergeClassAds() with the following settings:
 *  merge_conflicts = true
 *  mark_dirty = true
 *  keep_clean_when_possible=true
 */
void MergeClassAdsCleanly(ClassAd *merge_into, ClassAd *merge_from);

/* Like MergeClassAds() but with a set of attributes that will not be merged
 *
 */
int MergeClassAdsIgnoring(ClassAd *merge_into, ClassAd *merge_from,
						  const AttrNameSet & ignore, bool mark_dirty = true);

#endif
