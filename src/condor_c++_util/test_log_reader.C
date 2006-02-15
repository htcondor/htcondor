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
#include "user_log.c++.h"
#include "condor_debug.h"
#include <stdio.h>

static const char *	VERSION = "0.9.1";

enum Status { STATUS_OK, STATUS_CANCEL, STATUS_ERROR };

enum Verbosity { VERB_NONE = 0, VERB_ERROR, VERB_WARNING, VERB_ALL };

struct Arguments {
	const char *	logFile;
	int				maxExec;
	int				sleep;
	int				term;
	int				verbosity;
};

Status
CheckArgs(int argc, char **argv, Arguments &args);

int // 0 == okay, 1 == error
ReadEvents(Arguments &args);

int
main(int argc, char **argv)
{
		// Set up the dprintf stuff...
	Termlog = true;
	dprintf_config("test_log_reader", 2);
	DebugFlags = D_ALWAYS;

	int		result = 0;

	Arguments	args;

	Status tmpStatus = CheckArgs(argc, argv, args);

	if ( tmpStatus == STATUS_OK ) {
		result = ReadEvents(args);
	} else if ( tmpStatus == STATUS_ERROR ) {
		result = 1;
	}

	if ( result != 0 && args.verbosity >= VERB_ERROR ) {
		fprintf(stderr, "test_log_reader FAILED\n");
	}

	return result;
}

Status
CheckArgs(int argc, char **argv, Arguments &args)
{
	Status	status = STATUS_OK;

	const char *	usage = "Usage: test_log_reader [options]\n"
			"  -debug <level>: debug level (e.g., D_FULLDEBUG)\n"
			"  -logfile <filename>: the log file to write\n"
			"  -maxexec <number>: maximum number of execute events to read\n"
			"  -sleep <number>: how many seconds to sleep between events\n"
			"  -term <number>: number of terminate events to exit after\n"
			"  -usage: print this message and exit\n"
			"  -verbosity <number>: set verbosity level (default is 1)\n"
			"  -version: print the version number and compile date\n";

	args.logFile = NULL;
	args.maxExec = 0;
	args.sleep = 5;
	args.term = 1;
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

		} else if ( !strcmp(argv[index], "-maxexec") ) {
			if ( ++index >= argc ) {
				fprintf(stderr, "Value needed for -maxexec argument\n");
				printf("%s", usage);
				status = STATUS_ERROR;
			} else {
				args.maxExec = atoi(argv[index]);
			}

		} else if ( !strcmp(argv[index], "-sleep") ) {
			if ( ++index >= argc ) {
				fprintf(stderr, "Value needed for -sleep argument\n");
				printf("%s", usage);
				status = STATUS_ERROR;
			} else {
				args.sleep = atoi(argv[index]);
			}

		} else if ( !strcmp(argv[index], "-term") ) {
			if ( ++index >= argc ) {
				fprintf(stderr, "Value needed for -term argument\n");
				printf("%s", usage);
				status = STATUS_ERROR;
			} else {
				args.term = atoi(argv[index]);
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
			printf("test_log_reader: %s, %s\n", VERSION, __DATE__);
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

	return status;
}

int
ReadEvents(Arguments &args)
{
	int		result = 0;

	ReadUserLog	log(args.logFile);

	int		executeEventCount = 0;
	int		terminatedEventCount = 0;
	bool	done = false;

	while ( !done ) {
		ULogEvent	*event = NULL;

		ULogEventOutcome	outcome = log.readEvent(event);
		if ( outcome == ULOG_OK ) {
			if ( args.verbosity >= VERB_ALL ) {
				printf("Got an event from %d.%d.%d", event->cluster,
						event->proc, event->subproc);
			}

			switch ( event->eventNumber ) {
			case ULOG_SUBMIT:
				if ( args.verbosity >= VERB_ALL ) {
					printf(" (submit)\n");
				}
				break;

			case ULOG_EXECUTE:
				if ( args.verbosity >= VERB_ALL ) {
					printf(" (execute)\n");
				}
				if ( args.maxExec != 0 &&
						++executeEventCount > args.maxExec ) {
					if ( args.verbosity >= VERB_ERROR ) {
						fprintf(stderr, "Maximum number of execute "
								"events (%d) exceeded\n", args.maxExec);
					}
					result = 1;
					done = true;
				}
				break;

			case ULOG_JOB_TERMINATED:
				if ( args.verbosity >= VERB_ALL ) {
					printf(" (terminated)\n");
				}
				if ( args.term != 0 && ++terminatedEventCount >= args.term ) {
					if ( args.verbosity >= VERB_ALL ) {
						printf("Reached terminated event limit (%d); "
								"exiting\n", args.term);
					}
					done = true;
				}
				break;

			default:
				if ( args.verbosity >= VERB_ALL ) {
					printf(" (other)\n");
				}
				break;

			}

		} else if ( outcome == ULOG_NO_EVENT ) {
			sleep(args.sleep);

		} else if ( outcome == ULOG_RD_ERROR || outcome == ULOG_UNK_ERROR ) {
			if ( args.verbosity >= VERB_ERROR ) {
				fprintf(stderr, "Error reading event\n");
			}
			result = 1;
		}

		delete event;
	}

	return result;
}
