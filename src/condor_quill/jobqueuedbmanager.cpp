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
#include "condor_config.h"

//for the ATTR_*** variables stuff
#include "condor_attributes.h"

#include "condor_io.h"
#include "get_daemon_name.h"

#include "jobqueuedbmanager.h"
#include "quill_dbschema_def.h"
#include "classadlogentry.h"
#include "prober.h"
#include "classadlogparser.h"
#include "jobqueuedatabase.h"
#include "pgsqldatabase.h"
#include "requestservice.h"
#include "jobqueuecollection.h"


//! constructor
JobQueueDBManager::JobQueueDBManager()
{
	pollingTimeId = -1;
	purgeHistoryTimeId = -1;
	reindexTimeId = -1;

	totalSqlProcessed = 0;
	lastBatchSqlProcessed = 0;
	secsLastBatch = 0;
	isConnectedToDB = false;
	quillManageVacuum = false;

	ad = NULL;
	
		//the rest is done in config()
}

//! destructor
JobQueueDBManager::~JobQueueDBManager()
{
		// release Objects
	if (prober != NULL) {
		delete prober;
	}
	if (caLogParser != NULL) {
		delete caLogParser;
	}

	if (DBObj != NULL) {
		delete DBObj;
	}
		// release strings
	if (jobQueueLogFile != NULL) {
		free(jobQueueLogFile);
		jobQueueLogFile = NULL;
	}
	if (jobQueueDBIpAddress != NULL) {
		free(jobQueueDBIpAddress);
		jobQueueDBIpAddress = NULL;
	}
	if (jobQueueDBName != NULL) {
		free(jobQueueDBName);
		jobQueueDBName = NULL;
	}
	if (jobQueueDBConn != NULL) {
		free(jobQueueDBConn);
		jobQueueDBConn = NULL;
	}
	if (multi_sql_str != NULL) {
		free(multi_sql_str);
		multi_sql_str = NULL;
	}
}

//! Gets the writer password required by the quill 
//  daemon to access the database
char * 
JobQueueDBManager::getWritePassword(char *write_passwd_fname) {
	FILE *fp = NULL;
	char *passwd = (char *) malloc(64 * sizeof(char));

	fp = safe_fopen_wrapper(write_passwd_fname, "r");
	if(fp == NULL) {
		EXCEPT("Unable to open password file %s\n", write_passwd_fname);
	}
	
	if(fscanf(fp, "%s", passwd) != 1) {
		fclose(fp);
		EXCEPT("Unable to read password from file %s\n", write_passwd_fname);
	}
	fclose(fp);
	return passwd;
}
 
void
JobQueueDBManager::config(bool reconfig) 
{
	char *host, *port, *ptr_colon;
	int len, tmp1, tmp2, tmp3;

	if (param_boolean("QUILL_ENABLED", false) == false) {
		EXCEPT("Quill is currently disabled. Please set QUILL_ENABLED to "
				"TRUE if you want this functionality and read the manual about "
				"this feature since it requires other attributes to be set "
				"properly.");
	}

		//bail out if no SPOOL variable is defined since its used to 
		//figure out the location of the job_queue.log file
	char *spool = param("SPOOL");
	if(!spool) {
		EXCEPT("No SPOOL variable found in config file\n");
	}
  
	jobQueueLogFile = (char *) malloc(_POSIX_PATH_MAX * sizeof(char));
	sprintf(jobQueueLogFile, "%s/job_queue.log", spool);

		/*
		  Here we try to read the <ipaddress:port> and  
		  if one is not specified, we except out
		*/

	jobQueueDBIpAddress = param("QUILL_DB_IP_ADDR");
	if(jobQueueDBIpAddress) {
		len = strlen(jobQueueDBIpAddress);
		
			//both are subsets of jobQueueDBIpAddress so a size of
			//jobQueueDBIpAddress is more than enough
		host = (char *) malloc(len * sizeof(char));
		port = (char *) malloc(len * sizeof(char));

			//split the <ipaddress:port> into its two parts accordingly
		ptr_colon = strchr(jobQueueDBIpAddress, ':');
		strncpy(host, 
				jobQueueDBIpAddress, 
				ptr_colon - jobQueueDBIpAddress);
		host[ptr_colon - jobQueueDBIpAddress] = '\0';

		strcpy(port, ptr_colon+1);
	}
	else {
		EXCEPT("No QUILL_DB_IP_ADDR variable found in config file\n");
	}

		/* Here we read the database name and if one is not specified
		   we except out
		   If there are more than one quill daemons are writing to the
		   same databases, its absolutely necessary that the database
		   names be unique or else there would be clashes.  Having 
		   unique database names is the responsibility of the administrator
		*/
	jobQueueDBName = param("QUILL_DB_NAME");
	if(!jobQueueDBName) {
		EXCEPT("No QUILL_DB_NAME variable found in config file\n");
	}

	char *writePasswordFile = (char *) malloc(_POSIX_PATH_MAX * sizeof(char));
	sprintf(writePasswordFile, "%s/.quillwritepassword", spool);

	char *writePassword = getWritePassword(writePasswordFile);

	tmp1 = strlen(jobQueueDBName);
	tmp2 = strlen(writePassword);

		//tmp3 is the size of dbconn - its size is estimated to be
		//(2 * len) for the host/port part, tmp1 + tmp2 for the
		//password and dbname part and 1024 as a cautiously
		//overestimated sized buffer
	tmp3 = (2 * len) + tmp1 + tmp2 + 1024;

	jobQueueDBConn = (char *) malloc(tmp3 * sizeof(char));

	sprintf(jobQueueDBConn, 
			"host=%s port=%s user=quillwriter password=%s dbname=%s", 
			host, port, writePassword, jobQueueDBName);

	if(param_boolean("QUILL_MANAGE_VACUUM",false) == TRUE) {
		quillManageVacuum = true;

		dprintf(D_ALWAYS, "WARNING: You have chosen Quill to perform vacuuming\n");
		dprintf(D_ALWAYS, "Currently, Quill's vacuum calls are synchronous.\n");
		dprintf(D_ALWAYS, "Vacuum calls may take a long time to execute and in\n");
		dprintf(D_ALWAYS, "certain situations, this may cause the Quill daemon to\n");
		dprintf(D_ALWAYS, "be killed and restarted by the master. Instead, we\n");
		dprintf(D_ALWAYS, "recommend the user to upgrade to postgresql's version 8.1\n");
		dprintf(D_ALWAYS, "which manages all vacuuming tasks automatically.\n");
		dprintf(D_ALWAYS, "QUILL_MANAGE_VACUUM can then be set to false\n\n");
	}
	else {
		quillManageVacuum = false;
	}

		// read the polling period and if one is not specified use 
		// default value of 10 seconds
	pollingPeriod = param_integer("QUILL_POLLING_PERIOD",10);
  
		// length of history to keep; 
		// use default value of 6 months = 180 days :)
	purgeHistoryDuration = param_integer("QUILL_HISTORY_DURATION",180);

		/* 
		   the following parameters specifies the frequency of calling
		   the purge history command.  
		   This is different from purgeHistoryDuration as this just 
		   calls the command, which actually deletes only those rows
		   which are older than purgeHistoryDuration.  Since
		   the SQL itself for checking old records and deleting can be
		   quite expensive, we leave the frequency of calling it upto
		   the administrator.  By default, it is 24 hours. 
		*/
	
	historyCleaningInterval = param_integer("QUILL_HISTORY_CLEANING_INTERVAL",24);
  
	dprintf(D_ALWAYS, "Using Job Queue File %s\n", jobQueueLogFile);
	dprintf(D_ALWAYS, "Using Database IpAddress = %s\n", jobQueueDBIpAddress);
	dprintf(D_ALWAYS, "Using Database Name = %s\n", jobQueueDBName);

	dprintf(D_ALWAYS, "Using Database Connection Information: "
			"host=%s port=%s user=quillwriter dbname=%s", 
			host, port, jobQueueDBName);

	dprintf(D_ALWAYS, "Using Polling Period = %d\n", pollingPeriod);
	dprintf(D_ALWAYS, "Using Purge History Duration = %d days\n", 
			purgeHistoryDuration);
	dprintf(D_ALWAYS, "Using History Cleaning Interval = %d hours\n", 
			historyCleaningInterval);

	if(ad) {
		delete ad;
		ad = NULL;
	}

	if(writePassword) {
		free(writePassword);
		writePassword = NULL;
	}
	if(writePasswordFile) {
		free(writePasswordFile);
		writePasswordFile = NULL;
	}
	if(spool) {
		free(spool);
		spool = NULL;
	}
	if(host) {
		free(host);
		host = NULL;
	}
	if(port) {
		free(port);
		port = NULL;
	}

		// this function is also called when condor_reconfig is issued
		// and so we dont want to recreate all essential objects
	if(!reconfig) {
		prober = new Prober();
		caLogParser = new ClassAdLogParser();

		DBObj = new PGSQLDatabase(jobQueueDBConn);

		xactState = NOT_IN_XACT;
		numTimesPolled = 0; 

		multi_sql_str = NULL;
	}
  
		//this function assumes that certain members exist and so
		//it should be done after constructing those objects
	setJobQueueFileName(jobQueueLogFile);

		// START version number sniffing code
	int pg_version_number=0;
	if(connectDB(NOT_IN_XACT) == QUILL_FAILURE) {
		displayDBErrorMsg("Unable to connect to database in order to retrieve the PostgreSQL version --- ERROR");
	}         
	pg_version_number = DBObj->getDatabaseVersion();
	if(pg_version_number <= 0) {
		dprintf(D_ALWAYS, "Using unknown PostgreSQL version\n");
	}
	else {
		dprintf(D_ALWAYS, "Using PostgreSQL version %d\n",pg_version_number);
		if(pg_version_number < 80100) {
			dprintf(D_ALWAYS, "WARNING: You are using an older version of PostgreSQL\n");
			dprintf(D_ALWAYS, "We recommend that users upgrade to version 8.1 of PostgreSQL\n");
			dprintf(D_ALWAYS, "Maintenance tasks such as vacuuming in prior versions are not\n");
			dprintf(D_ALWAYS, "automated. Over time, this can degrade Quill's performance\n");
			dprintf(D_ALWAYS, "Starting 8.1, such maintenance tasks are completely integrated\n");
			dprintf(D_ALWAYS, "and automated.\n\n");
		}
	}
	disconnectDB(NOT_IN_XACT);
		// END version number sniffing code
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
	time_t t1, t2;

		//reset the reporting counters at the beginning of each probe
	lastBatchSqlProcessed = 0;
	
	st = getJQPollingInfo(); // get the last polling information

		//if we are unable to get to the database, then either the 
		//postgres server is down or the database is deleted.  In 
		//any case, we call checkSchema to create a database if needed
	if(st == QUILL_FAILURE) {
		st = checkSchema();
		if(st == QUILL_FAILURE) {
			return QUILL_FAILURE;
		}
	}

	fst = caLogParser->openFile();
	if(fst == FILE_OPEN_ERROR) {
		return QUILL_FAILURE;
	}

	probe_st = prober->probe(caLogParser->getCurCALogEntry(),
							 caLogParser->getFileDescriptor());	// polling

		//this and the gettimeofday call just below the switch together
		//determine the period of time taken by this whole batch of sql
		//statements
	t1 = time(NULL);

		// {init|add}JobQueueDB processes the  Log and stores probing
		// information into DB documentation for how do we determine 
		// the correct state is in the Prober->probe method
	switch(probe_st) {
	case INIT_QUILL:
		dprintf(D_ALWAYS, "POLLING RESULT: INIT\n");
		ret_st = initJobQueueTables();
		break;
	case ADDITION:
		dprintf(D_ALWAYS, "POLLING RESULT: ADDED\n");
		ret_st = addJobQueueTables();
		break;
	case COMPRESSED:
		dprintf(D_ALWAYS, "POLLING RESULT: COMPRESSED\n");
		ret_st = addOldJobQueueRecords();
		if(ret_st != QUILL_FAILURE) {
			ret_st = initJobQueueTables();
		}
		break;
	case PROBE_ERROR:
		dprintf(D_ALWAYS, "POLLING RESULT: ERROR\n");
        	ret_st = connectDB();                 
        	if(ret_st == QUILL_FAILURE) {
			displayDBErrorMsg("Unable to connect to database in order to clean tables --- ERROR");
			break;
        	}         
        	ret_st = cleanupJobQueueTables(); // delete all job queue tables

        	if(ret_st == QUILL_FAILURE) {
			displayDBErrorMsg("Init Job Queue Table unable to clean up job queue tables --- ERROR");
			break;
        	}
		disconnectDB(COMMIT_XACT);
		break;
	case NO_CHANGE:
		dprintf(D_ALWAYS, "POLLING RESULT: NO CHANGE\n");
		ret_st = QUILL_SUCCESS;
		break;
	default:
		dprintf(D_ALWAYS, "ERROR HAPPENED DURING POLLING\n");
		ret_st = QUILL_FAILURE;
	}

	t2 = time(NULL);

	secsLastBatch = t2 - t1;
	
	totalSqlProcessed += lastBatchSqlProcessed;

	fst = caLogParser->closeFile();

	if (ret_st == QUILL_SUCCESS) {
		time_t clock;
		char           *sql_str;
		
		(void)time(  (time_t *)&clock );
		
			/* update the last report time in jobqueuepollinginfo */
		sql_str = (char *) malloc(MAX_FIXED_SQL_STR_LENGTH * sizeof(char));
		memset(sql_str, 0, MAX_FIXED_SQL_STR_LENGTH);
		sprintf(sql_str, 
				"UPDATE JobQueuePollingInfo SET last_poll_time = %ld",
				clock);
		
		connectDB(NOT_IN_XACT);
		DBObj->execCommand(sql_str);
		disconnectDB(NOT_IN_XACT);
		if(sql_str)
			free(sql_str);
	}

	return ret_st;
}

/*! delete the job queue related tables
 *  \return the result status
 */	
QuillErrCode
JobQueueDBManager::cleanupJobQueueTables()
{
	const int		sqlNum = 6;
	int		i;
	char 	sql_str[sqlNum][256];

		// we only delete job queue related information.
		// historical information is deleted based on user policy
		// see QUILL_HISTORY_DURATION in the manual for more info
	sprintf(sql_str[0], 
			"DELETE FROM ClusterAds_Str;");
	sprintf(sql_str[1], 
			"DELETE FROM ClusterAds_Num;");
	sprintf(sql_str[2], 
			"DELETE FROM ProcAds_Str;");
	sprintf(sql_str[3], 
			"DELETE FROM ProcAds_Num;");
	sprintf(sql_str[4], 
			"DELETE FROM JobQueuePollingInfo;");
	sprintf(sql_str[5],
			"INSERT INTO JobQueuePollingInfo (last_file_mtime, last_file_size,log_seq_num,log_creation_time) VALUES (0,0,0,0);");

	for (i = 0; i < sqlNum; i++) {
		if (DBObj->execCommand(sql_str[i]) == QUILL_FAILURE) {
			displayDBErrorMsg("Clean UP ALL Data --- ERROR");
			return QUILL_FAILURE; 
		}
	}

	return QUILL_SUCCESS;
}

/*! vacuums the job queue related tables 
 */
QuillErrCode
JobQueueDBManager::tuneupJobQueueTables()
{
	const int		sqlNum = 5;
	int		i;
	char 	sql_str[sqlNum][128];

		// When a record is deleted, postgres only marks it
		// deleted.  Then space is reclaimed in a lazy fashion,
		// by vacuuming it, and as such we do this here.  
		// vacuuming is asynchronous, but can get pretty expensive
	sprintf(sql_str[0], 
			"VACUUM ANALYZE Clusterads_Str;");
	sprintf(sql_str[1], 
			"VACUUM ANALYZE Clusterads_Num;");
	sprintf(sql_str[2], 
			"VACUUM ANALYZE Procads_Str;");
	sprintf(sql_str[3], 
			"VACUUM ANALYZE Procads_Num;");
	sprintf(sql_str[4], 
			"VACUUM ANALYZE Jobqueuepollinginfo;");

	for (i = 0; i < sqlNum; i++) {
		if (DBObj->execCommand(sql_str[i]) == QUILL_FAILURE) {
			displayDBErrorMsg("VACUUM Database --- ERROR");
			return QUILL_FAILURE; 
		}
	}

	return QUILL_SUCCESS;
}


/*! purge all jobs from history according to user policy
 *  and vacuums all historical tables. 
 *  Since purging is based on a timer, quill needs to be up for at least 
 *  the duration of QUILL_HISTORY_DURATION, in order for purging to occur
 *  on a timely basis.
 */
void
JobQueueDBManager::purgeOldHistoryRows()
{
	const int		sqlNum = 4;
	int		i;
	char 	sql_str[sqlNum][256];

	dprintf(D_ALWAYS, "Starting purge of old History rows\n");
	sprintf(sql_str[0],
			"DELETE FROM History_Vertical WHERE cid IN (SELECT cid FROM "
			"History_Horizontal WHERE \"EnteredHistoryTable\" < "
			"timestamp with time zone 'now' - interval '%d days');", 
			purgeHistoryDuration);
	sprintf(sql_str[1],
			"DELETE FROM History_Horizontal WHERE \"EnteredHistoryTable\" < "
			"timestamp with time zone 'now' - interval '%d days';", 
			purgeHistoryDuration);

		// When a record is deleted, postgres only marks it
		// deleted.  Then space is reclaimed in a lazy fashion,
		// by vacuuming it, and as such we do this here.  
		// vacuuming is asynchronous, but can get pretty expensive
	sprintf(sql_str[2], 
			"VACUUM ANALYZE History_Horizontal;");
	sprintf(sql_str[3], 
			"VACUUM ANALYZE History_Vertical;");

		// the delete commands are wrapped inside a transaction.  We 
		// dont want to have stuff deleted from the horizontal but not 
		//the vertical and vice versa
	if(connectDB(BEGIN_XACT) == QUILL_FAILURE) {
		displayDBErrorMsg("Purge History Rows unable to connect--- ERROR");
		return; 
	}

		//ending at 2 since only the first 2 can be wrapped inside xact
	for (i = 0; i < 2; i++) {
		if (DBObj->execCommand(sql_str[i]) == QUILL_FAILURE) {
			displayDBErrorMsg("Purge History Rows --- ERROR");
			disconnectDB(ABORT_XACT);
			return; 
		}
	}
	disconnectDB(COMMIT_XACT);

	if(quillManageVacuum) {
		//vacuuming cannot be bound inside a transaction
		if(connectDB(NOT_IN_XACT) == QUILL_FAILURE) {
			displayDBErrorMsg("Vacuum History Rows unable to connect--- ERROR");
			return; 
		}
		//statements 2 and 3 should not wrapped inside xact
		for (i = 2; i < 4; i++) {
			if (DBObj->execCommand(sql_str[i]) == QUILL_FAILURE) {
				displayDBErrorMsg("Vacuum History Rows --- ERROR");
				disconnectDB(NOT_IN_XACT);
				return; 
			}
		}
		disconnectDB(NOT_IN_XACT);
	}

	dprintf(D_ALWAYS, "Finished purge of old History rows\n");

	daemonCore->Reset_Timer(reindexTimeId, 5);
}

/*! After we purge history, and optionally vacuum, postgres
 *  sometimes needs the history table reindexed.
 *  This can take some time, so do it in a separate handler from
 *  the above deletion, so quill is more responsive
 */
void
JobQueueDBManager::reindexTables()
{
	const int		sqlNum = 5;
	int		i;
	char 	*sql_str[sqlNum];

	// Skip reindexing iff QUILL_SHOULD_REINDEX is false
	if (param_boolean("QUILL_SHOULD_REINDEX",true) == false) {
		return;
	}

	sql_str[0] = "REINDEX table history_horizontal;";
	sql_str[1] = "REINDEX table clusterads_Num;";
	sql_str[2] = "REINDEX table clusterads_Str;";
	sql_str[3] = "REINDEX table procads_Num;";
	sql_str[4] = "REINDEX table procads_Str;";

	// The DB folks ensure us that the following very 
	// slow reindex should not be needed.
	//sql_str[5] = "REINDEX table history_vertical;";

	dprintf(D_ALWAYS, "Begin Reindexing postgres tables\n");

	if(connectDB(NOT_IN_XACT) == QUILL_FAILURE) {
		displayDBErrorMsg("Reindex tables unable to connect--- ERROR");
		return;
	}

	for (i = 0; i < sqlNum; i++) {
		if (jqDatabase->execCommand(sql_str[i]) == QUILL_FAILURE) {
			displayDBErrorMsg("Reindex tables --- ERROR");
			disconnectDB(NOT_IN_XACT);
			return; 
		}
	}

	disconnectDB(NOT_IN_XACT);
	dprintf(D_ALWAYS, "Finish Reindexing postgres tables\n");

	// Need to reset the timer to keep it alive
	daemonCore->Reset_Timer(reindexTimeId, 3600 * historyCleaningInterval); 
}

/*! connect to DBMS
 *  \return the result status
 *			QUILL_SUCCESS: Success
 * 			QUILL_FAILURE: Fail (DB connection and/or Begin Xact fail)
 */	
QuillErrCode
JobQueueDBManager::connectDB(XactState Xact)
{
	int st = 0;
	st = DBObj->connectDB(jobQueueDBConn);
	if (st == QUILL_FAILURE) { // connect to DB
		isConnectedToDB = false;		
		return QUILL_FAILURE;
	}
	
	isConnectedToDB = true;
	if (Xact == BEGIN_XACT) {
		if (DBObj->beginTransaction() == QUILL_FAILURE) { // begin XACT
			return QUILL_FAILURE;
		}
	}
  
	return QUILL_SUCCESS;
}


/*! disconnect from DBMS, and handle XACT (commit, abort, not in XACT)
 *  \param commit XACT command 
 */
QuillErrCode
JobQueueDBManager::disconnectDB(XactState commit)
{
	if (commit == COMMIT_XACT) {
		DBObj->commitTransaction(); // commit XACT
		xactState = NOT_IN_XACT;
	} else if (commit == ABORT_XACT) { // abort XACT
		DBObj->rollbackTransaction();
		xactState = NOT_IN_XACT;
	}

	DBObj->disconnectDB(); // disconnect from DB

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

	while(st == FILE_READ_SUCCESS) {	   
		if (processLogEntry(op_type, jobQueue) == QUILL_FAILURE) {
				// process each ClassAd Log Entry
			return QUILL_FAILURE;
		}
		st = caLogParser->readLogEntry(op_type);
	}

	return QUILL_SUCCESS;
}

/*! load job ads in a job queue collection into DB
 */
QuillErrCode
JobQueueDBManager::loadJobQueue(JobQueueCollection *jobQueue)
{
	char* 	ret_str;
	char*   ret_str2;
	char	sql_str[256];
	bool 	bFirst = true;

		//
		// Make a COPY SQL string and load it into ClusterAds_Str table
		//
	jobQueue->initAllJobAdsIteration();

	while((ret_str = jobQueue->getNextClusterAd_StrCopyStr()) != NULL) {

		if ((bFirst == true)&& (ret_str != NULL)) {			
			// we need to issue the COPY command first
			sprintf(sql_str, "COPY ClusterAds_Str FROM stdin;");
			if (DBObj->execCommand(sql_str) == QUILL_FAILURE) {
				displayDBErrorMsg("COPY ClusterAds_Str --- ERROR");
				return QUILL_FAILURE; // return a error code, 0
			}
	    
			bFirst = false;
		}
	  
		if (ret_str != NULL) {
			if (DBObj->sendBulkData(ret_str) == QUILL_FAILURE) {
				return QUILL_FAILURE;
			}
		}
	  
	}
	
	if (bFirst == false) {
		if (DBObj->sendBulkDataEnd() == QUILL_FAILURE) {
			return QUILL_FAILURE;
		}
	  
		bFirst = true;
	}	
	
		//
		// Make a COPY SQL string and load it into ClusterAds_Num table
		//
	jobQueue->initAllJobAdsIteration();
	while((ret_str = jobQueue->getNextClusterAd_NumCopyStr()) != NULL) {
	  		
		if ((bFirst == true)&& (ret_str != NULL)) {			
			sprintf(sql_str, "COPY ClusterAds_Num FROM stdin;");
			if (DBObj->execCommand(sql_str) == QUILL_FAILURE) {
				displayDBErrorMsg("COPY ClusterAds_Num --- ERROR");
				return QUILL_FAILURE; // return a error code, 0
			}
	    
			bFirst = false;
		}
	  
		if (ret_str != NULL) {
			if (DBObj->sendBulkData(ret_str) == QUILL_FAILURE) {
				return QUILL_FAILURE;
			}	
		}		
	  
	}
	
	if (bFirst == false) {
		if (DBObj->sendBulkDataEnd() == QUILL_FAILURE) {
			return QUILL_FAILURE;
		}
	  	  
		bFirst = true;
	}
	
	
	
	
	
		//
		// Make a COPY sql string and load it into ProcAds_Str table
		//
	jobQueue->initAllJobAdsIteration();
	while((ret_str = jobQueue->getNextProcAd_StrCopyStr()) != NULL) {
		if ((bFirst == true)&& (ret_str != NULL)) {			
			sprintf(sql_str, "COPY ProcAds_Str FROM stdin;");
			if (DBObj->execCommand(sql_str) == QUILL_FAILURE) {
				displayDBErrorMsg("COPY ProcAds_Str --- ERROR");
				return QUILL_FAILURE; 
			}
			
			bFirst = false;
		}
	  
	  
		if (ret_str != NULL) {
			if (DBObj->sendBulkData(ret_str) == QUILL_FAILURE) {
				return QUILL_FAILURE;
			}
		}
	  
	}
	
	if (bFirst == false) {
	  
		if (DBObj->sendBulkDataEnd() == QUILL_FAILURE) {
			return QUILL_FAILURE;
		}
	  	  
		bFirst = true;
	}
	
	
		//
		// Make a COPY sql string and load it into ProcAds_Num table
		//
	jobQueue->initAllJobAdsIteration();
	while((ret_str = jobQueue->getNextProcAd_NumCopyStr()) != NULL) {
		if ((bFirst == true)&& (ret_str != NULL)) {			
			sprintf(sql_str, "COPY ProcAds_Num FROM stdin;");
			if (DBObj->execCommand(sql_str) == QUILL_FAILURE) {
				displayDBErrorMsg("COPY ProcAds_Num --- ERROR");
				return QUILL_FAILURE; 
			}
	    
			bFirst = false;
		}
	  
		if (ret_str != NULL) {
			if (DBObj->sendBulkData(ret_str) == QUILL_FAILURE) {
				return QUILL_FAILURE;
			}
		}
	  
	}
	
	
	if (bFirst == false) {
		if (DBObj->sendBulkDataEnd() == QUILL_FAILURE) {
			return QUILL_FAILURE;
		}
	  
		bFirst = true;
	}
	

		/*
		  Make the SQL strings for both history_horizontal and 
		  vertical and execute them.  Our technique of bulk loading
		  history is different from that of the other tables. 
		  Instead of using the COPY command, we use successive INSERT
		  statements.  This code will execute rarely; it is executed
		  when quill needs to initialize its queue (see 'maintain' 
		  above) and a job gets deleted from the 
		  job queue (completed, removed, etc.) and as such needs to 
		  get into the history tables.  Quill, as part of its bulk 
		  loading routine will also make sure that jobs properly get
		  inserted into the history tables.
		*/		

	int st1=0, st2=0;
	ret_str = ret_str2 = NULL;
	jobQueue->initAllHistoryAdsIteration();
	st1 = jobQueue->getNextHistoryAd_SqlStr(ret_str, ret_str2);

	while(st1 > 0 &&  ret_str != NULL) {
			/*  st2 is not used anywhere below this.  
				We dont check on errors as:
				1) If there are connection errors, they'll get 
				resolved below anyway and 
				2) There can be integrity issues (duplicate key
				error).  This comes up frequently as stuff
				stays in history for long periods of time
				So our semantics here is ignore the error and
				move on to the next command in the job_queue.log
			*/
		st2 = DBObj->execCommand(ret_str);
		st2 = DBObj->execCommand(ret_str2);

			//these need to be nullified as the next routine expects
			//them to be so.  This is not a memory leak since these 
			//arguments are passed by reference and their actual 
			//locations are members of the JobQueueCollection class
			//and as such will be freed by it.
		ret_str = ret_str2 = NULL;
		st1 = jobQueue->getNextHistoryAd_SqlStr(ret_str, ret_str2);

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

		//  start of first phase of bulkloading

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
		//  end of first phase of bulkloading

		//  start of second phase of bulkloading


		// For job queue tables, send COPY string to RDBMS: 
		// For history tables, send INSERT to DBMS - all part of
		// bulk loading
	if (loadJobQueue(jobQueue) == QUILL_FAILURE) {
		delete jobQueue;
		return QUILL_FAILURE;
	}

		//  end of second phase of bulkloading
	delete jobQueue;

	return QUILL_SUCCESS;
}


/*! incrementally read and process log entries from file 
 */
QuillErrCode 
JobQueueDBManager::readAndWriteLogEntries(ClassAdLogParser *parser)
{
	int		op_type;
	FileOpErrCode st;
	
	st = parser->readLogEntry(op_type);
	while(st == FILE_READ_SUCCESS) {	   
		if (processLogEntry(op_type, false, parser) == QUILL_FAILURE) {
				// process each ClassAd Log Entry
			return QUILL_FAILURE;
		}
	   	lastBatchSqlProcessed++;
		
		st = parser->readLogEntry(op_type);
	}

	return QUILL_SUCCESS;
}

QuillErrCode
JobQueueDBManager::addOldJobQueueRecords() {
	
		//get the lastseqnum and cur_probed_seq_num
		//iterate through files job_queue.log.lastseqnum through cur_probed_seq_num - 1
		//for each file
		//initialize a new classadlogparser
		//if first file, set its offset to caLogParser->curCALogEntry->next_offset
		//go on reading and processing till end

	long int lastseqnum;
	long int curprobedseqnum;
	ClassAdLogParser parser;
	QuillErrCode st;
	FileOpErrCode fst;
	char *spool=NULL, *jqfileroot=NULL, *jqfile=NULL;

	lastseqnum = prober->getLastSequenceNumber();
	curprobedseqnum = prober->getCurProbedSequenceNumber();

		//bail out if no SPOOL variable is defined since its used to 
		//figure out the location of the job_queue.log file
	spool = param("SPOOL");
	if(!spool) {
		EXCEPT("No SPOOL variable found in config file\n");
	}
  
		//for the first old job queue file, we need to read past the last offset 
		//for successive files, we'll read from the beginning
	parser.setNextOffset(((ClassAdLogEntry *)caLogParser->getCurCALogEntry())->next_offset);

	st = connectDB(NOT_IN_XACT); // connect to DBMS

	if(st == QUILL_FAILURE) {
		displayDBErrorMsg("addOldJobQueueRecords unable to connect to database --- ERROR");
		return QUILL_FAILURE; 
	}

	jqfile = (char *) malloc(_POSIX_PATH_MAX * sizeof(char));
	jqfileroot = (char *) malloc(_POSIX_PATH_MAX * sizeof(char));
	sprintf(jqfileroot, "%s/job_queue.log", spool);

	for(long int i=lastseqnum; i < curprobedseqnum; i++) {
			//dont clean up state of job queue tables if this is the first file in the list
			//as we will start processing it from somewhere in the middle in which case we
			//need the state from that point up to the beginning of the file
		if(i > lastseqnum) { 
			st = cleanupJobQueueTables(); // delete all job queue tables
			
			if(st == QUILL_FAILURE) {
				displayDBErrorMsg("addOldJobQueueRecords unable to clean up job queue tables --- ERROR");
				break; 
			}
		}
		sprintf(jqfile, "%s.%ld", jqfileroot, i);
		parser.setJobQueueName(jqfile);

		fst = parser.openFile();
		if(fst == FILE_OPEN_ERROR) {
			dprintf(D_ALWAYS, "Could not open file old job queue file %s --- ERROR\n", jqfile);
			dprintf(D_ALWAYS, "Skipping over and going to the next job queue file\n");
		}
		else {
			dprintf(D_ALWAYS, "Reading from file %s\n", jqfile);
			st = readAndWriteLogEntries(&parser);
			dprintf(D_ALWAYS, "End reading from file %s\n", jqfile);
			
			parser.closeFile();
			
			if(st == QUILL_FAILURE) {
				break;
			}
		}
			//regardless of whether the file was successfully opened or not
		parser.setNextOffset(0);
	}

	disconnectDB(NOT_IN_XACT);

	if(spool) {
		free(spool);
		spool = NULL;
	}
	if(jqfile) {
		free(jqfile);
		jqfile = NULL;
	}
	if(jqfileroot) {
		free(jqfileroot);
		jqfileroot = NULL;
	}
	return st;
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
	connectDB(NOT_IN_XACT);

	caLogParser->setNextOffset();

	st = readAndWriteLogEntries(caLogParser);

		// Store a polling information into DB
	if (st == QUILL_SUCCESS) {
		setJQPollingInfo();
		
			// VACUUM should be called outside XACT
			// but since any outstanding xacts will be ended by the time
			// we reach here, we do not issue any commits here.  Doing so
			// would result in warning messages in the postgres log file
		
			//we VACUUM job queue tables twice every day, assuming that
			//quill has been up for 12 hours
		if (quillManageVacuum && 
			(numTimesPolled++ * pollingPeriod) > (60 * 60 * 12)) {
			dprintf(D_ALWAYS, "WARNING: You have chosen Quill to perform vacuuming\n");
			dprintf(D_ALWAYS, "Currently, Quill's vacuum calls are synchronous.\n");
			dprintf(D_ALWAYS, "Vacuum calls may take a long time to execute and in\n");
			dprintf(D_ALWAYS, "certain situations, this may cause the Quill daemon to\n");
			dprintf(D_ALWAYS, "be killed and restarted by the master. Instead, we\n");
			dprintf(D_ALWAYS, "recommend the user to upgrade to postgresql's version 8.1\n");
			dprintf(D_ALWAYS, "which manages all vacuuming tasks automatically.\n");
			dprintf(D_ALWAYS, "QUILL_MANAGE_VACUUM can then be set to false\n\n");
			tuneupJobQueueTables();
			numTimesPolled = 0;
		}

	}
	
	disconnectDB(NOT_IN_XACT); // end transaction

	return st;
}

/*! purge all job queue rows and process the entire job_queue.log file
 *  also vacuum the job queue tables
 */
QuillErrCode  
JobQueueDBManager::initJobQueueTables()
{
        QuillErrCode st;
        
        st = connectDB(); // connect to DBMS
                
        if(st == QUILL_FAILURE) {
                displayDBErrorMsg("Init Job Queue Tables unable to connect to database --- ERROR");
                return QUILL_FAILURE;
        }
         
        st = cleanupJobQueueTables(); // delete all job queue tables
        
        if(st == QUILL_FAILURE) {
                displayDBErrorMsg("Init Job Queue Table unable to clean up job queue tables --- ERROR");
                return QUILL_FAILURE;
        }

        st = buildAndWriteJobQueue(); // bulk load job queue log

			// Store polling information in database
        if (st == QUILL_SUCCESS) {
                setJQPollingInfo();
                disconnectDB(COMMIT_XACT); // end Xact
        }
                
        else {
			disconnectDB(ABORT_XACT); // disconnect
        }
        
        return st;
}


/*! handle a log Entry: work with a job queue collection.
 *  (not with DBMS directry)
 */
QuillErrCode 
JobQueueDBManager::processLogEntry(int op_type, JobQueueCollection* jobQueue)
{
	char *key, *mytype, *targettype, *name, *value, *newvalue;
	key = mytype = targettype = name = value = newvalue = NULL;
	QuillErrCode	st = QUILL_SUCCESS;

	int job_id_type;
	char cid[512];
	char pid[512];

		// REMEMBER:
		//	each get*ClassAdBody() funtion allocates the memory of 
		// 	parameters. Therefore, they all must be deallocated here,
		// and they are at the end of the routine
	switch(op_type) {
	case CondorLogOp_LogHistoricalSequenceNumber: 
		break;
	case CondorLogOp_NewClassAd: {
		if (caLogParser->getNewClassAdBody(key, mytype, targettype) == QUILL_FAILURE) {
			st = QUILL_FAILURE; 
			break;
		}
		job_id_type = getProcClusterIds(key, cid, pid);
		ClassAd* new_ad = new ClassAd();
		new_ad->SetMyTypeName("Job");
		new_ad->SetTargetTypeName("Machine");

		switch(job_id_type) {
		case IS_CLUSTER_ID:
			jobQueue->insertClusterAd(cid, new_ad);
			break;

		case IS_PROC_ID:
			jobQueue->insertProcAd(cid, pid, new_ad);
			break;

		default:
			dprintf(D_ALWAYS, "[QUILL] New ClassAd --- ERROR\n");
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
			ClassAd *cluster_ad = jobQueue->find(cid);
			ClassAd *proc_ad = jobQueue->find(cid,pid);
			if(!cluster_ad || !proc_ad) {
			    dprintf(D_ALWAYS, 
						"[QUILL] Destroy ClassAd --- Cannot find clusterad "
						"or procad in memory job queue");
				st = QUILL_FAILURE; 
				break;
			}
			ClassAd *cluster_ad_new = new ClassAd(*cluster_ad);
			ClassAd *history_ad = new ClassAd(*proc_ad);

			history_ad->ChainToAd(cluster_ad_new);
			jobQueue->insertHistoryAd(cid,pid,history_ad);
			jobQueue->removeProcAd(cid, pid);
			
			break;
		}
		default:
			dprintf(D_ALWAYS, "[QUILL] Destroy ClassAd --- ERROR\n");
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
		if( ClassAd::ClassAdAttributeIsPrivate(name) ) {
				// This hides private stuff like ClaimID.
			break;
		}
		newvalue = fillEscapeCharacters(value);
		job_id_type = getProcClusterIds(key, cid, pid);

		switch(job_id_type) {
		case IS_CLUSTER_ID: {
			ClassAd* cluster_ad = jobQueue->findClusterAd(cid);
			if (cluster_ad != NULL) {
				cluster_ad->Assign(name, newvalue);
			}
			else {
				dprintf(D_ALWAYS, 
						"[QUILL] ERROR: There is no such Cluster Ad[%s]\n", 
						cid);
			}
			break;
		}
		case IS_PROC_ID: {
			ClassAd* proc_ad = jobQueue->findProcAd(cid, pid);
			if (proc_ad != NULL) {
				proc_ad->Assign(name, newvalue);
			}
			else {
				dprintf(D_ALWAYS, 
						"[QUILL] ERROR: There is no such Proc Ad[%s.%s]\n", 
						cid, pid);
			}
			break;
		}
		default:
			dprintf(D_ALWAYS, "[QUILL] Set Attribute --- ERROR\n");
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
			ClassAd* cluster_ad = jobQueue->findClusterAd(cid);
			cluster_ad->Delete(name);
			break;
		}
		case IS_PROC_ID: {
			ClassAd* proc_ad = jobQueue->findProcAd(cid, pid);
			proc_ad->Delete(name);
			break;
		}
		default:
			dprintf(D_ALWAYS, "[QUILL] Delete Attribute --- ERROR\n");
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
	default:
		dprintf(D_ALWAYS, "[QUILL] Unsupported Job Queue Command\n");
		st = QUILL_FAILURE; 
		break;
	}

		// pointers are release
	if (key != NULL) free(key);
	if (mytype != NULL) free(mytype);
	if (targettype != NULL) free(targettype);
	if (name != NULL) free(name);
	if (value != NULL) free(value);
	if (newvalue != NULL) free(newvalue);
	return st;
}






/*! handle ClassAd Log Entry
 *
 * is a wrapper over all the processXXX functions
 * in this and all the processXXX routines, if exec_later == true, 
 * a SQL string is returned instead of actually sending it to the DB.
 * However, we always have exec_later = false, which means it actually
 * writes to the database in an eager fashion
 */
QuillErrCode 
JobQueueDBManager::processLogEntry(int op_type, bool exec_later, ClassAdLogParser *parser)
{
	char *key, *mytype, *targettype, *name, *value;
	key = mytype = targettype = name = value = NULL;
	QuillErrCode	st = QUILL_SUCCESS;

		// REMEMBER:
		//	each get*ClassAdBody() funtion allocates the memory of 
		// 	parameters. Therefore, they all must be deallocated here,
		//  and they are at the end of the routine
	switch(op_type) {
	case CondorLogOp_LogHistoricalSequenceNumber: 
		break;
	case CondorLogOp_NewClassAd:
		st = parser->getNewClassAdBody(key, mytype, targettype);
		if (st == QUILL_FAILURE) {
			break;
		}
		st = processNewClassAd(key, mytype, targettype, exec_later);			
		break;
	case CondorLogOp_DestroyClassAd:
		st = parser->getDestroyClassAdBody(key);
		if (st == QUILL_FAILURE) {
			break;
		}		
		st = processDestroyClassAd(key, exec_later);			
		break;
	case CondorLogOp_SetAttribute:
		st = parser->getSetAttributeBody(key, name, value);
		if (st == QUILL_FAILURE) {
			break;
		}
		st = processSetAttribute(key, name, value, exec_later);			
		break;
	case CondorLogOp_DeleteAttribute:
		st = parser->getDeleteAttributeBody(key, name);
		if (st == QUILL_FAILURE) {
			break;
		}
		st = processDeleteAttribute(key, name, exec_later);		
		break;
	case CondorLogOp_BeginTransaction:
		st = processBeginTransaction(exec_later);
		break;
	case CondorLogOp_EndTransaction:
		st = processEndTransaction(exec_later);
		break;
	default:
		dprintf(D_ALWAYS, "[QUILL] Unsupported Job Queue Command [%d]\n", 
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
JobQueueDBManager::displayDBErrorMsg(const char* errmsg)
{
	dprintf(D_ALWAYS, "[QUILL] %s\n", errmsg);
	dprintf(D_ALWAYS, "\t%s\n", DBObj->getDBError());
}

/*! separate a key into Cluster Id and Proc Id 
 *  \return job id type
 *			IS_CLUSTER_ID: when it is a cluster id
 *			IS_PROC_ID: when it is a proc id
 * 			IS_UNKNOWN_ID: it fails
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

		//i+1 since i is the '.' character
	pid_in_key = (const char*)(key + (i + 1));
	strcpy(pid, pid_in_key);

	if (atol(pid) == -1) { // Cluster ID
		return IS_CLUSTER_ID;	
	}
	return IS_PROC_ID; // Proc ID
}

/*! process NewClassAd command, working with DBMS
 *  \param key key
 *  \param mytype mytype
 *  \param ttype targettype
 *  \param exec_later send SQL into RDBMS now or not?
 *  \return the result status
 */
QuillErrCode 
JobQueueDBManager::processNewClassAd(char* key, 
									 char* mytype, 
									 char* ttype, 
									 bool exec_later)
{
		//here we assume that the cid,pid,mytype,targettypes are all 
		//small enough so that the entire sql string containing them
		//fits in a MAX_FIXED_SQL_STR_LENGTH buffer
	char sql_str1[MAX_FIXED_SQL_STR_LENGTH];
	char sql_str2[MAX_FIXED_SQL_STR_LENGTH];
	char cid[512];
	char pid[512];
	int  job_id_type;
	
		// It could be ProcAd or ClusterAd
		// So need to check
	job_id_type = getProcClusterIds(key, cid, pid);

	switch(job_id_type) {
	case IS_CLUSTER_ID:
		sprintf(sql_str1, 
				"DELETE From ClusterAds_Str where cid=%s;"
				"INSERT INTO ClusterAds_Str (cid, attr, val) VALUES "
				"(%s, 'MyType', '\"%s\"');", 
				cid, cid, mytype);

		sprintf(sql_str2, 
				"INSERT INTO ClusterAds_Str (cid, attr, val) VALUES "
				"(%s, 'TargetType', '\"%s\"');", 
				cid, ttype);

		break;
	case IS_PROC_ID:
		sprintf(sql_str1, 
				"DELETE From ProcAds_Str where cid=%s and pid=%s;"
				"INSERT INTO ProcAds_Str (cid, pid, attr, val) VALUES "
				"(%s, %s, 'MyType', '\"Job\"');", 
				cid, pid, cid, pid);

		sprintf(sql_str2, 
				"INSERT INTO ProcAds_Str (cid, pid, attr, val) VALUES "
				"(%s, %s, 'TargetType', '\"Machine\"');", 
				cid, pid);

		break;
	case IS_UNKNOWN_ID:
		dprintf(D_ALWAYS, "New ClassAd Processing --- ERROR\n");
		return QUILL_FAILURE; 
		break;
	}


	if (exec_later == false) { // execute them now
		if (DBObj->execCommand(sql_str1) == QUILL_FAILURE) {
			displayDBErrorMsg("New ClassAd Processing --- ERROR");
			return QUILL_FAILURE; 
		}
		if (DBObj->execCommand(sql_str2) == QUILL_FAILURE) {
			displayDBErrorMsg("New ClassAd Processing --- ERROR");
			return QUILL_FAILURE; 
		}
	}
	else {
		if (multi_sql_str != NULL) { // append them to a SQL buffer
			multi_sql_str = (char*)realloc(multi_sql_str, 
										   strlen(multi_sql_str) + 
										   strlen(sql_str1) + 
										   strlen(sql_str2) + 1);
			if(!multi_sql_str) {
				EXCEPT("Call to realloc failed\n");
			}

			strcat(multi_sql_str, sql_str1);
			strcat(multi_sql_str, sql_str2);
		}
		else {
			multi_sql_str = (char*)malloc(strlen(sql_str1) + 
										  strlen(sql_str2) + 1);
			sprintf(multi_sql_str,"%s%s", sql_str1, sql_str2);
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
 *  \param exec_later send SQL into RDBMS now or not?
 *  \return the result status
 */
QuillErrCode 
JobQueueDBManager::processDestroyClassAd(char* key, bool exec_later)
{
		//here we assume that the cid and pid  are all 
		//small enough so that the entire sql string containing them
		//fits in a MAX_FIXED_SQL_STR_LENGTH buffer
	char sql_str1[MAX_FIXED_SQL_STR_LENGTH]; 
	char sql_str2[MAX_FIXED_SQL_STR_LENGTH]; 
	char sql_str3[MAX_FIXED_SQL_STR_LENGTH];
	char sql_str4[MAX_FIXED_SQL_STR_LENGTH];
	char cid[100];
	char pid[100];
	bool inserthistory = false;
	int  job_id_type;
	QuillErrCode  st1, st2, st3;

  
		// It could be ProcAd or ClusterAd
		// So need to check
	job_id_type = getProcClusterIds(key, cid, pid);
  
	switch(job_id_type) {
	case IS_CLUSTER_ID:	// ClusterAds
		sprintf(sql_str1, 
				"DELETE FROM ClusterAds_Str WHERE cid = %s;", cid);
    
		sprintf(sql_str2, 
				"DELETE FROM ClusterAds_Num WHERE cid = %s;", cid);
		break;
    case IS_PROC_ID:

			/* the following ugly looking, however highly efficient SQL
			   does the following:
			   a) Get all rows from the ProcAds tables belonging to this job
			   b) Add to that all rows from ClusterAds table belonging
			      to this job and those that aren't already in ProcAds 
			   c) Do not get attributes which belong to the horizontal schema

			   Note that while combining rows from the ProcAds and ClusterAds 
			   views, we do a UNION ALL instead UNION since it avoids an
			   expensive sort and the rows are distinct anyway due to the IN 
			   constraint
			*/
		sprintf(sql_str3, 
				"INSERT INTO History_Vertical(cid,pid,attr,val) "
				"SELECT cid,pid,attr,val FROM (SELECT cid,pid,attr,val "
				"FROM ProcAds WHERE cid= %s and pid = %s "
				"UNION ALL "
				"SELECT cid,%s,attr,val FROM ClusterAds WHERE cid=%s "
				"AND attr NOT IN (SELECT attr FROM ProcAds WHERE cid =%s "
				"AND pid =%s)) AS T WHERE attr NOT IN "
				"('ClusterId','ProcId','Owner','QDate','RemoteWallClockTime',"
				"'RemoteUserCpu','RemoteSysCpu','ImageSize','JobStatus',"
				"'JobPrio','Cmd','CompletionDate','LastRemoteHost');"
				,cid,pid,pid,cid,cid,pid); 


			/* the following ugly looking, however highly efficient SQL
			   does the following:
			   a) Get all rows from the ProcAds tables belonging to this job
			   b) Add to that all rows from ClusterAds table belonging
			      to this job and those that aren't already in ProcAds 
			   c) Horizontalize this schema.  This is done via case statements
			   exploiting the highly clever, however, highly postgres specific
			   feature that NULL values come before non NULL values and as such
			   the MAX gives us exactly what we want - the non NULL value

			   Note that while combining rows from the ProcAds and ClusterAds 
			   views, we do a UNION ALL instead UNION since it avoids an
			   expensive sort and the rows are distinct anyway due to the IN 
			   constraint
			*/
		sprintf(sql_str4, 
				"INSERT INTO History_Horizontal(cid,pid,"
				"\"EnteredHistoryTable\",\"Owner\",\"QDate\","
				"\"RemoteWallClockTime\",\"RemoteUserCpu\",\"RemoteSysCpu\","
				"\"ImageSize\",\"JobStatus\",\"JobPrio\",\"Cmd\","
				"\"CompletionDate\",\"LastRemoteHost\") "
				"SELECT %s,%s, 'now', "
				"max(CASE WHEN attr='Owner' THEN val ELSE NULL END), "
				"max(CASE WHEN attr='QDate' THEN "
				     "cast(val as integer) ELSE NULL END), "
				"max(CASE WHEN attr='RemoteWallClockTime' THEN "
				     "cast(val as integer) ELSE NULL END), "
				"max(CASE WHEN attr='RemoteUserCpu' THEN "
				     "cast(val as float) ELSE NULL END), "
				"max(CASE WHEN attr='RemoteSysCpu' THEN "
				     "cast(val as float) ELSE NULL END), "
				"max(CASE WHEN attr='ImageSize' THEN "
				     "cast(val as integer) ELSE NULL END), "
				"max(CASE WHEN attr='JobStatus' THEN "
				     "cast(val as integer) ELSE NULL END), "
				"max(CASE WHEN attr='JobPrio' THEN "
				     "cast(val as integer) ELSE NULL END), "
				"max(CASE WHEN attr='Cmd' THEN val ELSE NULL END), "
				"max(CASE WHEN attr='CompletionDate' THEN "
				     "cast(val as integer) ELSE NULL END), "
				"max(CASE WHEN attr='LastRemoteHost' THEN val ELSE NULL END) "
				"FROM "
				"(SELECT cid,pid,attr,val FROM ProcAds WHERE cid=%s AND pid=%s "
				"UNION ALL "
				"SELECT cid,%s,attr,val FROM ClusterAds "
				"WHERE cid=%s AND attr NOT IN "
				"(SELECT attr FROM procads WHERE cid =%s AND pid =%s)) as T "
				"GROUP BY cid,pid;"
				,cid,pid,cid,pid,pid,cid,cid,pid); 

		inserthistory = true;

			//now that we've inserted rows into history, we can safely 
			//delete them from the procads tables
		sprintf(sql_str1, 
				"DELETE FROM ProcAds_Str WHERE cid = %s AND pid = %s;", 
				cid, pid);
    
		sprintf(sql_str2, 
				"DELETE FROM ProcAds_Num WHERE cid = %s AND pid = %s;", 
				cid, pid);
		break;
	case IS_UNKNOWN_ID:
		dprintf(D_ALWAYS, "[QUILL] Destroy ClassAd --- ERROR\n");
		return QUILL_FAILURE; 
		break;
	}
  
	if (exec_later == false) {

		if(inserthistory) { 
			/*  
				we can't afford to ignore the return values st1 and st2
				because in rare cases, we may be in the midst of a 
				xact and if either of these return values are QUILL_FAILURE, 
				we'd throw errors ad infinitum. Usually these failures 
				are caused by integrity violations caused when trying
				to insert jobs in the history tables when on with the 
				same id already exists (possibly from a previous instance 
				of quill). Our reaction to such failures, as implemented 
				below is to roll back the erroneous transaction and 
				proceed with processing the job queue file
			*/
			st1 = DBObj->execCommand(sql_str3);
			if(st1 == QUILL_FAILURE && xactState == BEGIN_XACT) {
				st3 = DBObj->rollbackTransaction();
				if(st3 == QUILL_FAILURE) {
					return QUILL_FAILURE;
				}
			}
			st2 = DBObj->execCommand(sql_str4);
			if(st2 == QUILL_FAILURE && xactState == BEGIN_XACT) {
				st3 = DBObj->rollbackTransaction();
				if(st3 == QUILL_FAILURE) {
					return QUILL_FAILURE;
				}
			}
			if((xactState == BEGIN_XACT) && 
			   (st1 == QUILL_FAILURE || st2 == QUILL_FAILURE)) {
				st3 = processBeginTransaction();
				if(st3 == QUILL_FAILURE) {
					return QUILL_FAILURE;
				}
			}
		}

		if (DBObj->execCommand(sql_str1) == QUILL_FAILURE) {
			displayDBErrorMsg("Destroy ClassAd Processing --- ERROR");
			return QUILL_FAILURE; 
		}
		if (DBObj->execCommand(sql_str2) == QUILL_FAILURE) {
			displayDBErrorMsg("Destroy ClassAd Processing --- ERROR");
			return QUILL_FAILURE; 
		}
	}
	else {
		if (multi_sql_str != NULL) {
			multi_sql_str = (char*)realloc(multi_sql_str, 
										   strlen(multi_sql_str) + 
										   strlen(sql_str1) + 
										   strlen(sql_str2) + 1);
			if(!multi_sql_str) {
				EXCEPT("Call to realloc failed\n");
			}
			strcat(multi_sql_str, sql_str1);
			strcat(multi_sql_str, sql_str2);
		}
		else {
			multi_sql_str = (char*)malloc(strlen(sql_str1) + 
										  strlen(sql_str2) + 1);
			sprintf(multi_sql_str, "%s%s", sql_str1, sql_str2);
		}
	}
  
	return QUILL_SUCCESS;
}

/*! process SetAttribute command, working with DBMS
 *  \param key key
 *  \param name attribute name
 *  \param value attribute value
 *  \param exec_later send SQL into RDBMS now or not?
 *  \return the result status
 *
 *	Note:
 *	Because this is not just update, but set. So, we need to delete and insert
 *  it.  We twiddled with an alternative way to do it (using NOT EXISTS) but
 *  found out that DELETE/INSERT works as efficiently.  
 */
QuillErrCode 
JobQueueDBManager::processSetAttribute(char* key, 
									   char* name, 
									   char* value, 
									   bool exec_later)
{
	char* sql_str_del_in;

	char  cid[512];
	char  pid[512];
	int   job_id_type;
	char* endptr;
	char* newvalue;  
	double doubleval = 0;

	if ( ClassAd::ClassAdAttributeIsPrivate(name) ) {
		return SUCCESS;
	}
	newvalue = fillEscapeCharacters(value);
		// It could be ProcAd or ClusterAd
		// So need to check
	job_id_type = getProcClusterIds(key, cid, pid);

	doubleval = strtod(value, &endptr);

	int sql_str_len = 2 * strlen(name) + (strlen(newvalue) + MAX_FIXED_SQL_STR_LENGTH);
	sql_str_del_in = (char *) malloc(sql_str_len * sizeof(char));
	memset(sql_str_del_in, 0, sql_str_len);
	
	switch(job_id_type) {
	case IS_CLUSTER_ID:
		if(value == endptr) {
			sprintf(sql_str_del_in,
					"DELETE FROM ClusterAds_Str WHERE cid = %s "
					"AND attr = '%s'; INSERT INTO ClusterAds_Str "
					"(cid, attr, val) VALUES (%s, '%s', '%s');",
					cid, name, cid, name, newvalue);
		}         
		
		else {
			if(*endptr != '\0') {  
				sprintf(sql_str_del_in,
						"DELETE FROM ClusterAds_Str WHERE cid = %s "
						"AND attr = '%s'; INSERT INTO ClusterAds_Str "
						"(cid, attr, val) VALUES (%s, '%s', '%s');",
						cid, name, cid, name, newvalue);
			}
			else {
				sprintf(sql_str_del_in,
						"DELETE FROM ClusterAds_Num WHERE cid = %s "
						"AND attr = '%s'; INSERT INTO ClusterAds_Num "
						"(cid, attr, val) VALUES (%s, '%s', %s);",
						cid, name, cid, name, value);
			}
		}         
		
		break;
		
	case IS_PROC_ID:
		if(value == endptr) {
			sprintf(sql_str_del_in,
					"DELETE FROM ProcAds_Str WHERE cid = %s AND "
					"pid = %s AND attr = '%s'; "
					"DELETE FROM ProcAds_Num WHERE cid = %s AND "
					"pid = %s AND attr = '%s'; "
					"INSERT INTO ProcAds_Str"
					"(cid, pid, attr, val) VALUES (%s, %s, '%s', '%s');",
					cid, pid, name, cid, pid, name, cid, pid, name, newvalue);
		}
		
		else {
			if(*endptr != '\0') {
				sprintf(sql_str_del_in,
						"DELETE FROM ProcAds_Str WHERE cid = %s AND "
						"pid = %s AND attr = '%s'; "
						"DELETE FROM ProcAds_Num WHERE cid = %s AND "
						"pid = %s AND attr = '%s'; "
						"INSERT INTO ProcAds_Str"
						"(cid, pid, attr, val) VALUES (%s, %s, '%s', '%s');",
						cid, pid, name, cid, pid, name, cid, pid, name, newvalue);
			}
			else {
				sprintf(sql_str_del_in,
						"DELETE FROM ProcAds_Num WHERE cid = %s AND "
						"pid = %s AND attr = '%s'; "
						"DELETE FROM ProcAds_Str WHERE cid = %s AND "
						"pid = %s AND attr = '%s'; "
						"INSERT INTO ProcAds_Num"
						"(cid, pid, attr, val) VALUES (%s, %s, '%s', %s);",
						cid, pid, name, cid, pid, name, cid, pid, name, value);
			}
		}
		break;    
		
	case IS_UNKNOWN_ID:
		dprintf(D_ALWAYS, "Set Attribute Processing --- ERROR\n");
		if(sql_str_del_in) {
			free(sql_str_del_in);
			sql_str_del_in = NULL;
		}
		if(newvalue) {
			free(newvalue);
			newvalue = NULL;
		}
		return QUILL_FAILURE;
		break;
	}
  
	QuillErrCode ret_st;

	if (exec_later == false) {
		ret_st = DBObj->execCommand(sql_str_del_in);

		if (ret_st == QUILL_FAILURE) {
			dprintf(D_ALWAYS, "Set Attribute --- Error [SQL] %s\n", 
					sql_str_del_in);
			displayDBErrorMsg("Set Attribute --- ERROR");     
 			if(sql_str_del_in) {
				free(sql_str_del_in);
				sql_str_del_in = NULL;
			}
			if(newvalue) {
				free(newvalue);
				newvalue = NULL;
			}
			return QUILL_FAILURE;
		}
	}
	else {
		if (multi_sql_str != NULL) {
				// NOTE:
				// this case is not trivial 
				// because there could be multiple insert
				// statements.
			multi_sql_str = (char*)realloc(multi_sql_str, 
										   strlen(multi_sql_str) + 
										   strlen(sql_str_del_in) + 1);
			if(!multi_sql_str) {
				EXCEPT("Call to realloc failed\n");
			}
			strcat(multi_sql_str, sql_str_del_in);
		}
		else {
			multi_sql_str = (char*)malloc(strlen(sql_str_del_in) + 1);
			strcpy(multi_sql_str, sql_str_del_in);
		}    
	}
  
	if(newvalue) {
		free(newvalue);
		newvalue = NULL;
	}
	if(sql_str_del_in) {
		free(sql_str_del_in);
		sql_str_del_in = NULL;
	}
	return QUILL_SUCCESS;
}


/*! process DeleteAttribute command, working with DBMS
 *  \param key key
 *  \param name attribute name
 *  \param exec_later send SQL into RDBMS now or not?
 *  \return the result status
 */
QuillErrCode 
JobQueueDBManager::processDeleteAttribute(char* key, 
										  char* name, 
										  bool exec_later)
{
	char sql_str1[MAX_FIXED_SQL_STR_LENGTH];
	char sql_str2[MAX_FIXED_SQL_STR_LENGTH];
	char cid[512];
	char pid[512];
	int  job_id_type;
	QuillErrCode	 ret_st;
	int num_result=0, db_err_code=0;

	memset(sql_str1, 0, MAX_FIXED_SQL_STR_LENGTH);
	memset(sql_str2, 0, MAX_FIXED_SQL_STR_LENGTH);

		// It could be ProcAd or ClusterAd
		// So need to check
	job_id_type = getProcClusterIds(key, cid, pid);

	switch(job_id_type) {
	case IS_CLUSTER_ID:
		sprintf(sql_str1, 
				"DELETE FROM ClusterAds_Str WHERE cid = %s AND attr = '%s';", 
				cid, name);
		sprintf(sql_str2, 
				"DELETE FROM ClusterAds_Num WHERE cid = %s AND attr = '%s';", 
				cid, name);

		break;
	case IS_PROC_ID:
		sprintf(sql_str1, 
				"DELETE FROM ProcAds_Str WHERE cid = %s AND pid = %s "
				"AND attr = '%s';", 
				cid, pid, name);
		sprintf(sql_str2, 
				"DELETE FROM ProcAds_Num WHERE cid = %s AND pid = %s AND "
				"attr = '%s';", 
				cid, pid, name);

		break;
	case IS_UNKNOWN_ID:
		dprintf(D_ALWAYS, "Delete Attribute Processing --- ERROR\n");
		return QUILL_FAILURE;
		break;
	}

	if (sql_str1 != NULL && sql_str2 != NULL) {
		if (exec_later == false) {
			ret_st = DBObj->execCommand(sql_str1, num_result, db_err_code);
		
			if (ret_st == QUILL_FAILURE) {
				dprintf(D_ALWAYS, "Delete Attribute --- ERROR, [SQL] %s\n", 
						sql_str1);
				displayDBErrorMsg("Delete Attribute --- ERROR");
				return QUILL_FAILURE;
			}
			else if (ret_st == QUILL_SUCCESS && num_result == 0) {
				ret_st = DBObj->execCommand(sql_str2);
			
				if (ret_st == QUILL_FAILURE) {
					dprintf(D_ALWAYS, "Delete Attribute --- ERROR [SQL] %s\n",
							sql_str2);
					displayDBErrorMsg("Delete Attribute --- ERROR");
					return QUILL_FAILURE;
				}
			}
		}
		else {
			if (multi_sql_str != NULL) {
				multi_sql_str = (char*)realloc(multi_sql_str, 
											   strlen(multi_sql_str) + 
											   strlen(sql_str1) + 
											   strlen(sql_str2) + 1);
				if(!multi_sql_str) {
					EXCEPT("Call to realloc failed\n");
				}

				strcat(multi_sql_str, sql_str1);
				strcat(multi_sql_str, sql_str2);
			}
			else {
				multi_sql_str = (char*)malloc(strlen(sql_str1) + 
											  strlen(sql_str2) + 1);
				sprintf(multi_sql_str, "%s%s", sql_str1, sql_str2);
			}
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
	if(!exec_later) {
			// the connection is not in Xact by default, so begin it
		if (DBObj->beginTransaction() == QUILL_FAILURE) { 
			return QUILL_FAILURE;			   				   
		}
	}
	xactState = BEGIN_XACT;
	return QUILL_SUCCESS;
}

/*! process EndTransaction command
 *  \return the result status
 */
QuillErrCode
JobQueueDBManager::processEndTransaction(bool exec_later)
{
	if(!exec_later) {		
		if (DBObj->commitTransaction() == QUILL_FAILURE) {
			return QUILL_FAILURE;			   				   
		}
	}
	xactState = COMMIT_XACT;
	return QUILL_SUCCESS;
}

//! initialize: currently check the DB schema
/*! \param initJQDB initialize DB?
 */
QuillErrCode
JobQueueDBManager::init(bool initJQDB)
{
	QuillErrCode st;
	FileOpErrCode fst;

	fst = caLogParser->openFile();
	if(fst == FILE_OPEN_ERROR) {
		return QUILL_FAILURE;
	}

	st = checkSchema();

	if (initJQDB == true) { // initialize Job Queue DB
		if (st == QUILL_FAILURE) {
			return QUILL_FAILURE;
		}

		prober->probe(caLogParser->getCurCALogEntry(),caLogParser->getFileDescriptor());	// polling
		st = initJobQueueTables();
		caLogParser->closeFile();
	}

	return st;
}


//! get Last Job Queue File Polling Information
QuillErrCode
JobQueueDBManager::getJQPollingInfo()
{
	long mtime;
	long size;
	long int seq_num;
	long int creation_time;

	ClassAdLogEntry* lcmd;
	char 	sql_str[MAX_FIXED_SQL_STR_LENGTH];
	int		ret_st, num_result=0;

	dprintf(D_FULLDEBUG, "Get JobQueue Polling Information\n");

	memset(sql_str, 0, MAX_FIXED_SQL_STR_LENGTH);

	lcmd = caLogParser->getCurCALogEntry();

	sprintf(sql_str, 
			"SELECT last_file_mtime, last_file_size, last_next_cmd_offset, "
			"last_cmd_offset, last_cmd_type, last_cmd_key, last_cmd_mytype,"
			"last_cmd_targettype, last_cmd_name, last_cmd_value,"
			"log_seq_num, log_creation_time FROM "
			"JobQueuePollingInfo;");
	
		// connect to DB
	ret_st = connectDB(NOT_IN_XACT);

	if(ret_st == QUILL_FAILURE) {
		return QUILL_FAILURE;
	}

	ret_st = DBObj->execQuery(sql_str, num_result);
	
	if (ret_st == QUILL_FAILURE) {
		dprintf(D_ALWAYS, "Reading JobQueuePollingInfo --- ERROR [SQL] %s\n", 
				sql_str);
		displayDBErrorMsg("Reading JobQueuePollingInfo --- ERROR");
		disconnectDB(NOT_IN_XACT);
		return QUILL_FAILURE;
	}
	else if (ret_st == QUILL_SUCCESS && num_result == 0) {
			// This case is a rare one since the jobqueuepollinginfo
			// table contains one tuple at all times 
		
		displayDBErrorMsg("Reading JobQueuePollingInfo --- ERROR "
						  "No Rows Retrieved from JobQueuePollingInfo\n");
		disconnectDB(NOT_IN_XACT);
		return QUILL_FAILURE;
	} 
	
	mtime = atol(DBObj->getValue(0,0)); // last_file_mtime
	size = atol(DBObj->getValue(0,1)); // last_file_size
	seq_num = atol(DBObj->getValue(0,10)); //historical sequence number
	creation_time = atol(DBObj->getValue(0,11)); //creation time

	prober->setLastModifiedTime(mtime);
	prober->setLastSize(size);
	prober->setLastSequenceNumber(seq_num);
	prober->setLastCreationTime(creation_time);

	lcmd->next_offset = atoi(DBObj->getValue(0,2)); // last_next_cmd_offset
	lcmd->offset = atoi(DBObj->getValue(0,3)); // last_cmd_offset
	lcmd->op_type = atoi(DBObj->getValue(0,4)); // last_cmd_type
	
	if(lcmd->key) { 
		free(lcmd->key);
	}
	if(lcmd->mytype) {
		free(lcmd->mytype);
	}
	if(lcmd->targettype) {
		free(lcmd->targettype);
	}
	if(lcmd->name) {
		free(lcmd->name);
	}
	if(lcmd->value) {
		free(lcmd->value);
	}
	
	lcmd->key = strdup(DBObj->getValue(0,5)); // last_cmd_key
	lcmd->mytype = strdup(DBObj->getValue(0,6)); // last_cmd_mytype
	lcmd->targettype = strdup(DBObj->getValue(0,7)); // last_cmd_targettype
	lcmd->name = strdup(DBObj->getValue(0,8)); // last_cmd_name
	lcmd->value = strdup(DBObj->getValue(0,9)); // last_cmd_value
	
		// disconnect to DB
	disconnectDB(NOT_IN_XACT);
	
		// release Query Result since it is no longer needed
	DBObj->releaseQueryResult(); 
	
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
/*! \warning This method must be called between connectDB and disconnectDB
 *           which means this method doesn't invoke thoses two methods
 */
QuillErrCode
JobQueueDBManager::setJQPollingInfo()
{
	long mtime;
	long size;
	long int seq_num;
	long int creation_time;

	ClassAdLogEntry* lcmd;
	char 	       *sql_str;
	QuillErrCode   ret_st;
	int            num_result=0, db_err_code=0;

	prober->incrementProbeInfo();
	mtime = prober->getLastModifiedTime();
	size = prober->getLastSize();
	seq_num = prober->getLastSequenceNumber();
	creation_time = prober->getLastCreationTime();
	lcmd = caLogParser->getCurCALogEntry();	
	
	int sql_str_len = (sizeof(lcmd->value) + MAX_FIXED_SQL_STR_LENGTH);
	sql_str = (char *) malloc(sql_str_len * sizeof(char));
	memset(sql_str, 0, sql_str_len);

	sprintf(sql_str, 
			"UPDATE JobQueuePollingInfo SET last_file_mtime = %ld, "
			"last_file_size = %ld, last_next_cmd_offset = %ld, "
			"last_cmd_offset = %ld, last_cmd_type = %d, "
			"log_seq_num = %ld, log_creation_time = %ld",
			mtime, size, lcmd->next_offset, lcmd->offset, lcmd->op_type,
			seq_num, creation_time);

	addJQPollingInfoSQL(sql_str, "last_cmd_key", lcmd->key);
	addJQPollingInfoSQL(sql_str, "last_cmd_mytype", lcmd->mytype);
	addJQPollingInfoSQL(sql_str, "last_cmd_targettype", lcmd->targettype);
	addJQPollingInfoSQL(sql_str, "last_cmd_name", lcmd->name);
	addJQPollingInfoSQL(sql_str, "last_cmd_value", lcmd->value);
	
	strcat(sql_str, ";");
	
	ret_st = DBObj->execCommand(sql_str, num_result, db_err_code);
	
	if (ret_st == QUILL_FAILURE) {
		dprintf(D_ALWAYS, "Update JobQueuePollingInfo --- ERROR [SQL] %s\n", 
				sql_str);
		displayDBErrorMsg("Update JobQueuePollingInfo --- ERROR");
	}
	else if (ret_st == QUILL_SUCCESS && num_result == 0) {
			// This case is a rare one since the jobqueuepollinginfo
			// table contains one tuple at all times 
		
		dprintf(D_ALWAYS, "Update JobQueuePollingInfo --- ERROR [SQL] %s\n", 
				sql_str);
		displayDBErrorMsg("Update JobQueuePollingInfo --- ERROR");
		ret_st = QUILL_FAILURE;
	} 

	if(sql_str) {
		free(sql_str);
		sql_str = NULL;
	}
	return ret_st;
}

/*! check the DB schema
 *  \return the result status QUILL_FAILURE/QUILL_SUCCESS
 */
QuillErrCode
JobQueueDBManager::checkSchema()
{
	char 	sql_str[MAX_FIXED_SQL_STR_LENGTH]; 
	
	int num_result=0;
	QuillErrCode ret_st, tmp_st;

		//
		// DB schema check done here using the following steps:
		//
		// 1. check the number of tables 
		// 2. check the list of tables
		// 3. check the one tuple of JobQueuePollingInfo table 

	ret_st = connectDB(NOT_IN_XACT);
	
		// no database or no server connection
	if(ret_st == QUILL_FAILURE) { 
		dprintf(D_ALWAYS, "Error: Unable to connect to db \"%s\"\n", 
				jobQueueDBName);

			//10 related to strlen("template1")
		char *tmp_conn = (char *) malloc(strlen(jobQueueDBConn) + 10); 
		
		strcpy(tmp_conn,jobQueueDBConn);
		char *tmp_found = strstr(tmp_conn, "dbname=");
		strcpy(tmp_found, "dbname=template1");

		/* Comment this out because it prints out the password to LOG. 
			Do a better job for next revision of Condor. */
/*		dprintf(D_ALWAYS, "tmp = %s\n", tmp_conn);	  */
		dprintf(D_FULLDEBUG, "Temporary connection to DB created\n");

		sprintf(sql_str, "CREATE DATABASE \"%s\"", jobQueueDBName);
		Database *tmp_db = new PGSQLDatabase(tmp_conn);

		tmp_st = tmp_db->connectDB(tmp_conn);
	  
		if (tmp_st == QUILL_FAILURE) { // connect to template1 databae
			dprintf(D_ALWAYS, "Error: Failed while trying to create "
					"database %s.\n", 
					jobQueueDBName);
			delete tmp_db;
			free(tmp_conn);
			tmp_conn = NULL;
			return QUILL_FAILURE;
		}
		tmp_st = tmp_db->execCommand(sql_str);
		if (tmp_st == QUILL_FAILURE) { // executing the create command
			dprintf(D_ALWAYS, "Error: Failed while trying to create "
					"database %s.\n", 
					jobQueueDBName);
			delete tmp_db;
			free(tmp_conn);
			tmp_conn = NULL;
			return QUILL_FAILURE;
		}
		tmp_db->disconnectDB();
		delete tmp_db;
		free(tmp_conn);	  
		tmp_conn = NULL;
	}

	strcpy(sql_str, SCHEMA_CHECK_STR); 
		// SCHEMA_CHECK_STR is defined in quill_dbschema_def.h

		// execute DB schema check!
	ret_st = DBObj->execQuery(sql_str, num_result);
	DBObj->releaseQueryResult();

	if (ret_st == QUILL_SUCCESS && num_result == SCHEMA_SYS_TABLE_NUM) {
		dprintf(D_ALWAYS, "Schema Check OK!\n");
	}
	
		// Schema is not defined in DB
	else if (ret_st == QUILL_SUCCESS && num_result != SCHEMA_SYS_TABLE_NUM) { 

		dprintf(D_ALWAYS, "Schema is not defined!\n");
		dprintf(D_ALWAYS, "Create DB Schema for quill!\n");
			// this conn is not in Xact so begin transaction here
		if (DBObj->beginTransaction() == QUILL_FAILURE) {			
			return QUILL_FAILURE;			   				 
		}
			//
			// Here, Create DB Schema:
			//
		strcpy(sql_str, SCHEMA_CREATE_PROCADS_TABLE_STR);
		ret_st = DBObj->execCommand(sql_str);
		if(ret_st == QUILL_FAILURE) {
			disconnectDB(ABORT_XACT);
			return QUILL_FAILURE;
		}

		strcpy(sql_str, SCHEMA_CREATE_CLUSTERADS_TABLE_STR);
		ret_st = DBObj->execCommand(sql_str);
		if(ret_st == QUILL_FAILURE) {
			disconnectDB(ABORT_XACT);
			return QUILL_FAILURE;
		}

		strcpy(sql_str, SCHEMA_CREATE_HISTORY_TABLE_STR);
		ret_st = DBObj->execCommand(sql_str);
		if(ret_st == QUILL_FAILURE) {
			disconnectDB(ABORT_XACT);
			return QUILL_FAILURE;
		}


		strcpy(sql_str, SCHEMA_CREATE_JOBQUEUEPOLLINGINFO_TABLE_STR);
		ret_st = DBObj->execCommand(sql_str);
		if(ret_st == QUILL_FAILURE) {
			disconnectDB(ABORT_XACT);
			return QUILL_FAILURE;
		}

		strcpy(sql_str, SCHEMA_CREATE_SCHEMA_VERSION_TABLE_STR);
		ret_st = DBObj->execCommand(sql_str);
		if(ret_st == QUILL_FAILURE) {
			disconnectDB(ABORT_XACT);
			return QUILL_FAILURE;
		}

		if (DBObj->commitTransaction() == QUILL_FAILURE) {			
			// TODO: did this just leak transactionResults?
			disconnectDB(ABORT_XACT);
			return QUILL_FAILURE;			   				 
		}
	}
	else { // Unknown error
		dprintf(D_ALWAYS, "Schema Check Unknown Error!\n");
		disconnectDB(NOT_IN_XACT);

		return QUILL_FAILURE;
	}

	strcpy(sql_str, SCHEMA_VERSION_STR); 
		// SCHEMA_VERSION_STR is defined in quill_dbschema_def.h

	ret_st = DBObj->execQuery(sql_str, num_result);

	if (ret_st == QUILL_SUCCESS && num_result == SCHEMA_VERSION_COUNT) {
		const char *tmp_ptr1, *tmp_ptr2, *tmp_ptr3, *tmp_ptr4;

		int major, minor, back_to_major, back_to_minor;
		
		tmp_ptr1 = DBObj->getValue(0,0);
		tmp_ptr2 = DBObj->getValue(0,1);
		tmp_ptr3 = DBObj->getValue(0,2);
		tmp_ptr4 = DBObj->getValue(0,3);

		if  (!tmp_ptr1 || !tmp_ptr2 || !tmp_ptr3 || !tmp_ptr4) {
			DBObj->releaseQueryResult();
			return QUILL_FAILURE;
		}
		
		major = atoi(tmp_ptr1);		
		minor = atoi(tmp_ptr2);
		back_to_major = atoi(tmp_ptr3);
		back_to_minor = atoi(tmp_ptr4);
		
		dprintf(D_ALWAYS, "Schema Version Check OK!\n");

			// free the query result because we dont need it anymore
		DBObj->releaseQueryResult();

		if ((major == 1 ) && (minor == 0) && (back_to_major == 1) && 
			(back_to_minor == 0)) {
				// we need to upgrade the schema to add a last_poll_time
				// column into the jobqueuepollinginfo table
			strcpy(sql_str, SCHEMA_CREATE_JOBQUEUEPOLLINGINFO_UPDATE1);
			ret_st = DBObj->execCommand(sql_str);
			if(ret_st == QUILL_FAILURE) {
				disconnectDB(NOT_IN_XACT);
				return QUILL_FAILURE;
			}

				// and also update the minor in schema version			
			if (DBObj->beginTransaction() == QUILL_FAILURE) {			
				return QUILL_FAILURE;			   				 
			}
			
			strcpy(sql_str, SCHEMA_VERSION_TABLE_UPDATE);
			ret_st = DBObj->execCommand(sql_str);
			if(ret_st == QUILL_FAILURE) {
				disconnectDB(ABORT_XACT);
				return QUILL_FAILURE;
			}
			if (DBObj->commitTransaction() == QUILL_FAILURE) {
				disconnectDB(ABORT_XACT);
				return QUILL_FAILURE;						
			}
		}
		// this would be a good place to check if the version matchedup
		// instead of just seeing if we got something back
	} else {
		// if for some reason its not freed above already
		DBObj->releaseQueryResult();

		// create a schema version table. this is cut and paste from
		// above, so revamp this once it's working
		if (DBObj->beginTransaction() == QUILL_FAILURE) {			
			return QUILL_FAILURE;			   				 
		}
		strcpy(sql_str, SCHEMA_CREATE_SCHEMA_VERSION_TABLE_STR);
		ret_st = DBObj->execCommand(sql_str);
		if(ret_st == QUILL_FAILURE) {
			disconnectDB(ABORT_XACT);
			return QUILL_FAILURE;
		}

		if (DBObj->commitTransaction() == QUILL_FAILURE) {			
			// TODO: did this just leak transactionResults?
			disconnectDB(ABORT_XACT);
			return QUILL_FAILURE;			   				 
		}
	}
	disconnectDB(NOT_IN_XACT);
	return QUILL_SUCCESS;
}


//! register all timer and command handlers
void
JobQueueDBManager::registerAll()
{
	registerCommands();
	registerTimers();
}

//! register all command handlers
void
JobQueueDBManager::registerCommands()
{
	daemonCore->Cancel_Command(QMGMT_READ_CMD);

		// register a handler for QMGMT_READ_CMD command from condor_q
	daemonCore->Register_Command(
						   QMGMT_READ_CMD, 
						   "QMGMT_READ_CMD",
						   (CommandHandlercpp)&JobQueueDBManager::handle_q,
						   "JobQueueDBManager::handle_q", 
						   this, 
						   READ, 
						   D_FULLDEBUG);
}

//! register all timer handlers
void
JobQueueDBManager::registerTimers()
{
		// clear previous timers
	if (pollingTimeId >= 0) {
		daemonCore->Cancel_Timer(pollingTimeId);
		pollingTimeId = -1;
	}
	if (purgeHistoryTimeId >= 0) {
		daemonCore->Cancel_Timer(purgeHistoryTimeId);
		purgeHistoryTimeId = -1;
	}

	if (reindexTimeId >= 0) {
		daemonCore->Cancel_Timer(reindexTimeId);
		reindexTimeId = -1;
	}

		// register timer handlers
	pollingTimeId = daemonCore->Register_Timer(
								  0, 
								  pollingPeriod,
								  (TimerHandlercpp)&JobQueueDBManager::pollingTime, 
								  "JobQueueDBManager::pollingTime", this);
		//historyCleaningInterval is specified in units of hours so
		//we multiply it by 3600 to get the # of seconds 
	purgeHistoryTimeId = daemonCore->Register_Timer(
						   5, 
						   historyCleaningInterval *3600,
						   (TimerHandlercpp)&JobQueueDBManager::purgeOldHistoryRows, 
						   "JobQueueDBManager::purgeOldHistoryRows", 
						   this);
	reindexTimeId = daemonCore->Register_Timer(
						   10,
								  (TimerHandlercpp)&JobQueueDBManager::reindexTables, 
								  "JobQueueDBManager::reindexTables", this);
}


//! update the QUILL_AD sent to the collector
/*! This method only updates the ad with new values of dynamic attributes
 *  See createQuillAd for how to create the ad in the first place
 */

void JobQueueDBManager::updateQuillAd(void) {
	char expr[1000];

	sprintf( expr, "%s = %d", ATTR_QUILL_SQL_LAST_BATCH, 
			 lastBatchSqlProcessed);
	ad->Insert(expr);

	sprintf( expr, "%s = %d", ATTR_QUILL_SQL_TOTAL, 
			 totalSqlProcessed);
	ad->Insert(expr);

	sprintf( expr, "%s = %d", "TimeToProcessLastBatch", 
			 secsLastBatch);
	ad->Insert(expr);

	sprintf( expr, "%s = %d", "IsConnectedToDB", 
			 isConnectedToDB);
	ad->Insert(expr);
}

//! create the QUILL_AD sent to the collector
/*! This method reads all quill-related configuration options from the 
 *  config file and creates a classad which can be sent to the collector
 */

void JobQueueDBManager::createQuillAd(void) {
	char expr[1000];

	char *scheddName;

	char *mysockname;
	char *tmp;

	ad = new ClassAd();
	ad->SetMyTypeName(QUILL_ADTYPE);
	ad->SetTargetTypeName("");
  
		// Publish all DaemonCore-specific attributes, which also handles
        // QUILL_ATTRS for us.
    daemonCore->publish(ad);

		// schedd info is used to identify the schedd 
		// corresponding to this quill 

	tmp = param( "SCHEDD_NAME" );
	if( tmp ) {
		scheddName = build_valid_daemon_name( tmp );
	} else {
		scheddName = default_daemon_name();
	}

	char *quill_name = param("QUILL_NAME");
	if(!quill_name) {
		EXCEPT("Cannot find variable QUILL_NAME in config file\n");
	}

	if (param_boolean("QUILL_IS_REMOTELY_QUERYABLE", true) == true) {
		sprintf( expr, "%s = TRUE", ATTR_QUILL_IS_REMOTELY_QUERYABLE);
	} else {
		sprintf( expr, "%s = FALSE", ATTR_QUILL_IS_REMOTELY_QUERYABLE);
	}
	ad->Insert(expr);

	sprintf( expr, "%s = %d", "QuillPollingPeriod", pollingPeriod );
	ad->Insert(expr);

	char *quill_query_passwd = param("QUILL_DB_QUERY_PASSWORD");
	if(!quill_query_passwd) {
		EXCEPT("Cannot find variable QUILL_DB_QUERY_PASSWORD "
			   "in config file\n");
	}
  
	sprintf( expr, "%s = \"%s\"", ATTR_QUILL_DB_QUERY_PASSWORD, 
			 quill_query_passwd );
	ad->Insert(expr);

	sprintf( expr, "%s = \"%s\"", ATTR_NAME, quill_name );
	ad->Insert(expr);

	sprintf( expr, "%s = \"%s\"", ATTR_SCHEDD_NAME, scheddName );
	ad->Insert(expr);

	if(scheddName) {
		delete [] scheddName;
	}

  
		// Put in our sinful string.  Note, this is never going to
		// change, so we only need to initialize it once.
	mysockname = strdup( daemonCore->InfoCommandSinfulString() );

	sprintf( expr, "%s = \"%s\"", ATTR_MY_ADDRESS, mysockname );
	ad->Insert(expr);

	sprintf( expr, "%s = \"<%s>\"", ATTR_QUILL_DB_IP_ADDR, 
			 jobQueueDBIpAddress );
	ad->Insert(expr);

	sprintf( expr, "%s = \"%s\"", ATTR_QUILL_DB_NAME, jobQueueDBName );
	ad->Insert(expr);

	if(tmp) free(tmp);
	if(quill_query_passwd) free(quill_query_passwd);
	if(quill_name) free(quill_name);
	if(mysockname) free(mysockname);
}


//! timer handler for each polling event
void
JobQueueDBManager::pollingTime()
{
	dprintf(D_ALWAYS, "******** Start of Probing Job Queue Log File ********\n");

		/*
		  instead of exiting on error, we simply continue sending
		  ads to collector and in some cases (e.g. database connection),
		  encode that in the ad for error diagnoses

		  this means that quill will not usually exit on loss of 
		  database connectivity, and that it will keep polling
		  and trying to connect until database is back up again 
		  and then resume execution 
		*/
	if (maintain() == QUILL_FAILURE) {
		dprintf(D_ALWAYS, 
				">>>>>>>> Fail: Probing Job Queue Log File <<<<<<<<\n");
	}

	else {
		dprintf(D_ALWAYS, "********* End of Probing Job Queue Log File *********\n");
	}

	dprintf(D_ALWAYS, "++++++++ Sending schedd ad to collector ++++++++\n");

	if(!ad) {
		createQuillAd();
	}

	updateQuillAd();

	daemonCore->sendUpdates ( UPDATE_QUILL_AD, ad, NULL, true );
	
	dprintf(D_ALWAYS, "++++++++ Sent schedd ad to collector ++++++++\n");
}

//! command handler for QMGMT_READ_CMD command from condor_q
/*!
 *  Much portion of this code was borrowed from handle_q in qmgmt.C which is
 *  part of schedd source code.
 */
int
JobQueueDBManager::handle_q(int, Stream* sock) {
//	---------------- Start of Borrowed Code -------------------
	QuillErrCode ret_st;
	
	dprintf(D_ALWAYS, "******** Start of Handling condor_q query ********\n");
	
	RequestService *rs = new RequestService(jobQueueDBConn);
	
    do {
        ret_st = rs->service((ReliSock *)sock);
    } while(ret_st != QUILL_FAILURE);
	

//	---------------- End of Borrowed Code -------------------

	delete rs;

	dprintf(D_ALWAYS, "******** End   of Handling condor_q query ********\n");

	return TRUE;
}

char * 
JobQueueDBManager::fillEscapeCharacters(char * str) {
	int i, j;
	
	int len = strlen(str);

		// here we allocate for the worst case -- every byte going to the
		// database needs to be escaped, except the trailing null
	char *newstr = (char *) malloc(( 2 * len + 1) * sizeof(char));
	
	j = 0;
	for (i = 0; i < len; i++) {
		switch(str[i]) {
        case '\\':
            newstr[j] = '\\';
            newstr[j+1] = '\\';
            j += 2;
            break;
        case '\'':
            newstr[j] = '\\';
            newstr[j+1] = '\'';
            j += 2;
            break;
        default:
            newstr[j] = str[i];
            j++;
            break;
		}
	}
	newstr[j] = '\0';
    return newstr;
}

