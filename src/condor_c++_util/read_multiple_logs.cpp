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
#include "condor_string.h" // for strnewp()
#include "tmp_dir.h"
#include "stat_wrapper.h"
#ifdef HAVE_EXT_CLASSADS
#ifndef WANT_CLASSAD_NAMESPACE
#define WANT_CLASSAD_NAMESPACE
#endif
#include "condor_fix_iostream.h"
#include "classad/classad_distribution.h"
#endif
#include "fs_util.h"

#define LOG_HASH_SIZE 37 // prime

#define LOG_INFO_HASH_SIZE 37 // prime

#define DEBUG_LOG_FILES 0 //TEMP
#if DEBUG_LOG_FILES
#  define D_LOG_FILES D_ALWAYS
#else
#  define D_LOG_FILES D_FULLDEBUG
#endif

///////////////////////////////////////////////////////////////////////////////

#if LAZY_LOG_FILES
unsigned int	InodeHash( const StatStructInode &inode )
{
	unsigned int result = inode & 0xffffffff;

	return result;
}
#endif

///////////////////////////////////////////////////////////////////////////////

ReadMultipleUserLogs::ReadMultipleUserLogs() :
#if !LAZY_LOG_FILES
	logHash(LOG_HASH_SIZE, hashFuncJobID, rejectDuplicateKeys)
#else
	allLogFiles(LOG_INFO_HASH_SIZE, InodeHash, rejectDuplicateKeys),
	activeLogFiles(LOG_INFO_HASH_SIZE, InodeHash, rejectDuplicateKeys)
#endif // LAZY_LOG_FILES
{
#if !LAZY_LOG_FILES
	pLogFileEntries = NULL;
	iLogFileCount = 0;
#endif // !LAZY_LOG_FILES
}

///////////////////////////////////////////////////////////////////////////////

#if !LAZY_LOG_FILES
ReadMultipleUserLogs::ReadMultipleUserLogs(StringList &listLogFileNames) :
	logHash(LOG_HASH_SIZE, hashFuncJobID, rejectDuplicateKeys)
{
	pLogFileEntries = NULL;
	iLogFileCount = 0;
	initialize(listLogFileNames);
}
#endif // !LAZY_LOG_FILES

///////////////////////////////////////////////////////////////////////////////

ReadMultipleUserLogs::~ReadMultipleUserLogs()
{
#if LAZY_LOG_FILES
	if (activeLogFileCount() != 0) {
    	dprintf(D_ALWAYS, "Warning: ReadMultipleUserLogs destructor "
					"called, but still monitoring %d log(s)!\n",
					activeLogFileCount());
	}
#endif // LAZY_LOG_FILES
	cleanup();
}

///////////////////////////////////////////////////////////////////////////////

#if !LAZY_LOG_FILES
bool
ReadMultipleUserLogs::initialize(StringList &listLogFileNames)
{
    dprintf(D_FULLDEBUG, "ReadMultipleUserLogs::initialize()\n");

	cleanup();

	iLogFileCount = listLogFileNames.number();
	if (iLogFileCount) {
		pLogFileEntries = new LogFileEntry[iLogFileCount];
		if( !pLogFileEntries ) {
		    EXCEPT( "ERROR: out of memory!\n");
		}
	}

	listLogFileNames.rewind();

	for (int i = 0; i < iLogFileCount; i++) {
		char *psFilename = listLogFileNames.next();
		pLogFileEntries[i].isInitialized = FALSE;
		pLogFileEntries[i].isValid = FALSE;
		pLogFileEntries[i].haveReadEvent = FALSE;
		pLogFileEntries[i].pLastLogEvent = NULL;
		pLogFileEntries[i].strFilename = psFilename;
		pLogFileEntries[i].logSize = 0;
	}

	bool result = initializeUninitializedLogs();

	return result;
}
#endif // !LAZY_LOG_FILES

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

#if LAZY_LOG_FILES
	LogFileMonitor *oldestEventMon = NULL;

	activeLogFiles.startIterations();
	LogFileMonitor *monitor;
	while ( activeLogFiles.iterate( monitor ) ) {
		ULogEventOutcome outcome = ULOG_OK;
		if ( !monitor->lastLogEvent ) {
			outcome = readEventFromLog( monitor );

			if ( outcome == ULOG_RD_ERROR || outcome == ULOG_UNK_ERROR ) {
				// peter says always return an error immediately,
				// then go on our merry way trying again if they
				// call us again.
				dprintf( D_ALWAYS, "ReadMultipleUserLogs: read error "
							"on log %s\n", monitor->logFile.Value() );
				return outcome;
			}
		}

		if ( outcome != ULOG_NO_EVENT ) {
			if ( oldestEventMon == NULL ||
						(oldestEventMon->lastLogEvent->eventTime >
						monitor->lastLogEvent->eventTime) ) {
				oldestEventMon = monitor;
			}
		}
	}

	if ( oldestEventMon == NULL ) {
		return ULOG_NO_EVENT;
	}

	event = oldestEventMon->lastLogEvent;
	oldestEventMon->lastLogEvent = NULL;
#else
	if (!iLogFileCount) {
		return ULOG_UNK_ERROR;
	}

    initializeUninitializedLogs();

	int iOldestEventIndex = -1;

	for ( int i = 0; i < iLogFileCount; i++ ) {
		LogFileEntry &log = pLogFileEntries[i];
		if ( log.isInitialized && log.isValid ) {
		    ULogEventOutcome eOutcome = ULOG_OK;
		    if ( !log.pLastLogEvent ) {
				eOutcome = readEventFromLog(log);

		        if ( eOutcome == ULOG_RD_ERROR || eOutcome == ULOG_UNK_ERROR ) {
			        // peter says always return an error immediately,
			        // then go on our merry way trying again if they
					// call us again.
			        dprintf(D_ALWAYS, "ReadMultipleUserLogs: read error "
								"on log %s\n",
					    log.strFilename.Value());
			        return eOutcome;
		        }
		    }

		    if ( eOutcome != ULOG_NO_EVENT ) {
			    if (iOldestEventIndex == -1 || 
				        (pLogFileEntries[iOldestEventIndex].pLastLogEvent->eventTime >
				        log.pLastLogEvent->eventTime)) {
				    iOldestEventIndex = i;
			    }
		    }
		}
	}

	if ( iOldestEventIndex == -1 ) {
		return ULOG_NO_EVENT;
	}
	
	event = pLogFileEntries[iOldestEventIndex].pLastLogEvent;
	pLogFileEntries[iOldestEventIndex].pLastLogEvent = NULL;
#endif

	return ULOG_OK;
}

///////////////////////////////////////////////////////////////////////////////

bool
ReadMultipleUserLogs::detectLogGrowth()
{
    dprintf( D_FULLDEBUG, "ReadMultipleUserLogs::detectLogGrowth()\n" );

#if !LAZY_LOG_FILES
    initializeUninitializedLogs();
#endif // !LAZY_LOG_FILES

	bool grew = false;

	    // Note: we must go through the whole loop even after we find a
		// log that grew, so we have the right log lengths for next time.
		// wenger 2003-04-11.
#if LAZY_LOG_FILES
	activeLogFiles.startIterations();
	LogFileMonitor *monitor;
	while ( activeLogFiles.iterate( monitor ) ) {
	    if ( LogGrew( monitor ) ) {
		    grew = true;
		}
	}
#else
    for (int index = 0; index < iLogFileCount; index++) {
	    if (LogGrew(pLogFileEntries[index])) {
		    grew = true;
		}
	}
#endif

    return grew;
}

///////////////////////////////////////////////////////////////////////////////

#if !LAZY_LOG_FILES
int
ReadMultipleUserLogs::getInitializedLogCount() const
{
    dprintf( D_FULLDEBUG, "ReadMultipleUserLogs::getInitializedLogCount()\n");

	int result = 0;

	for ( int i = 0; i < iLogFileCount; ++i ) {
		LogFileEntry &log = pLogFileEntries[i];
		if ( log.isInitialized ) ++result;
	}

	return result;
}
#endif // !LAZY_LOG_FILES

///////////////////////////////////////////////////////////////////////////////

int
ReadMultipleUserLogs::totalLogFileCount() const
{
#if LAZY_LOG_FILES
	return allLogFiles.getNumElements();
#else
	return iLogFileCount;
#endif // !LAZY_LOG_FILES
}

///////////////////////////////////////////////////////////////////////////////

#if !LAZY_LOG_FILES
void
MultiLogFiles::TruncateLogs(StringList &logFileNames)
{
    logFileNames.rewind();
	char *filename;
	while ( (filename = logFileNames.next()) ) {
        if ( access( filename, F_OK) == 0 ) {
		    dprintf( D_ALWAYS, "MultiLogFiles: truncating older "
						"version of %s\n", filename);
			int fd = safe_open_no_create( filename, O_WRONLY | O_TRUNC );
			if (fd == -1) {
		        dprintf( D_ALWAYS, "MultiLogFiles error: can't "
							"truncate %s\n", filename );
		    } else {
				if (close( fd ) == -1) {
		        	dprintf( D_ALWAYS, "MultiLogFiles error: can't "
								"close %s\n", filename );
				}
			}
		}
	}
}
#endif // !LAZY_LOG_FILES

///////////////////////////////////////////////////////////////////////////////

#if LAZY_LOG_FILES
bool
MultiLogFiles::InitializeFile(const char *filename, bool truncate,
			CondorError &errstack)
{
	dprintf( D_LOG_FILES, "MultiLogFiles::InitializeFile(%s, %d)\n",
				filename, (int)truncate );

	bool result = true;

	int flags = O_WRONLY;
	if ( truncate ) flags |= O_TRUNC;
	int fd = safe_create_keep_if_exists( filename, flags );
	if ( fd < 0 ) {
		errstack.pushf("MultiLogFiles", UTIL_ERR_OPEN_FILE,
					"Error (%d, %s) opening file %s for creation "
					"or truncation", errno, strerror( errno ), filename );
		result = false;
	} else {
		if ( close( fd ) != 0 ) {
			errstack.pushf("MultiLogFiles", UTIL_ERR_CLOSE_FILE,
						"Error (%d, %s) closing file %s for creation "
						"or truncation", errno, strerror( errno ), filename );
			result = false;
		}
	}

	return true;
}
#endif // LAZY_LOG_FILES

///////////////////////////////////////////////////////////////////////////////

void
ReadMultipleUserLogs::cleanup()
{

#if LAZY_LOG_FILES
	activeLogFiles.clear();

	allLogFiles.startIterations();
	LogFileMonitor *monitor;
	while ( allLogFiles.iterate( monitor ) ) {
		delete monitor;
	}
	allLogFiles.clear();
#else
	if (pLogFileEntries == NULL) {
		return;
	}

	for (int i = 0; i < iLogFileCount; i++) {
		if (pLogFileEntries[i].pLastLogEvent) {
			delete pLogFileEntries[i].pLastLogEvent;
		}
	}
	delete [] pLogFileEntries;
	
	pLogFileEntries = NULL;
	iLogFileCount = 0;
#endif // LAZY_LOG_FILES
}

///////////////////////////////////////////////////////////////////////////////

#if !LAZY_LOG_FILES
bool
ReadMultipleUserLogs::initializeUninitializedLogs()
{
    bool result = false;

	for (int index = 0; index < iLogFileCount; index++) {
	    LogFileEntry &log = pLogFileEntries[index];
		if (!log.isInitialized) {
		    if (log.readUserLog.initialize(log.strFilename.Value())) {
				log.isInitialized = true;
				log.isValid = true;
			    result = true;
			}
		}
	}

	return result;
}
#endif // !LAZY_LOG_FILES

///////////////////////////////////////////////////////////////////////////////

#if !LAZY_LOG_FILES
bool
ReadMultipleUserLogs::LogGrew(LogFileEntry &log)
{
    dprintf( D_FULLDEBUG, "ReadMultipleUserLogs::LogGrew(%s)\n",
			log.strFilename.Value());

	if ( !log.isInitialized || !log.isValid ) {
	    return false;
	}

	ReadUserLog::FileStatus fs = log.readUserLog.CheckFileStatus( );

	if ( ReadUserLog::LOG_STATUS_ERROR == fs ) {
		dprintf( D_FULLDEBUG,
				 "ReadMultipleUserLogs error: can't stat "
				 "condor log (%s): %s\n",
				 log.strFilename.Value(), strerror( errno ) );
		return false;
	}
	bool grew = ( fs != ReadUserLog::LOG_STATUS_NOCHANGE );
    dprintf( D_FULLDEBUG, "ReadMultipleUserLogs: %s\n",
			 grew ? "log GREW!" : "no log growth..." );

	return grew;
}
#endif // !LAZY_LOG_FILES

///////////////////////////////////////////////////////////////////////////////

#if LAZY_LOG_FILES
bool
ReadMultipleUserLogs::LogGrew( LogFileMonitor *monitor )
{
    dprintf( D_FULLDEBUG, "ReadMultipleUserLogs::LogGrew(%s)\n",
				monitor->logFile.Value() );

	ReadUserLog::FileStatus fs = monitor->readUserLog->CheckFileStatus();

	if ( ReadUserLog::LOG_STATUS_ERROR == fs ) {
		dprintf( D_FULLDEBUG,
				 "ReadMultipleUserLogs error: can't stat "
				 "condor log (%s): %s\n",
				 monitor->logFile.Value(), strerror( errno ) );
		return false;
	}
	bool grew = ( fs != ReadUserLog::LOG_STATUS_NOCHANGE );
    dprintf( D_FULLDEBUG, "ReadMultipleUserLogs: %s\n",
			 grew ? "log GREW!" : "no log growth..." );

	return grew;
}
#endif

///////////////////////////////////////////////////////////////////////////////

#if !LAZY_LOG_FILES
ULogEventOutcome
ReadMultipleUserLogs::readEventFromLog(LogFileEntry &log)
{
    dprintf( D_FULLDEBUG, "ReadMultipleUserLogs::readEventFromLog()\n");

	ULogEventOutcome	result;

	result = log.readUserLog.readEvent(log.pLastLogEvent);

   	if ( result == ULOG_OK ) {
			// Check for duplicate logs.  We only need to do that the
			// first time we've successfully read an event from this
			// log.
		if ( !log.haveReadEvent ) {
			log.haveReadEvent = true;

			if ( DuplicateLogExists(log.pLastLogEvent, &log) ) {
				result = ULOG_NO_EVENT;
				log.isValid = FALSE;
			}
		}
	}

	return result;
}
#endif // !LAZY_LOG_FILES

///////////////////////////////////////////////////////////////////////////////

#if LAZY_LOG_FILES
ULogEventOutcome
ReadMultipleUserLogs::readEventFromLog( LogFileMonitor *monitor )
{
	dprintf( D_FULLDEBUG, "ReadMultipleUserLogs::readEventFromLog(%s)\n",
				monitor->logFile.Value() );

	ULogEventOutcome	result =
				monitor->readUserLog->readEvent( monitor->lastLogEvent );

	return result;
}
#endif

///////////////////////////////////////////////////////////////////////////////

MyString
MultiLogFiles::fileNameToLogicalLines(const MyString &filename,
			StringList &logicalLines)
{
	MyString	result("");

	MyString fileContents = readFileToString(filename);
	if (fileContents == "") {
		result = "Unable to read file: " + filename;
		dprintf(D_ALWAYS, "MultiLogFiles: %s\n", result.Value());
		return result;
	}

		// Split the file string into physical lines.
		// Note: StringList constructor removes leading whitespace from lines.
	StringList physicalLines(fileContents.Value(), "\r\n");
	physicalLines.rewind();

		// Combine lines with continuation characters.
	MyString	combineResult = CombineLines(physicalLines, '\\',
				filename, logicalLines);
	if ( combineResult != "" ) {
		result = combineResult;
		return result;
	}
	logicalLines.rewind();

	return result;
}

///////////////////////////////////////////////////////////////////////////////

MyString
MultiLogFiles::readFileToString(const MyString &strFilename)
{
	dprintf( D_FULLDEBUG, "MultiLogFiles::readFileToString(%s)\n",
				strFilename.Value() );

	FILE *pFile = safe_fopen_wrapper(strFilename.Value(), "r");
	if (!pFile) {
		dprintf( D_ALWAYS, "MultiLogFiles::readFileToString: "
				"safe_fopen_wrapper(%s) failed with errno %d (%s)\n", strFilename.Value(),
				errno, strerror(errno) );
		return "";
	}

	if ( fseek(pFile, 0, SEEK_END) != 0 ) {
		dprintf( D_ALWAYS, "MultiLogFiles::readFileToString: "
				"fseek(%s) failed with errno %d (%s)\n", strFilename.Value(),
				errno, strerror(errno) );
		return "";
	}
	int iLength = ftell(pFile);
	if ( iLength == -1 ) {
		dprintf( D_ALWAYS, "MultiLogFiles::readFileToString: "
				"ftell(%s) failed with errno %d (%s)\n", strFilename.Value(),
				errno, strerror(errno) );
		fclose(pFile);
		return "";
	}
	MyString strToReturn;
	strToReturn.reserve_at_least(iLength);

	fseek(pFile, 0, SEEK_SET);
	char *psBuf = new char[iLength+1];
		/*  We now clear the buffer to ensure there will be a NULL at the
			end of our buffer after the fread().  Why no just do
				psBuf[iLength] = 0  ?
			Because on Win32, iLength may not point to the end of 
			the buffer because \r\n are converted into \n because
			the file is opened in text mode.  
		*/
	memset(psBuf,0,iLength+1);
	fread(psBuf, 1, iLength, pFile);
	fclose(pFile);

	strToReturn = psBuf;
	delete [] psBuf;

	return strToReturn;
}

///////////////////////////////////////////////////////////////////////////////
// Note: this method should get speeded up (see Gnats PR 846).

MyString
MultiLogFiles::loadLogFileNameFromSubFile(const MyString &strSubFilename,
		const MyString &directory)
{
	dprintf( D_FULLDEBUG, "MultiLogFiles::loadLogFileNameFromSubFile(%s, %s)\n",
				strSubFilename.Value(), directory.Value() );

	TmpDir		td;
	if ( directory != "" ) {
		MyString	errMsg;
		if ( !td.Cd2TmpDir(directory.Value(), errMsg) ) {
			dprintf(D_ALWAYS, "Error from Cd2TmpDir: %s\n", errMsg.Value());
			return "";
		}
	}

	StringList	logicalLines;
	if ( fileNameToLogicalLines( strSubFilename, logicalLines ) != "" ) {
		return "";
	}

	MyString	logFileName("");
	MyString	initialDir("");

		// Now look through the submit file logical lines to find the
		// log file and initial directory (if specified) and combine
		// them into a path to the log file that's either absolute or
		// relative to the DAG submit directory.
	const char *logicalLine;
	while( (logicalLine = logicalLines.next()) != NULL ) {
		MyString	submitLine(logicalLine);
		MyString	tmpLogName = getParamFromSubmitLine(submitLine, "log");
		if ( tmpLogName != "" ) {
			logFileName = tmpLogName;
		}

		MyString	tmpInitialDir = getParamFromSubmitLine(submitLine,
				"initialdir");
		if ( tmpInitialDir != "" ) {
			initialDir = tmpInitialDir;
		}
	}

		//
		// Check for macros in the log file name -- we currently don't
		// handle those.
		//
	if ( logFileName != "" ) {
		if ( strchr(logFileName.Value(), '$') ) {
			dprintf(D_ALWAYS, "MultiLogFiles: macros not allowed "
						"in log file name in DAG node submit files\n");
			logFileName = "";
		}
	}

	if ( logFileName != "" ) {
			// Prepend initialdir to log file name if log file name is not
			// an absolute path.
		if ( initialDir != "" && !fullpath(logFileName.Value()) ) {
			logFileName = initialDir + DIR_DELIM_STRING + logFileName;
		}

			// We do this in case the same log file is specified with a
			// relative and an absolute path.  
			// Note: we now do further checking that doesn't rely on
			// comparing paths to the log files.  wenger 2004-05-27.
		CondorError errstack;
		if ( !makePathAbsolute( logFileName, errstack ) ) {
			dprintf(D_ALWAYS, "%s\n", errstack.getFullText());
			return "";
		}
	}

	if ( directory != "" ) {
		MyString	errMsg;
		if ( !td.Cd2MainDir(errMsg) ) {
			dprintf(D_ALWAYS, "Error from Cd2MainDir: %s\n", errMsg.Value());
			return "";
		}
	}

	return logFileName;
}

///////////////////////////////////////////////////////////////////////////////

bool
MultiLogFiles::makePathAbsolute(MyString &filename, CondorError &errstack)
{
	if ( !fullpath(filename.Value()) ) {
			// I'd like to use realpath() here, but I'm not sure
			// if that's portable across all platforms.  wenger 2009-01-09.
		MyString	currentDir;
		char	tmpCwd[PATH_MAX];
		if ( getcwd(tmpCwd, PATH_MAX) ) {
			currentDir = tmpCwd;
		} else {
			errstack.pushf( "MultiLogFiles", UTIL_ERR_GET_CWD,
						"ERROR: getcwd() failed with errno %d (%s) at %s:%d\n",
						errno, strerror(errno), __FILE__, __LINE__);
			return false;
		}

		filename = currentDir + DIR_DELIM_STRING + filename;
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////

#ifdef HAVE_EXT_CLASSADS

// Skip whitespace in a std::string buffer.
void
MultiLogFiles::skip_whitespace(std::string const &s,int &offset) {
	while((int)s.size() > offset && isspace(s[offset])) offset++;
};

// Read a file into a std::string.  Return false on error.
MyString
MultiLogFiles::readFile(char const *filename,std::string& buf)
{
    char chunk[4000];
	MyString rtnVal;

	int fd = safe_open_wrapper(filename, O_RDONLY);
	if (fd < 0) {
		rtnVal.sprintf("error opening submit file %s: %s",
				filename, strerror(errno) );
		dprintf(D_ALWAYS, "%s\n", rtnVal.Value() );
		return rtnVal;
	}

    while(1) {
        size_t n = read(fd,chunk,sizeof(chunk)-1);
        if(n>0) {
            chunk[n] = '\0';
            buf += chunk;
        }
        else if(n==0) {
            break;
        }
        else {
            rtnVal.sprintf("failed to read submit file %s: %s",
					filename, strerror(errno) );
			dprintf(D_ALWAYS, "%s\n", rtnVal.Value() );
			close(fd);
			return rtnVal;
        }
    }

	close(fd);
    return rtnVal;
}

// Gets the log files from a Stork submit file.
MyString
MultiLogFiles::loadLogFileNamesFromStorkSubFile(
		const MyString &strSubFilename,
		const MyString &directory,
		StringList &listLogFilenames)
{
	MyString rtnVal;
	MyString path;
	std::string adBuf;
	classad::ClassAdParser parser;
	classad::PrettyPrint unparser;
	std::string unparsed;

	dprintf( D_FULLDEBUG,
			"MultiLogFiles::loadLogFileNamesFromStorkSubFile(%s, %s)\n",
				strSubFilename.Value(), directory.Value() );

	// Construct fully qualified path from directory and log file.
	if ( directory.Length() > 0 ) {
		path = directory + DIR_DELIM_STRING;
	}
	path += strSubFilename;

	// Read submit file into std::string buffer, the native input buffer for
	// the [new] ClassAds parser.
	rtnVal = MultiLogFiles::readFile( path.Value(), adBuf);
	if (rtnVal.Length() > 0 ) {
		return rtnVal;
	}

	// read all classads out of the input file
    int offset = 0;
    classad::ClassAd ad;

	// Loop through the Stork submit file, parsing out one submit job [ClassAd]
	// at a time.
	skip_whitespace(adBuf,offset);  // until the parser can do this itself
    while (parser.ParseClassAd(adBuf, ad, offset) ) {
		std::string logfile;

		// ad now contains the next Stork job ClassAd.  Extract log file, if
		// found.
		if ( ! ad.EvaluateAttrString("log", logfile) ) {
			// no log file specified
			continue;
		}

		// reject empty log file names
		if ( logfile.empty() ) {
			unparser.Unparse( unparsed, &ad);
			rtnVal.sprintf("Stork job specifies null log file:%s",
					unparsed.c_str() );
			return rtnVal;
		}

		// reject log file names with embedded macros
		if ( logfile.find('$') != std::string::npos) {
			unparser.Unparse( unparsed, &ad);
			rtnVal.sprintf("macros not allowed in Stork log file names:%s",
					unparsed.c_str() );
			return rtnVal;
		}

		// All logfile must be fully qualified paths.  Prepend the current
		// working directory if logfile not a fully qualified path.
		if ( ! fullpath(logfile.c_str() ) ) {
			char	tmpCwd[PATH_MAX];
			if ( ! getcwd(tmpCwd, sizeof(tmpCwd) ) ) {
				rtnVal.sprintf("getcwd() failed with errno %d (%s)",
						errno, strerror(errno));
				dprintf(D_ALWAYS, "ERROR: %s at %s:%d\n", rtnVal.Value(),
						__FILE__, __LINE__);
				return rtnVal;
			}
			std::string tmp  = tmpCwd;
			tmp += DIR_DELIM_STRING;
			tmp += logfile;
			logfile = tmp;
		}

		// Add the log file we just found to the log file list
		// (if it's not already in the list -- we don't want
		// duplicates).
		listLogFilenames.rewind();
		char *psLogFilename;
		bool bAlreadyInList = false;
		while ( (psLogFilename = listLogFilenames.next()) ) {
			if (logfile == psLogFilename) {
				bAlreadyInList = true;
			}
		}

		if (!bAlreadyInList) {
				// Note: append copies the string here.
			listLogFilenames.append(logfile.c_str() );
		}

        skip_whitespace(adBuf,offset);	// until the parser can do this itself
    }

	return rtnVal;
}
#endif /* HAVE_EXT_CLASSADS */

///////////////////////////////////////////////////////////////////////////////

int
MultiLogFiles::getQueueCountFromSubmitFile(const MyString &strSubFilename,
			const MyString &directory, MyString &errorMsg)
{
	dprintf( D_FULLDEBUG,
				"MultiLogFiles::getQueueCountFromSubmitFile(%s, %s)\n",
				strSubFilename.Value(), directory.Value() );

	int queueCount = 0;
	errorMsg = "";

	MyString	fullpath("");
	if ( directory != "" ) {
		fullpath = directory + DIR_DELIM_STRING + strSubFilename;
	} else {
		fullpath = strSubFilename;
	}

	StringList	logicalLines;
	if ( (errorMsg = fileNameToLogicalLines( strSubFilename,
				logicalLines)) != "" ) {
		return -1;
	}

		// Now look through the submit file logical lines to find any
		// queue commands, and count up the total number of job procs
		// to be queued.
	const char *	paramName = "queue";
	const char *logicalLine;
	while( (logicalLine = logicalLines.next()) != NULL ) {
		MyString	submitLine(logicalLine);
		submitLine.Tokenize();
		const char *DELIM = " ";
		const char *rawToken = submitLine.GetNextToken( DELIM, true );
		if ( rawToken ) {
			MyString	token(rawToken);
			token.trim();
			if ( !strcasecmp(token.Value(), paramName) ) {
				rawToken = submitLine.GetNextToken( DELIM, true );
				if ( rawToken ) {
					queueCount += atoi( rawToken );
				} else {
					queueCount++;
				}
			}
		}
	}

	return queueCount;
}

///////////////////////////////////////////////////////////////////////////////

#if !LAZY_LOG_FILES
// Note: it would probably make sense to have this method use the new
// getValuesFromFile() method in order to reduce more-or-less duplicate
// code.  However, I'm just leaving it alone for now.  If
// getJobLogsFromSubmitFiles() were to use getValuesFromFile(),
// getValuesFromFile() would have to have another argument to specify
// the number of tokens to skip between the keyword and the value.

MyString
MultiLogFiles::getJobLogsFromSubmitFiles(const MyString &strDagFileName,
            const MyString &jobKeyword, const MyString &dirKeyword,
			StringList &listLogFilenames)
{
	dprintf( D_FULLDEBUG, "MultiLogFiles::getJobLogsFromSubmitFiles(%s)\n",
				strDagFileName.Value() );

	MyString	errorMsg;
	StringList	logicalLines;
	if ( (errorMsg = fileNameToLogicalLines( strDagFileName,
				logicalLines )) != "" ) {
		return errorMsg;
	}

	const char *	logicalLine;
	while ( (logicalLine = logicalLines.next()) ) {

		if ( logicalLine && strcmp(logicalLine, "") ) {

				// Note: StringList constructor removes leading
				// whitespace from lines.
			StringList	tokens(logicalLine, " \t");
			tokens.rewind();

			char *word = tokens.next();
				// Treat a SUBDAG line like a JOB line, unless we're
				// looking for DATA nodes.
			if ( !stricmp(word, jobKeyword.Value()) ||
						(!stricmp(word, "subdag") &&
						stricmp(jobKeyword.Value(), "data")) ) {

				if ( !stricmp(word, "subdag") &&
							stricmp(jobKeyword.Value(), "data") ) {
			   			// Get INLINE or EXTERNAL
               	const char *inlineOrExt = tokens.next();
                	if ( strcasecmp( inlineOrExt, "EXTERNAL" ) ) {
						MyString result = MyString("ERROR: only SUBDAG ") +
									"EXTERNAL is supported at this time " +
									"(line <" + logicalLine + ">)";
                    	dprintf(D_ALWAYS, "%s\n", result.Value());
                    	return result;
                	}
				}

					// Get the node submit file name.
				const char *nodeName = tokens.next();
				const char *submitFile = tokens.next();
				if( !submitFile ) {
					MyString result = "Improperly-formatted DAG file: "
								"submit file missing from job line";
					dprintf(D_ALWAYS, "MultiLogFiles error: %s\n",
								result.Value());
			    	return result;
				}
				MyString strSubFile(submitFile);

					// Deal with nested DAGs specified with the "SUBDAG"
					// keyword.
				if ( !stricmp(word, "subdag") ) {
					strSubFile += ".condor.sub";
				}

				const char *directory = "";
				const char *nextTok = tokens.next();
				if ( nextTok ) {
					if ( !stricmp(nextTok, dirKeyword.Value()) ) {
						directory = tokens.next();
						if ( !directory ) {
							MyString result = "No directory specified "
									"after DIR keyword";
							dprintf(D_ALWAYS, "MultiLogFiles error: %s\n",
									result.Value());
							return result;
						}
					}
				}

					// get the log = value from the sub file
				MyString strLogFilename;
				if ( !stricmp(jobKeyword.Value(), "data") ) {
#ifdef HAVE_EXT_CLASSADS
						// Warning!  For the moment we are only supporting
						// one log file per Stork submit file.
						// wenger 2006-01-17.
					StringList tmpLogFiles;
					MyString tmpResult = loadLogFileNamesFromStorkSubFile(
								strSubFile, directory, tmpLogFiles);
					if ( tmpResult != "" ) return tmpResult;
					tmpLogFiles.rewind();
					strLogFilename = tmpLogFiles.next();
#else
					return "Stork unavailable on this platform because "
								"new classads are not yet supported";
#endif
				} else {
					strLogFilename = loadLogFileNameFromSubFile(
						strSubFile, directory);
					
				}
				if (strLogFilename == "") {
					MyString result = "No 'log =' value found in submit file "
								+ strSubFile + " for node " + nodeName;
					dprintf(D_ALWAYS, "MultiLogFiles: %s\n",
								result.Value());
					return result;
				}
			 
					// Add the log file we just found to the log file list
					// (if it's not already in the list -- we don't want
					// duplicates).
				listLogFilenames.rewind();
				char *psLogFilename;
				bool bAlreadyInList = false;
				while ( (psLogFilename = listLogFilenames.next()) ) {
					if (psLogFilename == strLogFilename) {
						bAlreadyInList = true;
					}
				}

				if (!bAlreadyInList) {
						// Note: append copies the string here.
					listLogFilenames.append(strLogFilename.Value());
				}

			// here we recurse into a splice to discover logfiles that it might
			// bring in.
			} else if ( !stricmp(word, "splice") )  {
				TmpDir spliceDir;
				MyString spliceName = tokens.next();
				MyString spliceDagFile = tokens.next();
				MyString dirTok = tokens.next();
				MyString directory = ".";
				MyString errMsg;

				dirTok.upper_case(); // case insensitive...
				if (dirTok == "DIR") { 
					directory = tokens.next();
					if (directory == "") {
						errorMsg = "Failed to parse DIR directory.";
					}
				}

				dprintf(D_FULLDEBUG, "getJobLogsFromSubmitFiles(): "
					"Processing SPLICE %s %s\n",
					spliceName.Value(), spliceDagFile.Value());
				
				// cd into directory specified by DIR, if any
				if ( !spliceDir.Cd2TmpDir(directory.Value(), errMsg) ) {
					errorMsg = "Unable to chdir into DIR directory.";
				}

				// Find all of the logs entries from the submit files.
				errorMsg = getJobLogsFromSubmitFiles( spliceDagFile, 
					jobKeyword, dirKeyword, listLogFilenames);

				// cd back out
				if ( !spliceDir.Cd2MainDir(errMsg) ) {
					errorMsg = "Unable to chdir back out of DIR directory.";
				}

				if (errorMsg != "") {
					return "Splice[" + spliceName + ":" + 
						spliceDagFile + "]: " + errorMsg;
				}
			}
		}
	}	

	return "";
}
#endif // !LAZY_LOG_FILES

///////////////////////////////////////////////////////////////////////////////

MyString
MultiLogFiles::getValuesFromFile(const MyString &fileName, 
			const MyString &keyword, StringList &values, int skipTokens)
{

	MyString	errorMsg;
	StringList	logicalLines;
	if ( (errorMsg = fileNameToLogicalLines( fileName,
				logicalLines )) != "" ) {
		return errorMsg;
	}

	const char *	logicalLine;
	while ( (logicalLine = logicalLines.next()) ) {

		if ( strcmp(logicalLine, "") ) {

				// Note: StringList constructor removes leading
				// whitespace from lines.
			StringList	tokens(logicalLine, " \t");
			tokens.rewind();

			if ( !stricmp(tokens.next(), keyword.Value()) ) {
					// Skip over unwanted tokens.
				for ( int skipped = 0; skipped < skipTokens; skipped++ ) {
					if ( !tokens.next() ) {
						MyString result = MyString( "Improperly-formatted DAG "
									"file: value missing after keyword <" ) +
									keyword + ">";
			    		return result;
					}
				}

					// Get the value.
				const char *newValue = tokens.next();
				if ( !newValue || !strcmp( newValue, "") ) {
					MyString result = MyString( "Improperly-formatted DAG "
								"file: value missing after keyword <" ) +
								keyword + ">";
			    	return result;
				}

					// Add the value we just found to the values list
					// (if it's not already in the list -- we don't want
					// duplicates).
				values.rewind();
				char *existingValue;
				bool alreadyInList = false;
				while ( (existingValue = values.next()) ) {
					if (!strcmp( existingValue, newValue ) ) {
						alreadyInList = true;
					}
				}

				if (!alreadyInList) {
						// Note: append copies the string here.
					values.append(newValue);
				}
			}
		}
	}	

	return "";
}

///////////////////////////////////////////////////////////////////////////////

MyString
MultiLogFiles::getParamFromSubmitLine(MyString &submitLine,
		const char *paramName)
{
	MyString	paramValue("");

	const char *DELIM = "=";

	submitLine.Tokenize();
	const char *	rawToken = submitLine.GetNextToken(DELIM, true);
	if ( rawToken ) {
		MyString	token(rawToken);
		token.trim();
		if ( !strcasecmp(token.Value(), paramName) ) {
			rawToken = submitLine.GetNextToken(DELIM, true);
			if ( rawToken ) {
				paramValue = rawToken;
				paramValue.trim();
			}
		}
	}

	return paramValue;
}

///////////////////////////////////////////////////////////////////////////////

MyString
MultiLogFiles::CombineLines(StringList &listIn, char continuation,
		const MyString &filename, StringList &listOut)
{
	dprintf( D_FULLDEBUG, "MultiLogFiles::CombineLines(%s, %c)\n",
				filename.Value(), continuation );

	listIn.rewind();

		// Physical line is one line in the file.
	const char	*physicalLine;
	while ( (physicalLine = listIn.next()) != NULL ) {

			// Logical line is physical lines combined as needed by
			// continuation characters (backslash).
		MyString	logicalLine(physicalLine);

		while ( logicalLine[logicalLine.Length()-1] == continuation ) {

				// Remove the continuation character.
			logicalLine.setChar(logicalLine.Length()-1, '\0');

				// Append the next physical line.
			physicalLine = listIn.next();
			if ( physicalLine ) {
				logicalLine += physicalLine;
			} else {
				MyString result = MyString("Improper file syntax: ") +
							"continuation character with no trailing line! (" +
							logicalLine + ") in file " + filename;
				dprintf(D_ALWAYS, "MultiLogFiles: %s\n", result.Value());
				return result;
			}
		}

		listOut.append(logicalLine.Value());
	}

	return ""; // blank means okay
}

///////////////////////////////////////////////////////////////////////////////

#if !LAZY_LOG_FILES
bool
ReadMultipleUserLogs::DuplicateLogExists(ULogEvent *event, LogFileEntry *log)
{
	bool	result = false;

	CondorID	id(event->cluster, event->proc, event->subproc);

	LogFileEntry *	oldLog;
	if ( logHash.lookup(id, oldLog) == 0 ) {
			// We already have an event for this job ID.  See whether
			// the log matches the one we already have.
		if ( log == oldLog ) {
			result = false;
		} else {
			dprintf(D_FULLDEBUG,
					"ReadMultipleUserLogs: found duplicate log\n");
			result = true;
		}
	} else {
			// First event for this job ID.  Insert the given log into
			// the hash table.
		if ( logHash.insert(id, log) != 0 ) {
			dprintf(D_ALWAYS,
					"ReadMultipleUserLogs: hash table insert error");
		}
		result = false;
	}

	return result;
}
#endif // LAZY_LOG_FILES

///////////////////////////////////////////////////////////////////////////////

unsigned int
ReadMultipleUserLogs::hashFuncJobID(const CondorID &key)
{
	int		result = (key._cluster * 29) ^ (key._proc * 7) ^ key._subproc;

		// Make sure we produce a non-negative result (modulus on negative
		// value may produce a negative result (implementation-dependent).
	if ( result < 0 ) result = -result;

	return (unsigned int)result;
}

///////////////////////////////////////////////////////////////////////////////

#if !LAZY_LOG_FILES
bool
MultiLogFiles::logFilesOnNFS(StringList &listLogFileNames, bool nfsIsError)
{
	int fileCount = listLogFileNames.number();
	listLogFileNames.rewind();

	for (int i = 0; i < fileCount; i++) {
		char *logFilename = listLogFileNames.next();

		if (logFileOnNFS(logFilename, nfsIsError)) {
			return true;
		}
	}

	// if we got here, we finished iterating and nothing was found to be 
	// on NFS.
	return false;
}
#endif // !LAZY_LOG_FILES

///////////////////////////////////////////////////////////////////////////////

bool
MultiLogFiles::logFileOnNFS(const char *logFilename, bool nfsIsError)
{

	BOOLEAN isNfs;
   
	if ( fs_detect_nfs( logFilename, &isNfs ) != 0 ) {
		// can't determine if it's on NFS
		dprintf(D_ALWAYS, "WARNING: can't determine whether log file %s "
			"is on NFS.\n", logFilename);

	} else if ( isNfs ) {
		if ( nfsIsError ) {
			dprintf(D_ALWAYS, "ERROR: log file %s is on NFS.\n", logFilename);
			return true;
		} else {
			dprintf(D_FULLDEBUG, "WARNING: log file %s is on NFS.  This "
				"could cause log file corruption and is _not_ recommended.\n",
				logFilename);
		}
	}

	return false;
}

#if LAZY_LOG_FILES
///////////////////////////////////////////////////////////////////////////////

bool
GetInode( const MyString &file, StatStructInode &inode,
			CondorError &errstack )
{
		// Make sure the log file exists.  Even though we may later call
		// InitializeFile(), we have to do it here first so we make sure
		// that the file exists and we can therefore get an inode for it.
		// We *don't* want to truncate the file here, though, because
		// we don't know for sure whether it's the first time we're seeing
		// it.
	if ( access( file.Value(), F_OK ) != 0 ) {
		if ( !MultiLogFiles::InitializeFile( file.Value(),
					false, errstack ) ) {
			errstack.pushf( "ReadMultipleUserLogs", UTIL_ERR_LOG_FILE,
						"Error initializing log file %s", file.Value() );
			return false;
		}
	}

	StatWrapper swrap;
	if ( swrap.Stat( file.Value() ) != 0 ) {
		errstack.pushf( "ReadMultipleUserLogs", UTIL_ERR_LOG_FILE,
					"Error getting inode for log file %s", file.Value() );
		return false;
	}
	inode = swrap.GetBuf()->st_ino;

	return true;
}

///////////////////////////////////////////////////////////////////////////////

// Note: logfile is not passed as a reference because we need a local
// copy to modify anyhow.
bool
ReadMultipleUserLogs::monitorLogFile( MyString logfile,
			bool truncateIfFirst, CondorError &errstack )
{
	dprintf( D_LOG_FILES, "ReadMultipleUserLogs::monitorLogFile(%s, %d)\n",
				logfile.Value(), truncateIfFirst );

//TEMP -- do we even need this anymore now that we're doing things by inode?
		// Make sure path is absolute to reduce the chance of the same
		// file getting referred to by different names (that will cause
		// errors).
	if ( !MultiLogFiles::makePathAbsolute( logfile, errstack ) ) {
		errstack.push( "ReadMultipleUserLogs", UTIL_ERR_LOG_FILE,
					"Error making log file path absolute in "
					"monitorLogFile()\n" );
		return false;
	}

	StatStructInode inode;
	if ( !GetInode( logfile, inode, errstack ) ) {
		errstack.push( "ReadMultipleUserLogs", UTIL_ERR_LOG_FILE,
					"Error getting inode in monitorLogFile()" );
		return false;
	}

	LogFileMonitor *monitor;
	if ( allLogFiles.lookup( inode, monitor ) == 0 ) {
		dprintf( D_LOG_FILES, "ReadMultipleUserLogs: found "
					"LogFileMonitor object for %s (%lu)\n",
					logfile.Value(), inode );

	} else {
		dprintf( D_LOG_FILES, "ReadMultipleUserLogs: didn't "
					"find LogFileMonitor object for %s (%lu)\n",
					logfile.Value(), inode );

			// Make sure the log file is in the correct state -- it must
			// exist, and be truncated if necessary.
		if ( !MultiLogFiles::InitializeFile( logfile.Value(),
					truncateIfFirst, errstack ) ) {
			errstack.pushf( "ReadMultipleUserLogs", UTIL_ERR_LOG_FILE,
						"Error initializing log file %s", logfile.Value() );
			return false;
		}

		monitor = new LogFileMonitor( logfile );
		ASSERT( monitor );
		dprintf( D_LOG_FILES, "ReadMultipleUserLogs: created LogFileMonitor "
					"object for log file %s\n", logfile.Value() );
		if ( allLogFiles.insert( inode, monitor ) != 0 ) {
			errstack.pushf( "ReadMultipleUserLogs", UTIL_ERR_LOG_FILE,
						"Error inserting %s into allLogFiles",
						logfile.Value() );
			return false;
		}
	}

	if ( monitor->refCount < 1 ) {
			// Open the log file (return to previous location if it was
			// opened before).
	
		if ( monitor->state ) {
			monitor->readUserLog = new ReadUserLog( *(monitor->state) );
		} else {
			monitor->readUserLog =
						new ReadUserLog( monitor->logFile.Value() );
		}

			// Workaround for gittrac #337.
		if ( LogGrew( monitor ) ) {
			if ( !monitor->lastLogEvent ) {
					// This will fail unless an event got written after the
					// call to CreateOrTruncateFile() above (race condition).
				(void)monitor->readUserLog->readEvent( monitor->lastLogEvent );
			}
		}

		if ( activeLogFiles.insert( inode, monitor ) != 0 ) {
			errstack.pushf( "ReadMultipleUserLogs", UTIL_ERR_LOG_FILE,
						"Error inserting %s (%lu) into activeLogFiles",
						logfile.Value(), inode );
			return false;
		} else {
			dprintf( D_LOG_FILES, "ReadMultipleUserLogs: added log "
						"file %s (%lu) to active list\n", logfile.Value(),
						inode );
		}
	}

	monitor->refCount++;
	
	return true;
}

///////////////////////////////////////////////////////////////////////////////

// Note: logfile is not passed as a reference because we need a local
// copy to modify anyhow.
bool
ReadMultipleUserLogs::unmonitorLogFile( MyString logfile,
			CondorError &errstack )
{
	dprintf( D_LOG_FILES, "ReadMultipleUserLogs::unmonitorLogFile(%s)\n",
				logfile.Value() );

//TEMP -- do we even need this anymore now that we're doing things by inode?
		// Make sure path is absolute to reduce the chance of the same
		// file getting referred to by different names (that will cause
		// errors).
	if ( !MultiLogFiles::makePathAbsolute( logfile, errstack ) ) {
		errstack.push( "ReadMultipleUserLogs", UTIL_ERR_LOG_FILE,
					"Error making log file path absolute in "
					"monitorLogFile()\n" );
		return false;
	}

	StatStructInode inode;
	if ( !GetInode( logfile, inode, errstack ) ) {
		errstack.push( "ReadMultipleUserLogs", UTIL_ERR_LOG_FILE,
					"Error getting inode in unmonitorLogFile()" );
		return false;
	}

	LogFileMonitor *monitor;
	if ( allLogFiles.lookup( inode, monitor ) == 0 ) {
		dprintf( D_LOG_FILES, "ReadMultipleUserLogs: found "
					"LogFileMonitor object for %s (%lu)\n",
					logfile.Value(), inode );

		monitor->refCount--;

		if ( monitor->refCount < 1 ) {
				// Okay, if we are no longer monitoring this file at all,
				// we need to close it.  We do that by saving its state
				// into a ReadUserLog::FileState object (so we can go back
				// to the right place if we later monitor it again) and
				// then deleting the ReadUserLog object.
			dprintf( D_LOG_FILES, "Closing file <%s>\n", logfile.Value() );

			if ( !monitor->state ) {
				monitor->state = new ReadUserLog::FileState();
				if ( !ReadUserLog::InitFileState( *(monitor->state) ) ) {
					errstack.pushf( "ReadMultipleUserLogs", UTIL_ERR_LOG_FILE,
								"Unable to initialize ReadUserLog::FileState "
								"object for log file %s", logfile.Value() );
					return false;
				}
			}

			if ( !monitor->readUserLog->GetFileState( *(monitor->state) ) ) {
				errstack.pushf( "ReadMultipleUserLogs", UTIL_ERR_LOG_FILE,
							"Error getting state for log file %s",
							logfile.Value() );
				return false;
			}

			delete monitor->readUserLog;
			monitor->readUserLog = NULL;


				// Now we remove this file from the "active" list, so
				// we don't check it the next time we get an event.
			if ( activeLogFiles.remove( inode ) != 0 ) {
				errstack.pushf( "ReadMultipleUserLogs", UTIL_ERR_LOG_FILE,
							"Error removing %s (%lu) from activeLogFiles",
							logfile.Value(), inode );
				return false;
			} else {
				dprintf( D_LOG_FILES, "ReadMultipleUserLogs: removed "
							"log file %s (%lu) from active list\n",
							logfile.Value(), inode );
			}
		}

	} else {
		errstack.pushf( "ReadMultipleUserLogs", UTIL_ERR_LOG_FILE,
					"Didn't find LogFileMonitor object for log "
					"file %s (%lu)!\n", logfile.Value(), inode );
		return false;
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////

void
ReadMultipleUserLogs::printAllLogMonitors( FILE *stream ) const
{
	fprintf( stream, "All log monitors:\n" );
	printLogMonitors( stream, allLogFiles );
}

///////////////////////////////////////////////////////////////////////////////

void
ReadMultipleUserLogs::printActiveLogMonitors( FILE *stream ) const
{
	fprintf( stream, "Active log monitors:\n" );
	printLogMonitors( stream, activeLogFiles );
}

///////////////////////////////////////////////////////////////////////////////

void
ReadMultipleUserLogs::printLogMonitors( FILE *stream,
			HashTable<StatStructInode, LogFileMonitor *> logTable ) const
{
	logTable.startIterations();
	StatStructInode	inode;
	LogFileMonitor *	monitor;
	while ( logTable.iterate( inode,  monitor ) ) {
		fprintf( stream, "  Inode: %lu\n", inode );
		fprintf( stream, "    Monitor: %p\n", monitor );
		fprintf( stream, "    Log file: <%s>\n", monitor->logFile.Value() );
		fprintf( stream, "    refCount: %d\n", monitor->refCount );
		fprintf( stream, "    lastLogEvent: %p\n", monitor->lastLogEvent );
	}
}

#endif // LAZY_LOG_FILES
