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
#ifndef __SQL_QUERY_H__
#define __SQL_QUERY_H__

#define MAX_QUERY_SIZE 1024

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
	void setQuery(query_types qtype, void **parameters);
	query_types getType();

	//print
	void Print();

	//print query results
	//int printResults(JobQueueDatabase *jqdb);

  private:
	char *query_str;
	query_types type;

	int createQueryString(query_types qtype, void **parameters);
};

#endif







