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
 *  @param merge_confliccts true to copy all attributes, false if you 
 *         don't want to replace attributes that already exist in the
 *         merge_into classad	
 */
void MergeClassAds(ClassAd *merge_into, ClassAd *merge_from, 
				   bool merge_conflicts);

#endif
