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
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <time.h>
#include <assert.h>
#include "write_user_log.h"
#include "read_multiple_logs.h"

static const char * VERSION = "0.9.3";

enum Status { STATUS_OK, STATUS_CANCEL, STATUS_ERROR };

static int		verbosity = 0;

Status CheckArgs(int argc, char **argv);
bool GetBadLogFiles();
bool ReadEventsLazy();
bool monitorLogFile( ReadMultipleUserLogs &reader, const char *logfile,
			bool truncateIfFirst );
bool unmonitorLogFile( ReadMultipleUserLogs &reader, const char *logfile );
bool ReadAndTestEvent(ReadMultipleUserLogs &reader, ULogEvent *expectedEvent);
void PrintEvent(ULogEvent *event);

int main(int argc, char **argv)
{
		// Set up the dprintf stuff...
	dprintf_set_tool_debug("test_multi_log", 0);
	set_debug_flags(NULL, D_ALWAYS);

	int		result = 0;

	Status tmpStatus = CheckArgs(argc, argv);
	if ( tmpStatus == STATUS_CANCEL ) {
		return 0;
	} else if ( tmpStatus == STATUS_ERROR ) {
		return 1;
	}

	printf("Testing multi-log reading code\n");
	fflush(stdout);

	if ( !ReadEventsLazy() ) {
		result = 1;
	}

	if ( result == 0 ) {
		printf("\nTest succeeded\n");
	} else {
		printf("\nTest FAILED !!!!!!!!!!!!!!!!!!!!!!!!\n");
	}
	fflush(stdout);

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
				set_debug_flags( argv[index], 0 );
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

// Note: we should add something here to test a set of log files that
// has files on more than one file system.  wenger 2009-07-16.

// true == okay; false == error
bool
ReadEventsLazy()
{
	bool	isOkay = true;

	printf("\nReading events with lazy log file evaluation\n");
	fflush(stdout);

	const char *file1 = "test_multi_log.log1";
	unlink( file1 );
	const char *file2 = "test_multi_log.log2";
	unlink( file2 );
	const char *file3 = "test_multi_log.log3";
	unlink( file3 );
	const char *file4 = file3;
	unlink( file4 );
	const char *file5 = "test_multi_log.log5";
	unlink( file5 );
		// Different path to same file -- make sure we deal with that
		// correctly.
	const char *file5a = "./test_multi_log.log5";
	unlink( file5a );
		// This will be a sym link.
	const char *file5b = "test_multi_log.log5b";
	unlink( file5b );
		// This will be a sym link.
	const char *file5c = "test_multi_log.log5c";
	unlink( file5c );
		// These will be a hard link.
	const char *file5d = "test_multi_log.log5d";
	unlink( file5d );

	ReadMultipleUserLogs lazyReader;
	size_t totalLogCount;
	if ( (totalLogCount = lazyReader.totalLogFileCount()) != 0 ) {
		printf( "FAILURE at %s, %d: ", __FILE__, __LINE__ );
		printf( "lazyReader.totalLogFileCount() was %zu; should "
					"have been 0\n", totalLogCount );
		isOkay = false;
	}

	if ( !monitorLogFile( lazyReader, file1, true ) ) {
		printf( "FAILURE at %s, %d: ", __FILE__, __LINE__ );
		printf( "error monitoring log file %s\n", file1 );
		isOkay = false;
	}
	if ( (totalLogCount = lazyReader.totalLogFileCount()) != 1 ) {
		printf( "FAILURE at %s, %d: ", __FILE__, __LINE__ );
		printf( "lazyReader.totalLogFileCount() was %zu; should "
					"have been 1\n", totalLogCount );
		isOkay = false;
	}

		// --------------------------------------------------------------
		// Make sure we don't see log growth or events on empty log files.
		//
	printf("Testing detectLogGrowth() on empty files...\n");
	if ( !lazyReader.detectLogGrowth() ) {
		printf("...succeeded\n");
		fflush(stdout);
	} else {
		printf( "...FAILURE at %s, %d\n", __FILE__, __LINE__ );
		fflush(stdout);
		isOkay = false;
	}

	ULogEvent	*event;
	printf("Testing readEvent() on empty files...\n");
	if ( lazyReader.readEvent(event) != ULOG_NO_EVENT ) {
		printf( "...FAILURE at %s, %d\n", __FILE__, __LINE__ );
		fflush(stdout);
		isOkay = false;
	} else {
		printf("...succeeded\n");
		fflush(stdout);
	}

	printf("Testing writing and reading events...\n");
	fflush(stdout);

	WriteUserLog		log1("test", file1, 1, 0, 0, false);
	WriteUserLog		log2("test", file2, 2, 0, 0, true);
	WriteUserLog		log3("test", file3, 3, 0, 0, false);
	WriteUserLog		log4("test", file4, 4, 0, 0, false);

	SubmitEvent	subE;
	strcpy(subE.submitHost, "<128.105.165.12:32779>");
	if ( !log1.writeEvent(&subE) ) {
		printf( "FAILURE at %s, %d: ", __FILE__, __LINE__ );
		printf( "writeEvent() failed" );
		fflush( stdout );
		isOkay = false;
	}

	if ( !lazyReader.detectLogGrowth() ) {
		printf( "FAILURE at %s, %d: ", __FILE__, __LINE__ );
		printf( "should have gotten log growth" );
		fflush( stdout );
		isOkay = false;
	}

	subE.cluster = 1;
	if ( !ReadAndTestEvent(lazyReader, &subE) ) {
		isOkay = false;
	}

	if ( lazyReader.detectLogGrowth() ) {
		printf( "FAILURE at %s, %d: ", __FILE__, __LINE__ );
		printf( "should NOT have gotten log growth" );
		fflush( stdout );
		isOkay = false;
	}

	JobTerminatedEvent	termE;
	termE.normal = true;
	termE.returnValue = 0;
	if ( !log1.writeEvent(&termE) ) {
		printf( "FAILURE at %s, %d: ", __FILE__, __LINE__ );
		printf( "writeEvent() failed" );
		fflush( stdout );
		isOkay = false;
	}

	termE.cluster = 1;
	if ( !ReadAndTestEvent(lazyReader, &termE) ) {
		isOkay = false;
	}

		//
		// Make sure truncating works.
		//
	if ( !log2.writeEvent(&subE) ) {
		printf( "FAILURE at %s, %d: ", __FILE__, __LINE__ );
		printf( "writeEvent() failed" );
		fflush( stdout );
		isOkay = false;
	}

	if (!monitorLogFile( lazyReader, file2, true ) ) {
		isOkay = false;
	}

	if ( !ReadAndTestEvent( lazyReader, NULL ) ) {
		isOkay = false;
	}

		//
		// Unmonitoring a file we haven't monitored yet should fail.
		//
	if ( unmonitorLogFile( lazyReader, file3 ) ) {
		printf( "FAILURE at %s, %d: ", __FILE__, __LINE__ );
		printf( "Unmonitoring file %s should have failed\n", file3 );
		fflush( stdout );
		isOkay = false;
	}

		//
		// Make sure monitoring the same file several times works okay.
		//
	if (!monitorLogFile( lazyReader, file3, true ) ) {
		isOkay = false;
	}
	if (!monitorLogFile( lazyReader, file3, true ) ) {
		isOkay = false;
	}
	if (!monitorLogFile( lazyReader, file3, true ) ) {
		isOkay = false;
	}
	if (!monitorLogFile( lazyReader, file3, true ) ) {
		isOkay = false;
	}
	if ( (totalLogCount = lazyReader.totalLogFileCount()) != 3 ) {
		printf( "FAILURE at %s, %d: ", __FILE__, __LINE__ );
		printf( "lazyReader.totalLogFileCount() was %zu; should "
					"have been 3\n", totalLogCount );
		fflush( stdout );
		isOkay = false;
	}

	lazyReader.printAllLogMonitors( stdout );

	if ( !log2.writeEvent(&subE) ) {
		printf( "FAILURE at %s, %d: ", __FILE__, __LINE__ );
		printf( "writeEvent() failed" );
		fflush( stdout );
		isOkay = false;
	}
		// Sleep to make event order deterministic, to simplify the test.
	sleep(2);
	if ( !log3.writeEvent(&subE) ) {
		printf( "FAILURE at %s, %d: ", __FILE__, __LINE__ );
		printf( "writeEvent() failed" );
		fflush( stdout );
		isOkay = false;
	}

	if ( !lazyReader.detectLogGrowth() ) {
		printf( "FAILURE at %s, %d: ", __FILE__, __LINE__ );
		printf( "should have gotten log growth" );
		fflush( stdout );
		isOkay = false;
	}

	subE.cluster = 2;
	if ( !ReadAndTestEvent(lazyReader, &subE) ) {
		isOkay = false;
	}

	subE.cluster = 3;
	if ( !ReadAndTestEvent(lazyReader, &subE) ) {
		isOkay = false;
	}

	if ( !ReadAndTestEvent(lazyReader, NULL) ) {
		isOkay = false;
	}

	if ( !log2.writeEvent(&termE) ) {
		printf( "FAILURE at %s, %d: ", __FILE__, __LINE__ );
		printf( "writeEvent() failed" );
		fflush( stdout );
		isOkay = false;
	}
		// Sleep to make event order deterministic, to simplify the test.
	sleep(2);
	if ( !log3.writeEvent(&termE) ) {
		printf( "FAILURE at %s, %d: ", __FILE__, __LINE__ );
		printf( "writeEvent() failed" );
		fflush( stdout );
		isOkay = false;
	}

	termE.cluster = 2;
	if ( !ReadAndTestEvent(lazyReader, &termE) ) {
		isOkay = false;
	}

	termE.cluster = 3;
	if ( !ReadAndTestEvent(lazyReader, &termE) ) {
		isOkay = false;
	}

	if ( !ReadAndTestEvent(lazyReader, NULL) ) {
		isOkay = false;
	}

	if (!unmonitorLogFile( lazyReader, file3 ) ) {
		isOkay = false;
	}
	if (!unmonitorLogFile( lazyReader, file3 ) ) {
		isOkay = false;
	}
	if (!unmonitorLogFile( lazyReader, file3 ) ) {
		isOkay = false;
	}
	if (!unmonitorLogFile( lazyReader, file3 ) ) {
		isOkay = false;
	}
	if ( (totalLogCount = lazyReader.totalLogFileCount()) != 3 ) {
		printf( "FAILURE at %s, %d: ", __FILE__, __LINE__ );
		printf( "lazyReader.totalLogFileCount() was %zu; should "
					"have been 3\n", totalLogCount );
		fflush( stdout );
		isOkay = false;
	}

	lazyReader.printActiveLogMonitors( stdout );

	if (!monitorLogFile( lazyReader, file4, true ) ) {
		isOkay = false;
	}
	if ( !log4.writeEvent(&subE) ) {
		printf( "FAILURE at %s, %d: ", __FILE__, __LINE__ );
		printf( "writeEvent() failed" );
		fflush( stdout );
		isOkay = false;
	}

	subE.cluster = 4;
	if ( !ReadAndTestEvent(lazyReader, &subE) ) {
		isOkay = false;
	}

	if ( !ReadAndTestEvent(lazyReader, NULL) ) {
		isOkay = false;
	}

	if ( !log4.writeEvent(&termE) ) {
		printf( "FAILURE at %s, %d: ", __FILE__, __LINE__ );
		printf( "writeEvent() failed" );
		fflush( stdout );
		isOkay = false;
	}

	termE.cluster = 4;
	if ( !ReadAndTestEvent(lazyReader, &termE) ) {
		isOkay = false;
	}

	if ( !ReadAndTestEvent(lazyReader, NULL) ) {
		isOkay = false;
	}

		//
		// Make sure things work with truncateIfFirst == false; also
		// monitoring the same file with two different paths.
		//
	unlink( file5 );
	WriteUserLog		log5("test", file5, 5, -1, -1, false);
	if (!monitorLogFile( lazyReader, file5, false ) ) {
		printf( "FAILURE at %s, %d: ", __FILE__, __LINE__ );
		printf( "error unmonitoring log file %s\n", file5 );
		fflush( stdout );
		isOkay = false;
	}
	if (!monitorLogFile( lazyReader, file5a, false ) ) {
		printf( "FAILURE at %s, %d: ", __FILE__, __LINE__ );
		printf( "error monitoring log file %s\n", file5a );
		fflush( stdout );
		isOkay = false;
	}

	if ( !log5.writeEvent(&subE) ) {
		printf( "FAILURE at %s, %d: ", __FILE__, __LINE__ );
		printf( "writeEvent() failed" );
		fflush( stdout );
		isOkay = false;
	}

	ExecuteEvent	execE;
	strcpy(execE.executeHost, "<128.105.666.99:12345>");
	if ( !log5.writeEvent(&execE) ) {
		printf( "FAILURE at %s, %d: ", __FILE__, __LINE__ );
		printf( "writeEvent() failed" );
		fflush( stdout );
		isOkay = false;
	}

	if ( !ReadAndTestEvent(lazyReader, &subE) ) {
		isOkay = false;
	}
	if ( !ReadAndTestEvent(lazyReader, &execE) ) {
		isOkay = false;
	}
	if ( !ReadAndTestEvent(lazyReader, NULL) ) {
		isOkay = false;
	}

	GenericEvent	genE;
	strcpy(genE.info, "job type: transfer");
	if ( !log5.writeEvent(&genE) ) {
		printf( "FAILURE at %s, %d: ", __FILE__, __LINE__ );
		printf( "writeEvent() failed" );
		fflush( stdout );
		isOkay = false;
	}
	if ( !ReadAndTestEvent(lazyReader, &genE) ) {
		isOkay = false;
	}

	strcpy(genE.info, "src_url: file:/dev/null");
	if ( !log5.writeEvent(&genE) ) {
		printf( "FAILURE at %s, %d: ", __FILE__, __LINE__ );
		printf( "writeEvent() failed" );
		fflush( stdout );
		isOkay = false;
	}
	if ( !ReadAndTestEvent(lazyReader, &genE) ) {
		isOkay = false;
	}

	strcpy(genE.info, "dest_url: file:/dev/null");
	if ( !log5.writeEvent(&genE) ) {
		printf( "FAILURE at %s, %d: ", __FILE__, __LINE__ );
		printf( "writeEvent() failed" );
		fflush( stdout );
		isOkay = false;
	}
	if ( !ReadAndTestEvent(lazyReader, &genE) ) {
		isOkay = false;
	}

	if ( !log5.writeEvent(&termE) ) {
		printf( "FAILURE at %s, %d: ", __FILE__, __LINE__ );
		printf( "writeEvent() failed" );
		fflush( stdout );
		isOkay = false;
	}
	if ( !ReadAndTestEvent(lazyReader, &termE) ) {
		isOkay = false;
	}

		// --------------------------------------------------------------
		// Make sure we *don't* see events on an unmonitored file, but that
		// we *do* see them after we monitor the file again.
		//
	if (!unmonitorLogFile( lazyReader, file5 ) ) {
		isOkay = false;
	}
		//TEMP -- what if we *don't* unmonitor and remonitor file5a here?  that's even a harder case...
	if (!unmonitorLogFile( lazyReader, file5a ) ) {
		printf( "FAILURE at %s, %d: ", __FILE__, __LINE__ );
		printf( "error unmonitoring file %s\n", file5a );
		fflush( stdout );
		isOkay = false;
	}

	if ( !log5.initialize( 6, 0, 0, NULL )) {
		printf( "FAILURE at %s, %d: ", __FILE__, __LINE__ );
		printf( "error re-initializing log5\n" );
		fflush( stdout );
		isOkay = false;
	}

	if ( !log5.writeEvent(&subE) ) {
		printf( "FAILURE at %s, %d: ", __FILE__, __LINE__ );
		printf( "writeEvent() failed" );
		fflush( stdout );
		isOkay = false;
	}

	strcpy(execE.executeHost, "<128.105.777.99:12345>");
	if ( !log5.writeEvent(&execE) ) {
		printf( "FAILURE at %s, %d: ", __FILE__, __LINE__ );
		printf( "writeEvent() failed" );
		fflush( stdout );
		isOkay = false;
	}

	if ( !log5.writeEvent(&termE) ) {
		printf( "FAILURE at %s, %d: ", __FILE__, __LINE__ );
		printf( "writeEvent() failed" );
		fflush( stdout );
		isOkay = false;
	}

	if ( !ReadAndTestEvent( lazyReader, NULL ) ) {
		isOkay = false;
	}

		// Note: monitoring the "secondary" path to this log.
	if (!monitorLogFile( lazyReader, file5a, true ) ) {
		printf( "FAILURE at %s, %d: ", __FILE__, __LINE__ );
		printf( "error monitoring file %s\n", file5a );
		fflush( stdout );
		isOkay = false;
	}

	if ( !ReadAndTestEvent( lazyReader, &subE ) ) {
		isOkay = false;
	}

		// Test links to log files.
	if ( symlink( file5, file5b ) != 0 ) {
		printf( "FAILURE at %s, %d: ", __FILE__, __LINE__ );
		printf( "error (%d, %s) creating symlink %s %s\n", errno,
					strerror( errno ), file5, file5b );
		isOkay = false;
	} else {
		printf( "Created link %s to %s\n", file5b, file5 );
	}
		// Note: monitoring the sym link to this log.
	if (!monitorLogFile( lazyReader, file5b, true ) ) {
		printf( "FAILURE at %s, %d: ", __FILE__, __LINE__ );
		printf( "error monitoring file %s\n", file5b );
		fflush( stdout );
		isOkay = false;
	}

	if ( symlink( file5, file5c ) != 0 ) {
		printf( "FAILURE at %s, %d: ", __FILE__, __LINE__ );
		printf( "error (%d, %s) creating symlink %s %s\n", errno,
					strerror( errno ), file5, file5c );
		isOkay = false;
	} else {
		printf( "Created link %s to %s\n", file5c, file5 );
	}
		// Note: monitoring the sym link to this log.
	if (!monitorLogFile( lazyReader, file5c, true ) ) {
		printf( "FAILURE at %s, %d: ", __FILE__, __LINE__ );
		printf( "error monitoring file %s\n", file5c );
		fflush( stdout );
		isOkay = false;
	}

		// Note: this test will fail on Windows (see the function
		// GetFileID() in read_multiple_logs.cpp).
	link( file5, file5d );
		// Note: monitoring the hard link to this log.
	if (!monitorLogFile( lazyReader, file5d, true ) ) {
		printf( "FAILURE at %s, %d: ", __FILE__, __LINE__ );
		printf( "error monitoring file %s\n", file5d );
		fflush( stdout );
		isOkay = false;
	}

	if ( (totalLogCount = lazyReader.totalLogFileCount()) != 4 ) {
		printf( "FAILURE at %s, %d: ", __FILE__, __LINE__ );
		printf( "lazyReader.totalLogFileCount() was %zu; should "
					"have been 4\n", totalLogCount );
		fflush( stdout );
		isOkay = false;
	}

	if ( !ReadAndTestEvent( lazyReader, &execE ) ) {
		isOkay = false;
	}
	if ( !ReadAndTestEvent(lazyReader, &termE) ) {
		isOkay = false;
	}
	if ( !ReadAndTestEvent(lazyReader, NULL) ) {
		isOkay = false;
	}

		//
		// Make sure monitoring and then immediately unmonitoring a file
		// doesn't mess things up.
		//
	const char *file6 = "test_multi_log.log6";
	unlink( file6 );

	if (!monitorLogFile( lazyReader, file6, true ) ) {
		printf( "FAILURE at %s, %d: ", __FILE__, __LINE__ );
		printf( "error monitoring file %s\n", file6 );
		fflush( stdout );
		isOkay = false;
	}
	if (!unmonitorLogFile( lazyReader, file6 ) ) {
		printf( "FAILURE at %s, %d: ", __FILE__, __LINE__ );
		printf( "error unmonitoring file %s\n", file6 );
		fflush( stdout );
		isOkay = false;
	}


	if ( isOkay ) {
		printf("...succeeded\n");
		fflush(stdout);
	} else {
		printf("...failed\n");
		fflush(stdout);
	}

	printf("\nNote: we should get a warning here about log files still "
				"being monitored...\n");

	return isOkay;
}

// true == okay; false == error
bool
monitorLogFile( ReadMultipleUserLogs &reader, const char *logfile,
			bool truncateIfFirst )
{
	CondorError errstack;
	if ( !reader.monitorLogFile( logfile, truncateIfFirst, errstack ) ) {
		printf( "Error monitoring log file %s: %s\n", logfile,
					errstack.getFullText().c_str() );
		return false;
	}

	return true;
}

// true == okay; false == error
bool
unmonitorLogFile( ReadMultipleUserLogs &reader, const char *logfile )
{
	CondorError errstack;
	if ( !reader.unmonitorLogFile( logfile, errstack ) ) {
		printf( "Error unmonitoring log file %s: %s\n", logfile,
					errstack.getFullText().c_str() );
		return false;
	}

	return true;
}

// true == okay; false == error
bool
ReadAndTestEvent(ReadMultipleUserLogs &reader, ULogEvent *expectedEvent)
{
	bool	isOkay = true;

	ULogEvent	*event = NULL;
	if ( reader.readEvent(event) != ULOG_OK ) {
		if ( expectedEvent ) {
		printf( "FAILURE at %s, %d: ", __FILE__, __LINE__ );
			printf("FAIL: error reading event");
			printf(" (at %s: %d)\n", __FILE__, __LINE__);
			fflush(stdout);
			isOkay = false;
		}
	} else {
		if ( verbosity >= 1 ) PrintEvent(event);

		if ( expectedEvent ) {
			if ( expectedEvent->eventNumber != event->eventNumber ||
					expectedEvent->cluster != event->cluster ||
					expectedEvent->proc != event->proc ||
					expectedEvent->subproc != event->subproc ) {
		printf( "FAILURE at %s, %d: ", __FILE__, __LINE__ );
				printf("FAIL: event read does not match expected event");
				printf(" (at %s: %d)\n", __FILE__, __LINE__);
				printf("  Expected event: ");
				PrintEvent(expectedEvent);
				printf("  Event read: ");
				PrintEvent(event);
				fflush(stdout);
				isOkay = false;
			}
		} else {
		printf( "FAILURE at %s, %d: ", __FILE__, __LINE__ );
			printf("FAIL: should NOT have gotten an event");
			printf(" (at %s: %d)\n", __FILE__, __LINE__);
			fflush(stdout);
			isOkay = false;
		}
	}

	if ( !isOkay ) {
		printf("  Expecting ");
		PrintEvent(expectedEvent);
		printf("; got ");
		PrintEvent(event);
		printf("\n");
		fflush(stdout);
	}

	delete event;

	return isOkay;
}

void
PrintEvent(ULogEvent *event)
{
	if ( event ) {
		printf("Event: %d.%d.%d: %s", event->cluster, event->proc,
				event->subproc,event->eventName());
		fflush(stdout);
	} else {
		printf("Event: NULL\n");
		fflush(stdout);
	}
}
