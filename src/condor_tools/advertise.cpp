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
	fprintf(stderr,"Usage: %s [options] <update-command> [<classad-filename>]\n",cmd);
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
	bool with_ack = false;


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
			dprintf_config ("TOOL" );
		} else if(argv[i][0]!='-' || !strcmp(argv[i],"-")) {
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

	FILE *file;
	ClassAd *ad;
	Daemon *collector;
	Sock *sock;
	int eof,error,empty;

	switch( command ) {
	case UPDATE_STARTD_AD_WITH_ACK:
		with_ack = true;
		break;
	}

	if( with_ack ) {
		use_tcp =  true;
	}

	if(!filename || !strcmp(filename,"-")) {
		file = stdin;
		filename = "(stdin)";
	} else {
		file = safe_fopen_wrapper(filename,"r");
	}
	if(!file) {
		fprintf(stderr,"couldn't open %s: %s\n",filename,strerror(errno));
		return 1;
	}

	ad = new ClassAd(file,"***",eof,error,empty);
	if(error) {
		fprintf(stderr,"couldn't parse ClassAd in %s\n",filename);
		delete ad;
		return 1;
	}

	if(empty) {
		fprintf(stderr,"%s is empty\n",filename);
		delete ad;
		return 1;
	}

	// If there's no "MyAddress", generate one..
	MyString tmp = "";
	ad->LookupString( ATTR_MY_ADDRESS, tmp );
	if ( tmp.Length() == 0 ) {
		tmp.sprintf( "<%s:0>", my_ip_string() );
		ad->Assign( ATTR_MY_ADDRESS, tmp.Value() );
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

		if( with_ack ) {
			sock->decode();
			int ok = 0;
			if( !sock->get(ok) || !sock->end_of_message() ) {
				fprintf(stderr,"failed to get ack from %s\n",collector->addr());
				had_error = true;
				delete sock;
				continue;
			}
		}

		delete sock;
	}

	delete collectors;
	delete ad;

	return (had_error)?1:0;
}
