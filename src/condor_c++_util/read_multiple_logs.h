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
