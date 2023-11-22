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
#include "ipv6_hostname.h"

#include <algorithm>

void
usage( const char *cmd , const char * opt)
{
	fprintf(stderr,"Usage: %s [options] <update-command> [<classad-filename>]\n",cmd);
	fprintf(stderr,"Where options are:\n");
	fprintf(stderr,"    -help             Display options\n");
	fprintf(stderr,"    -version          Display Condor version\n");
	fprintf(stderr,"    -pool <hostname>  Use this central manager\n");
	fprintf(stderr,"    -debug            Show extra debugging info\n");
	fprintf(stderr,"    -tcp              Ship classad via TCP (default)\n");
	fprintf(stderr,"    -udp              Ship classad via UDP\n");
	fprintf(stderr,"    -multiple         Publish multiple ads, separated by blank lines\n");

	if (opt && ( !strcmp(opt,"all") || !strcmp(opt,"cmd"))) {
		fprintf(stderr,"\nAnd <update-command> is one of:\n");
		static const char * const aPre[] = {"UPDATE_", "MERGE_", "INVALIDATE_"};
		for (int jj = 0; jj < (int)COUNTOF(aPre); ++jj) {
			for (int id = UPDATE_STARTD_AD; id < UPDATE_STARTD_AD + 100; ++id) {
				const char * cmdname = getCollectorCommandString(id);
				if (cmdname && starts_with(cmdname, aPre[jj])) {
					fprintf(stderr,"    %s\n",cmdname);
				}
			}
		}
	}

	fprintf(stderr, "\nIf <classad-filename> does not exist or is -, classads will be read from standard input\n");
	fprintf(stderr,"Example: %s -debug UPDATE_STORAGE_AD adfile\n\n",cmd);
}

// this class is used to help parse one or more ClassAd's from a FILE*
class ToolClassAdFileParseHelper : public ClassAdFileParseHelper
{
 public:
	// Some compilers whine when you have virtual methods but not an
	// explicit virtual destructor
	virtual ~ToolClassAdFileParseHelper() {}
	ToolClassAdFileParseHelper(bool multi) : multiple(multi) {};

	// return non-zero if new parser, o if old (line oriented) parser
	// TODO: fix this to handle new style classads also...
	virtual int NewParser(classad::ClassAd & /*ad*/, FILE* /*file*/, bool & detected_long, std::string & /*errmsg*/) { detected_long = false; return 0; }

	// return 0 to skip (is_comment), 1 to parse line, 2 for end-of-classad, -1 for abort
	virtual int PreParse(std::string & line, classad::ClassAd & /*ad*/, FILE* /*file*/) {
		// if this line matches the ad delimitor, tell the parser to stop parsing
		if (multiple && line.empty())
			return 2; //end-of-classad

		// check for blank lines or lines whose first character is #
		// tell the parse to skip those lines, otherwise tell the parser to
		// parse the line.
		for (size_t ix = 0; ix < line.size(); ++ix) {
			if (line[ix] == '#')
				return 0; // skip this line, but don't stop parsing.
			if (line[ix] != ' ' && line[ix] != '\t')
				return 1; // parse this line
		}
		return 0; // skip this line, but don't stop parsing.
	}

	// return 0 to skip and continue, 1 to re-parse line, 2 to quit parsing with success, -1 to abort parsing.
	virtual int OnParseError(std::string & line, classad::ClassAd & /*ad*/, FILE* file) {
		// print out where we barfed to the log file
		dprintf(D_ALWAYS,"failed to create classad; bad expr = '%s'\n", line.c_str());

		if ( ! multiple) return -1; // abort

		// skip the remainder of the ad by reading until we see eof or a blank line.
		line = "";
		while ( ! line.empty() && line[0] != '\n') {
			if (feof(file))
				break;
			if ( ! readLine(line, file, false))
				break;
		}
		chomp(line);
		return -1; // abort
	}

 private:
	bool multiple;
	std::string delim;
};


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
	bool with_ack = false;
	bool allow_multiple = false;
	bool many_connections = false;

	set_priv_initialize(); // allow uid switching if root
	config();

	bool use_tcp = param_boolean( "UPDATE_COLLECTOR_WITH_TCP", true );

	for( i=1; i<argc; i++ ) {
		if(!strcmp(argv[i],"-help") || !strcmp(argv[i],"--help")) {
			usage(argv[0], argv[i+1] ? argv[i+1] : "cmd");
			exit(0);
		} else if(!strcmp(argv[i],"-pool")) {
			i++;
			if(!argv[i]) {
				fprintf(stderr,"-pool requires an argument.\n\n");
				usage(argv[0], NULL);
				exit(1);
			}
			pool = argv[i];
		} else if(!strncmp(argv[i],"-tcp",strlen(argv[i]))) {
			use_tcp = true;
		} else if(!strncmp(argv[i],"-udp",strlen(argv[i]))) {
			use_tcp = false;
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
					usage(argv[0], "all");
					exit(1);
				}
			} else if(!filename) {
				filename = argv[i];
			} else {
				fprintf(stderr,"Extra argument: %s\n\n",argv[i]);
				usage(argv[0], NULL);
				exit(1);
			}
		} else if(!strncmp(argv[i],"-many-connections", strlen(argv[i]))) { 
			many_connections = true;
		} else {
			fprintf(stderr,"Unknown argument: %s\n\n",argv[i]);
			usage(argv[0], NULL);
			exit(1);
		}
	}

	FILE *file;
	ClassAdList ads;
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

	// create class that we can use to influence the behavior of InsertFromFile
	ToolClassAdFileParseHelper parse_helper(allow_multiple);

	for (;;) {
		ClassAd *ad = new ClassAd();
		int error;
		bool eof;
		int cAttrs = InsertFromFile(file, *ad, eof, error, &parse_helper);
		if (error < 0) {
			fprintf(stderr,"couldn't parse ClassAd in %s\n", filename);
			delete ad;
			return 1;
		}
		if (cAttrs > 0) {
			ads.Insert(ad);
		} else {
			delete ad;
			ad = 0; // so we don't double delete it
		}
		if (eof) {
			break;
		}
		if( !allow_multiple && ads.Length() > 0 ) {
			fprintf(stderr,"ERROR: multiple ads in %s\n", filename);
			delete ad;
			return 1;
		}
	}

	if(ads.Length() == 0) {
		fprintf(stderr,"%s is empty\n",filename);
		return 1;
	}

	// If the command is unspecified, guess.
	if( command == -1 ) {
		ads.Rewind();
		ClassAd * c = ads.Next();
		std::string myType;
		c->LookupString( ATTR_MY_TYPE, myType );
		if( myType.empty() ) {
			fprintf( stderr, "MyType not set in ad, aborting.\n" );
			return 1;
		}
		std::transform( myType.begin(), myType.end(), myType.begin(), toupper );

		std::string commandString;
		if( myType == "GENERIC" ) {
			commandString = "UPDATE_AD_GENERIC";
		} else {
			formatstr( commandString, "UPDATE_%s_AD", myType.c_str() );
		}

		int commandInt = getCommandNum( commandString.c_str() );
		if( commandInt == -1 ) {
			dprintf( D_FULLDEBUG, "Unrecognized ad type '%s', using GENERIC.\n", myType.c_str() );
			commandInt = UPDATE_AD_GENERIC;
		}
		command = commandInt;
	}

	CollectorList * collectors = CollectorList::create(pool);

	bool had_error = false;

	for (auto& collector : collectors->getList()) {

		dprintf(D_FULLDEBUG,"locating collector %s...\n", collector->name());

		if(!collector->locate(Daemon::LOCATE_FOR_LOOKUP)) {
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
				std::string tmp;
				// TODO: Picking IPv4 arbitrarily.
				std::string my_ip = get_local_ipaddr(CP_IPV4).to_ip_string();
				formatstr( tmp, "<%s:0>", my_ip.c_str() );
				ad->Assign( ATTR_MY_ADDRESS, tmp );
			}

			if( !sock ) {
				sock = collector->startCommand(command,
					use_tcp ? Stream::reli_sock : Stream::safe_sock,
					20);
			} else {
					// Use existing connection.
				sock->encode();
				sock->put(command);
			}

			int result = 0;
			if ( sock ) {
				result += putClassAd( sock, *ad );
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
			if (many_connections) {
				sock = NULL;
			}
		}
		if( sock ) {
			CondorVersionInfo const *ver = sock->get_peer_version();
			if( !ver || ver->built_since_version(7,7,3) ) {
					// graceful hangup so the collector knows we are done
				sock->encode();
				int hangup_cmd = DC_NOP;
				sock->put(hangup_cmd);
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

	if (many_connections) {
		sleep(3600);
	}
	delete collectors;

	return (had_error)?1:0;
}
