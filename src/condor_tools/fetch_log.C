/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
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
#include "condor_classad.h"
#include "condor_version.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "daemon.h"
#include "reli_sock.h"
#include "command_strings.h"

void
usage( char *cmd )
{
	fprintf(stderr,"Usage: %s [options] <machine-name> <log-name> <file-name>\n",cmd);
	fprintf(stderr,"Where options are:\n");
	fprintf(stderr,"    -help             Display options\n");
	fprintf(stderr,"    -version          Display Condor version\n");
	fprintf(stderr,"    -pool <hostname>  Use this central manager\n");
	fprintf(stderr,"    -debug            Show extra debugging info\n");
	fprintf(stderr,"\nExample: %s -debug coral STARTD\n\n",cmd);
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
	char *file_name = 0;
	char *pool=0;
	int i;

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
			set_debug_flags("D_FULLDEBUG");
		} else if(argv[i][0]!='-') {
			if(!machine_name) {
				machine_name = argv[i];
			} else if(!log_name) {
				log_name = argv[i];
			} else if(!file_name) {
				file_name = argv[i];
			} else {
				fprintf(stderr,"Extra argument: %s\n\n",argv[i]);
				usage(argv[0]);
				exit(1);
			}
		} else {
			fprintf(stderr,"Unknown argument: %s\n\n",argv[i]);
			usage(argv[0]);
			exit(1);
		}
	}

	if( !machine_name || !log_name || !file_name ) {
		fprintf(stderr,"You must give three arguments: a machine, log, and file name.\n");
		usage(argv[0]);
		exit(1);
	}

	Daemon *master;
	ReliSock *sock;

	if(pool) {
		master = new Daemon( DT_MASTER, machine_name, pool );
	} else {
		master = new Daemon( DT_MASTER, machine_name, 0 );
	}
		
	dprintf(D_FULLDEBUG,"Locating master process on %s...\n",machine_name);

	if(!master->locate()) {
		fprintf(stderr,"Couldn't locate master on %s: %s\n",machine_name,master->error());
		exit(1);
	}

	dprintf(D_FULLDEBUG,"Master is %s <%s:%d>\n",master->hostname(),master->addr(),master->port());
	
	sock = master->reliSock();

	if(!sock) {
		fprintf(stderr,"couldn't connect to master %s at <%s:%d>\n",master->hostname(),master->addr(),master->port());
		return 1;
	}

	sock->encode();
	sock->put( FETCH_LOG );
	sock->put( log_name );
	sock->end_of_message();

	sock->decode();
	int result = sock->get_file(file_name,0);
	
	if(result<=0) {
		fprintf(stderr,"Unable to fetch log.\n");
		fprintf(stderr,"There might not be a log file named '%s.'\n",log_name);
		fprintf(stderr,"Or, '%s' may not authorize you to view it.\n",machine_name);
		fprintf(stderr,"Or, you might not have permission to write to '%s.'\n",file_name);
		return 1;
	} else {
		return 0;
	}
}
