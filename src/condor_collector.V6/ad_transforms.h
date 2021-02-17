/***************************************************************
 *
 * Copyright (C) 1990-2020, Condor Team, Computer Sciences Department,
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

#ifndef _CONDOR_AD_TRANSFORMS_H
#define _CONDOR_AD_TRANSFORMS_H

#include <vector>
#include <memory>

#include "xform_utils.h"

class AdTransforms {
	public:
		AdTransforms() : m_mset_ckpt(nullptr) {}
		~AdTransforms() {}

		void config(const char * param_prefix);

		int transform(ClassAd *ad, CondorError *errorStack);

		bool shouldTransform() { return !m_transforms_list.empty(); }

	private:
		std::vector<std::unique_ptr<MacroStreamXFormSource>> m_transforms_list;
		XFormHash m_mset;
			// m_mset owns this pointer; we do not manage it.
		MACRO_SET_CHECKPOINT_HDR *m_mset_ckpt;
};

#endif
