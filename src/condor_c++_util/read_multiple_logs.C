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

#include "condor_common.h"
#include "condor_debug.h"
#include "read_multiple_logs.h"

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
	
	// Note: StringList constructor removes leading whitespace from lines.
	StringList listLines( strSubFile.Value(), "\r\n");

	listLines.rewind();
	const char *psLine;
	MyString strPreviousLogFilename;
	while( (psLine = listLines.next()) ) {
		MyString strLine = psLine;
		if (!stricmp(strLine.Substr(0, 2).Value(), "log")) {
			int iEqPos = strLine.FindChar('=',0);
			if (iEqPos == -1) {
				return "";
			}

			iEqPos++;
			while (iEqPos < strLine.Length() && (strLine[iEqPos] == ' ' ||
					strLine[iEqPos] == '\t')) {
				iEqPos++;
			}

			if (iEqPos >= strLine.Length()) {
				return "";
			}

			MyString strToReturn = strLine.Substr(iEqPos, strLine.Length());

			return strToReturn;
		}
	}
	return "";
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

	StringList listLines( strDagFileContents.Value(), "\r\n");
	listLines.rewind();
	const char *psLine;
	while( (psLine = listLines.next()) ) {
		MyString strLine = psLine;
		
		// this internal loop is for '\' line continuation
		while (strLine[strLine.Length()-1] == '\\') {
			strLine[strLine.Length()-1] = 0;
			psLine = listLines.next();
			if (psLine) {
				strLine += psLine;
			}
		}

		if (strLine.Length() <= 3) {
			continue;
		}

		MyString strFirstThree = strLine.Substr(0, 2);
		if (!stricmp(strFirstThree.Value(), jobKeyword.Value())) {

                // Format of a job line should be:
				// JOB <name> <script> [DONE]
			StringList tokens (strLine.Value(), " \t");
			tokens.rewind();
			tokens.next(); // Skip JOB
			tokens.next(); // Skip <name>
			const char *submitFile = tokens.next();
			if( !submitFile ) {
			    return "Improperly-formatted DAG file";
			}
			MyString strSubFile(submitFile);


			// get the log= value from the sub file

			MyString strLogFilename = loadLogFileNameFromSubFile(strSubFile);

			if (strLogFilename == "") {
				return "No 'log =' found in submit file " + strSubFile;
			}
			
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

	return "";
}
