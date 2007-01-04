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
#include "condor_debug.h"
#include "sqlquery.h"

#define avg_time_template "SELECT avg((now() - 'epoch'::timestamp with time zone) - cast(QDate || ' seconds' as interval)) \
         FROM \
           (SELECT \
             (CASE WHEN p.QDate ISNULL THEN c.QDate ELSE p.QDate END) AS QDate, \
             (CASE WHEN p.JobStatus ISNULL THEN c.JobStatus ELSE p.JobStatus END) AS JobStatus \
            FROM (select cid, pid, \
          	        max(CASE attr WHEN 'QDate'  THEN val ELSE NULL END) AS QDate, \
              	    max(CASE attr WHEN 'JobStatus'  THEN val ELSE NULL END) AS JobStatus \
	              FROM procads GROUP BY cid, pid having cid != 0) AS p, \
                 (select cid, \
 	                max(CASE attr WHEN 'QDate'  THEN val ELSE NULL END) AS QDate, \
	                max(CASE attr WHEN 'JobStatus'  THEN val ELSE NULL END) AS JobStatus \
	              FROM clusterads GROUP BY cid having cid != 0) AS c \
            WHERE p.cid = c.cid) AS h \
         WHERE (jobStatus != '3'::text) and (jobstatus != '4'::text);"


SQLQuery::
SQLQuery ()
{
  query_str = 0;
  declare_cursor_str = 0;
  fetch_cursor_str = 0;
  close_cursor_str = 0;
}

SQLQuery::
SQLQuery (query_types qtype, void **parameters)
{
	createQueryString(qtype,parameters);
}


SQLQuery::
~SQLQuery ()
{
  if(query_str) 
	  free(query_str);
  if(declare_cursor_str) 
	  free(declare_cursor_str);
  if(fetch_cursor_str) 
	  free(fetch_cursor_str);
  if(close_cursor_str) 
	  free(close_cursor_str);
}

query_types SQLQuery::
getType() 
{
  return type;
}

char * SQLQuery::
getQuery() 
{
  return query_str;
}

char * SQLQuery::
getDeclareCursorStmt() 
{
  return declare_cursor_str;
}

char * SQLQuery::
getFetchCursorStmt() 
{
  return fetch_cursor_str;
}

char * SQLQuery::
getCloseCursorStmt() 
{
  return close_cursor_str;
}

void SQLQuery::
setQuery(query_types qtype, void **parameters) 
{
  createQueryString(qtype, parameters);
}

int SQLQuery::
createQueryString(query_types qtype, void **parameters) {  
  query_str = (char *) malloc(MAX_QUERY_SIZE * sizeof(char));
  declare_cursor_str = (char *) malloc(MAX_QUERY_SIZE * sizeof(char));
  fetch_cursor_str = (char *) malloc(MAX_QUERY_SIZE * sizeof(char));
  close_cursor_str = (char *) malloc(MAX_QUERY_SIZE * sizeof(char));

  switch(qtype) {
    
  case HISTORY_ALL_HOR:
	sprintf(declare_cursor_str, 
			"DECLARE HISTORY_ALL_HOR_CUR CURSOR FOR SELECT * FROM HISTORY_HORIZONTAL ORDER BY CID, PID;");
	sprintf(fetch_cursor_str,
			"FETCH FORWARD 100 FROM HISTORY_ALL_HOR_CUR");
	sprintf(close_cursor_str,
			"CLOSE HISTORY_ALL_HOR_CUR");
    break;
  case HISTORY_ALL_VER:
	sprintf(declare_cursor_str, 
			"DECLARE HISTORY_ALL_VER_CUR CURSOR FOR SELECT * FROM HISTORY_VERTICAL ORDER BY CID, PID;");
	sprintf(fetch_cursor_str,
			"FETCH FORWARD 5000 FROM HISTORY_ALL_VER_CUR");
	sprintf(close_cursor_str,
			"CLOSE HISTORY_ALL_VER_CUR");
    break;
  case HISTORY_CLUSTER_HOR:
	sprintf(declare_cursor_str, 
			"DECLARE HISTORY_CLUSTER_HOR_CUR CURSOR FOR SELECT * FROM HISTORY_HORIZONTAL WHERE cid=%d ORDER BY CID, PID;",
			*((int *)parameters[0]));
	sprintf(fetch_cursor_str,
			"FETCH FORWARD 100 FROM HISTORY_CLUSTER_HOR_CUR");
	sprintf(close_cursor_str,
			"CLOSE HISTORY_CLUSTER_HOR_CUR");
    break;
  case HISTORY_CLUSTER_VER:
	sprintf(declare_cursor_str, 
			"DECLARE HISTORY_CLUSTER_VER_CUR CURSOR FOR SELECT * FROM HISTORY_VERTICAL WHERE cid=%d ORDER BY CID, PID;",
			*((int *)parameters[0]));
	sprintf(fetch_cursor_str,
			"FETCH FORWARD 5000 FROM HISTORY_CLUSTER_VER_CUR");
	sprintf(close_cursor_str,
			"CLOSE HISTORY_CLUSTER_VER_CUR");
    break;
  case HISTORY_CLUSTER_PROC_HOR:
    sprintf(declare_cursor_str, 
			"DECLARE HISTORY_CLUSTER_PROC_HOR_CUR CURSOR FOR SELECT * FROM HISTORY_HORIZONTAL WHERE cid=%d and pid=%d ORDER BY CID, PID;",
			*((int *)parameters[0]), *((int *)parameters[1]));
	sprintf(fetch_cursor_str,
			"FETCH FORWARD 100 FROM HISTORY_CLUSTER_PROC_HOR_CUR");
	sprintf(close_cursor_str,
			"CLOSE HISTORY_CLUSTER_PROC_HOR_CUR");
    break;  
  case HISTORY_CLUSTER_PROC_VER:
    sprintf(declare_cursor_str, 
			"DECLARE HISTORY_CLUSTER_PROC_VER_CUR CURSOR FOR SELECT * FROM HISTORY_VERTICAL WHERE cid=%d and pid=%d "
			"ORDER BY CID, PID;",
			*((int *)parameters[0]), *((int *)parameters[1]));
	sprintf(fetch_cursor_str,
			"FETCH FORWARD 5000 FROM HISTORY_CLUSTER_PROC_VER_CUR");
	sprintf(close_cursor_str,
			"CLOSE HISTORY_CLUSTER_PROC_VER_CUR");
    break;  
  case HISTORY_OWNER_HOR:
	sprintf(declare_cursor_str,
			"DECLARE HISTORY_OWNER_HOR_CUR CURSOR FOR SELECT * FROM HISTORY_HORIZONTAL WHERE \"Owner\"='\"%s\"' "
			"ORDER BY CID,PID;",
			((char *)parameters[0]));
	sprintf(fetch_cursor_str,
			"FETCH FORWARD 100 FROM HISTORY_OWNER_HOR_CUR");
	sprintf(close_cursor_str,
			"CLOSE HISTORY_OWNER_HOR_CUR");
	  break;
  case HISTORY_OWNER_VER:
	sprintf(declare_cursor_str,
			"DECLARE HISTORY_OWNER_VER_CUR CURSOR FOR SELECT hv.cid,hv.pid,hv.attr,hv.val FROM "
			"HISTORY_HORIZONTAL hh, HISTORY_VERTICAL hv "
			"WHERE hh.cid=hv.cid AND hh.pid=hv.pid AND hh.\"Owner\"='\"%s\"'"
			" ORDER BY CID,PID;",
			((char *)parameters[0]));
	sprintf(fetch_cursor_str,
			"FETCH FORWARD 5000 FROM HISTORY_OWNER_VER_CUR");
	sprintf(close_cursor_str,
			"CLOSE HISTORY_OWNER_VER_CUR");
	break;
  case HISTORY_COMPLETEDSINCE_HOR:
	sprintf(declare_cursor_str,
			"DECLARE HISTORY_COMPLETEDSINCE_HOR_CUR CURSOR FOR SELECT * FROM History_Horizontal "
			"WHERE \"CompletionDate\" > "
			"date_part('epoch', '%s'::timestamp with time zone) "
			"ORDER BY cid,pid;",
			((char *)parameters[0]));
	sprintf(fetch_cursor_str,
			"FETCH FORWARD 100 FROM HISTORY_COMPLETEDSINCE_HOR_CUR");
	sprintf(close_cursor_str,
			"CLOSE HISTORY_COMPLETEDSINCE_HOR_CUR");
    break;
  case HISTORY_COMPLETEDSINCE_VER:
	sprintf(declare_cursor_str,
			"SET enable_mergejoin=false; "
			"DECLARE HISTORY_COMPLETEDSINCE_VER_CUR CURSOR FOR SELECT hv.cid,hv.pid,hv.attr,hv.val FROM "
			"HISTORY_HORIZONTAL hh, HISTORY_VERTICAL hv "
			"WHERE hh.cid=hv.cid AND hh.pid=hv.pid AND "
			"hh.\"CompletionDate\" > "
			"date_part('epoch', '%s'::timestamp with time zone) "
			"ORDER BY hh.cid,hh.pid;",
			((char *)parameters[0]));
	sprintf(fetch_cursor_str,
			"FETCH FORWARD 5000 FROM HISTORY_COMPLETEDSINCE_VER_CUR");
	sprintf(close_cursor_str,
			"CLOSE HISTORY_COMPLETEDSINCE_VER_CUR");
    break;
  case QUEUE_AVG_TIME:
	sprintf(query_str, avg_time_template);
	break;
	
  default:
	EXCEPT("Incorrect query type specified\n");
	return -1;
  }
  return 1;
}

void SQLQuery::
Print() 
{
  printf("Query = %s\n", query_str);
  printf("DeclareCursorStmt = %s\n", declare_cursor_str);
  printf("FetchCursorStmt = %s\n", fetch_cursor_str);
  printf("CloseCursorStmt = %s\n", close_cursor_str);
}

