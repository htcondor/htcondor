/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "condor_debug.h"
#include "read_multiple_logs.h"
#include "condor_string.h" // for strnewp()
#include "tmp_dir.h"
#ifdef WANT_NEW_CLASSADS
#ifndef WANT_NAMESPACES
#define WANT_NAMESPACES
#endif
#include "condor_fix_iostream.h"
#include "classad_distribution.h"
#endif
#include "fs_util.h"

#define LOG_HASH_SIZE 37 // prime

///////////////////////////////////////////////////////////////////////////////

ReadMultipleUserLogs::ReadMultipleUserLogs() :
	logHash(LOG_HASH_SIZE, hashFuncJobID, rejectDuplicateKeys)
{
	pLogFileEntries = NULL;
	iLogFileCount = 0;
}

///////////////////////////////////////////////////////////////////////////////

ReadMultipleUserLogs::ReadMultipleUserLogs(StringList &listLogFileNames) :
	logHash(LOG_HASH_SIZE, hashFuncJobID, rejectDuplicateKeys)
{
	pLogFileEntries = NULL;
	iLogFileCount = 0;
	initialize(listLogFileNames);
}

///////////////////////////////////////////////////////////////////////////////

ReadMultipleUserLogs::~ReadMultipleUserLogs()
{
	cleanup();
}

///////////////////////////////////////////////////////////////////////////////

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

	return ULOG_OK;
}

///////////////////////////////////////////////////////////////////////////////

bool
ReadMultipleUserLogs::detectLogGrowth()
{
    dprintf( D_FULLDEBUG, "ReadMultipleUserLogs::detectLogGrowth()\n");

    initializeUninitializedLogs();

	bool grew = false;

	    // Note: we must go through the whole loop even after we find a
		// log that grew, so we have the right log lengths for next time.
		// wenger 2003-04-11.
    for (int index = 0; index < iLogFileCount; index++) {
	    if (LogGrew(pLogFileEntries[index])) {
		    grew = true;
		}
	}

    return grew;
}

///////////////////////////////////////////////////////////////////////////////

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

///////////////////////////////////////////////////////////////////////////////

void
MultiLogFiles::DeleteLogs(StringList &logFileNames)
{
    logFileNames.rewind();
	char *filename;
	while ( (filename = logFileNames.next()) ) {
        if ( access( filename, F_OK) == 0 ) {
		    dprintf( D_ALWAYS, "MultiLogFiles: deleting older "
						"version of %s\n", filename);
		    if (remove (filename) == -1) {
		        dprintf( D_ALWAYS, "MultiLogFiles error: can't "
							"remove %s\n", filename );
		    }
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

void
ReadMultipleUserLogs::cleanup()
{
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
}

///////////////////////////////////////////////////////////////////////////////

bool
ReadMultipleUserLogs::initializeUninitializedLogs()
{
    bool result = false;

	for (int index = 0; index < iLogFileCount; index++) {
	    LogFileEntry &log = pLogFileEntries[index];
		if (!log.isInitialized) {
		    if (log.readUserLog.initialize(log.strFilename.GetCStr())) {
				log.isInitialized = true;
				log.isValid = true;
			    result = true;
			}
		}
	}

	return result;
}

///////////////////////////////////////////////////////////////////////////////

bool
ReadMultipleUserLogs::LogGrew(LogFileEntry &log)
{
    dprintf( D_FULLDEBUG, "ReadMultipleUserLogs::LogGrew(%s)\n",
			log.strFilename.GetCStr());

	if ( !log.isInitialized || !log.isValid ) {
	    return false;
	}

    int fd = log.readUserLog.getfd();
    ASSERT( fd != -1 );
    struct stat buf;
    
    if( fstat( fd, &buf ) == -1 ) {
		dprintf( D_FULLDEBUG, "ReadMultipleUserLogs error: can't stat "
					"condor log (%s): %s\n",
					  log.strFilename.GetCStr(), strerror( errno ) );
		return false;
	}
    
    int oldSize = log.logSize;
    log.logSize = buf.st_size;
    
    bool grew = (buf.st_size > oldSize);
    dprintf( D_FULLDEBUG, "ReadMultipleUserLogs: %s\n",
				  grew ? "log GREW!" : "no log growth..." );

	return grew;
}

///////////////////////////////////////////////////////////////////////////////

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

	MyString	currentDir;
	char	tmpCwd[PATH_MAX];
	if ( getcwd(tmpCwd, PATH_MAX) ) {
		currentDir = tmpCwd;
	} else {
		dprintf(D_ALWAYS,
				"ERROR: getcwd() failed with errno %d (%s) at %s:%d\n",
				errno, strerror(errno), __FILE__, __LINE__);
		return "";
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
		if ( !fullpath(logFileName.Value()) ) {
			if ( currentDir != "" ) {
				logFileName = currentDir + DIR_DELIM_STRING + logFileName;
			} else {
				dprintf(D_ALWAYS, "MultiLogFiles: unable to get "
							"current directory\n");
				logFileName = "";
			}
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

#ifdef WANT_NEW_CLASSADS

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
#endif /* WANT_NEW_CLASSADS */

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

			if ( !stricmp(tokens.next(), jobKeyword.Value()) ) {

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
#ifdef WANT_NEW_CLASSADS
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
			}
		}
	}	

	return "";
}

///////////////////////////////////////////////////////////////////////////////

MyString
MultiLogFiles::getValuesFromFile(const MyString &fileName, 
			const MyString &keyword, StringList &values)
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
			dprintf(D_FULLDEBUG,
					"ReadMultipleUserLogs: hash table insert error");
		}
		result = false;
	}

	return result;
}

///////////////////////////////////////////////////////////////////////////////

int
ReadMultipleUserLogs::hashFuncJobID(const CondorID &key, int numBuckets)
{
	int		result = (key._cluster * 29) ^ (key._proc * 7) ^ key._subproc;

		// Make sure we produce a non-negative result (modulus on negative
		// value may produce a negative result (implementation-dependent).
	if ( result < 0 ) result = -result;

	result %= numBuckets;

	return result;
}

///////////////////////////////////////////////////////////////////////////////

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


