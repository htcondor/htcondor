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

#include "condor_common.h"
#include "condor_attributes.h"

#include "pgsqldatabase.h"
#include "historysnapshot.h"

//! constructor
HistorySnapshot::HistorySnapshot(const char* dbcon_str)
{
	jqDB = new PGSQLDatabase(dbcon_str);
	curAd = NULL;
	curClusterId_hor = curProcId_hor = curClusterId_ver = curProcId_ver = -1;
}

//! destructor
HistorySnapshot::~HistorySnapshot()
{
	if (jqDB != NULL) {
		delete(jqDB);
	}
}

//! query the history tables
QuillErrCode
HistorySnapshot::sendQuery(SQLQuery *queryhor, 
						   SQLQuery *queryver, 
						   bool longformat)
{
  QuillErrCode st;
  
  historyads_hor_num = 0;
  historyads_ver_num = 0;

  st = jqDB->connectDB();
  
  if(st == FAILURE) {
    return FAILURE;
  }

  st = jqDB->queryHistoryDB(queryhor, 
							queryver, 
							longformat, 
							historyads_hor_num, 
							historyads_ver_num); 
  if (st == HISTORY_EMPTY) {// There is no live jobs
    return HISTORY_EMPTY;
  }

  if (st == FAILURE_QUERY_HISTORYADS_HOR ||
	  st == FAILURE_QUERY_HISTORYADS_VER) { // Got some error 
    printf("Error while querying the database: %s\n", jqDB->getDBError());
    return FAILURE;
  }
  
  return(printResults(queryhor, queryver, longformat));
}

QuillErrCode
HistorySnapshot::printResults(SQLQuery *queryhor, 
							  SQLQuery *queryver, 
							  bool longformat) {
  AttrList *ad = 0;

  // initialize index variables
  cur_historyads_hor_index = 0;  	
  cur_historyads_ver_index = 0;

  if (!longformat) {
	  short_header();
  }

  while(getNextAd_Hor(ad) != DONE_HISTORY_CURSOR) {    
	  if (longformat) { 
		  getNextAd_Ver(ad);
		  ad->fPrint(stdout); printf("\n"); 
	  } 
	  else {
		  displayJobShort(ad);
	  }
  }
  if(ad != NULL) {
	  delete ad;
  }
  return SUCCESS;
}

QuillErrCode
HistorySnapshot::getNextAd_Hor(AttrList*& ad)
{
	const char	*cid, *pid, *attr, *val;
	char *expr;

	if(cur_historyads_hor_index >= historyads_hor_num) {
	  return DONE_HISTORY_CURSOR;
	}
	if (ad != NULL) {
		delete ad;
	}
	ad = new AttrList();

	cid = jqDB->getHistoryHorValue(cur_historyads_hor_index, 0); // cid
	pid = jqDB->getHistoryHorValue(cur_historyads_hor_index, 1); // pid

	curClusterId_hor = atoi((char *)cid);
	curProcId_hor = atoi((char *) pid);

	expr = (char*)malloc(strlen(ATTR_CLUSTER_ID) + strlen(cid) + 4);
	sprintf(expr, "%s = %s", ATTR_CLUSTER_ID, cid);
	ad->Insert(expr);
	free(expr);

	expr = (char*)malloc(strlen(ATTR_PROC_ID) + strlen(pid) + 4);
	sprintf(expr, "%s = %s", ATTR_PROC_ID, pid);
	ad->Insert(expr);
	free(expr);

	//
	// build the next ad
	//

	int numfields = jqDB->getHistoryHorNumFields();

		//starting from 2 as 0 and 1 are cid and pid respectively
	for(int i=2; i < numfields; i++) {
	  attr = jqDB->getHistoryHorFieldName(i); // attr
	  val = jqDB->getHistoryHorValue(cur_historyads_hor_index, i); // val
	  
	  expr = (char*)malloc(strlen(attr) + strlen(val) + 4);
	  sprintf(expr, "%s = %s", attr, val);
	  // add an attribute with a value into ClassAd
	  ad->Insert(expr);
	  free(expr);
	}

	cur_historyads_hor_index++;

	return SUCCESS;
}


//! iterate one by one
/*
*/
QuillErrCode
HistorySnapshot::getNextAd_Ver(AttrList*& ad)
{
	const char	*cid, *pid, *attr, *val;

	if(cur_historyads_ver_index >= historyads_ver_num) {
	   return DONE_HISTORY_CURSOR;
	}

	cid = jqDB->getHistoryVerValue(cur_historyads_ver_index, 0); // cid
	pid = jqDB->getHistoryVerValue(cur_historyads_ver_index, 1); // cid

	curClusterId_ver = atoi((char *)cid);
	curProcId_ver = atoi((char *) pid);

	// for HistoryAds table
	while(cur_historyads_ver_index < historyads_ver_num) {
		cid = jqDB->getHistoryVerValue(cur_historyads_ver_index, 0); // cid
		pid = jqDB->getHistoryVerValue(cur_historyads_ver_index, 1); // pid

		if (cid == NULL  
		   || curClusterId_ver != atoi(cid) 
		   || curProcId_ver != atoi(pid)) {
			break;
		}

		attr = jqDB->getHistoryVerValue(cur_historyads_ver_index, 2); // attr
		val = jqDB->getHistoryVerValue(cur_historyads_ver_index, 3); // val

		cur_historyads_ver_index++;

		char* expr = (char*)malloc(strlen(attr) + strlen(val) + 4);
		sprintf(expr, "%s = %s", attr, val);
		// add an attribute with a value into ClassAd
		ad->Insert(expr);
		free(expr);
	};

	return SUCCESS;
}

//! release snapshot
QuillErrCode
HistorySnapshot::release()
{
	QuillErrCode st1, st2;
	st1 = jqDB->releaseHistoryResults();
	st2 = jqDB->disconnectDB();

	if(st1 == SUCCESS && st2 == SUCCESS) {
		return SUCCESS;
	}
	else {
		return FAILURE;
	}
}

