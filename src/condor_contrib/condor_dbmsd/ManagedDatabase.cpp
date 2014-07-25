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
#include "ManagedDatabase.h"
#include "dbms_utils.h"
#include "condor_config.h"
#include "pgsqldatabase.h"
#include "condor_email.h"

ManagedDatabase::ManagedDatabase() {
	char *tmp;
	QuillErrCode ret_st;

	if (param_boolean("QUILL_ENABLED", false) == false) {
		EXCEPT("Quill++ is currently disabled. Please set QUILL_ENABLED to "
			   "TRUE if you want this functionality and read the manual "
			   "about this feature since it requires other attributes to be "
			   "set properly.");
	}

		//bail out if no SPOOL variable is defined since its used to 
		//figure out the location of the quillWriter password file
	char *spool = param("SPOOL");
	if(!spool) {
		EXCEPT("No SPOOL variable found in config file\n");
	}	

		/*
		  Here we try to read the database parameters in config
		  the db ip address format is <ipaddress:port> 
		*/
	dt = getConfigDBType();
	dbIpAddress = param("QUILL_DB_IP_ADDR");
	dbName = param("QUILL_DB_NAME");
	dbUser = param("QUILL_DB_USER");

	dbConnStr = getDBConnStr(dbIpAddress,
							 dbName,
							 dbUser,
							 spool);

	dprintf(D_ALWAYS, "Using Database Type = Postgres\n");
	dprintf(D_ALWAYS, "Using Database IpAddress = %s\n", 
			dbIpAddress?dbIpAddress:"");
	dprintf(D_ALWAYS, "Using Database Name = %s\n", 
			dbName?dbName:"");
	dprintf(D_ALWAYS, "Using Database User = %s\n", 
			dbUser?dbUser:"");

	if (spool) {
		free(spool);
	}

	switch (dt) {				
	case T_PGSQL:
		DBObj = new PGSQLDatabase(dbConnStr);
		break;
	default:
		break;
	}		

			/* default to a week of resource history info */
	resourceHistoryDuration = param_integer("QUILL_RESOURCE_HISTORY_DURATION", 7);

			/* default to a week of job run information */
	runHistoryDuration = param_integer("QUILL_RUN_HISTORY_DURATION", 7);

			/* default to 10 years of job history */
	jobHistoryDuration = param_integer("QUILL_JOB_HISTORY_DURATION", 3650);

			/* default to 20 GB */
	dbSizeLimit = param_integer("QUILL_DBSIZE_LIMIT", 20);	
		
		/* check if schema version is ok */
	ret_st = DBObj->connectDB();
	if (ret_st == QUILL_FAILURE) {
		dprintf(D_ALWAYS, "config: unable to connect to DB--- ERROR");
		EXCEPT("config: unable to connect to DB\n");
	}	

		/* the following will also throw an exception of the schema 
		   version is not correct */
	DBObj->assertSchemaVersion();	

	DBObj->disconnectDB();
}

ManagedDatabase::~ManagedDatabase() {
	if (dbIpAddress) {
		free(dbIpAddress);
		dbIpAddress = NULL;
	}

	if (dbName) {
		free(dbName);
		dbName = NULL;
	}

	if (dbUser) {
		free(dbUser);
		dbUser = NULL;
	}

	if (dbConnStr) {
		free(dbConnStr);
		dbConnStr = NULL;
	}

	if (DBObj) {
		delete DBObj;
	}
}

void ManagedDatabase::PurgeDatabase() {
	QuillErrCode ret_st;
	int dbsize;
	int num_result;
	MyString sql_str;

	ret_st = DBObj->connectDB();

		/* call the puging routine */
	if (ret_st == QUILL_FAILURE) {
		dprintf(D_ALWAYS, "ManagedDatabase::PurgeDatabase: unable to connect to DB--- ERROR\n");
		return;
	}	

	switch (dt) {				
	case T_PGSQL:
		sql_str.formatstr("select quill_purgeHistory(%d, %d, %d)",
						resourceHistoryDuration,
						runHistoryDuration,
						jobHistoryDuration);
		break;
	default:
			// can't have this case
		dprintf(D_ALWAYS, "database type is %d, unexpected! exiting!\n", dt);
		ASSERT(0);
		break;
	}

	ret_st = DBObj->execCommand(sql_str.Value());
	if (ret_st == QUILL_FAILURE) {
		dprintf(D_ALWAYS, "ManagedDatabase::PurgeDatabase --- ERROR [SQL] %s\n", 
				sql_str.Value());
	}

	ret_st = DBObj->commitTransaction();
	if (ret_st == QUILL_FAILURE) {
		dprintf(D_ALWAYS, "ManagedDatabase::PurgeDatabase --- ERROR [COMMIT] \n");
	}
	
		/* query the space usage and if it is above some threshold, send
		  a warning to the administrator 
		*/
	
	sql_str.formatstr("SELECT dbsize FROM quillDBMonitor");
	ret_st = DBObj->execQuery(sql_str.Value(), num_result);

	if ((ret_st == QUILL_SUCCESS) && 
		(num_result == 1)) {
		dbsize = atoi(DBObj->getValue(0, 0));		
		
			/* if dbsize is bigger than 75% the dbSizeLimit, send a 
			   warning to the administrator with information about 
			   the situation and suggestion for looking at the table 
			   sizes using the following sql statement and tune down
			   the *HistoryDuration parameters accordingly.

			   This is an oracle version of sql for examining the 
			   table sizes in descending order, sql for other 
			   databases can be similarly constructed:
			   SELECT NUM_ROWS*AVG_ROW_LEN, table_name
			   FROM USER_TABLES
			   ORDER BY  NUM_ROWS*AVG_ROW_LEN DESC;
			*/
		if (dbsize/1024 > dbSizeLimit) {
			FILE *email;
			char msg_body[4000];

			snprintf(msg_body, 4000, "Current database size (> %d MB) is "
					 "bigger than 75 percent of the limit (%d GB). Please "
					 "decrease the values of these parameters: "
					 "QUILL_RESOURCE_HISTORY_DURATION, "
					 "QUILL_RUN_HISTORY_DURATION, "
					 "QUILL_JOB_HISTORY_DURATION or QUILL_DBSIZE_LIMIT\n", 
					 dbsize, dbSizeLimit);

				/* notice that dbsize is stored in unit of MB, but
				   dbSizeLimit is stored in unit of GB */
			dprintf(D_ALWAYS, "%s", msg_body);

			email = email_admin_open(msg_body);
			
			if (email) {
				email_close ( email );
			} else {
				dprintf( D_ALWAYS, "ERROR: Can't send email to the Condor "
						 "Administrator\n" );
			}
		} 

	} else {
		dprintf(D_ALWAYS, "Reading quillDBMonitor --- ERROR or returned # of rows is not exactly one [SQL] %s\n", 
				sql_str.Value());		
	}
	
	DBObj->releaseQueryResult();

	ret_st = DBObj->disconnectDB();
	if (ret_st == QUILL_FAILURE) {
		dprintf(D_ALWAYS, "ManagedDatabase::disconnectDB: unable to disconnect --- ERROR\n");
		return;
	}		
}

void ManagedDatabase::ReindexDatabase() {
	QuillErrCode ret_st;
	MyString sql_str;

	switch (dt) {				
	case T_PGSQL:
		ret_st = DBObj->connectDB();
		
			/* call the reindex routine */
		if (ret_st == QUILL_FAILURE) {
			dprintf(D_ALWAYS, "ManagedDatabase::ReindexDatabase: unable to connect to DB--- ERROR\n");
			return;
		}	
		
		sql_str.formatstr("select quill_reindexTables()");

		ret_st = DBObj->execCommand(sql_str.Value());
		if (ret_st == QUILL_FAILURE) {
			dprintf(D_ALWAYS, "ManagedDatabase::ReindexDatabase --- ERROR [SQL] %s\n", 
					sql_str.Value());
		}


		ret_st = DBObj->disconnectDB();
		if (ret_st == QUILL_FAILURE) {
			dprintf(D_ALWAYS, "ManagedDatabase::disconnectDB: unable to disconnect --- ERROR\n");
			return;
		}		

		break;
	default:
			// can't have this case
		dprintf(D_ALWAYS, "database type is %d, unexpected! exiting!\n", dt);
		ASSERT(0);
		break;
	}

}
