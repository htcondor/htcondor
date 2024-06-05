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
#include "string_list.h"
#include "condor_id.h"
#include "CondorError.h"
#include <iosfwd>
#include <string>
#include <vector>
#include <map>

class MultiLogFiles
{
public:
		/** Gets the specified value from a submit file (looking for the
			syntax <keyword> = <value>).
			@param strSubFilename: the submit file name
			@param directory: the directory of the submit file (can be blank)
			@param The keyword of the value we need
			@return The value, or "" if there is an error
		*/
    static std::string loadValueFromSubFile(const std::string &strSubFilename,
			const std::string &directory, const char *keyword);

		/** Makes the given filename an absolute path
			@param the name of the file (input/output)
			@param a CondorError object to hold any error information
			@return true if successful, false if failed
		 */
	static bool makePathAbsolute(std::string &filename, CondorError &errstack);

		/** Initializes the given file -- creates it if it doesn't exist,
			possibly truncates it if it does.
			@param the name of the file to create or truncate
			@param whether to truncate the file
			@param a CondorError object to hold any error information
			@return true if successful, false if failed
		 */
	static bool InitializeFile(const char *filename, bool truncate,
				CondorError &errstack);

		/** Determines whether the given log file is on NFS and that
			is an error (if the log file is on NFS, but nfsIsError is
			false, this method prints a warning (at the FULLDEBUG level)
			but returns false).
			@param The log file name
			@param Whether having a log file on NFS is a fatal error (as
				opposed to a warning)
			@return true iff at least the log file is on NFS and nfsIsError
				is true
		*/
	static bool logFileNFSError(const char *fileName, bool nfsIsError);

	class FileReader
	{
	public:
			/** Constructor -- doesn't really do anything -- open is
				a separate method so errors can be returned.
			 */
		FileReader();

			/** Destructor -- closes the file if it's open.
			 */
		~FileReader();

			/** Open this file.
				@param filename: the file to open
				@return: "" on success; error message on failure
			 */
		std::string Open( const std::string &filename );

			/** Real the next "logical" line from the file.  (This means
				lines are combined if they end with a continuation character.)
				@param line: a string to receive the line string
				@return: true iff we got any data
			 */
		bool NextLogicalLine( std::string &line );

			/** Close the file.
			 */
		void Close();

	private:
		FILE *_fp;
	};

		/** Reads in the specified file, breaks it into lines, and
			combines the lines into "logical" lines (joins continued
			lines).
			@param The filename
			@param The vector of strings to receive the logical lines
			@return "" if okay, error message otherwise
		*/
	static std::string fileNameToLogicalLines(const std::string &filename,
			std::vector<std::string> &logicalLines);

private:
	    /** Read the entire contents of the given file into a string.
		 * @param The name of the file.
		 * @return The contents of the file.
		 */
	static std::string readFileToString(const std::string &strFilename);

		/**
		 * Get the given parameter if it is defined in the given submit file
		 * line.
		 * @param The submit file line.
		 * @param The name of the parameter to get.
		 * @return The parameter value defined in that line, or "" if the
		 *   parameter is not defined.
		 */
	static std::string getParamFromSubmitLine(const std::string &submitLine,
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
	static std::string CombineLines(const std::string &dataIn, char continuation,
			const std::string &filename, std::vector<std::string> &listOut);
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

		/** Returns a status code for the set of all log files currently being
			monitored (typically only a single file).
			If any of the files has grown, return ReadUserLog::LOG_STATUS_GROWN
			If any of the files is in error state, return ReadUserLog::LOG_STATUS_ERROR
			Otherwise, return ReadUserLog::LOG_STATUS_NOCHANGE
		 */
	ReadUserLog::FileStatus GetLogStatus();

		/** Returns the total number of user logs this object "knows
			about".
		 */
	size_t totalLogFileCount() const;

		/** Monitor the given log file
			@param the log file to monitor
			@param whether to truncate the log file if this is the
				first time we're seeing it
			@param a CondorError object to hold any error information
			@return true if successful, false if failed
		*/
	bool monitorLogFile(const std::string & logfile, bool truncateIfFirst,
				CondorError &errstack);

		/** Unmonitor the given log file
			@param the log file to unmonitor
			@param a CondorError object to hold any error information
			@return true if successful, false if failed
		*/
	bool unmonitorLogFile(const std::string & logfile, CondorError &errstack);

		/** Returns the number of log files we're actively monitoring
			at the present time.
		 */
	size_t activeLogFileCount() const { return activeLogFiles.size(); }

		/** Print information about all LogMonitor objects.
			@param the stream to print to.  If NULL, do dprintf().
		*/
	void printAllLogMonitors( FILE *stream ) const;

		/** Print information about active LogMonitor objects.
			@param the stream to print to.  If NULL, do dprintf().
		*/
	void printActiveLogMonitors( FILE *stream ) const;

protected:
	friend class CheckEvents;

	static size_t hashFuncJobID(const CondorID &key);

private:
	void cleanup();

	struct LogFileMonitor {
		LogFileMonitor( const std::string &file ) : logFile(file), refCount(0),
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
		std::string logFile;

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

		// allLogFiles contains pointers to all of the LogFileMonitors we know
		// about; 
		// activeLogFiles contains just the active ones (to make it
		// easier to read events).  Note that active log files are in *both*
		// hash tables.  
		//
		// Note: these should also index on a combination of
		// st_ino and st_dev (see gittrac #328). wenger 2009-07-16.

	std::map<std::string, LogFileMonitor *> allLogFiles;

	std::map<std::string, LogFileMonitor *> activeLogFiles;

		/**
		 * Read an event from a log monitor.
		 * @param The log monitor to read from.
		 * @return The outcome of trying to read an event.
		 */
	ULogEventOutcome readEventFromLog( LogFileMonitor *monitor );

		/** Print information about the LogMonitor objects in the
			given logTable.
			@param the stream to print to.  If NULL, do dprintf().
			@param the logTable to print (all or active only).
		*/
	void printLogMonitors(FILE *stream,
				const std::map<std::string, LogFileMonitor *>& logTable) const;

};

#endif
