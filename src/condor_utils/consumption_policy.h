/***************************************************************
 *
 * Copyright (C) 1990-2012, Condor Team, Computer Sciences Department,
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
#ifndef CONSUMPTION_POLICY_H
#define CONSUMPTION_POLICY_H

#include "condor_classad.h"
#include <map>

using std::string;
using std::map;

// returns true if resource ad supports consumption policy feature
bool cp_supports_policy(ClassAd& resource, bool strict = true);

// override legacy RequestXxx with CP values
// saves originals for restoration after requirements matching
void cp_override_requested(ClassAd& job, ClassAd& resource, map<string, double>& consumption);

// restore original RequestXxx values
void cp_restore_requested(ClassAd& job, const map<string, double>& consumption);

// returns true if resource has sufficient assets to service the job
bool cp_sufficient_assets(ClassAd& job, ClassAd& resource);
bool cp_sufficient_assets(ClassAd& resource, const map<string, double>& consumption);

// deduct assets and return the cost.  if 'test' is given as true, do
// not deduct, just return the cost of such a deduction
double cp_deduct_assets(ClassAd& job, ClassAd& resource, bool test=false);

// compute the consumption values and store in the consumption map
void cp_compute_consumption(ClassAd& job, ClassAd& resource, map<string, double>& consumption);


#endif // CONSUMPTION_POLICY_H
