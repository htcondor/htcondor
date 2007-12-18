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


#ifndef _PARENT_TRACKER_H
#define _PARENT_TRACKER_H

#include "proc_family_tracker.h"

class ParentTracker : public ProcFamilyTracker {

public:

	ParentTracker(ProcFamilyMonitor* pfm) :
		ProcFamilyTracker(pfm), m_keep_checking(false) { }

	void find_processes(procInfo*&);
	bool check_process(procInfo*);

private:
	bool m_keep_checking;
};

#endif
