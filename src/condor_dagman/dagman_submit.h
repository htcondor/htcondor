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


#ifndef DAGMAN_SUBMIT_H
#define DAGMAN_SUBMIT_H

#include "condor_id.h"

bool condor_submit(const Dagman &dm, Job* node, CondorID& condorID);

bool send_reschedule(const Dagman &dm);

void set_fake_condorID( int subprocID );

bool fake_condor_submit( CondorID& condorID, Job* job, const char* DAGNodeName,
					const char* directory, const char *logFile );

int get_fake_condorID();

bool writePreSkipEvent( CondorID& condorID, Job* job, const char* DAGNodeName, 
			   const char* directory, const char *logFile );

#endif /* #ifndef DAGMAN_SUBMIT_H */
