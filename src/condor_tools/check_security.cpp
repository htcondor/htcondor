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

void
usage( char *cmd )
{
	fprintf(stderr,"Usage: %s [options] <authz-level | command-name | command-int>\n",cmd);
	fprintf(stderr,"Where options are:\n");
	fprintf(stderr,"    -help             Display options\n");
	fprintf(stderr,"    -version          Display Condor version\n");
	fprintf(stderr,"    -addr <hostname>  Use this sinful string\n");
	fprintf(stderr,"    -debug            Show extra debugging info\n");
	fprintf(stderr,"\nExample: %s -debug WRITE -addr \"<127.0.0.1:9618>\"\n\n",cmd);
}

void
version()
{
	printf( "%s\n%s\n", CondorVersion(), CondorPlatform() );
}



void process_err_stack(CondorError *errstack) {

	printf("%s\n", errstack->getFullText(true).c_str());

}


void print_useful_info(ClassAd *ad) {
	MyString  val;

	ad->LookupString("remoteversion", val);
	printf("Remote Version: %s\n", val.Value());
	val = CondorVersion();
	printf("Local  Version: %s\n", val.Value());

	printf ("\n");

	ad->LookupString("authentication", val);
	if (strcasecmp(val.Value(), "no") == 0) {
		printf("Authentication: none\n");
	} else {
		ad->LookupString("authmethods", val);
		printf("Authentication: %s\n", val.Value());
	}

	ad->LookupString("encryption", val);
	if (strcasecmp(val.Value(), "no") == 0) {
		printf("Encryption:     none\n");
	} else {
		ad->LookupString("cryptomethods", val);
		printf("Encryption:     %s\n", val.Value());
	}

	ad->LookupString("integrity", val);
	if (strcasecmp(val.Value(), "no") == 0) {
		printf("Integrity:      none\n");
	} else {
		printf("Integrity:      MD5\n");
	}

	printf ("\n");

	ad->LookupString("sid", val);
	printf("Session ID:     %s\n", val.Value());

	ad->LookupString("myremoteusername", val);
	printf("Remote Mapping: %s\n", val.Value());

	printf ("\n");

}



void print_info(char * addr,  int cmd, CondorError *errstack) {

	MyString cmd_map_ent;
	cmd_map_ent.formatstr ("{%s,<%i>}", addr, cmd); 

	MyString session_id;
	int ret = (SecMan::command_map)->lookup(cmd_map_ent, session_id);

	// IMPORTANT: this hashtable returns 0 on success!
	if (ret) {
		printf("no cmd map!\n");
		return;
	}

	// IMPORTANT: this hashtable returns 1 on success!
	KeyCacheEntry *k;
	ret = (SecMan::session_cache)->lookup(session_id.Value(), k);
	if (!ret) {
		printf("no session!\n");
		return;
	}

	ClassAd *policy = k->policy();
	print_useful_info(policy);
	process_err_stack(errstack);

}


int getSampleCommand( int authz_level ) {
	switch(authz_level) {
		case ALLOW:
			return DC_NOP;
		case READ:
			return CONFIG_VAL;
		case WRITE:
			return DC_RECONFIG;
		case NEGOTIATOR:
			return NEGOTIATE;
		case ADMINISTRATOR:
			return DC_OFF_FAST;
		case OWNER:
			return VACATE_CLAIM;
		case CONFIG_PERM:
			return STORE_POOL_CRED;
		case DAEMON:
			return DC_TIME_OFFSET;
		case ADVERTISE_STARTD_PERM:
			return UPDATE_STARTD_AD;
		case ADVERTISE_SCHEDD_PERM:
			return UPDATE_SCHEDD_AD;
		case ADVERTISE_MASTER_PERM:
			return UPDATE_MASTER_AD;
	}

	return -1;

}

int getSomeCommandFromString ( char * cmdstring ) {

	int res = -1;

	res = getPermissionFromString( cmdstring );
	if (res != -1) {
		res = getSampleCommand( res );
		printf( "recognized %s as authorization level, using command %i.\n", cmdstring, res );
		return res;
	}

	res = getCommandNum( cmdstring );
	if (res != -1) {
		printf( "recognized %s as command name, using command %i.\n", cmdstring, res );
		return res;
	}

	res = atoi ( cmdstring );
	if (res > 0) {
		char compare_conversion[20];
		sprintf(compare_conversion, "%i", res);
		if (strcmp(cmdstring, compare_conversion) == 0) {
			printf( "recognized %i as command number.\n", res );
			return res;
		}
	}

	return -1;

}



int main( int argc, char *argv[] )
{
	char *address=0;
	char *optional_config=0;
	int subcmd=-1;
	int i;

	myDistro->Init( argc, argv );
	config();

	for( i=1; i<argc; i++ ) {
		if(!strcmp(argv[i],"-help")) {
			usage(argv[0]);
			exit(0);
		} else if(!strncmp(argv[i],"-config",strlen(argv[i]))) {	
			i++;
			if(!argv[i]) {
				fprintf(stderr,"-config requires an argument.\n\n");
				usage(argv[0]);
				exit(1);
			}
			optional_config = argv[i];
		} else if(!strncmp(argv[i],"-address",strlen(argv[i]))) {	
			i++;
			if(!argv[i]) {
				fprintf(stderr,"-address requires an argument.\n\n");
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
			if(subcmd==-1) {
				subcmd = getSomeCommandFromString(argv[i]);
				if(subcmd==-1) {
					fprintf(stderr,"Not recognized as an authorization level, command name, or command int: %s\n\n",argv[i]);
					usage(argv[0]);
					exit(1);
				}
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

	// sanity check invocation
	if(subcmd==-1) {
		printf("Missing <authz-level | command-name | command-int> argument, defaulting to DC_NOP\n\n");
		subcmd = DC_NOP;
	}

	// load the supplied config if specified
	if (optional_config) {
		process_config_source( optional_config, "special config", NULL, true);
		//process_config_source( optional_config, "special config", get_local_hostname().Value(), true);
	}


	CondorError errstack;

	Daemon* daemon;
	if(address) {
		daemon = new Daemon( DT_ANY, address, 0 );
		if (!(daemon->locate())) {
			printf("couldn't local %s!  check the -address argument.\n", address);
			exit(1);
		}
	} else {
		printf("Missing -address argument, attempting to talk to local condor_master\n");
		daemon = new Daemon( DT_MASTER, NULL, 0 );
		if (!(daemon->locate())) {
			printf("couldn't locate local master!  use the -address argument.\n");
			exit(1);
		}
	}

	ReliSock *sock = NULL;
	int return_code = 1;

	if( (sock = (ReliSock*)(daemon->startSubCommand(DC_AUTHENTICATE, subcmd, Stream::reli_sock, 0, &errstack))) ) {
		sock->end_of_message();
		delete sock;

		printf("\nSUCCESS!\n\n");
		print_info(daemon->addr(), subcmd, &errstack);

		return_code = 0;
	} else {
		printf("\nFAILED!\n\n");
		process_err_stack(&errstack);
		return_code = 1;
	}

	delete daemon;
	return return_code;

}

