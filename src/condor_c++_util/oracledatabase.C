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
#include "oracledatabase.h"
#include "MyString.h"

extern const bool history_hor_clob_field [] = {FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE};
extern const bool history_ver_clob_field [] = {FALSE, FALSE, FALSE, FALSE, TRUE};
extern const bool proc_hor_clob_field [] = {FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, TRUE, FALSE, FALSE, FALSE};
const bool proc_ver_clob_field [] = {FALSE, FALSE, FALSE, TRUE};
extern const bool cluster_hor_clob_field [] = {FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, TRUE, TRUE, FALSE};
const bool cluster_ver_clob_field [] = {FALSE, FALSE, TRUE};

static MyString getStringFromClob(ResultSet *res, int col);

//! constructor
ORACLEDatabase::ORACLEDatabase(const char* connect)
{
	/* local variables */
	char *temp;
 	char host[100]="";
	char port[10]="";
	char dbname[100]="";
	char user_name[100]="";
	char pass_word[100]="";
	int  len1, len2, len3;
	char *token;	

	connected = false;
	env = NULL;
	conn = NULL;
	in_tranx = false;

	historyHorRes = historyVerRes = procAdsHorRes = procAdsVerRes = 
		clusterAdsHorRes = clusterAdsVerRes = queryRes = NULL;

	stmt = queryStmt = historyHorStmt = historyVerStmt = 
		procAdsHorStmt = procAdsVerStmt = clusterAdsHorStmt = 
		clusterAdsVerStmt = NULL;

	if (connect != NULL) {
			/* make a copy of connect so that we can tokenize it */
		temp = strdup(connect);

			/* parse the connect string, it's assumed to be the following
			   format and there is no space within the values of any 
			   of the following parameters: 
			   host=%s port=%s user=%s password=%s dbname=%s
			*/
		token = strtok(temp, " ");
		if(token) {
			sscanf(token, "host=%s", host);
		}

		token = strtok(NULL, " ");
		if(token) {
			sscanf(token, "port=%s", port);
		}

		token = strtok(NULL, " ");
		if(token) {
			sscanf(token, "user=%s", user_name);
		}

		token = strtok(NULL, " ");
		if(token) {
			sscanf(token, "password=%s", pass_word);
		}

		token = strtok(NULL, " ");
		if(token) {
			sscanf(token, "dbname=%s", dbname);
		}

			/* now we have all parameters, put them into userName, password 
			   and connectString if available, 
			   the connectString has this format: [host[:port]][/database] 
			*/
		len1 = strlen(user_name);

		this->userName = (char*)malloc(len1 + 1);
		strcpy(this->userName, user_name);		

		len1 = strlen(pass_word);
		this->password = (char*)malloc(len1+1);
		strcpy(this->password, pass_word);

		len1 = strlen(host);
		len2 = strlen(port);
		len3 = strlen(dbname);

		this ->connectString = (char *)malloc(len1 + len2 + len3 + 5);
		this->connectString[0] = '\0';
		if (len1 > 0) {
				/* host is not empty */
			strcat(this->connectString, host);
			if (len2 > 0) {
					/* port is not empty */
				strcat (this->connectString, ":");
				strcat(this->connectString, port);
			}
		}

		if (len3 > 0) {
				/* dbname is not empty */
			strcat(this->connectString, "/");
			strcat(this->connectString, dbname);
		}
    } else {
		this->userName = (char *)malloc(1);
		this->userName[0] = '\0'; 
		this->password = (char *)malloc(1);
		this->password[0] = '\0'; 
		this->connectString = (char *)malloc(1);
		this->connectString[0] = '\0'; 
    }

#ifdef TT_COLLECT_SQL
	sqllog_fp = fopen("trace.sql", "a");
#endif 

}

//! destructor
ORACLEDatabase::~ORACLEDatabase()
{
    if (connected && conn != NULL) { 
        // free result set and statement handle if any
	if (queryStmt) {
		if (queryRes) 
			queryStmt->closeResultSet (queryRes);
		conn->terminateStatement (queryStmt);
	}
        
	if (historyHorStmt) {
		if (historyHorRes) 
			historyHorStmt->closeResultSet (historyHorRes);
		conn->terminateStatement (historyHorStmt);
	}
	
	if (historyVerStmt) {
		if (historyVerRes) 
			historyVerStmt->closeResultSet (historyVerRes);
		conn->terminateStatement (historyVerStmt);
	}        
        
	if (procAdsHorStmt) {
		if (procAdsHorRes) 
			procAdsHorStmt->closeResultSet (procAdsHorRes);
		conn->terminateStatement (procAdsHorStmt);
	}
	
	if (procAdsVerStmt) {
		if (procAdsVerRes) 
			procAdsVerStmt->closeResultSet (procAdsVerRes);
		conn->terminateStatement (procAdsVerStmt);
	}
	
	if (clusterAdsHorStmt) {
		if (clusterAdsHorRes) 
			clusterAdsHorStmt->closeResultSet (clusterAdsHorRes);
		conn->terminateStatement (clusterAdsHorStmt);
	}
	
	if (clusterAdsVerStmt) {
		if (clusterAdsVerRes) 
			clusterAdsVerStmt->closeResultSet (clusterAdsVerRes);
		conn->terminateStatement (clusterAdsVerStmt);
	}
    }	
		// free connection and environment handle 
	if (connected == true) {
		if (conn != NULL) {
			env->terminateConnection(conn);                 
		}
		
		if (env != NULL) {
			Environment::terminateEnvironment(env);
		}

		connected = false;
	}
        
                // free string memory 
	if (userName != NULL) {
		free(userName);
	}
        
	if (password != NULL) {
		free(password);
	}

	if (connectString != NULL) {
		free(connectString);
	}
}

//! connect to DB
QuillErrCode
ORACLEDatabase::connectDB()
{
	try {
		env = Environment::createEnvironment(Environment::OBJECT);
		
		if (env == NULL) {
			dprintf(D_ALWAYS, "ERROR CREATING Environment in ORACLEDatabase::connectDB, Check if ORACLE environment variables are set correctly\n");
			return QUILL_FAILURE;
		}
		
		conn = env->createConnection(userName, password, connectString);
	} catch (SQLException ex) {
		
		dprintf(D_ALWAYS, "ERROR CREATING CONNECTION\n");
		dprintf(D_ALWAYS, "Database connect string: %s, User name: %s\n",  connectString, userName);
		dprintf(D_ALWAYS, "Error number: %d, Error message: %s in ORACLEDatabase::connectDB\n", ex.getErrorCode(), ex.getMessage().c_str());
		errorMsg.sprintf("Error number: %d, Error message: %s", 
						ex.getErrorCode(), ex.getMessage().c_str());

		return QUILL_FAILURE;
	}
	connected = true;       
	return QUILL_SUCCESS;
}

//@ disconnect from DBMS
QuillErrCode
ORACLEDatabase::disconnectDB() 
{
	if ((connected == true) && (env != NULL) && (conn != NULL))
	{
		env->terminateConnection(conn);			
		Environment::terminateEnvironment(env);
		conn = NULL;
		env = NULL;
	}

	connected = false;
	return QUILL_SUCCESS;
}

//! begin Transaction
QuillErrCode 
ORACLEDatabase::beginTransaction() 
{
	if (!connected) {
		dprintf(D_ALWAYS, "Not connected to database in ORACLEDatabase::beginTransaction\n");
		return QUILL_FAILURE;
	}

	if (in_tranx) {
		dprintf(D_ALWAYS, "Can't start a tranx within a tranx in ORACLEDatabase::beginTransaction\n");
		return QUILL_FAILURE;
	}

	in_tranx = true;
	return QUILL_SUCCESS;
}

//! commit Transaction
QuillErrCode 
ORACLEDatabase::commitTransaction()
{	
	if (!connected) {
		dprintf(D_ALWAYS, "Not connected to database in ORACLEDatabase::commitTransaction\n");
		return QUILL_FAILURE;
	}

#ifdef TT_COLLECT_SQL
	fprintf(sqllog_fp, "COMMIT;\n");
	fflush(sqllog_fp);
#endif

	try {
		conn->commit();
	}	catch (SQLException ex) {

		dprintf(D_ALWAYS, "ERROR COMMITTING TRANSACTION\n");
		dprintf(D_ALWAYS, "Error number: %d, Error message: %s in ORACLEDatabase::commitTransaction\n", ex.getErrorCode(), ex.getMessage().c_str());
		errorMsg.sprintf("Error number: %d, Error message: %s", 
						ex.getErrorCode(), ex.getMessage().c_str());

			/* ORA-03113 or 03114 mean that the connection between Client and 
			   Server process was broken.

			   ORA-04031 means that shared pool is out of memory. Disconnect
			   so that we avoid getting the same error over and over. Also 
			   this will avoid the sql log being truncated.
			*/
		if (ex.getErrorCode() == 3113 ||
			ex.getErrorCode() == 3114 ||
			ex.getErrorCode() == 4031) {
			emailDBError(ex.getErrorCode(), "Oracle");
			disconnectDB();
		}

		in_tranx = false;
		return QUILL_FAILURE;
	}

	dprintf(D_FULLDEBUG, "SQL COMMAND: COMMIT TRANSACTION\n");
	in_tranx = false;
	return QUILL_SUCCESS;
}

//! abort Transaction
QuillErrCode
ORACLEDatabase::rollbackTransaction()
{
	if (!connected) {
		dprintf(D_ALWAYS, "Not connected to database in ORACLEDatabase::rollbackTransaction\n");
		return QUILL_FAILURE;
	}

#ifdef TT_COLLECT_SQL
	fprintf(sqllog_fp, "ROLLBACK;\n");
	fflush(sqllog_fp);
#endif

	try {
		conn->rollback();
	}	catch (SQLException ex) {

		dprintf(D_ALWAYS, "ERROR ROLLING BACK TRANSACTION\n");
		dprintf(D_ALWAYS, "Error number: %d, Error message: %s in ORACLEDatabase::rollbackTransaction\n", ex.getErrorCode(), ex.getMessage().c_str());
		errorMsg.sprintf("Error number: %d, Error message: %s", 
						ex.getErrorCode(), ex.getMessage().c_str());

			/* ORA-03113 means that the connection between Client and Server 
			   process was broken.
			*/
			/* ORA-04031 means that shared pool is out of memory. Disconnect
			   so that we avoid getting the same error over and over. Also 
			   this will avoid the sql log being truncated.
			 */
		if (ex.getErrorCode() == 3113 ||
			ex.getErrorCode() == 3114 ||
			ex.getErrorCode() == 4031) {
			emailDBError(ex.getErrorCode(), "Oracle");
			disconnectDB();
		}

		in_tranx = false;
		return QUILL_FAILURE;
	}

	dprintf(D_FULLDEBUG, "SQL COMMAND: ROLLBACK TRANSACTION\n");
	in_tranx = false;
	return QUILL_SUCCESS;
}

/*! execute a command
 *
 *  execaute SQL which doesn't have any retrieved result, such as
 *  insert, delete, and udpate.
 *
 */
QuillErrCode 
ORACLEDatabase::execCommand(const char* sql, 
						   int &num_result)
{
	struct timeval tvStart, tvEnd;

	if (!connected) {
		dprintf(D_ALWAYS, "Not connected to database in ORACLEDatabase::execCommand\n");
		return QUILL_FAILURE;
	}

	dprintf(D_FULLDEBUG, "SQL COMMAND: %s\n", sql);
	
#ifdef TT_COLLECT_SQL
	fprintf(sqllog_fp, "%s;\n", sql);
	fflush(sqllog_fp);
#endif

#ifdef TT_TIME_SQL
	gettimeofday( &tvStart, NULL );
#endif

	try {
		stmt = conn->createStatement (sql);
		num_result = stmt->executeUpdate ();
	} catch (SQLException ex) {
		dprintf(D_ALWAYS, "ERROR EXECUTING UPDATE\n");
		dprintf(D_ALWAYS,  "[SQL: %s]\n", sql);		
		dprintf(D_ALWAYS, "Error number: %d, Error message: %s in ORACLEDatabase::execCommand\n", ex.getErrorCode(), ex.getMessage().c_str());
		errorMsg.sprintf("Error number: %d, Error message: %s", 
						ex.getErrorCode(), ex.getMessage().c_str());

		conn->terminateStatement (stmt);
		stmt = NULL;

			/* ORA-03113 means that the connection between Client and Server 
			   process was broken.
			 */
			/* ORA-04031 means that shared pool is out of memory. Disconnect
			   so that we avoid getting the same error over and over. Also 
			   this will avoid the sql log being truncated.
			 */
		if (ex.getErrorCode() == 3113 ||
			ex.getErrorCode() == 3114 ||
			ex.getErrorCode() == 4031) {
			emailDBError(ex.getErrorCode(), "Oracle");
			disconnectDB();
		}

		return QUILL_FAILURE;				
	}

	conn->terminateStatement (stmt);	

#ifdef TT_TIME_SQL
	gettimeofday( &tvEnd, NULL );

	dprintf(D_FULLDEBUG, "Execution time: %ld\n", 
			(tvEnd.tv_sec - tvStart.tv_sec)*1000 + 
			(tvEnd.tv_usec - tvStart.tv_usec)/1000);
#endif
	
	stmt = NULL;
	return QUILL_SUCCESS;
}

QuillErrCode 
ORACLEDatabase::execCommand(const char* sql) 
{
	int num_result = 0;
	return execCommand(sql, num_result);
}

/*! execute a SQL query
 */
QuillErrCode
ORACLEDatabase::execQuery(const char* sql,
			  ResultSet *&result,
			  Statement *&stmt,
			  int &num_result)
{
	ResultSet *res;
	ResultSet::Status rs;
	int temp=0;

	if (!connected) {
		dprintf(D_ALWAYS, "Not connected to database in ORACLEDatabase::execQuery\n");
		num_result = -1;
		return QUILL_FAILURE;
	}

	dprintf(D_FULLDEBUG, "SQL Query = %s\n", sql);

	try {
		stmt = conn->createStatement (sql);
		res = stmt->executeQuery ();
		
			/* fetch from res to count the number of rows */
		while ((rs = res->next()) != ResultSet::END_OF_FETCH) {
			temp++;  	
		}

		stmt->closeResultSet (res);
		
			/* query again to get new result structure to pass back */
		result = stmt->executeQuery();
		num_result = temp;	
		
	} catch (SQLException ex) {
		conn->terminateStatement (stmt);
		result = NULL;
		stmt = NULL;
		
		dprintf(D_ALWAYS, "ERROR EXECUTING QUERY\n");
		dprintf(D_ALWAYS,  "[SQL: %s]\n", sql);         
		dprintf(D_ALWAYS, "Error number: %d, Error message: %s in ORACLEDatabase::execQuery\n", ex.getErrorCode(), ex.getMessage().c_str());
		errorMsg.sprintf("Error number: %d, Error message: %s", 
						ex.getErrorCode(), ex.getMessage().c_str());
		
			/* ORA-03113 means that the connection between Client 
			   and Server process was broken.
			*/
			/* ORA-04031 means that shared pool is out of memory. Disconnect
			   so that we avoid getting the same error over and over. Also 
			   this will avoid the sql log being truncated.
			*/
		if (ex.getErrorCode() == 3113 ||
			ex.getErrorCode() == 3114 ||
			ex.getErrorCode() == 4031) {
			emailDBError(ex.getErrorCode(), "Oracle");
			disconnectDB();
		}                   

		num_result = -1;

		return QUILL_FAILURE;                 
	}

	return QUILL_SUCCESS;
}


/*! execute a SQL query
 */
QuillErrCode
ORACLEDatabase::execQuery(const char* sql)
{
  int num_result;
  queryResCursor = -1;
  return execQuery(sql, queryRes, queryStmt, num_result);
}

/*! execute a SQL query
 */
QuillErrCode
ORACLEDatabase::execQuery(const char* sql, int &num_result)
{
  queryResCursor = -1;
  return execQuery(sql, queryRes, queryStmt, num_result);
}

/*
QuillErrCode
ORACLEDatabase::fetchNext() 
{
	ResultSet::Status rs;

	if (!rset) {
		dprintf(D_ALWAYS, "no result to fetch in ORACLEDatabase::fetchNext\n");
		return QUILL_FAILURE;
	}

	try {
		rs = rset->next ();
	}  catch (SQLException ex) {
		stmt->closeResultSet (rset);
		conn->terminateStatement (stmt);
		stmt = NULL;
		rset = NULL;

		dprintf(D_ALWAYS, "ERROR FETCHING NEXT\n");
		dprintf(D_ALWAYS, "Error number: %d, Error message: %s in ORACLEDatabase::fetchNext\n", ex.getErrorCode(), ex.getMessage().c_str());
		errorMsg.sprintf("Error number: %d, Error message: %s", 
						ex.getErrorCode(), ex.getMessage().c_str());

		if (ex.getErrorCode() == 3113 ||
		    ex.getErrorCode() == 3114 ||
			ex.getErrorCode() == 4031) {
			emailDBError(ex.getErrorCode(), "Oracle");
			disconnectDB();
		}   

		return QUILL_FAILURE;			
	}

	if (rs == ResultSet::END_OF_FETCH) {
		stmt->closeResultSet (rset);
		conn->terminateStatement (stmt); 
		rset = NULL;
		stmt = NULL;
		return QUILL_FAILURE;
	} else {
		return QUILL_SUCCESS;
	}
}
*/

//! get a column from the current row for the executed query
// the string value returned must be copied out before calling getValue 
// again.
const char*
ORACLEDatabase::getValue(int row, int col)
{
	ResultSet::Status rs;
	const char *rv;

	if (!queryRes) {
		dprintf(D_ALWAYS, "no result to fetch in ORACLEDatabase::getValue\n");
		return NULL;
	}
        
	try {
			/* if we are trying to fetch a row which is past, 
			   error out 
			*/
		if (row < queryResCursor) {
			dprintf(D_ALWAYS, "Fetching previous row is not supported in ORACLEDatabase::getValue\n");
			return NULL;
		}

			/* first position to the row as specified */
		while (queryResCursor < row) {
			rs = queryRes->next (); 

				/* the requested row doesn't exist */	
			if (rs == ResultSet::END_OF_FETCH) {
				queryStmt->closeResultSet (queryRes);
				conn->terminateStatement (queryStmt); 
				queryRes = NULL;
				queryStmt = NULL;
				return NULL;
			}

			queryResCursor++;	
		}

			/* col index is 0 based, for oracle, since col index 
			   is 1 based, therefore add 1 to the column index 
			*/
		cv = (queryRes->getString(col+1)).c_str();
	} catch (SQLException ex) {
		queryStmt->closeResultSet (queryRes);
		conn->terminateStatement (queryStmt);
		queryStmt = NULL;
		queryRes = NULL;

		dprintf(D_ALWAYS, "Error number: %d, Error message: %s in ORACLEDatabase::getValue\n", ex.getErrorCode(), ex.getMessage().c_str());
		errorMsg.sprintf("Error number: %d, Error message: %s", 
						ex.getErrorCode(), ex.getMessage().c_str());
		
			/* ORA-03113 means that the connection between Client 
			   and Server process was broken.
			*/
			/* ORA-04031 means that shared pool is out of memory. Disconnect
			   so that we avoid getting the same error over and over. Also 
			   this will avoid the sql log being truncated.
			 */
		if (ex.getErrorCode() == 3113 ||
			ex.getErrorCode() == 3114 ||
			ex.getErrorCode() == 4031) {
			emailDBError(ex.getErrorCode(), "Oracle");
			disconnectDB();
		}               
        
		return NULL;                    
	}
	
        
	rv = cv.Value();

	return rv;
}

//! get a column from the current row for the executed query as an integer
/*
int
ORACLEDatabase::getIntValue(int col)
{
	int rv;

	if (!rset) {
		dprintf(D_ALWAYS, "no result to fetch in ORACLEDatabase::getIntValue\n");
		return 0;
	}
	
	try {
		rv = rset->getInt(col+1);
	} catch (SQLException ex) {
		stmt->closeResultSet (rset);
		conn->terminateStatement (stmt);
		stmt = NULL;
		rset = NULL;

		dprintf(D_ALWAYS, "Error number: %d, Error message: %s in ORACLEDatabase::getIntValue\n", ex.getErrorCode(), ex.getMessage().c_str());

		if (ex.getErrorCode() == 3113 ||
		    ex.getErrorCode() == 3114 ||
			ex.getErrorCode() == 4031) {
			emailDBError(ex.getErrorCode(), "Oracle");
			disconnectDB();
		}   

		return 0;			
	}

	return rv;
}
*/

//! release the generic query result object
QuillErrCode
ORACLEDatabase::releaseQueryResult()
{
	if(queryRes != NULL) {
		if (queryStmt != NULL) {
			queryStmt->closeResultSet (queryRes);
			conn->terminateStatement (queryStmt);
			queryStmt = NULL;
		}
		
		queryRes = NULL;
	}
	
	return QUILL_SUCCESS;
}

//! check if the connection is ok
QuillErrCode
ORACLEDatabase::checkConnection()
{
	if (connected) 
		return QUILL_SUCCESS;
	else 
		return QUILL_FAILURE;
}

//! check if the connection is ok
QuillErrCode
ORACLEDatabase::resetConnection()
{
	return connectDB();
}

// get the field name at given column index
const char *
ORACLEDatabase::getHistoryHorFieldName(int col)
{

/* 
the following are saved codes which uses Oracle API to get column 
metadata, but haven't been made to work due to a link error 
"undefined reference to `std::vector<oracle::occi::MetaData ..."
*/

/*
	OCCI_STD_NAMESPACE::vector<MetaData>listOfColumns;

	if (!historyHorRes) {
		dprintf(D_ALWAYS, "no result retrieved in ORACLEDatabase::historyHorRes\n");
		return NULL;
	}	

	try {
		listOfColumns = historyHorRes->getColumnListMetaData();			
	} catch (SQLException ex) { 			
		dprintf(D_ALWAYS, "Error number: %d, Error message: %s in ORACLEDatabase::getHistoryHorNumFields\n", ex.getErrorCode(), ex.getMessage().c_str());
		return NULL;
	}	

	if (col >= listOfColumns.size()) {
		dprintf(D_ALWAYS, "column index %d exceeds max column num %d in ORACLEDatabase::getHistoryHorFieldName.\n", col, listOfColumns.size());
		return NULL;
	} else {
		cv = (listOfColumns[col].getString(MetaData::ATTR_NAME)).c_str();
		return cv.Value();
	}
*/

	if (col >= QUILLPP_HistoryHorFieldNum) {
		dprintf(D_ALWAYS, "column index %d exceeds max column num %d in ORACLEDatabase::getHistoryHorFieldName.\n", col, QUILLPP_HistoryHorFieldNum);
		return NULL;
	} else {
		return QUILLPP_HistoryHorFields[col];		
	}

}

// get the number of fields returned in result
const int
ORACLEDatabase::getHistoryHorNumFields()
{

/* 
the following are saved codes which uses Oracle API to get column 
metadata, but haven't been made to work due to a link error 
"undefined reference to `std::vector<oracle::occi::MetaData ..."
*/

/*
	OCCI_STD_NAMESPACE::vector<MetaData>listOfColumns;

	if (!historyHorRes) {
		dprintf(D_ALWAYS, "no result retrieved in ORACLEDatabase::historyHorRes\n");
		return -1;
	}	

	try {
		listOfColumns = historyHorRes->getColumnListMetaData();			
	} catch (SQLException ex) { 			
		dprintf(D_ALWAYS, "Error number: %d, Error message: %s in ORACLEDatabase::getHistoryHorNumFields\n", ex.getErrorCode(), ex.getMessage().c_str());
		return -1;
	}

	return listOfColumns.size();
*/

	return QUILLPP_HistoryHorFieldNum;
}

//! release the history query result object
QuillErrCode
ORACLEDatabase::releaseHistoryResults()
{
	if(historyHorRes != NULL) {
		if (historyHorStmt != NULL) {
			historyHorStmt->closeResultSet (historyHorRes);
			conn->terminateStatement (historyHorStmt);
			historyHorStmt = NULL;
		} else {
			dprintf(D_ALWAYS, "ERROR - statement handle is NULL while historyHorRes is not NULL in ORACLEDatabase::releaseHistoryResults\n");
		}
		
		historyHorRes = NULL;
	}
        
	if(historyVerRes != NULL) {
		if (historyVerStmt != NULL) {
			historyVerStmt->closeResultSet (historyVerRes);
			conn->terminateStatement (historyVerStmt);
			historyVerStmt = NULL;
		} else {
			dprintf(D_ALWAYS, "ERROR - statement handle is NULL while historyVerRes is not NULL in ORACLEDatabase::releaseHistoryResults\n");
		}
		
		historyVerRes = NULL;
	}

	return QUILL_SUCCESS;
}

//! release the job queue query result object
QuillErrCode
ORACLEDatabase::releaseJobQueueResults()
{
	if(procAdsHorRes != NULL) {
		if (procAdsHorStmt != NULL) {
			procAdsHorStmt->closeResultSet (procAdsHorRes);
			conn->terminateStatement (procAdsHorStmt);
			procAdsHorStmt = NULL;
		} else {
			dprintf(D_ALWAYS, "ERROR - statement handle is NULL while procAdsHorRes is not NULL in ORACLEDatabase::releaseJobQueueResult\n");
		}
		
		procAdsHorRes = NULL;
	}
	
	if(procAdsVerRes != NULL) {
		if (procAdsVerStmt != NULL) {
			procAdsVerStmt->closeResultSet (procAdsVerRes);
			conn->terminateStatement (procAdsVerStmt);
			procAdsVerStmt = NULL;
		} else {
			dprintf(D_ALWAYS, "ERROR - statement handle is NULL while procAdsVerRes is not NULL in ORACLEDatabase::releaseJobQueueResult\n");
		}
		
		procAdsVerRes = NULL;
	}
	
	if(clusterAdsVerRes != NULL) {
		if (clusterAdsVerStmt != NULL) {
			clusterAdsVerStmt->closeResultSet (clusterAdsVerRes);
			conn->terminateStatement (clusterAdsVerStmt);
			clusterAdsVerStmt = NULL;
		} else {
			dprintf(D_ALWAYS, "ERROR - statement handle is NULL while clusterAdsVerRes is not NULL in ORACLEDatabase::releaseJobQueueResult\n");
		}
		
		clusterAdsVerRes = NULL;
	}
	
	if(clusterAdsHorRes != NULL) {
		if (clusterAdsHorStmt != NULL) {
			clusterAdsHorStmt->closeResultSet (clusterAdsHorRes);
			conn->terminateStatement (clusterAdsHorStmt);
			clusterAdsHorStmt = NULL;
		} else {
			dprintf(D_ALWAYS, "ERROR - statement handle is NULL while clusterAdsHorRes is not NULL in ORACLEDatabase::releaseJobQueueResult\n");
		}
		
		clusterAdsHorRes = NULL;
	}
	
	return QUILL_SUCCESS;
}

//! get a DBMS error message
const char*
ORACLEDatabase::getDBError()
{
	return errorMsg.Value();
}

/*! get the job queue
 *
 *	\return 
 *		JOB_QUEUE_EMPTY: There is no job in the queue
 *      FAILURE_QUERY_* : error querying table *
 *		QUILL_SUCCESS: There is some job in the queue and query was successful
 *
 *		
 */
QuillErrCode
ORACLEDatabase::getJobQueueDB(int *clusterarray, int numclusters, 
							  int *procarray, int numprocs, 
							  bool isfullscan,
							  const char *scheddname,
							  int& procAdsHorRes_num, int& procAdsVerRes_num,
							  int& clusterAdsHorRes_num, 
							  int& clusterAdsVerRes_num)
{
	MyString procAds_hor_query, procAds_ver_query;
	MyString clusterAds_hor_query, clusterAds_ver_query; 
	MyString clusterpredicate, procpredicate, temppredicate;

	QuillErrCode st;
	int i;

	if(isfullscan) {
		procAds_hor_query.sprintf("SELECT cluster_id, proc_id, jobstatus, imagesize, remoteusercpu, remotewallclocktime, remotehost, globaljobid, jobprio,  args, case when shadowbday is null then null else (extract(day from (SYS_EXTRACT_UTC(shadowbday) - to_timestamp_tz('01/01/1970 UTC', 'MM/DD/YYYY TZD')))*86400 + extract(hour from (SYS_EXTRACT_UTC(shadowbday) - to_timestamp_tz('01/01/1970 UTC', 'MM/DD/YYYY TZD')))*3600 + extract(minute from (SYS_EXTRACT_UTC(shadowbday) - to_timestamp_tz('01/01/1970 UTC', 'MM/DD/YYYY TZD')))*60 + extract(second from (SYS_EXTRACT_UTC(shadowbday) - to_timestamp_tz('01/01/1970 UTC', 'MM/DD/YYYY TZD')))) end as shadowbday, case when enteredcurrentstatus is null then null else (extract(day from (SYS_EXTRACT_UTC(enteredcurrentstatus) - to_timestamp_tz('01/01/1970 UTC', 'MM/DD/YYYY TZD')))*86400 + extract(hour from (SYS_EXTRACT_UTC(enteredcurrentstatus) - to_timestamp_tz('01/01/1970 UTC', 'MM/DD/YYYY TZD')))*3600 + extract(minute from (SYS_EXTRACT_UTC(enteredcurrentstatus) - to_timestamp_tz('01/01/1970 UTC', 'MM/DD/YYYY TZD')))*60 + extract(second from (SYS_EXTRACT_UTC(enteredcurrentstatus) - to_timestamp_tz('01/01/1970 UTC', 'MM/DD/YYYY TZD')))) end as enteredcurrentstatus, numrestarts FROM quillwriter.procads_horizontal WHERE scheddname=\'%s\' ORDER BY cluster_id, proc_id", scheddname);
		procAds_ver_query.sprintf("SELECT cluster_id, proc_id, attr, val FROM quillwriter.procads_vertical WHERE scheddname=\'%s\' ORDER BY cluster_id, proc_id", scheddname);

		clusterAds_hor_query.sprintf("SELECT cluster_id, owner, jobstatus, jobprio, imagesize, (extract(day from (SYS_EXTRACT_UTC(qdate) - to_timestamp_tz('01/01/1970 UTC', 'MM/DD/YYYY TZD')))*86400 + extract(hour from (SYS_EXTRACT_UTC(qdate) - to_timestamp_tz('01/01/1970 UTC', 'MM/DD/YYYY TZD')))*3600 + extract(minute from (SYS_EXTRACT_UTC(qdate) - to_timestamp_tz('01/01/1970 UTC', 'MM/DD/YYYY TZD')))*60 + extract(second from (SYS_EXTRACT_UTC(qdate) - to_timestamp_tz('01/01/1970 UTC', 'MM/DD/YYYY TZD')))) as qdate, remoteusercpu, remotewallclocktime, cmd, args, jobuniverse FROM quillwriter.clusterads_horizontal WHERE scheddname=\'%s\' ORDER BY cluster_id", scheddname);

		clusterAds_ver_query.sprintf("SELECT cluster_id, attr, val FROM quillwriter.clusterads_vertical WHERE scheddname=\'%s\' ORDER BY cluster_id", scheddname);
	}

	/* OK, this is a little confusing.
	 * cluster and proc array are tied together - you can ask for a cluster,
     * or a cluster and a proc, but never just a proc
     * think cluster and procarrays as a an array like this:
     *
     *  42, 1
     *  43, -1
     *  44, 5
     *  44, 6
     *  45, -1
     * 
     * this means return job 42.1, all jobs for cluster 43, only 44.5 and 44.6,
     * and all of cluster 45
     *
     * there is no way to say 'give me proc 5 of every cluster'

		numprocs is never used. numclusters may have redundant information:
		querying for jobs 31.20, 31.21, 31.22..31.25   gives queries likes

		cluster ads hor:  SELECT cluster, owner, jobstatus, jobprio, imagesize, 
           qdate, remoteusercpu, remotewallclocktime, cmd, args  i
            FROM clusterads_horizontal WHERE 
                scheddname='epaulson@swingline.cs.wisc.edu'  AND 
                (cluster = 31) OR (cluster = 31)  OR (cluster = 31)  
                OR (cluster = 31)  OR (cluster = 31)  ORDER BY cluster;

         cluster ads ver: SELECT cluster, attr, val FROM clusterads_vertical 
                     WHERE scheddname='epaulson@swingline.cs.wisc.edu'  AND 
                     (cluster = 31) OR (cluster = 31)  OR (cluster = 31)  OR 
                     (cluster = 31)  OR (cluster = 31)  ORDER BY cluster;

         proc ads hor SELECT cluster, proc, jobstatus, imagesize, remoteusercpu,
             remotewallclocktime, remotehost, globaljobid, jobprio,  args  
            FROM procads_horizontal WHERE 
                     scheddname='epaulson@swingline.cs.wisc.edu'  AND 
                  (cluster = 31 AND proc = 20) OR (cluster = 31 AND proc = 21) 
                 OR (cluster = 31 AND proc = 22)  
                 OR (cluster = 31 AND proc = 23)  
                 OR (cluster = 31 AND proc = 24)  ORDER BY cluster, proc;

         proc ads ver SELECT cluster, proc, attr, val FROM procads_vertical 
             WHERE scheddname='epaulson@swingline.cs.wisc.edu'  
               AND (cluster = 31 AND proc = 20) OR (cluster = 31 AND proc = 21)
            OR (cluster = 31 AND proc = 22)  OR (cluster = 31 AND proc = 23) 
             OR (cluster = 31 AND proc = 24)  ORDER BY cluster, proc;

      --erik, 7.24,2006

	 */


	else {
	    if(numclusters > 0) {
			// build up the cluster predicate
			clusterpredicate.sprintf("%s%d)", 
					" AND ( (cluster_id = ",clusterarray[0]);
			for(i=1; i < numclusters; i++) {
				clusterpredicate.sprintf_cat( 
				"%s%d) ", " OR (cluster_id = ", clusterarray[i] );
      		}

			// now build up the proc predicate string. 	
			// first decide how to open it
			 if(procarray[0] != -1) {
					procpredicate.sprintf("%s%d%s%d)", 
							" AND ( (cluster_id = ", clusterarray[0], 
							" AND proc_id = ", procarray[0]);
	 		} else {  // no proc for this entry, so only have cluster
					procpredicate.sprintf( "%s%d)", 
								" AND ( (cluster_id = ", clusterarray[0]);
	 		}
	
			// fill in the rest of hte proc predicate 
	 		// note that we really want to iterate till numclusters and not 
			// numprocs because procarray has holes and clusterarray does not
			for(i=1; i < numclusters; i++) {
				if(procarray[i] != -1) {
					procpredicate.sprintf_cat( "%s%d%s%d) ", 
					" OR (cluster_id = ",clusterarray[i]," AND proc_id = ",procarray[i]);
				} else { 
					procpredicate.sprintf_cat( "%s%d) ", 
						" OR (cluster_id = ", clusterarray[i]);
				}
			} //end offor loop

			// balance predicate strings, since it needs to get
			// and-ed with the schedd name below
			clusterpredicate += " ) ";
			procpredicate += " ) ";
		} // end of numclusters > 0


		procAds_hor_query.sprintf( 
			"SELECT cluster_id, proc_id, jobstatus, imagesize, remoteusercpu, remotewallclocktime, remotehost, globaljobid, jobprio, args, case when shadowbday is null then null else (extract(day from (SYS_EXTRACT_UTC(shadowbday) - to_timestamp_tz('01/01/1970 UTC', 'MM/DD/YYYY TZD')))*86400 + extract(hour from (SYS_EXTRACT_UTC(shadowbday) - to_timestamp_tz('01/01/1970 UTC', 'MM/DD/YYYY TZD')))*3600 + extract(minute from (SYS_EXTRACT_UTC(shadowbday) - to_timestamp_tz('01/01/1970 UTC', 'MM/DD/YYYY TZD')))*60 + extract(second from (SYS_EXTRACT_UTC(shadowbday) - to_timestamp_tz('01/01/1970 UTC', 'MM/DD/YYYY TZD')))) end as shadowbday, case when enteredcurrentstatus is null then null else (extract(day from (SYS_EXTRACT_UTC(enteredcurrentstatus) - to_timestamp_tz('01/01/1970 UTC', 'MM/DD/YYYY TZD')))*86400 + extract(hour from (SYS_EXTRACT_UTC(enteredcurrentstatus) - to_timestamp_tz('01/01/1970 UTC', 'MM/DD/YYYY TZD')))*3600 + extract(minute from (SYS_EXTRACT_UTC(enteredcurrentstatus) - to_timestamp_tz('01/01/1970 UTC', 'MM/DD/YYYY TZD')))*60 + extract(second from (SYS_EXTRACT_UTC(enteredcurrentstatus) - to_timestamp_tz('01/01/1970 UTC', 'MM/DD/YYYY TZD')))) end as enteredcurrentstatus, numrestarts FROM quillwriter.procads_horizontal WHERE scheddname=\'%s\' %s ORDER BY cluster_id, proc_id", scheddname, procpredicate.Value() );

		procAds_ver_query.sprintf(
	"SELECT cluster_id, proc_id, attr, val FROM quillwriter.procads_vertical WHERE scheddname=\'%s\' %s ORDER BY cluster_id, proc_id", 
			scheddname, procpredicate.Value() );

		clusterAds_hor_query.sprintf(
			"SELECT cluster_id, owner, jobstatus, jobprio, imagesize, (extract(day from (SYS_EXTRACT_UTC(qdate) - to_timestamp_tz('01/01/1970 UTC', 'MM/DD/YYYY TZD')))*86400 + extract(hour from (SYS_EXTRACT_UTC(qdate) - to_timestamp_tz('01/01/1970 UTC', 'MM/DD/YYYY TZD')))*3600 + extract(minute from (SYS_EXTRACT_UTC(qdate) - to_timestamp_tz('01/01/1970 UTC', 'MM/DD/YYYY TZD')))*60 + extract(second from (SYS_EXTRACT_UTC(qdate) - to_timestamp_tz('01/01/1970 UTC', 'MM/DD/YYYY TZD')))) as qdate, remoteusercpu, remotewallclocktime, cmd, args, jobuniverse FROM quillwriter.clusterads_horizontal WHERE scheddname=\'%s\' %s ORDER BY cluster_id", scheddname, clusterpredicate.Value());

		clusterAds_ver_query.sprintf(
		"SELECT cluster_id, attr, val FROM quillwriter.clusterads_vertical WHERE scheddname=\'%s\' %s ORDER BY cluster_id", scheddname, clusterpredicate.Value());	
	}

	/*dprintf(D_ALWAYS, "clusterAds_hor_query = %s\n", clusterAds_hor_query.Value());
	dprintf(D_ALWAYS, "clusterAds_ver_query = %s\n", clusterAds_ver_query.Value());
	dprintf(D_ALWAYS, "procAds_hor_query = %s\n", procAds_hor_query.Value());
	dprintf(D_ALWAYS, "procAds_ver_query = %s\n", procAds_ver_query.Value()); */

	  // Query against ClusterAds_Hor Table
  if ((st = execQuery(clusterAds_hor_query.Value(), clusterAdsHorRes, 
					  clusterAdsHorStmt,
					  clusterAdsHorRes_num)) == QUILL_FAILURE) {
	  return FAILURE_QUERY_CLUSTERADS_HOR;
  }
	  // Query against ClusterAds_Ver Table
  if ((st = execQuery(clusterAds_ver_query.Value(), clusterAdsVerRes, 
					  clusterAdsVerStmt,
					  clusterAdsVerRes_num)) == QUILL_FAILURE) {
		// FIXME to return something other than clusterads_num!
	  return FAILURE_QUERY_CLUSTERADS_VER;
  }
	  // Query against procAds_Hor Table
  if ((st = execQuery(procAds_hor_query.Value(), procAdsHorRes, 
					  procAdsHorStmt,
					  procAdsHorRes_num)) == QUILL_FAILURE) {
	  return FAILURE_QUERY_PROCADS_HOR;
  }
	  // Query against procAds_ver Table
  if ((st = execQuery(procAds_ver_query.Value(), procAdsVerRes, 
					  procAdsVerStmt,
					  procAdsVerRes_num)) == QUILL_FAILURE) {
	  return FAILURE_QUERY_PROCADS_VER;
  }
  
  if (clusterAdsVerRes_num == 0 && clusterAdsHorRes_num == 0) {
    return JOB_QUEUE_EMPTY;
  }

  clusterAdsVerResCursor = clusterAdsHorResCursor = procAdsVerResCursor 
  = procAdsHorResCursor = -1;

  return QUILL_SUCCESS;
}

/*! get the historical information
 *
 *	\return
 *		QUILL_SUCCESS: declare cursor succeeded 
 *		FAILURE_QUERY_*: query failed
 */
QuillErrCode
ORACLEDatabase::openCursorsHistory(SQLQuery *queryhor,
                                  SQLQuery *queryver,
                                  bool longformat)
{
	QuillErrCode st;
	int num_result;
	if ((st = execQuery(queryhor->getQuery(), historyHorRes, historyHorStmt, num_result)) == QUILL_FAILURE) {
		return FAILURE_QUERY_HISTORYADS_HOR;
	}

	if (num_result == 0) {
		return HISTORY_EMPTY;
	}

	if (longformat && (st = execQuery(queryver->getQuery(), historyVerRes, historyVerStmt, num_result)) == QUILL_FAILURE) {
		return FAILURE_QUERY_HISTORYADS_VER;
	}
	
	historyHorResCursor = historyVerResCursor = -1;

	return QUILL_SUCCESS;
}


QuillErrCode
ORACLEDatabase::closeCursorsHistory(SQLQuery *queryhor,
                                  SQLQuery *queryver,
                                  bool longformat)
{	
		/* nothing to be done for oracle */
	return QUILL_SUCCESS;
}

//! get a value retrieved from History_Horizontal table
QuillErrCode
ORACLEDatabase::getHistoryHorValue(SQLQuery *queryhor, int row, int col, const char **value)
{
	ResultSet::Status rs;

	if (!historyHorRes) {
		dprintf(D_ALWAYS, "no historyHorRes to fetch in ORACLEDatabase::getJobQueueHistoryHorValue\n");
		*value = NULL;
		return FAILURE_QUERY_HISTORYADS_HOR;
	}	
	
	try {
			/* if we are trying to fetch a row which is past, 
			   error out 
			*/
		if (row < historyHorResCursor) {
			dprintf(D_ALWAYS, "Fetching previous row is not supported in ORACLEDatabase::getJobQueueHistoryHorValue\n");
			*value = NULL;
			return FAILURE_QUERY_HISTORYADS_HOR;
		}

			/* first position to the row as specified */
		while (historyHorResCursor < row) {
			rs = historyHorRes->next (); 

				/* the requested row doesn't exist */	
			if (rs == ResultSet::END_OF_FETCH) {
				historyHorStmt->closeResultSet (historyHorRes);
				conn->terminateStatement (historyHorStmt); 
				historyHorRes = NULL;
				historyHorStmt = NULL;
				*value = NULL;
				return DONE_HISTORY_HOR_CURSOR;
			}

			historyHorResCursor++;	
		}
		
			/* col index is 0 based, for oracle, since col index 
			   is 1 based, therefore add 1 to the column index 
			*/
		if (!history_hor_clob_field[col]) {
			cv = (historyHorRes->getString(col+1)).c_str();
		} else {
			cv = getStringFromClob(historyHorRes, col);
		}

			/* add double quotes back if needed */
		if (QUILLPP_HistoryHorIsQuoted[col]) {
			MyString temp = cv;
			cv.sprintf("\"%s\"", temp.Value());
		} 

	} catch (SQLException ex) {
		historyHorStmt->closeResultSet (historyHorRes);
		conn->terminateStatement (historyHorStmt);
		historyHorStmt = NULL;
		historyHorRes = NULL;

		dprintf(D_ALWAYS, "Error number: %d, Error message: %s in ORACLEDatabase::getJobQueueHistoryHorValue\n", ex.getErrorCode(), ex.getMessage().c_str());
		errorMsg.sprintf("Error number: %d, Error message: %s", 
						ex.getErrorCode(), ex.getMessage().c_str());
		
			/* ORA-03113 means that the connection between Client 
			   and Server process was broken.
			*/
			/* ORA-04031 means that shared pool is out of memory. Disconnect
			   so that we avoid getting the same error over and over. Also 
			   this will avoid the sql log being truncated.
			 */		
		if (ex.getErrorCode() == 3113 ||
			ex.getErrorCode() == 3114 ||
			ex.getErrorCode() == 4031) {
			emailDBError(ex.getErrorCode(), "Oracle");
			disconnectDB();
		}    

		*value = NULL;
		return FAILURE_QUERY_HISTORYADS_HOR;
	}

	*value = cv.Value();

	return QUILL_SUCCESS;			
}


//! get a value retrieved from History_Vertical table
QuillErrCode
ORACLEDatabase::getHistoryVerValue(SQLQuery *queryver, int row, int col, const char **value)
{
	ResultSet::Status rs;

	if (!historyVerRes) {
		dprintf(D_ALWAYS, "no historyVerRes to fetch in ORACLEDatabase::getJobQueueHistoryVerValue\n");
		*value = NULL;
		return FAILURE_QUERY_HISTORYADS_VER;
	}

	try {
			/* if we are trying to fetch a row which is past, 
			   error out 
			*/
		if (row < historyVerResCursor) {
			dprintf(D_ALWAYS, "Fetching previous row is not supported in ORACLEDatabase::getJobQueueHistoryVerValue\n");
			*value = NULL;
			return FAILURE_QUERY_HISTORYADS_VER;
		}

			/* first position to the row as specified */
		while (historyVerResCursor < row) {
			rs = historyVerRes->next (); 

				/* the requested row doesn't exist */	
			if (rs == ResultSet::END_OF_FETCH) {
				historyVerStmt->closeResultSet (historyVerRes);
				conn->terminateStatement (historyVerStmt); 
				historyVerRes = NULL;
				historyVerStmt = NULL;
				*value = NULL;
				return DONE_HISTORY_VER_CURSOR;
			}

			historyVerResCursor++;	
		}
		
			/* col index is 0 based, for oracle, since col index 
			   is 1 based, therefore add 1 to the column index 
			*/
		if (!history_ver_clob_field[col]) {
			cv = (historyVerRes->getString(col+1)).c_str();		
		} else {
			cv = getStringFromClob(historyVerRes, col);
		}

	} catch (SQLException ex) {
		historyVerStmt->closeResultSet (historyVerRes);
		conn->terminateStatement (historyVerStmt);
		historyVerStmt = NULL;
		historyVerRes = NULL;

		dprintf(D_ALWAYS, "Error number: %d, Error message: %s in ORACLEDatabase::getJobQueueHistoryVerValue\n", ex.getErrorCode(), ex.getMessage().c_str());
		errorMsg.sprintf("Error number: %d, Error message: %s", 
						ex.getErrorCode(), ex.getMessage().c_str());
		
			/* ORA-03113 means that the connection between Client 
			   and Server process was broken.
			*/
			/* ORA-04031 means that shared pool is out of memory. Disconnect
			   so that we avoid getting the same error over and over. Also 
			   this will avoid the sql log being truncated.
			 */
		if (ex.getErrorCode() == 3113 ||
			ex.getErrorCode() == 3114 ||
			ex.getErrorCode() == 4031) {
			emailDBError(ex.getErrorCode(), "Oracle");
			disconnectDB();
		}       

		*value = NULL;
		return FAILURE_QUERY_HISTORYADS_VER;
	}
	
    *value = cv.Value();

	return QUILL_SUCCESS;				
}

//! get a value retrieved from ProcAds_Hor table
const char*
ORACLEDatabase::getJobQueueProcAds_HorValue(int row, int col)
{
	ResultSet::Status rs;
	const char *rv;

	if (!procAdsHorRes) {
		dprintf(D_ALWAYS, "no procAdsHorRes to fetch in ORACLEDatabase::getJobQueueProcAds_HorValue\n");
		return NULL;
	}

	try {
			/* if we are trying to fetch a row which is past, 
			   error out 
			*/
		if (row < procAdsHorResCursor) {
			dprintf(D_ALWAYS, "Fetching previous row is not supported in ORACLEDatabase::getJobQueueProcAds_HorValue\n");
			return NULL;
		}

			/* first position to the row as specified */
		while (procAdsHorResCursor < row) {
			rs = procAdsHorRes->next (); 

				/* the requested row doesn't exist */	
			if (rs == ResultSet::END_OF_FETCH) {
				procAdsHorStmt->closeResultSet (procAdsHorRes);
				conn->terminateStatement (procAdsHorStmt); 
				procAdsHorRes = NULL;
				procAdsHorStmt = NULL;
				return NULL;
			}

			procAdsHorResCursor++;	
		}
		
			/* col index is 0 based, for oracle, since col index 
			   is 1 based, therefore add 1 to the column index 
			*/
		if (!proc_hor_clob_field[col]) {
			cv = (procAdsHorRes->getString(col+1)).c_str();
		} else {
			cv = getStringFromClob(procAdsHorRes, col);
		}		

		if (procAdsHorRes->isNull(col+1)) {
			return NULL;
		}

			/* add double quotes back if needed */
		if (proc_field_is_quoted[col]) {
			MyString temp = cv;
			cv.sprintf("\"%s\"", temp.Value());
		} 
		
	} catch (SQLException ex) {
		procAdsHorStmt->closeResultSet (procAdsHorRes);
		conn->terminateStatement (procAdsHorStmt);
		procAdsHorStmt = NULL;
		procAdsHorRes = NULL;

		dprintf(D_ALWAYS, "Error number: %d, Error message: %s in ORACLEDatabase::getJobQueueProcAds_HorValue\n", ex.getErrorCode(), ex.getMessage().c_str());
		errorMsg.sprintf("Error number: %d, Error message: %s", 
						ex.getErrorCode(), ex.getMessage().c_str());
		
			/* ORA-03113 means that the connection between Client 
			   and Server process was broken.
			*/
			/* ORA-04031 means that shared pool is out of memory. Disconnect
			   so that we avoid getting the same error over and over. Also 
			   this will avoid the sql log being truncated.
			 */
		if (ex.getErrorCode() == 3113 ||
			ex.getErrorCode() == 3114 ||
			ex.getErrorCode() == 4031) {
			emailDBError(ex.getErrorCode(), "Oracle");
			disconnectDB();
		}               

		return NULL;             		
	}

	rv = cv.Value();

	return rv;
}

//! get a value retrieved from ProcAds_Ver table
const char*
ORACLEDatabase::getJobQueueProcAds_VerValue(int row, int col)
{
	ResultSet::Status rs;
	const char *rv;

	if (!procAdsVerRes) {
		dprintf(D_ALWAYS, "no procAdsVerRes to fetch in ORACLEDatabase::getJobQueueProcAds_VerValue\n");
		return NULL;
	}
	
	try {
			/* if we are trying to fetch a row which is past, 
			   error out 
			*/
		if (row < procAdsVerResCursor) {
			dprintf(D_ALWAYS, "Fetching previous row is not supported in ORACLEDatabase::getJobQueueProcAds_VerValue\n");
			return NULL;
		}

			/* first position to the row as specified */
		while (procAdsVerResCursor < row) {
			rs = procAdsVerRes->next (); 

				/* the requested row doesn't exist */	
			if (rs == ResultSet::END_OF_FETCH) {
				procAdsVerStmt->closeResultSet (procAdsVerRes);
				conn->terminateStatement (procAdsVerStmt); 
				procAdsVerRes = NULL;
				procAdsVerStmt = NULL;
				return NULL;
			}

			procAdsVerResCursor++;	
		}
		
			/* col index is 0 based, for oracle, since col index 
			   is 1 based, therefore add 1 to the column index 
			*/
		if (!proc_ver_clob_field[col]) {
			cv = (procAdsVerRes->getString(col+1)).c_str();		
		} else {
			cv = getStringFromClob(procAdsVerRes, col);
		}

	} catch (SQLException ex) {
		procAdsVerStmt->closeResultSet (procAdsVerRes);
		conn->terminateStatement (procAdsVerStmt);
		procAdsVerStmt = NULL;
		procAdsVerRes = NULL;

		dprintf(D_ALWAYS, "Error number: %d, Error message: %s in ORACLEDatabase::getJobQueueProcAds_VerValue\n", ex.getErrorCode(), ex.getMessage().c_str());
		errorMsg.sprintf("Error number: %d, Error message: %s", 
						ex.getErrorCode(), ex.getMessage().c_str());
		
			/* ORA-03113 means that the connection between Client 
			   and Server process was broken.
			*/
			/* ORA-04031 means that shared pool is out of memory. Disconnect
			   so that we avoid getting the same error over and over. Also 
			   this will avoid the sql log being truncated.
			 */		
		if (ex.getErrorCode() == 3113 ||
			ex.getErrorCode() == 3114 ||
			ex.getErrorCode() == 4031) {
			emailDBError(ex.getErrorCode(), "Oracle");
			disconnectDB();
		}

		return NULL;             		
	}

	rv = cv.Value();

	return rv;
}

//! get the field name at given column index from the cluster ads
const char *
ORACLEDatabase::getJobQueueClusterHorFieldName(int col) 
{
	return cluster_field_names[col];

		/* we don't use the column names as in table schema because they 
		   are not exactly what's needed for a classad
		*/
		//return PQfname(clusterAdsHorRes, col);
}

//! get number of fields returned in the horizontal cluster ads
const int 
ORACLEDatabase::getJobQueueClusterHorNumFields() 
{
  return cluster_field_num;
}

//! get the field name at given column index from proc ads
const char *
ORACLEDatabase::getJobQueueProcHorFieldName(int col) 
{
		//return PQfname(procAdsHorRes, col);
	return proc_field_names[col];
}

//! get number of fields in the proc ad horizontal
const int 
ORACLEDatabase::getJobQueueProcHorNumFields() 
{
  return proc_field_num;
}

//! get a value retrieved from ClusterAds_Hor table
const char*
ORACLEDatabase::getJobQueueClusterAds_HorValue(int row, int col)
{
	ResultSet::Status rs;
	const char *rv;

	if (!clusterAdsHorRes) {
		dprintf(D_ALWAYS, "no clusterAdsHorRes to fetch in ORACLEDatabase::getJobQueueClusterAds_HorValue\n");
		return NULL;
	}

	try {
			/* if we are trying to fetch a row which is past, 
			   error out 
			*/
		if (row < clusterAdsHorResCursor) {
			dprintf(D_ALWAYS, "Fetching previous row is not supported in ORACLEDatabase::getJobQueueClusterAds_HorValue\n");
			return NULL;
		}

			/* first position to the row as specified */
		while (clusterAdsHorResCursor < row) {
			rs = clusterAdsHorRes->next (); 

				/* the requested row doesn't exist */	
			if (rs == ResultSet::END_OF_FETCH) {
				clusterAdsHorStmt->closeResultSet (clusterAdsHorRes);
				conn->terminateStatement (clusterAdsHorStmt); 
				clusterAdsHorRes = NULL;
				clusterAdsHorStmt = NULL;
				return NULL;
			}

			clusterAdsHorResCursor++;	
		}
		
			/* col index is 0 based, for oracle, since col index 
			   is 1 based, therefore add 1 to the column index 
			*/
		if (!cluster_hor_clob_field[col]) {
			cv = (clusterAdsHorRes->getString(col+1)).c_str();		
		} else {
			cv = getStringFromClob(clusterAdsHorRes, col);
		}

		if (clusterAdsHorRes->isNull(col+1)) {
			return NULL;
		}

			/* add double quotes back if needed */
		if (cluster_field_is_quoted[col]) {
			MyString temp = cv;
			cv.sprintf("\"%s\"", temp.Value());
		} 
		
	} catch (SQLException ex) {
		clusterAdsHorStmt->closeResultSet (clusterAdsHorRes);
		conn->terminateStatement (clusterAdsHorStmt);
		clusterAdsHorStmt = NULL;
		clusterAdsHorRes = NULL;

		dprintf(D_ALWAYS, "Error number: %d, Error message: %s in ORACLEDatabase::getJobQueueClusterAds_HorValue\n", ex.getErrorCode(), ex.getMessage().c_str());
		errorMsg.sprintf("Error number: %d, Error message: %s", 
						ex.getErrorCode(), ex.getMessage().c_str());
		
			/* ORA-03113 means that the connection between Client 
			   and Server process was broken.
			*/
			/* ORA-04031 means that shared pool is out of memory. Disconnect
			   so that we avoid getting the same error over and over. Also 
			   this will avoid the sql log being truncated.
			 */
		if (ex.getErrorCode() == 3113 ||
			ex.getErrorCode() == 3114 ||
			ex.getErrorCode() == 4031) {
			emailDBError(ex.getErrorCode(), "Oracle");
			disconnectDB();
		}
        
		return NULL;             		
	}

	rv = cv.Value();

	return rv;
}

//! get a value retrieved from ClusterAds_Ver table
const char*
ORACLEDatabase::getJobQueueClusterAds_VerValue(int row, int col)
{
	ResultSet::Status rs;
	const char *rv;

	if (!clusterAdsVerRes) {
		dprintf(D_ALWAYS, "no clusterAdsVerRes to fetch in ORACLEDatabase::getJobQueueClusterAds_VerValue\n");
		return NULL;
	}

	try {
			/* if we are trying to fetch a row which is past, 
			   error out 
			*/
		if (row < clusterAdsVerResCursor) {
			dprintf(D_ALWAYS, "Fetching previous row is not supported in ORACLEDatabase::getJobQueueClusterAds_VerValue\n");
			return NULL;
		}

			/* first position to the row as specified */
		while (clusterAdsVerResCursor < row) {
			rs = clusterAdsVerRes->next (); 

				/* the requested row doesn't exist */	
			if (rs == ResultSet::END_OF_FETCH) {
				clusterAdsVerStmt->closeResultSet (clusterAdsVerRes);
				conn->terminateStatement (clusterAdsVerStmt); 
				clusterAdsVerRes = NULL;
				clusterAdsVerStmt = NULL;
				return NULL;
			}

			clusterAdsVerResCursor++;	
		}
		
			/* col index is 0 based, for oracle, since col index 
			   is 1 based, therefore add 1 to the column index 
			*/
		if (!cluster_ver_clob_field[col]) {
			cv = (clusterAdsVerRes->getString(col+1)).c_str();		
		} else {
			cv = getStringFromClob(clusterAdsVerRes, col);
		}

	} catch (SQLException ex) {
		clusterAdsVerStmt->closeResultSet (clusterAdsVerRes);
		conn->terminateStatement (clusterAdsVerStmt);
		clusterAdsVerStmt = NULL;
		clusterAdsVerRes = NULL;

		dprintf(D_ALWAYS, "Error number: %d, Error message: %s in ORACLEDatabase::getJobQueueClusterAds_VerValue\n", ex.getErrorCode(), ex.getMessage().c_str());
		errorMsg.sprintf("Error number: %d, Error message: %s", 
						ex.getErrorCode(), ex.getMessage().c_str());		

			/* ORA-03113 means that the connection between Client 
			   and Server process was broken.
			*/
			/* ORA-04031 means that shared pool is out of memory. Disconnect
			   so that we avoid getting the same error over and over. Also 
			   this will avoid the sql log being truncated.
			 */
		if (ex.getErrorCode() == 3113 ||
			ex.getErrorCode() == 3114 ||
			ex.getErrorCode() == 4031) {
			emailDBError(ex.getErrorCode(), "Oracle");
			disconnectDB();
		}               

		return NULL;             		
	}

	rv = cv.Value();

	return rv;
}

static MyString getStringFromClob(ResultSet *res, int col)
{
	MyString rv;
	
	Clob tmplob;
	unsigned char *tmpbuf;
	unsigned int len;

	tmplob = res->getClob(col+1);	

	if (!tmplob.isNull()) {
		len = tmplob.length();		
		tmpbuf = (unsigned char *)malloc(len+1);
			
		tmplob.read(len, tmpbuf, len, 1);
		tmpbuf[len] = '\0';

		rv = (char *)tmpbuf;

		free(tmpbuf);
	} else {
		rv = "";
	}
	
	return rv;
}

/*! execute a command with binding variables
 *
 *  execaute SQL which has bind variables, the only data types
 *  supported for now are: string, timestamp, integer
 *  
 *  the number of bind variables in the sql must be the same as the
 *  number of strings pointed by val_arr. 
 *  1) bnd_cnt should be the number of strings in the array;
 *  2) val_arr is the array of strings;
 *  3) typ_arr is the array of data types for the strings;
 *
 */
QuillErrCode 
ORACLEDatabase::execCommandWithBind(const char* sql, 
									int bnd_cnt,
									const char** val_arr,
									QuillAttrDataType *typ_arr)
{
	struct timeval tvStart, tvEnd;
	int i;
	oracle::occi::Stream *instream;

	if (!connected) {
		dprintf(D_ALWAYS, "Not connected to database in ORACLEDatabase::execCommand\n");
		return QUILL_FAILURE;
	}

	dprintf(D_FULLDEBUG, "SQL COMMAND: %s\n", sql);
	
#ifdef TT_COLLECT_SQL
	fprintf(sqllog_fp, "%s;\n", sql);
	fflush(sqllog_fp);
#endif

#ifdef TT_TIME_SQL
	gettimeofday( &tvStart, NULL );
#endif

	try {
		stmt = conn->createStatement (sql);
		for ( i = 1; i <= bnd_cnt; i++) {
			if (typ_arr[i-1] == CONDOR_TT_TYPE_STRING) {
				stmt->setString(i, val_arr[i-1]);
			} else if (typ_arr[i-1] == CONDOR_TT_TYPE_NUMBER) {
				stmt->setInt(i, *(const int *)val_arr[i-1]);
			} else if (typ_arr[i-1] == CONDOR_TT_TYPE_TIMESTAMP) {
					// timestamp
				oracle::occi::Timestamp ts1;
				ts1.fromText(val_arr[i-1], 
							 QUILL_ORACLE_TIMESTAMP_FORAMT, 
							 "", env);
				stmt->setTimestamp(i, ts1);				
			} else {
				dprintf(D_ALWAYS, "unknown data type in ORACLEDatabase::execCommandWithBind\n");
				conn->terminateStatement (stmt);
				return QUILL_FAILURE;				
			}
		}

		stmt->executeUpdate ();

	} catch (SQLException ex) {
		dprintf(D_ALWAYS, "ERROR EXECUTING UPDATE\n");
		dprintf(D_ALWAYS,  "[SQL: %s]\n", sql);		
		dprintf(D_ALWAYS, "Error number: %d, Error message: %s in ORACLEDatabase::execCommand\n", ex.getErrorCode(), ex.getMessage().c_str());
		errorMsg.sprintf("Error number: %d, Error message: %s", 
						ex.getErrorCode(), ex.getMessage().c_str());

		conn->terminateStatement (stmt);
		stmt = NULL;

			/* ORA-03113 means that the connection between Client and Server 
			   process was broken.
			 */
			/* ORA-04031 means that shared pool is out of memory. Disconnect
			   so that we avoid getting the same error over and over. Also 
			   this will avoid the sql log being truncated.
			 */
		if (ex.getErrorCode() == 3113 ||
			ex.getErrorCode() == 3114 ||
			ex.getErrorCode() == 4031) {
			emailDBError(ex.getErrorCode(), "Oracle");
			disconnectDB();
		}

		return QUILL_FAILURE;				
	}

	conn->terminateStatement (stmt);	

#ifdef TT_TIME_SQL
	gettimeofday( &tvEnd, NULL );

	dprintf(D_FULLDEBUG, "Execution time: %ld\n", 
			(tvEnd.tv_sec - tvStart.tv_sec)*1000 + 
			(tvEnd.tv_usec - tvStart.tv_usec)/1000);
#endif
	
	stmt = NULL;
	return QUILL_SUCCESS;
}

/*! execute a query with binding variables
 *
 *  execaute SQL which has bind variables, the only data types
 *  supported for now are: string, timestamp, integer
 *  
 *  the number of bind variables in the sql must be the same as the
 *  number of strings pointed by val_arr. 
 *  1) bnd_cnt should be the number of strings in the array;
 *  2) val_arr is the array of strings;
 *  3) typ_arr is the array of data types for the strings;
 *
 */
/*! execute a SQL query
 */
QuillErrCode
ORACLEDatabase::execQueryWithBind(const char* sql,
						  int bnd_cnt,
						  const char **val_arr,
						  QuillAttrDataType *typ_arr,
						  int &num_result)
{
	ResultSet *res;
	ResultSet::Status rs;
	int temp=0;
	int i;

	queryResCursor = -1;

	if (!connected) {
		dprintf(D_ALWAYS, "Not connected to database in ORACLEDatabase::execQuery\n");
		num_result = -1;
		return QUILL_FAILURE;
	}

	dprintf(D_FULLDEBUG, "SQL Query = %s\n", sql);

	try {
		queryStmt = conn->createStatement (sql);

		for ( i = 1; i <= bnd_cnt; i++) {
			if (typ_arr[i-1] == CONDOR_TT_TYPE_STRING) {
				queryStmt->setString(i, val_arr[i-1]);
			} else if (typ_arr[i-1] == CONDOR_TT_TYPE_NUMBER) {
				queryStmt->setInt(i, *(const int *)val_arr[i-1]);
			} else if (typ_arr[i-1] == CONDOR_TT_TYPE_TIMESTAMP) {
					// timestamp
				oracle::occi::Timestamp ts1;
				ts1.fromText(val_arr[i-1], 
							 QUILL_ORACLE_TIMESTAMP_FORAMT, 
							 "", env);
				queryStmt->setTimestamp(i, ts1);				
			} else {
				dprintf(D_ALWAYS, "unknown data type in ORACLEDatabase::execQueryWithBind\n");
				conn->terminateStatement (queryStmt);
				return QUILL_FAILURE;				
			}
		}

		res = queryStmt->executeQuery ();
		
			/* fetch from res to count the number of rows */
		while ((rs = res->next()) != ResultSet::END_OF_FETCH) {
			temp++;  	
		}

		queryStmt->closeResultSet (res);
		
			/* query again to get new result structure to pass back */
		queryRes = queryStmt->executeQuery();
		num_result = temp;	
		
	} catch (SQLException ex) {
		conn->terminateStatement (queryStmt);
		queryRes = NULL;
		queryStmt = NULL;
		
		dprintf(D_ALWAYS, "ERROR EXECUTING QUERY\n");
		dprintf(D_ALWAYS,  "[SQL: %s]\n", sql);         
		dprintf(D_ALWAYS, "Error number: %d, Error message: %s in ORACLEDatabase::execQuery\n", ex.getErrorCode(), ex.getMessage().c_str());
		errorMsg.sprintf("Error number: %d, Error message: %s", 
						ex.getErrorCode(), ex.getMessage().c_str());
		
			/* ORA-03113 means that the connection between Client 
			   and Server process was broken.
			*/
			/* ORA-04031 means that shared pool is out of memory. Disconnect
			   so that we avoid getting the same error over and over. Also 
			   this will avoid the sql log being truncated.
			 */
		if (ex.getErrorCode() == 3113 ||
			ex.getErrorCode() == 3114 ||
			ex.getErrorCode() == 4031) {
			emailDBError(ex.getErrorCode(), "Oracle");
			disconnectDB();
		}    

		num_result = -1;

		return QUILL_FAILURE;                 
	}

	return QUILL_SUCCESS;
}
