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
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <time.h>
#include <assert.h>
#include "user_log.c++.h"
#include "string_list.h"
#include "read_multiple_logs.h"

static const char * VERSION = "0.9.1";

MULTI_LOG_HASH_INSTANCE; // For the multi-log-file code...

enum Status { STATUS_OK, STATUS_CANCEL, STATUS_ERROR };

static ReadMultipleUserLogs	reader;
static int		verbosity = 0;

Status CheckArgs(int argc, char **argv);
bool CompareStringLists(StringList &reference, StringList &test);
bool GetGoodLogFiles(StringList &logFiles);
bool GetBadLogFiles();
bool ReadEvents(StringList &logFiles);
bool ReadAndTestEvent(ULogEvent *expectedEvent);
void PrintEvent(ULogEvent *event);

int main(int argc, char **argv)
{
		// Set up the dprintf stuff...
	Termlog = true;
	dprintf_config("test_multi_log", 2);
	DebugFlags = D_ALWAYS;

	int		result = 0;

	Status tmpStatus = CheckArgs(argc, argv);
	if ( tmpStatus == STATUS_CANCEL ) {
		return 0;
	} else if ( tmpStatus == STATUS_ERROR ) {
		return 1;
	}

	if ( !GetBadLogFiles() ) {
		result = 1;
	}

	StringList		logFiles;

	if ( !GetGoodLogFiles(logFiles) ) {
		result = 1;
	}

	if ( !ReadEvents(logFiles) ) {
		result = 1;
	}

	if ( result == 0 ) {
		printf("\nTest succeeded\n");
	} else {
		printf("\nTest FAILED !!!!!!!!!!!!!!!!!!!!!!!!\n");
	}

	return result;
}



Status
CheckArgs(int argc, char **argv)
{
	Status	status = STATUS_OK;

	const char *	usage = "Usage: test_multi_log [options]\n"
			"  -debug <level>: debug level (e.g., D_FULLDEBUG)\n"
			"  -usage: print this message and exit\n"
			"  -verbosity <number>: set verbosity level (default is 0)\n"
			"  -version: print the version number and compile date\n";

	for ( int index = 1; index < argc; ++index ) {
		if ( !strcmp(argv[index], "-debug") ) {
			if ( ++index >= argc ) {
				fprintf(stderr, "Value needed for -debug argument\n");
				printf("%s", usage);
				status = STATUS_ERROR;
			} else {
				set_debug_flags( argv[index] );
			}

		} else if ( !strcmp(argv[index], "-usage") ) {
			printf("%s", usage);
			status = STATUS_CANCEL;

		} else if ( !strcmp(argv[index], "-verbosity") ) {
			if ( ++index >= argc ) {
				fprintf(stderr, "Value needed for -verbosity argument\n");
				printf("%s", usage);
				status = STATUS_ERROR;
			} else {
				verbosity = atoi(argv[index]);
			}
		} else if ( !strcmp(argv[index], "-version") ) {
			printf("test_multi_log: %s, %s\n", VERSION, __DATE__);
			status = STATUS_CANCEL;

		} else {
			fprintf(stderr, "Unrecognized argument: <%s>\n", argv[index]);
			printf("%s", usage);
			status = STATUS_ERROR;
		}
	}

	return status;
}

// true == okay; false == error
bool
GetGoodLogFiles(StringList &logFiles)
{
	bool		isOkay = true;

	printf("Getting log files...\n");
	MyString		errorMsg;
	MultiLogFiles	mlf;
	errorMsg = mlf.getJobLogsFromSubmitFiles(
			"test_multi_log1.dag", "job", "dir", logFiles);
	if ( errorMsg != "" ) {
		printf("...error getting log files: %s", errorMsg.Value());
		printf(" (at %s: %d)\n", __FILE__, __LINE__);
		isOkay = false;
	} else {
		printf("...succeeded\n");
	}

	if ( verbosity >= 1 ) {
		printf("Log files:\n");
		logFiles.print();
	}

	char currentDir[PATH_MAX];
	if ( !getcwd(currentDir, PATH_MAX) ) {
		fprintf(stderr, "Buffer is too short for getcwd()");
		printf(" (at %s: %d)\n", __FILE__, __LINE__);
		isOkay = false;
	}
	MyString	logPath;
	StringList	reference;
	logPath = MyString(currentDir) + "/test_multi_log.log1";
	reference.append(logPath.Value());
	logPath = MyString(currentDir) + "/../condor_c++_util/test_multi_log.log2";
	reference.append(logPath.Value());
	logPath = MyString(currentDir) + "/test_multi_log.log3";
	reference.append(logPath.Value());
	logPath = MyString(currentDir) + "/../condor_c++_util/test_multi_log.log3";
	reference.append(logPath.Value());

	if ( !CompareStringLists(reference, logFiles) ) {
		isOkay = false;
	}

	return isOkay;
}


// true == okay; false == error
bool
GetBadLogFiles()
{
	bool		isOkay = true;

	StringList	logFiles;

	printf("Getting log files...\n");
	MyString		errorMsg;
	MultiLogFiles	mlf;
	errorMsg = mlf.getJobLogsFromSubmitFiles(
			"test_multi_log2.dag", "job", "dir", logFiles);
	if ( errorMsg != "" ) {
		printf("...got expected error (%s)\n", errorMsg.Value());
	} else {
		printf("...should have gotten an error (test failed)");
		printf(" (at %s: %d)\n", __FILE__, __LINE__);
		isOkay = false;
	}

	if ( verbosity >= 1 ) {
		printf("Log files:\n");
		logFiles.print();
	}

	printf("Getting log files...\n");
	errorMsg = mlf.getJobLogsFromSubmitFiles(
			"test_multi_log3.dag", "job", "dir", logFiles);
	if ( errorMsg != "" ) {
		printf("...got expected error (%s)\n", errorMsg.Value());
	} else {
		printf("...should have gotten an error (test failed)");
		printf(" (at %s: %d)\n", __FILE__, __LINE__);
		isOkay = false;
	}

	if ( verbosity >= 1 ) {
		printf("Log files:\n");
		logFiles.print();
	}

	return isOkay;
}

// true == okay; false == error
bool
CompareStringLists(StringList &reference, StringList &test)
{
	bool	isOkay = true;

	if ( reference.number() != test.number() ) {
		printf("Error: expected %d strings, got %d", reference.number(),
				test.number());
		printf(" (at %s: %d)\n", __FILE__, __LINE__);
		isOkay = false;
	}

	reference.rewind();
	char	*str;
	while ( (str = reference.next()) != NULL ) {
		if ( !test.contains(str) ) {
			printf("Test list should contain <%s> but does not", str);
			printf(" (at %s: %d)\n", __FILE__, __LINE__);
			isOkay = false;
		}
	}

	return isOkay;
}

// true == okay; false == error
bool
ReadEvents(StringList &logFiles)
{
	bool	isOkay = true;

	MultiLogFiles::DeleteLogs(logFiles);

	// Note: return value of false is okay here because log files are
	// empty.
	if ( !reader.initialize(logFiles) ) {
		if ( verbosity >= 1 ) printf("reader.initialize() returns false\n");
	} else {
		if ( verbosity >= 1 ) printf("reader.initialize() returns true\n");
	}

	printf("Testing detectLogGrowth() on empty files...\n");
	if ( !reader.detectLogGrowth() ) {
		printf("...succeeded\n");
	} else {
		printf("...failed");
		printf(" (at %s: %d)\n", __FILE__, __LINE__);
		isOkay = false;
	}

	ULogEvent	*event;
	printf("Testing readEvent() on empty files...\n");
	if ( reader.readEvent(event) != ULOG_NO_EVENT ) {
		printf("...failed");
		printf(" (at %s: %d)\n", __FILE__, __LINE__);
		isOkay = false;
	} else {
		printf("...succeeded\n");
	}

	printf("Testing writing and reading events...\n");

	UserLog		log1("test", "test_multi_log.log1", 1, 0, 0, false);
	UserLog		log2("test", "test_multi_log.log2", 2, 0, 0, true);
	UserLog		log3("test", "test_multi_log.log3", 3, 0, 0, false);
	UserLog		log4("test", "test_multi_log.log3", 4, 0, 0, false);

	SubmitEvent	subE;
	strcpy(subE.submitHost, "<128.105.165.12:32779>");
	if ( !log1.writeEvent(&subE) ) {
		printf("Error: writeEvent() failed");
		printf(" (at %s: %d)\n", __FILE__, __LINE__);
		isOkay = false;
	}

	if ( !reader.detectLogGrowth() ) {
		printf("Error: should have gotten log growth");
		printf(" (at %s: %d)\n", __FILE__, __LINE__);
		isOkay = false;
	}

	subE.cluster = 1;
	if ( !ReadAndTestEvent(&subE) ) {
		isOkay = false;
	}

	if ( reader.detectLogGrowth() ) {
		printf("Error: should NOT have gotten log growth");
		printf(" (at %s: %d)\n", __FILE__, __LINE__);
		isOkay = false;
	}

	JobTerminatedEvent	termE;
	if ( !log1.writeEvent(&termE) ) {
		printf("Error: writeEvent() failed");
		printf(" (at %s: %d)\n", __FILE__, __LINE__);
		isOkay = false;
	}

	termE.cluster = 1;
	if ( !ReadAndTestEvent(&termE) ) {
		isOkay = false;
	}

	if ( !log2.writeEvent(&subE) ) {
		printf("Error: writeEvent() failed");
		printf(" (at %s: %d)\n", __FILE__, __LINE__);
		isOkay = false;
	}
		// Sleep to make event order deterministic, to simplify the test.
	sleep(2);
	if ( !log3.writeEvent(&subE) ) {
		printf("Error: writeEvent() failed");
		printf(" (at %s: %d)\n", __FILE__, __LINE__);
		isOkay = false;
	}

	if ( !reader.detectLogGrowth() ) {
		printf("Error: should have gotten log growth");
		printf(" (at %s: %d)\n", __FILE__, __LINE__);
		isOkay = false;
	}

	subE.cluster = 2;
	if ( !ReadAndTestEvent(&subE) ) {
		isOkay = false;
	}

	subE.cluster = 3;
	if ( !ReadAndTestEvent(&subE) ) {
		isOkay = false;
	}

	if ( !ReadAndTestEvent(NULL) ) {
		isOkay = false;
	}

	if ( !log2.writeEvent(&termE) ) {
		printf("Error: writeEvent() failed");
		printf(" (at %s: %d)\n", __FILE__, __LINE__);
		isOkay = false;
	}
		// Sleep to make event order deterministic, to simplify the test.
	sleep(2);
	if ( !log3.writeEvent(&termE) ) {
		printf("Error: writeEvent() failed");
		printf(" (at %s: %d)\n", __FILE__, __LINE__);
		isOkay = false;
	}

	termE.cluster = 2;
	if ( !ReadAndTestEvent(&termE) ) {
		isOkay = false;
	}

	termE.cluster = 3;
	if ( !ReadAndTestEvent(&termE) ) {
		isOkay = false;
	}

	if ( !ReadAndTestEvent(NULL) ) {
		isOkay = false;
	}

	if ( !log4.writeEvent(&subE) ) {
		printf("Error: writeEvent() failed");
		printf(" (at %s: %d)\n", __FILE__, __LINE__);
		isOkay = false;
	}

	subE.cluster = 4;
	if ( !ReadAndTestEvent(&subE) ) {
		isOkay = false;
	}

	if ( !ReadAndTestEvent(NULL) ) {
		isOkay = false;
	}

	if ( !log4.writeEvent(&termE) ) {
		printf("Error: writeEvent() failed");
		printf(" (at %s: %d)\n", __FILE__, __LINE__);
		isOkay = false;
	}

	termE.cluster = 4;
	if ( !ReadAndTestEvent(&termE) ) {
		isOkay = false;
	}

	if ( !ReadAndTestEvent(NULL) ) {
		isOkay = false;
	}

	if ( isOkay ) {
		printf("...succeeded\n");
	} else {
		printf("...failed\n");
	}

	return isOkay;
}

bool
ReadAndTestEvent(ULogEvent *expectedEvent)
{
	bool	isOkay = true;

	ULogEvent	*event;
	if ( reader.readEvent(event) != ULOG_OK ) {
		if ( expectedEvent ) {
			printf("Error reading event");
			printf(" (at %s: %d)\n", __FILE__, __LINE__);
			isOkay = false;
		}
	} else {
		if ( verbosity >= 1 ) PrintEvent(event);

		if ( expectedEvent ) {
			if ( expectedEvent->eventNumber != event->eventNumber ||
					expectedEvent->cluster != event->cluster ||
					expectedEvent->proc != event->proc ||
					expectedEvent->subproc != event->subproc ) {
				printf("Event read does not match expected event");
				printf(" (at %s: %d)\n", __FILE__, __LINE__);
				printf("  Expected event: ");
				PrintEvent(expectedEvent);
				printf("  Event read: ");
				PrintEvent(event);
				isOkay = false;
			}
		} else {
			printf("Error: should NOT have gotten an event");
			printf(" (at %s: %d)\n", __FILE__, __LINE__);
			isOkay = false;
		}
	}

	if ( !isOkay ) {
		printf("  Expecting ");
		PrintEvent(expectedEvent);
	}

	return isOkay;
}

void
PrintEvent(ULogEvent *event)
{
	if ( event ) {
		printf("Event: %d.%d.%d: %s\n", event->cluster, event->proc,
				event->subproc,ULogEventNumberNames[event->eventNumber]);
	} else {
		printf("Event: NULL\n");
	}
}
