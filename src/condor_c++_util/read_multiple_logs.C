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
	cleanup();

	iLogFileCount = listLogFileNames.number();
	if (iLogFileCount) {
		pLogFileEntries = new LogFileEntry[iLogFileCount];
	}

	listLogFileNames.rewind();

	for (int i = 0; i < iLogFileCount; i++) {
		char *psFilename = listLogFileNames.next();
		pLogFileEntries[i].pLastLogEvent = NULL;
		pLogFileEntries[i].strFilename = psFilename;
		if (!pLogFileEntries[i].readUserLog.initialize(psFilename)) {
			return false;
		}
	}

	return true;
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
	if (!iLogFileCount) {
		return ULOG_UNK_ERROR;
	}
	int iOldestEventIndex = -1;

	for (int i = 0; i < iLogFileCount; i++) {
		ULogEventOutcome eOutcome = ULOG_OK;
		if (!pLogFileEntries[i].pLastLogEvent) {
			eOutcome = pLogFileEntries[i].readUserLog.readEvent(
					pLogFileEntries[i].pLastLogEvent);
		}

		if (eOutcome == ULOG_RD_ERROR || eOutcome == ULOG_UNK_ERROR) {
			// peter says always return an error immediately,
			// then go on our merry way trying again if they call us again.
			dprintf(D_ALWAYS, "read error on log %s\n",
					pLogFileEntries[i].strFilename.Value());
			return eOutcome;
		}

		if (eOutcome != ULOG_NO_EVENT) {
			if (iOldestEventIndex == -1 || 
				pLogFileEntries[iOldestEventIndex].pLastLogEvent->eventTime >
				pLogFileEntries[i].pLastLogEvent->eventTime) {
				iOldestEventIndex = i;
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
// end of ReadMultipleUserLogs member functions
///////////////////////////////////////////////////////////////////////////////

MyString
makeString(int iValue)
{
	char psToReturn[16];
	sprintf(psToReturn,"%d",iValue);
	return MyString(psToReturn);
}

///////////////////////////////////////////////////////////////////////////////

bool
fileExists(const MyString &strFile)
{
	int fd = open(strFile.Value(), O_RDONLY);
	if (fd == -1) {
		return false;
	}
	close(fd);
	return true;
}

///////////////////////////////////////////////////////////////////////////////

MyString
readFileToString(const MyString &strFilename)
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
	psBuf[iLength] = 0;
	strToReturn = psBuf;
	delete [] psBuf;

	fclose(pFile);
	return strToReturn;
}

///////////////////////////////////////////////////////////////////////////////

MyString
loadLogFileNameFromSubFile(const MyString &strSubFilename)
{
	MyString strSubFile = readFileToString(strSubFilename);
	
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
getJobLogsFromSubmitFiles(StringList &listLogFilenames,
				const MyString &strDagFileName)
{
	MyString strDagFile = readFileToString(strDagFileName);
	if (strDagFile == "") {
		return "Unable to read dag file";
	}

	StringList listLines( strDagFile.Value(), "\n");
	listLines.rewind();
	const char *psLine;
	while( (psLine = listLines.next()) ) {
		MyString strLine = psLine;
		
		// this internal loop is for '/' line continuation
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
		bool bFoundNonWhitespace = false;
		int iEndOfSubFileName = strLine.Length()-1;
		if (!stricmp(strFirstThree.Value(), "job") ||
				!stricmp(strFirstThree.Value(), "dap")) {
			int iLastWhitespace = strLine.Length();
			int iPos;
			for (iPos = strLine.Length()-1; iPos >= 0; iPos--) {
				if (strLine[iPos] == '\t' || strLine[iPos] == ' ') {	
					if (!bFoundNonWhitespace) {
						iEndOfSubFileName--;
						continue;
					}

					iLastWhitespace = iPos;
					break;
				} else {
					bFoundNonWhitespace = true;
				}
			}

			MyString strSubFile = strLine.Substr(iLastWhitespace+1,
								iEndOfSubFileName);

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
				listLogFilenames.append(strLogFilename.Value());
			}
		}
	}	

	return "";
}
