/***************************************************************
 *
 * Copyright (C) 1990-2016, Condor Team, Computer Sciences Department,
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

#ifndef _CONDOR_JOB_TRANSFORMS_H
#define _CONDOR_JOB_TRANSFORMS_H

#include "xform_utils.h"
#include <vector>

class JobTransforms {
 public:
	JobTransforms();
	~JobTransforms();

	void initAndReconfig();

	int transformJob(
		ClassAd *ad,
		const PROC_ID & jid,
		classad::References * xform_attrs,
		CondorError *errorStack,
		bool is_late_mat = false);

	bool shouldTransform() { return !transforms_list.empty(); }

 private:
	std::vector<MacroStreamXFormSource *> transforms_list;
	XFormHash mset;
	MACRO_SET_CHECKPOINT_HDR * mset_ckpt;

	int set_dirty_attributes(ClassAd *ad, int cluster, int proc, classad::References * attrs=nullptr);
	void clear_transforms_list();
};


#endif
