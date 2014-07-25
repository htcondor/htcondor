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


#include "condor_common.h"

//for the config_fill_ad call
#include "condor_config.h"

//for the ATTR_*** variables stuff
#include "condor_attributes.h"

#include "condor_io.h"
#include "get_daemon_name.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <time.h>

#include "jobqueuedbmanager.h"
#include "ClassAdLogProber.h"
#include "ClassAdLogParser.h"
#include "jobqueuedatabase.h"
#include "pgsqldatabase.h"
#include "jobqueuecollection.h"

#include "condor_ttdb.h"
#include "dbms_utils.h"

//! constructor
JobQueueDBManager::JobQueueDBManager()
{
	DBObj = NULL;
	caLogParser = NULL;
	dt = T_PGSQL;
	jobQueueDBConn = NULL;
	jobQueueDBIpAddress = NULL;
	jobQueueDBName = NULL;
	jobQueueDBUser = NULL;
	jobQueueLogFile = NULL;
	prober = NULL;
	scheddbirthdate = 0;
	scheddname = NULL;
	xactState = NOT_IN_XACT;
}

//! destructor
JobQueueDBManager::~JobQueueDBManager()
{
	if (prober != NULL) {
		delete prober;
	}
	if (caLogParser != NULL) {
		delete caLogParser;
	}
	if (DBObj != NULL) {
			// the destructor will disconnect the database
		delete DBObj;
	}
		// release strings
	if (jobQueueLogFile != NULL) {
		free(jobQueueLogFile);
	}
	if (jobQueueDBIpAddress != NULL) {
		free(jobQueueDBIpAddress);
	}
	if (jobQueueDBName != NULL) {
		free(jobQueueDBName);
	}
	if (jobQueueDBUser != NULL) {
		free(jobQueueDBUser);
	}
	if (jobQueueDBConn != NULL) {
		free(jobQueueDBConn);
	}
	if (scheddname != NULL) {
		free(scheddname);
	}
}
 
void
JobQueueDBManager::config(bool reconfig) 
{
	char *tmp;
	MyString sql_str;
	int bndcnt = 0;
	const char *data_arr[3];
	QuillAttrDataType   data_typ[3];

	if (param_boolean("QUILL_ENABLED", false) == false) {
		EXCEPT("Quill++ is currently disabled. Please set QUILL_ENABLED to "
			   "TRUE if you want this functionality and read the manual "
			   "about this feature since it requires other attributes to be "
			   "set properly.");
	}

		//bail out if no SPOOL variable is defined since its used to 
		//figure out the location of the job_queue.log file
	char *spool = param("SPOOL");
	if(!spool) {
		EXCEPT("No SPOOL variable found in config file\n");
	}
  
	jobQueueLogFile = (char *) malloc(_POSIX_PATH_MAX * sizeof(char));
	snprintf(jobQueueLogFile,_POSIX_PATH_MAX * sizeof(char), 
			 "%s/job_queue.log", spool);

		/*
		  Here we try to read the database parameters in config
		  the db ip address format is <ipaddress:port> 
		*/
	dt = getConfigDBType();

	jobQueueDBIpAddress = param("QUILL_DB_IP_ADDR");

	jobQueueDBName = param("QUILL_DB_NAME");

	jobQueueDBUser = param("QUILL_DB_USER");
  	
	jobQueueDBConn = getDBConnStr(jobQueueDBIpAddress,
								  jobQueueDBName,
								  jobQueueDBUser,
								  spool);
								  
	dprintf(D_ALWAYS, "Using Job Queue File %s\n", jobQueueLogFile);

	dprintf(D_ALWAYS, "Using Database Type = Postgres\n");
	dprintf(D_ALWAYS, "Using Database IpAddress = %s\n", 
			jobQueueDBIpAddress?jobQueueDBIpAddress:"");
	dprintf(D_ALWAYS, "Using Database Name = %s\n", 
			jobQueueDBName?jobQueueDBName:"");
	dprintf(D_ALWAYS, "Using Database User = %s\n", 
			jobQueueDBUser?jobQueueDBUser:"");

	if(spool) {
		free(spool);
		spool = NULL;
	}

		// this function is also called when condor_reconfig is issued
		// and so we dont want to recreate all essential objects
	if(!reconfig) {
		prober = new ClassAdLogProber();
		caLogParser = new ClassAdLogParser();

		switch (dt) {				
		case T_PGSQL:
			DBObj = new PGSQLDatabase(jobQueueDBConn);
			break;
		default:
			break;;
		}

		xactState = NOT_IN_XACT;

		QuillErrCode ret_st;

		ret_st = DBObj->connectDB();
		if (ret_st == QUILL_FAILURE) {
			displayErrorMsg("config: unable to connect to DB--- ERROR");
			EXCEPT("config: unable to connect to DB\n");
		}

			/* the following will also throw an exception if the schema 
			   version is not correct */
		DBObj->assertSchemaVersion();
		
		tmp = param( "SCHEDD_NAME" );
		if( tmp ) {
			scheddname = build_valid_daemon_name( tmp );
			dprintf(D_FULLDEBUG, "scheddname %s built from param value %s\n", 
					scheddname, tmp);
			free(tmp);

		} else {
			scheddname = default_daemon_name();
			dprintf(D_FULLDEBUG, "scheddname built from default daemon name: %s\n", scheddname);
		}

		{
				/* create an entry in jobqueuepollinginfo if this schedd is the 
				 * first time being logged to database
				 */
			sql_str.formatstr("INSERT INTO jobqueuepollinginfo (scheddname, last_file_mtime, last_file_size) SELECT '%s', 0, 0 FROM dummy_single_row_table WHERE NOT EXISTS (SELECT * FROM jobqueuepollinginfo WHERE scheddname = '%s')", scheddname, scheddname);
		
			ret_st = DBObj->execCommand(sql_str.Value());
			if (ret_st == QUILL_FAILURE) {
				dprintf(D_ALWAYS, "Insert JobQueuePollInfo --- ERROR [SQL] %s\n", 
						sql_str.Value());
			}			
		}

		{
				/* create an entry in currency table if this schedd is the first
				 * time being logged to database 
				 */
			sql_str.formatstr("INSERT INTO currencies (datasource) SELECT '%s' FROM dummy_single_row_table WHERE NOT EXISTS (SELECT * FROM currencies WHERE datasource = '%s')", scheddname, scheddname);

			ret_st = DBObj->execCommand(sql_str.Value());
			if (ret_st == QUILL_FAILURE) {
				dprintf(D_ALWAYS, "Insert Currency --- ERROR [SQL] %s\n", sql_str.Value());
			}
		}
		
		ret_st = DBObj->commitTransaction();
		if (ret_st == QUILL_FAILURE) {
			dprintf(D_ALWAYS, "Commit transaction failed in JobQueueDBManager::config\n");
		}

		if (param_boolean("QUILL_MAINTAIN_DB_CONN", true) == false) {
			ret_st = DBObj->disconnectDB();
			if (ret_st == QUILL_FAILURE) {
				dprintf(D_ALWAYS, "JobQueueDBManager:config: unable to disconnect database --- ERROR\n");
			}	
		}
	}

		//this function assumes that certain members have been initialized
		// (specifically prober and caLogParser) and so the order is important.
	setJobQueueFileName(jobQueueLogFile);
}

//! set the path to the job queue
void
JobQueueDBManager::setJobQueueFileName(const char* jqName)
{
	prober->setJobQueueName(jqName);
	caLogParser->setJobQueueName(jqName);
}

//! get the parser
ClassAdLogParser* 
JobQueueDBManager::getClassAdLogParser() 
{
	return caLogParser;
}

//! maintain the job queue 
QuillErrCode
JobQueueDBManager::maintain()
{	
	QuillErrCode st, ret_st; 
	FileOpErrCode fst;
	ProbeResultType probe_st;	
	struct stat fstat;

		// check if the job queue log exists, if not just skip polling
	if ((stat(caLogParser->getJobQueueName(), &fstat) == -1)) {
		if (errno == ENOENT) {
			return QUILL_SUCCESS;
		} else {
				// otherwise there is an error accessing the log
			return QUILL_FAILURE;
		}
		
	}

	st = getJQPollingInfo(); // get the last polling information

		//if we are unable to get to the polling info file, then either the 
		//postgres server is down or the database is deleted.
	if(st == QUILL_FAILURE) {
		return QUILL_FAILURE;
	}

	fst = caLogParser->openFile();
	if(fst == FILE_OPEN_ERROR) {
		return QUILL_FAILURE;
	}
	
		// polling
	probe_st = prober->probe(caLogParser->getCurCALogEntry(), 
							 caLogParser->getFilePointer());

	// no harm in doing this every time - if someone shutdown and reset
	// the schedd, we want to be sure to have the latest verison (we could
	// check for it in either the INIT_QUILL or COMPRESSED state too)
	scheddbirthdate = prober->getCurProbedCreationTime();

		// {init|add}JobQueueDB processes the  Log and stores probing
		// information into DB documentation for how do we determine 
		// the correct state is in the Prober->probe method
	switch(probe_st) {
	case INIT_QUILL:
		dprintf(D_ALWAYS, "JOB QUEUE POLLING RESULT: INIT\n");
		ret_st = initJobQueueTables();
		break;
	case ADDITION:
		dprintf(D_ALWAYS, "JOB QUEUE POLLING RESULT: ADDED\n");
		ret_st = addJobQueueTables();
		break;
	case COMPRESSED:
		dprintf(D_ALWAYS, "JOB QUEUE POLLING RESULT: COMPRESSED\n");
		ret_st = initJobQueueTables();
		break;
	case PROBE_ERROR:
		dprintf(D_ALWAYS, "JOB QUEUE POLLING RESULT: ERROR\n");
		ret_st = initJobQueueTables();
		break;
	case NO_CHANGE:
		dprintf(D_ALWAYS, "JOB QUEUE POLLING RESULT: NO CHANGE\n");	
		ret_st = QUILL_SUCCESS;
		break;
	default:
		dprintf(D_ALWAYS, "ERROR HAPPENED DURING JOB QUEUE POLLING\n");
		ret_st = QUILL_FAILURE;
	}

	fst = caLogParser->closeFile();
	return ret_st;
}

/*! delete the job queue related tables
 *  \return the result status
 *			1: Success
 *			0: Fail	(SQL execution fail)
 */	
QuillErrCode
JobQueueDBManager::cleanupJobQueueTables()
{
	const int	sqlNum = 4;
	int		i;
	MyString sql_str[sqlNum];
	
		// we only delete job queue related information.
	sql_str[0].formatstr(
			"DELETE FROM clusterads_horizontal WHERE scheddname = '%s'", 
			 scheddname);
	sql_str[1].formatstr(
			"DELETE FROM clusterads_vertical WHERE scheddname = '%s'", 
			 scheddname);
	sql_str[2].formatstr(
			"DELETE FROM procads_horizontal WHERE scheddname = '%s'", 
			 scheddname);
	sql_str[3].formatstr(
			"DELETE FROM procads_vertical WHERE scheddname = '%s'", 
			 scheddname);

	for (i = 0; i < sqlNum; i++) {
		if (DBObj->execCommand(sql_str[i].Value()) == QUILL_FAILURE) {
			displayErrorMsg("Clean UP ALL Data --- ERROR");
			
			return QUILL_FAILURE;
		}
	}

	return QUILL_SUCCESS;
}

/*! build the job queue collection from job_queue.log file
 */
QuillErrCode
JobQueueDBManager::buildJobQueue(JobQueueCollection *jobQueue)
{
	int		op_type;
	FileOpErrCode st;

	st = caLogParser->readLogEntry(op_type);
	assert(st != FILE_FATAL_ERROR);
	if(st == FILE_OPEN_ERROR) {
		return QUILL_FAILURE;
	}

	while (st == FILE_READ_SUCCESS) {
		if (processLogEntry(op_type, jobQueue) == QUILL_FAILURE) {
				// process each ClassAd Log Entry
			return QUILL_FAILURE;
		}
		st = caLogParser->readLogEntry(op_type);
		assert(st != FILE_FATAL_ERROR);
	}

	return QUILL_SUCCESS;
}

/*! load job ads in a job queue collection into DB
 */
QuillErrCode
JobQueueDBManager::loadJobQueue(JobQueueCollection *jobQueue)
{
	QuillErrCode errStatus;

		//
		// load cluster ads into ClusterAds_Horizontal and 
		// ClusterAds_Vertical tables
		//
	jobQueue->initAllJobAdsIteration();

	while(jobQueue->loadNextClusterAd(errStatus)) {
		if (errStatus == QUILL_FAILURE) 
			return errStatus;
	}
	
		//
		// load Proc Ads into ProcAds_Horizontal and 
		// ProcAds_Vertical tables
		//
	jobQueue->initAllJobAdsIteration();
	while(jobQueue->loadNextProcAd(errStatus)) {
		if (errStatus == QUILL_FAILURE) 
			return errStatus;
	}
			
	return QUILL_SUCCESS;
}


/*! build an in-memory list of jobs by reading entries in job_queue.log 
 *  file and dump them into the database.  There are various in-memory lists
 *  for live jobs, i.e. jobs not completed, and historical jobs.  All of them
 *  are written to their appropriate tables
*/
QuillErrCode
JobQueueDBManager::buildAndWriteJobQueue()
{
		//this is an array of linked lists, which keep growing, so 
		//although the array is of fixed size=2000, the ever-growing
		//linked lists can accomodate any number of jobs
	JobQueueCollection *jobQueue = new JobQueueCollection(2000);

	jobQueue->setDBObj(DBObj);
	jobQueue->setDBtype(dt);

	dprintf(D_FULLDEBUG, "Bulkloading 1st Phase: Parsing a job_queue.log "
			"file and building job collection!\n");

		//read from the beginning
	caLogParser->setNextOffset(0);

		//construct the in memory job queue and history
	if (buildJobQueue(jobQueue) == QUILL_FAILURE) {
		delete jobQueue;
		return QUILL_FAILURE;
	}

	dprintf(D_FULLDEBUG, "Bulkloading 2nd Phase: Loading jobs into DBMS!\n");

		// For job queue tables, send COPY string to RDBMS: 
	if (loadJobQueue(jobQueue) == QUILL_FAILURE) {
		delete jobQueue;
		return QUILL_FAILURE;
	}

		//  END OF SECOND PHASE OF BULKLOADING
	delete jobQueue;
	return QUILL_SUCCESS;
}


/*! incrementally read and process log entries from file 
 */
QuillErrCode
JobQueueDBManager::readAndWriteLogEntries()
{
	int op_type = 0;
	FileOpErrCode st;

		// turn off sequential scan so that the incremental update always use 
		// index regardless whether the statistics are correct or not.
		// this feature is postgres specific, comment it out for now.
		/* 
	if (DBObj->execCommand("set enable_seqscan=false;") == QUILL_FAILURE) {
		displayErrorMsg("Turning off seq scan --- ERROR");
		return QUILL_FAILURE; 
	}
		*/

	st = caLogParser->readLogEntry(op_type);
	assert(st != FILE_FATAL_ERROR);

		// Process ClassAd Log Entry
	while (st == FILE_READ_SUCCESS) {
		if (processLogEntry(op_type) == QUILL_FAILURE) {
			return QUILL_FAILURE; 
		}
		st = caLogParser->readLogEntry(op_type);
		assert(st != FILE_FATAL_ERROR);
	}

		// turn on sequential scan again
		/*
	if (DBObj->execCommand("set enable_seqscan=true;") == QUILL_FAILURE) {
		displayErrorMsg("Turning on seq scan --- ERROR");
		return QUILL_FAILURE;
	}
		*/

	return QUILL_SUCCESS;
}

/*! process only DELTA, i.e. entries of the job queue log which were
 *  added since the last offset read/set by quill
 */
QuillErrCode
JobQueueDBManager::addJobQueueTables()
{
	QuillErrCode st;

		//we dont set transactions explicitly.  they are set by the 
		//schedd via the 'begin transaction' log entry

	caLogParser->setNextOffset();

	st = readAndWriteLogEntries();

		// Store a polling information into DB
	if (st == QUILL_SUCCESS) {
		setJQPollingInfo();
	} else {
		// a transaction might have been began, need to be rolled back
		// otherwise subsequent SQLs will continue to fail
		DBObj->rollbackTransaction();
		xactState = NOT_IN_XACT;
	}

	return st;
}

/*! purge all job queue rows and process the entire job_queue.log file
 *  also vacuum the job queue tables
 */
QuillErrCode
JobQueueDBManager::initJobQueueTables()
{
	QuillErrCode st;

	st = DBObj->beginTransaction();

	if(st == QUILL_FAILURE) {
		displayErrorMsg("JobQueueDBManager::initJobQueueTables: unable to begin a transaction --- ERROR");
		return QUILL_FAILURE; 
	}
	
	st = cleanupJobQueueTables(); // delete all job queue tables

	if(st == QUILL_FAILURE) {
		displayErrorMsg("JobQueueDBManager::initJobQueueTables: unable to clean up job queue tables --- ERROR");
		return QUILL_FAILURE; 
	}

	st = buildAndWriteJobQueue(); // bulk load job queue log

		// Store polling information in database
	if (st == QUILL_SUCCESS) {
		setJQPollingInfo();

			// VACUUM should be called outside XACT
			// So, Commit XACT shouble be invoked beforehand.
		DBObj->commitTransaction(); // end XACT
		xactState = NOT_IN_XACT;
	} else {
		DBObj->rollbackTransaction();
		xactState = NOT_IN_XACT;
	}

	return st;	
}


/*! handle a log Entry: work with a job queue collection.
 *  (not with DBMS directry)
 */
QuillErrCode
JobQueueDBManager::processLogEntry(int op_type, JobQueueCollection* jobQueue)
{
	char *key, *mytype, *targettype, *name, *value;
	key = mytype = targettype = name = value = NULL;
	QuillErrCode st = QUILL_SUCCESS;

	int job_id_type;
	char cid[512];
	char pid[512];

		// REMEMBER:
		//	each get*ClassAdBody() funtion allocates the memory of 
		// 	parameters. Therefore, they all must be deallocated here,
		// and they are at the end of the routine
	switch(op_type) {
	case CondorLogOp_NewClassAd: {
		if (caLogParser->getNewClassAdBody(key, mytype, targettype) == QUILL_FAILURE)
			{
				st = QUILL_FAILURE;
				break;
			}
		job_id_type = getProcClusterIds(key, cid, pid);
		ClassAd* ad = new ClassAd();
		SetMyTypeName(*ad, "Job");
		SetTargetTypeName(*ad, "Machine");

		switch(job_id_type) {
		case IS_CLUSTER_ID:
			jobQueue->insertClusterAd(cid, ad);
			break;

		case IS_PROC_ID:
			jobQueue->insertProcAd(cid, pid, ad);
			break;

		default:
			dprintf(D_ALWAYS, "[QUILL++] New ClassAd --- ERROR\n");
			st = QUILL_FAILURE; 
			break;
		}

		break;
	}
	case CondorLogOp_DestroyClassAd: {
		if (caLogParser->getDestroyClassAdBody(key) == QUILL_FAILURE) {
			st = QUILL_FAILURE; 
			break;		
		}

		job_id_type = getProcClusterIds(key, cid, pid);

		switch(job_id_type) {
		case IS_CLUSTER_ID:
			jobQueue->removeClusterAd(cid);
			break;

		case IS_PROC_ID: {
			ClassAd *clusterad = jobQueue->find(cid);
			ClassAd *procad = jobQueue->find(cid,pid);
			if(!clusterad || !procad) {
			    dprintf(D_ALWAYS, 
						"[QUILL++] Destroy ClassAd --- Cannot find clusterad "
						"or procad in memory job queue");
				st = QUILL_FAILURE; 
				break;
			}

			jobQueue->removeProcAd(cid, pid);
			
			break;
		}
		default:
			dprintf(D_ALWAYS, "[QUILL++] Destroy ClassAd --- ERROR\n");
			st = QUILL_FAILURE; 
			break;
		}

		break;
	}
	case CondorLogOp_SetAttribute: {	
		if (caLogParser->getSetAttributeBody(key, name, value) == QUILL_FAILURE) {
			st = QUILL_FAILURE; 
			break;
		}
						
		job_id_type = getProcClusterIds(key, cid, pid);

		char tmp[512];

		snprintf(tmp, 512, "%s = %s", name, value);

		switch (job_id_type) {
		case IS_CLUSTER_ID: {
			ClassAd* ad = jobQueue->findClusterAd(cid);
			if (ad != NULL) {
				ad->Insert(tmp);
			}
			else {
				dprintf(D_ALWAYS, "[QUILL++] ERROR: There is no such Cluster Ad[%s]\n", cid);
			}
			break;
		}
		case IS_PROC_ID: {
			ClassAd* ad = jobQueue->findProcAd(cid, pid);
			if (ad != NULL) {
				ad->Insert(tmp);
			}
			else {
				dprintf(D_ALWAYS, "[QUILL++] ERROR: There is no such Proc Ad[%s.%s]\n", cid, pid);
			}
			break;
		}
		default:
			dprintf(D_ALWAYS, "[QUILL++] Set Attribute --- ERROR\n");
			st = QUILL_FAILURE; 
			break;
		}
		break;
	}
	case CondorLogOp_DeleteAttribute: {
		if (caLogParser->getDeleteAttributeBody(key, name) == QUILL_FAILURE) {
			st = QUILL_FAILURE; 
			break;
		}

		job_id_type = getProcClusterIds(key, cid, pid);

		switch(job_id_type) {
		case IS_CLUSTER_ID: {
			ClassAd* ad = jobQueue->findClusterAd(cid);
			ad->Delete(name);
			break;
		}
		case IS_PROC_ID: {
			ClassAd* ad = jobQueue->findProcAd(cid, pid);
			ad->Delete(name);
			break;
		}
		default:
			dprintf(D_ALWAYS, "[QUILL++] Delete Attribute --- ERROR\n");
			st = QUILL_FAILURE; 
		}
		break;
	}
	case CondorLogOp_BeginTransaction:
		st = processBeginTransaction(true);
		break;
	case CondorLogOp_EndTransaction:
		st = processEndTransaction(true);
		break;
			// skip the log historical sequence number command
	case CondorLogOp_LogHistoricalSequenceNumber:
		break;
	default:
		printf("[QUILL++] Unsupported Job Queue Command\n");
		st = QUILL_FAILURE; 
		break;
	}

		// pointers are release
	if (key != NULL) free(key);
	if (mytype != NULL) free(mytype);
	if (targettype != NULL) free(targettype);
	if (name != NULL) free(name);
	if (value != NULL) free(value);
	return st;
}

/*! handle ClassAd Log Entry
 *
 * is a wrapper over all the processXXX functions
 * in this and all the processXXX routines
 */
QuillErrCode
JobQueueDBManager::processLogEntry(int op_type)
{
	char *key, *mytype, *targettype, *name, *value;
	key = mytype = targettype = name = value = NULL;
	QuillErrCode	st = QUILL_SUCCESS;
	ParserErrCode	pst = PARSER_SUCCESS;

		// REMEMBER:
		//	each get*ClassAdBody() funtion allocates the memory of 
		// 	parameters. Therefore, they all must be deallocated here,
		//  and they are at the end of the routine

	switch(op_type) {
	case CondorLogOp_LogHistoricalSequenceNumber: 
		break;
	case CondorLogOp_NewClassAd:
		pst = caLogParser->getNewClassAdBody(key, mytype, targettype);
		if (pst == PARSER_FAILURE) {
			st = QUILL_FAILURE;
			break;
		}
		st = processNewClassAd(key, mytype, targettype);
		break;
	case CondorLogOp_DestroyClassAd:
		pst = caLogParser->getDestroyClassAdBody(key);
		if (pst == PARSER_FAILURE) {
			st = QUILL_FAILURE;
			break;
		}		
		st = processDestroyClassAd(key);
		break;
	case CondorLogOp_SetAttribute:
		pst = caLogParser->getSetAttributeBody(key, name, value);
		if (pst == PARSER_FAILURE) {
			st = QUILL_FAILURE;
			break;
		}
		st = processSetAttribute(key, name, value);
		break;
	case CondorLogOp_DeleteAttribute:
		pst = caLogParser->getDeleteAttributeBody(key, name);
		if (pst == PARSER_FAILURE) {
			st = QUILL_FAILURE;
			break;
		}
		st = processDeleteAttribute(key, name);
		break;
	case CondorLogOp_BeginTransaction:
		st = processBeginTransaction(false);
		break;
	case CondorLogOp_EndTransaction:
		st = processEndTransaction(false);
		break;
	default:
		dprintf(D_ALWAYS, "[QUILL++] Unsupported Job Queue Command [%d]\n", 
				op_type);
		st = QUILL_FAILURE;
		break;
	}

		// pointers are release
	if (key != NULL) free(key);
	if (mytype != NULL) free(mytype);
	if (targettype != NULL) free(targettype);
	if (name != NULL) free(name);
	if (value != NULL) free(value);

	return st;
}

/*! display a verbose error message
 */
void
JobQueueDBManager::displayErrorMsg(const char* errmsg)
{
	dprintf(D_ALWAYS, "[QUILL++] %s\n", errmsg);
}

/*! separate a key into Cluster Id and Proc Id 
 *  \return key type 
 *			1: when it is a cluster id
 *			2: when it is a proc id
 * 			0: it fails
 *
 *	\warning The memories of cid and pid should be allocated in advance.
 */
JobIdType
JobQueueDBManager::getProcClusterIds(const char* key, char* cid, char* pid)
{
	int key_len, i;
	long iCid;
	const char*	pid_in_key;

	if (key == NULL) {
		return IS_UNKNOWN_ID;
	}

	key_len = strlen(key);

	for (i = 0; i < key_len; i++) {
		if(key[i]  != '.')	
			cid[i]=key[i];
		else {
			cid[i] = '\0';
			break;
		}
	}

		// In case the key doesn't include "."
	if (i == key_len) {
		return IS_UNKNOWN_ID; // Error
	}

		// These two lines are for removing a leading zero.
	iCid = atol(cid);
	sprintf(cid,"%ld", iCid);


	pid_in_key = (key + (i + 1));
	strcpy(pid, pid_in_key);

	if (atol(pid) == -1) {// Cluster ID
		return IS_CLUSTER_ID;	
	}

	return IS_PROC_ID; // Proc ID
}

/*! process NewClassAd command, working with DBMS
 *  \param key key
 *  \param mytype mytype
 *  \param ttype targettype
 *  \return the result status
 *			0: error
 *			1: success
 */
QuillErrCode
JobQueueDBManager::processNewClassAd(char* key, 
									 char* mytype, 
									 char* ttype)
{
	MyString sql_str;
	char  cid[512];
	char  pid[512];
	int   job_id_type;
	const char *data_arr[4];
	QuillAttrDataType  data_typ[4];

		// It could be ProcAd or ClusterAd
		// So need to check
	job_id_type = getProcClusterIds(key, cid, pid);

	switch(job_id_type) {
	case IS_CLUSTER_ID:
		sql_str.formatstr("INSERT INTO ClusterAds_Horizontal (scheddname, cluster_id) VALUES ('%s', '%s')", scheddname, cid);
		break;
	case IS_PROC_ID:
		sql_str.formatstr("INSERT INTO ProcAds_Horizontal (scheddname, cluster_id, proc_id) VALUES ('%s', '%s', '%s')", scheddname, cid, pid);
		break;
	case IS_UNKNOWN_ID:
		dprintf(D_ALWAYS, "New ClassAd Processing --- ERROR\n");
		return QUILL_FAILURE; // return a error code, 0
		break;
	}

	{
		if (DBObj->execCommand(sql_str.Value()) == QUILL_FAILURE) {
			displayErrorMsg("New ClassAd Processing --- ERROR");
			return QUILL_FAILURE;
		}
	}

	return QUILL_SUCCESS;
}

/*! process DestroyClassAd command, working with DBMS
 *  Also responsible for writing history records
 * 
 *  Note: Currently we can obtain a 'fairly' accurate view of the history.
 *  'fairly' because 
 *  a) we do miss attributes which the schedd puts in the 
 *  history file itself, and the job_queue.log file is unaware of these
 *  attributes.  Since our history capturing scheme is via the job_queue.log
 *  file, we miss these attributes.  On the UWisc pool, this misses 3 
 *  attributes: LastMatchTime, NumJobMatches, and WantMatchDiagnostics
 *
 *  b) Also, some jobs do not get into the history tables  in the following 
 *  rare case:
 *  Quill is unoperational for whatever reason, jobs complete execution and
 *  get deleted from the queue, the job queue log gets truncated, and quill
 *  wakes back up.  Since the job queue log is truncated, quill is blissfully
 *  unaware of the jobs that finished while it was not operational.
 *  A possible fix for this is that the schedd should not truncate job logs
 *  when Quill is unoperational, however, we want Quill to be as independent
 *  as possible, and as such, for now, we live with this rare anomaly.
 * 
 *  \param key key
 *  \return the result status
 *			0: error
 *			1: success
 */
QuillErrCode
JobQueueDBManager::processDestroyClassAd(char* key)
{
	MyString sql_str1; 
	MyString sql_str2; 
	char cid[100];
	char pid[100];
	int  job_id_type;
	const char *data_arr[4];
	QuillAttrDataType  data_typ[4];

		// It could be ProcAd or ClusterAd
		// So need to check
	job_id_type = getProcClusterIds(key, cid, pid);
  
	switch(job_id_type) {
	case IS_CLUSTER_ID:	// ClusterAds
		sql_str1.formatstr("DELETE FROM ClusterAds_Horizontal WHERE scheddname = '%s' and cluster_id = %s", scheddname, cid);
    
		sql_str2.formatstr("DELETE FROM ClusterAds_Vertical WHERE scheddname = '%s' and cluster_id = %s", scheddname, cid);
		break;
	case IS_PROC_ID:
				/* generate SQL to remove the job from job tables */
		sql_str1.formatstr("DELETE FROM ProcAds_horizontal WHERE scheddname = '%s' and cluster_id = %s AND proc_id = %s", 
						 scheddname, cid, pid);
		sql_str2.formatstr("DELETE FROM ProcAds_vertical WHERE scheddname = '%s' and cluster_id = %s AND proc_id = %s", 
						 scheddname, cid, pid);
		break;
	case IS_UNKNOWN_ID:
		dprintf(D_ALWAYS, "[QUILL++] Destroy ClassAd --- ERROR\n");
		return QUILL_FAILURE; // return a error code, 0
		break;
	}
  
	{
		if (DBObj->execCommand(sql_str1.Value()) == QUILL_FAILURE) {
			displayErrorMsg("Destroy ClassAd Processing --- ERROR");
			return QUILL_FAILURE; // return a error code, 0
		}

		if (DBObj->execCommand(sql_str2.Value()) == QUILL_FAILURE) {
			displayErrorMsg("Destroy ClassAd Processing --- ERROR");
			return QUILL_FAILURE; // return a error code, 0
		}
	}

	return QUILL_SUCCESS;
}

/*! process SetAttribute command, working with DBMS
 *  \param key key
 *  \param name attribute name
 *  \param value attribute value
 *  \return the result status
 *			0: error
 *			1: success
 *	Note:
 *	Because this is not just update, but set. So, we need to delete and insert
 *  it.  We twiddled with an alternative way to do it (using NOT EXISTS) but
 *  found out that DELETE/INSERT works as efficiently.  the old sql is kept
 *  around in case :)
 */
QuillErrCode
JobQueueDBManager::processSetAttribute(char* key, 
									   char* name, 
									   char* value)
{
	MyString sql_str_del_in;
	MyString sql_str2 = "";
	char cid[512];
	char pid[512];
	int  job_id_type;
		//int		ret_st;
	MyString newvalue;

	const char *data_arr1[6];
	QuillAttrDataType  data_typ1[6];

	const char *data_arr2[6];
	QuillAttrDataType  data_typ2[6];

	QuillAttrDataType  attr_type;

	MyString ts_expr_val;

		// It could be ProcAd or ClusterAd
		// So need to check
	job_id_type = getProcClusterIds(key, cid, pid);
  
	switch(job_id_type) {
	case IS_CLUSTER_ID:
		if(isHorizontalClusterAttribute(name, attr_type)) {

			if (attr_type == CONDOR_TT_TYPE_TIMESTAMP) {
				time_t clock;				
				clock = atoi(value);				

				{
					MyString ts_expr;

					ts_expr = condor_ttdb_buildts(&clock, dt);

					if (ts_expr.IsEmpty()) {
						dprintf(D_ALWAYS, "ERROR: Timestamp expression not built in JobQueueDBManager::processSetAttribute\n");
						return QUILL_FAILURE;
					}

					sql_str_del_in.formatstr(
										   "UPDATE ClusterAds_Horizontal SET %s = (%s) WHERE scheddname = '%s' and cluster_id = '%s'", name, ts_expr.Value(), scheddname, cid);
				}
			} else {
					// strip double quote for string type values
				if ((attr_type == CONDOR_TT_TYPE_STRING || 
					 attr_type == CONDOR_TT_TYPE_CLOB) &&
					!stripdoublequotes(value)) {
					dprintf(D_ALWAYS, "ERROR: string constant not double quoted for attribute %s in JobQueueDBManager::ProcessSetAttribute\n", name);
				}
				newvalue = condor_ttdb_fillEscapeCharacters(value, dt);
					// escape single quote within the value

				{
					sql_str_del_in.formatstr("UPDATE ClusterAds_Horizontal SET %s = '%s' WHERE scheddname = '%s' and cluster_id = '%s'", name, newvalue.Value(), scheddname, cid);
				}
			}
		} else {
			newvalue = condor_ttdb_fillEscapeCharacters(value, dt);

			{
				sql_str_del_in.formatstr("DELETE FROM ClusterAds_Vertical WHERE scheddname = '%s' and cluster_id = '%s' AND attr = '%s'", scheddname, cid, name);
			}

			{
				sql_str2.formatstr("INSERT INTO ClusterAds_Vertical (scheddname, cluster_id, attr, val) VALUES ('%s', '%s', '%s', '%s')", scheddname, cid, name, newvalue.Value());
			}
		}

		break;
	case IS_PROC_ID:
		if(isHorizontalProcAttribute(name, attr_type)) {

			if (attr_type == CONDOR_TT_TYPE_TIMESTAMP) {
				time_t clock;
				clock = atoi(value);

				{
					MyString ts_expr;				
					ts_expr = condor_ttdb_buildts(&clock, dt);

					if (ts_expr.IsEmpty()) {
						dprintf(D_ALWAYS, "ERROR: Timestamp expression not built in JobQueueDBManager::processSetAttribute\n");
						return QUILL_FAILURE;
					}
				
					sql_str_del_in.formatstr(
									   "UPDATE ProcAds_Horizontal SET %s = (%s) WHERE scheddname = '%s' and cluster_id = '%s' and proc_id = '%s'", name, ts_expr.Value(), scheddname, cid, pid);
				}
			} else {
					// strip double quote for string type values
				if ((attr_type == CONDOR_TT_TYPE_STRING || 
					 attr_type == CONDOR_TT_TYPE_CLOB) &&
					!stripdoublequotes(value)) {
					dprintf(D_ALWAYS, "ERROR: string constant not double quoted for attribute %s in JobQueueDBManager::ProcessSetAttribute\n", name);
				}
				
				newvalue = condor_ttdb_fillEscapeCharacters(value, dt);

				{
					sql_str_del_in.formatstr("UPDATE ProcAds_Horizontal SET %s = '%s' WHERE scheddname = '%s' and cluster_id = '%s' and proc_id = '%s'", name, newvalue.Value(), scheddname, cid, pid);
				}
			}
		} else {
			newvalue = condor_ttdb_fillEscapeCharacters(value, dt);

			{
				sql_str_del_in.formatstr("DELETE FROM ProcAds_Vertical WHERE scheddname = '%s' and cluster_id = '%s' AND proc_id = '%s' AND attr = '%s'", scheddname, cid, pid, name);
			}

			{
				sql_str2.formatstr("INSERT INTO ProcAds_Vertical (scheddname, cluster_id, proc_id, attr, val) VALUES ('%s', '%s', '%s', '%s', '%s')", scheddname, cid, pid, name, newvalue.Value());	
			}
		}
		
		break;
	case IS_UNKNOWN_ID:
		dprintf(D_ALWAYS, "Set Attribute Processing --- ERROR\n");
		return QUILL_FAILURE;
		break;
	}
  
	QuillErrCode ret_st;

	{
		ret_st = DBObj->execCommand(sql_str_del_in.Value());
	}

	if (ret_st == QUILL_FAILURE) {
		dprintf(D_ALWAYS, "Set Attribute --- Error [SQL] %s\n", 
				sql_str_del_in.Value());
		displayErrorMsg("Set Attribute --- ERROR");      

		return QUILL_FAILURE;
	}

	if (!sql_str2.IsEmpty()) {
		
		{
			ret_st = DBObj->execCommand(sql_str2.Value());
		}
		
		if (ret_st == QUILL_FAILURE) {
			dprintf(D_ALWAYS, "Set Attribute --- Error [SQL] %s\n", 
					sql_str2.Value());
			displayErrorMsg("Set Attribute --- ERROR");      

			return QUILL_FAILURE;
		}
	}	

	return QUILL_SUCCESS;
}


/*! process DeleteAttribute command, working with DBMS
 *  \param key key
 *  \param name attribute name
 *  \return the result status
 *			0: error
 *			1: success
 */
QuillErrCode
JobQueueDBManager::processDeleteAttribute(char* key, 
										  char* name)
{	
	MyString sql_str = "";
	char cid[512];
	char pid[512];
	int  job_id_type;
	QuillErrCode  ret_st;
	QuillAttrDataType attr_type;

		// It could be ProcAd or ClusterAd
		// So need to check
	job_id_type = getProcClusterIds(key, cid, pid);

	switch(job_id_type) {
	case IS_CLUSTER_ID:
		if(isHorizontalClusterAttribute(name, attr_type)) {
			sql_str.formatstr(
					 "UPDATE ClusterAds_Horizontal SET %s = NULL WHERE scheddname = '%s' and cluster_id = '%s'", name, scheddname, cid);
		} else {
			sql_str.formatstr(
					 "DELETE FROM ClusterAds_Vertical WHERE scheddname = '%s' and cluster_id = '%s' AND attr = '%s'", scheddname, cid, name);			
		}

		break;
	case IS_PROC_ID:
		if(isHorizontalProcAttribute(name, attr_type)) {
			sql_str.formatstr(
					 "UPDATE ProcAds_Horizontal SET %s = NULL WHERE scheddname = '%s' and cluster_id = '%s' AND proc_id = '%s'", name, scheddname, cid, pid);
		} else {
			sql_str.formatstr(
					 "DELETE FROM ProcAds_Vertical WHERE scheddname = '%s' and cluster_id = '%s' AND proc_id = '%s' AND attr = '%s'", scheddname, cid, pid, name);
			
		}

		break;
	case IS_UNKNOWN_ID:
		dprintf(D_ALWAYS, "Delete Attribute Processing --- ERROR\n");
		return QUILL_FAILURE;
		break;
	}

	if (!sql_str.IsEmpty()) {
		ret_st = DBObj->execCommand(sql_str.Value());
		
		if (ret_st == QUILL_FAILURE) {
			dprintf(D_ALWAYS, "Delete Attribute --- ERROR, [SQL] %s\n",
					sql_str.Value());
			displayErrorMsg("Delete Attribute --- ERROR");

			return QUILL_FAILURE;
		}
	}

	return QUILL_SUCCESS;
}

/*! process BeginTransaction command
 *  \return the result status
 */
QuillErrCode
JobQueueDBManager::processBeginTransaction(bool exec_later)
{
	xactState = BEGIN_XACT;
	if (!exec_later) {
		if (DBObj->beginTransaction() == QUILL_FAILURE) 
			return QUILL_FAILURE;
	}			   				
	return QUILL_SUCCESS;
}

/*! process EndTransaction command
 *  \return the result status
 */
QuillErrCode
JobQueueDBManager::processEndTransaction(bool exec_later)
{
	xactState = COMMIT_XACT;
	if (!exec_later) {
		if (DBObj->commitTransaction() == QUILL_FAILURE) 
			return QUILL_FAILURE;			   			
	}
	return QUILL_SUCCESS;
}

//! initialize: currently check the DB schema
/*! \param initJQDB initialize DB?
 */
QuillErrCode
JobQueueDBManager::init()
{
	return QUILL_SUCCESS;
}

//! get Last Job Queue File Polling Information
QuillErrCode
JobQueueDBManager::getJQPollingInfo()
{
	long mtime;
	long size;
	ClassAdLogEntry* lcmd;
	MyString sql_str;
	int	ret_st;
	int num_result;

	lcmd = caLogParser->getCurCALogEntry();

	dprintf(D_FULLDEBUG, "Get JobQueue Polling Information\n");

	sql_str.formatstr("SELECT last_file_mtime, last_file_size, last_next_cmd_offset, last_cmd_offset, last_cmd_type, last_cmd_key, last_cmd_mytype, last_cmd_targettype, last_cmd_name, last_cmd_value from JobQueuePollingInfo where scheddname = '%s'", scheddname);

	ret_st = DBObj->execQuery(sql_str.Value(), num_result);
		
	if (ret_st == QUILL_FAILURE) {
		dprintf(D_ALWAYS, "Reading JobQueuePollInfo --- ERROR [SQL] %s\n", 
				sql_str.Value());
		displayErrorMsg("Reading JobQueuePollInfo --- ERROR");
		return QUILL_FAILURE;
	}
	else if (ret_st == QUILL_SUCCESS && num_result == 0) {
			// This case is a rare one since the jobqueuepollinginfo
			// table contains one tuple at all times 		
		displayErrorMsg("Reading JobQueuePollingInfo --- ERROR "
						  "No Rows Retrieved from JobQueuePollingInfo\n");
		DBObj->releaseQueryResult(); // release Query Result
		return QUILL_FAILURE;
	} 

	mtime = atoi(DBObj->getValue(0, 0)); // last_file_mtime
	size =  atoi(DBObj->getValue(0, 1)); // last_file_size

	prober->setLastModifiedTime(mtime);
	prober->setLastSize(size);

		// last_next_cmd_offset
	lcmd->next_offset = atoi(DBObj->getValue(0,2)); 
	lcmd->offset = atoi(DBObj->getValue(0,3)); // last_cmd_offset
	lcmd->op_type = atoi(DBObj->getValue(0,4)); // last_cmd_type
	
	if (lcmd->key) {
		free(lcmd->key);
	}

	if (lcmd->mytype) {
		free(lcmd->mytype);
	}

	if (lcmd->targettype) {
		free(lcmd->targettype);
	}

	if (lcmd->name) {
		free(lcmd->name);
	}

	if (lcmd->value) {
		free(lcmd->value);
	}

	lcmd->key = strdup(DBObj->getValue(0,5)); // last_cmd_key
	lcmd->mytype = strdup(DBObj->getValue(0,6)); // last_cmd_mytype
		// last_cmd_targettype
	lcmd->targettype = strdup(DBObj->getValue(0,7)); 
	lcmd->name = strdup(DBObj->getValue(0,8)); // last_cmd_name
	lcmd->value = strdup(DBObj->getValue(0,9)); // last_cmd_value
	
	DBObj->releaseQueryResult(); // release Query Result
										  // since it is no longer needed
	return QUILL_SUCCESS;	
}

void 
JobQueueDBManager::addJQPollingInfoSQL(char* dest, char* src_name, char* src_val)
{
	if (src_name != NULL && src_val != NULL) {
		strcat(dest, ", ");
		strcat(dest, src_name);
		strcat(dest, " = '");
		strcat(dest, src_val);
		strcat(dest, "'");
	}
}

//! set Current Job Queue File Polling Information
QuillErrCode
JobQueueDBManager::setJQPollingInfo()
{
	long mtime;
	long size;	
	ClassAdLogEntry* lcmd;
	char *sql_str, *tmp;
	int   len;
	QuillErrCode   ret_st;
	int            num_result=0;

	prober->incrementProbeInfo();
	mtime = prober->getLastModifiedTime();
	size = prober->getLastSize();
	lcmd = caLogParser->getCurCALogEntry();	
	
	len = 2048 + strlen(scheddname) + sizeof(lcmd->value);
	sql_str = (char *)malloc(len * sizeof(char));
	snprintf(sql_str, len,
			"UPDATE JobQueuePollingInfo SET last_file_mtime = %ld, last_file_size = %ld, last_next_cmd_offset = %ld, last_cmd_offset = %ld, last_cmd_type = %d", mtime, size, lcmd->next_offset, lcmd->offset, lcmd->op_type);
	
	addJQPollingInfoSQL(sql_str, "last_cmd_key", lcmd->key);
	addJQPollingInfoSQL(sql_str, "last_cmd_mytype", lcmd->mytype);
	addJQPollingInfoSQL(sql_str, "last_cmd_targettype", lcmd->targettype);
	addJQPollingInfoSQL(sql_str, "last_cmd_name", lcmd->name);
	addJQPollingInfoSQL(sql_str, "last_cmd_value", lcmd->value);
	
	len = 50+strlen(scheddname);
	tmp = (char *) malloc(len);
	snprintf(tmp, len, " WHERE scheddname = '%s'", scheddname);
	strcat(sql_str, tmp);
	
	ret_st = DBObj->execCommand(sql_str, num_result);
	
	if (ret_st == QUILL_FAILURE) {
		dprintf(D_ALWAYS, "Update JobQueuePollInfo --- ERROR [SQL] %s\n", 
				sql_str);
		displayErrorMsg("Update JobQueuePollInfo --- ERROR");
	}
	else if (ret_st == QUILL_SUCCESS && num_result == 0) {
			// This case is a rare one since the jobqueuepollinginfo
			// table contains one tuple at all times 

		dprintf(D_ALWAYS, "Update JobQueuePollInfo --- ERROR [SQL] %s\n", 
				sql_str);
		displayErrorMsg("Update JobQueuePollInfo --- ERROR");
		ret_st = QUILL_FAILURE;
	} 	
	
	free(sql_str);
	free(tmp);

	return ret_st;
}
