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
#include "condor_api.h"
#include "condor_config.h"
#include <errno.h>
#include <string.h>
#include "file_sql.h"
#include "truncate.h"
#include "subsystem_info.h"
#include "file_lock.h"

#include <sys/stat.h>

#define FILESIZELIMT 1900000000L

FILESQL::FILESQL(bool use_sql_log)
{
	if(use_sql_log == false) {
    	is_dummy = true;
	} else {
		is_dummy = false;
	}
	is_open = false;
	is_locked = false;
	outfilename = (char *) 0;
	fileflags = O_WRONLY|O_CREAT|O_APPEND;
	outfiledes = -1;
	fp = NULL;
	lock = (FileLock *)0;
}

FILESQL::FILESQL(const char *outputfilename, int flags, bool use_sql_log)
{
	if(use_sql_log == false) {
    	is_dummy = true;
	} else {
		is_dummy = false;
	}
	is_open = false;
	is_locked = false;
	this->outfilename = strdup(outputfilename);
	fileflags = flags;
	outfiledes = -1;
	fp = NULL;
	lock = (FileLock *)0;
}

FILESQL::~FILESQL()
{
	if(file_isopen()) {
		file_close();
	}
	is_open = false;
	is_locked = false;
	if (outfilename) free(outfilename);
	outfiledes = -1;
	fp = NULL;
}

bool FILESQL::file_isopen()
{
	return is_open;
}

bool FILESQL::file_islocked()
{
	return is_locked;
}

QuillErrCode FILESQL::file_truncate() {
	int retval;

    if (is_dummy) return QUILL_SUCCESS;

	if(!file_isopen()) {
		dprintf(D_ALWAYS, "Error calling truncate: the file needs to be first opened\n");
		return QUILL_FAILURE;
	}

	retval = ftruncate(outfiledes, 0);

	if(retval < 0) {
		dprintf(D_ALWAYS, "Error calling ftruncate, errno = %d\n", errno);
		return QUILL_FAILURE;
	}

	return QUILL_SUCCESS;
}

QuillErrCode FILESQL::file_open()
{
    if (is_dummy) return QUILL_SUCCESS;
    
	if (!outfilename) {
		dprintf(D_ALWAYS,"No SQL log file specified\n");
		return QUILL_FAILURE;
	}

	outfiledes = safe_open_wrapper_follow(outfilename,fileflags,0644);

	if(outfiledes < 0)
	{
		dprintf(D_ALWAYS,"Error opening SQL log file %s : %s\n",outfilename,strerror(errno));
		is_open = false;
		return QUILL_FAILURE;
	}
	else
	{
		is_open = true;
		
		/* Create a lock object when opening the file.
		 * Note: We very purposefully pass the "outfilename" to the
		 * FileLock ctor here; doing so enables the FileLock object
		 * to use kernel mutexes instead of on-disk file locks.  Not
		 * only is this more efficient, but on Windows it is required
		 * because later we will try to read from the locked file.  If
		 * it is "really" locked -vs- using Condor's advisory locking
		 * via the kernel mutexes, these reads will fail on Win32.
		 */
		lock = new FileLock(outfiledes,NULL,outfilename); 
		return QUILL_SUCCESS;
	}
}

QuillErrCode FILESQL::file_close()
{
	int retval;
	
    if (is_dummy) return QUILL_SUCCESS;
	
    if (!is_open)
		return QUILL_FAILURE;

	if (lock) {
			/* This also releases the lock on the file, were it held */
		delete lock; 
		lock  = (FileLock *)0;
	}

		/* fp is associated with the outfiledes, we should be closing one or 
		 * the other, but not both. Otherwise the second one will give an 
		 * error.
		 */
	if (fp) {
		retval = fclose(fp);
		fp = NULL;
	}
	else {
		retval = close(outfiledes);
		if(retval < 0)
			dprintf(D_ALWAYS, "Error closing SQL log file %s : %s\n",outfilename,strerror(errno));
	}

	is_open = false;
	is_locked = false;
	outfiledes = -1;

	if (retval < 0) {
		return QUILL_FAILURE;
	}
	else {
		return QUILL_SUCCESS;
	}
}

QuillErrCode FILESQL::file_lock()
{
    if (is_dummy) return QUILL_SUCCESS;
	
	if(!is_open)
	{
		dprintf(D_ALWAYS,"Error locking :SQL log file %s not open yet\n",outfilename);
		return QUILL_FAILURE;
	}

	if(is_locked)
		return QUILL_SUCCESS;

	if(lock->obtain(WRITE_LOCK) == 0) /* 0 means an unsuccessful lock */
	{
		dprintf(D_ALWAYS,"Error locking SQL log file %s\n",outfilename);
		return QUILL_FAILURE;
	}
	else
		is_locked = true;

	return QUILL_SUCCESS;
}

QuillErrCode FILESQL::file_unlock()
{
    if (is_dummy) return QUILL_SUCCESS;
	
	if(!is_open)
	{
		dprintf(D_ALWAYS,"Error unlocking :SQL log file %s not open yet\n",outfilename);
		return QUILL_FAILURE;
	}

	if(!is_locked)
		return QUILL_SUCCESS;

	if(lock->release() == 0) /* 0 means an unsuccessful lock */
	{
		dprintf(D_ALWAYS,"Error unlocking SQL log file %s\n",outfilename);
		return QUILL_FAILURE;
	}
	else
		is_locked = false;

	return QUILL_SUCCESS;
}

int FILESQL::file_readline(MyString *buf) 
{
    if (is_dummy) return TRUE;
	
	if(!fp)
		fp = fdopen(outfiledes, "r");

	return (buf->readLine(fp, true));
}

AttrList *FILESQL::file_readAttrList() 
{
	AttrList *ad = 0;

	if(is_dummy) return ad;

	if(!fp)
		fp = fdopen(outfiledes, "r");

	int EndFlag=0;
	int ErrorFlag=0;
	int EmptyFlag=0;

    if( !( ad=new AttrList(fp,"***\n", EndFlag, ErrorFlag, EmptyFlag) ) ){
		EXCEPT("file_readAttrList Error:  Out of memory\n" );
    }

    if( ErrorFlag ) {
		dprintf( D_ALWAYS, "\t*** Warning: Bad Log file; skipping malformed Attr List\n" );
		ErrorFlag=0;
		delete ad;
		ad = 0;
    } 

	if( EmptyFlag ) {
		dprintf( D_ALWAYS, "\t*** Warning: Empty Attr List\n" );
		EmptyFlag=0;
		delete ad;
		ad = 0;
    }

	return ad;
}

QuillErrCode FILESQL::file_newEvent(const char *eventType, AttrList *info) {
	int retval = 0;
	struct stat file_status;

    if (is_dummy) return QUILL_SUCCESS;
	if(!is_open)
	{
		dprintf(D_ALWAYS,"Error in logging new event to Quill SQL log : File not open\n");
		return QUILL_FAILURE;
	}

	if(file_lock() == QUILL_FAILURE) {
		return QUILL_FAILURE;
	}

	fstat(outfiledes, &file_status);

		// only write to the log if it's not exceeding the log size limit
	if (file_status.st_size < FILESIZELIMT) {
		retval = write(outfiledes,"NEW ", strlen("NEW "));
		retval = write(outfiledes,eventType, strlen(eventType));
		retval = write(outfiledes,"\n", strlen("\n"));

		MyString temp;
		const char *tempv;
	
		retval = info->sPrint(temp);
		tempv = temp.Value();
		retval = write(outfiledes,tempv, strlen(tempv));

		retval = write(outfiledes,"***",3); /* Now the delimitor*/
		retval = write(outfiledes,"\n",1); /* Now the newline*/
	}
	
	if(file_unlock() == QUILL_FAILURE) {
		return QUILL_FAILURE;
	}

	if (retval < 0) {
		return QUILL_FAILURE;
	} else {
		return QUILL_SUCCESS;	
	}
}

QuillErrCode FILESQL::file_updateEvent(const char *eventType, 
									   AttrList *info, 
									   AttrList *condition) {
	int retval = 0;
	struct stat file_status;

    if (is_dummy) return QUILL_SUCCESS;
	if(!is_open)
	{
		dprintf(D_ALWAYS,"Error in logging event to Quill SQL Log : File not open\n");
		return QUILL_FAILURE;
	}

	if(file_lock() == QUILL_FAILURE) {
		return QUILL_FAILURE;
	}

	fstat(outfiledes, &file_status);

		// only write to the log if it's not exceeding the log size limit
	if (file_status.st_size < FILESIZELIMT) {
		retval = write(outfiledes,"UPDATE ", strlen("UPDATE "));
		retval = write(outfiledes,eventType, strlen(eventType));
		retval = write(outfiledes,"\n", strlen("\n"));

		MyString temp, temp1;
		const char *tempv;

		retval = info->sPrint(temp);
		tempv = temp.Value();
		retval = write(outfiledes,tempv, strlen(tempv));

		retval = write(outfiledes,"***",3); /* Now the delimitor*/
		retval = write(outfiledes,"\n",1); /* Now the newline*/

		retval = condition->sPrint(temp1);
		tempv = temp1.Value();
		retval = write(outfiledes,tempv, strlen(tempv));
		
		retval = write(outfiledes,"***",3); /* Now the delimitor*/
		retval = write(outfiledes,"\n",1); /* Now the newline*/	
	}

	if(file_unlock() == QUILL_FAILURE) {
		return QUILL_FAILURE;
	}

	if (retval < 0) {
		return QUILL_FAILURE;	
	} else {
		return QUILL_SUCCESS;	
	}
}

#if 0
QuillErrCode FILESQL::file_deleteEvent(const char *eventType, 
									   AttrList *condition) {
	int retval = 0;
	struct stat file_status;

	if(!is_open)
	{
		dprintf(D_ALWAYS,"Error in logging to file : File not open\n");
		return QUILL_FAILURE;
	}

	if(file_lock() == QUILL_FAILURE) {
		return QUILL_FAILURE;
	}

	fstat(outfiledes, &file_status);

		// only write to the log if it's not exceeding the log size limit
	if (file_status.st_size < FILESIZELIMT) {
		retval = write(outfiledes,"DELETE ", strlen("DELETE "));
		retval = write(outfiledes,eventType, strlen(eventType));
		retval = write(outfiledes,"\n", strlen("\n"));

		MyString temp;
		const char *tempv;
	
		retval = condition->sPrint(temp);
		tempv = temp.Value();
		retval = write(outfiledes,tempv, strlen(tempv));

		retval = write(outfiledes,"***",3); /* Now the delimitor*/
		retval = write(outfiledes,"\n",1); /* Now the newline*/
	}

	if(file_unlock() == QUILL_FAILURE) {
		return QUILL_FAILURE;
	}

	if (retval < 0) {
		return QUILL_FAILURE;	
	}
	else {
		return QUILL_SUCCESS;
	}

}
#endif

/* We put FileObj definition here because modules such as file_transfer.o and
 * classad_log.o uses FILEObj and they are part of cplus_lib.a. This way we 
 * won't get the FILEObj undefined error during compilation of any code which 
 * needs cplus_lib.a. Notice the FILEObj is just a pointer, the real object 
 * should be created only when a real database connection is needed. E.g. 
 * most daemons need database connection, there we can create a database 
 * connection in the  main function of daemon process.
 */

FILESQL *FILEObj = NULL;

/*static */ FILESQL *
FILESQL::createInstance(bool use_sql_log) { 
	FILESQL *ptr = NULL;
	MyString outfilename = "";

	MyString param_name;
	param_name.formatstr("%s_SQLLOG", get_mySubSystem()->getName());

	char *tmp = param(param_name.Value());
	if( tmp ) {
		outfilename = tmp;
		free(tmp);
	}
	else {
		tmp = param ("LOG");		

		if (tmp) {
			outfilename.formatstr("%s/sql.log", tmp);
			free(tmp);
		}
		else {
			outfilename.formatstr("sql.log");
		}
	}

	ptr = new FILESQL(outfilename.Value(), O_WRONLY|O_CREAT|O_APPEND, use_sql_log);

	if (ptr->file_open() == QUILL_FAILURE) {
		dprintf(D_ALWAYS, "FILESQL createInstance failed\n");
	}

	return ptr;
}


/*static */ void 
FILESQL::daemonAdInsert(
ClassAd *cl, 
const char *adType,
FILESQL *dbh, 
int &prevLHF)
{
	ClassAd clCopy;
	MyString tmp;
	
		// make a copy so that we can add timestamp attribute into it
	clCopy = *cl;
	
	tmp.formatstr("%s = %d", "PrevLastReportedTime", prevLHF);
	(&clCopy)->Insert(tmp.Value());

		// set the lastReportedTime and make it the new prevLHF
	prevLHF = (int)time(NULL);

	tmp.formatstr("%s = %d", "LastReportedTime", prevLHF);
	(&clCopy)->Insert(tmp.Value());

	ASSERT( dbh );
	dbh->file_newEvent(adType, &clCopy);	
}
