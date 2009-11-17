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

#ifndef _JOBQUEUEDBMANAGER_H_
#define _JOBQUEUEDBMANAGER_H_

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

#ifndef MAX_FIXED_SQL_STR_LENGTH
#define MAX_FIXED_SQL_STR_LENGTH 2048
#endif

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
	void    config(bool reconfig = false);
	
		//! initialize: currently check the DB schema
	QuillErrCode	init(bool initJQDB);
	
		//! register all timer and command handlers
	void	registerAll();
		//! register all command handlers
	void	registerCommands();
		//! register all timer handlers
	void	registerTimers();
	
		//! maintain the database
	QuillErrCode  	maintain();
	
		//! get Last Job Queue File Polling Information 
	QuillErrCode    getJQPollingInfo();
		//! set Current Job Queue File Polling Information 
	QuillErrCode	setJQPollingInfo();
		//	
		// accessors
		//
	void	            setJobQueueFileName(const char* jqName);
	JobQueueDatabase* 	getJobQueueDBObj() { return DBObj; }
	const char*         getJobQueueDBConn() { return jobQueueDBConn; }
	ClassAdLogParser*	getClassAdLogParser();
	
	
		//
		// handlers
		//

		//! timer handler for maintaining job queue and sending QUILL_AD to collector
	void		pollingTime();	

		//! command handler QMGMT_CMD command; services condor_q queries
		//! posed to quill usually as a fail-over option when the database 
		//! is directly not accessible.  
	int 		handle_q(int, Stream*);
	

		//! escape quoted strings since postgres doesn't like 
		//! unescaped single quotes
	static char *   fillEscapeCharacters(char *str);
	
 private:
	ClassAd 	    *ad;
	
		//! create the QUILL_AD that's sent to the collector
	void 	 createQuillAd(void);

		//! update the QUILL_AD's dynamic attributes
	void 	 updateQuillAd(void);
		//
		// helper functions
		// ----
		// All DB access methods here do not call Xact and connection related 
		// function within them.
		// So you must call connectDB() and disconnectDB() before and after 
		// calling them.
		//
	
		//! purge all job queue rows and process the entire job_queue.log file
	QuillErrCode 	initJobQueueTables();
		//! process only the DELTA
	QuillErrCode 	addJobQueueTables();
		//! process records from previous job queue files during truncation
	QuillErrCode 	addOldJobQueueRecords();
	

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
	QuillErrCode 	buildAndWriteJobQueue();

		//! does the actual building of the job queue
	QuillErrCode    buildJobQueue(JobQueueCollection *jobQueue);
	
		//! dumps the in-memory job queue to the database
	QuillErrCode    loadJobQueue(JobQueueCollection *jobQueue);
	
		//! the routine that processes each entry (insert/delete/etc.)
	QuillErrCode	processLogEntry(int op_type, JobQueueCollection *jobQueue);


		/* 
		   The following routines are related to handling the common case 
		   of processing incremental changes to the job_queue.log file
		   Unlike buildAndWriteJobQueue, this does not maintain 
		   an in-memory table of jobs and as so it does not consume 
		   as much memory at the expense of taking slightly longer
		   There's a processXXX for each kind of log entry
		*/

		//! incrementally read and write log entries to database
		//! calls processLogEntry on each new log entry in the job_queue.log file
	
	QuillErrCode    readAndWriteLogEntries(ClassAdLogParser *parser);

		//! is a wrapper over all the processXXX functions
		//! in this and all the processXXX routines, if exec_later == true, 
		//! a SQL string is returned instead of actually sending it to the DB.
		//! However, we always have exec_later = false, which means it actually
		//! writes to the database in an eager fashion
	QuillErrCode	processLogEntry(int op_type, 
					bool exec_later, 
					ClassAdLogParser *parser);
	QuillErrCode	processNewClassAd(char* key, 
					char* mytype, 
					char* ttype, 
					bool exec_later = false);
		//! in addition to deleting the classad, this routine is also 
		//! responsible for maintaining the history tables.  Thanks to
		//! this catch, we can get history for free, i.e. without having
		//! to sniff the history file
	QuillErrCode    processDestroyClassAd(char* key, bool exec_later = false);
	QuillErrCode	processSetAttribute(char* key, 
						char* name, 
						char* value, 
						bool exec_later = false);
	QuillErrCode    processDeleteAttribute(char* key, 
						char* name, 
						bool exec_later = false);
	QuillErrCode    processBeginTransaction(bool exec_later = false);
	QuillErrCode	processEndTransaction(bool exec_later = false);


		//! deletes all rows from all job queue related tables
	QuillErrCode    cleanupJobQueueTables();
	
		//! runs the postgres garbage collection and statistics 
		//! collection routines on the job queue related tables
	QuillErrCode    tuneupJobQueueTables();

		//! deletes all history rows that are older than a certain 
		//! user specified period (specified by QUILL_HISTORY_DURATION 
		//! condor_config file).  
		//! Since purging is based on a timer, quill needs to be 
		//! up for at least the duration of 
		//! QUILL_HISTORY_DURATION, in order for purging to occur 
		//! on a timely basis
	void             purgeOldHistoryRows();

		//! After the history purge, we need to reindex all the
		//! tables.
	void             reindexTables();

		//! split key into cid and pid
	JobIdType	    getProcClusterIds(const char* key, 
						char* cid, 
						char* pid);

		//! utility routine to show database error mesage
	void	displayDBErrorMsg(const char* errmsg);

		//! connect and disconnect to the postgres database
	QuillErrCode 	connectDB(XactState Xact = BEGIN_XACT);
	QuillErrCode    disconnectDB(XactState commit = COMMIT_XACT);

		//! checks the database and its schema 
		//! if necessary creates the database and all requisite tables/views
	QuillErrCode    checkSchema();

		//! concatenates src_name=val to dest SQL string
	void	addJQPollingInfoSQL(char* dest, char* src_name, char* src_val);
	
		// gets the writer password needed by quill daemon to access database
	char *  getWritePassword(char *write_passwd_fname);

		//
		// members
		//
	Prober*	            prober;			//!< Prober
	ClassAdLogParser*   caLogParser;	//!< ClassAd Log Parser
	JobQueueDatabase*   DBObj;		//!< Database object

	XactState	xactState;		    //!< current XACT state

		//together these constitute the dynamic attributes that are inserted
		//into the quill ad sent to the collector
	int     totalSqlProcessed;
	int     lastBatchSqlProcessed;
	int     secsLastBatch;
	bool    isConnectedToDB;

	char*	jobQueueLogFile; 	//!< Job Queue Log File Path
	char*	jobQueueDBConn;  	//!< DB connection string
	char*	jobQueueDBIpAddress;    //!< <IP:PORT> address of DB
	char*	jobQueueDBName;         //!< DB Name

	int	purgeHistoryTimeId;	//!< timer handler id of purgeOldHistoryRows
	int	purgeHistoryDuration;	//!< number of days to keep history around
	int     historyCleaningInterval;//!< number of hours between two successive
	                                //!< successive calls to purgeOldHistoryRows

	int	pollingTimeId;		//!< timer handler id of pollingTime function
	int	pollingPeriod;		//!< polling time period in seconds

	int reindexTimeId; 		//!< timer handler id for reindex tables

	char*	multi_sql_str;		//!< buffer for SQL

	int	numTimesPolled;		//!< used to vacuum and analyze job queue tables
	int     quillManageVacuum; 	//!< user defined flag that determines whether quill should perform vacuum or not
};

#endif /* _JOBQUEUEDBMANAGER_H_ */
