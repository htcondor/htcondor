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


#ifndef _HISTORYSNAPSHOT_H_
#define _HISTORYSNAPSHOT_H_

#include "condor_common.h"
#include "jobqueuedatabase.h"
#include "sqlquery.h"
#include "classad_collection.h"
#include "history_utils.h"
#include "quill_enums.h"
#include "ad_printmask.h"

//! HistorySnapshot
/*! it provides interface to iterate the query result 
 *  from the history tables when it services condor_history
 */
class HistorySnapshot {
public:
	//! constructor
	HistorySnapshot(const char* dbcon_str);
	//! destructor
	~HistorySnapshot();

	//! high level method
	QuillErrCode sendQuery(SQLQuery *, SQLQuery *, bool, 
		bool fileformat = false, bool custForm = false, 
		AttrListPrintMask *pmask = NULL, const char *constraint = "");

	//! used to determine whether result set is empty
	bool isHistoryEmpty() { return isHistoryEmptyFlag; }

	//! release snapshot
	QuillErrCode release();
	
private:

	bool isHistoryEmptyFlag;

		// pointer into current row of the result set - used to 
		// support the getNextAd_* methods below
	int cur_historyads_hor_index;
	int cur_historyads_ver_index;

	int	curClusterId_hor;  //!< current Cluster Id
	int	curProcId_hor;	   //!< current Proc Id
	int	curClusterId_ver;  //!< current Cluster Id
	int	curProcId_ver;	   //!< current Proc Id

	ClassAd		*curAd;	//!< current Job Ad
	JobQueueDatabase	*jqDB;	//!< Database object
	dbtype	dt;	/* database type */

	QuillErrCode getNextAd_Ver(AttrList *&ad, SQLQuery *queryhor);
	QuillErrCode getNextAd_Hor(AttrList *&ad, SQLQuery *queryver);
	QuillErrCode printResults(SQLQuery *queryhor, 
							  SQLQuery *queryver, 
							  bool longformat, bool fileformat = false,
							  bool custForm = false, 
							  AttrListPrintMask *pmask = NULL,
							  const char *constraint = "");
};

#endif
