/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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
#ifndef _JOBQUEUEDATABASE_H_
#define _JOBQUEUEDATABASE_H_

#include "condor_common.h"
#include "sqlquery.h"
#include "quill_enums.h"
#include "libpq-fe.h"

//! JobQueueDatabase
/*! It provides interfaces to talk to DBMS
 */
class JobQueueDatabase
{
public:
	//! destructor
	virtual ~JobQueueDatabase() {};

	//! connect to DBMS
	virtual QuillErrCode		connectDB() = 0;
	//! connect to DBMS
	virtual QuillErrCode		connectDB(const char* connect) = 0;
	//! disconnect from DBMS
	virtual QuillErrCode		disconnectDB() = 0;

	//! begin Transaction
	virtual QuillErrCode		beginTransaction() = 0;
	//! commit Transaction
	virtual QuillErrCode		commitTransaction() = 0;
	//! abort Transaction
	virtual QuillErrCode		rollbackTransaction() = 0;

	//! execute a command
	/*! execute SQL which doesn't have any retrieved result, such as
	 *  insert, delete, and udpate.
	 */
	virtual QuillErrCode		execCommand(const char* sql) = 0;
	//! execute a SQL query
	virtual QuillErrCode		execQuery(const char* sql) = 0;


	virtual QuillErrCode		execCommand(const char* sql, 
											int &num_result,
											int &db_err_code) = 0;
	//! execute a SQL query
	virtual QuillErrCode		execQuery(const char* sql,
										  int &num_result) = 0;

	//! get a result for the executed SQL
	virtual const char*	        getValue(int row, int col) = 0;
	virtual const char*         getHistoryHorFieldName(int col) = 0;
	virtual const int           getHistoryHorNumFields() = 0;

	//! release query result
	virtual QuillErrCode		releaseHistoryResults() = 0;
	virtual QuillErrCode        releaseJobQueueResults() = 0;
	virtual QuillErrCode        releaseQueryResult() = 0;

	//! get a DBMS error message
	virtual char*	getDBError() = 0;

	//! put bulk data into DBMS
	virtual QuillErrCode		sendBulkData(char* data) = 0;
	//! put an end flag for bulk loading
	virtual QuillErrCode		sendBulkDataEnd() = 0;

		//
		// Job Queue DB processing methods
		//
	//! get the queue from the database
	virtual QuillErrCode        getJobQueueDB(int, int, char *, bool,
											  int&, int&, int&, int&) = 0;
	//! get a value retrieved from ProcAds_Str table
	virtual const char*         getJobQueueProcAds_StrValue(int row, 
															int col) = 0;
	//! get a value retrieved from ProcAds_Num table
	virtual const char*         getJobQueueProcAds_NumValue(int row, 
															int col) = 0;
	//! get a value retrieved from ClusterAds_Str table
	virtual const char*         getJobQueueClusterAds_StrValue(int row, 
															   int col) = 0;
	//! get a value retrieved from ClusterAds_Num table
	virtual const char*         getJobQueueClusterAds_NumValue(int row, 
															   int col) = 0;

	//! get the history from the database
	virtual QuillErrCode        queryHistoryDB(SQLQuery *,SQLQuery *,bool,int&,int&) = 0;

	virtual const char*         getHistoryHorValue(int row, int col) = 0;
	virtual const char*         getHistoryVerValue(int row, int col) = 0;

protected:
	char	*con_str;	//!< connection string
	bool	connected; 	//!< connection status
};

#endif
