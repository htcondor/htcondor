/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#ifndef _PGSQLDATABASE_H_
#define _PGSQLDATABASE_H_

#include "condor_common.h"
#include "libpq-fe.h"
#include "sqlquery.h"
#include "jobqueuedatabase.h"
#include "quill_enums.h"

#ifndef MAX_FIXED_SQL_STR_LENGTH
#define MAX_FIXED_SQL_STR_LENGTH 2048
#endif

//! PGSQLDataabse: JobQueueDatabase for PostgreSQL
//
class PGSQLDatabase : public JobQueueDatabase
{
public:
	
	PGSQLDatabase();
	PGSQLDatabase(const char* connect);
	~PGSQLDatabase();

		// connection handling routines
	QuillErrCode         connectDB();
	QuillErrCode		 connectDB(const char* connect);
	QuillErrCode		 disconnectDB();

		// transaction handling routines
	QuillErrCode		 beginTransaction();
	QuillErrCode 		 commitTransaction();
	QuillErrCode 		 rollbackTransaction();

		// query and command handling routines
	QuillErrCode 	 	 execCommand(const char* sql, 
									 int &num_result,
									 int &db_err_code);
	QuillErrCode 	 	 execCommand(const char* sql);

	QuillErrCode 	 	 execQuery(const char* sql);
	QuillErrCode 	 	 execQuery(const char* sql, 
								   PGresult*& result);
	QuillErrCode 	 	 execQuery(const char* sql,
								   int &num_result);
	QuillErrCode 	 	 execQuery(const char* sql, 
								   PGresult*& result,
								   int &num_result);

		// copy command handling routines
	QuillErrCode		 sendBulkData(char* data);
	QuillErrCode		 sendBulkDataEnd();

		// result set accessors 
	const char*	         getValue(int row, int col);
	const char*	         getJobQueueProcAds_StrValue(int row, 
													 int col);
	const char*	         getJobQueueProcAds_NumValue(int row, 
													 int col);
	const char*	         getJobQueueClusterAds_StrValue(int row, 
														int col);
	const char*	         getJobQueueClusterAds_NumValue(int row, 
														int col);

		// the history based accessors automatically fetch next n results 
		// using the cursor when an out-of-range tuple is accessed
	QuillErrCode         getHistoryHorValue(SQLQuery *queryhor, 
											int row, 
											int col, 
											char **value);
	QuillErrCode         getHistoryVerValue(SQLQuery *queryver, 
											int row, 
											int col, 
											char **value);

		// history schema metadata routines
	const int            getHistoryHorNumFields();
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

		// high level job queue fetching code - the corresponding 
		// functionality for history is now done inside getHistoryHorValue 
		// and getHistoryVerValue
	QuillErrCode		 getJobQueueDB(int *, int, int *, int, char *, bool, 
									   int&, int&, int&, int&);


		// get the error string 
	char*		         getDBError();

		// for printing useful warning messages 
	int                  getDatabaseVersion();

private:
	PGconn		         *connection;		//!< connection object
	PGresult	         *queryRes; 	    //!< result for general query

		// history relevant members
	PGresult             *historyHorRes;
	PGresult             *historyVerRes;
	int                  historyHorFirstRowIndex;
	int                  historyVerFirstRowIndex;
	int                  historyHorNumRowsFetched;
	int                  historyVerNumRowsFetched;
		// job queue relevant members 
	PGresult	         *procAdsStrRes;	//!< result for ProcAds_Str table
	PGresult	         *procAdsNumRes;	//!< result for ProcAds_Num table
	PGresult	         *clusterAdsStrRes;//!< result for ClusterAds_Str table
	PGresult	         *clusterAdsNumRes;//!< result for ClusterAds_num table
};

#endif /* _PGSQLDATABASE_H_ */
