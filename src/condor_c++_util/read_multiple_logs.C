/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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

///////////////////////////////////////////////////////////////////////////////

ReadMultipleUserLogs::ReadMultipleUserLogs()
{
	pLogFileEntries = NULL;
	iLogFileCount = 0;
}

///////////////////////////////////////////////////////////////////////////////

ReadMultipleUserLogs::ReadMultipleUserLogs(StringList &listLogFileNames)
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

	for (int i = 0; i < iLogFileCount; i++) {
		LogFileEntry &log = pLogFileEntries[i];
		if (log.isInitialized) {
		    ULogEventOutcome eOutcome = ULOG_OK;
		    if (!log.pLastLogEvent) {
			    eOutcome = log.readUserLog.readEvent(log.pLastLogEvent);

		        if (eOutcome == ULOG_RD_ERROR || eOutcome == ULOG_UNK_ERROR) {
			        // peter says always return an error immediately,
			        // then go on our merry way trying again if they
					// call us again.
			        dprintf(D_ALWAYS, "read error on log %s\n",
					    log.strFilename.Value());
			        return eOutcome;
		        }
		    }

		    if (eOutcome != ULOG_NO_EVENT) {
			    if (iOldestEventIndex == -1 || 
				        (pLogFileEntries[iOldestEventIndex].pLastLogEvent->eventTime >
				        log.pLastLogEvent->eventTime)) {
				    iOldestEventIndex = i;
			    }
		    }
		}
	}

	if (iOldestEventIndex == -1) {
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

void
ReadMultipleUserLogs::DeleteLogs(StringList &logFileNames)
{
    logFileNames.rewind();
	char *filename;
	while ( (filename = logFileNames.next()) ) {
        if ( access( filename, F_OK) == 0 ) {
		    dprintf( D_ALWAYS, "Deleting older version of %s\n", filename);
		    if (remove (filename) == -1) {
		        dprintf( D_ALWAYS, "Error: can't remove %s\n", filename );
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

	if (!log.isInitialized) {
	    return false;
	}

    int fd = log.readUserLog.getfd();
    ASSERT( fd != -1 );
    struct stat buf;
    
    if( fstat( fd, &buf ) == -1 ) {
		dprintf( D_FULLDEBUG, "ERROR: can't stat condor log (%s): %s\n",
					  log.strFilename.GetCStr(), strerror( errno ) );
		return false;
	}
    
    int oldSize = log.logSize;
    log.logSize = buf.st_size;
    
    bool grew = (buf.st_size > oldSize);
    dprintf( D_FULLDEBUG, "%s\n",
				  grew ? "Log GREW!" : "No log growth..." );

	return grew;
}

///////////////////////////////////////////////////////////////////////////////

MyString
ReadMultipleUserLogs::readFileToString(const MyString &strFilename)
{
	FILE *pFile = fopen(strFilename.Value(), "r");
	if (!pFile) {
		return "";
	}

	fseek(pFile, 0, SEEK_END);
	int iLength = ftell(pFile);
	MyString strToReturn;
	strToReturn.reserve_at_least(iLength+1);

	fseek(pFile, 0, SEEK_SET);
	char *psBuf = new char[iLength+1];
	fread(psBuf, 1, iLength, pFile);
	fclose(pFile);

	psBuf[iLength] = 0;
	strToReturn = psBuf;
	delete [] psBuf;

	return strToReturn;
}

///////////////////////////////////////////////////////////////////////////////

MyString
ReadMultipleUserLogs::loadLogFileNameFromSubFile(const MyString &strSubFilename)
{
	MyString strSubFile = readFileToString(strSubFilename);
	
		// Split the node submit file string into lines.
		// Note: StringList constructor removes leading whitespace from lines.
	StringList physicalLines( strSubFile.Value(), "\r\n");
	physicalLines.rewind();

		// Combine lines with continuation characters.
	StringList	logicalLines;
	MyString	combineResult = CombineLines(physicalLines, '\\', logicalLines);
	if ( combineResult != "" ) {
		return combineResult;
	}
	logicalLines.rewind();

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

		// Prepend initialdir to log file name if log file name is not
		// an absolute path.
	if ( initialDir != "" && logFileName[0] != '/' ) {
		logFileName = initialDir + "/" + logFileName;
	}

	return logFileName;
}

///////////////////////////////////////////////////////////////////////////////

MyString
ReadMultipleUserLogs::getJobLogsFromSubmitFiles(const MyString &strDagFileName,
            const MyString &jobKeyword, StringList &listLogFilenames)
{
	MyString strDagFileContents = readFileToString(strDagFileName);
	if (strDagFileContents == "") {
		return "Unable to read DAG file";
	}

		// Split the DAG submit file string into lines.
		// Note: StringList constructor removes leading whitespace from lines.
	StringList physicalLines(strDagFileContents.Value(), "\r\n");
	physicalLines.rewind();

		// Combine lines with continuation characters.
	StringList	logicalLines;
	MyString	combineResult = CombineLines(physicalLines, '\\', logicalLines);
	if ( combineResult != "" ) {
		return combineResult;
	}
	logicalLines.rewind();

	const char *	logicalLine;
	while ( (logicalLine = logicalLines.next()) ) {

		if ( logicalLine && strcmp(logicalLine, "") ) {

				// Note: StringList constructor removes leading
				// whitespace from lines.
			StringList	tokens(logicalLine, " \t");
			tokens.rewind();

			if ( !stricmp(tokens.next(), jobKeyword.Value()) ) {

					// Get the node submit file name.
				tokens.next(); // Skip <name>
				const char *submitFile = tokens.next();
				if( !submitFile ) {
			    	return "Improperly-formatted DAG file: submit file "
							"missing from job line";
				}
				MyString strSubFile(submitFile);

					// get the log = value from the sub file
				MyString strLogFilename = loadLogFileNameFromSubFile(strSubFile);
				if (strLogFilename == "") {
					return "No 'log =' value found in submit file " +
							strSubFile;
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
ReadMultipleUserLogs::getParamFromSubmitLine(MyString submitLine,
		const char *paramName)
{
	MyString	logFileName("");

		// We need to copy the string here because strtok modifies its
		// argument...
	char *		lineBuf = strnewp(submitLine.Value());
	const char *DELIM = " \t";

	char *		token = strtok(lineBuf, DELIM);
	if ( token && !strcasecmp(token, paramName) ) {
		token = strtok(NULL, DELIM);
		if ( token && !strcasecmp(token, "=") ) {
			token = strtok(NULL, DELIM);
			if ( token ) {
				logFileName = token;
			}
		}
	}

	delete [] lineBuf;

	return logFileName;
}

///////////////////////////////////////////////////////////////////////////////

MyString
ReadMultipleUserLogs::CombineLines(StringList &listIn, char continuation,
		StringList &listOut)
{
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
				return "Improper DAG file: continuation character "
						"with no trailing line!";
			}
		}

		listOut.append(logicalLine.Value());
	}

	return ""; // blank means okay
}
