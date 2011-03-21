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

#ifndef _build_job_env_H_
#define _build_job_env_H_

#include "condor_classad.h"
#include "env.h"

/** Given a job's classad, return environment variables job should get.

The Env object passed in is assumed to already contain the environment
variables given by the user in the job ad (in the Env or Environment
attribute). The variables set here may or may no override those already
set, as deemed appropriate.

This includes:
- X509_USER_PROXY (will override what's already set in job_env)

This should eventually include:
- Condor provided environment like _CONDOR_SCRATCH_DIR
- "POSIX" stuff (Possible inclusion: LOGNAME (aka username),
  HOME, SHELL, PATH.  Less likely: USER, USERNAME, PWD, TZ, TERM)

This might, but may not eventually include:
- Contents of the ATTR_JOB_ENVIRONMENT

Typical use might look something like this (taken from
condor_starter.V6.1/starter.C)

		Env * proc_env; // Comes from outside
		ClassAd JobAd; // Comes from outside
		bool using_file_transfer; // Comes from outside
		build_job_env(*proc_env, JobAd, using_file_transfer);

Input:

job_env - The environment of the job. Any environment changes will be made
to this object.

ad - The jobs classad.  It is assumed that ATTR_JOB_IWD is set to
something meanful _locally_.  (condor_starter.V6.1 rewrites
ATTR_JOB_IWD to be the local execute directory if the job uses
file transfer.  This is correct behavior.)

using_file_transfer - Was file transfer used?  Relevant because
sometimes we need to rewrite paths.

*/
void build_job_env(Env &job_env, const ClassAd & ad, bool using_file_transfer);

#endif /* _build_job_env_H_ */
