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
