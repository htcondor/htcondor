/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-2003 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#ifndef READ_MULTIPLE_LOGS_H
#define READ_MULTIPLE_LOGS_H

// This class allows reading more than one user log file, as if they
// were merged into one big log file, sorted by event time.  It returns
// ULogEvent objects just like the ReadUserLog class.  It tries to 
// return the events sorted by time, but cannot distinguish between 
// events in different files that occurred in the same second. 

#include "condor_common.h"
#include "user_log.c++.h"
#include "MyString.h"
#include "string_list.h"

class ReadMultipleUserLogs
{
public:
	ReadMultipleUserLogs();
	explicit ReadMultipleUserLogs(StringList &listLogFileNames);

	~ReadMultipleUserLogs();

    bool initialize(StringList &listLogFileNames);

    	/** Returns the "next" event from any log file.  The event pointer 
        	is set to point to a newly instatiated ULogEvent object.
    	*/
    ULogEventOutcome readEvent (ULogEvent * & event);

private:
	void cleanup();

	struct LogFileEntry
	{
		MyString strFilename;
		ReadUserLog readUserLog;
		ULogEvent *pLastLogEvent;
	};

	int iLogFileCount;
	LogFileEntry *pLogFileEntries;
};

	/** Gets the userlog files used by a dag
		on success, the return value will be ""
		on failure, it will be an appropriate error message
	*/
MyString getJobLogsFromSubmitFiles(StringList &listLogFilenames, 
									const MyString &strDagFile);

	// some misc. self-explanatory fcns which maybe could find a better home
MyString loadLogFileNameFromSubFile(const MyString &strSubFilename);

MyString readFileToString(const MyString &strFilename);

bool fileExists(const MyString &strFile);

MyString makeString(int iValue);

#endif
