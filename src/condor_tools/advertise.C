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
#include "condor_classad.h"
#include "condor_version.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "command_strings.h"
#include "daemon.h"
#include "safe_sock.h"
#include "condor_distribution.h"
#include "daemon_list.h"
#include "dc_collector.h"
#include "my_hostname.h"

void
usage( char *cmd )
{
	fprintf(stderr,"Usage: %s [options] <update-command> <classad-filename>\n",cmd);
	fprintf(stderr,"Where options are:\n");
	fprintf(stderr,"    -help             Display options\n");
	fprintf(stderr,"    -version          Display Condor version\n");
	fprintf(stderr,"    -pool <hostname>  Use this central manager\n");
	fprintf(stderr,"    -debug            Show extra debugging info\n");
	fprintf(stderr,"    -tcp              Ship classad via TCP (default is UDP)\n");
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
	bool use_tcp = false;


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
		} else if(!strncmp(argv[i],"-tcp",strlen(argv[i]))) {
			use_tcp = true;
		} else if(!strcmp(argv[i],"-version")) {
			version();
			exit(0);
		} else if(!strcmp(argv[i],"-debug")) {
				// dprintf to console
			Termlog = 1;
			dprintf_config ("TOOL", 2 );
		} else if(argv[i][0]!='-') {
			if(command==-1) {
				command = getCollectorCommandNum(argv[i]);
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
	Sock *sock;
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

	// If there's no "MyAddress", generate one..
	ExprTree *tree = ad->Lookup( ATTR_MY_ADDRESS );
	MyString tmp = "";
	if ( tree ) {
		tmp = ((MyString *)tree->RArg())->Value();
	}
	if ( tmp.Length() == 0 ) {
		tmp.sprintf( "%s = \"<%s:0>\"", ATTR_MY_ADDRESS, my_ip_string() );
		ad->Insert( tmp.GetCStr() );
	}

	CollectorList * collectors;
	if ( pool ) {
		collector = new Daemon( DT_COLLECTOR, pool, 0 );
		collectors = new CollectorList();
		collectors->append (collector);
	} else {
		collectors = CollectorList::create();
	}

	bool had_error = false;

	collectors->rewind();
	while (collectors->next(collector)) {
		
		dprintf(D_FULLDEBUG,"locating collector %s...\n", collector->name());

		if(!collector->locate()) {
			fprintf(stderr,"couldn't locate collector: %s\n",collector->error());
			had_error = true;
			continue;
		}

		dprintf(D_FULLDEBUG,"collector is %s located at %s\n",
				collector->hostname(),collector->addr());
	
		if ( use_tcp ) {
			sock = collector->startCommand(command,Stream::reli_sock,20);
		} else {
			sock = collector->startCommand(command,Stream::safe_sock,20);
		}

		int result = 0;
		if ( sock ) {
			result += ad->put( *sock );
			result += sock->end_of_message();
		}
		if ( result != 2 ) {
			fprintf(stderr,"failed to send classad to %s\n",collector->addr());
			had_error = true;
			delete sock;
			continue;
		}

		delete sock;
	}

	delete collectors;

	return (had_error)?1:0;
}
