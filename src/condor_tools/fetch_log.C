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
#include "condor_classad.h"
#include "condor_version.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "daemon.h"
#include "dc_collector.h"
#include "reli_sock.h"
#include "command_strings.h"
#include "condor_distribution.h"

void
usage( char *cmd )
{
	fprintf(stderr,"Usage: %s [options] <machine-name> <subsystem>[.ext]\n",cmd);
	fprintf(stderr,"Where options are:\n");
	fprintf(stderr,"    -help             Display options\n");
	fprintf(stderr,"    -version          Display Condor version\n");
	fprintf(stderr,"    -pool <hostname>  Use this central manager\n");
	fprintf(stderr,"    -debug            Show extra debugging info\n");
	fprintf(stderr,"To select a particular daemon to talk to (does NOT select log file), use:\n");
	fprintf(stderr,"    -master\n");
	fprintf(stderr,"    -schedd\n");
	fprintf(stderr,"    -startd\n");
	fprintf(stderr,"    -collector\n");
	fprintf(stderr,"    -negotiator\n");
	fprintf(stderr,"    -kbdd\n");
	fprintf(stderr,"    -dagman\n"); 
	fprintf(stderr,"    -view_collector\n");
	fprintf(stderr,"The subsystem name plus optional extension specifies the log file.\n");
	fprintf(stderr,"Possible subsystem names (anything with an entry XXX_LOG in remote config file):\n");

	fprintf(stderr,"    MASTER\n");
	fprintf(stderr,"    COLLECTOR\n");
	fprintf(stderr,"    NEGOTIATOR\n");
	fprintf(stderr,"    NEGOTIATOR_MATCH\n");
	fprintf(stderr,"    SCHEDD\n");
	fprintf(stderr,"    SHADOW\n");
	fprintf(stderr,"    STARTD\n");
	fprintf(stderr,"    STARTER\n");
	fprintf(stderr,"    KBDD\n");
	fprintf(stderr,"\nExample 1: %s -debug coral STARTD\n",cmd);
	fprintf(stderr,"\nExample 2: %s -debug coral STARTER.vm2\n\n",cmd);
}

void
version()
{
	printf( "%s\n%s\n", CondorVersion(), CondorPlatform() );
}

int main( int argc, char *argv[] )
{
	char *machine_name = 0;
	char *log_name = 0;
	char *pool=0;
	int i;

	daemon_t type = DT_MASTER;

	myDistro->Init( argc, argv );
	config();

	for( i=1; i<argc; i++ ) {
		if(!strcmp(argv[i],"-help")) {
			usage(argv[0]);
			exit(0);
		} else if(!strcmp(argv[i],"-pool")) {	
			i++;
			if(!argv[i]) {
				fprintf(stderr,"-pool requires an argument.\n\n");
				usage(argv[0]);
				exit(1);
			}
			pool = argv[i];
		} else if(!strcmp(argv[i],"-version")) {
			version();
			exit(0);
		} else if(!strcmp(argv[i],"-debug")) {
            Termlog = 1;
            dprintf_config ("TOOL", 2);
		} else if(argv[i][0]=='-') {
			type = stringToDaemonType(&argv[i][1]);
			if(type==DT_NONE) {
				usage(argv[0]);
				exit(1);
			}
		} else if(argv[i][0]!='-') {
			if(!machine_name) {
				machine_name = argv[i];
			} else if(!log_name) {
				log_name = argv[i];
			} else {
				fprintf(stderr,"Extra argument: %s\n\n",argv[i]);
				usage(argv[0]);
				exit(1);
			}
		} else {
			usage(argv[0]);
			exit(1);
		}
	}

	if( !machine_name || !log_name ) {
		usage(argv[0]);
		exit(1);
	}

	Daemon *daemon;
	ReliSock *sock;

	if (pool) {
		DCCollector col( pool );
		if( ! col.addr() ) {
			fprintf( stderr, "Error: %s\n", col.error() );
			exit(1);
		}
		daemon = new Daemon( type, machine_name, col.addr() );
	} else {
		daemon = new Daemon( type, machine_name );
	}

		
	dprintf(D_FULLDEBUG,"Locating daemon process on %s...\n",machine_name);

	if(!daemon->locate()) {
		fprintf(stderr,"Couldn't locate daemon on %s: %s\n",machine_name,daemon->error());
		exit(1);
	}

	dprintf(D_FULLDEBUG,"Daemon %s is %s\n",daemon->hostname(),daemon->addr());
	
	sock = (ReliSock*)daemon->startCommand( DC_FETCH_LOG, Sock::reli_sock);

	if(!sock) {
		fprintf(stderr,"couldn't connect to daemon %s at %s\n",daemon->hostname(),daemon->addr());
		return 1;
	}

	sock->put( DC_FETCH_LOG_TYPE_PLAIN );
	sock->put( log_name );
	sock->end_of_message();

	int result = -1;
	int exitcode = 1;
	char *reason=0;
	filesize_t filesize;

	sock->decode();
	sock->code(result);

	switch(result) {
		case -1:
			reason = "permission denied";
			break;
		case DC_FETCH_LOG_RESULT_SUCCESS:
			result = sock->get_file(&filesize,1,0);
			if(result>=0) {
				exitcode = 0;
			} else {
				reason = "connection lost";
			}
			break;
		case DC_FETCH_LOG_RESULT_NO_NAME:
			reason = "no log file by that name";
			break;
		case DC_FETCH_LOG_RESULT_CANT_OPEN:
			reason = "server was unable to access it";
			break;
		case DC_FETCH_LOG_RESULT_BAD_TYPE:
			reason = "server can't provide that type of log";
			break;
		default:
			reason = "unknown error";
			break;
	}

	if(exitcode!=0) {
		fprintf(stderr,"Couldn't fetch log: %s.\n",reason);
	}

	return exitcode;
}
