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

