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
	fprintf(stderr,"Usage: %s [options] TOKEN [TOKEN [TOKEN [...]]]\n",cmd);
	fprintf(stderr,"\nwhere TOKEN is an (authorization-level | command-name | command-int)\n\n");
	fprintf(stderr,"general options:\n");
	fprintf(stderr,"    -help                           Display options\n");
	fprintf(stderr,"    -version                        Display Condor version\n");
	fprintf(stderr,"    -debug                          Show extra debugging info\n");
	fprintf(stderr,"\nspecifying target options:\n");
	fprintf(stderr,"    -address <sinful>               Use this sinful string\n");
	fprintf(stderr,"    -pool    <host>                 NOT IMPLEMENTED\n");
	fprintf(stderr,"    -name    <name>                 NOT IMPLEMENTED\n");
	fprintf(stderr,"    -type    <subsystem>            NOT IMPLEMENTED (default: SCHEDD)\n");
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


void print_useful_info(ClassAd *ad, ClassAd *authz_ad) {
	MyString  val;

	ad->LookupString("remoteversion", val);
	printf("Remote Version:              %s\n", val.Value());
	val = CondorVersion();
	printf("Local  Version:              %s\n", val.Value());

	printf ("\n");

	ad->LookupString("sid", val);
	printf("Session ID:                  %s\n", val.Value());

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

}



void print_info(char * addr,  int cmd, ClassAd *authz_ad, CondorError *errstack) {

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
	print_useful_info(policy, authz_ad);

	if (errstack->getFullText() != "") {
		printf ("Information about authentication methods that were attempted but failed:\n");
		process_err_stack(errstack);
	}

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
		dprintf( D_ALWAYS, "recognized %s as authorization level, using command %i.\n", cmdstring, res );
		return res;
	}

	res = getCommandNum( cmdstring );
	if (res != -1) {
		dprintf( D_ALWAYS, "recognized %s as command name, using command %i.\n", cmdstring, res );
		return res;
	}

	res = atoi ( cmdstring );
	if (res > 0) {
		char compare_conversion[20];
		sprintf(compare_conversion, "%i", res);
		if (strcmp(cmdstring, compare_conversion) == 0) {
			dprintf( D_ALWAYS, "recognized %i as command number.\n", res );
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
		fprintf( stderr, "Missing <authz-level | command-name | command-int> argument, defaulting to DC_NOP\n\n");
		subcmd = DC_NOP;
	}

	// load the supplied config if specified
	if (optional_config) {
		process_config_source( optional_config, "special config", NULL, true);
		//process_config_source( optional_config, "special config", get_local_hostname().Value(), true);
	}



	//
	// LETS GET TO WORK!
	//

	CondorError errstack;
	ClassAd authz_ad;

	Daemon *daemon = NULL;
	ReliSock *sock = NULL;
	int return_code = 1;

	if(address) {
		daemon = new Daemon( DT_ANY, address, 0 );
		if (!(daemon->locate())) {
			fprintf(stderr, "couldn't locate %s!  check the -address argument.\n", address);
			goto cleanup;
		}
	} else {
		fprintf( stderr, "Missing -address argument, attempting to talk to local condor_schedd\n");
		daemon = new Daemon( DT_SCHEDD, NULL, 0 );
		if (!(daemon->locate())) {
			fprintf(stderr, "couldn't locate local schedd!  use the -address argument.\n");
			goto cleanup;
		}
	}


	sock = (ReliSock*) daemon->makeConnectedSocket( Stream::reli_sock, 0, 0, &errstack );
	if (!sock) {
		goto print_results;
	}

	bool sc_success;
	sc_success = daemon->startSubCommand(DC_SEC_QUERY, subcmd, sock, 0, &errstack);
	if (!sc_success) {
		goto print_results;
	}

	sock->decode();
	if (!authz_ad.initFromStream(*sock) ||
		!sock->end_of_message()) {
		return_code = 1;
		goto print_results;
	}

	return_code = 0;


print_results:

	if (return_code == 0) {
		print_info(daemon->addr(), subcmd, &authz_ad, &errstack);
	} else {
		if(sock) {
			printf("sock exists.\n");
			MyString a = sock->getAuthenticationMethodUsed();
			if (!a.IsEmpty()) {
				printf("auth: %s\n", a.Value());
			}
		}
		process_err_stack(&errstack);
	}

cleanup:
	if(sock) {
		delete sock;
		sock = NULL;
	}

	if(daemon) {
		delete daemon;
		daemon = NULL;
	}

	return return_code;
}

