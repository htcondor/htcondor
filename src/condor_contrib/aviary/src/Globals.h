/***************************************************************
 *
 * Copyright (C) 2009-2011 Red Hat, Inc.
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

#ifndef _GLOBALS_H
#define _GLOBALS_H

// c++ includes
#include <map>

// condor includes
#include "condor_common.h"

// local includes
#include "Job.h"
#include "JobServerObject.h"
#include "SubmissionObject.h"
#include "cmpstr.h"

using namespace std;
using namespace aviary::query;

namespace aviary {
namespace query {

typedef map<const char *, Job *, cmpstr> JobCollectionType;
typedef map<const char *, SubmissionObject *, cmpstr> SubmissionCollectionType;
typedef map<int, const char *> OwnerlessClusterType;

extern JobCollectionType g_jobs;
extern SubmissionCollectionType g_submissions;
extern OwnerlessClusterType g_ownerless;

}}

#endif /* _GLOBALS_H */
