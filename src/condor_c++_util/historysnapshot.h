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
		AttrListPrintMask *pmask = NULL);

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
							  AttrListPrintMask *pmask = NULL);
};

#endif
