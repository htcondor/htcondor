/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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

#include "condor_common.h"
#include "condor_classad.h"
#include "condor_string.h"

typedef HashTable<MyString,MyString> StringMap;
/** Given a job's classad, return environment variables job should get.

This includes:
- X509_USER_PROXY

This should eventually include:
- Condor provided environment like _CONDOR_SCRATCH_DIR
- "POSIX" stuff (HOME, USERNAME, TEMP?)

This might, but may not eventually include:
- Contents of the ATTR_JOB_ENVIRONMENT

Typical use might look something like this (taken from condor_starter.V6.1/starter.C)
		Env * proc_env; // Comes from outside
		StringMap classenv = build_job_env(JobAd);
		sm.startIterations();
		MyString key,value;
		while(sm.iterate(key,value)) {
			proc_env->Put(key.Value(),value.Value());
		}


*/
StringMap build_job_env(const ClassAd & ad);

#endif /* _build_job_env_H_ */
