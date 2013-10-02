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
#include "condor_secman.h"
#include "command_strings.h"
#include "daemon.h"
#include "safe_sock.h"
#include "condor_distribution.h"
#include "daemon_list.h"
#include "dc_collector.h"
#include "my_hostname.h"

using namespace std;

void
usage( char *cmd )
{
	fprintf(stderr,"Usage: %s [options] TOKEN [TOKEN [TOKEN [...]]]\n",cmd);
	fprintf(stderr,"\nwhere TOKEN is ( ALL | <authorization-level> | <command-name> | <command-int> )\n\n");
	fprintf(stderr,"general options:\n");
	fprintf(stderr,"    -config <filename>              Add configuration from specified file\n");
	fprintf(stderr,"    -debug                          Show extra debugging info\n");
	fprintf(stderr,"    -version                        Display Condor version\n");
	fprintf(stderr,"    -help                           Display options\n");
	fprintf(stderr,"\nspecifying target options:\n");
	fprintf(stderr,"    -address <sinful>               Use this sinful string\n");
	fprintf(stderr,"    -pool    <host>                 Query this collector\n");
	fprintf(stderr,"    -name    <name>                 Find a daemon with this name\n");
	fprintf(stderr,"    -type    <subsystem>            Type of daemon to contact (default: SCHEDD)\n");
	fprintf(stderr,"\noutput options (specify only one):\n");
	fprintf(stderr,"    -quiet                          No output, only sets status\n");
	fprintf(stderr,"    -table                          Print one result per line\n");
	fprintf(stderr,"    -verbose                        Print all available information\n");
	fprintf(stderr,"\nExample:\n    %s -addr \"<127.0.0.1:9618>\" -table READ WRITE DAEMON\n\n",cmd);
}

void
version()
{
	printf( "%s\n%s\n", CondorVersion(), CondorPlatform() );
}



void process_err_stack(CondorError *errstack) {

	printf("%s\n", errstack->getFullText(true).c_str());

}


void print_useful_info_1(bool rv, MyString name, Sock*, ClassAd *ad, ClassAd *authz_ad, CondorError *) {
	MyString  val;

	if(!rv) {
		printf("%s failed!  Use -verbose for more information.\n", name.Value());
		return;
	}

	printf("%s using (", name.Value());

	ad->LookupString("encryption", val);
	if (strcasecmp(val.Value(), "no") == 0) {
		printf("no encryption");
	} else {
		ad->LookupString("cryptomethods", val);
		printf("%s", val.Value());
	}

	printf(", ");

	ad->LookupString("integrity", val);
	if (strcasecmp(val.Value(), "no") == 0) {
		printf("no integrity");
	} else {
		printf("MD5");
	}

	printf(", and ");

	ad->LookupString("authentication", val);
	if (strcasecmp(val.Value(), "no") == 0) {
		printf("no authentication");
	} else {
		ad->LookupString("authmethods", val);
		printf("%s", val.Value());
	}

	printf(") ");

	bool bval;
	authz_ad->LookupBool(ATTR_SEC_AUTHORIZATION_SUCCEEDED, bval);
	printf(bval ? "succeeded" : "failed");

	printf(" as ");

	ad->LookupString("myremoteusername", val);
	printf("%s", val.Value());

	printf ("\n");
}


void print_useful_info_2(bool rv, int cmd, MyString name, Sock*, ClassAd *ad, ClassAd *authz_ad, CondorError *errstack) {
	MyString  val;

	if(!rv) {
		printf("%s failed!\n", name.Value());
		process_err_stack(errstack);
		printf("\n");
		return;
	}

	ad->LookupString("remoteversion", val);
	printf("Remote Version:              %s\n", val.Value());
	val = CondorVersion();
	printf("Local  Version:              %s\n", val.Value());

	ad->LookupString("sid", val);
	printf("Session ID:                  %s\n", val.Value());
	printf("Instruction:                 %s\n", name.Value());
	printf("Command:                     %i\n", cmd);


	ad->LookupString("encryption", val);
	if (strcasecmp(val.Value(), "no") == 0) {
		printf("Encryption:                  none\n");
	} else {
		ad->LookupString("cryptomethods", val);
		printf("Encryption:                  %s\n", val.Value());
	}

	ad->LookupString("integrity", val);
	if (strcasecmp(val.Value(), "no") == 0) {
		printf("Integrity:                   none\n");
	} else {
		printf("Integrity:                   MD5\n");
	}

	ad->LookupString("authentication", val);
	if (strcasecmp(val.Value(), "no") == 0) {
		printf("Authentication:              none\n");
	} else {
		ad->LookupString("authmethods", val);
		printf("Authenticated using:         %s\n", val.Value());
		ad->LookupString("authmethodslist", val);
		printf("All authentication methods:  %s\n", val.Value());
	}

	ad->LookupString("myremoteusername", val);
	printf("Remote Mapping:              %s\n", val.Value());

	bool bval;
	authz_ad->LookupBool(ATTR_SEC_AUTHORIZATION_SUCCEEDED, bval);
	printf("Authorized:                  %s\n", (bval ? "TRUE" : "FALSE"));

	printf ("\n");

	if (errstack->getFullText() != "") {
		printf ("Information about authentication methods that were attempted but failed:\n");
		process_err_stack(errstack);
		printf ("\n");
	}

}


void print_useful_info_10(bool rv, MyString name, Sock*, ClassAd *ad, ClassAd *authz_ad, CondorError *) {
	MyString  val;

	printf("%20s", name.Value());

	if(!rv) {
		printf ("           FAIL       FAIL      FAIL     FAIL FAIL  (use -verbose for more info)\n");
		return;
	}

	ad->LookupString("authentication", val);
	if (strcasecmp(val.Value(), "no") == 0) {
		val = "none";
	} else {
		ad->LookupString("authmethods", val);
	}
	printf("%15s", val.Value());

	ad->LookupString("encryption", val);
	if (strcasecmp(val.Value(), "no") == 0) {
		val = "none";
	} else {
		ad->LookupString("cryptomethods", val);
	}
	printf("%11s", val.Value());

	ad->LookupString("integrity", val);
	if (strcasecmp(val.Value(), "no") == 0) {
		val = "none";
	} else {
		val = "MD5";
	}
	printf("%10s", val.Value());

	bool bval;
	authz_ad->LookupBool(ATTR_SEC_AUTHORIZATION_SUCCEEDED, bval);
	printf(bval ? "    ALLOW " : "     DENY ");

	ad->LookupString("myremoteusername", val);
	printf("%s", val.Value());

	printf("\n");
}


void print_info(bool rv, char * addr, Sock* s, MyString name, int cmd, ClassAd *authz_ad, CondorError *errstack, int output_mode) {
	MyString cmd_map_ent;
	cmd_map_ent.formatstr ("{%s,<%i>}", addr, cmd); 

	MyString session_id;
	KeyCacheEntry *k = NULL;
	ClassAd *policy = NULL;
	int ret = 0;
	
	if(rv) {
		// IMPORTANT: this hashtable returns 0 on success!
		ret = (SecMan::command_map)->lookup(cmd_map_ent, session_id);
		if (ret) {
			printf("no cmd map!\n");
			return;
		}

		// IMPORTANT: this hashtable returns 1 on success!
		ret = (SecMan::session_cache)->lookup(session_id.Value(), k);
		if (!ret) {
			printf("no session!\n");
			return;
		}

		policy = k->policy();
	}


	if (output_mode == 0) {
		// print nothing!!
	} else if (output_mode == 1) {
		print_useful_info_1(rv, name, s, policy, authz_ad, errstack);
	} else if (output_mode == 2) {
		print_useful_info_2(rv, cmd, name, s, policy, authz_ad, errstack);
	} else if (output_mode == 10) {
		print_useful_info_10(rv, name, s, policy, authz_ad, errstack);
	}
}


int getSomeCommandFromString ( const char * cmdstring ) {

	int res = -1;

	res = getPermissionFromString( cmdstring );
	if (res != -1) {
		res = getSampleCommand( res );
		dprintf( D_ALWAYS, "recognized %s as authorization level, using command %i.\n", cmdstring, res );
		return res;
	}

	res = getCommandNum( cmdstring );
	if (res != -1) {
		dprintf( D_ALWAYS, "recognized %s as command name, using command %i.\n", cmdstring, res );
		return res;
	}

	res = atoi ( cmdstring );
	if (res > 0 || (strcmp("0", cmdstring) == 0)) {
		char compare_conversion[20];
		sprintf(compare_conversion, "%i", res);
		if (strcmp(cmdstring, compare_conversion) == 0) {
			dprintf( D_ALWAYS, "recognized %i as command number.\n", res );
			return res;
		}
	}

	return -1;

}


bool do_item(Daemon* d, MyString name, int num, int output_mode) {

	CondorError errstack;
	ClassAd authz_ad;
	bool sc_success;
	ReliSock *sock = NULL;
	bool fn_success = false;

	sock = (ReliSock*) d->makeConnectedSocket( Stream::reli_sock, 0, 0, &errstack );
	if (sock) {

		sc_success = d->startSubCommand(DC_SEC_QUERY, num, sock, 0, &errstack);
		if (sc_success) {

			sock->decode();
			if (getClassAd(sock, authz_ad) &&
				sock->end_of_message()) {
				fn_success = true;
			}
		}
	}

	print_info(fn_success, d->addr(), sock, name, num, &authz_ad, &errstack, output_mode);

	return fn_success;

}




int main( int argc, char *argv[] )
{
	char *pool=0;
	char *name=0;
	char *address=0;
	char *optional_config=0;
	int  output_mode = -1;
	daemon_t dtype = DT_NONE;
	int i;

	ExtArray<MyString> worklist_name;
	ExtArray<int> worklist_number;
	int worklist_count = 0;
	Daemon * daemon = NULL;

	myDistro->Init( argc, argv );
	config();

	for( i=1; i<argc; i++ ) {
		if(!strncmp(argv[i],"-help",strlen(argv[i]))) {
			usage(argv[0]);
			exit(0);
		} else if(!strncmp(argv[i],"-quiet",strlen(argv[i]))) {	
			if(output_mode == -1) {
				output_mode = 0;
			} else {
				fprintf(stderr,"ERROR: only one output mode may be specified.\n\n");
				usage(argv[0]);
				exit(1);
			}
		} else if(!strncmp(argv[i],"-table",strlen(argv[i]))) {	
			if(output_mode == -1) {
				output_mode = 10;
			} else {
				fprintf(stderr,"ERROR: only one output mode may be specified.\n\n");
				usage(argv[0]);
				exit(1);
			}
		} else if(!strncmp(argv[i],"-verbose",strlen(argv[i]))) {	
			if(output_mode == -1) {
				output_mode = 2;
			} else {
				fprintf(stderr,"ERROR: only one output mode may be specified.\n\n");
				usage(argv[0]);
				exit(1);
			}
		} else if(!strncmp(argv[i],"-config",strlen(argv[i]))) {	
			i++;
			if(!argv[i]) {
				fprintf(stderr,"ERROR: -config requires an argument.\n\n");
				usage(argv[0]);
				exit(1);
			}
			optional_config = argv[i];
		} else if(!strncmp(argv[i],"-pool",strlen(argv[i]))) {	
			i++;
			if(!argv[i]) {
				fprintf(stderr,"ERROR: -pool requires an argument.\n\n");
				usage(argv[0]);
				exit(1);
			}
			if(address) {
				fprintf(stderr,"ERROR: -address cannot be used with -pool or -name.\n\n");
				usage(argv[0]);
				exit(1);
			}
			pool = argv[i];
		} else if(!strncmp(argv[i],"-name",strlen(argv[i]))) {	
			i++;
			if(!argv[i]) {
				fprintf(stderr,"ERROR: -name requires an argument.\n\n");
				usage(argv[0]);
				exit(1);
			}
			if(address) {
				fprintf(stderr,"ERROR: -address cannot be used with -pool or -name.\n\n");
				usage(argv[0]);
				exit(1);
			}
			name = argv[i];
		} else if(!strncmp(argv[i],"-type",strlen(argv[i]))) {	
			i++;
			if(!argv[i]) {
				fprintf(stderr,"ERROR: -type requires an argument.\n\n");
				usage(argv[0]);
				exit(1);
			}
			dtype = stringToDaemonType(argv[i]);
			if( dtype == DT_NONE) {
				fprintf(stderr,"ERROR: unrecognized daemon type: %s\n\n", argv[i]);
				usage(argv[0]);
				exit(1);
			}
		} else if(!strncmp(argv[i],"-address",strlen(argv[i]))) {	
			i++;
			if(!argv[i]) {
				fprintf(stderr,"ERROR: -address requires an argument.\n\n");
				usage(argv[0]);
				exit(1);
			}
			if(pool || name) {
				fprintf(stderr,"ERROR: -address cannot be used with -pool or -name.\n\n");
				usage(argv[0]);
				exit(1);
			}
			address = argv[i];
		} else if(!strcmp(argv[i],"-version")) {
			version();
			exit(0);
		} else if(!strcmp(argv[i],"-debug")) {
				// dprintf to console
			dprintf_set_tool_debug("TOOL", 0);
		} else if(argv[i][0]!='-' || !strcmp(argv[i],"-")) {
			// a special case
			if(strcasecmp("ALL", argv[i]) == 0) {
				worklist_name[worklist_count++] = "ALLOW";
				worklist_name[worklist_count++] = "READ";
				worklist_name[worklist_count++] = "WRITE";
				worklist_name[worklist_count++] = "NEGOTIATOR";
				worklist_name[worklist_count++] = "ADMINISTRATOR";
				worklist_name[worklist_count++] = "OWNER";
				worklist_name[worklist_count++] = "CONFIG";
				worklist_name[worklist_count++] = "DAEMON";
				worklist_name[worklist_count++] = "ADVERTISE_STARTD";
				worklist_name[worklist_count++] = "ADVERTISE_SCHEDD";
				worklist_name[worklist_count++] = "ADVERTISE_MASTER";
			} else {
				// an individual item to act on
				worklist_name[worklist_count++] = argv[i];
			}
		} else {
			fprintf(stderr,"ERROR: Unknown argument: %s\n\n",argv[i]);
			usage(argv[0]);
			exit(1);
		}
	}


	// 1 (normal) is the default
	if(output_mode == -1) {
		output_mode = 1;
	}

	// use some default
	if(worklist_count == 0) {
		if(output_mode) {
			fprintf( stderr, "WARNING: Missing <authz-level | command-name | command-int> argument, defaulting to DC_NOP\n");
		}
		worklist_name[0] = "DC_NOP";
		worklist_count++;
	}


	// convert each item
	bool all_okay = true;
	for (i=0; i<worklist_count; i++) {
		int c = getSomeCommandFromString(worklist_name[i].Value());
		if (c == -1) {
			if(output_mode) {
				fprintf(stderr, "ERROR: Could not understand TOKEN \"%s\".\n", worklist_name[i].Value());
			}
			all_okay = false;
		} else {
			worklist_number[i] = c;
		}
	}
	if (!all_okay) {
		exit(1);
	}


	//
	// LETS GET TO WORK!
	//

	if(dtype == DT_NONE) {
		dtype = DT_SCHEDD;
	}

	if(address) {
		daemon = new Daemon( DT_ANY, address, 0 );
	} else {
		if (pool) {
			DCCollector col( pool );
			if( ! col.addr() ) {
				fprintf( stderr, "ERROR: %s\n", col.error() );
				exit(1);
			}
			daemon = new Daemon( dtype, name, col.addr() );
		} else {
			daemon = new Daemon( dtype, name );
		}
	}

	if (!(daemon->locate())) {
		if(output_mode) {
			fprintf(stderr, "ERROR: couldn't locate %s!\n", address?address:name);
		}
		delete daemon;
		exit(1);
	}

	// do we need to print headers?
	if(output_mode == 10) {
		printf ("         Instruction Authentication Encryption Integrity Decision Identity\n");
	}

	// load the supplied config if specified
	if (optional_config) {
		process_config_source( optional_config, "special config", NULL, true);
		//process_config_source( optional_config, "special config", get_local_hostname().Value(), true);

		// ZKM TODO FIXME check the success of loading the config
	}

	all_okay = true;
	for(i=0; i<worklist_count; i++) {
		// any item failing induces failure of whole program
		if (!do_item(daemon, worklist_name[i], worklist_number[i], output_mode)) {
			all_okay = false;
		}
	}

	if(daemon) {
		delete daemon;
		daemon = NULL;
	}

	return (all_okay ? 0 : 1);

}

