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


#include "condor_common.h"
#include "condor_attributes.h"
#include "condor_config.h"
#include "pgsqldatabase.h"
#include "historysnapshot.h"
#include "quill_enums.h"
#include "condor_classad.h"

#ifdef HAVE_EXT_POSTGRESQL

//! constructor
HistorySnapshot::HistorySnapshot(const char* dbcon_str)
{
	char *tmp;

	dt = T_PGSQL; // assume PGSQL by default
	tmp = param("QUILL_DB_TYPE");
	if (tmp) {
		if (strcasecmp(tmp, "PGSQL") == 0) {
			dt = T_PGSQL;
		}
		free(tmp);
	} 

	switch (dt) {				
	case T_PGSQL:
		jqDB = new PGSQLDatabase(dbcon_str);
		break;
	default:
		jqDB = NULL;
		break;;
	}

	curAd = NULL;
	curClusterId_hor = curProcId_hor = curClusterId_ver = curProcId_ver = -1;
	isHistoryEmptyFlag = false;

	cur_historyads_hor_index = -1;
	cur_historyads_ver_index = -1;
	dt = T_PGSQL;
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
						   AttrListPrintMask *pmask, const char *constraint)
{
  QuillErrCode st;
  
  st = jqDB->connectDB();
  
  if(st == QUILL_FAILURE) {
    return QUILL_FAILURE;
  }

  if(jqDB->beginTransaction() == QUILL_FAILURE) {
	  printf("Error while querying the database: unable to start new transaction");
	  return QUILL_FAILURE;
  }

	  /* to ensure read consistency with while maximizing concurrency, 
		 use read only transaction if a database supports it.
	  */
	  /* INSERT CODE TO SET TRANSACTION READ ONLY: HERE */
  
  st = jqDB->openCursorsHistory(queryhor, 
								queryver, 
								constraint ? true : longformat);
  
  if (st == HISTORY_EMPTY) {
	  isHistoryEmptyFlag = true;
	  return HISTORY_EMPTY;
  }

  if (st == FAILURE_QUERY_HISTORYADS_HOR ||
	  st == FAILURE_QUERY_HISTORYADS_VER) {
	  printf("Error while opening the history cursors: %s\n", jqDB->getDBError());
	  return QUILL_FAILURE;
  }
  
  st = printResults(queryhor, 
					queryver, 
					longformat,
					fileformat,
					custForm,
					pmask,
					constraint);
  
  if (st != QUILL_SUCCESS) {
	  printf("Error while querying the history cursors: %s\n", jqDB->getDBError());
	  return QUILL_FAILURE;
  }
  
  st = jqDB->closeCursorsHistory(queryhor,
								 queryver,
								 longformat);
  
  if (st != QUILL_SUCCESS) {
    printf("Error while closing the history cursors: %s\n", jqDB->getDBError());
    return QUILL_FAILURE;
  }

  if(jqDB->commitTransaction() == QUILL_FAILURE) {
	  printf("Error while querying the database: unable to commit transaction");
	  return QUILL_FAILURE;
  }

  return QUILL_SUCCESS;
}

QuillErrCode
HistorySnapshot::printResults(SQLQuery *queryhor, 
							  SQLQuery *queryver, 
							  bool longformat, bool fileformat,
							  bool custForm, 
							AttrListPrintMask *pmask,
							const char *constraint /* = "" */) {
  AttrList *ad = 0;
  QuillErrCode st = QUILL_SUCCESS;

  // initialize index variables
   
  off_t offset = 0, last_line = 0;

  cur_historyads_hor_index = 0;  	
  cur_historyads_ver_index = 0;

  if (!longformat && !custForm) {
	  short_header();
  }

	ExprTree *tree = NULL;

	if (constraint) {
		 ParseClassAdRvalExpr(constraint, tree);
	}

  while(1) {
	  st = getNextAd_Hor(ad, queryhor);

	  if(st != QUILL_SUCCESS) 
		  break;
	  
	  if (longformat || constraint) { 
		  st = getNextAd_Ver(ad, queryver);

		
		  if (constraint && EvalBool(ad, tree) == FALSE) {
			continue;
		  }
		  // in the case of vertical, we dont want to quit if we run
		  // out of tuples because 1) we want to display whats in the ad
		  // and 2) the horizontal cursor will correctly determine when
		  // to stop - this is because in all cases, we only pull out those
          // tuples from vertical which join with a horizontal tuple
		  if(st != QUILL_SUCCESS && st != DONE_HISTORY_VER_CURSOR) 
			  break;
		  if (fileformat) { // Print out the job ads in history file format, i.e., print the *** delimiters
			  MyString owner, ad_str, temp;
			  int compl_date;

			  ad->sPrint(ad_str);
                       if (!ad->LookupString(ATTR_OWNER, owner))
                                 owner = "NULL";
                         if (!ad->LookupInteger(ATTR_COMPLETION_DATE, compl_date))
                                 compl_date = 0;
                         temp.formatstr("*** Offset = %ld ClusterId = %d ProcId = %d Owner = \"%s\" CompletionDate = %d\n",
                                        offset - last_line, curClusterId_hor, curProcId_hor, owner.Value(), compl_date);

                         offset += ad_str.Length() + temp.Length();
                         last_line = temp.Length();
                         fprintf(stdout, "%s", ad_str.Value());
                         fprintf(stdout, "%s", temp.Value());
                 }     else if (longformat) {
                         ad->fPrint(stdout);
                         printf("\n");
                 }

	  } 
	  if (!longformat) {
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

  return QUILL_SUCCESS;
}

QuillErrCode
HistorySnapshot::getNextAd_Hor(AttrList*& ad, SQLQuery *queryhor)
{
	QuillErrCode st;
	const char *cid, *pid, *attr, *val;	
	char *expr;
	int i;
	
	st = jqDB->getHistoryHorValue(queryhor, 
								  cur_historyads_hor_index, 1, &cid); // cid

		
		// we only check the return value, st,  for the first call to 
		// getHistoryHorValue, in this case, for getting cid out. 
		// subsequent calls would be accessing cached values anyway
	
		// the HISTORY_EMPTY case is special for getNextAd_Hor
	if(st == DONE_HISTORY_HOR_CURSOR && cur_historyads_hor_index == 0)
		isHistoryEmptyFlag = true;

	if(st == DONE_HISTORY_HOR_CURSOR || st == FAILURE_QUERY_HISTORYADS_HOR) {
		return st;
	}

	if (ad != NULL) {
		delete ad;
		ad = NULL;
	}
	ad = new AttrList();

	curClusterId_hor = atoi(cid);
	expr = (char*)malloc(strlen(ATTR_CLUSTER_ID) + strlen(cid) + 4);
	sprintf(expr, "%s = %s", ATTR_CLUSTER_ID, cid);
	ad->Insert(expr);
	free(expr);

	jqDB->getHistoryHorValue(queryhor, 
							 cur_historyads_hor_index, 2, &pid); // pid

	curProcId_hor = atoi(pid);

	expr = (char*)malloc(strlen(ATTR_PROC_ID) + strlen(pid) + 4);
	sprintf(expr, "%s = %s", ATTR_PROC_ID, pid);
	ad->Insert(expr);
	free(expr);

	//
	// build the next ad
	//

	int numfields = jqDB->getHistoryHorNumFields();

		// starting from 3 as 0, 1, and 2 are scheddname, cid and pid 
		// respectively
	for(i=3; i < numfields; i++) {
	  attr = (char *) jqDB->getHistoryHorFieldName(i); // attr
	  jqDB->getHistoryHorValue(queryhor, cur_historyads_hor_index, i, &val); // val
	  
	  expr = (char*)malloc(strlen(attr) + strlen(val) + 4);
	  sprintf(expr, "%s = %s", attr, val);
	  // add an attribute with a value into ClassAd
	  // printf("Inserting %s as an expr\n", expr);
	  ad->Insert(expr);
	  free(expr);
	}

		// increment water
	cur_historyads_hor_index++;

	return QUILL_SUCCESS;
}


//! iterate one by one
/*
*/
QuillErrCode
HistorySnapshot::getNextAd_Ver(AttrList*& ad, SQLQuery *queryver)
{
	const char	*cid, *pid, *val, *temp;
	char *attr;
	QuillErrCode st = QUILL_SUCCESS;

	st = jqDB->getHistoryVerValue(queryver, 
								  cur_historyads_ver_index, 1, &cid); // cid

		// we only check the return value for the first call, 
		// in this case for getting cid
	if(st == DONE_HISTORY_VER_CURSOR || st == FAILURE_QUERY_HISTORYADS_VER) {
		return st;
	}

	curClusterId_ver = atoi(cid);
	
	jqDB->getHistoryVerValue(queryver, 
							 cur_historyads_ver_index, 2, &pid); // pid

	curProcId_ver = atoi(pid);

	// for HistoryAds table
	while(1) {
		st = jqDB->getHistoryVerValue(queryver, 
									  cur_historyads_ver_index, 1, &cid);// cid
		
			// we only check the return value for the first call, 
			// in this case for getting cid
		if(st == DONE_HISTORY_VER_CURSOR  || st == FAILURE_QUERY_HISTORYADS_VER)
			break;

		if (cid == NULL  
			|| curClusterId_ver != atoi(cid)) {
			break;
		}    

		st = jqDB->getHistoryVerValue(queryver, 
									  cur_historyads_ver_index, 2, &pid); // pid
		
		if (pid == NULL || 
			curProcId_ver != atoi(pid)) {
			break;
		}

		jqDB->getHistoryVerValue(queryver, 
								 cur_historyads_ver_index, 3, &temp); // attr

		if (temp != NULL) {
			attr = strdup(temp);
		} else {
			attr = NULL;
		}

		jqDB->getHistoryVerValue(queryver, 
								 cur_historyads_ver_index, 4, &val); // val

		cur_historyads_ver_index++;

		if ((attr != NULL) && (val != NULL)) {
				// add the attribute,value pair to the classad
			char* expr = (char*)malloc(strlen(attr) + strlen(val) + 4);
			sprintf(expr, "%s = %s", attr, val);
			ad->Insert(expr);
			free(expr);
		}

		if (attr != NULL) {
			free (attr);
			attr = NULL;
		}
	}
	
	return st;
}

//! release snapshot
QuillErrCode
HistorySnapshot::release()
{
	QuillErrCode st1, st2;
	st1 = jqDB->releaseHistoryResults();
	st2 = jqDB->disconnectDB();

	if(st1 == QUILL_SUCCESS && st2 == QUILL_SUCCESS) {
		return QUILL_SUCCESS;
	}
	else {
		return QUILL_FAILURE;
	}
}

#endif
