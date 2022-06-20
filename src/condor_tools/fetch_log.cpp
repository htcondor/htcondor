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
#include "condor_classad.h"
#include "condor_version.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "daemon.h"
#include "dc_collector.h"
#include "reli_sock.h"
#include "command_strings.h"
#include "condor_distribution.h"
#include "match_prefix.h"

int handleHistoryDir(ReliSock *);

void
usage( const char *cmd )
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
	fprintf(stderr,"    HISTORY\n");
	fprintf(stderr,"    STARTD_HISTORY\n");
	fprintf(stderr,"\nExample 1: %s -debug coral STARTD\n",cmd);
	fprintf(stderr,"\nExample 2: %s -debug coral STARTER.slot2\n\n",cmd);
}

void
version()
{
	printf( "%s\n%s\n", CondorVersion(), CondorPlatform() );
}

int main( int argc, const char *argv[] )
{
	const char *machine_name = 0;
	const char *log_name = 0;
	const char *pool=0;
	const char *pcolon=0;
	int i;

	daemon_t type = DT_MASTER;

	set_priv_initialize(); // allow uid switching if root
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
		} else if(is_dash_arg_colon_prefix(argv[i],"debug",&pcolon,1)) {
			dprintf_set_tool_debug("TOOL", (pcolon && pcolon[1]) ? pcolon+1 : nullptr);
		} else if(argv[i][0]=='-') {
			type = stringToDaemonType(&argv[i][1]);
			if( type == DT_NONE || type == DT_DAGMAN) {
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

	if(!daemon->locate(Daemon::LOCATE_FOR_ADMIN)) {
		fprintf(stderr,"Couldn't locate daemon on %s: %s\n",machine_name,daemon->error());
		exit(1);
	}

	dprintf(D_FULLDEBUG,"Daemon %s is %s\n",daemon->hostname(),daemon->addr());
	
	sock = (ReliSock*)daemon->startCommand( DC_FETCH_LOG, Sock::reli_sock, 1200);

	if(!sock) {
		fprintf(stderr,"couldn't connect to daemon %s at %s\n",daemon->hostname(),daemon->addr());
		return 1;
	}

	int commandType = DC_FETCH_LOG_TYPE_PLAIN;
	if ((strcmp(log_name, "HISTORY") == 0) || (strcmp(log_name, "STARTD_HISTORY") == 0)) {
		commandType = DC_FETCH_LOG_TYPE_HISTORY;
	}

	if ((strcmp(log_name, "STARTD.PER_JOB_HISTORY_DIR") == 0) || (strcmp(log_name, "STARTD.PER_JOB_HISTORY_DIR") == 0)) {
		commandType = DC_FETCH_LOG_TYPE_HISTORY_DIR;
	}

	sock->put( commandType );
	sock->put( log_name );
	sock->end_of_message();

	if (commandType == DC_FETCH_LOG_TYPE_HISTORY_DIR) {
		return handleHistoryDir(sock);
	}

	int result = -1;
	int exitcode = 1;
	const char *reason=NULL;
	filesize_t filesize;

	sock->decode();
	if (!sock->code(result)) {
		fprintf(stderr,"Couldn't fetch log: server did not reply\n");
		return -1;
	}

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
			while (!sock->get_file(&filesize, 1, 0)) ;
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

int handleHistoryDir(ReliSock *sock) {
	int  result = -1;
	filesize_t filesize;
	char *filename = 0;

	sock->decode();

	if (!sock->code(result)) {
		printf("Can't connect to server\n");
		exit(1);
	}
	while (result == 1) {
			int fd = -1;
			filename = NULL;
			if (!sock->code(filename)) {
				printf("Can't get filename from server\n");
				exit(1);
			}
			fd = safe_open_wrapper_follow(filename, O_CREAT | O_WRONLY);
			if (fd < 0) {
				printf("Can't open local file %s for writing\n", filename);
				exit(1);
			}
			result = sock->get_file(&filesize,fd,0);
			close(fd);
			
			if (!sock->code(result)) {
				printf("Cannot get result status from server\n");
				result = -1;
			}
			free(filename);
	}
	return 0;
}
