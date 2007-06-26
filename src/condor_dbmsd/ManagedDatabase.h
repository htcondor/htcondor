/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
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
#ifndef _MANAGED_DATABASE_H
#define _MANAGED_DATABASE_H

#include "condor_common.h"
#include "quill_enums.h"

#include "jobqueuedatabase.h"

class ManagedDatabase {
 public:
	ManagedDatabase();
	virtual ~ManagedDatabase();
	void PurgeDatabase();
	void ReindexDatabase();

	dbtype dt;
	char *dbIpAddress;
	char *dbName;
	char *dbUser;
	char *dbConnStr;
	JobQueueDatabase *DBObj;
	int resourceHistoryDuration; // in number of days
	int runHistoryDuration; // in number of days
	int jobHistoryDuration; // in number of days
	int dbSizeLimit; // in number of gigabytes
};

#endif

