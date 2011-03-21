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


#ifndef _JOBQUEUESNAPSHOT_H_
#define _JOBQUEUESNAPSHOT_H_

#include "condor_common.h"
#include "jobqueuedatabase.h"
#include "classad_collection.h"
#include "quill_enums.h"

//! JobQueueSnapshot
/*! it provides interface to iterate the query result 
 *  from Job Queue Databse when it services condor_q.
 */
class JobQueueSnapshot {
public:
	//! constructor
	JobQueueSnapshot(const char* dbcon_str);
	//! destructor
	~JobQueueSnapshot();

	//! prepare iteration of Job Ads in the job queue database
	QuillErrCode startIterateAllClassAds(int *clusterarray, 
					     int numclusters, 
					     int *procarray, 
					     int numprocs,
				    	     char *owner, 
					     bool isfullscan,
						time_t scheddBirthdate,
						char *&lastUpdate);
	//! iterate one by one
	QuillErrCode iterateAllClassAds(ClassAd*& ad);
	//! release snapshot
	QuillErrCode release();
	
private:
		// 
		// helper functions
		//
	QuillErrCode getNextClusterAd(char*, ClassAd*&);
	QuillErrCode getNextProcAd(ClassAd*&);

	int job_num;

	int cur_procads_hor_index;
	int	procads_hor_num;
	int cur_procads_ver_index;
	int	procads_ver_num;
	int cur_clusterads_hor_index;
	int	clusterads_hor_num;
	int cur_clusterads_ver_index;
	int	clusterads_ver_num;

	char curClusterId[20];		//!< current Cluster Id
	char curProcId[20];			//!< current Proc Id

	ClassAd				*curClusterAd;	//!< current Job Ad
	JobQueueDatabase	*jqDB;			//!< Databse object
	dbtype 				 dt;
};

#endif
