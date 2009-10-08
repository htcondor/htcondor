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


#ifndef READ_MULTIPLE_LOGS_H
#define READ_MULTIPLE_LOGS_H

// This class allows reading more than one user log file, as if they
// were merged into one big log file, sorted by event time.  It returns
// ULogEvent objects just like the ReadUserLog class.  It tries to 
// return the events sorted by time, but cannot distinguish between 
// events in different files that occurred in the same second. 

#include "condor_common.h"
#include "read_user_log.h"
#include "MyString.h"
#include "string_list.h"
#include "HashTable.h"
#include "condor_id.h"
#include "CondorError.h"
#include "stat_struct.h"
#include <iosfwd>
#include <string>

class MultiLogFiles
{
public:
		/** Gets values from a file, where the file contains lines of
			the form
				<keyword> <value>
			with arbitrary whitespace between the two tokens.
			@param fileName: the name of the file to parse
			@param keyword: the keyword string
			@param values: the list of values found
			@param skipTokens: number of tokens to skip between keyword
				and value
			@return "" if okay, an error message otherwise
		*/
	static MyString getValuesFromFile(const MyString &fileName,
			const MyString &keyword, StringList &values, int skipTokens = 0);

	    /** Gets the log file from a Condor submit file.
		    on success, the return value will be the log file name
		    on failure, it will be ""
		 */
    static MyString loadLogFileNameFromSubFile(const MyString &strSubFilename,
			const MyString &directory);

		/** Makes the given filename an absolute path
			@param the name of the file (input/output)
			@param a CondorError object to hold any error information
			@return true if successful, false if failed
		 */
	static bool makePathAbsolute(MyString &filename, CondorError &errstack);

#ifdef HAVE_EXT_CLASSADS
	    /** Gets the log files from a Stork submit file.
		 * @param The submit file name.
		 * @param The directory containing the submit file.
		 * @param Output string list of log file names.
		 * @return "" if okay, or else an error message.
		 */
    static MyString loadLogFileNamesFromStorkSubFile(
		const MyString &strSubFilename,
		const MyString &directory,
		StringList &listLogFilenames);
#endif

		/** Gets the number of job procs queued by a submit file
			@param The submit file name
			@param The submit file directory
			@param A MyString to receive any error message
			@return -1 if an error, otherwise the number of job procs
				queued by the submit file
		*/
	static int getQueueCountFromSubmitFile(const MyString &strSubFilename,
	            const MyString &directory, MyString &errorMsg);

		/** Initializes the given file -- creates it if it doesn't exist,
			possibly truncates it if it does.
			@param the name of the file to create or truncate
			@param whether to truncate the file
			@param a CondorError object to hold any error information
			@return true if successful, false if failed
		 */
	static bool InitializeFile(const char *filename, bool truncate,
				CondorError &errstack);

		/** Determines whether the given log file is on NFS.
			@param The log file name
			@param Whether having a log file on NFS is a fatal error (as
				opposed to a warning)
			@return true iff at least the log file is on NFS and nfsIsError
				is true
		*/
	static bool logFileOnNFS(const char *fileName, bool nfsIsError);

		/** Reads in the specified file, breaks it into lines, and
			combines the lines into "logical" lines (joins continued
			lines).
			@param The filename
			@param The StringList to receive the logical lines
			@return "" if okay, error message otherwise
		*/
	static MyString fileNameToLogicalLines(const MyString &filename,
				StringList &logicalLines);

private:
	    /** Read the entire contents of the given file into a MyString.
		 * @param The name of the file.
		 * @return The contents of the file.
		 */
    static MyString readFileToString(const MyString &strFilename);

		/**
		 * Get the given parameter if it is defined in the given submit file
		 * line.
		 * @param The submit file line.
		 * @param The name of the parameter to get.
		 * @return The parameter value defined in that line, or "" if the
		 *   parameter is not defined.
		 */
	static MyString getParamFromSubmitLine(MyString &submitLine,
			const char *paramName);

		/**
		 * Combine input ("physical") lines that end with the given
		 * continuation character into "logical" lines.
		 * @param Input string list of "physical" lines.
		 * @param Continuation character.
		 * @param Filename (for error messages).
		 * @param Output string list of "logical" lines.
		 * @return "" if okay, or else an error message.
		 */
	static MyString CombineLines(StringList &listIn, char continuation,
			const MyString &filename, StringList &listOut);

#ifdef HAVE_EXT_CLASSADS
		/**
		 * Skip whitespace in a std::string buffer.  This is a helper function
		 * for loadLogFileNamesFromStorkSubFile().  When the new ClassAds
		 * parser can skip whitespace on it's own, this function can be
		 * removed.
		 * @param buffer name
		 * @param input/output offset into buffer
		 * @return void
		 */
	static void skip_whitespace(std::string const &s,int &offset);

		/**
		 * Read a file into a std::string helper function for
		 * loadLogFileNamesFromStorkSubFile().
		 * @param Filename to read.
		 * @param output buffer
		 * @return "" if okay, or else an error message.
		 */
	static MyString readFile(char const *filename,std::string& buf);
#endif

};

class ReadMultipleUserLogs
{
public:
	ReadMultipleUserLogs();

	~ReadMultipleUserLogs();

    	/** Returns the "next" event from any log file.  The event pointer 
        	is set to point to a newly instatiated ULogEvent object.
    	*/
    ULogEventOutcome readEvent (ULogEvent * & event);

	    /** Returns true iff any of the logs we're monitoring grew since
		    the last time this method was called.
		 */
	bool detectLogGrowth();

		/** Returns the total number of user logs this object "knows
			about".
		 */
	int totalLogFileCount() const;

		/** Monitor the given log file
			@param the log file to monitor
			@param whether to truncate the log file if this is the
				first time we're seeing it
			@param a CondorError object to hold any error information
			@return true if successful, false if failed
		*/
	bool monitorLogFile(MyString logfile, bool truncateIfFirst,
				CondorError &errstack);

		/** Unmonitor the given log file
			@param the log file to unmonitor
			@param a CondorError object to hold any error information
			@return true if successful, false if failed
		*/
	bool unmonitorLogFile(MyString logfile, CondorError &errstack);

		/** Returns the number of log files we're actively monitoring
			at the present time.
		 */
	int activeLogFileCount() const { return activeLogFiles.getNumElements(); }

		/** Print information about all LogMonitor objects.
			@param the stream to print to.
		*/
	void printAllLogMonitors( FILE *stream ) const;

		/** Print information about active LogMonitor objects.
			@param the stream to print to.
		*/
	void printActiveLogMonitors( FILE *stream ) const;

protected:
	friend class CheckEvents;

	static unsigned int hashFuncJobID(const CondorID &key);

private:
	void cleanup();

	struct LogFileMonitor {
		LogFileMonitor( const MyString &file ) : logFile(file), refCount(0),
					readUserLog(NULL), state(NULL), stateError(false),
					lastLogEvent(NULL) {}

		~LogFileMonitor() {
			delete readUserLog;
			readUserLog = NULL;

			if ( state ) {
				ReadUserLog::UninitFileState( *state );
			}
			delete state;
			state = NULL;

			delete lastLogEvent;
			lastLogEvent = NULL;
		}

			// The file name corresponding to this object; if the same
			// actual log file is registered via multiple paths, this
			// is the first path used to register it.
		MyString	logFile;

			// Reference count -- how many net calls to monitor this log?
		int			refCount;

			// The actual log reader object -- may be null
		ReadUserLog	*readUserLog;

			// Log reader state -- used to pick up where we left off if
			// we close and re-open the same log file.
		ReadUserLog::FileState	*state;

			// True iff we got an error when trying to save log file
			// state -- subsequent attempts to monitor this file should
			// fail in that case.
		bool stateError;

			// The last event we read from this log.
		ULogEvent	*lastLogEvent;
	};

		// allLogFiles contains pointers to all of the LogFileMonitors
		// we know about; activeLogFiles contains just the active ones
		// (to make it easier to read events).  Note that active log files
		// are in *both* hash tables.
		// Note: these should be changed to STL maps, and should
		// also index on a combination of st_ino and st_dev (see gittrac
		// #328). wenger 2009-07-16.
	HashTable<StatStructInode, LogFileMonitor *>	allLogFiles;

	HashTable<StatStructInode, LogFileMonitor *>	activeLogFiles;

	// For instantiation in programs that use this class.
#define MULTI_LOG_HASH_INSTANCE template class \
		HashTable<StatStructInode, ReadMultipleUserLogs::LogFileMonitor *>

		/** Returns true iff the given log grew since the last time
		    we called this method on it.
		    @param The LogFileMonitor object to test.
			@param The filename this corresponds to (for error messages
				only).
		    @return True iff the log grew.

		 */
	static bool LogGrew( LogFileMonitor *monitor );

		/**
		 * Read an event from a log monitor.
		 * @param The log monitor to read from.
		 * @return The outcome of trying to read an event.
		 */
	ULogEventOutcome readEventFromLog( LogFileMonitor *monitor );

	void printLogMonitors(FILE *stream,
				HashTable<StatStructInode, LogFileMonitor *> logTable) const;

};

#endif
