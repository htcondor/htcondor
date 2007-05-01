/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
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
