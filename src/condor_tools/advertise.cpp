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
	fprintf(stderr,"    -multiple         Publish multiple ads, separated by blank lines\n");
	fprintf(stderr,"\nExample: %s -debug UPDATE_STORAGE_AD adfile\n\n",cmd);
}

void
version()
{
	printf( "%s\n%s\n", CondorVersion(), CondorPlatform() );
}

int main( int argc, char *argv[] )
{
	const char *filename=0;
	char *pool=0;
	int command=-1;
	int i;
	bool use_tcp = false;
	bool with_ack = false;
	bool allow_multiple = false;

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
		} else if(!strncmp(argv[i],"-multiple",strlen(argv[i]))) {
				// We don't set allow_multiple=true by default, because
				// existing users (e.g. glideinWMS) have stray blank lines
				// in the input file.
			allow_multiple = true;
		} else if(!strcmp(argv[i],"-version")) {
			version();
			exit(0);
		} else if(!strcmp(argv[i],"-debug")) {
				// dprintf to console
			dprintf_set_tool_debug("TOOL", 0);
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
	ClassAdList ads;
	Daemon *collector;
	Sock *sock;

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
		file = safe_fopen_wrapper_follow(filename,"r");
	}
	if(!file) {
		fprintf(stderr,"couldn't open %s: %s\n",filename,strerror(errno));
		return 1;
	}

	while(!feof(file)) {
		int eof=0,error=0,empty=0;
		char const *delim = "\n";
		if( !allow_multiple ) {
			delim = "***";
		}
		ClassAd *ad = new ClassAd(file,const_cast<char *>(delim),eof,error,empty);
		if(error) {
			fprintf(stderr,"couldn't parse ClassAd in %s\n",filename);
			delete ad;
			return 1;
		}
		if( empty ) {
			delete ad;
			break;
		}
		if( !allow_multiple && ads.Length() > 0 ) {
			fprintf(stderr,"ERROR: failed to parse '%s' as a ClassAd attribute\n",delim);
			delete ad;
			return 1;
		}
		ads.Insert(ad);
	}

	if(ads.Length() == 0) {
		fprintf(stderr,"%s is empty\n",filename);
		return 1;
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

		sock = NULL;

		ClassAd *ad;
		int success_count = 0;
		int failure_count = 0;
		ads.Rewind();
		while( (ad=ads.Next()) ) {

				// If there's no "MyAddress", generate one..
			if( !ad->Lookup( ATTR_MY_ADDRESS ) ) {
				MyString tmp;
				tmp.formatstr( "<%s:0>", my_ip_string() );
				ad->Assign( ATTR_MY_ADDRESS, tmp.Value() );
			}

			if ( use_tcp ) {
				if( !sock ) {
					sock = collector->startCommand(command,Stream::reli_sock,20);
				}
				else {
						// Use existing connection.
					sock->encode();
					sock->put(command);
				}
			} else {
					// We must open a new UDP socket each time.
				delete sock;
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
				failure_count++;
				delete sock;
				sock = NULL;
				continue;
			}

			if( with_ack ) {
				sock->decode();
				int ok = 0;
				if( !sock->get(ok) || !sock->end_of_message() ) {
					fprintf(stderr,"failed to get ack from %s\n",collector->addr());
					had_error = true;
					failure_count++;
					delete sock;
					sock = NULL;
					continue;
				}

					// ack protocol does not allow for multiple updates,
					// so close the socket now
				delete sock;
				sock = NULL;
			}

			success_count++;
		}
		if( sock ) {
			CondorVersionInfo const *ver = sock->get_peer_version();
			if( !ver || ver->built_since_version(7,7,3) ) {
					// graceful hangup so the collector knows we are done
				sock->encode();
				command = DC_NOP;
				sock->put(command);
				sock->end_of_message();
			}

			delete sock;
			sock = NULL;
		}

		printf("Sent %d of %d ad%s to %s.\n",
			   success_count,
			   success_count + failure_count,
			   success_count+failure_count == 1 ? "" : "s",
			   collector->name());
	}

	delete collectors;

	return (had_error)?1:0;
}
