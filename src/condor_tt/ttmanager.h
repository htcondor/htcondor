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

#ifndef _TTMANAGER_H_
#define _TTMANAGER_H_

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "daemon.h"
#include "jobqueuedbmanager.h"
#include "quill_enums.h"

#define CONDOR_TT_MAXLOGNUM 7
#define CONDOR_TT_MAXLOGPATHLEN 1024

// TTManager
/* This class implements TT's SQL log parsing and database updating logic
 */
class TTManager : public Service
{
 public:
	TTManager();
	
	~TTManager();
	
		// read all config options from condor_config file. 
		// Also create class members instead of doing it in the constructor
	void    config(bool reconfig = false);

		// register all timer handlers
	void	registerTimers();
	
		// timer handler for maintaining job queue
	void	pollingTime();

 private:

		// top level maintain function
	void     maintain();

		// general event log processing function (non-transactional data)
	QuillErrCode event_maintain();
		// xml log maintain function
	QuillErrCode xml_maintain();

		// check and throw away big files
	void    checkAndThrowBigFiles();

		// create the QUILL_AD that's sent to the collector
	void     createQuillAd(void);

		// update the QUILL_AD's dynamic attributes
	void     updateQuillAd(void);

		// database updating functions for various tables
	QuillErrCode insertMachines(AttrList *ad);
	QuillErrCode insertEvents(AttrList *ad);
	QuillErrCode insertFiles(AttrList *ad);
	QuillErrCode insertFileusages(AttrList *ad);
	QuillErrCode insertHistoryJob(AttrList *ad);
	QuillErrCode insertBasic(AttrList *ad, char *tableName);
	QuillErrCode updateBasic(AttrList *info, AttrList *condition, 
							 char *tableName);
	QuillErrCode insertScheddAd(AttrList *ad);
	QuillErrCode insertMasterAd(AttrList *ad);
	QuillErrCode insertNegotiatorAd(AttrList *ad);
	QuillErrCode insertRuns(AttrList *ad);
	QuillErrCode insertTransfers(AttrList *ad);
	
	void handleErrorSqlLog();

	char    sqlLogList[CONDOR_TT_MAXLOGNUM][CONDOR_TT_MAXLOGPATHLEN];

		/* Adding 1 more to the copy list below for a special "thrown" log 
		   which records events when a log is thrown away because it's size 
		   exceeds a limit. 
		*/
	char    sqlLogCopyList[CONDOR_TT_MAXLOGNUM+1][CONDOR_TT_MAXLOGPATHLEN];
	int     numLogs;
        
	int		pollingTimeId;		   // timer handler id of pollingTime function
	int		pollingPeriod;		   // polling time period in seconds

	CollectorList   *collectors;
	ClassAd         *quillad;

	JobQueueDatabase* DBObj;
	JobQueueDBManager jqDBManager;
	dbtype dt;
	MyString  currentSqlLog;
	MyString  errorSqlStmt;
	bool      maintain_db_conn;
};

#endif /* _TTMANAGER_H_ */
