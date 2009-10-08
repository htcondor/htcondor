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


#ifndef _PGSQLDATABASE_H_
#define _PGSQLDATABASE_H_

#include "condor_common.h"
#include "libpq-fe.h"
#include "sqlquery.h"
#include "jobqueuedatabase.h"
#include "quill_enums.h"
#include "MyString.h"

#ifndef MAX_FIXED_SQL_STR_LENGTH
#define MAX_FIXED_SQL_STR_LENGTH 2048
#endif

//! PGSQLDataabse: Job Queue Database for PostgreSQL
//
class PGSQLDatabase : public JobQueueDatabase
{
public:
	
	PGSQLDatabase(const char* connect);
	~PGSQLDatabase();

		// connection handling routines
	QuillErrCode         connectDB();
	QuillErrCode		 disconnectDB();
    QuillErrCode         checkConnection();
	QuillErrCode         resetConnection();

		// transaction handling routines
	QuillErrCode		 beginTransaction();
	QuillErrCode 		 commitTransaction();
	QuillErrCode 		 rollbackTransaction();

		// query and command handling routines
	QuillErrCode 	 	 execCommand(const char* sql, 
									 int &num_result);
	QuillErrCode 	 	 execCommand(const char* sql);
	QuillErrCode		 execCommandWithBind(const char* sql, 
											 int bnd_cnt,
											 const char** val_arr,
											 QuillAttrDataType *typ_arr);
		// query methods
	QuillErrCode 	 	 execQuery(const char* sql);
	QuillErrCode 	 	 execQuery(const char* sql, PGresult *&result, 
								   int &num_result);
	QuillErrCode         execQuery(const char* sql,
								   int &num_result);
	QuillErrCode		 execQueryWithBind(const char* sql,
										   int bnd_cnt,
										   const char **val_arr,
										   QuillAttrDataType *typ_arr,
										   int &num_result);

		// result set accessors 
	const char*	         getValue(int row, int col);

		// the history based accessors automatically fetch next n results 
		// using the cursor when an out-of-range tuple is accessed
	QuillErrCode         getHistoryHorValue(SQLQuery *queryhor, 
											int row, 
											int col, 
											const char **value);
	QuillErrCode         getHistoryVerValue(SQLQuery *queryver, 
											int row, 
											int col, 
											const char **value);

		// history schema metadata routines
	int            getHistoryHorNumFields();
	const char*          getHistoryHorFieldName(int col);


		// cursor declaration and reclamation routines
	QuillErrCode		 openCursorsHistory(SQLQuery *, 
											SQLQuery *, 
											bool);
	
	QuillErrCode		 closeCursorsHistory(SQLQuery *, 
											 SQLQuery *, 
											 bool);
	
		// result set dallocation routines
	QuillErrCode         releaseQueryResult();
	QuillErrCode         releaseJobQueueResults();
	QuillErrCode		 releaseHistoryResults();		

		//
		// Job Queue DB processing methods
		//
	//! get the queue from the database
	QuillErrCode        getJobQueueDB(int *, int, int *, int,  bool,
									  const char *, int&, int&, int&, 
									  int&);
	
		//! get a value retrieved from ProcAds_Hor table
	const char*         getJobQueueProcAds_HorValue(int row, 
													int col);

	//! get a value retrieved from ProcAds_Ver table
	const char*         getJobQueueProcAds_VerValue(int row, 
													int col);

		//! get a value retrieved from ClusterAds_Hor table
	const char*         getJobQueueClusterAds_HorValue(int row, 
													   int col);
	//! get a value retrieved from ClusterAds_Ver table
	const char*         getJobQueueClusterAds_VerValue(int row, 
													   int col);

	const char*         getJobQueueClusterHorFieldName(int col);

	int           getJobQueueClusterHorNumFields();

	const char*         getJobQueueProcHorFieldName(int col);

	int           getJobQueueProcHorNumFields();


		// get the error string 
	const char*			getDBError();

		// for printing useful warning messages 
	int                  getDatabaseVersion();

		// BulkData

	QuillErrCode		sendBulkData(const char *);
	QuillErrCode		sendBulkDataEnd();

private:
	PGconn		         *connection;	//!< connection object
	PGresult	         *queryRes; 	//!< result for general query
	char				 *con_str;		//!< connection string
	
	MyString			bufferedResult;

		// history relevant members
	PGresult             *historyHorRes;
	PGresult             *historyVerRes;

	int                  historyHorFirstRowIndex;
	int                  historyVerFirstRowIndex;
	int                  historyHorNumRowsFetched;
	int                  historyVerNumRowsFetched;

		// only for job queue tables retrieval
	PGresult	         *procAdsHorRes;	//!< result for ProcAds_Str table
	PGresult	         *procAdsVerRes;	//!< result for ProcAds_Num table
	PGresult	         *clusterAdsHorRes;//!< result for ClusterAds_Str table
	PGresult	         *clusterAdsVerRes;//!< result for ClusterAds_num table
};

#endif /* _PGSQLDATABASE_H_ */
