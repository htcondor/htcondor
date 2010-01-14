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


#include "classad/common.h"
#include "classad/classad.h"
#include "classad/classad_functional_tester.h"

using namespace std;

#if (__GNUC__<3)
#define CLASS
#else
#define CLASS class
#endif

BEGIN_NAMESPACE(classad)

template CLASS map<string, Variable *>;
template CLASS map<string, ClassAd *>;
template CLASS map<string, ClassAdCollection *>;

END_NAMESPACE

