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

#ifndef __SQL_QUERY_H__
#define __SQL_QUERY_H__

#define MAX_QUERY_SIZE 4096

//forward definition
class JobQueueDatabase;

enum query_types {
  HISTORY_ALL_HOR,
  HISTORY_ALL_VER,
  HISTORY_CLUSTER_HOR,
  HISTORY_CLUSTER_VER,
  HISTORY_CLUSTER_PROC_HOR, 
  HISTORY_CLUSTER_PROC_VER,
  HISTORY_OWNER_HOR,
  HISTORY_OWNER_VER,
  HISTORY_COMPLETEDSINCE_HOR,
  HISTORY_COMPLETEDSINCE_VER,
  QUEUE_AVG_TIME
};

class SQLQuery
{
  public:
	// ctor/dtor
	SQLQuery ();
	SQLQuery (query_types qtype, void **parameters);
	~SQLQuery ();

	//getters and setters 
	char * getQuery();
	char * getDeclareCursorStmt();
	char * getFetchCursorStmt();
	char * getCloseCursorStmt();
	void setQuery(query_types qtype, void **parameters);
	void setScheddname(char *name);
	void setJobqueuebirthdate(time_t birthdate);
	query_types getType();

	void prepareQuery(); // Prepares the query string(s)

	//print
	void Print();

	//print query results
	//int printResults(JobQueueDatabase *jqdb);

  private:
	char *query_str;

    //applicable only for cursor queries
	char *declare_cursor_str; 
	char *fetch_cursor_str; 
	char *close_cursor_str; 
	query_types type;
	void **query_predicates;
	char *scheddname;
	time_t jobqueuebirthdate;

	int createQueryString(query_types qtype, void **parameters);
};

#endif
