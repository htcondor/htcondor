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

#ifndef _JOBQUEUEDATABASE_H_
#define _JOBQUEUEDATABASE_H_

#include "condor_common.h"
#include "sqlquery.h"
#include "quill_enums.h"
#include "condor_debug.h"
#include "condor_email.h"

extern const int QUILLPP_HistoryHorFieldNum;
extern const char *QUILLPP_HistoryHorFields[];
extern const int QUILLPP_HistoryHorIsQuoted[];
extern const int proc_field_num;
extern const char *proc_field_names [];
extern const int proc_field_is_quoted [];
extern const int cluster_field_num;
extern const char *cluster_field_names [];
extern const int cluster_field_is_quoted [];
extern const bool history_hor_clob_field [];
extern const bool history_ver_clob_field [];
extern const bool proc_hor_clob_field [];
extern const bool proc_ver_clob_field [];
extern const bool cluster_hor_clob_field [];
extern const bool cluster_ver_clob_field [];

#define QUILL_ORACLE_STRINGLIT_LIMIT 4000

//! JobQueueDatabase
/*! It provides interfaces to talk to DBMS
 */
class JobQueueDatabase
{
public:
	//! destructor
	virtual ~JobQueueDatabase() {};

		// connection handling routines
	virtual QuillErrCode		connectDB() = 0;

	//! disconnect from DBMS
	virtual QuillErrCode		disconnectDB() = 0;

		// transaction handling routines
	virtual QuillErrCode		beginTransaction() = 0;
	virtual QuillErrCode		commitTransaction() = 0;
	virtual QuillErrCode		rollbackTransaction() = 0;

	//! execute a command
	/*! execute SQL which doesn't have any retrieved result, such as
	 *  insert, delete, and udpate.
	 */
	virtual QuillErrCode		execCommand(const char* sql, 
											int &num_result) = 0;
	virtual QuillErrCode		execCommand(const char* sql) = 0;
	virtual QuillErrCode		execCommandWithBind(const char* sql, 
													int bnd_cnt,
													const char** val_arr,
													QuillAttrDataType *typ_arr) = 0;

	//! execute a SQL query
	virtual QuillErrCode		execQuery(const char* sql) = 0;
	virtual QuillErrCode        execQuery(const char* sql,
										  int &num_result) = 0;
	virtual QuillErrCode		execQueryWithBind(const char* sql,
												  int bnd_cnt,
												  const char **val_arr,
												  QuillAttrDataType *typ_arr,
												  int &num_result) = 0;

	//! get a result for the executed SQL
	// virtual QuillErrCode 		fetchNext() = 0;
	virtual const char*         getValue(int row, int col) = 0;
	//virtual int			        getIntValue(int col) = 0;

	//! release query result
	virtual QuillErrCode        releaseQueryResult() = 0;

	virtual QuillErrCode        checkConnection() = 0;
	virtual QuillErrCode        resetConnection() = 0;

		//
		// Job Queue DB processing methods
		//
	//! get the queue from the database
	virtual QuillErrCode        getJobQueueDB(int *, int, int *, int,  bool,
											  const char *, int&, int&, int&, 
											  int&) = 0;
	
		//! get a value retrieved from ProcAds_Hor table
	virtual const char*         getJobQueueProcAds_HorValue(int row, 
															int col) = 0;

	//! get a value retrieved from ProcAds_Ver table
	virtual const char*         getJobQueueProcAds_VerValue(int row, 
															int col) = 0;

		//! get a value retrieved from ClusterAds_Hor table
	virtual const char*         getJobQueueClusterAds_HorValue(int row, 
															   int col) = 0;
	//! get a value retrieved from ClusterAds_Ver table
	virtual const char*         getJobQueueClusterAds_VerValue(int row, 
															   int col) = 0;
	virtual QuillErrCode        getHistoryHorValue(SQLQuery *queryhor, 
												   int row, 
												   int col, 
												   const char **value) = 0;
	virtual QuillErrCode        getHistoryVerValue(SQLQuery *queryver, 
												   int row, 
												   int col, 
												   const char **value) = 0;

		// history schema metadata routines
	virtual const char*         getHistoryHorFieldName(int col) = 0;
	virtual int           getHistoryHorNumFields() = 0;

		// cursor declaration and reclamation routines
	virtual QuillErrCode        openCursorsHistory(SQLQuery *,
												   SQLQuery *,
												   bool) = 0;
	virtual QuillErrCode        closeCursorsHistory(SQLQuery *,
													SQLQuery *,
													bool) = 0;
		// result set deallocation routines
	virtual QuillErrCode        releaseJobQueueResults() = 0;
	virtual QuillErrCode		releaseHistoryResults() = 0;
	virtual const char*         getJobQueueClusterHorFieldName(int col) = 0;

	virtual int           getJobQueueClusterHorNumFields() = 0;

	virtual const char*         getJobQueueProcHorFieldName(int col) = 0;

	virtual int           getJobQueueProcHorNumFields() = 0;


		// get the error string
	virtual const char*	getDBError() = 0;

		// high level job queue fetching code - the corresponding 
		// functionality for history is now done inside getHistoryHorValue 
		// and getHistoryVerValue


	void assertSchemaVersion() {
		int len, num_result;
		char *sql_str;
		QuillErrCode ret_st;

		if (!connected) {
			dprintf(D_ALWAYS, "Not connected to database in JobQueueDatabase::assertSchemaVersion\n");
			return;
		}

		len = 2048;
		sql_str = (char *) malloc (len * sizeof(char));

		snprintf(sql_str, len, "SELECT major, minor, back_to_major, back_to_minor FROM quill_schema_version");

		ret_st = this->execQuery(sql_str, num_result);

		if ((ret_st != QUILL_SUCCESS) || (num_result != 1)) {
			EXCEPT("schema version not found or incorrect\n");
		} else {
			int major, minor, back_to_major, back_to_minor;
			major = atoi(this->getValue(0,0));
			minor = atoi(this->getValue(0,1));
			back_to_major = atoi(this->getValue(0,2));
			back_to_minor = atoi(this->getValue(0,3));
		
			if (!(major == 2 && minor == 0 && back_to_major == 2 && back_to_minor == 0)) {
				dprintf(D_ALWAYS, "schema version is not correct. Expected major=2, minor=0, back_to_major=2, back_to_minor=2, but found major=%d, minor=%d, back_to_major=%d, back_to_minor=%d\n", major, minor, back_to_major, back_to_minor);
			
				this->releaseQueryResult();
			
				EXCEPT("schema version not correct\n");
			}
		}
		free(sql_str);
		this->releaseQueryResult();		
	}

	void emailDBError(int errorcode, const char *dbtype) {
		FILE *email;
		char msg_body[4000];
		
		snprintf(msg_body, 4000, "Database system error occurred: error code = "
				 "%d, database type = %s\n", 
				 errorcode, dbtype);

		email = email_admin_open(msg_body);

		if (email) {
			email_close ( email );
		} else {
			dprintf( D_ALWAYS, "ERROR: Can't send email to the Condor "
					 "Administrator\n" );
		}		
	}

	virtual QuillErrCode	sendBulkData(const char *){return QUILL_FAILURE;}
	virtual QuillErrCode	sendBulkDataEnd() {return QUILL_FAILURE;}

	bool isConnected() const { return connected;}
protected:
	bool	connected; 	//!< connection status
};

#endif
