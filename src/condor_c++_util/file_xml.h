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

#ifndef FILEXML_H
#define FILEXML_H

#include "file_sql.h"

/*
 * Config params used by XML Logger:
 * 
 * WANT_XML_LOG: Switch to turn off XML Logging
 * MAX_XML_LOG: Max size of XML Log (in bytes)
 * <SUBSYS>_XMLLOG: Filename of XML Log used by <SUBSYS>
 *
 */

class FILEXML : public FILESQL
{

/*
private:
	bool 	is_open;
	bool 	is_locked;
	char *outfilename;
	int fileflags;
	int outfiledes;
	FileLock *lock;
	FILE *fp;
*/
public:
	
	FILEXML(bool use_xml_logfile = false) : FILESQL(use_xml_logfile) { }
	FILEXML(const char *outputfilename,int flags=O_WRONLY|O_CREAT|O_APPEND, bool use_xml_logfile = false) : FILESQL(outfilename, flags, use_xml_logfile) { }
	virtual ~FILEXML() {}

/*
	bool file_isopen();
	bool file_islocked();
	QuillErrCode file_open();
	QuillErrCode file_close();
	QuillErrCode file_lock();
	QuillErrCode file_unlock();
*/
	QuillErrCode file_newEvent(const char *eventType, AttrList *info);
	QuillErrCode file_updateEvent(const char *eventType, AttrList *info, 
								  AttrList *condition);
//	QuillErrCode file_deleteEvent(const char *eventType, AttrList *condition);

//	int  file_readline(MyString *buf);
	AttrList  *file_readAttrList();
//	QuillErrCode  file_truncate();

	static FILEXML *createInstanceXML();
};


#endif
