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
#include "command_strings.h"
#include "daemon.h"
#include "safe_sock.h"

void
usage( char *cmd )
{
	fprintf(stderr,"Usage: %s [options] <update-command> <classad-filename>\n",cmd);
	fprintf(stderr,"Where options are:\n");
	fprintf(stderr,"    -help             Display options\n");
	fprintf(stderr,"    -version          Display Condor version\n");
	fprintf(stderr,"    -pool <hostname>  Use this central manager\n");
	fprintf(stderr,"    -debug            Show extra debugging info\n");
	fprintf(stderr,"\nExample: %s -debug UPDATE_STORAGE_AD adfile\n\n",cmd);
}

void
version()
{
	printf( "%s\n%s\n", CondorVersion(), CondorPlatform() );
}

int main( int argc, char *argv[] )
{
	char *filename=0;
	char *pool=0;
	int command=-1;
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
			if(command==-1) {
				command = get_collector_command_num(argv[i]);
				if(command==-1) {
					fprintf(stderr,"Unknown command name %s\n\n",argv[i]);
					usage(argv[0]);
					exit(1);
				}
			} else if(!filename) {
				filename = argv[i];
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

	if(!filename) {
		fprintf(stderr,"You must give a filename containing a ClassAd.\n\n");
		usage(argv[0]);
		exit(1);
	}

	FILE *file;
	ClassAd *ad;
	Daemon *collector;
	SafeSock *sock;
	int eof,error,empty;

	file = fopen(filename,"r");
	if(!file) {
		fprintf(stderr,"couldn't open %s: %s\n",filename,strerror(errno));
		return 1;
	}

	ad = new ClassAd(file,"***",eof,error,empty);
	if(error) {
		fprintf(stderr,"couldn't parse ClassAd in %s\n",filename);
		return 1;
	}

	if(empty) {
		fprintf(stderr,"%s is empty\n",filename);
		return 1;
	}

	if(pool) {
		collector = new Daemon( DT_COLLECTOR, 0, pool );
	} else {
		collector = new Daemon( DT_COLLECTOR, 0, 0 );
	}
		
	dprintf(D_FULLDEBUG,"locating collector...\n");

	if(!collector->locate()) {
		fprintf(stderr,"couldn't locate collector: %s\n",collector->error());
		exit(1);
	}

	dprintf(D_FULLDEBUG,"collector is %s <%s:%d>\n",collector->hostname(),collector->addr(),collector->port());
	
	sock = collector->safeSock();

	sock->encode();
	sock->put( command );
	ad->put( *sock );
	sock->end_of_message();

	return 0;
}
