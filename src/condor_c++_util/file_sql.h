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

#ifndef FILESQL_H
#define FILESQL_H

#include "condor_common.h"
#include "condor_attrlist.h"

class FileLock;
class MyString;

#include <stdio.h> // for FILE *

#include "quill_enums.h"

class FILESQL 
{
protected:

    bool    is_dummy;
	bool 	is_open;
	bool 	is_locked;
	char *outfilename;
	int fileflags;
	int outfiledes;
	FileLock *lock;
	FILE *fp;
public:
	
	FILESQL(bool use_sql_logfile = false);
	FILESQL(const char *outfilename,int flags=O_WRONLY|O_CREAT|O_APPEND, bool use_sql_log = false);
	virtual ~FILESQL();
	bool file_isopen();
	bool file_islocked();
	QuillErrCode file_open();
	QuillErrCode file_close();
	QuillErrCode file_lock();
	QuillErrCode file_unlock();
	QuillErrCode file_newEvent(const char *eventType, AttrList *info);
	QuillErrCode file_updateEvent(const char *eventType, AttrList *info, 
								  AttrList *condition);
	QuillErrCode file_deleteEvent(const char *eventType, AttrList *condition);
	int  file_readline(MyString *buf);
	AttrList  *file_readAttrList();
	QuillErrCode  file_truncate();

	static FILESQL *createInstance(bool use_sql_log);

	static void daemonAdInsert(ClassAd *cl, const char *adType,
					FILESQL *dbh, int &prevLHF);

};

#endif
