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

#include "condor_common.h"
#include "condor_classad.h"
#include "condor_string.h"
#include "condor_attributes.h"
#include "sslutils.h"
#include "basename.h"
#include "directory.h"
#include "build_job_env.h"

StringMap build_job_env(const ClassAd & ad) {
	StringMap results(1,MyStringHash,updateDuplicateKeys);// rejectDuplicateKeys?

	MyString Iwd;
	if( ! ad.LookupString(ATTR_JOB_IWD, Iwd) ) {
		ASSERT(0);
		dprintf(D_ALWAYS, "Job ClassAd lacks required attribute %s.  Job's environment may be incorrect.\n", ATTR_JOB_IWD);
		return results;
	}

	MyString X509Path;
	if(ad.LookupString(ATTR_X509_USER_PROXY, X509Path)) {
		if( ! fullpath(X509Path.Value()) ) {
			// It's not a full path, so glob on the IWD onto the front
			char * newpath = dircat(Iwd.Value(), X509Path.Value());
			X509Path = newpath;
			delete [] newpath; // dircat returnned newed memory.

		}
		results.insert(X509_USER_PROXY, X509Path.Value());
	}

	return results;
}

int Insert(ClassAd & ad, const char * lhs, const char * rhs) {
	MyString s;
	s.sprintf("%s=\"%s\"",lhs,rhs);
	return ad.Insert(s.Value());
}

#if 0
int main() {
	ClassAd ad;
	Insert(ad,ATTR_JOB_IWD,"/path/to/iwd");
	Insert(ad,ATTR_X509_USER_PROXY,"/path/bob");
	StringMap sm = build_job_env(ad);

	sm.startIterations();
	MyString key,value;
	while(sm.iterate(key,value)) {
		printf("%s=%s\n",key.Value(), value.Value());
	}

	
	return 0;
}
#endif
