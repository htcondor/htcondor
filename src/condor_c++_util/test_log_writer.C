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
#include "condor_distribution.h"
#include "user_log.c++.h"
#include "condor_config.h"
#include "condor_debug.h"
#include <stdio.h>

static const char *	VERSION = "0.9.2";

enum Status { STATUS_OK, STATUS_CANCEL, STATUS_ERROR };

enum Verbosity { VERB_NONE = 0, VERB_ERROR, VERB_WARNING, VERB_ALL };

struct Arguments {
	bool			isXml;
	const char *	logFile;
	int				numExec;
	int				sleep_seconds;
	int				sleep_useconds;
	int				cluster;
	int				proc;
	int				subproc;
	int				numProcs;
	bool			stork;
	const char *	submitNote;
	int				verbosity;
	const char *	genericEventStr;
	const char *    persistFile;
};

char*	mySubSystem = "TEST_LOG_WRITER";

Status
CheckArgs(int argc, char **argv, Arguments &args);

int // 0 == okay, 1 == error
WriteEvents(Arguments &args, int cluster, int proc, int subproc );

const char *timestr( void );

// Simple term signal handler
static bool	global_done = false;
void handle_sig(int sig)
{
	(void) sig;
	printf( "Got signal; shutting down\n" );
	global_done = true;
}

int
main(int argc, char **argv)
{

		// initialize to read from config file
	myDistro->Init( argc, argv );
	config();

		// Set up the dprintf stuff...
	Termlog = true;
	dprintf_config("test_log_writer");
	DebugFlags = D_ALWAYS;

	int		result = 0;

	Arguments	args;

	Status tmpStatus = CheckArgs(argc, argv, args);

	if ( tmpStatus == STATUS_OK ) {
		int		max_proc = args.proc + args.numProcs - 1;
		for( int proc = args.proc; proc <= max_proc; proc++ ) {
			result = WriteEvents(args, args.cluster, proc, 0 );
			if ( result || global_done ) {
				break;
			}
		}
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

	const char *	usage =
		"Usage: test_log_writer [options] <filename>\n"
		"  -cluster <number>: Use cluster %d (default = getpid())\n"
		"  -debug <level>: debug level (e.g., D_FULLDEBUG)\n"
		"  -generic <string>: Write generic event\n"
		"  -jobid <c.p.s>: combined -cluster, -proc, -subproc\n"
		"  -numexec <number>: number of execute events to write / proc\n"
		"  -numprocs <number>: Number of procs (default = 10)\n"
		"  -persist <file>: persist writer state to file (for jobid gen)\n"
		"  -proc <number>: Use proc %d (default = 0)\n"
		"  -sleep <number>: how many seconds to sleep between events\n"
		"  -stork: simulate Stork (-1 for proc and subproc)\n"
		"  -submit_note <string>: submit event note\n"
		"  -subproc <number>: Use subproc %d (default = 0)\n"
		"  -usage: print this message and exit\n"
		"  -v: increase verbose level by 1\n"
		"  -verbosity <number>: set verbosity level (default is 1)\n"
		"  -version: print the version number and compile date\n"
		"  -xml: write the log in XML\n"
		"  <filename>: the log file to write to\n";

	args.isXml			= false;
	args.logFile		= NULL;
	args.numExec		= 1;
	args.cluster		= -1;
	args.proc			= -1;
	args.subproc		= -1;
	args.numProcs		= 10;
	args.sleep_seconds	= 5;
	args.sleep_useconds	= 0;
	args.stork			= false;
	args.submitNote		= "";
	args.verbosity		= 1;
	args.genericEventStr = NULL;
	args.persistFile	= NULL;

	for ( int index = 1; index < argc; ++index ) {
		if ( !strcmp(argv[index], "-cluster") ) {
			if ( ++index >= argc ) {
				fprintf(stderr, "Value needed for -cluster argument\n");
				printf("%s", usage);
				status = STATUS_ERROR;
			} else {
				args.cluster = atoi( argv[index] );
			}

		} else if ( !strcmp(argv[index], "-debug") ) {
			if ( ++index >= argc ) {
				fprintf(stderr, "Value needed for -debug argument\n");
				printf("%s", usage);
				status = STATUS_ERROR;
			} else {
				set_debug_flags( argv[index] );
			}

		} else if ( !strcmp(argv[index], "-jobid") ) {
			if ( ++index >= argc ) {
				fprintf(stderr, "Value needed for -jobid argument\n");
				printf("%s", usage);
				status = STATUS_ERROR;
			} else {
				if ( *argv[index] == '.' ) {
					sscanf( argv[index], ".%d.%d", 
							&args.proc, &args.subproc );
				}
				else {
					sscanf( argv[index], "%d.%d.%d",
							&args.cluster, &args.proc, &args.subproc );
				}
			}

		} else if ( !strcmp(argv[index], "-generic") ) {
			if ( ++index >= argc ) {
				fprintf(stderr, "Value needed for -generic argument\n");
				printf("%s", usage);
				status = STATUS_ERROR;
			} else {
				args.genericEventStr = argv[index];
			}

		} else if ( !strcmp(argv[index], "-numexec") ) {
			if ( ++index >= argc ) {
				fprintf(stderr, "Value needed for -numexec argument\n");
				printf("%s", usage);
				status = STATUS_ERROR;
			} else {
				args.numExec = atoi(argv[index]);
			}

		} else if ( !strcmp(argv[index], "-numprocs") ) {
			if ( ++index >= argc ) {
				fprintf(stderr, "Value needed for -numprocs argument\n");
				printf("%s", usage);
				status = STATUS_ERROR;
			} else {
				args.numProcs = atoi(argv[index]);
			}

		} else if ( !strcmp(argv[index], "-proc") ) {
			if ( ++index >= argc ) {
				fprintf(stderr, "Value needed for -proc argument\n");
				printf("%s", usage);
				status = STATUS_ERROR;
			} else {
				args.proc = atoi(argv[index]);
			}

		} else if ( !strcmp(argv[index], "-persist") ) {
			if ( ++index >= argc ) {
				fprintf(stderr, "Value needed for -persist argument\n");
				printf("%s", usage);
				status = STATUS_ERROR;
			} else {
				args.persistFile = argv[index];
			}

		} else if ( !strcmp(argv[index], "-sleep") ) {
			if ( ++index >= argc ) {
				fprintf(stderr, "Value needed for -sleep argument\n");
				printf("%s", usage);
				status = STATUS_ERROR;
			} else {
				double	sec = atof(argv[index]);
				args.sleep_seconds  = (int) trunc( sec );
				args.sleep_useconds =
					(int) (1e6 * ( sec - args.sleep_seconds ) );
			}

		} else if ( !strcmp(argv[index], "-stork") ) {
			args.stork = true;

		} else if ( !strcmp(argv[index], "-subproc") ) {
			if ( ++index >= argc ) {
				fprintf(stderr, "Value needed for -subproc argument\n");
				printf("%s", usage);
				status = STATUS_ERROR;
			} else {
				args.subproc = atoi(argv[index]);
			}

		} else if ( !strcmp(argv[index], "-submit_note") ) {
			if ( ++index >= argc ) {
				fprintf(stderr, "Value needed for -submit_note argument\n");
				printf("%s", usage);
				status = STATUS_ERROR;
			} else {
				args.submitNote = argv[index];
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

		} else if ( !strcmp(argv[index], "-v") ) {
			args.verbosity++;

		} else if ( !strcmp(argv[index], "-version") ) {
			printf("test_log_writer: %s, %s\n", VERSION, __DATE__);
			status = STATUS_CANCEL;

		} else if ( !strcmp(argv[index], "-xml") ) {
			args.isXml = true;

		} else if ( argv[index][0] != '-' ) {
			args.logFile = argv[index];

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

	// Read the persisted file (if specified)
	if ( args.persistFile ) {
		FILE	*fp = safe_fopen_wrapper( args.persistFile, "r" );
		if ( fp ) {
			int		cluster, proc, subproc;
			if ( 3 == fscanf( fp, "%d.%d.%d", &cluster, &proc, &subproc ) ) {
				if ( args.cluster < 0 ) args.cluster = cluster;
				if ( args.proc < 0 )    args.proc    = proc;
				if ( args.subproc < 0 ) args.subproc = subproc;
			}
			fclose( fp );
		}
	}

	// Set defaults for the cluster, proc & subproc
	if ( args.cluster < 0 ) args.cluster = getpid();
	if ( args.proc < 0 )    args.proc    = 0;
	if ( args.subproc < 0 ) args.subproc = 0;

	// Stork sets these to -1
	if ( args.stork ) {
		args.proc = -1;
		args.subproc = -1;
	}

	// Update the persisted file (if specified)
	if ( args.persistFile ) {
		FILE	*fp = safe_fopen_wrapper( args.persistFile, "w" );
		if ( fp ) {
			fprintf( fp, "%d.%d.%d", args.cluster+1, args.proc, args.subproc );
			fclose( fp );
		}
	}

	return status;
}

int
WriteEvents(Arguments &args, int cluster, int proc, int subproc )
{
	int		result = 0;

	signal( SIGTERM, handle_sig );
	signal( SIGQUIT, handle_sig );
	signal( SIGINT, handle_sig );

	UserLog	log("owner", args.logFile, cluster, proc, subproc, args.isXml);

		//
		// Write the submit event.
		//
	if ( args.verbosity >= VERB_ALL ) {
		printf("Writing submit event (%d.%d.%d) @ %s\n",
			   cluster, proc, subproc, timestr() );
	}
	SubmitEvent	submit;
	strcpy(submit.submitHost, "<128.105.165.12:32779>");
		// Note: the line below is designed specifically to work with
		// Kent's dummy stork_submit script for testing DAGs with
		// DATA nodes.
	submit.submitEventLogNotes = strdup(args.submitNote);
	submit.submitEventUserNotes = strdup("User info");

	if ( !log.writeEvent(&submit) ) {
		if ( args.verbosity >= VERB_ERROR ) {
			fprintf(stderr, "Error writing log event\n");
		}
		result = 1;
	}

		//
		// Write a single generic event
		//
	if ( args.genericEventStr ) {
		GenericEvent	generic;
		strncpy(generic.info, args.genericEventStr, sizeof(generic.info) );
		generic.info[sizeof(generic.info)-1] = '\0';
		if ( args.verbosity >= VERB_ALL ) {
			printf("Writing generic event '%s' @ %s\n",
				   generic.info, timestr() );
		}
		if ( !log.writeEvent(&generic) ) {
			if ( args.verbosity >= VERB_ERROR ) {
				fprintf(stderr, "Error writing log event\n");
			}
			result = 1;
		}
	}

		//
		// Write execute events.
		//
	if ( args.verbosity >= VERB_ALL ) {
		printf( "Writing %d events for job %d.%d.%d\n",
				args.numExec, cluster, proc, subproc );
	}
	for ( int event = 0; event < args.numExec; ++event ) {
		if ( global_done ) {
			break;
		}
		if ( args.verbosity >= VERB_ALL ) {
			printf("Writing execute event (%d.%d.%d) @ %s\n",
				   cluster, proc, subproc, timestr() );
		}
		ExecuteEvent	execute;
		strcpy(execute.executeHost, "<128.105.165.12:32779>");

		if ( !log.writeEvent(&execute) ) {
			if ( args.verbosity >= VERB_ERROR ) {
				fprintf(stderr, "Error writing log event\n");
			}
			result = 1;
		}

		if ( args.sleep_seconds ) {
			sleep( args.sleep_seconds);
		}
		if ( args.sleep_useconds ) {
			usleep( args.sleep_useconds);
		}
	}

		//
		// Write the terminated event.
		//
	if ( args.verbosity >= VERB_ALL ) {
		printf("Writing terminate event (%d.%d.%d) @ %s\n",
			   cluster, proc, subproc, timestr() );
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

const char *timestr( void )
{
	static char	tbuf[64];
	time_t now = time( NULL );
	strncpy( tbuf, ctime( &now ), sizeof(tbuf) );
	tbuf[sizeof(tbuf)-1] = '\0';
	if ( strlen(tbuf) ) {
		tbuf[strlen(tbuf)-1] = '\0';
	}
	return tbuf;
}
