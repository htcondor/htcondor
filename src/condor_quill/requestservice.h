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
								 int &cluster, int &proc, char *owner);
	void		 freeJobAd(ClassAd *&ad);
	QuillErrCode closeConnection();

	JobQueueSnapshot	*jqSnapshot;	//!< JobQueueSnapshot object
};
#endif
