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

	    /** Sets the list of log files to monitor; throws away any
		    previous list of files to monitor.
		 */
    bool initialize(StringList &listLogFileNames);

    	/** Returns the "next" event from any log file.  The event pointer 
        	is set to point to a newly instatiated ULogEvent object.
    	*/
    ULogEventOutcome readEvent (ULogEvent * & event);

	    /** Returns true iff any of the logs we're monitoring grew since
		    the last time this method was called.
		 */
	bool detectLogGrowth();

	    /** Deletes the given log files.
		 */
	static void DeleteLogs(StringList &logFileNames);

	    /** Gets the userlog files used by a dag
		    on success, the return value will be ""
		    on failure, it will be an appropriate error message
	    */
    static MyString getJobLogsFromSubmitFiles(const MyString &strDagFileName,
			const MyString &jobKeyword, StringList &listLogFilenames);

	    /** Gets the log file from a Condor submit file.
		    on success, the return value will be the log file name
		    on failure, it will be ""
		 */
    static MyString loadLogFileNameFromSubFile(const MyString &strSubFilename);

private:
	void cleanup();

	struct LogFileEntry
	{
		bool isInitialized;
		MyString strFilename;
		ReadUserLog readUserLog;
		ULogEvent *pLastLogEvent;
		off_t logSize;
	};

	int iLogFileCount;
	LogFileEntry *pLogFileEntries;

	    /** Goes through the list of logs and tries to initialize (open
		    the file of) any that aren't initialized yet.
			Returns true iff it successfully initialized *any* previously-
			uninitialized log.
		 */
	bool initializeUninitializedLogs();

		/** Returns true iff the given log grew since the last time
		    we called this method on it.
		 */
	static bool LogGrew(LogFileEntry &log);

	    /** Read the entire contents of the given file into a MyString.
		 */
    static MyString readFileToString(const MyString &strFilename);
};

#endif
