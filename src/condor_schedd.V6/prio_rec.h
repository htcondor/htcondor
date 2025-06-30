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

#include <deque>

/* this record contains all the parameters required for
 * assigning priorities to all jobs */

class prio_rec {
public:
	PROC_ID     id{0,0};
	int         pre_job_prio1{0};
	int         pre_job_prio2{0};
	int         post_job_prio1{0};
	int         post_job_prio2{0};
	int         job_prio{0};
	int         auto_cluster_id{0};
	std::string submitter;
	bool        not_runnable{false};
	bool        matched{false};
};

// prio rec lower bound by submitter
// This allows us to binary search the PrioRecArray by submitter
// Note this must match the primary key comparison in struct prio_compar (in qmgmt.cpp)
struct prio_rec_submitter_lb {
	bool operator()(const prio_rec& a, const std::string &user) const {
		if (a.submitter.length() < user.length()) {
			return false;
		}

		if (a.submitter.length() > user.length()) {
			return true;
		}

		if (a.submitter > user) {
			return true;
		}

		return false;
	}
};

// The corresponding upper bound function, the negation of the above
struct prio_rec_submitter_ub {
	bool operator()(const std::string &user, const prio_rec &a) const {
		if (a.submitter.length() < user.length()) {
			return true;
		}

		if (a.submitter.length() > user.length()) {
			return false;
		}

		if (a.submitter < user) {
			return true;
		}

		return false;
	}
};


#endif
