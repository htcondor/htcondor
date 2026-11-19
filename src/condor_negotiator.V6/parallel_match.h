/***************************************************************
 *
 * Copyright (C) 1990-2026, Condor Team, Computer Sciences Department,
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

#ifndef _PARALLEL_MATCH_H
#define _PARALLEL_MATCH_H

#include "condor_classad.h"
#include <vector>

// Match ad1 against each of the candidates in parallel, using up to
// threads OpenMP threads, and append the matching candidates to matches.
// If halfMatch is true, only the candidate's Requirements are checked;
// otherwise both sides must match.  Returns true if anything matched.
// Does not take ownership of any of the ads.
//
// This lives in the negotiator rather than condor_utils so that only the
// negotiator links against the OpenMP runtime.
bool ParallelIsAMatch(ClassAd *ad1, std::vector<ClassAd*> &candidates, std::vector<ClassAd*> &matches, int threads, bool halfMatch = false);

#endif
