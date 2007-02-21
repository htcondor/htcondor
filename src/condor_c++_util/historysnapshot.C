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
#include "condor_attributes.h"

#include "pgsqldatabase.h"
#include "historysnapshot.h"

//! constructor
HistorySnapshot::HistorySnapshot(const char* dbcon_str)
{
	jqDB = new PGSQLDatabase(dbcon_str);
	curAd = NULL;
	curClusterId_hor = curProcId_hor = curClusterId_ver = curProcId_ver = -1;
	isHistoryEmptyFlag = false;
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
						   bool longformat,
						   bool fileformat,
						   bool custForm,
						   AttrListPrintMask *pmask)
{
  QuillErrCode st;
  
  st = jqDB->connectDB();
  
  if(st == FAILURE) {
    return FAILURE;
  }

  if(jqDB->beginTransaction() == FAILURE) {
	  printf("Error while querying the database: unable to start new transaction");
	  return FAILURE;
  }

  st = jqDB->openCursorsHistory(queryhor, 
								queryver, 
								longformat);
  
  if (st != SUCCESS) {
	  printf("Error while opening the history cursors: %s\n", jqDB->getDBError());
	  return FAILURE;
  }
  
  st = printResults(queryhor, 
					queryver, 
					longformat,
					fileformat,
					custForm,
					pmask);
  
  if (st != SUCCESS) {
	  printf("Error while querying the history cursors: %s\n", jqDB->getDBError());
	  return FAILURE;
  }
  
  st = jqDB->closeCursorsHistory(queryhor,
								 queryver,
								 longformat);
  
  if (st != SUCCESS) {
    printf("Error while closing the history cursors: %s\n", jqDB->getDBError());
    return FAILURE;
  }

  if(jqDB->commitTransaction() == FAILURE) {
	  printf("Error while querying the database: unable to commit transaction");
	  return FAILURE;
  }

  return SUCCESS;
}

QuillErrCode
HistorySnapshot::printResults(SQLQuery *queryhor, 
							  SQLQuery *queryver, 
							  bool longformat, bool fileformat,
							  bool custForm, AttrListPrintMask *pmask) {
  AttrList *ad = 0;
  QuillErrCode st = SUCCESS;

  // initialize index variables
   
  off_t offset = 0, last_line = 0;

  cur_historyads_hor_index = 0;  	
  cur_historyads_ver_index = 0;

  if (!longformat && !custForm) {
	  short_header();
  }

  while(1) {
	  st = getNextAd_Hor(ad, queryhor);

	  if(st != SUCCESS) 
		  break;
	  
	  if (longformat) { 
		  st = getNextAd_Ver(ad, queryver);

		  // in the case of vertical, we dont want to quit if we run
		  // out of tuples because 1) we want to display whats in the ad
		  // and 2) the horizontal cursor will correctly determine when
		  // to stop - this is because in all cases, we only pull out those
          // tuples from vertical which join with a horizontal tuple
		  if(st != SUCCESS && st != DONE_HISTORY_VER_CURSOR) 
			  break;
		  if (fileformat) { // Print out the job ads in history file format, i.e., print the *** delimiters
			  MyString owner, ad_str, temp;
			  int compl_date;

			  ad->sPrint(ad_str);
                       if (!ad->LookupString(ATTR_OWNER, owner))
                                 owner = "NULL";
                         if (!ad->LookupInteger(ATTR_COMPLETION_DATE, compl_date))
                                 compl_date = 0;
                         temp.sprintf("*** Offset = %ld ClusterId = %d ProcId = %d Owner = \"%s\" CompletionDate = %d\n",
                                        offset - last_line, curClusterId_hor, curProcId_hor, owner.GetCStr(), compl_date);

                         offset += ad_str.Length() + temp.Length();
                         last_line = temp.Length();
                         fprintf(stdout, "%s", ad_str.GetCStr());
                         fprintf(stdout, "%s", temp.GetCStr());
                 }     else {
                         ad->fPrint(stdout);
                         printf("\n");
                 }

	  } 
	  else {
	  	if (custForm == true) {
			ASSERT(pmask != NULL);
			pmask->display(stdout, ad);
		} else {
			displayJobShort(ad);
		}
	  }
  }

  if(ad != NULL) {
	  delete ad;
	  ad = NULL;
  }

  if(st == FAILURE_QUERY_HISTORYADS_HOR || st == FAILURE_QUERY_HISTORYADS_VER)
	  return st;

  return SUCCESS;
}

QuillErrCode
HistorySnapshot::getNextAd_Hor(AttrList*& ad, SQLQuery *queryhor)
{
	QuillErrCode st;
	char *cid, *pid, *attr, *val;
	char *expr;
	int i;
	
	st = jqDB->getHistoryHorValue(queryhor, 
								  cur_historyads_hor_index, 0, &cid); // cid

		
		// we only check the return value, st,  for the first call to 
		// getHistoryHorValue, in this case, for getting cid out. 
		// subsequent calls would be accessing cached values anyway
	
		// the HISTORY_EMPTY case is special for getNextAd_Hor
	if(st == DONE_HISTORY_HOR_CURSOR && cur_historyads_hor_index == 0)
		isHistoryEmptyFlag = true;

	if(st == DONE_HISTORY_HOR_CURSOR || st == FAILURE_QUERY_HISTORYADS_HOR) {
		return st;
	}

	jqDB->getHistoryHorValue(queryhor, 
							 cur_historyads_hor_index, 1, &pid); // pid

	if (ad != NULL) {
		delete ad;
		ad = NULL;
	}
	ad = new AttrList();

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
	for(i=2; i < numfields; i++) {
	  attr = (char *) jqDB->getHistoryHorFieldName(i); // attr
	  jqDB->getHistoryHorValue(queryhor, cur_historyads_hor_index, i, &val); // val
	  
	  expr = (char*)malloc(strlen(attr) + strlen(val) + 4);
	  sprintf(expr, "%s = %s", attr, val);
	  // add an attribute with a value into ClassAd
	  ad->Insert(expr);
	  free(expr);
	}

		// increment water
	cur_historyads_hor_index++;

	return SUCCESS;
}


//! iterate one by one
/*
*/
QuillErrCode
HistorySnapshot::getNextAd_Ver(AttrList*& ad, SQLQuery *queryver)
{
	char	*cid, *pid, *attr, *val;
	QuillErrCode st = SUCCESS;

	st = jqDB->getHistoryVerValue(queryver, 
								  cur_historyads_ver_index, 0, &cid); // cid

		// we only check the return value for the first call, 
		// in this case for getting cid
	if(st == DONE_HISTORY_VER_CURSOR || st == FAILURE_QUERY_HISTORYADS_VER) {
		return st;
	}

	jqDB->getHistoryVerValue(queryver, 
							 cur_historyads_ver_index, 1, &pid); // pid

	curClusterId_ver = atoi((char *)cid);
	curProcId_ver = atoi((char *) pid);

	// for HistoryAds table
	while(1) {
		st = jqDB->getHistoryVerValue(queryver, 
									  cur_historyads_ver_index, 0, &cid);// cid
		
			// we only check the return value for the first call, 
			// in this case for getting cid
		if(st == DONE_HISTORY_VER_CURSOR  || st == FAILURE_QUERY_HISTORYADS_VER)
			break;

		st = jqDB->getHistoryVerValue(queryver, 
									  cur_historyads_ver_index, 1, &pid); // pid

		if (cid == NULL  
		   || curClusterId_ver != atoi(cid) 
		   || curProcId_ver != atoi(pid)) {
			break;
		}

		jqDB->getHistoryVerValue(queryver, 
								 cur_historyads_ver_index, 2, &attr); // attr
		jqDB->getHistoryVerValue(queryver, 
								 cur_historyads_ver_index, 3, &val); // val

		cur_historyads_ver_index++;

			// add the attribute,value pair to the classad
		char* expr = (char*)malloc(strlen(attr) + strlen(val) + 4);
		sprintf(expr, "%s = %s", attr, val);
		ad->Insert(expr);
		free(expr);
	};
	
	return st;
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

