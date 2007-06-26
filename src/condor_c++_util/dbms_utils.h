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
#ifndef _DBMS_UTILS_H
#define _DBMS_UTILS_H

#include "quill_enums.h"
#include "MyString.h"
#include "jobqueuedatabase.h"
#include "condor_attrlist.h"

#if defined( __cplusplus )
extern "C" {
#endif

		// get the database type specified in the config file
	dbtype getConfigDBType();
	char *getDBConnStr(const char* jobQueueDBIpAddress,
					   const char* jobQueueDBName,
					   const char* jobQueueDBUser,
					   const char* spool
					   );
	bool stripdoublequotes(char *attVal);
	bool stripdoublequotes_MyString(MyString &value);
	bool isHorizontalHistoryAttribute(const char *attName, 
									  QuillAttrDataType &attr_type);
	bool isHorizontalClusterAttribute(const char *attName, 
									  QuillAttrDataType &attr_type);
	bool isHorizontalProcAttribute(const char *attName,
								   QuillAttrDataType &attr_type);
	bool isHorizontalMachineAttr(char *attName, 
								 QuillAttrDataType &attr_type);
	bool isHorizontalDaemonAttr(char *attName, 
								QuillAttrDataType &attr_type);

		// insert a job into the history tables
	QuillErrCode insertHistoryJobCommon(AttrList *ad, JobQueueDatabase* DBObj, dbtype dt, MyString & errorSqlStmt, 
										const char*scheddname, const time_t scheddbirthdate);
#if defined( __cplusplus )
}
#endif

#endif /* _DBMS_UTILS_H */
