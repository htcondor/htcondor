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


#ifndef _REQUESTSERVICE_H_
#define _REQUESTSERVICE_H_

#include "condor_common.h"
#include "condor_io.h"
#include "jobqueuedbmanager.h"
#include "jobqueuesnapshot.h"
#include "quill_enums.h"

//! RequestService
/*! this class services requests from condor_q via socket communication.

 *   Note: This class is largely deprecated.  This is because condor_q++
 *   now talks directly to postgres instead of calling this command
 */
class RequestService
{
public:
	//! constructor
	RequestService(const char* DBconn);
	//! destructor
	~RequestService();

	//! service requests from condor_q via sockect
	QuillErrCode service(ReliSock *syscall_sock);

private:
		//
		// helper functions
		//
	QuillErrCode getNextJobByConstraint(const char* constraint, 
										int initScan, 
										ClassAd *&);
	static bool  parseConstraint(const char *constraint, 
					int *&clusterarray, int &numclusters, 
					int *&procarray, int &numprocs,
					char *owner);
	void		 freeJobAd(ClassAd *&ad);
	QuillErrCode closeConnection();

	JobQueueSnapshot	*jqSnapshot;	//!< JobQueueSnapshot object
};
#endif
