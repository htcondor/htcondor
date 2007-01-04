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

		// connection handling routines
	virtual QuillErrCode		connectDB() = 0;
	virtual QuillErrCode		connectDB(const char* connect) = 0;
	virtual QuillErrCode		disconnectDB() = 0;

		// transaction handling routines
	virtual QuillErrCode		beginTransaction() = 0;
	virtual QuillErrCode		commitTransaction() = 0;
	virtual QuillErrCode		rollbackTransaction() = 0;


		// query and command handling routines
	virtual QuillErrCode		execCommand(const char* sql) = 0;
	virtual QuillErrCode		execCommand(const char* sql, 
											int &num_result,
											int &db_err_code) = 0;
	virtual QuillErrCode		execQuery(const char* sql) = 0;
	virtual QuillErrCode		execQuery(const char* sql,
										  int &num_result) = 0;

		// copy command handling routines
	virtual QuillErrCode		sendBulkData(char* data) = 0;
	virtual QuillErrCode		sendBulkDataEnd() = 0;

		// result set accessors - all except the history table accessors are 
		// cursor based. the history based accessors automatically fetch 
		// next n results using the cursor
	virtual const char*	        getValue(int row, int col) = 0;
	virtual const char*         getJobQueueProcAds_StrValue(int row, 
															int col) = 0;
	virtual const char*         getJobQueueProcAds_NumValue(int row, 
															int col) = 0;
	virtual const char*         getJobQueueClusterAds_StrValue(int row, 
															   int col) = 0;
	virtual const char*         getJobQueueClusterAds_NumValue(int row, 
															   int col) = 0;
	virtual QuillErrCode        getHistoryHorValue(SQLQuery *queryhor, 
												   int row, 
												   int col, 
												   char **value) = 0;
	virtual QuillErrCode        getHistoryVerValue(SQLQuery *queryver, 
												   int row, 
												   int col, 
												   char **value) = 0;

		// history schema metadata routines
	virtual const char*         getHistoryHorFieldName(int col) = 0;
	virtual const int           getHistoryHorNumFields() = 0;

		// cursor declaration and reclamation routines
	virtual QuillErrCode        openCursorsHistory(SQLQuery *,
												   SQLQuery *,
												   bool) = 0;
	virtual QuillErrCode        closeCursorsHistory(SQLQuery *,
													SQLQuery *,
													bool) = 0;
		// result set deallocation routines
	virtual QuillErrCode        releaseQueryResult() = 0;
	virtual QuillErrCode        releaseJobQueueResults() = 0;
	virtual QuillErrCode		releaseHistoryResults() = 0;

		// get the error string
	virtual char*	getDBError() = 0;

		// high level job queue fetching code - the corresponding 
		// functionality for history is now done inside getHistoryHorValue 
		// and getHistoryVerValue
	virtual QuillErrCode        getJobQueueDB(int *, int, int *, int, 
											  char *, bool,
											  int&, int&, int&, int&) = 0;
		// for printing useful warning messages 
	virtual int 		getDatabaseVersion() = 0;
protected:
	char	*con_str;	//!< connection string
	bool	connected; 	//!< connection status
};

#endif
