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
					     bool isfullscan);
	//! iterate one by one
	QuillErrCode iterateAllClassAds(ClassAd*& ad);
	//! release snapshot
	QuillErrCode release();
	
private:
		// 
		// helper functions
		//
	QuillErrCode getNextClusterAd(const char*&, ClassAd*&);
	QuillErrCode getNextProcAd(ClassAd*&);

	int job_num;
	int cur_procads_str_index;
	int	procads_str_num;
	int cur_procads_num_index;
	int	procads_num_num;
	int cur_clusterads_str_index;
	int	clusterads_str_num;
	int cur_clusterads_num_index;
	int	clusterads_num_num;

	const char*	curClusterId;		//!< current Cluster Id
	const char*	curProcId;			//!< current Proc Id

	ClassAd				*curClusterAd;	//!< current Job Ad
	JobQueueDatabase	*jqDB;			//!< Job Queue Databse object
};

#endif
