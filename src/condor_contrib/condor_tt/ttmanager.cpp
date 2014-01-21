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
#include "get_daemon_name.h"
#include "condor_attributes.h"
#include "condor_event.h"
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <time.h>
#include "condor_md.h"

#include "jobqueuedbmanager.h"
#include "ttmanager.h"
#include "file_sql.h"
#include "file_xml.h"
#include "jobqueuedatabase.h"
#include "pgsqldatabase.h"
#include "misc_utils.h"
#include "condor_ttdb.h"
#include "dbms_utils.h"
#include "ipv6_hostname.h"

char logParamList[][30] = {"NEGOTIATOR_SQLLOG", "SCHEDD_SQLLOG", 
						   "SHADOW_SQLLOG", "STARTER_SQLLOG", 
						   "STARTD_SQLLOG", "SUBMIT_SQLLOG", 
						   "COLLECTOR_SQLLOG", ""};

#define CONDOR_TT_FILESIZELIMT 1900000000L
#define CONDOR_TT_THROWFILE numLogs
#define CONDOR_TT_EVENTTYPEMAXLEN 100
#define CONDOR_TT_TIMELEN 60

static unsigned attHashFunction (const MyString &str);
static int file_checksum(char *filePathName, int fileSize, char *sum);
static QuillErrCode append(char *destF, char *srcF);

//! constructor
TTManager::TTManager()
{
		//nothing here...its all done in config()
	DBObj = (JobQueueDatabase  *) 0;
}

//! destructor
TTManager::~TTManager()
{
	if (collectors) {
		delete collectors;
	}
}

void
TTManager::config(bool reconfig) 
{
	char *tmp;
	int   i = 0, j = 0;
	int   found = 0;

	numLogs = 0;

	pollingTimeId = -1;
	totalSqlProcessed = 0;
	lastBatchSqlProcessed = 0;

		/* check all possible log parameters */
	while (logParamList[i][0] != '\0') {
		tmp = param(logParamList[i]);
		if (tmp) {
			
			found = 0;

				// check if the new log file is already in the list
			for (j = 0 ; j < numLogs; j++) {
				if (strcmp(tmp, sqlLogList[j]) == 0) {
					found = 1;
					break;
				}
			}

			if (found) {
				free(tmp);
				i++;
				continue;
			}
				
			strncpy(sqlLogList[numLogs], tmp, CONDOR_TT_MAXLOGPATHLEN-5);
			snprintf(sqlLogCopyList[numLogs], CONDOR_TT_MAXLOGPATHLEN, 
					 "%s.copy", sqlLogList[numLogs]);
			numLogs++;
			free(tmp);
		}		
		i++;
	}

		/* add the default log file in case no log file is specified in 
		   config 
		*/
	tmp = param("LOG");
	if (tmp) {
		snprintf(sqlLogList[numLogs], CONDOR_TT_MAXLOGPATHLEN-5, 
				 "%s/sql.log", tmp);
		snprintf(sqlLogCopyList[numLogs], CONDOR_TT_MAXLOGPATHLEN, 
				 "%s.copy", sqlLogList[numLogs]);
	} else {
		snprintf(sqlLogList[numLogs], CONDOR_TT_MAXLOGPATHLEN-5, "sql.log");
		snprintf(sqlLogCopyList[numLogs], CONDOR_TT_MAXLOGPATHLEN, 
				 "%s.copy", sqlLogList[numLogs]);
	}
	numLogs++;

		/* the "thrown" file is for recording events where big files are 
		   thrown away
		*/
	if (tmp) {
		snprintf(sqlLogCopyList[CONDOR_TT_THROWFILE], CONDOR_TT_MAXLOGPATHLEN,
				 "%s/thrown.log", tmp);
		free(tmp);
	}
	else {
		snprintf(sqlLogCopyList[CONDOR_TT_THROWFILE], CONDOR_TT_MAXLOGPATHLEN,
				 "thrown.log");
	}
	
		// read the polling period and if one is not specified use 
		// default value of 10 seconds
	pollingPeriod = param_integer("QUILL_POLLING_PERIOD", 10);
  
	dprintf(D_ALWAYS, "Using Polling Period = %d\n", pollingPeriod);
	dprintf(D_ALWAYS, "Using logs ");
	for(i=0; i < numLogs; i++)
		dprintf(D_ALWAYS, "%s ", sqlLogList[i]);
	dprintf(D_ALWAYS, "\n");

	maintain_db_conn = param_boolean("QUILL_MAINTAIN_DB_CONN", true);

	bool maintain_soft_state = param_boolean("QUILL_MAINTAIN_SOFT_STATE", true);
	AttributeCache.setMaintainSoftState(maintain_soft_state);

	jqDBManager.config(reconfig);
	jqDBManager.init();

	DBObj = jqDBManager.getJobQueueDBObj();
	dt = jqDBManager.getJobQueueDBType();
}

//! register all timer handlers
void
TTManager::registerTimers()
{
		// clear previous timers
	if (pollingTimeId >= 0)
		daemonCore->Cancel_Timer(pollingTimeId);

		// register timer handlers
	pollingTimeId = daemonCore->Register_Timer(0, 
											   pollingPeriod,
											   (TimerHandlercpp)&TTManager::pollingTime, 
											   "pollingTime", this);
}

//! timer handler for each polling event
void
TTManager::pollingTime()
{
		/*
		  instead of exiting on error, we simply return 
		  this means that tt will not usually exit on loss of 
		  database connectivity or other errors, and that it will keep polling
		  until it's successful.
		*/
	maintain();

	dprintf(D_ALWAYS, "++++++++ Sending Quill ad to collector ++++++++\n");

	if(!quillad) {
		createQuillAd();
	}

	updateQuillAd();

	collectors->sendUpdates ( UPDATE_QUILL_AD, quillad, NULL, true );

	dprintf(D_ALWAYS, "++++++++ Sent Quill ad to collector ++++++++\n");

}

void
TTManager::maintain() 
{	
	QuillErrCode retcode;
	bool bothOk = TRUE;
	QuillErrCode ret_st;

	totalSqlProcessed += lastBatchSqlProcessed;
	lastBatchSqlProcessed = 0;

	if (maintain_db_conn == false) {
		dprintf(D_FULLDEBUG, "TTManager::maintain: connect to DB\n");
		ret_st = DBObj->connectDB();
		if (ret_st == QUILL_FAILURE) {
			dprintf(D_ALWAYS, "TTManager::maintain: unable to connect to DB--- ERROR\n");
			return;
		}		
	}

		// first call the job queue maintain function
	dprintf(D_ALWAYS, "******** Start of Polling Job Queue Log ********\n");

	retcode = jqDBManager.maintain();

	if (retcode == QUILL_FAILURE) {
		dprintf(D_ALWAYS, 
				">>>>>>>> Fail: Polling Job Queue Log <<<<<<<<\n");		
		bothOk = FALSE;
	} else {
		dprintf(D_ALWAYS, "********* End of Polling Job Queue Log *********\n");
	}

		// call the event log maintain function
	dprintf(D_ALWAYS, "******** Start of Polling Event Log ********\n");

	retcode = this->event_maintain();

	if (retcode == QUILL_FAILURE) {
		dprintf(D_ALWAYS, 
				">>>>>>>> Fail: Polling Event Log <<<<<<<<\n");		
		bothOk = FALSE;
	} else {
		dprintf(D_ALWAYS, "********* End of Polling Event Log *********\n");
	}

		// update currency if both log polling succeed
	if (bothOk) {
			// update the currency table
		MyString	sql_str;
		time_t clock;
		const char *scheddname;
		MyString ts_expr;
		const char *data_arr[3];
		QuillAttrDataType   data_typ[3];
		
		(void)time(  (time_t *)&clock );

		scheddname = jqDBManager.getScheddname();

		ret_st = DBObj->beginTransaction();
		if (ret_st == QUILL_FAILURE) {
			dprintf(D_ALWAYS, "Begin transaction failed in TTManager::maintain\n");
		}

		{
			ts_expr = condor_ttdb_buildts(&clock, dt);
	
			if (ts_expr.IsEmpty()) 
				{
					dprintf(D_ALWAYS, "ERROR: Timestamp expression not built\n");
					return;
				}

			sql_str.formatstr("UPDATE currencies SET lastupdate = %s WHERE datasource = '%s'", ts_expr.Value(), scheddname);

			ret_st = DBObj->execCommand(sql_str.Value());
			if (ret_st == QUILL_FAILURE) {
				dprintf(D_ALWAYS, "Update currency --- ERROR [SQL] %s\n", sql_str.Value());
			}
		}

		ret_st = DBObj->commitTransaction();
		if (ret_st == QUILL_FAILURE) {
			dprintf(D_ALWAYS, "Commit transaction failed in TTManager::maintain\n");
		}

	}

		// call the xml log maintain function
	dprintf(D_ALWAYS, "******** Start of Polling XML Log ********\n");

	retcode = this->xml_maintain();

	if (retcode == QUILL_FAILURE) {
		dprintf(D_ALWAYS, 
				">>>>>>>> Fail: Polling XML Log <<<<<<<<\n");		
		bothOk = FALSE;
	} else {
		dprintf(D_ALWAYS, "********* End of Polling XML Log *********\n");
	}

	if (maintain_db_conn == false) {
		dprintf(D_FULLDEBUG, "TTManager::maintain: disconnect from DB\n");
		ret_st = DBObj->disconnectDB();
		if (ret_st == QUILL_FAILURE) {
			dprintf(D_ALWAYS, "TTManager::maintain: unable to disconnect from DB--- ERROR\n");
			return;
		}		
	}
}


//! update the QUILL_AD sent to the collector
/*! This method only updates the ad with new values of dynamic attributes
 *  See createQuillAd for how to create the ad in the first place
 */

void TTManager::updateQuillAd(void) {

	char expr[1000];

	sprintf( expr, "%s = %d", ATTR_QUILL_SQL_LAST_BATCH, 
			 lastBatchSqlProcessed);
	quillad->Insert(expr);

	sprintf( expr, "%s = %d", ATTR_QUILL_SQL_TOTAL, 
			 totalSqlProcessed);
	quillad->Insert(expr);

	sprintf( expr, "%s = %d", "TimeToProcessLastBatch", 
			 0); // secsLastBatch);
	quillad->Insert(expr);

	sprintf( expr, "%s = %d", "IsConnectedToDB", 
			 (DBObj == 0) ? false : DBObj->isConnected());
	quillad->Insert(expr);

}

//! create the QUILL_AD sent to the collector
/*! This method reads all quill-related configuration options from the 
 *  config file and creates a classad which can be sent to the collector
 */

void TTManager::createQuillAd(void) {
	char expr[1000];

	char *scheddName;

	char *mysockname;
	char *tmp;

	quillad = new ClassAd();
	SetMyTypeName(*quillad, QUILL_ADTYPE);
	SetTargetTypeName(*quillad, "");
  
	config_fill_ad(quillad);

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
	quillad->Insert(expr);

	sprintf( expr, "%s = %d", "QuillPollingPeriod", pollingPeriod );
	quillad->Insert(expr);

	/*
	char *quill_query_passwd = param("QUILL_DB_QUERY_PASSWORD");
	if(!quill_query_passwd) {
		EXCEPT("Cannot find variable QUILL_DB_QUERY_PASSWORD "
			   "in config file\n");
	}
  
	sprintf( expr, "%s = \"%s\"", ATTR_QUILL_DB_QUERY_PASSWORD, 
			 quill_query_passwd );
	quillad->Insert(expr);
	*/

	sprintf( expr, "%s = \"%s\"", ATTR_NAME, quill_name );
	quillad->Insert(expr);

	sprintf( expr, "%s = \"%s\"", ATTR_SCHEDD_NAME, scheddName );
	quillad->Insert(expr);

	if(scheddName) {
		delete[] scheddName;
	}

	sprintf( expr, "%s = \"%s\"", ATTR_MACHINE, my_full_hostname() ); 
	quillad->Insert(expr);
  
		// Put in our sinful string.  Note, this is never going to
		// change, so we only need to initialize it once.
	mysockname = strdup( daemonCore->InfoCommandSinfulString() );

	sprintf( expr, "%s = \"%s\"", ATTR_MY_ADDRESS, mysockname );
	quillad->Insert(expr);

	/*
	sprintf( expr, "%s = \"<%s>\"", ATTR_QUILL_DB_IP_ADDR, 
			 jobQueueDBIpAddress );
	quillad->Insert(expr);
	*/

	/*
	sprintf( expr, "%s = \"%s\"", ATTR_QUILL_DB_NAME, jobQueueDBName );
	quillad->Insert(expr);
	*/

	collectors = CollectorList::create();
  
	if(tmp) free(tmp);
	/*if(quill_query_passwd) free(quill_query_passwd); */
	if(quill_name) free(quill_name);
	if(mysockname) free(mysockname);
}

QuillErrCode
TTManager::event_maintain() 
{
	FILESQL *filesqlobj = NULL;
	const char *buf = (char *)0;

	int  buflength=0;
	bool firststmt = true;
	char optype[8], eventtype[CONDOR_TT_EVENTTYPEMAXLEN];
	AttrList *ad = 0, *ad1 = 0;
	MyString *line_buf = 0;
	bool useTempTable = false;
	
	useTempTable = param_boolean("QUILL_USE_TEMP_TABLE", false);
	
		/* copy event log files */	
	int i;
	for(i=0; i < numLogs; i++) {
		filesqlobj = new FILESQL(sqlLogList[i], O_CREAT|O_RDWR, true);

	    if (filesqlobj->file_open() == QUILL_FAILURE) {
			goto ERROREXIT;
		}

		if (filesqlobj->file_lock() == QUILL_FAILURE) {
			goto ERROREXIT;
		}		
		
		if (append(sqlLogCopyList[i], sqlLogList[i]) == QUILL_FAILURE) {
			goto ERROREXIT;
		}

		if(filesqlobj->file_truncate() == QUILL_FAILURE) {
			goto ERROREXIT;
		}

		if(filesqlobj->file_unlock() == QUILL_FAILURE) {
			goto ERROREXIT;
		}
		delete filesqlobj;
		filesqlobj = NULL;
	}

		/* clear up the previous errorSqlStmt and currentSqlLog */
	errorSqlStmt = (char *) 0;
	currentSqlLog = (char *) 0;

		/* process copies of event logs, notice we add 1 to numLogs because 
		   the last file is the special "thrown" file
		*/
	for(i=0; i < numLogs+1; i++) {
		currentSqlLog = sqlLogCopyList[i];
		filesqlobj = new FILESQL(sqlLogCopyList[i], O_CREAT|O_RDWR, true);
		if (filesqlobj->file_open() == QUILL_FAILURE) {
			goto ERROREXIT;
		}
			
		if (filesqlobj->file_lock() == QUILL_FAILURE) {
			goto ERROREXIT;
		}

		firststmt = true;
		line_buf = new MyString();
		while(filesqlobj->file_readline(line_buf)) {
			buf = line_buf->Value();

				// if it is empty, we assume it's the end of log file
			buflength = strlen(buf);
			if(buflength == 0) {
				break;
			}

				// if this is just a new line, just skip it
			if (strcmp(buf, "\n") == 0) {
				delete line_buf;
				line_buf = new MyString();
				continue;
			}			

			if(firststmt) {
				if (dt == T_PGSQL) {
					if (useTempTable)
						QuillErrCode err = DBObj->execCommand("create temp table ad(attr varchar(4000),val varchar(4000)) on commit delete rows");
				}
				if((DBObj->beginTransaction()) == QUILL_FAILURE) {
					dprintf(D_ALWAYS, "Begin transaction --- Error\n");
					goto DBERROR;
				}
				firststmt = false;
				AttributeCache.clearAttributesTransaction();
			}

				// init the optype and eventtype
			strcpy(optype, "");
			strcpy(eventtype, "");
			sscanf(buf, "%7s %49s", optype, eventtype);

			if (strcmp(optype, "NEW") == 0) {
					// first read in the classad until the seperate ***

				if( !( ad=filesqlobj->file_readAttrList()) ) {
					goto ERROREXIT;
				} 				

	   			lastBatchSqlProcessed++;
				if (strcasecmp(eventtype, "Machines") == 0) {		
					if  (insertMachines(ad) == QUILL_FAILURE)   // has vert
						goto DBERROR;
				} else if (strcasecmp(eventtype, "Events") == 0) {
					if  (insertEvents(ad) == QUILL_FAILURE) 
						goto DBERROR;
				} else if (strcasecmp(eventtype, "Files") == 0) {
					if  (insertFiles(ad) == QUILL_FAILURE) 
						goto DBERROR;
				} else if (strcasecmp(eventtype, "Fileusages") == 0) {
					if  (insertFileusages(ad) == QUILL_FAILURE) 
						goto DBERROR;
				} else if (strcasecmp(eventtype, "History") == 0) {
					if  (insertHistoryJob(ad) == QUILL_FAILURE) 
						goto DBERROR;
				} else if (strcasecmp(eventtype, "ScheddAd") == 0) {
					if  (insertScheddAd(ad) == QUILL_FAILURE)  // has vert
						goto DBERROR;	
				} else if (strcasecmp(eventtype, "MasterAd") == 0) {
					if  (insertMasterAd(ad) == QUILL_FAILURE)  // has vert
						goto DBERROR;
				} else if (strcasecmp(eventtype, "NegotiatorAd") == 0) {
					if  (insertNegotiatorAd(ad) == QUILL_FAILURE) // has vert
						goto DBERROR;
				} else if (strcasecmp(eventtype, "Runs") == 0) {
					if  (insertRuns(ad) == QUILL_FAILURE) 
						goto DBERROR;
				} else if (strcasecmp(eventtype, "Transfers") == 0) {
					if  (insertTransfers(ad) == QUILL_FAILURE) 
						goto DBERROR;					
				} else {
					if (insertBasic(ad, eventtype) == QUILL_FAILURE) 
						goto DBERROR;
				}
				
				delete ad;
				ad  = 0;

			} else if (strcmp(optype, "UPDATE") == 0) {
				if( !( ad=filesqlobj->file_readAttrList()) ) {
					goto ERROREXIT;
				} 		
			   
					// the second ad can be null, meaning there is no where clause
				ad1=filesqlobj->file_readAttrList();

				if (updateBasic(ad, ad1, eventtype) == QUILL_FAILURE)
					goto DBERROR;
				
				delete ad;
				if (ad1) delete ad1;
				ad = 0;
				ad1 = 0;

			} else if (strcmp(optype, "DELETE") == 0) {
				dprintf(D_ALWAYS, "DELETE not supported yet\n");
			} else {
				dprintf(D_ALWAYS, "unknown optype in log: %s\n", optype);
			}

				// destroy the string object to reclaim memory
			delete line_buf;
			line_buf = new MyString();
		}

		if(filesqlobj->file_truncate() == QUILL_FAILURE) {
			goto ERROREXIT;
		}

		if(filesqlobj->file_unlock() == QUILL_FAILURE) {
			goto ERROREXIT;
		}

		if(!firststmt) {
			if((DBObj->commitTransaction()) == QUILL_FAILURE) {
				dprintf(D_ALWAYS, "End transaction --- Error\n");
				goto DBERROR;
			}
			AttributeCache.commitAttributesTransaction();
		}

		if(filesqlobj) {
			delete filesqlobj;
			filesqlobj = NULL;
		}

		if (line_buf) {
			delete line_buf;
			line_buf = (MyString *)0;
		}
	}

	return QUILL_SUCCESS;

 ERROREXIT:
	if(filesqlobj) {
		delete filesqlobj;
	}
	
	if (line_buf) {
		delete line_buf;
	}

	if (ad)
		delete ad;
	
	if (ad1) 
		delete ad1;

	return QUILL_FAILURE;

 DBERROR:
	if(filesqlobj) {
		delete filesqlobj;
	}	
	
	if (line_buf) 
		delete line_buf;

	if (ad)
		delete ad;

	if (ad1) 
		delete ad1;

	// the failed transaction must be rolled back
	// so that subsequent SQLs don't continue to fail
	DBObj->rollbackTransaction();

		/* if the database connection is ok and we fail because of an 
		   error in sql processing, we record the error sql and the 
		   sql log in the database and truncate the error sql log. 
		   if we fail because of other database error (e.g. broken 
		   connection), we check if a sql log has exceeded its 	
		   size limit, and if so, we throw it away.
		*/
	if (DBObj->checkConnection() == QUILL_SUCCESS) {
		if (!errorSqlStmt.IsEmpty() &&
			!currentSqlLog.IsEmpty()) {
			this -> handleErrorSqlLog();
		}
	} else {
		this -> checkAndThrowBigFiles();
		DBObj->resetConnection();
	}

	return QUILL_FAILURE;
}

void TTManager::checkAndThrowBigFiles() {
	struct stat file_status;
	FILESQL *filesqlobj, *thrownfileobj;

	ClassAd tmpCl1;
	ClassAd *tmpClP1 = &tmpCl1;
	char tmp[512];

	thrownfileobj = new FILESQL(sqlLogCopyList[CONDOR_TT_THROWFILE], O_WRONLY|O_CREAT|O_APPEND, true);

	thrownfileobj ->file_open();

	int i;
	for(i=0; i < numLogs; i++) {
		stat(sqlLogCopyList[i], &file_status);
		
			// if the file is bigger than the max file size, we throw it away 
		if (file_status.st_size > CONDOR_TT_FILESIZELIMT) {
			filesqlobj = new FILESQL(sqlLogCopyList[i], O_RDWR, true);
			filesqlobj->file_open();
			filesqlobj->file_truncate();
			delete filesqlobj;

				/* string values are double quoted */
			snprintf(tmp, 512, "filename = \"%s\"", sqlLogCopyList[i]);
			tmpClP1->Insert(tmp);		
			
			snprintf(tmp, 512, "machine_id = \"%s\"", my_full_hostname());
			tmpClP1->Insert(tmp);		

			snprintf(tmp, 512, "log_size = %d", (int)file_status.st_size);
			tmpClP1->Insert(tmp);		
			
			snprintf(tmp, 512, "throwtime = %d", (int)file_status.st_mtime);
			tmpClP1->Insert(tmp);				
			
			thrownfileobj->file_newEvent("Throwns", tmpClP1);
		}
	}

	delete thrownfileobj;
}

QuillErrCode
TTManager::xml_maintain() 
{
	bool want_xml = false;
	FILEXML *filexmlobj = NULL;
	char xmlParamList[][30] = {"NEGOTIATOR_XMLLOG", "SCHEDD_XMLLOG", 
						   "SHADOW_XMLLOG", "STARTER_XMLLOG", 
						   "STARTD_XMLLOG", "SUBMIT_XMLLOG", 
						   "COLLECTOR_XMLLOG", ""};	
	char *dump_path = "/u/p/a/pachu/RA/LogCorr/xml-logs";
	char *tmp, *fname;

	int numXLogs = 0, i = 0, found = 0;
	char    xmlLogList[CONDOR_TT_MAXLOGNUM][CONDOR_TT_MAXLOGPATHLEN];
	char    xmlLogCopyList[CONDOR_TT_MAXLOGNUM][CONDOR_TT_MAXLOGPATHLEN];

		// check if XML logging is turned on & if not, exit
	want_xml = param_boolean("WANT_XML_LOG", false);

	if ( !want_xml )
		return QUILL_SUCCESS;

		// build list of xml logs and the copies
	while (xmlParamList[i][0] != '\0') {
		tmp = param(xmlParamList[i]);
		if (tmp) {
			
			found = 0;

				// check if the new log file is already in the list
			int j;
			for (j = 0 ; j < numXLogs; j++) {
				if (strcmp(tmp, xmlLogList[j]) == 0) {
					found = 1;
					break;
				}
			}

			if (found) {
				free(tmp);
				i++;
				continue;
			}
				
			strncpy(xmlLogList[numXLogs], tmp, CONDOR_TT_MAXLOGPATHLEN);
			fname = strrchr(tmp, '/')+1;
			snprintf(xmlLogCopyList[numXLogs], CONDOR_TT_MAXLOGPATHLEN, "%s/%s-%s.xml", dump_path, fname, get_local_hostname().Value());
			numXLogs++;
			free(tmp);
		}		
		i++;
	}

		/* add the default log file in case no log file is specified in config */
	tmp = param("LOG");
	if (tmp) {
		snprintf(xmlLogList[numXLogs], CONDOR_TT_MAXLOGPATHLEN, "%s/Events.xml", tmp);
		snprintf(xmlLogCopyList[numXLogs], CONDOR_TT_MAXLOGPATHLEN, "%s/Events-%s.xml", dump_path, get_local_hostname().Value());
	} else {
		snprintf(xmlLogList[numXLogs], CONDOR_TT_MAXLOGPATHLEN, "Events.xml");
		snprintf(xmlLogCopyList[numXLogs], CONDOR_TT_MAXLOGPATHLEN, "%s/Events-%s.xml", dump_path, get_local_hostname().Value());
	}
	numXLogs++;	

		/* copy xml log files */	
	for(i=0; i < numXLogs; i++) {
		filexmlobj = new FILEXML(xmlLogList[i], O_CREAT|O_RDWR);

	    if (filexmlobj->file_open() == QUILL_FAILURE) {
			goto ERROREXIT;
		}

		if (filexmlobj->file_lock() == QUILL_FAILURE) {
			goto ERROREXIT;
		}		
		
		if (append(xmlLogCopyList[i], xmlLogList[i]) == QUILL_FAILURE) {
			goto ERROREXIT;
		}

		if(filexmlobj->file_truncate() == QUILL_FAILURE) {
			goto ERROREXIT;
		}

		if(filexmlobj->file_unlock() == QUILL_FAILURE) {
			goto ERROREXIT;
		}
		delete filexmlobj;
		filexmlobj = NULL;
	}

	return QUILL_SUCCESS;

 ERROREXIT:
	if(filexmlobj) {
		delete filexmlobj;
	}
	
	return QUILL_FAILURE;

}


QuillErrCode TTManager::insertMachines(AttrList *ad) {
	AttributeHashSmartPtrType newClAd = AttributeCache.newAttributeHash(); // for holding attributes going to vertical table
	MyString sql_stmt;
	MyString classAd;
	const char *iter;
	char *attName = NULL, *attVal;
	MyString tmpVal = "";
	MyString attNameList = "";
	MyString attValList = "";
	MyString inlist = "";
	int isFirst = TRUE;
	MyString aName, aVal, temp, machine_id;

	MyString lastReportedTime = "";
	MyString lastReportedTimeValue = "";
	MyString clob_comp_expr;
	bool useTempTable = param_boolean("QUILL_USE_TEMP_TABLE", false);;

		// previous LastReportedTime from the current classad
		// previous LastReportedTime from the database's machines_horizontal
    int  prevLHFInAd = 0;
    int  prevLHFInDB = 0;
	int	 ret_st;
	QuillAttrDataType  attr_type;
	int  num_result = 0;

	MyString ts_expr;

	const char *data_arr[7];
	QuillAttrDataType   data_typ[7];

	sPrintAd(classAd, *ad);

	// Insert stuff into Machines_Horizontal

	classAd.Tokenize();
	iter = classAd.GetNextToken("\n", true);

	while (iter != NULL)
		{
				/* the attribute name can't be longer than the log entry line 
				   size make sure attName is freed always to prevent memory 
				   leak. 
				*/
			attName = (char *)malloc(strlen(iter));

			sscanf(iter, "%s =", attName);
			attVal = (char*)strstr(iter, "= ");
			attVal += 2;

			if (strcasecmp(attName, "PrevLastReportedTime") == 0) {
				prevLHFInAd = atoi(attVal);
			}
			else if (isHorizontalMachineAttr(attName, attr_type)) {
					// should go into machines_horizontal table
				if (isFirst) {
						//is the first in the list
					isFirst = FALSE;
					
					if (strcasecmp(attName, "lastreportedtime") == 0) {
							/* for lastreportedtime, we want to store both 
							   the epoch seconds and the string timestamp
							   value. The epoch seconds is stored in a column
							   named lastreportedtime_epoch.
							*/
						attNameList.formatstr("(lastreportedtime, lastreportedtime_epoch");

					} else {
						if (strcasecmp(attName, ATTR_NAME) == 0) {
							attNameList.formatstr("(machine_id");
						} else {
							attNameList.formatstr("(%s", attName);
						}

					}

					switch (attr_type) {

					case CONDOR_TT_TYPE_STRING:
						if (!stripdoublequotes(attVal)) {
							dprintf(D_ALWAYS, "ERROR: string constant not double quoted for attribute %s in TTManager::insertMachines\n", attName);
						}
						
						if (strcasecmp(attName, ATTR_NAME) == 0) {
							machine_id = attVal;
						}

						attValList.formatstr("('%s'", strchr(attVal,'\'') ? " " : attVal);
						break;
					case CONDOR_TT_TYPE_BOOL:
							// boolean value are stored as string in db, but 
							// its value is not double quoted
						attValList.formatstr("('%s'", attVal);
						break;
					case CONDOR_TT_TYPE_TIMESTAMP:
						time_t clock;
						clock = atoi(attVal);
						
						ts_expr = condor_ttdb_buildts(&clock, dt);

						if (ts_expr.IsEmpty()) {
							dprintf(D_ALWAYS, "ERROR: Timestamp expression not built\n");
							if (attName) {
								free(attName);
								attName = NULL;
							}
							return QUILL_FAILURE;							
						}

						if (strcasecmp(attName, "lastreportedtime") == 0) {
							attValList.formatstr("(%s, %s", ts_expr.Value(), 
											   attVal);
							lastReportedTime.formatstr("%s", ts_expr.Value());
							lastReportedTimeValue = condor_ttdb_buildtsval(&clock, dt);
						} else {
							attValList.formatstr("(%s", ts_expr.Value());
						}

						break;
					case CONDOR_TT_TYPE_NUMBER:
						attValList.formatstr("(%s", attVal);
						break;
					default:
						dprintf(D_ALWAYS, "insertMachines: Unsupported horizontal machine attribute %s\n", attName);
						if (attName) {
							free(attName);
							attName = NULL;
						}						
						return QUILL_FAILURE;
						break;							
					}
				} else {
						// is not the first in the list
					if (strcasecmp(attName, "lastreportedtime") == 0) {
						attNameList += ", ";
						
						attNameList += "lastreportedtime, lastreportedtime_epoch";

					} else {
						attNameList += ", ";

						if (strcasecmp(attName, ATTR_NAME) == 0) {
							attNameList += "machine_id";
						} else {
							attNameList += attName;
						}
					}

					attValList += ", ";

					switch (attr_type) {

					case CONDOR_TT_TYPE_STRING:
						if (!stripdoublequotes(attVal)) {
							dprintf(D_ALWAYS, "ERROR: string constant not double quoted for attribute %s in TTManager::insertMachines\n", attName);
						}
						
						if (strcasecmp(attName, ATTR_NAME) == 0) {
							machine_id = attVal;
						}

						tmpVal.formatstr("'%s'", strchr(attVal,'\'') ? " " : attVal);
						break;
					case CONDOR_TT_TYPE_BOOL:
							// boolean value are stored as string in db, but 
							// its value is not double quoted
						tmpVal.formatstr("'%s'", attVal);
						break;
					case CONDOR_TT_TYPE_TIMESTAMP:
						time_t clock;
						clock = atoi(attVal);

						ts_expr = condor_ttdb_buildts(&clock, dt);

						if (ts_expr.IsEmpty()) {
							dprintf(D_ALWAYS, "ERROR: Timestamp expression not built\n");
							if (attName) {
								free(attName);
								attName = NULL;
							}							
							return QUILL_FAILURE;							
						}
						
						if (strcasecmp(attName, "lastreportedtime") == 0) {
							tmpVal.formatstr("%s, %s", ts_expr.Value(), 
										   attVal);
							lastReportedTime.formatstr("%s", ts_expr.Value());
							lastReportedTimeValue = condor_ttdb_buildtsval(&clock, dt);
						} else {
							tmpVal.formatstr("%s", ts_expr.Value());
						}

						break;
					case CONDOR_TT_TYPE_NUMBER:
						tmpVal.formatstr("%s", attVal);
						break;
					default:
						dprintf(D_ALWAYS, "insertMachines: Unsupported horizontal machine attribute %s\n", attName);
						if (attName) {
							free(attName);
							attName = NULL;
						}
						return QUILL_FAILURE;
					}

					attValList += tmpVal;
				}
			} else {  // static attributes (go into Machine table)
				aName = attName;
				aVal = attVal;
				// insert into new ClassAd too (since this needs to go into DB)
				newClAd->insert(aName, aVal);
				if (inlist.IsEmpty()) {					
					inlist.formatstr("('%s'", attName);
				} else {
					inlist += ",'";
					inlist += attName;
					inlist += "'";
				}
			}

			if (attName) {
				free(attName);
				attName = NULL;
			}
			iter = classAd.GetNextToken("\n", true);
		}

	if (!attNameList.IsEmpty())
		attNameList += ")";

	if (!attValList.IsEmpty())
		attValList += ")";

	if (!inlist.IsEmpty())
		inlist += ")";

	{
			// get the previous lastreportedtime from the database 
		sql_stmt.formatstr("SELECT lastreportedtime_epoch FROM Machines_Horizontal WHERE machine_id = '%s'", machine_id.Value());

		ret_st = DBObj->execQuery(sql_stmt.Value(), num_result);
	}

	if (ret_st == QUILL_FAILURE) {
		dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		dprintf(D_ALWAYS, "sql = %s\n", sql_stmt.Value());
		errorSqlStmt = sql_stmt;
		return QUILL_FAILURE;
	}
	else if (ret_st == QUILL_SUCCESS && num_result > 0) {
		prevLHFInDB = atoi(DBObj->getValue(0, 0));		
	}
	
	DBObj->releaseQueryResult();

		// set end time if the previous lastReporteTime matches, otherwise
		// leave it as NULL (by default)
	if (prevLHFInDB == prevLHFInAd) {
		sql_stmt.formatstr("INSERT INTO Machines_Horizontal_History(machine_id, opsys, arch, state, activity, keyboardidle, consoleidle, loadavg, condorloadavg, totalloadavg, virtualmemory, memory, totalvirtualmemory, cpubusytime, cpuisbusy, currentrank , clockmin, clockday, lastreportedtime, enteredcurrentactivity, enteredcurrentstate, updatesequencenumber, updatestotal, updatessequenced, updateslost, globaljobid, end_time) SELECT machine_id, opsys, arch, state, activity, keyboardidle, consoleidle, loadavg, condorloadavg, totalloadavg, virtualmemory, memory, totalvirtualmemory, cpubusytime, cpuisbusy, currentrank, clockmin, clockday, lastreportedtime, enteredcurrentactivity, enteredcurrentstate, updatesequencenumber, updatestotal, updatessequenced, updateslost, globaljobid, %s FROM Machines_Horizontal WHERE machine_id = '%s'", lastReportedTime.Value(), machine_id.Value());
	} else {
		sql_stmt.formatstr("INSERT INTO Machines_Horizontal_History (machine_id, opsys, arch, state, activity, keyboardidle, consoleidle, loadavg, condorloadavg, totalloadavg, virtualmemory, memory, totalvirtualmemory, cpubusytime, cpuisbusy, currentrank , clockmin, clockday, lastreportedtime, enteredcurrentactivity, enteredcurrentstate, updatesequencenumber, updatestotal, updatessequenced, updateslost, globaljobid) SELECT machine_id, opsys, arch, state, activity, keyboardidle, consoleidle, loadavg, condorloadavg, totalloadavg, virtualmemory, memory, totalvirtualmemory, cpubusytime, cpuisbusy, currentrank, clockmin, clockday, lastreportedtime, enteredcurrentactivity, enteredcurrentstate, updatesequencenumber, updatestotal, updatessequenced, updateslost, globaljobid FROM Machines_Horizontal WHERE machine_id = '%s'", machine_id.Value());
	}
	
	{
		if (DBObj->execCommand(sql_stmt.Value()) == QUILL_FAILURE) {
			dprintf(D_ALWAYS, "Executing Statement --- Error\n");
			dprintf(D_ALWAYS, "sql = %s\n", sql_stmt.Value());
			errorSqlStmt = sql_stmt;
			return QUILL_FAILURE;
		}
	}

	{
		sql_stmt.formatstr("DELETE FROM Machines_Horizontal WHERE machine_id = '%s'", machine_id.Value());

		if (DBObj->execCommand(sql_stmt.Value()) == QUILL_FAILURE) {
			dprintf(D_ALWAYS, "Executing Statement --- Error\n");
			dprintf(D_ALWAYS, "sql = %s\n", sql_stmt.Value());
			errorSqlStmt = sql_stmt;
			return QUILL_FAILURE;
		}
	}

	sql_stmt.formatstr("INSERT INTO Machines_Horizontal %s VALUES %s", attNameList.Value(), attValList.Value());

	if (DBObj->execCommand(sql_stmt.Value()) == QUILL_FAILURE) {
		 dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		 dprintf(D_ALWAYS, "sql = %s\n", sql_stmt.Value());
		 errorSqlStmt = sql_stmt;
		 return QUILL_FAILURE;
	}
	 
		// Insert changes into Machine

		// if the previous lastReportedTime doesn't match, this means the 
		// daemon has been shutdown for a while, we should move everything
		// into the Machines_vertical_history (with a NULL end_time)!
	if (prevLHFInDB == prevLHFInAd) {
		sql_stmt.formatstr("INSERT INTO Machines_Vertical_History (machine_id, attr, val, start_time, end_time) SELECT machine_id, attr, val, start_time, %s FROM Machines_Vertical WHERE machine_id = '%s' AND attr NOT IN %s", lastReportedTime.Value(), machine_id.Value(), inlist.Value());
	} else {
		sql_stmt.formatstr("INSERT INTO Machines_Vertical_History (machine_id, attr, val, start_time) SELECT machine_id, attr, val, start_time FROM Machines_Vertical WHERE machine_id = '%s'", machine_id.Value());		 
	}

	if (DBObj->execCommand(sql_stmt.Value()) == QUILL_FAILURE) {
		dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		dprintf(D_ALWAYS, "sql = %s\n", sql_stmt.Value());
		errorSqlStmt = sql_stmt;
		return QUILL_FAILURE;
	}
		
	if (prevLHFInDB == prevLHFInAd) {
		sql_stmt.formatstr("DELETE FROM Machines_Vertical WHERE machine_id = '%s' AND attr NOT IN %s", machine_id.Value(), inlist.Value());
	} else {
		sql_stmt.formatstr("DELETE FROM Machines_Vertical WHERE machine_id = '%s'", machine_id.Value());		 
	}

	if (DBObj->execCommand(sql_stmt.Value()) == QUILL_FAILURE) {
		dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		dprintf(D_ALWAYS, "sql = %s\n", sql_stmt.Value());
		errorSqlStmt = sql_stmt;
		return QUILL_FAILURE;
	}
	 
	MyString bulk;
	if (dt == T_PGSQL) {
		if (useTempTable) {
			QuillErrCode err = DBObj->execCommand("truncate ad");
			if (err == QUILL_FAILURE) {
				dprintf(D_ALWAYS, "Error running truncate ad\n");
				errorSqlStmt = "truncate ad";
				return QUILL_FAILURE;
			}
			err = DBObj->execCommand("COPY ad from STDIN");
			if (err == QUILL_FAILURE) {
				dprintf(D_ALWAYS, "Error running COPY ad from STDIN\n");
				errorSqlStmt = "Copy ad";
				return QUILL_FAILURE;
			}
		}
	}
	 
		// Put this set of attributes into our soft-state transaction cache.
		// This transaction cache will be committed when we commit to the 
		// database.
	AttributeCache.insertTransaction(MACHINE_HASH, machine_id);

	newClAd->startIterations();
	while (newClAd->iterate(aName, aVal)) {

			// See if this attribute name/value pair was previously committed
			// into the database.  If so, continue on to the next attribute.
		if ( AttributeCache.alreadyCommitted(aName, aVal) ) {
			// skip
			dprintf(D_FULLDEBUG,"Skipping update of unchanged attr %s\n",aName.Value());
			continue;
		}

		{
			aVal.replaceString("'"," ");
			aVal.replaceString("\""," ");
			aVal.replaceString("\t"," ");

		if (!useTempTable) {
			sql_stmt.formatstr("INSERT INTO Machines_Vertical (machine_id, attr, val, start_time) SELECT '%s', '%s', '%s', %s FROM dummy_single_row_table WHERE NOT EXISTS (SELECT * FROM Machines_Vertical WHERE machine_id = '%s' AND attr = '%s')", machine_id.Value(), aName.Value(), aVal.Value(), lastReportedTime.Value(), machine_id.Value(), aName.Value());

			if (DBObj->execCommand(sql_stmt.Value()) == QUILL_FAILURE) {
				dprintf(D_ALWAYS, "Executing Statement --- Error\n");
				dprintf(D_ALWAYS, "sql = %s\n", sql_stmt.Value());
				errorSqlStmt = sql_stmt;
				return QUILL_FAILURE;
			}
			
			clob_comp_expr = condor_ttdb_compare_clob_to_lit(dt, "val", aVal.Value());

			sql_stmt.formatstr("INSERT INTO Machines_Vertical_History (machine_id, attr, val, start_time, end_time) SELECT machine_id, attr, val, start_time, %s FROM Machines_Vertical WHERE machine_id = '%s' AND attr = '%s' AND %s", lastReportedTime.Value(), machine_id.Value(), aName.Value(), clob_comp_expr.Value());

			if (DBObj->execCommand(sql_stmt.Value()) == QUILL_FAILURE) {
				dprintf(D_ALWAYS, "Executing Statement --- Error\n");
				dprintf(D_ALWAYS, "sql = %s\n", sql_stmt.Value());		
				errorSqlStmt = sql_stmt;
				return QUILL_FAILURE;
			}

			sql_stmt.formatstr("UPDATE Machines_Vertical SET val = '%s', start_time = %s WHERE machine_id = '%s' AND attr = '%s' AND %s", aVal.Value(), lastReportedTime.Value(), machine_id.Value(), aName.Value(), clob_comp_expr.Value());

			if (DBObj->execCommand(sql_stmt.Value()) == QUILL_FAILURE) {
				dprintf(D_ALWAYS, "Executing Statement --- Error\n");
				dprintf(D_ALWAYS, "sql = %s\n", sql_stmt.Value());
				errorSqlStmt = sql_stmt;
				return QUILL_FAILURE;
			}		 
		}
		bulk.formatstr_cat("%s\t%s\n", aName.Value(), aVal.Value());
		}

	}

	if (dt == T_PGSQL) {
		if (useTempTable) {
		DBObj->sendBulkData(bulk.Value());
		DBObj->sendBulkDataEnd();

		sql_stmt.formatstr(
	"INSERT INTO Machines_Vertical(machine_id, attr, val, start_time) "
	" 		select '%s', ad.attr, ad.val, %s "
	"       from ad "
	"       where attr not in (select attr from Machines_vertical as mv "
    "                                       where mv.machine_id = '%s')"
		
		, machine_id.Value(), lastReportedTime.Value(), machine_id.Value());

		if (DBObj->execCommand(sql_stmt.Value()) == QUILL_FAILURE) {
			dprintf(D_ALWAYS, "Executing Statement --- Error\n");
			dprintf(D_ALWAYS, "sql = %s\n", sql_stmt.Value());
			errorSqlStmt = sql_stmt;
			return QUILL_FAILURE;
		}		 

			// Copy any attributes for this machine whose value have
			// changed since last time to the machines_vertical_history
			// table.
		sql_stmt.formatstr(
	"INSERT INTO Machines_Vertical_History(machine_id, attr, val, start_time, end_time)"
	"  select '%s', ad.attr, ad.val, mv.start_time, %s "
	"         from ad, Machines_Vertical as mv "
	"         where mv.machine_id = '%s' and ad.attr = mv.attr and "
	"              ad.val != mv.val "

		, machine_id.Value(), lastReportedTime.Value(), machine_id.Value());

		if (DBObj->execCommand(sql_stmt.Value()) == QUILL_FAILURE) {
			dprintf(D_ALWAYS, "Executing Statement --- Error\n");
			dprintf(D_ALWAYS, "sql = %s\n", sql_stmt.Value());
			errorSqlStmt = sql_stmt;
			return QUILL_FAILURE;
		}		 


			// Update any attributes in the machines_vertical table
			// that were already there, but now have new values
		sql_stmt.formatstr(
	"UPDATE Machines_Vertical "
	"       SET val = ad.val, start_time = %s "
	"			from ad "
	"		where "
	"             Machines_Vertical.machine_id = '%s' and "
	"             Machines_Vertical.attr = ad.attr and "
	"             Machines_Vertical.val != ad.val"

		, lastReportedTime.Value(), machine_id.Value());
	}

	if (DBObj->execCommand(sql_stmt.Value()) == QUILL_FAILURE) {
		dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		dprintf(D_ALWAYS, "sql = %s\n", sql_stmt.Value());		
		errorSqlStmt = sql_stmt;
		return QUILL_FAILURE;
	}
	}

	return QUILL_SUCCESS;
}

QuillErrCode TTManager::insertScheddAd(AttrList *ad) {
	AttributeHashSmartPtrType newClAd = AttributeCache.newAttributeHash(); // for holding attributes going to vertical table

	MyString sql_stmt;
	MyString classAd;
	const char *iter;
	
	char *attName = NULL, *attVal;
	MyString attNameList = "";
	MyString attValList = "";
	MyString tmpVal = "";
	MyString aName, aVal, temp;
	MyString inlist = "";
	MyString lastReportedTime = "";
	MyString lastReportedTimeValue = "";
	MyString daemonName = "";

		// previous LastReportedTime from the current classad
		// previous LastReportedTime from the database's machines_horizontal
    int  prevLHFInAd = 0;
    int  prevLHFInDB = 0;
	int	 ret_st;
    QuillAttrDataType attr_type;
	int  num_result = 0;

	MyString ts_expr;
	MyString clob_comp_expr;
	
		// first generate MyType='Scheduler' attribute
	attNameList.formatstr("(MyType");
	attValList.formatstr("('Scheduler'");

	sPrintAd(classAd, *ad);

	classAd.Tokenize();
	iter = classAd.GetNextToken("\n", true);

	while (iter != NULL)
		{
			attName = (char *)malloc(strlen(iter));
			sscanf(iter, "%s =", attName);
			attVal = (char*)strstr(iter, "= ");
			attVal += 2;

			if (strcasecmp(attName, "PrevLastReportedTime") == 0) {
				prevLHFInAd = atoi(attVal);
			}

				/* notice that the Name and LastReportedTime are both a 
				   horizontal daemon attribute and horizontal schedd attribute,
				   therefore we need to check both seperately.
				*/
			if (isHorizontalDaemonAttr(attName, attr_type)) {
				if (strcasecmp(attName, "lastreportedtime") == 0) {
					attNameList += ", ";
					
					attNameList += "lastreportedtime, lastreportedtime_epoch";
					
				} else {
					attNameList += ", ";
					attNameList += attName;
				}

				attValList += ", ";

				switch (attr_type) {

				case CONDOR_TT_TYPE_STRING:
					if (!stripdoublequotes(attVal)) {
						dprintf(D_ALWAYS, "ERROR: string constant not double quoted for attribute %s in TTManager::insertSchedd\n", attName);
					}

					if (strcasecmp(attName, ATTR_NAME) == 0) {
						daemonName.formatstr("%s", attVal);
					}
								
					tmpVal.formatstr("'%s'", attVal);
					break;
				case CONDOR_TT_TYPE_TIMESTAMP:
					time_t clock;
					clock = atoi(attVal);

					ts_expr = condor_ttdb_buildts(&clock, dt);	

					if (ts_expr.IsEmpty()) {
						dprintf(D_ALWAYS, "ERROR: Timestamp expression not built\n");
						if (attName) {
							free(attName);
							attName = NULL;
						}
						return QUILL_FAILURE;							
					}

					if (strcasecmp(attName, "lastreportedtime") == 0) { 
						tmpVal.formatstr("%s, %s", ts_expr.Value(), attVal);
						lastReportedTime.formatstr("%s", ts_expr.Value());
						lastReportedTimeValue = condor_ttdb_buildtsval(&clock, dt);
					} else {
						tmpVal.formatstr("%s", ts_expr.Value());
					}
					
					break;
				case CONDOR_TT_TYPE_NUMBER:
					tmpVal.formatstr("%s", attVal);
					break;
				default:
					dprintf(D_ALWAYS, "insertScheddAd: unsupported horizontal daemon attribute %s\n", attName);
					if (attName) {
						free(attName);
						attName = NULL;
					}
					return QUILL_FAILURE;
				}

				attValList += tmpVal;

			} else if (strcasecmp(attName, "PrevLastReportedTime") != 0) {

					// the rest of attributes go to the vertical schedd table
				aName = attName;
				aVal = attVal;

					// insert into new ClassAd to be inserted into DB
				newClAd->insert(aName, aVal);				

					// build an inlist of the vertical attribute names
				if (inlist.IsEmpty()) {
					inlist.formatstr("('%s'", attName);
				} else {
					inlist += ",'";
					inlist += attName;
					inlist += "'";
				}			
			}

			if (attName) {
				free (attName);
				attName = NULL;
			}
			iter = classAd.GetNextToken("\n", true);
		}

	if (!attNameList.IsEmpty()) attNameList += ")";
	if (!attValList.IsEmpty()) attValList += ")";
	if (!inlist.IsEmpty()) inlist += ")";

		// get the previous lastreportedtime from the database 
	sql_stmt.formatstr("SELECT lastreportedtime_epoch FROM daemons_horizontal WHERE MyType = 'Scheduler' AND Name = '%s'", daemonName.Value());

	ret_st = DBObj->execQuery(sql_stmt.Value(), num_result);

	if (ret_st == QUILL_FAILURE) {
		dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		dprintf(D_ALWAYS, "sql = %s\n", sql_stmt.Value());
		errorSqlStmt = sql_stmt;
		return QUILL_FAILURE;
	}
	else if (ret_st == QUILL_SUCCESS && num_result > 0) {
		prevLHFInDB = atoi(DBObj->getValue(0, 0));	   
	}

	DBObj->releaseQueryResult();

		/* move the horizontal daemon attributes tuple to history
		   set end time if the previous lastReportedTime matches, otherwise
		   leave it as NULL (by default)	
		*/
	if (prevLHFInDB == prevLHFInAd) {
		sql_stmt.formatstr("INSERT INTO Daemons_Horizontal_History (MyType, Name, LastReportedTime, MonitorSelfTime, MonitorSelfCPUUsage, MonitorSelfImageSize, MonitorSelfResidentSetSize, MonitorSelfAge, UpdateSequenceNumber, UpdatesTotal, UpdatesSequenced, UpdatesLost, UpdatesHistory, endtime) SELECT MyType, Name, LastReportedTime, MonitorSelfTime, MonitorSelfCPUUsage, MonitorSelfImageSize, MonitorSelfResidentSetSize, MonitorSelfAge, UpdateSequenceNumber, UpdatesTotal, UpdatesSequenced, UpdatesLost, UpdatesHistory, %s FROM Daemons_Horizontal WHERE MyType = 'Scheduler' AND Name = '%s'", lastReportedTime.Value(), daemonName.Value());
	} else {
		sql_stmt.formatstr("INSERT INTO Daemons_Horizontal_History (MyType, Name, LastReportedTime, MonitorSelfTime, MonitorSelfCPUUsage, MonitorSelfImageSize, MonitorSelfResidentSetSize, MonitorSelfAge, UpdateSequenceNumber, UpdatesTotal, UpdatesSequenced, UpdatesLost, UpdatesHistory) SELECT MyType, Name, LastReportedTime, MonitorSelfTime, MonitorSelfCPUUsage, MonitorSelfImageSize, MonitorSelfResidentSetSize, MonitorSelfAge, UpdateSequenceNumber, UpdatesTotal, UpdatesSequenced, UpdatesLost, UpdatesHistory FROM Daemons_Horizontal WHERE MyType = 'Scheduler' AND Name = '%s'", daemonName.Value());
	}
	
	if (DBObj->execCommand(sql_stmt.Value()) == QUILL_FAILURE) {
		dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		dprintf(D_ALWAYS, "sql = %s\n", sql_stmt.Value());
		errorSqlStmt = sql_stmt;
		return QUILL_FAILURE;
	}
	
	sql_stmt.formatstr("DELETE FROM  Daemons_Horizontal WHERE MyType = 'Scheduler' AND Name = '%s'", daemonName.Value());

	if (DBObj->execCommand(sql_stmt.Value()) == QUILL_FAILURE) {
		dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		dprintf(D_ALWAYS, "sql = %s\n", sql_stmt.Value());
		errorSqlStmt = sql_stmt;
		return QUILL_FAILURE;
	}

		// insert new tuple into daemons_horizontal 
	sql_stmt.formatstr("INSERT INTO daemons_horizontal %s VALUES %s", attNameList.Value(), attValList.Value());

	 if (DBObj->execCommand(sql_stmt.Value()) == QUILL_FAILURE) {
		 dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		 dprintf(D_ALWAYS, "sql = %s\n", sql_stmt.Value());
		 errorSqlStmt = sql_stmt;	
		 return QUILL_FAILURE;
	 }	 

		// Make changes into daemons_vertical and daemons_vertical_history

		 /* if the previous lastReportedTime doesn't match, this means the 
			daemon has been shutdown for a while, we should move everything
			into the daemons_vertical_history (with a NULL end_time)!
			if the previous lastReportedTime matches, only move attributes that 
			don't appear in the new class ad
		 */
	 if (prevLHFInDB == prevLHFInAd) {
		 sql_stmt.formatstr("INSERT INTO Daemons_Vertical_History (MyType, Name, LastReportedTime, attr, val, endtime) SELECT MyType, name, lastreportedtime, attr, val, %s FROM Daemons_Vertical WHERE MyType = 'Scheduler' AND name = '%s' AND attr NOT IN %s", lastReportedTime.Value(), daemonName.Value(), inlist.Value());
	 } else {
		 sql_stmt.formatstr("INSERT INTO Daemons_Vertical_History (MyType, Name, LastReportedTime, attr, val) SELECT MyType, name, lastreportedtime, attr, val FROM Daemons_Vertical WHERE MyType = 'Scheduler' AND name = '%s'", daemonName.Value());		 
	 }	 

	 if (DBObj->execCommand(sql_stmt.Value()) == QUILL_FAILURE) {
		 dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		 dprintf(D_ALWAYS, "sql = %s\n", sql_stmt.Value());
		 errorSqlStmt = sql_stmt;
		 return QUILL_FAILURE;
	 }

	 if (prevLHFInDB == prevLHFInAd) {
		 sql_stmt.formatstr("DELETE FROM daemons_vertical WHERE MyType = 'Scheduler' AND name = '%s' AND attr NOT IN %s", daemonName.Value(), inlist.Value());
	 } else {
		 sql_stmt.formatstr("DELETE FROM daemons_vertical WHERE MyType = 'Scheduler' AND name = '%s'", daemonName.Value());		 
	 }

	 if (DBObj->execCommand(sql_stmt.Value()) == QUILL_FAILURE) {
		 dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		 dprintf(D_ALWAYS, "sql = %s\n", sql_stmt.Value());
		 errorSqlStmt = sql_stmt;		 
		 return QUILL_FAILURE;
	 }

		// Put this set of attributes into our soft-state transaction cache.
		// This transaction cache will be committed when we commit to the 
		// database.
	AttributeCache.insertTransaction(SCHEDD_HASH, daemonName);

		 // insert the vertical attributes
	 newClAd->startIterations();
	 while (newClAd->iterate(aName, aVal)) {

			// See if this attribute name/value pair was previously committed
			// into the database.  If so, continue on to the next attribute.
		if ( AttributeCache.alreadyCommitted(aName, aVal) ) {
			// skip
			dprintf(D_FULLDEBUG,"Skipping update of unchanged attr %s\n",aName.Value());
			continue;
		}


		 {
			 sql_stmt.formatstr("INSERT INTO daemons_vertical (MyType, name, attr, val, lastreportedtime) SELECT 'Scheduler', '%s', '%s', '%s', %s FROM dummy_single_row_table WHERE NOT EXISTS (SELECT * FROM daemons_vertical WHERE MyType = 'Scheduler' AND name = '%s' AND attr = '%s')", daemonName.Value(), aName.Value(), aVal.Value(), lastReportedTime.Value(), daemonName.Value(), aName.Value());

			 if (DBObj->execCommand(sql_stmt.Value()) == QUILL_FAILURE) {
				 dprintf(D_ALWAYS, "Executing Statement --- Error\n");
				 dprintf(D_ALWAYS, "sql = %s\n", sql_stmt.Value());
				 errorSqlStmt = sql_stmt;
				 return QUILL_FAILURE;
			 }

			 clob_comp_expr = condor_ttdb_compare_clob_to_lit(dt, "val", aVal.Value());

			 sql_stmt.formatstr("INSERT INTO daemons_vertical_history (MyType, name, lastreportedtime, attr, val, endtime) SELECT MyType, name, lastreportedtime, attr, val, %s FROM daemons_vertical WHERE MyType = 'Scheduler' AND name = '%s' AND attr = '%s' AND %s", lastReportedTime.Value(), daemonName.Value(), aName.Value(), clob_comp_expr.Value());

			 if (DBObj->execCommand(sql_stmt.Value()) == QUILL_FAILURE) {
				 dprintf(D_ALWAYS, "Executing Statement --- Error\n");
				 dprintf(D_ALWAYS, "sql = %s\n", sql_stmt.Value());		
				 errorSqlStmt = sql_stmt;			 
				 return QUILL_FAILURE;
			 }
		 
			 sql_stmt.formatstr("UPDATE daemons_vertical SET val = '%s', lastreportedtime = %s WHERE MyType = 'Scheduler' AND name = '%s' AND attr = '%s' AND %s", aVal.Value(), lastReportedTime.Value(), daemonName.Value(), aName.Value(), clob_comp_expr.Value());
		 
			 if (DBObj->execCommand(sql_stmt.Value()) == QUILL_FAILURE) {
				 dprintf(D_ALWAYS, "Executing Statement --- Error\n");
				 dprintf(D_ALWAYS, "sql = %s\n", sql_stmt.Value());
				 errorSqlStmt = sql_stmt;	
				 return QUILL_FAILURE;
			 }
		 }
	 }
	 return QUILL_SUCCESS;
}

QuillErrCode TTManager::insertMasterAd(AttrList *ad) {
	AttributeHashSmartPtrType newClAd = AttributeCache.newAttributeHash(); // for holding attributes going to vertical table

	MyString sql_stmt;
	MyString classAd;
	const char *iter;
	
	char *attName = NULL, *attVal;
	MyString aName, aVal, temp;
	MyString attNameList = "";
	MyString attValList = "";
	MyString tmpVal = "";
	MyString inlist = "";
	MyString lastReportedTime = "";
	MyString lastReportedTimeValue = "";
	MyString daemonName = "";

		// previous LastReportedTime from the current classad
    int  prevLHFInAd = 0;
    int  prevLHFInDB = 0;
	int	 ret_st;
	QuillAttrDataType  attr_type;
	int  num_result = 0;

	MyString ts_expr;
	MyString clob_comp_expr;

	const char *data_arr[7];
	QuillAttrDataType   data_typ[7];

		// first generate MyType='Master' attribute
	attNameList.formatstr("(MyType");
	attValList.formatstr("('Master'");

	sPrintAd(classAd, *ad);

	classAd.Tokenize();
	iter = classAd.GetNextToken("\n", true);

	while (iter != NULL)
		{
			attName = (char *)malloc(strlen(iter));
			sscanf(iter, "%s =", attName);
			attVal = (char*)strstr(iter, "= ");
			attVal += 2;

			if (strcasecmp(attName, "PrevLastReportedTime") == 0) {
				prevLHFInAd = atoi(attVal);
			}
						
				/* notice that the Name and LastReportedTime are both a 
				   horizontal daemon attribute and horizontal schedd attribute,
				   therefore we need to check both seperately.
				*/
			if (isHorizontalDaemonAttr(attName, attr_type)) {
				if (strcasecmp(attName, "lastreportedtime") == 0) {
					attNameList += ", ";
					
					attNameList += 
						   "lastreportedtime, lastreportedtime_epoch";
					
				} else {
					attNameList += ", ";
					attNameList += attName;
				}

				attValList += ", ";

				switch (attr_type) {

				case CONDOR_TT_TYPE_STRING:
					if (!stripdoublequotes(attVal)) {
						dprintf(D_ALWAYS, "ERROR: string constant not double quoted for attribute %s in TTManager::insertSchedd\n", attName);
					}						

					if (strcasecmp(attName, ATTR_NAME) == 0) {
						daemonName.formatstr("%s", attVal);
					}

					tmpVal.formatstr("'%s'", attVal);
					break;
				case CONDOR_TT_TYPE_TIMESTAMP:
					time_t clock;
					clock = atoi(attVal);

					ts_expr = condor_ttdb_buildts(&clock, dt);	

					if (ts_expr.IsEmpty()) {
						dprintf(D_ALWAYS, "ERROR: Timestamp expression not built\n");	
						if (attName) {
							free(attName);
							attName = NULL;
						}
						return QUILL_FAILURE;							
					}

					if (strcasecmp(attName, "lastreportedtime") == 0) { 
						tmpVal.formatstr("%s, %s", ts_expr.Value(), attVal);
						lastReportedTime.formatstr("%s", ts_expr.Value());
						lastReportedTimeValue = condor_ttdb_buildtsval(&clock, dt);
					} else {
						tmpVal.formatstr("%s", ts_expr.Value());
					}
					
					break;
				case CONDOR_TT_TYPE_NUMBER:
					tmpVal.formatstr("%s", attVal);
					break;
				default:
					dprintf(D_ALWAYS, "insertMasterAd: unsupported horizontal daemon attribute %s\n", attName);
					if (attName) {
						free(attName);					
						attName = NULL;
					}
					return QUILL_FAILURE;
				}

				attValList += tmpVal;
					
			} else if (strcasecmp(attName, "PrevLastReportedTime") != 0) {
					/* the rest of attributes go to the vertical master
					   table.
					*/
				aName = attName;
				aVal = attVal;

					// insert into new ClassAd to be inserted into DB
				newClAd->insert(aName, aVal);				

					// build an inlist of the vertical attribute names
				if (inlist.IsEmpty()) {
					inlist.formatstr("('%s'", attName);
				} else {
					inlist += ",'";
					inlist += attName;
					inlist += "'";
				}			
			}
			
			if (attName) {
				free (attName);
				attName = NULL;
			}
			iter = classAd.GetNextToken("\n", true);
		}
			
	if (!attNameList.IsEmpty()) attNameList += ")";
	if (!attValList.IsEmpty()) attValList += ")";
	if (!inlist.IsEmpty()) inlist += ")";

	{
			// get the previous lastreportedtime from the database 
		sql_stmt.formatstr("SELECT lastreportedtime_epoch FROM daemons_horizontal WHERE MyType = 'Master' AND Name = '%s'", daemonName.Value());
	
		ret_st = DBObj->execQuery(sql_stmt.Value(), num_result);
	}

	if (ret_st == QUILL_FAILURE) {
		dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		dprintf(D_ALWAYS, "sql = %s\n", sql_stmt.Value());
		errorSqlStmt = sql_stmt;		
		return QUILL_FAILURE;
	}
	else if (ret_st == QUILL_SUCCESS && num_result > 0) {
		prevLHFInDB = atoi(DBObj->getValue(0, 0));		
	}

	DBObj->releaseQueryResult();

		/* move the horizontal daemon attributes tuple to history
		   set end time if the previous lastReportedTime matches, otherwise
		   leave it as NULL (by default)	
		*/
	if (prevLHFInDB == prevLHFInAd) {
		sql_stmt.formatstr("INSERT INTO Daemons_Horizontal_History (MyType, Name, lastReportedTime, MonitorSelfTime, MonitorSelfCPUUsage, MonitorSelfImageSize, MonitorSelfResidentSetSize, MonitorSelfAge, UpdateSequenceNumber, UpdatesTotal, UpdatesSequenced, UpdatesLost, UpdatesHistory, endtime) SELECT MyType, Name, lastReportedTime, MonitorSelfTime, MonitorSelfCPUUsage, MonitorSelfImageSize, MonitorSelfResidentSetSize, MonitorSelfAge, UpdateSequenceNumber, UpdatesTotal, UpdatesSequenced, UpdatesLost, UpdatesHistory, %s FROM Daemons_Horizontal WHERE MyType = 'Master' AND Name = '%s'", lastReportedTime.Value(), daemonName.Value());
	} else {
		sql_stmt.formatstr("INSERT INTO Daemons_Horizontal_History (MyType, Name, lastReportedTime, MonitorSelfTime, MonitorSelfCPUUsage, MonitorSelfImageSize, MonitorSelfResidentSetSize, MonitorSelfAge, UpdateSequenceNumber, UpdatesTotal, UpdatesSequenced, UpdatesLost, UpdatesHistory) SELECT MyType, Name, lastReportedTime, MonitorSelfTime, MonitorSelfCPUUsage, MonitorSelfImageSize, MonitorSelfResidentSetSize, MonitorSelfAge, UpdateSequenceNumber, UpdatesTotal, UpdatesSequenced, UpdatesLost, UpdatesHistory FROM Daemons_Horizontal WHERE MyType = 'Master' AND Name = '%s'", daemonName.Value());
	}
	
	{
		if (DBObj->execCommand(sql_stmt.Value()) == QUILL_FAILURE) {
			dprintf(D_ALWAYS, "Executing Statement --- Error\n");
			dprintf(D_ALWAYS, "sql = %s\n", sql_stmt.Value());
			errorSqlStmt = sql_stmt;
			return QUILL_FAILURE;
		}	
	}

	{
		sql_stmt.formatstr("DELETE FROM  Daemons_Horizontal WHERE MyType = 'Master' AND Name = '%s'", daemonName.Value());

		if (DBObj->execCommand(sql_stmt.Value()) == QUILL_FAILURE) {
			dprintf(D_ALWAYS, "Executing Statement --- Error\n");
			dprintf(D_ALWAYS, "sql = %s\n", sql_stmt.Value());
			errorSqlStmt = sql_stmt;
			return QUILL_FAILURE;
		}
	}

		// insert new tuple into daemons_horizontal 
	sql_stmt.formatstr("INSERT INTO daemons_horizontal %s VALUES %s", attNameList.Value(), attValList.Value());
	
	if (DBObj->execCommand(sql_stmt.Value()) == QUILL_FAILURE) {
		dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		dprintf(D_ALWAYS, "sql = %s\n", sql_stmt.Value());
		errorSqlStmt = sql_stmt;
		return QUILL_FAILURE;
	}		 

		// Make changes into daemons_vertical and daemons_vertical_history	

		/* if the previous lastReportedTime doesn't match, this means the 
		   daemon has been shutdown for a while, we should move everything
		   into the daemons_vertical_history (with a NULL end_time)!
		   if the previous lastReportedTime matches, only move attributes that 
		   don't appear in the new class ad
		*/
	 if (prevLHFInDB == prevLHFInAd) {
		 sql_stmt.formatstr("INSERT INTO Daemons_Vertical_History (MyType, name, lastReportedTime, attr, val, endtime) SELECT MyType, name, lastReportedTime, attr, val, %s FROM Daemons_Vertical WHERE MyType = 'Master' AND name = '%s' AND attr NOT IN %s", lastReportedTime.Value(), daemonName.Value(), inlist.Value());
	 } else {
		 sql_stmt.formatstr("INSERT INTO Daemons_Vertical_History (MyType, Name, lastReportedTime, attr, val) SELECT MyType, name, lastReportedTime, attr, val FROM Daemons_Vertical WHERE MyType = 'Master' AND name = '%s'", daemonName.Value());
	 } 

	 if (DBObj->execCommand(sql_stmt.Value()) == QUILL_FAILURE) {
		 dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		 dprintf(D_ALWAYS, "sql = %s\n", sql_stmt.Value());
		 errorSqlStmt = sql_stmt;
		 return QUILL_FAILURE;
	 }

	 if (prevLHFInDB == prevLHFInAd) {
		 sql_stmt.formatstr("DELETE FROM daemons_vertical WHERE MyType = 'Master' AND name = '%s' AND attr NOT IN %s", daemonName.Value(), inlist.Value());
	 } else {
		 sql_stmt.formatstr("DELETE FROM daemons_vertical WHERE MyType = 'Master' AND name = '%s'", daemonName.Value());
	 }

	 if (DBObj->execCommand(sql_stmt.Value()) == QUILL_FAILURE) {
		 dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		 dprintf(D_ALWAYS, "sql = %s\n", sql_stmt.Value());
		 errorSqlStmt = sql_stmt;
		 return QUILL_FAILURE;
	 }	 

		// Put this set of attributes into our soft-state transaction cache.
		// This transaction cache will be committed when we commit to the 
		// database.
	AttributeCache.insertTransaction(MASTER_HASH, daemonName);

		 // insert the vertical attributes
	 newClAd->startIterations();
	 while (newClAd->iterate(aName, aVal)) {

 			// See if this attribute name/value pair was previously committed
			// into the database.  If so, continue on to the next attribute.
		if ( AttributeCache.alreadyCommitted(aName, aVal) ) {
			// skip
			dprintf(D_FULLDEBUG,"Skipping update of unchanged attr %s\n",aName.Value());
			continue;
		}

		 {
			 sql_stmt.formatstr("INSERT INTO daemons_vertical (MyType, name, attr, val, lastreportedtime) SELECT 'Master', '%s', '%s', '%s', %s FROM dummy_single_row_table WHERE NOT EXISTS (SELECT * FROM daemons_vertical WHERE MyType = 'Master' AND name = '%s' AND attr = '%s')", daemonName.Value(), aName.Value(), aVal.Value(), lastReportedTime.Value(), daemonName.Value(), aName.Value());

			 if (DBObj->execCommand(sql_stmt.Value()) == QUILL_FAILURE) {
				 dprintf(D_ALWAYS, "Executing Statement --- Error\n");
				 dprintf(D_ALWAYS, "sql = %s\n", sql_stmt.Value());
				 errorSqlStmt = sql_stmt;
				 return QUILL_FAILURE;
			 }	 

			 clob_comp_expr = condor_ttdb_compare_clob_to_lit(dt, "val", aVal.Value());

			 sql_stmt.formatstr("INSERT INTO daemons_vertical_history (MyType, name, lastreportedtime, attr, val, endtime) SELECT MyType, name, lastreportedtime, attr, val, %s FROM daemons_vertical WHERE MyType = 'Master' AND name = '%s' AND attr = '%s' AND %s", lastReportedTime.Value(), daemonName.Value(), aName.Value(), clob_comp_expr.Value());

			 if (DBObj->execCommand(sql_stmt.Value()) == QUILL_FAILURE) {
				 dprintf(D_ALWAYS, "Executing Statement --- Error\n");
				 dprintf(D_ALWAYS, "sql = %s\n", sql_stmt.Value());		
				 errorSqlStmt = sql_stmt;
				 return QUILL_FAILURE;
			 }

			 sql_stmt.formatstr("UPDATE daemons_vertical SET val = '%s', lastreportedtime = %s WHERE MyType = 'Master' AND name = '%s' AND attr = '%s' AND %s", aVal.Value(), lastReportedTime.Value(), daemonName.Value(), aName.Value(), clob_comp_expr.Value());
		 
			 if (DBObj->execCommand(sql_stmt.Value()) == QUILL_FAILURE) {
				 dprintf(D_ALWAYS, "Executing Statement --- Error\n");
				 dprintf(D_ALWAYS, "sql = %s\n", sql_stmt.Value());
				 errorSqlStmt = sql_stmt;
				 return QUILL_FAILURE;
			 }
		 }
	 }

	 return QUILL_SUCCESS;
}

QuillErrCode TTManager::insertNegotiatorAd(AttrList *ad) {
	AttributeHashSmartPtrType newClAd = AttributeCache.newAttributeHash(); // for holding attributes going to vertical table

	MyString sql_stmt;
	MyString classAd;
	const char *iter;
	
	char *attName = NULL, *attVal;
	MyString aName, aVal, temp;
	MyString attNameList = "";
	MyString attValList = "";
	MyString tmpVal = "";
	MyString inlist = "";
	MyString lastReportedTime = "";
	MyString daemonName = "";

		// previous LastReportedTime from the current classad
    int  prevLHFInAd = 0;
    int  prevLHFInDB = 0;
	int	 ret_st;
	QuillAttrDataType  attr_type;
	int  num_result = 0;

	MyString ts_expr;
	MyString clob_comp_expr;

		// first generate MyType='Negotiator' attribute
	attNameList.formatstr("(MyType");
	attValList.formatstr("('Negotiator'");

	sPrintAd(classAd, *ad);

	classAd.Tokenize();
	iter = classAd.GetNextToken("\n", true);

	while (iter != NULL)
		{
			attName = (char *)malloc(strlen(iter));
			sscanf(iter, "%s =", attName);
			attVal = (char*)strstr(iter, "= ");
			attVal += 2;

			if (strcasecmp(attName, "PrevLastReportedTime") == 0) {
				prevLHFInAd = atoi(attVal);
			}
			
				/* notice that the Name and LastReportedTime are both a 
				   horizontal daemon attribute and horizontal schedd attribute,
				   therefore we need to check both seperately.
				*/
			if (isHorizontalDaemonAttr(attName, attr_type)) {
				if (strcasecmp(attName, "lastreportedtime") == 0) {
					attNameList += ", ";
					
					attNameList += 
						   "lastreportedtime, lastreportedtime_epoch";
					
				} else {
					attNameList += ", ";
					attNameList += attName;
				}

				attValList += ", ";

				switch (attr_type) {

				case CONDOR_TT_TYPE_STRING:
					if (!stripdoublequotes(attVal)) {
						dprintf(D_ALWAYS, "ERROR: string constant not double quoted for attribute %s in TTManager::insertNegotiatorAd\n", attName);
					}

					if (strcasecmp(attName, ATTR_NAME) == 0) {
						daemonName.formatstr("%s", attVal);
					}

					tmpVal.formatstr("'%s'", attVal);
					break;
				case CONDOR_TT_TYPE_TIMESTAMP:
					time_t clock;
					clock = atoi(attVal);

					ts_expr = condor_ttdb_buildts(&clock, dt);	

					if (ts_expr.IsEmpty()) {
						dprintf(D_ALWAYS, "ERROR: Timestamp expression not built\n");
						if (attName) {
							free(attName);
							attName = NULL;
						}
						return QUILL_FAILURE;
					}

					if (strcasecmp(attName, "lastreportedtime") == 0) {
						tmpVal.formatstr("%s, %s", ts_expr.Value(), attVal);
						lastReportedTime.formatstr("%s", ts_expr.Value());
					} else {
						tmpVal.formatstr("%s", ts_expr.Value());
					}
					
					break;
				case CONDOR_TT_TYPE_NUMBER:
					tmpVal.formatstr("%s", attVal);
					break;
				default:
					dprintf(D_ALWAYS, "insertNegotiatorAd: unsupported horizontal daemon attribute\n");
					if (attName) {
						free(attName);
						attName = NULL;
					}				
					return QUILL_FAILURE;
				}

				attValList += tmpVal;
					
			} else if (strcasecmp(attName, "PrevLastReportedTime") != 0) {
					/* the rest of attributes go to the vertical negotiator 
					   table.
					*/
				aName = attName;
				aVal = attVal;

					// insert into new ClassAd to be inserted into DB
				newClAd->insert(aName, aVal);				

					// build an inlist of the vertical attribute names
				if (inlist.IsEmpty()) {
					inlist.formatstr("('%s'", attName);
				} else {
					inlist += ",'";
					inlist += attName;
					inlist += "'";
				}			
			}

			if (attName) {
				free(attName);
				attName = NULL;
			}
			iter = classAd.GetNextToken("\n", true);
		}	

	if (!attNameList.IsEmpty()) attNameList += ")";
	if (!attValList.IsEmpty()) attValList += ")";
	if (!inlist.IsEmpty()) inlist += ")";

		// get the previous lastreportedtime from the database 
	sql_stmt.formatstr("SELECT lastreportedtime_epoch FROM daemons_horizontal WHERE MyType = 'Negotiator' AND Name = '%s'", daemonName.Value());
	
	ret_st = DBObj->execQuery(sql_stmt.Value(), num_result);

	if (ret_st == QUILL_FAILURE) {
		dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		dprintf(D_ALWAYS, "sql = %s\n", sql_stmt.Value());
		errorSqlStmt = sql_stmt;
		return QUILL_FAILURE;
	}
	else if (ret_st == QUILL_SUCCESS && num_result > 0) {
		prevLHFInDB = atoi(DBObj->getValue(0, 0));		
	}

	DBObj->releaseQueryResult();

		/* move the horizontal daemon attributes tuple to history
		   set end time if the previous lastReportedTime matches, otherwise
		   leave it as NULL (by default)	
		*/
	if (prevLHFInDB == prevLHFInAd) {
		sql_stmt.formatstr("INSERT INTO Daemons_Horizontal_History (MyType, Name, LastReportedTime, MonitorSelfTime, MonitorSelfCPUUsage, MonitorSelfImageSize, MonitorSelfResidentSetSize, MonitorSelfAge, UpdateSequenceNumber, UpdatesTotal, UpdatesSequenced, UpdatesLost, UpdatesHistory, endtime) SELECT MyType, Name, LastReportedTime, MonitorSelfTime, MonitorSelfCPUUsage, MonitorSelfImageSize, MonitorSelfResidentSetSize, MonitorSelfAge, UpdateSequenceNumber, UpdatesTotal, UpdatesSequenced, UpdatesLost, UpdatesHistory, %s FROM Daemons_Horizontal WHERE MyType = 'Negotiator' AND Name = '%s'", lastReportedTime.Value(), daemonName.Value());
	} else {
		sql_stmt.formatstr("INSERT INTO Daemons_Horizontal_History (MyType, Name, LastReportedTime, MonitorSelfTime, MonitorSelfCPUUsage, MonitorSelfImageSize, MonitorSelfResidentSetSize, MonitorSelfAge, UpdateSequenceNumber, UpdatesTotal, UpdatesSequenced, UpdatesLost, UpdatesHistory) SELECT MyType, Name, LastReportedTime, MonitorSelfTime, MonitorSelfCPUUsage, MonitorSelfImageSize, MonitorSelfResidentSetSize, MonitorSelfAge, UpdateSequenceNumber, UpdatesTotal, UpdatesSequenced, UpdatesLost, UpdatesHistory FROM Daemons_Horizontal WHERE MyType = 'Negotiator' AND Name = '%s'", daemonName.Value());
	}
	
	if (DBObj->execCommand(sql_stmt.Value()) == QUILL_FAILURE) {
		dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		dprintf(D_ALWAYS, "sql = %s\n", sql_stmt.Value());
		errorSqlStmt = sql_stmt;
		return QUILL_FAILURE;
	}

	sql_stmt.formatstr("DELETE FROM  Daemons_Horizontal WHERE MyType = 'Negotiator' AND Name = '%s'", daemonName.Value());

	if (DBObj->execCommand(sql_stmt.Value()) == QUILL_FAILURE) {
		dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		dprintf(D_ALWAYS, "sql = %s\n", sql_stmt.Value());
		errorSqlStmt = sql_stmt;
		return QUILL_FAILURE;
	}
		// insert new tuple into daemons_horizontal 
	sql_stmt.formatstr("INSERT INTO daemons_horizontal %s VALUES %s", attNameList.Value(), attValList.Value());
	
	if (DBObj->execCommand(sql_stmt.Value()) == QUILL_FAILURE) {
		dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		dprintf(D_ALWAYS, "sql = %s\n", sql_stmt.Value());
		errorSqlStmt = sql_stmt;
		return QUILL_FAILURE;
	}

		/* Make changes into daemons_vertical and 
		   daemons_vertical_history
		*/

		/* if the previous lastReportedTime doesn't match, this means the 
		   daemon has been shutdown for a while, we should move everything
		   into the daemons_vertical_history (with a NULL end_time)!
		   if the previous lastReportedTime matches, only move attributes that 
		   don't appear in the new class ad
		*/
	 if (prevLHFInDB == prevLHFInAd) {
		 sql_stmt.formatstr("INSERT INTO Daemons_Vertical_History (MyType, Name, LastReportedTime, attr, val, endtime) SELECT MyType, name, lastreportedtime, attr, val, %s FROM Daemons_Vertical WHERE MyType = 'Negotiator' AND name = '%s' AND attr NOT IN %s", lastReportedTime.Value(), daemonName.Value(), inlist.Value());
	 } else {
		 sql_stmt.formatstr("INSERT INTO Daemons_Vertical_History (MyType, Name, LastReportedTime, attr, val) SELECT MyType, name, lastreportedtime, attr, val FROM Daemons_Vertical WHERE MyType = 'Negotiator' AND name = '%s'", daemonName.Value());
	 }

	 if (DBObj->execCommand(sql_stmt.Value()) == QUILL_FAILURE) {
		 dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		 dprintf(D_ALWAYS, "sql = %s\n", sql_stmt.Value());
		 errorSqlStmt = sql_stmt;
		 return QUILL_FAILURE;
	 }

	 if (prevLHFInDB == prevLHFInAd) {
		 sql_stmt.formatstr("DELETE FROM daemons_vertical WHERE MyType = 'Negotiator' AND name = '%s' AND attr NOT IN %s", daemonName.Value(), inlist.Value());
	 } else {
		 sql_stmt.formatstr("DELETE FROM daemons_vertical WHERE MyType = 'Negotiator' AND name = '%s'", daemonName.Value());
	 }

	 if (DBObj->execCommand(sql_stmt.Value()) == QUILL_FAILURE) {
		 dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		 dprintf(D_ALWAYS, "sql = %s\n", sql_stmt.Value());
		 errorSqlStmt = sql_stmt;
		 return QUILL_FAILURE;
	 }	 

 		// Put this set of attributes into our soft-state transaction cache.
		// This transaction cache will be committed when we commit to the 
		// database.
	AttributeCache.insertTransaction(NEGOTIATOR_HASH, daemonName);

		 // insert the vertical attributes
	 newClAd->startIterations();
	 while (newClAd->iterate(aName, aVal)) {

			// See if this attribute name/value pair was previously committed
			// into the database.  If so, continue on to the next attribute.
		if ( AttributeCache.alreadyCommitted(aName, aVal) ) {
			// skip
			dprintf(D_FULLDEBUG,"Skipping update of unchanged attr %s\n",aName.Value());
			continue;
		}

		 sql_stmt.formatstr("INSERT INTO daemons_vertical (MyType, name, attr, val, lastreportedtime) SELECT 'Negotiator', '%s', '%s', '%s', %s FROM dummy_single_row_table WHERE NOT EXISTS (SELECT * FROM daemons_vertical WHERE MyType = 'Negotiator' AND name = '%s' AND attr = '%s')", daemonName.Value(), aName.Value(), aVal.Value(), lastReportedTime.Value(), daemonName.Value(), aName.Value());

		 if (DBObj->execCommand(sql_stmt.Value()) == QUILL_FAILURE) {
			 dprintf(D_ALWAYS, "Executing Statement --- Error\n");
			 dprintf(D_ALWAYS, "sql = %s\n", sql_stmt.Value());
			 errorSqlStmt = sql_stmt;
			 return QUILL_FAILURE;
		 }	 

		 clob_comp_expr = condor_ttdb_compare_clob_to_lit(dt, "val", aVal.Value());

		 sql_stmt.formatstr("INSERT INTO daemons_vertical_history (MyType, name, lastreportedtime, attr, val, endtime) SELECT MyType, name, lastreportedtime, attr, val, %s FROM daemons_vertical WHERE MyType = 'Negotiator' AND name = '%s' AND attr = '%s' AND %s", lastReportedTime.Value(), daemonName.Value(), aName.Value(), clob_comp_expr.Value());

		 if (DBObj->execCommand(sql_stmt.Value()) == QUILL_FAILURE) {
			 dprintf(D_ALWAYS, "Executing Statement --- Error\n");
			 dprintf(D_ALWAYS, "sql = %s\n", sql_stmt.Value());	
			 errorSqlStmt = sql_stmt;
			 return QUILL_FAILURE;
		 }

		 sql_stmt.formatstr("UPDATE daemons_vertical SET val = '%s', lastreportedtime = %s WHERE MyType = 'Negotiator' AND name = '%s' AND attr = '%s' AND %s", aVal.Value(), lastReportedTime.Value(), daemonName.Value(), aName.Value(), clob_comp_expr.Value());
		 
		 if (DBObj->execCommand(sql_stmt.Value()) == QUILL_FAILURE) {
			dprintf(D_ALWAYS, "Executing Statement --- Error\n");
			dprintf(D_ALWAYS, "sql = %s\n", sql_stmt.Value());
			errorSqlStmt = sql_stmt;
			return QUILL_FAILURE;
		 }
	 }
	
	return QUILL_SUCCESS;
}

QuillErrCode TTManager::insertBasic(AttrList *ad, char *tableName) {
	MyString sql_stmt;
	MyString classAd;
	const char *iter;	
	MyString newvalue;

	char *attName = NULL, *attVal;
	MyString attNameList = "";
	MyString attValList = "";
	int isFirst = TRUE;
	bool isMatches = FALSE, isRejects = FALSE, 
		isThrown = FALSE, isGeneric = FALSE;
	bool isString = FALSE;

	if (strcasecmp(tableName, "Matches") == 0) 
		isMatches = TRUE;

	else if (strcasecmp(tableName, "Rejects") == 0) 
		isRejects = TRUE;
        
	else if (strcasecmp(tableName, "Throwns") == 0) 
		isThrown = TRUE;

	else if (strcasecmp(tableName, "generic_messages") == 0) 
		isGeneric = TRUE;

	sPrintAd(classAd, *ad);

	classAd.Tokenize();
	iter = classAd.GetNextToken("\n", true);

	while (iter != NULL)
		{
				// the attribute name can't be longer than the log entry line size
			attName = (char *)malloc(strlen(iter));
			
			sscanf(iter, "%s =", attName);
			attVal = (char*)strstr(iter, "= ");
			attVal += 2;

			if ((isMatches && 
				 (strcasecmp(attName, "match_time") == 0)) ||
				(isRejects && 
				 (strcasecmp(attName, "reject_time") == 0)) ||
				(isThrown && 
				 (strcasecmp(attName, "throwtime") == 0)) ||
				(isGeneric && 
				 (strcasecmp(attName, "eventtime") == 0))) {
					/* all timestamp value must be passed as an integer of 
					   seconds from epoch time.
					*/
				time_t clock;

				clock = atoi(attVal);

				newvalue = condor_ttdb_buildts(&clock, dt);

				if (newvalue.IsEmpty()) {
					dprintf(D_ALWAYS, "ERROR: Timestamp expression not built in TTManager::insertBasic\n");
					if (attName) {
						free(attName);
						attName = NULL;
					}
					return QUILL_FAILURE;							
				}	
				
			} else {
					/* for other values, check if it contains escape char 
					   escape single quote if any within the value
					*/
				newvalue = condor_ttdb_fillEscapeCharacters(attVal, dt);
			}

			if (isFirst) {
					//is the first in the list
				isFirst = FALSE;

				attNameList.formatstr("(%s", attName);
					/* if the value begins with a double quote, it means that
					   it is a string value and should be single quoted. 
					   This assumption is made in all functions where 	
					   we treat a column which we don't have prior knowledge
					   of its data type 
					*/
				if (newvalue[0] == '"') {
					stripdoublequotes_MyString(newvalue);
					attValList.formatstr("('%s'", newvalue.Value());
				} else {
					attValList.formatstr("(%s", newvalue.Value());
				}
			} else {					
						// is not the first in the list
				attNameList += ", ";
				attNameList += attName;
				attValList += ", ";				
				if (newvalue[0] == '"') {
					attValList += "'";
					stripdoublequotes_MyString(newvalue);
					isString = TRUE;
				} else 
					isString = FALSE;
				
				attValList += newvalue;
				if (isString) {
					attValList += "'";
				}
			}

			free(attName);
			iter = classAd.GetNextToken("\n", true);
		}

	if (!attNameList.IsEmpty()) attNameList += ")";
	if (!attValList.IsEmpty()) attValList += ")";

	sql_stmt.formatstr("INSERT INTO %s %s VALUES %s", tableName, attNameList.Value(), attValList.Value());

	if (DBObj->execCommand(sql_stmt.Value()) == QUILL_FAILURE) {
		dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		dprintf(D_ALWAYS, "sql = %s\n", sql_stmt.Value());
		errorSqlStmt = sql_stmt;
		return QUILL_FAILURE;
	}

	return QUILL_SUCCESS;
}

QuillErrCode TTManager::insertRuns(AttrList *ad) {
	MyString sql_stmt;
	MyString classAd;
	const char *iter;	
	MyString newvalue;

	char *attName = NULL, *attVal;
    MyString attNameList = "";
	MyString attValList = "";

	MyString runid_expr;
	bool isString = FALSE;

		// first generate runid attribute
	runid_expr = condor_ttdb_buildseq(dt, "SeqRunId");

	if (runid_expr.IsEmpty()) {
		dprintf(D_ALWAYS, "Sequence expression not build in TTManager::insertRuns\n");
		return QUILL_FAILURE;
	}

	attNameList.formatstr("(run_id");
	attValList.formatstr("(%s", runid_expr.Value());

	sPrintAd(classAd, *ad);

	classAd.Tokenize();
	iter = classAd.GetNextToken("\n", true);

	while (iter != NULL)
		{
				// the attribute name can't be longer than the log entry line size
			attName = (char *)malloc(strlen(iter));
			
			sscanf(iter, "%s =", attName);
			attVal = (char*)strstr(iter, "= ");
			attVal += 2;

			if ((strcasecmp(attName, "startts") == 0) || 
				(strcasecmp(attName, "endts") == 0)) {
				time_t clock;
				clock = atoi(attVal);
				newvalue = condor_ttdb_buildts(&clock, dt);

				if (newvalue.IsEmpty()) {
					dprintf(D_ALWAYS, "ERROR: Timestamp expression not built in TTManager::insertRuns\n");
					if (attName) {
						free(attName);
						attName = NULL;
					}
					return QUILL_FAILURE;
				}
				
			} else {

					// escape single quote if any within the value
				newvalue = condor_ttdb_fillEscapeCharacters(attVal, dt);
			}

				// is not the first in the list
			attNameList += ", ";
			attNameList += attName;
			attValList += ", ";
			if (newvalue[0] == '"') {
				attValList += "'";
				stripdoublequotes_MyString(newvalue);
				isString = TRUE;
			} else
				isString = FALSE;

			attValList += newvalue;
			if (isString) {
				attValList += "'";
			}

			free(attName);
			iter = classAd.GetNextToken("\n", true);
		}

	if (!attNameList.IsEmpty()) attNameList += ")";
	if (!attValList.IsEmpty()) attValList += ")";

	sql_stmt.formatstr("INSERT INTO Runs %s VALUES %s", attNameList.Value(), attValList.Value());

	if (DBObj->execCommand(sql_stmt.Value()) == QUILL_FAILURE) {
		dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		dprintf(D_ALWAYS, "sql = %s\n", sql_stmt.Value());
		errorSqlStmt = sql_stmt;
		return QUILL_FAILURE;
	}

	return QUILL_SUCCESS;
}

QuillErrCode TTManager::insertEvents(AttrList *ad) {
	MyString sql_stmt;
	MyString classAd;
	const char *iter;	
	char *attName = NULL, *attVal;
	MyString scheddname = "";
	MyString cluster = "";
	MyString proc = "";
	MyString gjid = "";
	MyString subproc = "";
	MyString eventts = "";
	MyString messagestr = "";

	int eventtype;
	MyString newvalue;

	sPrintAd(classAd, *ad);

	classAd.Tokenize();
	iter = classAd.GetNextToken("\n", true);

	while (iter != NULL)
		{
				// the attribute name can't be longer than the log entry line size
			attName = (char *)malloc(strlen(iter));
			
			sscanf(iter, "%s =", attName);
			attVal = (char*)strstr(iter, "= ");
			attVal += 2;

			if (strcasecmp(attName, "eventtime") == 0) {
				time_t clock;
				clock = atoi(attVal);
				newvalue = condor_ttdb_buildts(&clock, dt);

				if (newvalue.IsEmpty()) {
					dprintf(D_ALWAYS, "ERROR: Timestamp expression not built in TTManager::insertEvents\n");
					free(attName);
					return QUILL_FAILURE;
				}
				
			} else {
					// strip double qutoes if any
				stripdoublequotes(attVal);

					// escape single quote if any within the value
				newvalue = condor_ttdb_fillEscapeCharacters(attVal, dt);
			}

			if (strcasecmp(attName, "scheddname") == 0) {
				scheddname = newvalue;
			} else if (strcasecmp(attName, "cluster_id") == 0) {
				cluster = newvalue;
			} else if (strcasecmp(attName, "proc_id") == 0) {
				proc = newvalue;
			} else if (strcasecmp(attName, "spid") == 0) {
				subproc = newvalue;
			} else if (strcasecmp(attName, "globaljobid") == 0) {
				gjid = newvalue;
			} else if (strcasecmp(attName, "eventtype") == 0) {
				eventtype = atoi(newvalue.Value());
			} else if (strcasecmp(attName, "eventtime") == 0) {
				eventts = newvalue;
			} else if (strcasecmp(attName, "description") == 0) {
				messagestr = newvalue;
			}
			
			free(attName);
			iter = classAd.GetNextToken("\n", true);
		}

	if (eventtype == ULOG_JOB_ABORTED || eventtype == ULOG_JOB_HELD || ULOG_JOB_RELEASED) {
		sql_stmt.formatstr("INSERT INTO events (scheddname, cluster_id, proc_id, globaljobid, eventtype, eventtime, description) VALUES ('%s', %s, %s, '%s', %d, %s, '%s')", 
				scheddname.Value(), cluster.Value(), proc.Value(), gjid.Value(), eventtype, eventts.Value(), messagestr.Value());
	} else {
		sql_stmt.formatstr("INSERT INTO events (scheddname, cluster_id, proc_id, run_id, eventtype, eventtime, description) SELECT '%s', %s, %s, run_id, %d, %s, '%s'  FROM runs WHERE scheddname = '%s'  AND cluster_id = %s and proc_id = %s AND spid = %s AND endtype is null", scheddname.Value(), cluster.Value(), proc.Value(), eventtype, eventts.Value(), messagestr.Value(), scheddname.Value(), cluster.Value(), proc.Value(), subproc.Value());
	}

	if (DBObj->execCommand(sql_stmt.Value()) == QUILL_FAILURE) {
		dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		dprintf(D_ALWAYS, "sql = %s\n", sql_stmt.Value());
		errorSqlStmt = sql_stmt;
		return QUILL_FAILURE;
	}

	return QUILL_SUCCESS;
}

QuillErrCode TTManager::insertFiles(AttrList *ad) {
	MyString sql_stmt;
	MyString classAd;
	const char *iter;	
	char *attName = NULL, *attVal;
	MyString seqexpr;

	char f_name[_POSIX_PATH_MAX] = "", f_host[50] = "", 
		f_path[_POSIX_PATH_MAX] = "", f_ts[30] = "";
	int f_size;
	char pathname[_POSIX_PATH_MAX] = "";
	char hexSum[MAC_SIZE*2+1] = "", sum[MAC_SIZE+1] = "";	
	bool fileSame = TRUE;
	struct stat file_status;
	time_t old_ts;
	MyString ts_expr;

	sPrintAd(classAd, *ad);

	classAd.Tokenize();
	iter = classAd.GetNextToken("\n", true);

	while (iter != NULL)
		{
				// the attribute name can't be longer than the log entry line size
			attName = (char *)malloc(strlen(iter));
			
			sscanf(iter, "%s =", attName);
			attVal = (char*)strstr(iter, "= ");
			attVal += 2;

				// strip double quotes if any
			stripdoublequotes(attVal);

			if (strcasecmp(attName, "f_name") == 0) {
				strcpy(f_name, attVal);
			} else if (strcasecmp(attName, "f_host") == 0) {
				strcpy(f_host, attVal);
			} else if (strcasecmp(attName, "f_path") == 0) {
				strcpy(f_path, attVal);
			} else if (strcasecmp(attName, "f_ts") == 0) {
				strcpy(f_ts, attVal);
			} else if (strcasecmp(attName, "f_size") == 0) {
				f_size = atoi(attVal);
			}
			
			free(attName);

			iter = classAd.GetNextToken("\n", true);
		}

		/* build timestamp expression */
	old_ts = atoi(f_ts);
	ts_expr = condor_ttdb_buildts(&old_ts, dt);	

	if (ts_expr.IsEmpty()) {
		dprintf(D_ALWAYS, "ERROR: Timestamp expression not built in TTManager::insertFiles\n");

		return QUILL_FAILURE;
	}	
	
	sprintf(pathname, "%s/%s", f_path, f_name);

		// check if the file is still there and the same
	if (stat(pathname, &file_status) < 0) {
		dprintf(D_FULLDEBUG, "ERROR in TTManager::insertFiles: File '%s' can not be accessed.\n", 
				pathname);
		fileSame = FALSE;
	} else {
		if (old_ts != file_status.st_mtime) {
			fileSame = FALSE;
		}
	}

  	if (fileSame && (f_size > 0) && file_checksum(pathname, f_size, sum)) {
		int i;
  		for (i = 0; i < MAC_SIZE; i++)
  			sprintf(&hexSum[2*i], "%2x", sum[i]);		
  		hexSum[2*MAC_SIZE] = '\0';
  	}
  	else
		hexSum[0] = '\0';

	seqexpr = condor_ttdb_buildseq(dt, "condor_seqfileid");

	if (seqexpr.IsEmpty()) {
		dprintf(D_ALWAYS, "Sequence expression not built in TTManager::insertFiles\n");
		return QUILL_FAILURE;
	}

	sql_stmt.formatstr(
			"INSERT INTO files (file_id, name, host, path, lastmodified, filesize, checksum) SELECT %s, '%s', '%s', '%s', %s, %d, '%s' FROM dummy_single_row_table WHERE NOT EXISTS (SELECT * FROM files WHERE  name='%s' and path='%s' and host='%s' and lastmodified=%s)", seqexpr.Value(), f_name, f_host, f_path, ts_expr.Value(), f_size, hexSum, f_name, f_path, f_host, ts_expr.Value());

	if (DBObj->execCommand(sql_stmt.Value()) == QUILL_FAILURE) {
		dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		dprintf(D_ALWAYS, "sql = %s\n", sql_stmt.Value());
		errorSqlStmt = sql_stmt;
		return QUILL_FAILURE;
	}

	return QUILL_SUCCESS;
}

QuillErrCode TTManager::insertFileusages(AttrList *ad) {
	MyString sql_stmt;
	MyString classAd;
	const char *iter;	
	char *attName, *attVal;
	
	char f_name[_POSIX_PATH_MAX] = "", f_host[50] = "", f_path[_POSIX_PATH_MAX] = "", f_ts[30] = "", globaljobid[100] = "", type[20] = "";
	int f_size;

	time_t clock;
	MyString ts_expr;
	MyString onerow_expr;

	sPrintAd(classAd, *ad);

	classAd.Tokenize();
	iter = classAd.GetNextToken("\n", true);

	while (iter != NULL)
		{
				// the attribute name can't be longer than the log entry line size
			attName = (char *)malloc(strlen(iter));
			
			sscanf(iter, "%s =", attName);
			attVal = (char*)strstr(iter, "= ");
			attVal += 2;

			stripdoublequotes(attVal);

			if (strcasecmp(attName, "f_name") == 0) {
				strcpy(f_name, attVal);
			} else if (strcasecmp(attName, "f_host") == 0) {
				strcpy(f_host, attVal);
			} else if (strcasecmp(attName, "f_path") == 0) {
				strcpy(f_path, attVal);
			} else if (strcasecmp(attName, "f_ts") == 0) {
				strcpy(f_ts, attVal);
			} else if (strcasecmp(attName, "f_size") == 0) {
				f_size = atoi(attVal);
			} else if (strcasecmp(attName, "globalJobId") == 0) {
				strcpy(globaljobid, attVal);
			} else if (strcasecmp(attName, "type") == 0) {
				strcpy(type, attVal);
			}

			free(attName);
			iter = classAd.GetNextToken("\n", true);
		}

	clock = atoi(f_ts);
	ts_expr = condor_ttdb_buildts(&clock, dt);	

	if (ts_expr.IsEmpty()) {
		dprintf(D_ALWAYS, "ERROR: Timestamp expression not built in TTManager::insertFileusages\n");

		return QUILL_FAILURE;
	}

	onerow_expr = condor_ttdb_onerow_clause(dt);	

	if (onerow_expr.IsEmpty()) {
		dprintf(D_ALWAYS, "ERROR: LIMIT 1 expression not built in TTManager::insertFileusages\n");

		return QUILL_FAILURE;
	}

	sql_stmt.formatstr(
			"INSERT INTO fileusages (globaljobid, file_id, usagetype) SELECT '%s', file_id, '%s' FROM files WHERE  name='%s' and path='%s' and host='%s' and lastmodified=%s %s", globaljobid, type, f_name, f_path, f_host, ts_expr.Value(), onerow_expr.Value());
	
	if (DBObj->execCommand(sql_stmt.Value()) == QUILL_FAILURE) {
		dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		dprintf(D_ALWAYS, "sql = %s\n", sql_stmt.Value());
		errorSqlStmt = sql_stmt;
		return QUILL_FAILURE;
	}

	return QUILL_SUCCESS;
}

QuillErrCode TTManager::insertHistoryJob(AttrList *ad) {

	return insertHistoryJobCommon(ad, DBObj, dt, errorSqlStmt, jqDBManager.getScheddname(), jqDBManager.getScheddbirthdate());
}

QuillErrCode TTManager::insertTransfers(AttrList *ad) {
  MyString sql_stmt;
  MyString classAd;
  const char *iter;
  char *attName = NULL, *attVal;

  char globaljobid[100];
  char src_name[_POSIX_PATH_MAX] = "", src_host[50] = "",
    src_path[_POSIX_PATH_MAX] = "";
  char dst_name[_POSIX_PATH_MAX] = "", dst_host[50] = "",
    dst_path[_POSIX_PATH_MAX] = "";
  char path[_POSIX_PATH_MAX] = "", name[_POSIX_PATH_MAX] = "";
  char pathname[2*_POSIX_PATH_MAX];
  char dst_daemon[15];
  char f_ts[30];
  int src_port = 0,dst_port = 0;
  int delegation_method_id = 0;
  char is_encrypted[6] = "";
  time_t old_ts;
  MyString last_modified;
  int transfer_size, elapsed;
  int f_size;
  bool fileSame = TRUE;
  struct stat file_status;
  char hexSum[MAC_SIZE*2+1] = "", sum[MAC_SIZE+1] = "";
  time_t transfer_time;
  MyString transfer_time_expr;

  sPrintAd(classAd, *ad);

  classAd.Tokenize();
  iter = classAd.GetNextToken("\n", true);

  while (iter != NULL) {
    // the attribute name can't be longer than the log entry line size
    attName = (char *)malloc(strlen(iter));
    sscanf(iter, "%s =", attName);
    attVal = (char*)strstr(iter, "= ");
    attVal += 2;

	stripdoublequotes(attVal);

    if (strcasecmp(attName, "globaljobid") == 0) {
      strncpy(globaljobid, attVal, 100);
    } else if (strcasecmp(attName, "src_name") == 0) {
      strncpy(src_name, attVal, _POSIX_PATH_MAX);
    } else if (strcasecmp(attName, "src_host") == 0) {
      strncpy(src_host, attVal, 50);
    } else if (strcasecmp(attName, "src_port") == 0) {
      src_port = atoi(attVal); 
    } else if (strcasecmp(attName, "src_path") == 0) {
      strncpy(src_path, attVal, _POSIX_PATH_MAX);
    } else if (strcasecmp(attName, "dst_name") == 0) {
      strncpy(dst_name, attVal, _POSIX_PATH_MAX);
    } else if (strcasecmp(attName, "dst_host") == 0) {
      strncpy(dst_host, attVal, 50);
    } else if (strcasecmp(attName, "dst_port") == 0) {
      dst_port = atoi(attVal); 
    } else if (strcasecmp(attName, "dst_path") == 0) {
      strncpy(dst_path, attVal, _POSIX_PATH_MAX);
    } else if (strcasecmp(attName, "dst_daemon") == 0) {
      strncpy(dst_daemon, attVal, 15);
    } else if (strcasecmp(attName, "f_ts") == 0) {
      strncpy(f_ts, attVal, 30);
    } else if (strcasecmp(attName, "transfer_size") == 0) {
      transfer_size = atoi(attVal);
    } else if (strcasecmp(attName, "elapsed") == 0) {
      elapsed = atoi(attVal);
    } else if (strcasecmp(attName, "transfer_time") == 0) {
      transfer_time = atoi(attVal);
    } else if (strcasecmp(attName, "is_encrypted") == 0) {
      strncpy(is_encrypted, attVal, 6);
    } else if (strcasecmp(attName, "delegation_method_id") == 0) {
      delegation_method_id = atoi(attVal);
    }

    free(attName);
    iter = classAd.GetNextToken("\n", true);
  }

  // Build timestamp expressions 
  old_ts = atoi(f_ts);
  last_modified = condor_ttdb_buildts(&old_ts, dt);

  if (last_modified.IsEmpty()) {
    dprintf(D_ALWAYS, "ERROR: Timestamp expression not build in TTManager::insertTransfers 1\n");
    return QUILL_FAILURE;
  }

  transfer_time_expr = condor_ttdb_buildts(&transfer_time, dt);

  if (transfer_time_expr.IsEmpty()) {
    dprintf(D_ALWAYS, "ERROR: Timestamp expression not build in TTManager::insertTransfers 2\n");
    return QUILL_FAILURE;
  }

  // Compute the checksum
  // We don't want to use the file on the starter side since it is
  // a temporary file
  if(strcmp(dst_daemon, "STARTER") == 0)  {
    strncpy(path, src_path, _POSIX_PATH_MAX);
    strncpy(name, src_name, _POSIX_PATH_MAX);
  } else {
    strncpy(path, dst_path, _POSIX_PATH_MAX);
    strncpy(name, dst_name, _POSIX_PATH_MAX);

	// Temporary hack - src_path is never right when downloading in the shadow
	// so always make this NULL
	src_path[0] = '\0';
  }

  sprintf(pathname, "%s/%s", path, name);

  // Check if file is still there with same last modified time
  if (stat(pathname, &file_status) < 0) {
	  dprintf(D_FULLDEBUG, "ERROR in TTManager::insertTransfers: File '%s' can not be accessed.\n", 
			  pathname);
	  fileSame = FALSE;
  } else {
	  if (old_ts != file_status.st_mtime) {
		  fileSame = FALSE;
	  }
  }

  f_size = file_status.st_size;
  if (fileSame && (f_size > 0) && file_checksum(pathname, f_size, sum)) {
		int i;
    for (i = 0; i < MAC_SIZE; i++)
      sprintf(&hexSum[2*i], "%2x", sum[i]);		
    hexSum[2*MAC_SIZE] = '\0';
  }
  else
		hexSum[0] = '\0';
  
  sql_stmt.formatstr(
          "INSERT INTO transfers (globaljobid, src_name, src_host, src_port, src_path, dst_name, dst_host, dst_port, dst_path, transfer_size_bytes, elapsed, dst_daemon, checksum, last_modified, transfer_time, is_encrypted, delegation_method_id) VALUES ('%s', '%s', '%s', %d, '%s', '%s', '%s', %d, '%s', %d, %d, '%s', '%s', %s, %s,'%s',%d)", globaljobid, src_name, src_host, src_port, src_path, dst_name, dst_host, dst_port, dst_path, transfer_size, elapsed, dst_daemon, hexSum, last_modified.Value(), transfer_time_expr.Value(),is_encrypted,delegation_method_id);

	if (DBObj->execCommand(sql_stmt.Value()) == QUILL_FAILURE) {
		dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		dprintf(D_ALWAYS, "sql = %s\n", sql_stmt.Value());
		errorSqlStmt = sql_stmt;
		return QUILL_FAILURE;
	}

	return QUILL_SUCCESS;
}

QuillErrCode TTManager::updateBasic(AttrList *info, AttrList *condition, 
									char *tableName) {
	MyString sql_stmt;
	MyString classAd, classAd1;
	const char *iter;	
	MyString setList = "";
	MyString whereList = "";
	char *attName = NULL, *attVal;
	MyString newvalue;
	bool isRuns = FALSE;
	bool isString = FALSE;

	if (strcasecmp (tableName, "Runs") == 0) {
		isRuns = TRUE;
	}

	if (!info) return QUILL_SUCCESS;

	sPrintAd(classAd, *info);

	classAd.Tokenize();
	iter = classAd.GetNextToken("\n", true);	

	while (iter != NULL)
		{
				// the attribute name can't be longer than the log entry line size
			attName = (char *)malloc(strlen(iter));
			
			sscanf(iter, "%s =", attName);
			attVal = (char*)strstr(iter, "= ");
			attVal += 2;

			if (isRuns && (strcasecmp(attName, "endts") == 0)) {
				time_t clock;
				clock = atoi(attVal);
				newvalue = condor_ttdb_buildts(&clock, dt);

				if (newvalue.IsEmpty()) {
					dprintf(D_ALWAYS, "ERROR: Timestamp expression not built in TTManager::insertRuns\n");
					free(attName);
					return QUILL_FAILURE;
				}
				
			} else {
			
					// escape single quote if any within the value
				newvalue = condor_ttdb_fillEscapeCharacters(attVal, dt);
			}
			
			setList += attName;
			setList += " = ";
			if (newvalue[0] == '"') {
				setList += "'";
				stripdoublequotes_MyString(newvalue);
				isString = TRUE;
			} else
				isString = FALSE;

			setList += newvalue;
			if (isString) {
				setList += "'";
			}			
			setList += ", ";
			
			free(attName);

			iter = classAd.GetNextToken("\n", true);
		}

		// remove the last comma
	setList.setChar(setList.Length()-2, '\0');

	if (condition) {
		sPrintAd(classAd1, *condition);

		classAd1.Tokenize();
		iter = classAd1.GetNextToken("\n", true);	

		while (iter != NULL)
			{
					// the attribute name can't be longer than the log entry line size
				attName = (char *)malloc(strlen(iter));
				
				sscanf(iter, "%s =", attName);
				attVal = (char*)strstr(iter, "= ");
				attVal += 2;			

					// change smth=null (in classad) to smth is null (in sql)
				if (strcasecmp(attVal, "null") == 0) {
					whereList += attName;
				    whereList += " is null and ";

					free(attName);

					iter = classAd1.GetNextToken("\n", true);
					continue;
				}

				if (isRuns && (strcasecmp(attName, "endts") == 0)) {
					time_t clock;
					clock = atoi(attVal);
					newvalue = condor_ttdb_buildts(&clock, dt);

					if (newvalue.IsEmpty()) {
						dprintf(D_ALWAYS, "ERROR: Timestamp expression not built in TTManager::insertRuns\n");
						free(attName);
						return QUILL_FAILURE;
					}
					
				} else {					
					newvalue = attVal;
				}
				
				whereList += attName;
				whereList += " = ";

				if (newvalue[0] == '"') {
					whereList += "'";
					stripdoublequotes_MyString(newvalue);
					isString = TRUE;
				} else
					isString = FALSE;
				
				whereList += newvalue;
				if (isString) {
					whereList += "'";
				}	

				whereList += " and ";

				free(attName);

				iter = classAd1.GetNextToken("\n", true);
			}
		
			// remove the last " and "
		whereList.setChar(whereList.Length()-5, '\0');
	}

		// build sql stmt
	sql_stmt.formatstr("UPDATE %s SET %s WHERE %s", tableName, setList.Value(), whereList.Value());		
	
	if (DBObj->execCommand(sql_stmt.Value()) == QUILL_FAILURE) {
		dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		dprintf(D_ALWAYS, "sql = %s\n", sql_stmt.Value());
		errorSqlStmt = sql_stmt;
		return QUILL_FAILURE;
	}
	
	return QUILL_SUCCESS;
}

void TTManager::handleErrorSqlLog() 
{
	MyString hostname_val;
	MyString ts_expr;
	MyString sql_stmt;
	struct stat file_status;
	time_t f_ts; 
	int src = -1;
	char buffer[2050];
	int rv;
	FILESQL *filesqlobj = NULL;
	MyString newvalue;

	filesqlobj = new FILESQL(currentSqlLog.Value(), O_RDWR, true);
	
	if (!filesqlobj) goto ERROREXIT;
	
	if (filesqlobj->file_open() == QUILL_FAILURE) {
		goto ERROREXIT;
	}

	if (filesqlobj->file_lock() == QUILL_FAILURE) {
		goto ERROREXIT;
	}		

		/* get the last modified time for the sql log */
	stat(currentSqlLog.Value(), &file_status);
	f_ts = file_status.st_mtime;
	
	ts_expr = condor_ttdb_buildts(&f_ts, dt);

		/* get the host name where the sql log sits */
	hostname_val = my_full_hostname();

		/* escape quotation in error sql */
	newvalue = condor_ttdb_fillEscapeCharacters(errorSqlStmt.Value(), dt);

		/* insert an entry into the Error_Sqllogs table */
	sql_stmt.formatstr("INSERT INTO Error_Sqllogs (LogName, Host, LastModified, ErrorSql, LogBody, ErrorMessage) VALUES ('%s', '%s', %s, '%s', '', '%s')", 
					 currentSqlLog.Value(), 
					 hostname_val.Value(), 
					 ts_expr.Value(), 
					 newvalue.Value(), 
					 DBObj->getDBError());

	if (DBObj->execCommand(sql_stmt.Value()) == QUILL_FAILURE) {
		dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		dprintf(D_ALWAYS, "sql = %s\n", sql_stmt.Value());
		goto ERROREXIT;
	}	

		/* copy the body in the sql log to the LogBody field of the entry */
	src = safe_open_wrapper (currentSqlLog.Value(), O_RDONLY);

#if 0
/* This piece of code is commented out because when there is an error, we
	create a new row in the sql log and place the ENTIRE log file into
	the newly created row. Under conditions where the condor_quilld is
	down for a long time, the log file may have grown to hundreds of
	megabytes.  Additionally, if a stable error rate is being produced
	by quill, like say thousands of jobs in schedd_sql.log have a
	type constraint error for a jobad attribute and those jobs aren't
	leaving the queue anytime soon, you'll get thousands of copies
	of the log file inserted into the database when condor_quilld
	is restarted. This will easily fill most reasonable disk volumes.
	This scenario actually happened at a customer's site and it took
	*days* to figure it out and recover from it.

	A better fix isn't being produced since, A) quill is going dark,
	B) we know of noone using this functionality, and C) this is a
	stable series and a small patch is needing to be produced.

	-psilord 09/29/2010
*/
	rv = read(src, buffer, 2048);

	while(rv > 0) {
			// end the string properly
		buffer[rv] = '\0';
		newvalue = condor_ttdb_fillEscapeCharacters(buffer, dt);

		sql_stmt.formatstr("UPDATE Error_Sqllogs SET LogBody = LogBody || '%s' WHERE LogName = '%s' and Host = '%s' and LastModified = %s", 
						 newvalue.Value(), 
						 currentSqlLog.Value(), 
						 hostname_val.Value(), 
						 ts_expr.Value());

		if (DBObj->execCommand(sql_stmt.Value()) == QUILL_FAILURE) {
			dprintf(D_ALWAYS, "Executing Statement --- Error\n");
			dprintf(D_ALWAYS, "sql = %s\n", sql_stmt.Value());			
			goto ERROREXIT;
		}
		
		rv = read(src, buffer, 2048);
	}
#endif 

	if ((DBObj->commitTransaction()) == QUILL_FAILURE) {
		goto ERROREXIT;
	}

	if(filesqlobj->file_truncate() == QUILL_FAILURE) {
		goto ERROREXIT;
	}
	
	if(filesqlobj->file_unlock() == QUILL_FAILURE) {
		goto ERROREXIT;
	}

 ERROREXIT:
	if(filesqlobj) {
		delete filesqlobj;
	}

	if (src != -1) {
		close (src);
	}

}


static int file_checksum(char *filePathName, int fileSize, char *sum) {
	Condor_MD_MAC *checker = new Condor_MD_MAC();
	unsigned char *checksum;


	if (!filePathName || !sum) {
		delete checker;
		return FALSE;
	}

#if 0
	int fd;
	char data[4097];
	int rv;

	fd = safe_open_wrapper(filePathName, O_RDONLY);
	if (fd < 0) {
		dprintf(D_FULLDEBUG, "schedd_file_checksum: can't open %s\n", filePathName);
		return FALSE;
	}

	rv = read(fd, data, 4096);

	while (rv > 0) {
		checker->addMD((unsigned char *) data, rv);
		rv = read(fd, data, 4096);
	}

	close(fd);
#endif
	checker->addMDFile(filePathName);
	checksum = checker->computeMD();

	if (checksum){
        memcpy(sum, checksum, MAC_SIZE);
        free(checksum);
	}
	else {
		dprintf(D_FULLDEBUG, "schedd_file_checksum: computeMD failed\n");
		delete checker;
		return FALSE;
	}

	delete checker;
	return TRUE;
}

static QuillErrCode append(char *destF, char *srcF) 
{	
	int dest = -1;
	int src = -1;
	char buffer[4097];
	int rv;
	QuillErrCode result = QUILL_SUCCESS;

	dest = safe_open_wrapper (destF, O_WRONLY|O_CREAT|O_APPEND, 0644);
	src = safe_open_wrapper (srcF, O_RDONLY);

	rv = read(src, buffer, 4096);
	if ( rv < 0 ) {
		result = QUILL_FAILURE;
	}

	while(rv > 0) {
		if (write(dest, buffer, rv) < 0) {
			result = QUILL_FAILURE;
			break;
		}
		
		rv = read(src, buffer, 4096);
	}

	if (dest != -1) close (dest);
	if (src != -1 ) close (src);

	return result;
}

// hash function for strings
static unsigned attHashFunction (const MyString &str)
{       
		unsigned hashVal = str.Hash();
        return hashVal;
}




TTManager::AttributeCache::AttributeCache()
{
	maintain_soft_state = true;

	MachineHash = new NamedAdHashType(50,MyStringHash,updateDuplicateKeys);
	ASSERT(MachineHash);
	ScheddHash = new NamedAdHashType(50,MyStringHash,updateDuplicateKeys);
	ASSERT(ScheddHash);
	MasterHash = new NamedAdHashType(50,MyStringHash,updateDuplicateKeys);
	ASSERT(MasterHash);
	NegotiatorHash = new NamedAdHashType(50,MyStringHash,updateDuplicateKeys);
	ASSERT(NegotiatorHash);

	temp_MachineHash = NULL;
	temp_ScheddHash = NULL;
	temp_MasterHash = NULL; 						
	temp_NegotiatorHash = NULL; 						

		// note: no need to free m_nullAdHash, it is a counted_ptr
	m_nullAdHash = (AttributeHashSmartPtrType) (new AttributeHash(2,attHashFunction,updateDuplicateKeys)); 
}

void TTManager::AttributeCache::clear(NamedAdHashType* & p)
{
	delete p;
	p = NULL;
}

TTManager::AttributeCache::~AttributeCache()
{
	clearAttributesTransaction();

	clear(MachineHash);
	clear(ScheddHash);
	clear(MasterHash);
	clear(NegotiatorHash);
}

TTManager::AttributeHashSmartPtrType TTManager::AttributeCache::newAttributeHash()
{
	AttributeHash* newClAd = new AttributeHash(200, attHashFunction, updateDuplicateKeys);
	ASSERT(newClAd);
	AttributeHashSmartPtrType smartPtr(newClAd);

	m_currentAdHash = smartPtr;

	return smartPtr;
}

bool TTManager::AttributeCache::setMaintainSoftState(bool new_val)
{
	bool old_val = maintain_soft_state;
	maintain_soft_state = new_val;
	return old_val;
}

void TTManager::AttributeCache::insertTransaction(TTManager::AttrHashType aType,
												  const MyString & name)
{
	NamedAdHashType **namedAdHash;

	m_currentTransactionAdHash = m_nullAdHash;
	m_currentCommittedAdHash = m_nullAdHash;

	if ( maintain_soft_state == false ) {
		return;
	}

	if ( name.IsEmpty() ) {
		return;
	}

	switch ( aType ) {
	case MACHINE_HASH:
		namedAdHash = &temp_MachineHash;
		MachineHash->lookup(name,m_currentCommittedAdHash);
		break;
	case SCHEDD_HASH:
		namedAdHash = &temp_ScheddHash;
		ScheddHash->lookup(name,m_currentCommittedAdHash);
		break;
	case MASTER_HASH:
		namedAdHash = &temp_MasterHash;
		MasterHash->lookup(name,m_currentCommittedAdHash);
		break;
	case NEGOTIATOR_HASH:
		namedAdHash = &temp_NegotiatorHash;
		NegotiatorHash->lookup(name,m_currentCommittedAdHash);
		break;
	default:
		dprintf(D_FULLDEBUG,"Unknown ClassAd type=%d  LINE=%d\n",aType,__LINE__);
		return;
	}

	if (*namedAdHash == NULL) {
		*namedAdHash = new NamedAdHashType(50,MyStringHash,updateDuplicateKeys);
		ASSERT(*namedAdHash);
	} else {
		(*namedAdHash)->lookup(name,m_currentTransactionAdHash);		
	}

	(*namedAdHash)->insert(name,m_currentAdHash);
}

bool TTManager::AttributeCache::alreadyCommitted(const MyString& aName, const MyString& aVal)
{
	MyString tmpVal;

	if ( m_currentTransactionAdHash->lookup(aName,tmpVal)==0 &&
		 tmpVal == aVal )
	{
		return true;
	}

	if ( m_currentCommittedAdHash->lookup(aName,tmpVal)==0 &&
		 tmpVal == aVal )
	{
		return true;
	}

	return false;
}

void TTManager::AttributeCache::commit(NamedAdHashType* transaction, NamedAdHashType* committed)
{
	AttributeHashSmartPtrType tmp;
	MyString aName;

	if ( transaction && committed ) {
		transaction->startIterations();
		while (transaction->iterate(aName, tmp)) {
			committed->insert(aName,tmp);
		}
	}	
}

void TTManager::AttributeCache::commitAttributesTransaction()
{
	commit(temp_MachineHash,MachineHash);
	commit(temp_ScheddHash,ScheddHash);
	commit(temp_MasterHash,MasterHash);
	commit(temp_NegotiatorHash,NegotiatorHash);
}

void TTManager::AttributeCache::clearAttributesTransaction()
{
	clear(temp_MachineHash);
	clear(temp_ScheddHash);
	clear(temp_MasterHash);
	clear(temp_NegotiatorHash);
}
