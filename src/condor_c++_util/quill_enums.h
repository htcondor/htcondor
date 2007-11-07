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

#ifndef __QUILL_ENUMS_H__
#define __QUILL_ENUMS_H__

//the IS was prepended to the below constants in order 
//to avoid conflict with occurences of the same symbol
//in other parts of condor code, e.g. ../h/proc.h

enum    JobIdType {   IS_CLUSTER_ID,
                     	IS_PROC_ID,
                        IS_UNKNOWN_ID};

enum    XactState {    NOT_IN_XACT,
                        BEGIN_XACT,
                        COMMIT_XACT,
                        ABORT_XACT};

enum QuillErrCode {	    QUILL_FAILURE,
						FAILURE_QUERY_PROCADS_HOR,
						FAILURE_QUERY_PROCADS_VER,
						FAILURE_QUERY_CLUSTERADS_HOR,
						FAILURE_QUERY_CLUSTERADS_VER,
						FAILURE_QUERY_HISTORYADS_HOR,
						FAILURE_QUERY_HISTORYADS_VER,
						JOB_QUEUE_EMPTY,
						HISTORY_EMPTY,
						DONE_JOBS_CURSOR,
						DONE_HISTORY_HOR_CURSOR,
						DONE_HISTORY_VER_CURSOR,
						DONE_CLUSTERADS_CURSOR,
						DONE_PROCADS_CURSOR,
						DONE_PROCADS_CUR_CLUSTERAD,
						QUILL_SUCCESS};


enum ProbeResultType {  PROBE_ERROR, 
						NO_CHANGE, 
						INIT_QUILL, 
						ADDITION, 
						COMPRESSED};

enum FileOpErrCode {    FILE_OPEN_ERROR,
						FILE_READ_ERROR,
						FILE_WRITE_ERROR,
						FILE_READ_EOF,
                        FILE_READ_SUCCESS,
                        FILE_OP_SUCCESS};


typedef enum
	{
		T_ORACLE, 
		T_PGSQL, 
		T_DB2, 
		T_SQLSERVER,
		T_MYSQL
	} dbtype;

enum QuillAttrDataType {
	CONDOR_TT_TYPE_CLOB,
	CONDOR_TT_TYPE_STRING,
	CONDOR_TT_TYPE_NUMBER,
	CONDOR_TT_TYPE_TIMESTAMP,
	CONDOR_TT_TYPE_BOOL,
	CONDOR_TT_TYPE_UNKNOWN
};
#endif







