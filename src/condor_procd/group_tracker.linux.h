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


#ifndef _GROUP_TRACKER_H
#define _GROUP_TRACKER_H

#include "proc_family_tracker.h"
#include "gid_pool.linux.h"

class GroupTracker : public ProcFamilyTracker {

public:

	GroupTracker(ProcFamilyMonitor* pfm, gid_t min_gid, gid_t max_gid);

	bool add_mapping(ProcFamily* family, gid_t& group);
	bool remove_mapping(ProcFamily* family);
	bool check_process(procInfo* pi);

private:

	GIDPool m_gid_pool;
};

#endif
