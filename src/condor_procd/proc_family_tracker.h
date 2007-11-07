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


#ifndef _PROC_FAMILY_TRACKER_H
#define _PROC_FAMILY_TRACKER_H

#include "../condor_procapi/procapi.h"

class ProcFamilyMonitor;
class ProcFamily;

class ProcFamilyTracker {

public:

	ProcFamilyTracker(ProcFamilyMonitor* pfm) { m_monitor = pfm; }

	virtual ~ProcFamilyTracker() { }
	
	virtual void find_processes(procInfo*&);

protected:
	virtual bool check_process(procInfo*) = 0;

	ProcFamilyMonitor* m_monitor;
};

#endif
