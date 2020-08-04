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

////////////////////////////////////////////////////////////////////////////////
//
// prio_rec.h
//
////////////////////////////////////////////////////////////////////////////////

#ifndef _PRIO_REC_H_
#define _PRIO_REC_H_

const 	int		INITIAL_MAX_PRIO_REC = 2048;

/* this record contains all the parameters required for
 * assigning priorities to all jobs */

class prio_rec {
public:
    PROC_ID     id;
    int         pre_job_prio1;
    int         pre_job_prio2;
    int         post_job_prio1;
    int         post_job_prio2;
    int         job_prio;
    int         status;
    int         qdate;
    char        submitter[MAX_CONDOR_USERNAME_LEN];
	int			auto_cluster_id;

	prio_rec() {
		id.cluster = 0;
		id.proc = 0;
		job_prio = 0;
		status = 0;
		qdate = 0;
		submitter[0] = 0;
		auto_cluster_id = 0;
		pre_job_prio1 = 0;
		pre_job_prio2 = 0;
		post_job_prio1 = 0;
		post_job_prio2 = 0;
	}
};

#endif
