#ifndef FILESQL_H
#define FILESQL_H

class FileLock;
class AttrList;
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
