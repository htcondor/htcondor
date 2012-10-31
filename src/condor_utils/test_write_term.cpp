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
#include "write_user_log.h"
#include "condor_debug.h"
#include <stdio.h>

static const char *	VERSION = "0.9.2";

enum Status { STATUS_OK, STATUS_CANCEL, STATUS_ERROR };

struct Arguments {
	const char *	logFile;
	int				cluster;
	bool			writeExec;
};

Status
CheckArgs(int argc, char **argv, Arguments &args);

int // 0 == okay, 1 == error
WriteTermEvent(Arguments &args);

int // 0 == okay, 1 == error
WriteExecEvent(Arguments &args);

int
main(int argc, char **argv)
{
		// Set up the dprintf stuff...
	dprintf_set_tool_debug("test_write_term", 0);
	set_debug_flags(NULL, D_ALWAYS);

	int		result = 0;

	Arguments	args;

	Status tmpStatus = CheckArgs(argc, argv, args);

	if ( tmpStatus == STATUS_OK ) {
		result = WriteTermEvent(args);
	} else if ( tmpStatus == STATUS_ERROR ) {
		result = 1;
	}

	if ( args.writeExec && (tmpStatus == STATUS_OK) && (result == 0) ) {
		result = WriteExecEvent(args);
	}

	if ( result != 0 ) {
		fprintf(stderr, "test_write_term FAILED\n");
	}

	return result;
}

Status
CheckArgs(int argc, char **argv, Arguments &args)
{
	Status	status = STATUS_OK;

	const char *	usage = "Usage: test_write_term [options]\n"
			"  -logfile <filename>: the log file to write\n"
			"  -cluster <number>: the cluster part of the job ID\n"
			"  -exec: write execute event after terminated event\n"
			"  -usage: print this message and exit\n"
			"  -version: print the version number and compile date\n";

	args.logFile = NULL;
	args.cluster = -1;
	args.writeExec = false;

	for ( int index = 1; index < argc; ++index ) {
		if ( !strcmp(argv[index], "-cluster") ) {
			if ( ++index >= argc ) {
				fprintf(stderr, "Value needed for -cluster argument\n");
				printf("%s", usage);
				status = STATUS_ERROR;
			} else {
				args.cluster = atoi(argv[index]);
			}

		} else if ( !strcmp(argv[index], "-logfile") ) {
			if ( ++index >= argc ) {
				fprintf(stderr, "Value needed for -logfile argument\n");
				printf("%s", usage);
				status = STATUS_ERROR;
			} else {
				args.logFile = argv[index];
			}

		} else if ( !strcmp(argv[index], "-exec") ) {
			args.writeExec = true;

		} else if ( !strcmp(argv[index], "-usage") ) {
			printf("%s", usage);
			status = STATUS_CANCEL;

		} else if ( !strcmp(argv[index], "-version") ) {
			printf("test_write_term: %s, %s\n", VERSION, __DATE__);
			status = STATUS_CANCEL;

		} else {
			fprintf(stderr, "Unrecognized argument: <%s>\n", argv[index]);
			printf("%s", usage);
			status = STATUS_ERROR;
		}
	}

	if ( status == STATUS_OK && args.logFile == NULL ) {
		fprintf(stderr, "Log file must be specified\n");
		printf("%s", usage);
		status = STATUS_ERROR;
	}

	if ( status == STATUS_OK && args.cluster == -1 ) {
		fprintf(stderr, "Cluster must be specified\n");
		printf("%s", usage);
		status = STATUS_ERROR;
	}

	return status;
}

int
WriteTermEvent(Arguments &args)
{
	int		result = 0;

	UserLog	log("owner", args.logFile, args.cluster, 0, 0, false);

	sleep(10);

		//
		// Write the terminated event.
		//
	printf("Writing terminated event\n");

	JobTerminatedEvent	terminated;
	terminated.normal = true;
	terminated.returnValue = 0;
	terminated.signalNumber = 0;
	terminated.sent_bytes = 1000;
	terminated.recvd_bytes = 2000;
	terminated.total_sent_bytes = 5000;
	terminated.total_recvd_bytes = 7000;

	if ( !log.writeEvent(&terminated) ) {
		fprintf(stderr, "Error writing log event\n");
		result = 1;
	}

	return result;
}

int
WriteExecEvent(Arguments &args)
{
	int		result = 0;

	UserLog	log("owner", args.logFile, args.cluster, 0, 0, false);

	sleep(10);

		//
		// Write the execute event.
		//
	printf("Writing execute event\n");

	ExecuteEvent	execute;
	strcpy( execute.executeHost, "<fake.host.for.test:1234>" );

	if ( !log.writeEvent(&execute) ) {
		fprintf(stderr, "Error writing log event\n");
		result = 1;
	}

	return result;
}
