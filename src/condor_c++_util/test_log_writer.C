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
#include "user_log.c++.h"
#include "condor_debug.h"
#include <stdio.h>

static const char *	VERSION = "0.9.1";

enum Status { STATUS_OK, STATUS_CANCEL, STATUS_ERROR };

enum Verbosity { VERB_NONE = 0, VERB_ERROR, VERB_WARNING, VERB_ALL };

struct Arguments {
	bool			isXml;
	const char *	logFile;
	int				numExec;
	int				sleep;
	int				verbosity;
};

Status
CheckArgs(int argc, char **argv, Arguments &args);

int // 0 == okay, 1 == error
WriteEvents(Arguments &args);

int
main(int argc, char **argv)
{
		// Set up the dprintf stuff...
	Termlog = true;
	dprintf_config("test_log_writer", 2);
	DebugFlags = D_ALWAYS;

	int		result = 0;

	Arguments	args;

	Status tmpStatus = CheckArgs(argc, argv, args);

	if ( tmpStatus == STATUS_OK ) {
		result = WriteEvents(args);
	} else if ( tmpStatus == STATUS_ERROR ) {
		result = 1;
	}

	if ( result != 0 && args.verbosity >= VERB_ERROR ) {
		fprintf(stderr, "test_log_writer FAILED\n");
	}

	return result;
}

Status
CheckArgs(int argc, char **argv, Arguments &args)
{
	Status	status = STATUS_OK;

	const char *	usage = "Usage: test_log_writer [options]\n"
			"  -debug <level>: debug level (e.g., D_FULLDEBUG)\n"
			"  -logfile <filename>: the log file to write\n"
			"  -numexec <number>: number of execute events to write\n"
			"  -sleep <number>: how many seconds to sleep between events\n"
			"  -usage: print this message and exit\n"
			"  -verbosity <number>: set verbosity level (default is 1)\n"
			"  -version: print the version number and compile date\n"
			"  -xml: write the log in XML\n";

	args.isXml = false;
	args.logFile = NULL;
	args.numExec = 10;
	args.sleep = 5;
	args.verbosity = 1;

	for ( int index = 1; index < argc; ++index ) {
		if ( !strcmp(argv[index], "-debug") ) {
			if ( ++index >= argc ) {
				fprintf(stderr, "Value needed for -debug argument\n");
				printf("%s", usage);
				status = STATUS_ERROR;
			} else {
				set_debug_flags( argv[index] );
			}

		} else if ( !strcmp(argv[index], "-logfile") ) {
			if ( ++index >= argc ) {
				fprintf(stderr, "Value needed for -logfile argument\n");
				printf("%s", usage);
				status = STATUS_ERROR;
			} else {
				args.logFile = argv[index];
			}

		} else if ( !strcmp(argv[index], "-numexec") ) {
			if ( ++index >= argc ) {
				fprintf(stderr, "Value needed for -numexec argument\n");
				printf("%s", usage);
				status = STATUS_ERROR;
			} else {
				args.numExec = atoi(argv[index]);
			}

		} else if ( !strcmp(argv[index], "-sleep") ) {
			if ( ++index >= argc ) {
				fprintf(stderr, "Value needed for -sleep argument\n");
				printf("%s", usage);
				status = STATUS_ERROR;
			} else {
				args.sleep = atoi(argv[index]);
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
				args.verbosity = atoi(argv[index]);
			}

		} else if ( !strcmp(argv[index], "-version") ) {
			printf("test_log_writer: %s, %s\n", VERSION, __DATE__);
			status = STATUS_CANCEL;

		} else if ( !strcmp(argv[index], "-xml") ) {
			args.isXml = true;

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

	return status;
}

int
WriteEvents(Arguments &args)
{
	int		result = 0;

	UserLog	log("owner", args.logFile, getpid(), 0, 0, args.isXml);

		//
		// Write the submit event.
		//
	if ( args.verbosity >= VERB_ALL ) {
		printf("Writing submit event\n");
	}
	SubmitEvent	submit;
	strcpy(submit.submitHost, "<128.105.165.12:32779>");
		// Note: the line below is designed specifically to work with
		// Kent's dummy stork_submit script for testing DAGs with
		// DATA nodes.
	submit.submitEventLogNotes = strdup("DAG Node: B");
	submit.submitEventUserNotes = strdup("User info");

	if ( !log.writeEvent(&submit) ) {
		if ( args.verbosity >= VERB_ERROR ) {
			fprintf(stderr, "Error writing log event\n");
		}
		result = 1;
	}


		//
		// Write execute events.
		//
	for ( int event = 0; event < args.numExec; ++event ) {
		if ( args.verbosity >= VERB_ALL ) {
			printf("Writing execute event\n");
		}
		ExecuteEvent	execute;
		strcpy(execute.executeHost, "<128.105.165.12:32779>");

		if ( !log.writeEvent(&execute) ) {
			if ( args.verbosity >= VERB_ERROR ) {
				fprintf(stderr, "Error writing log event\n");
			}
			result = 1;
		}

		sleep(args.sleep);
	}


		//
		// Write the terminated event.
		//
	if ( args.verbosity >= VERB_ALL ) {
		printf("Writing terminated event\n");
	}
	JobTerminatedEvent	terminated;
	terminated.normal = true;
	terminated.returnValue = 0;
	terminated.signalNumber = 0;
	terminated.sent_bytes = 1000;
	terminated.recvd_bytes = 2000;
	terminated.total_sent_bytes = 5000;
	terminated.total_recvd_bytes = 7000;

	if ( !log.writeEvent(&terminated) ) {
		if ( args.verbosity >= VERB_ERROR ) {
			fprintf(stderr, "Error writing log event\n");
		}
		result = 1;
	}

	return result;
}
