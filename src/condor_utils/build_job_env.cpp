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
#include "condor_debug.h"
#include "condor_classad.h"
#include "condor_attributes.h"
#include "basename.h"
#include "directory.h"
#include "build_job_env.h"

#define X509_USER_PROXY "X509_USER_PROXY"

void build_job_env(Env &job_env, const ClassAd & ad, bool using_file_transfer)
{
	std::string Iwd;
	if( ! ad.LookupString(ATTR_JOB_IWD, Iwd) ) {
		ASSERT(0);
		dprintf(D_ALWAYS, "Job ClassAd lacks required attribute %s.  Job's environment may be incorrect.\n", ATTR_JOB_IWD);
		return;
	}

	std::string X509Path;
	if(ad.LookupString(ATTR_X509_USER_PROXY, X509Path)) {
		if(using_file_transfer) {
			// The X509 proxy was put into the IWD, effectively flattening
			// any relative or absolute paths it might have.  So chomp it down.
			// (Don't try to do this in one line; the old string might be deleted
			// before the copy.)
			std::string tmp = condor_basename(X509Path.c_str());
			X509Path = tmp; 
		}
		if( ! fullpath(X509Path.c_str()) ) {
			// It's not a full path, so glob on the IWD onto the front
			std::string tmp;
			dircat(Iwd.c_str(), X509Path.c_str(), tmp);
			X509Path = tmp;
		}
		job_env.SetEnv(X509_USER_PROXY, X509Path.c_str());
	}
}

