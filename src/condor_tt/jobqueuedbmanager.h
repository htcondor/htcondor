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

#ifndef _CONDOR_JOBQUEUEDBMANAGER_H_
#define _CONDOR_JOBQUEUEDBMANAGER_H_

#include "condor_common.h"
#include "condor_daemon_core.h"
#include "daemon.h"

//for the collectorlist::create call
#include "dc_collector.h" 

#include "classadlogentry.h"
#include "jobqueuecollection.h"
#include "quill_enums.h"

class Prober;
class ClassAdLogParser;
class JobQueueDatabase;

//! JobQueueDBManager
/*! \brief this class orchestrates all of Quill.  
 */
class JobQueueDBManager : public Service
{
 public:
		//! constructor	
	JobQueueDBManager();
	
		//! destructor	
	~JobQueueDBManager();
	
		//! read all config options from condor_config file. 
		//! Also create class members instead of doing it in the constructor
	void config(bool reconfig = false);
	
		//! initialize: currently check the DB schema
	QuillErrCode init();
	
		//! maintain the database
	QuillErrCode maintain();
	
		//! get Last Job Queue File Polling Information 
	QuillErrCode getJQPollingInfo(); 	
		//! set Current Job Queue File Polling Information 
	QuillErrCode setJQPollingInfo(); 	
		//	
		// accessors
		//
	void setJobQueueFileName(const char* jqName);
	JobQueueDatabase* getJobQueueDBObj() { return DBObj; }
	ClassAdLogParser* getClassAdLogParser();
	const char* getScheddname() { return scheddname; }
	time_t getScheddbirthdate() { return scheddbirthdate; }
	dbtype getJobQueueDBType() { return dt; }
	char *   fillEscapeCharacters(const char *str);

 private:
		//
		// helper functions
		// ----
	
		//! purge all job queue rows and process the entire job_queue.log file
	QuillErrCode initJobQueueTables();
		//! process only the DELTA
	QuillErrCode addJobQueueTables();
	
		/*! 
		  The following routines are related to the bulk-loading phase
		  of quill where it processes and keeps the entire job queue in 
		  memory and writes it all at once.  We do this only when 
		  a) quill first wakes up, or b) if the log is compressed, or 
		  c) there is an error and we just have to read from scratch
		  It is not called in the frequent case of incremental changes
		  to the log
		*/

		//! build the job queue collection by reading entries in 
		//! job_queue.log file and load them into RDBMS		
	QuillErrCode buildAndWriteJobQueue();

		//! does the actual building of the job queue
	QuillErrCode buildJobQueue(JobQueueCollection *jobQueue);
	
		//! dumps the in-memory job queue to the database
	QuillErrCode loadJobQueue(JobQueueCollection *jobQueue);
	
		//! the routine that processes each entry (insert/delete/etc.)
	QuillErrCode processLogEntry(int op_type, JobQueueCollection *jobQueue);


		/* 
		   The following routines are related to handling the common case 
		   of processing incremental changes to the job_queue.log file
		   Unlike buildAndWriteJobQueue, this does not maintain 
		   an in-memory table of jobs and as so it does not consume 
		   as much memory at the expense of taking slightly longer
		   There's a processXXX for each kind of log entry
		*/

		//! incrementally read and write log entries to database
		//! calls processLogEntry on each new log entry in the job_queue.log
		//!	file
	
	QuillErrCode readAndWriteLogEntries();

		//! is a wrapper over all the processXXX functions
		//! in this and all the processXXX routines
	QuillErrCode processLogEntry(int op_type);
	QuillErrCode processNewClassAd(char* key, 
									char* mytype, 
									char* ttype);
		//! in addition to deleting the classad, this routine is also 
		//! responsible for maintaining the history tables.  Thanks to
		//! this catch, we can get history for free, i.e. without having
		//! to sniff the history file
	QuillErrCode processDestroyClassAd(char* key);
	QuillErrCode processSetAttribute(char* key, 
									  char* name, 
									  char* value);
	QuillErrCode processDeleteAttribute(char* key, 
										char* name);
	QuillErrCode processBeginTransaction(bool exec_later);
	QuillErrCode processEndTransaction(bool exec_later);

		//! deletes all rows from all job queue related tables
	QuillErrCode cleanupJobQueueTables();
	
		//! split key into cid and pid
	JobIdType getProcClusterIds(const char* key, char* cid, char* pid);

		//! utility routine to show database error mesage
	void displayErrorMsg(const char* errmsg);

	void addJQPollingInfoSQL(char* dest, char* src_name, char* src_val);

		//
		// members
		//
	Prober*	            prober;			//!< Prober
	ClassAdLogParser*	caLogParser;	//!< ClassAd Log Parser
	JobQueueDatabase*	DBObj;		//!< Database obj
	XactState	xactState;		    //!< current XACT state
	char*	jobQueueLogFile; 		//!< Job Queue Log File Path
	char*	jobQueueDBConn;  		//!< DB connection string
	char*	jobQueueDBIpAddress;    //!< <IP:PORT> address of DB
	char*	jobQueueDBName;         //!< DB Name
	char*   jobQueueDBUser;
	char*   scheddname;
	time_t	scheddbirthdate;
	dbtype  dt;
};

#endif /* _CONDOR_JOBQUEUEDBMANAGER_H_ */
