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

#include "condor_common.h"
#include "condor_io.h"
#include "pgsqldatabase.h"

//! constructor
PGSQLDatabase::PGSQLDatabase()
{
  connected = false;
  con_str = NULL;
  procAdsStrRes = procAdsNumRes = clusterAdsStrRes = clusterAdsNumRes = queryRes = historyHorRes = historyVerRes = NULL;  
}

//! constructor
PGSQLDatabase::PGSQLDatabase(const char* connect)
{
	connected = false;
	procAdsStrRes = procAdsNumRes = clusterAdsStrRes = clusterAdsNumRes = queryRes = historyHorRes = historyVerRes = NULL;  

	if (connect != NULL) {
		con_str = (char*)malloc(strlen(connect) + 1);
		strcpy(con_str, connect);
	}
	else {
		con_str = NULL;
	}
}

//! destructor
PGSQLDatabase::~PGSQLDatabase()
{
	procAdsStrRes = procAdsNumRes = clusterAdsStrRes = clusterAdsNumRes = queryRes = historyHorRes = historyVerRes = NULL;  
	if ((connected == true) && (connection != NULL)) {
		PQfinish(connection);
		connected = false;
		connection = NULL;
	}
	
	if (con_str != NULL) {
		free(con_str);
	}
	
}

//! connect to DB
QuillErrCode
PGSQLDatabase::connectDB()
{
	return connectDB(con_str);
}

//! connect to DB
/*! \param connect DB connect string
 */
QuillErrCode
PGSQLDatabase::connectDB(const char* connect)
{
	if ((connection = PQconnectdb(connect)) == NULL)
	{
		dprintf(D_ALWAYS, "Fatal error - unable to allocate connection to DB\n");
		return FAILURE;
	}
	
	if (PQstatus(connection) != CONNECTION_OK)
		{
			dprintf(D_ALWAYS, "Connection to database '%s' failed.\n", PQdb(connection));
		  	dprintf(D_ALWAYS, "%s", PQerrorMessage(connection));
			
			dprintf(D_ALWAYS, "Deallocating connection resources to database '%s'\n", PQdb(connection));
			PQfinish(connection);
			connection = NULL;
			return FAILURE;
        }

	connected = true;
	
	return SUCCESS;
}

//! get a DBMS error message
char*
PGSQLDatabase::getDBError()
{
	return PQerrorMessage(connection);
}

//! get the server version number, 
//! -1 if connection is invalid
int 
PGSQLDatabase::getDatabaseVersion() 
{
	int pg_version_number = 0;   
	pg_version_number = PQserverVersion(connection);
	if(pg_version_number > 0) {
		return pg_version_number;
	}
	else {
		return -1;
	}
}

//@ disconnect from DBMS
QuillErrCode
PGSQLDatabase::disconnectDB() 
{
	if ((connected == true) && (connection != NULL))
	{
		PQfinish(connection);
		connection = NULL;
	}

	connected = false;
	return SUCCESS;
}

//! begin Transaction
QuillErrCode 
PGSQLDatabase::beginTransaction() 
{
	PGresult	*result;

	if (PQstatus(connection) == CONNECTION_OK)
	{
		result = PQexec(connection, "BEGIN");

		if(result) {
			PQclear(result);		
			result = NULL;
		}

		dprintf(D_FULLDEBUG, "SQL COMMAND: BEGIN TRANSACTION\n");
		return SUCCESS;
	}
	else {
		dprintf(D_ALWAYS, "ERROR STARTING NEW TRANSACTION\n");
		return FAILURE;
	}
}

//! commit Transaction
QuillErrCode 
PGSQLDatabase::commitTransaction()
{
	PGresult	*result;

	if (PQstatus(connection) == CONNECTION_OK)
	{
		result = PQexec(connection, "COMMIT");

		if(result) {
			PQclear(result);		
			result = NULL;
		}
		dprintf(D_FULLDEBUG, "SQL COMMAND: COMMIT TRANSACTION\n");
		return SUCCESS;
	}
	else {
		dprintf(D_ALWAYS, "ERROR COMMITTING TRANSACTION\n");
		return FAILURE;
	}
}

//! abort Transaction
QuillErrCode
PGSQLDatabase::rollbackTransaction()
{
	PGresult	*result;

	if (PQstatus(connection) == CONNECTION_OK)
	{
		result = PQexec(connection, "ROLLBACK");

		if(result) {
			PQclear(result);		
			result = NULL;
		}

		dprintf(D_FULLDEBUG, "SQL COMMAND: ROLLBACK TRANSACTION\n");
		return SUCCESS;
	}
	else {
		return FAILURE;
	}
}

/*! execute a command
 *
 *  execaute SQL which doesn't have any retrieved result, such as
 *  insert, delete, and udpate.
 *
 */
QuillErrCode 
PGSQLDatabase::execCommand(const char* sql, 
						   int &num_result,
						   int &db_err_code)
{
	PGresult 	*result;
	char*		num_result_str = NULL;

	dprintf(D_FULLDEBUG, "SQL COMMAND: %s\n", sql);
	if ((result = PQexec(connection, sql)) == NULL)
	{
		dprintf(D_ALWAYS, 
			"[SQL EXECUTION ERROR1] %s\n", PQerrorMessage(connection));
		dprintf(D_ALWAYS, 
			"[SQL: %s]\n", sql);
		return FAILURE;
	}
	else if ((PQresultStatus(result) != PGRES_COMMAND_OK) &&
			(PQresultStatus(result) != PGRES_COPY_IN)) {
		dprintf(D_ALWAYS, 
			"[SQL EXECUTION ERROR2] %s\n", PQerrorMessage(connection));
		dprintf(D_ALWAYS, 
			"[SQL: %s]\n", sql);
		db_err_code =  atoi(PQresultErrorField(result, PG_DIAG_SQLSTATE));
		dprintf(D_ALWAYS, 
			"[SQLERRORCODE: %d]\n", db_err_code);
		PQclear(result);
		return FAILURE;
	}
	else {
		num_result_str = PQcmdTuples(result);
		if (num_result_str != NULL) {
			num_result = atoi(num_result_str);
		}
	}
	
	if(result) {
		PQclear(result);		
		result = NULL;
	}

	return SUCCESS;
}

QuillErrCode 
PGSQLDatabase::execCommand(const char* sql) 
{
	int num_result = 0, db_err_code = 0;
	return execCommand(sql, num_result, db_err_code);
}


/*! execute a SQL query
 *
 *	NOTE:
 *		queryRes shouldn't be PQcleared
 *		when the query is correctly executed.
 *		It is PQcleared in case of error.
 */
QuillErrCode
PGSQLDatabase::execQuery(const char* sql, 
						 PGresult*& result, 
						 int &num_result)
{
	dprintf(D_FULLDEBUG, "SQL Query = %s\n", sql);
	if ((result = PQexec(connection, sql)) == NULL)
	{
		dprintf(D_ALWAYS, 
			"[SQL EXECUTION ERROR] %s\n", PQerrorMessage(connection));
		dprintf(D_ALWAYS, 
			"[ERRONEOUS SQL: %s]\n", sql);
		return FAILURE;
	}
	else if (PQresultStatus(result) != PGRES_TUPLES_OK) {
		dprintf(D_ALWAYS, 
			"[SQL EXECUTION ERROR] %s\n", PQerrorMessage(connection));
		dprintf(D_ALWAYS, 
			"[ERRONEOUS SQL: %s]\n", sql);
		if(result) {
			PQclear(result);		
			result = NULL;
		}

		return FAILURE;
	}

	num_result = PQntuples(result);

	return SUCCESS;
}

QuillErrCode
PGSQLDatabase::execQuery(const char* sql, 
						 PGresult*& result) {
	int num_result = 0;
	return execQuery(sql, queryRes, num_result);
}

//! execute a SQL query
QuillErrCode
PGSQLDatabase::execQuery(const char* sql, int &num_result) 
{
	return execQuery(sql, queryRes, num_result);
}

//! execute a SQL query
QuillErrCode
PGSQLDatabase::execQuery(const char* sql) 
{
	int num_result = 0;
	return execQuery(sql, queryRes, num_result);
}

//! get the field name at given column index
const char *
PGSQLDatabase::getHistoryHorFieldName(int col) 
{
  return PQfname(historyHorRes, col);
}

//! get number of fields returned in result
const int 
PGSQLDatabase::getHistoryHorNumFields() 
{
  return PQnfields(historyHorRes);
}

//! get a result for the executed query
const char*
PGSQLDatabase::getValue(int row, int col)
{
	return PQgetvalue(queryRes, row, col);
}

//! release the generic query result object
QuillErrCode
PGSQLDatabase::releaseQueryResult()
{
	if(queryRes != NULL) {
	   PQclear(queryRes);
	}
	
	queryRes = NULL;

	return SUCCESS;
}

//! release all history results
QuillErrCode
PGSQLDatabase::releaseHistoryResults()
{
	if(historyHorRes != NULL) {
	   PQclear(historyHorRes);
	}
	historyHorRes = NULL;

	if(historyVerRes != NULL) {
	   PQclear(historyVerRes);
	}
	historyVerRes = NULL;

	return SUCCESS;
}


/*! get the job queue
 *
 *	\return 
 *		JOB_QUEUE_EMPTY: There is no job in the queue
 *      FAILURE_QUERY_* : error querying table *
 *		SUCCESS: There is some job in the queue and query was successful
 *
 *		
 */
QuillErrCode
PGSQLDatabase::getJobQueueDB(int *clusterarray, int numclusters, int *procarray, int numprocs,
			     char *owner, bool isfullscan,
			     int& procAdsStrRes_num, int& procAdsNumRes_num, 
			     int& clusterAdsStrRes_num, int& clusterAdsNumRes_num)
{
  
  char *procAds_str_query, *procAds_num_query, *clusterAds_str_query, *clusterAds_num_query;
  char *clusterpredicate, *procpredicate, *temppredicate;
  QuillErrCode st;
  int i;

  procAds_str_query = (char *) malloc(MAX_FIXED_SQL_STR_LENGTH * sizeof(char));
  procAds_num_query = (char *) malloc(MAX_FIXED_SQL_STR_LENGTH * sizeof(char));
  clusterAds_str_query = (char *) malloc(MAX_FIXED_SQL_STR_LENGTH * sizeof(char));
  clusterAds_num_query = (char *) malloc(MAX_FIXED_SQL_STR_LENGTH * sizeof(char));

  if(isfullscan) {
    strcpy(procAds_str_query, "SELECT cid, pid, attr, val FROM ProcAds_Str ORDER BY cid, pid;");
    strcpy(procAds_num_query, "SELECT cid, pid, attr, val FROM ProcAds_Num ORDER BY cid, pid;");
    strcpy(clusterAds_str_query, "SELECT cid, attr, val FROM ClusterAds_Str ORDER BY cid;");
    strcpy(clusterAds_num_query, "SELECT cid, attr, val FROM ClusterAds_Num ORDER BY cid;");	   
  }

  else {
    clusterpredicate = (char *) malloc(1024 * sizeof(char));
    strcpy(clusterpredicate, "  ");
    procpredicate = (char *) malloc(1024 * sizeof(char));
    strcpy(procpredicate, "  ");
    temppredicate = (char *) malloc(1024 * sizeof(char));
    strcpy(temppredicate, "  ");

    if(numclusters > 0) {
      sprintf(clusterpredicate, "%s%d)", " WHERE (cid = ", clusterarray[0]);
      for(i=1; i < numclusters; i++) {
	 sprintf(temppredicate, "%s%d) ", " OR (cid = ", clusterarray[i]);
	 strcat(clusterpredicate, temppredicate); 	 
      }

	 if(procarray[0] != -1) {
            sprintf(procpredicate, "%s%d%s%d)", " WHERE (cid = ", clusterarray[0], " AND pid = ", procarray[0]);
	 }
	 else {
            sprintf(procpredicate, "%s%d)", " WHERE (cid = ", clusterarray[0]);
	 }
	 
	 // note that we really want to iterate till numclusters and not numprocs 
	 // because procarray has holes and clusterarray does not
         for(i=1; i < numclusters; i++) {
	    if(procarray[i] != -1) {
	       sprintf(temppredicate, "%s%d%s%d) ", " OR (cid = ", clusterarray[i], " AND pid = ", procarray[i]);
	       procpredicate = strcat(procpredicate, temppredicate); 	 
            }
	    else {
	       sprintf(temppredicate, "%s%d) ", " OR (cid = ", clusterarray[i]);
	       procpredicate = strcat(procpredicate, temppredicate); 	 
            }
	 }
    }

    sprintf(procAds_str_query, "%s %s %s", 
	    "SELECT cid, pid, attr, val FROM ProcAds_Str", 
	    procpredicate,
	    "ORDER BY cid, pid;");
    sprintf(procAds_num_query, "%s %s %s", 
	    "SELECT cid, pid, attr, val FROM ProcAds_Num",
	    procpredicate,
	    "ORDER BY cid, pid;");
    sprintf(clusterAds_str_query, "%s %s %s", 
	    "SELECT cid, attr, val FROM ClusterAds_Str",
	    clusterpredicate,
	    "ORDER BY cid;");
    sprintf(clusterAds_num_query, "%s %s %s",
	    "SELECT cid, attr, val FROM ClusterAds_Num",
	    clusterpredicate,
	    "ORDER BY cid;");	   

    free(clusterpredicate);
    free(procpredicate);
    free(temppredicate);
  }

  /*dprintf(D_ALWAYS, "clusterAds_str_query = %s\n", clusterAds_str_query);
  dprintf(D_ALWAYS, "clusterAds_num_query = %s\n", clusterAds_num_query);
  dprintf(D_ALWAYS, "procAds_str_query = %s\n", procAds_str_query);
  dprintf(D_ALWAYS, "procAds_num_query = %s\n", procAds_num_query);*/

	  // Query against ProcAds_Str Table
  if ((st = execQuery(procAds_str_query, procAdsStrRes, procAdsStrRes_num)) == FAILURE) {
	  return FAILURE_QUERY_PROCADS_STR;
  }
  
	  // Query against ProcAds_Num Table
  if ((st = execQuery(procAds_num_query, procAdsNumRes, procAdsNumRes_num)) == FAILURE) {
	  return FAILURE_QUERY_PROCADS_NUM;
  }
  
	  // Query against ClusterAds_Str Table
  if ((st = execQuery(clusterAds_str_query, clusterAdsStrRes, clusterAdsStrRes_num)) == FAILURE) {
	  return FAILURE_QUERY_CLUSTERADS_STR;
  } 
	  // Query against ClusterAds_Num Table
  if ((st = execQuery(clusterAds_num_query, clusterAdsNumRes, clusterAdsNumRes_num)) == FAILURE) {
	  return FAILURE_QUERY_CLUSTERADS_NUM;
  }
  
  free(procAds_str_query);
  free(procAds_num_query);
  free(clusterAds_str_query);
  free(clusterAds_num_query);

  if (clusterAdsNumRes_num == 0 && clusterAdsStrRes_num == 0) {
    return JOB_QUEUE_EMPTY;
  }

  return SUCCESS;
}

/*! get the historical information
 *
 *	\return
 *		HISTORY_EMPTY: There is no Job in history
 *		SUCCESS: history is not empty and query succeeded
 *		FAILURE_QUERY_*: query failed
 */
QuillErrCode
PGSQLDatabase::queryHistoryDB(SQLQuery *queryhor, 
			      SQLQuery *queryver, 
			      bool longformat, 
			      int& historyads_hor_num, 
			      int& historyads_ver_num)
{  
	QuillErrCode st;
	if ((st = execQuery(queryhor->getQuery(), historyHorRes, historyads_hor_num)) == FAILURE) {
		return FAILURE_QUERY_HISTORYADS_HOR;
	}
	if (longformat && (st = execQuery(queryver->getQuery(), historyVerRes, historyads_ver_num)) == FAILURE) {
		return FAILURE_QUERY_HISTORYADS_VER;
	}
  
	if (historyads_hor_num == 0) {
		return HISTORY_EMPTY;
	}
	
	return SUCCESS;
}

//! get a value retrieved from History_Horizontal table
const char*
PGSQLDatabase::getHistoryHorValue(int row, int col)
{
	return PQgetvalue(historyHorRes, row, col);
}

//! get a value retrieved from History_Vertical table
const char*
PGSQLDatabase::getHistoryVerValue(int row, int col)
{
	return PQgetvalue(historyVerRes, row, col);
}

//! get a value retrieved from ProcAds_Str table
const char*
PGSQLDatabase::getJobQueueProcAds_StrValue(int row, int col)
{
	return PQgetvalue(procAdsStrRes, row, col);
}

//! get a value retrieved from ProcAds_Num table
const char*
PGSQLDatabase::getJobQueueProcAds_NumValue(int row, int col)
{
	return PQgetvalue(procAdsNumRes, row, col);
}

//! get a value retrieved from ClusterAds_Str table
const char*
PGSQLDatabase::getJobQueueClusterAds_StrValue(int row, int col)
{
	return PQgetvalue(clusterAdsStrRes, row, col);
}

//! get a value retrieved from ClusterAds_Num table
const char*
PGSQLDatabase::getJobQueueClusterAds_NumValue(int row, int col)
{
	return PQgetvalue(clusterAdsNumRes, row, col);
}

//! release the result for job queue database
QuillErrCode
PGSQLDatabase::releaseJobQueueResults()
{
	if (procAdsStrRes != NULL) {
		PQclear(procAdsStrRes);
		procAdsStrRes = NULL;
	}
	if (procAdsNumRes != NULL) {
		PQclear(procAdsNumRes);
		procAdsNumRes = NULL;
	}
	if (clusterAdsStrRes != NULL) {
		PQclear(clusterAdsStrRes);
		clusterAdsStrRes = NULL;
	}
	if (clusterAdsNumRes != NULL) {
		PQclear(clusterAdsNumRes);
		clusterAdsNumRes = NULL;
	}

	return SUCCESS;
}	


//! put a bulk data into DBMS
QuillErrCode
PGSQLDatabase::sendBulkData(char* data)
{
  dprintf(D_FULLDEBUG, "bulk copy data = %s\n\n", data);
  
  if (PQputCopyData(connection, data, strlen(data)) <= 0)
    {
      dprintf(D_ALWAYS, 
	      "[Bulk Data Sending ERROR] %s\n", PQerrorMessage(connection));
      dprintf(D_ALWAYS, 
	      "[Data: %s]\n", data);
      return FAILURE;
    }
  
  return SUCCESS;
}

//! put an end flag for bulk loading
QuillErrCode
PGSQLDatabase::sendBulkDataEnd()
{
	PGresult* result;

	if (PQputCopyEnd(connection, NULL) < 0)
	{
		dprintf(D_ALWAYS, 
			"[Bulk Data End Sending ERROR] %s\n", PQerrorMessage(connection));
		return FAILURE;
	}

	
	if ((result = PQgetResult(connection)) != NULL) {
		if (PQresultStatus(result) != PGRES_COMMAND_OK) {
			dprintf(D_ALWAYS, 
				"[Bulk Last Data Sending ERROR] %s\n", PQerrorMessage(connection));
			PQclear(result);
			return FAILURE;
		}
	}

	if(result) {
		PQclear(result);		
		result = NULL;
	}

	return SUCCESS;
}

