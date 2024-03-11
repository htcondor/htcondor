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
#include "condor_debug.h"
#include "read_multiple_logs.h"
#include "condor_string.h"
#include "tmp_dir.h"
#include "stat_wrapper.h"
#include "condor_getcwd.h"

#include <iostream>
#include "classad/classad_distribution.h"

#include "fs_util.h"

#define DEBUG_LOG_FILES 0 //TEMP
#if DEBUG_LOG_FILES
#  define D_LOG_FILES D_ALWAYS
#else
#  define D_LOG_FILES D_FULLDEBUG
#endif

///////////////////////////////////////////////////////////////////////////////

ReadMultipleUserLogs::ReadMultipleUserLogs()
{
}

///////////////////////////////////////////////////////////////////////////////

ReadMultipleUserLogs::~ReadMultipleUserLogs()
{
	if (activeLogFileCount() != 0) {
    	dprintf(D_ALWAYS, "Warning: ReadMultipleUserLogs destructor "
					"called, but still monitoring %zu log(s)!\n",
					activeLogFileCount());
	}
	cleanup();
}

///////////////////////////////////////////////////////////////////////////////

bool
operator>(const tm &lhs, const tm &rhs)
{
	if (lhs.tm_year > rhs.tm_year) {
		return true;
	}
	if (lhs.tm_year < rhs.tm_year) {
		return false;
	}
	if (lhs.tm_yday > rhs.tm_yday) {
		return true;
	}
	if (lhs.tm_yday < rhs.tm_yday) {
		return false;
	}
	if (lhs.tm_hour > rhs.tm_hour) {
		return true;
	}
	if (lhs.tm_hour < rhs.tm_hour) {
		return false;
	}
	if (lhs.tm_min > rhs.tm_min) {
		return true;
	}
	if (lhs.tm_min < rhs.tm_min) {
		return false;
	}
	if (lhs.tm_sec > rhs.tm_sec) {
		return true;
	}
	return false;
}

///////////////////////////////////////////////////////////////////////////////

ULogEventOutcome
ReadMultipleUserLogs::readEvent (ULogEvent * & event)
{
    dprintf(D_FULLDEBUG, "ReadMultipleUserLogs::readEvent()\n");

	LogFileMonitor *oldestEventMon = NULL;

	for (auto& [key, monitor] : activeLogFiles) {
		ULogEventOutcome outcome = ULOG_OK;
			// If monitor->lastLogEvent != null, we already have an
			// unconsumed event from that log, so we don't need to
			// actually read the log again.
		if ( !monitor->lastLogEvent ) {
			outcome = readEventFromLog( monitor );

			if ( outcome == ULOG_RD_ERROR || outcome == ULOG_UNK_ERROR ) {
				// peter says always return an error immediately,
				// then go on our merry way trying again if they
				// call us again.
				dprintf( D_ALWAYS, "ReadMultipleUserLogs: read error "
							"on log %s\n", monitor->logFile.c_str() );
				return outcome;
			}
		}

		if ( outcome != ULOG_NO_EVENT ) {
			if (monitor->lastLogEvent) {
				if ( oldestEventMon == NULL ||
							(oldestEventMon->lastLogEvent->GetEventclock() >
							monitor->lastLogEvent->GetEventclock()) ) {
					oldestEventMon = monitor;
				}
			}
		}
	}

	if ( oldestEventMon == NULL ) {
		return ULOG_NO_EVENT;
	}

	event = oldestEventMon->lastLogEvent;
	oldestEventMon->lastLogEvent = NULL; // event has been consumed

	return ULOG_OK;
}

///////////////////////////////////////////////////////////////////////////////

ReadUserLog::FileStatus
ReadMultipleUserLogs::GetLogStatus()
{
	dprintf( D_FULLDEBUG, "ReadMultipleUserLogs::GetLogStatus()\n" );

	ReadUserLog::FileStatus status = ReadUserLog::LOG_STATUS_NOCHANGE;

	// Iterate over all the log files and check their statuses.
	for (auto& [key, monitor] : activeLogFiles) {
		ReadUserLog::FileStatus fs = monitor->readUserLog->CheckFileStatus();
		// If a log files has grown, we want to return ReadUserLog::LOG_STATUS_GROWN
		// Do not exit the loop early, since checking the file status also
		// updates the internal member variable tracking the size of the file
		if ( fs == ReadUserLog::LOG_STATUS_GROWN ) {
			status = fs;
		}
		// If a log file has shrunk or is in error, we want to return the
		// correct status code.
		// We can exit early here, because we're just going to abort.
		else if ( fs == ReadUserLog::LOG_STATUS_ERROR || fs == ReadUserLog::LOG_STATUS_SHRUNK ) {
			status = fs;
			dprintf( D_ALWAYS, "MultiLogFiles: detected error, cleaning up all log monitors\n" );
			cleanup();
			break;
		}
	}

    return status;
}

///////////////////////////////////////////////////////////////////////////////

size_t
ReadMultipleUserLogs::totalLogFileCount() const
{
	return allLogFiles.size();
}

///////////////////////////////////////////////////////////////////////////////

bool
MultiLogFiles::InitializeFile(const char *filename, bool truncate,
			CondorError &errstack)
{
	dprintf( D_LOG_FILES, "MultiLogFiles::InitializeFile(%s, %d)\n",
				filename, (int)truncate );

	int flags = O_WRONLY;
	if ( truncate ) {
		flags |= O_TRUNC;
		dprintf( D_ALWAYS, "MultiLogFiles: truncating log file %s\n",
					filename );
	}

		// Two-phase attempt at open here is to make things work if
		// a log file is a symlink to another file (see gittrac #2704).
	int fd = safe_create_fail_if_exists( filename, flags );
	if ( fd < 0 && errno == EEXIST ) {
		fd = safe_open_no_create_follow( filename, flags );
	}
	if ( fd < 0 ) {
		errstack.pushf("MultiLogFiles", UTIL_ERR_OPEN_FILE,
					"Error (%d, %s) opening file %s for creation "
					"or truncation", errno, strerror( errno ), filename );
		return false;
	}

	if ( close( fd ) != 0 ) {
		errstack.pushf("MultiLogFiles", UTIL_ERR_CLOSE_FILE,
					"Error (%d, %s) closing file %s for creation "
					"or truncation", errno, strerror( errno ), filename );
		return false;
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////

void
ReadMultipleUserLogs::cleanup()
{
	activeLogFiles.clear();

	for (auto& [key, monitor] : allLogFiles) {
		delete monitor;
	}
	allLogFiles.clear();
}

///////////////////////////////////////////////////////////////////////////////

ULogEventOutcome
ReadMultipleUserLogs::readEventFromLog( LogFileMonitor *monitor )
{
	dprintf( D_FULLDEBUG, "ReadMultipleUserLogs::readEventFromLog(%s)\n",
				monitor->logFile.c_str() );

	ULogEventOutcome	result =
				monitor->readUserLog->readEvent( monitor->lastLogEvent );

	return result;
}

///////////////////////////////////////////////////////////////////////////////
MultiLogFiles::FileReader::FileReader()
{
	_fp = NULL;
}

MultiLogFiles::FileReader::~FileReader()
{
	Close();
}

std::string
MultiLogFiles::FileReader::Open( const std::string &filename )
{
	std::string result = "";

	_fp = safe_fopen_wrapper_follow( filename.c_str(), "r" );
	if ( !_fp ) {
		formatstr( result, "MultiLogFiles::FileReader::Open(): "
				"safe_fopen_wrapper_follow(%s) failed with errno %d (%s)\n",
				filename.c_str(), errno, strerror(errno) );
		dprintf( D_ALWAYS, "%s", result.c_str() );
	}

	return result;
}

bool
MultiLogFiles::FileReader::NextLogicalLine( std::string &line )
{
	int lines_read = 0;
	char *tmpLine = getline_trim( _fp, lines_read );
	if ( tmpLine != NULL ) {
		line = tmpLine;
		return true;
	}

	return false; // EOF
}

void
MultiLogFiles::FileReader::Close()
{
	if ( _fp ) {
		fclose( _fp );
		_fp = NULL;
	}
}

///////////////////////////////////////////////////////////////////////////////

std::string
MultiLogFiles::fileNameToLogicalLines(const std::string &filename,
			std::vector<std::string> &logicalLines)
{
	std::string result;

	std::string fileContents = readFileToString(filename);
	if (fileContents == "") {
		result = "Unable to read file: " + filename;
		dprintf(D_ALWAYS, "MultiLogFiles: %s\n", result.c_str());
		return result;
	}

		// Parse the file string into physical lines, and
		// combine into logical lines with continuation characters.
		// Note: The parsing removes leading whitespace.
	std::string combineResult = CombineLines(fileContents, '\\',
				filename, logicalLines);
	if ( combineResult != "" ) {
		result = combineResult;
		return result;
	}

	return result;
}

///////////////////////////////////////////////////////////////////////////////

std::string
MultiLogFiles::readFileToString(const std::string &strFilename)
{
	dprintf( D_FULLDEBUG, "MultiLogFiles::readFileToString(%s)\n",
				strFilename.c_str() );

	FILE *pFile = safe_fopen_wrapper_follow(strFilename.c_str(), "r");
	if (!pFile) {
		dprintf( D_ALWAYS, "MultiLogFiles::readFileToString: "
				"safe_fopen_wrapper_follow(%s) failed with errno %d (%s)\n", strFilename.c_str(),
				errno, strerror(errno) );
		return "";
	}

	if ( fseek(pFile, 0, SEEK_END) != 0 ) {
		dprintf( D_ALWAYS, "MultiLogFiles::readFileToString: "
				"fseek(%s) failed with errno %d (%s)\n", strFilename.c_str(),
				errno, strerror(errno) );
		fclose(pFile);
		return "";
	}
	int iLength = ftell(pFile);
	if ( iLength == -1 ) {
		dprintf( D_ALWAYS, "MultiLogFiles::readFileToString: "
				"ftell(%s) failed with errno %d (%s)\n", strFilename.c_str(),
				errno, strerror(errno) );
		fclose(pFile);
		return "";
	}
	std::string strToReturn;
	strToReturn.reserve(iLength);

	if (fseek(pFile, 0, SEEK_SET) < 0) {
		dprintf( D_ALWAYS, "MultiLogFiles::readFileToString: "
				"fseek(%s) failed with errno %d (%s)\n", strFilename.c_str(),
				errno, strerror(errno) );
		fclose(pFile);
		return "";
	}

	char *psBuf = new char[iLength+1];
		/*  We now clear the buffer to ensure there will be a NULL at the
			end of our buffer after the fread().  Why no just do
				psBuf[iLength] = 0  ?
			Because on Win32, iLength may not point to the end of 
			the buffer because \r\n are converted into \n because
			the file is opened in text mode.  
		*/
	memset(psBuf,0,iLength+1);
	int ret = fread(psBuf, 1, iLength, pFile);
	psBuf[iLength] = '\0'; // static analysis doesn't know memset?

	if (ret == 0) {
		dprintf( D_ALWAYS, "MultiLogFiles::readFileToString: "
				"fread failed with errno %d (%s)\n", 
				errno, strerror(errno) );
		fclose(pFile);
		delete [] psBuf;
		return "";
	}
	
	fclose(pFile);

	strToReturn = psBuf;
	delete [] psBuf;

	return strToReturn;
}

///////////////////////////////////////////////////////////////////////////////
// Note: this method should get speeded up (see Gnats PR 846).

std::string
MultiLogFiles::loadValueFromSubFile(const std::string &strSubFilename,
		const std::string &directory, const char *keyword)
{
	dprintf( D_FULLDEBUG, "MultiLogFiles::loadValueFromSubFile(%s, %s, %s)\n",
				strSubFilename.c_str(), directory.c_str(), keyword );

	TmpDir		td;
	if ( directory != "" ) {
		std::string	errMsg;
		if ( !td.Cd2TmpDir(directory.c_str(), errMsg) ) {
			dprintf(D_ALWAYS, "Error from Cd2TmpDir: %s\n", errMsg.c_str());
			return "";
		}
	}

	std::vector<std::string> logicalLines;
	if ( fileNameToLogicalLines( strSubFilename, logicalLines ) != "" ) {
		return "";
	}

	std::string value;

		// Now look through the submit file logical lines to find the
		// value corresponding to the keyword.
	for (const auto& submitLine : logicalLines) {
		std::string tmpValue = getParamFromSubmitLine(submitLine, keyword);
		if ( tmpValue != "" ) {
			value = tmpValue;
		}
	}

		//
		// Check for macros in the value -- we currently don't
		// handle those.
		//
	if ( value != "" ) {
		if ( strchr(value.c_str(), '$') ) {
			dprintf(D_ALWAYS, "MultiLogFiles: macros not allowed "
						"in %s in DAG node submit files\n", keyword);
			value = "";
		}
	}

	if ( directory != "" ) {
		std::string	errMsg;
		if ( !td.Cd2MainDir(errMsg) ) {
			dprintf(D_ALWAYS, "Error from Cd2MainDir: %s\n", errMsg.c_str());
			return "";
		}
	}

	return value;
}

///////////////////////////////////////////////////////////////////////////////

bool
MultiLogFiles::makePathAbsolute(std::string &filename, CondorError &errstack)
{
	if ( !fullpath(filename.c_str()) ) {
			// I'd like to use realpath() here, but I'm not sure
			// if that's portable across all platforms.  wenger 2009-01-09.
		std::string currentDir;
		if ( !condor_getcwd(currentDir) ) {
			errstack.pushf( "MultiLogFiles", UTIL_ERR_GET_CWD,
						"ERROR: condor_getcwd() failed with errno %d (%s) at %s:%d",
						errno, strerror(errno), __FILE__, __LINE__);
			return false;
		}

		filename = currentDir + DIR_DELIM_STRING + filename;
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////

std::string
MultiLogFiles::getParamFromSubmitLine(const std::string &submitLineIn,
		const char *paramName)
{
	std::string paramValue;

	const char *DELIM = "=";

	StringTokenIterator submittok(submitLineIn, DELIM, true);
	const char *token = submittok.next();
	if ( token ) {
		if ( !strcasecmp(token, paramName) ) {
			token = submittok.next();
			if ( token ) {
				paramValue = token;
			}
		}
	}

	return paramValue;
}

///////////////////////////////////////////////////////////////////////////////

std::string
MultiLogFiles::CombineLines(const std::string &dataIn, char continuation,
		const std::string &filename, std::vector<std::string> &listOut)
{
	dprintf( D_FULLDEBUG, "MultiLogFiles::CombineLines(%s, %c)\n",
				filename.c_str(), continuation );

	// Logical line is physical lines combined as needed by
	// continuation characters (backslash).
	std::string logicalLine;

	for (const auto& physicalLine : StringTokenIterator(dataIn, "\r\n", false)) {

		logicalLine += physicalLine;

		if (logicalLine[logicalLine.length()-1] == continuation) {
			// Remove the continuation character.
			logicalLine.erase(logicalLine.length()-1);
		} else {
			// Logical line is complete, add to the output.
			listOut.emplace_back(logicalLine);
			logicalLine.clear();
		}
	}
	if (!logicalLine.empty()) {
		std::string result = std::string("Improper file syntax: ") +
					"continuation character with no trailing line! (" +
					logicalLine + ") in file " + filename;
		dprintf(D_ALWAYS, "MultiLogFiles: %s\n", result.c_str());
		return result;
	}

	return ""; // blank means okay
}

///////////////////////////////////////////////////////////////////////////////

size_t
ReadMultipleUserLogs::hashFuncJobID(const CondorID &key)
{
	long result = (key._cluster * 29) ^ (key._proc * 7) ^ key._subproc;

		// Make sure we produce a non-negative result (modulus on negative
		// value may produce a negative result (implementation-dependent).
	if ( result < 0 ) result = -result;

	return (size_t)result;
}

///////////////////////////////////////////////////////////////////////////////

bool
MultiLogFiles::logFileNFSError(const char *logFilename, bool nfsIsError)
{

	bool isNfs;
   
	if ( fs_detect_nfs( logFilename, &isNfs ) != 0 ) {
		// can't determine if it's on NFS
		dprintf(D_ALWAYS, "WARNING: can't determine whether log file %s "
			"is on NFS.\n", logFilename);

	} else if ( isNfs ) {
		if ( nfsIsError ) {
			dprintf(D_ALWAYS, "ERROR: log file %s is on NFS.\n", logFilename);
			return true;
		}
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////////
// Note: on Unix/Linux, the file ID is a string encoding the combination of
// device number and inode; on Windows the file ID is simply the value
// _fullpath() returns on the path we're given.  The Unix/Linux version
// is preferable because it will work correctly even if there are hard
// links to log files; but there are no inodes on Windows, so we're
// doing what we can.
bool
GetFileID( const std::string &filename, std::string &fileID,
			CondorError &errstack )
{

		// Make sure the log file exists.  Even though we may later call
		// InitializeFile(), we have to make sure the file exists here
		// first so we make sure that the file exists and we can therefore
		// get an inode or real path for it.
		// We *don't* want to truncate the file here, though, because
		// we don't know for sure whether it's the first time we're seeing
		// it.
	if ( access( filename.c_str(), F_OK ) != 0 ) {
		if ( !MultiLogFiles::InitializeFile( filename.c_str(),
					false, errstack ) ) {
			errstack.pushf( "ReadMultipleUserLogs", UTIL_ERR_LOG_FILE,
						"Error initializing log file %s", filename.c_str() );
			return false;
		}
	}

#ifdef WIN32
	char *tmpRealPath = realpath( filename.c_str(), NULL );
	if ( !tmpRealPath ) {
		errstack.pushf( "ReadMultipleUserLogs", UTIL_ERR_LOG_FILE,
					"Error (%d, %s) getting real path for specified path %s",
					errno, strerror( errno ), filename.c_str() );
		return false;
	}

	fileID = tmpRealPath;
	free( tmpRealPath );
#else
	StatWrapper swrap;
	if ( swrap.Stat( filename.c_str() ) != 0 ) {
		errstack.pushf( "ReadMultipleUserLogs", UTIL_ERR_LOG_FILE,
					"Error getting inode for log file %s",
					filename.c_str() );
		return false;
	}
	formatstr( fileID, "%llu:%llu", (unsigned long long)swrap.GetBuf()->st_dev,
				(unsigned long long)swrap.GetBuf()->st_ino );
#endif

	return true;
}

///////////////////////////////////////////////////////////////////////////////

bool
ReadMultipleUserLogs::monitorLogFile( const std::string & l,
			bool truncateIfFirst, CondorError &errstack )
{
	std::string logfile = l;
	dprintf( D_LOG_FILES, "ReadMultipleUserLogs::monitorLogFile(%s, %d)\n",
				logfile.c_str(), truncateIfFirst );

	std::string fileID;
	if ( !GetFileID( logfile, fileID, errstack ) ) {
		errstack.push( "ReadMultipleUserLogs", UTIL_ERR_LOG_FILE,
					"Error getting file ID in monitorLogFile()" );
		return false;
	}

	LogFileMonitor *monitor = nullptr;
	auto it = allLogFiles.find(fileID);
	if (it != allLogFiles.end()) {
		dprintf( D_LOG_FILES, "ReadMultipleUserLogs: found "
					"LogFileMonitor object for %s (%s)\n",
					logfile.c_str(), fileID.c_str() );
		monitor = it->second;
	} else {
		dprintf( D_LOG_FILES, "ReadMultipleUserLogs: didn't "
					"find LogFileMonitor object for %s (%s)\n",
					logfile.c_str(), fileID.c_str() );

			// Make sure the log file is in the correct state -- it must
			// exist, and be truncated if necessary.
		if ( !MultiLogFiles::InitializeFile( logfile.c_str(),
					truncateIfFirst, errstack ) ) {
			errstack.pushf( "ReadMultipleUserLogs", UTIL_ERR_LOG_FILE,
						"Error initializing log file %s", logfile.c_str() );
			return false;
		}

		monitor = new LogFileMonitor( logfile );
		ASSERT( monitor );
		dprintf( D_LOG_FILES, "ReadMultipleUserLogs: created LogFileMonitor "
					"object for log file %s\n", logfile.c_str() );
			// Note: we're only putting a pointer to the LogFileMonitor
			// object into the hash table; the actual LogFileMonitor should
			// only be deleted in this object's destructor.
		allLogFiles[fileID] = monitor;
	}

	if ( monitor->refCount < 1 ) {
			// Open the log file (return to previous location if it was
			// opened before).
	
		if ( monitor->state ) {
				// If we get here, we've monitored this log file before,
				// so restore the previous state.
			if ( monitor->stateError ) {
				errstack.pushf( "ReadMultipleUserLogs", UTIL_ERR_LOG_FILE,
							"Monitoring log file %s fails because of "
							"previous error saving file state",
							logfile.c_str() );
				return false;
			}

			monitor->readUserLog = new ReadUserLog( *(monitor->state) );
		} else {
				// Monitoring this log file for the first time, so create
				// the log reader from scratch.
			monitor->readUserLog =
						new ReadUserLog( monitor->logFile.c_str() );
		}

		activeLogFiles[fileID] = monitor;
		dprintf( D_LOG_FILES, "ReadMultipleUserLogs: added log "
					"file %s (%s) to active list\n", logfile.c_str(),
					fileID.c_str() );
	}

	monitor->refCount++;

	return true;
}

///////////////////////////////////////////////////////////////////////////////

bool
ReadMultipleUserLogs::unmonitorLogFile( const std::string & l,
			CondorError &errstack )
{
	std::string logfile = l;
	dprintf( D_LOG_FILES, "ReadMultipleUserLogs::unmonitorLogFile(%s)\n",
				logfile.c_str() );

	std::string fileID;
	if ( !GetFileID( logfile, fileID, errstack ) ) {
		errstack.push( "ReadMultipleUserLogs", UTIL_ERR_LOG_FILE,
					"Error getting file ID in unmonitorLogFile()" );
		return false;
	}

	LogFileMonitor *monitor;
	auto it = activeLogFiles.find(fileID);
	if (it == activeLogFiles.end()) {
		errstack.pushf( "ReadMultipleUserLogs", UTIL_ERR_LOG_FILE,
					"Didn't find LogFileMonitor object for log "
					"file %s (%s)!", logfile.c_str(),
					fileID.c_str() );
		dprintf( D_ALWAYS, "ReadMultipleUserLogs error: %s\n",
					errstack.message() );
		printAllLogMonitors( NULL );
		return false;
	}
	monitor = it->second;

	dprintf( D_LOG_FILES, "ReadMultipleUserLogs: found "
				"LogFileMonitor object for %s (%s)\n",
				logfile.c_str(), fileID.c_str() );

	monitor->refCount--;

	if ( monitor->refCount < 1 ) {
			// Okay, if we are no longer monitoring this file at all,
			// we need to close it.  We do that by saving its state
			// into a ReadUserLog::FileState object (so we can go back
			// to the right place if we later monitor it again) and
			// then deleting the ReadUserLog object.
		dprintf( D_LOG_FILES, "Closing file <%s>\n", logfile.c_str() );

		if ( !monitor->state ) {
			monitor->state = new ReadUserLog::FileState();
			if ( !ReadUserLog::InitFileState( *(monitor->state) ) ) {
				errstack.pushf( "ReadMultipleUserLogs", UTIL_ERR_LOG_FILE,
							"Unable to initialize ReadUserLog::FileState "
							"object for log file %s", logfile.c_str() );
				monitor->stateError = true;
				delete monitor->state;
				monitor->state = NULL;
				return false;
			}
		}

		if ( !monitor->readUserLog->GetFileState( *(monitor->state) ) ) {
			errstack.pushf( "ReadMultipleUserLogs", UTIL_ERR_LOG_FILE,
						"Error getting state for log file %s",
						logfile.c_str() );
			monitor->stateError = true;
			delete monitor->state;
			monitor->state = NULL;
			return false;
		}

		delete monitor->readUserLog;
		monitor->readUserLog = NULL;

			// Now we remove this file from the "active" list, so
			// we don't check it the next time we get an event.
		if (activeLogFiles.erase(fileID) == 0) {
			errstack.pushf( "ReadMultipleUserLogs", UTIL_ERR_LOG_FILE,
						"Error removing %s (%s) from activeLogFiles",
						logfile.c_str(), fileID.c_str() );
			dprintf( D_ALWAYS, "ReadMultipleUserLogs error: %s\n",
						errstack.message() );
			printAllLogMonitors( NULL );
			return false;
		}

		dprintf( D_LOG_FILES, "ReadMultipleUserLogs: removed "
					"log file %s (%s) from active list\n",
					logfile.c_str(), fileID.c_str() );
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////

void
ReadMultipleUserLogs::printAllLogMonitors( FILE *stream ) const
{
	if ( stream != NULL ) {
		fprintf( stream, "All log monitors:\n" );
	} else {
		dprintf( D_ALWAYS, "All log monitors:\n" );
	}
	printLogMonitors( stream, allLogFiles );
}

///////////////////////////////////////////////////////////////////////////////

void
ReadMultipleUserLogs::printActiveLogMonitors( FILE *stream ) const
{
	if ( stream != NULL ) {
		fprintf( stream, "Active log monitors:\n" );
	} else {
		dprintf( D_ALWAYS, "Active log monitors:\n" );
	}
	printLogMonitors( stream, activeLogFiles );
}

///////////////////////////////////////////////////////////////////////////////

void
ReadMultipleUserLogs::printLogMonitors( FILE *stream,
			const std::map<std::string, LogFileMonitor *>& logTable ) const
{
	for (auto& [fileID, monitor] : logTable) {
		if ( stream != NULL ) {
			fprintf( stream, "  File ID: %s\n", fileID.c_str() );
			fprintf( stream, "    Monitor: %p\n", monitor );
			fprintf( stream, "    Log file: <%s>\n", monitor->logFile.c_str() );
			fprintf( stream, "    refCount: %d\n", monitor->refCount );
			fprintf( stream, "    lastLogEvent: %p\n", monitor->lastLogEvent );
		} else {
			dprintf( D_ALWAYS, "  File ID: %s\n", fileID.c_str() );
			dprintf( D_ALWAYS, "    Monitor: %p\n", monitor );
			dprintf( D_ALWAYS, "    Log file: <%s>\n", monitor->logFile.c_str() );
			dprintf( D_ALWAYS, "    refCount: %d\n", monitor->refCount );
			dprintf( D_ALWAYS, "    lastLogEvent: %p\n", monitor->lastLogEvent );
		}
	}
}
