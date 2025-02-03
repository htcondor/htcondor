/***************************************************************
 *
 * Copyright (C) 2019, HTCondor Team, Computer Sciences Department,
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
#include "condor_config.h"
#include "condor_distribution.h"

#include "condor_auth_passwd.h"
#include "match_prefix.h"
#include "CondorError.h"
#include "daemon.h"
#include "dc_collector.h"
#include "directory.h"
#include "ipv6_hostname.h"
#include "token_utils.h"

void print_usage(const char *argv0) {
	fprintf(stderr, "Usage: %s [-type TYPE] [-name NAME] [-pool POOL] [-authz AUTHZ] [-lifetime VAL] [-token NAME | -file NAME]\n\n"
		"Generates a token from a remote daemon and prints its contents to stdout.\n"
		"\nToken options:\n"
		"    -authz    <authz>                Whitelist one or more authorization\n"
		"    -lifetime <val>                  Max token lifetime, in seconds\n"
		"    -identity <val>                  Requested identity in token\n"
		"Specifying target options:\n"
		"    -pool    <host>                 Query this collector\n"
		"    -name    <name>                 Find a daemon with this name\n"
		"    -type    <subsystem>            Type of daemon to contact (default: COLLECTOR)\n"
		"If not specified, the pool's collector is targeted.\n"
		"\nOther options:\n"
		"    -token    <NAME>                 Name of token file in tokens.d\n"
		"    -file     <NAME>                 Token filename\n", argv0);
	exit(1);
}

int
request_remote_token(const std::string &pool, const std::string &name, daemon_t dtype,
	const std::vector<std::string> &authz_list, long lifetime, const std::string &identity,
	const std::string &token_name, bool use_tokens_dir)
{
	std::unique_ptr<Daemon> daemon;
	if (!pool.empty()) {
		DCCollector col(pool.c_str());
		if (!col.addr()) {
			fprintf(stderr, "ERROR: %s\n", col.error());
			exit(1);
		}
		daemon.reset(new Daemon( dtype, name.c_str(), col.addr() ));
	} else {
		daemon.reset(new Daemon( dtype, name.c_str() ));
	}

	if (!(daemon->locate(Daemon::LOCATE_FOR_LOOKUP))) {
		if (!name.empty()) {
			fprintf(stderr, "ERROR: couldn't locate daemon %s!\n", name.c_str());
		} else {
			fprintf(stderr, "ERROR: couldn't locate default daemon type.\n");
		}
		exit(1);
	}

	char hostname[MAXHOSTNAMELEN];
	if (condor_gethostname(hostname, sizeof(hostname))) {
		fprintf(stderr, "ERROR: Unable to determine hostname.\n");
		exit(1);
	}

	CondorError err;
	std::string token, request_id;

	std::string client_id = std::string(hostname) + "-" + std::to_string(get_csrng_uint() % 1000);

	if (!daemon->startTokenRequest(identity, authz_list, lifetime, client_id, token, request_id, &err)) {
		fprintf(stderr, "Failed to request a new token: %s\n", err.getFullText().c_str());
		exit(1);
	}
	if (request_id.empty()) {
		fprintf(stderr, "Remote daemon did not provide a valid request ID.\n");
		exit(1);
	}
	printf("Token request enqueued.  Ask an administrator to please approve request %s.\n", request_id.c_str());

	while (token.empty()) {
		sleep(5);
		if (!daemon->finishTokenRequest(client_id, request_id, token, &err)) {
			fprintf(stderr, "Failed to get a status update for token request: %s\n", err.getFullText().c_str());
			exit(1);
		}
	}

	std::string err_msg;
	if (!htcondor::write_out_token(token_name, token, "", use_tokens_dir, &err_msg)) {
		fprintf(stderr, "%s\n", err_msg.c_str());
		exit(1);
	}
	return 0;
}

int main(int argc, const char *argv[]) {

	set_priv_initialize();
	config();

	daemon_t dtype = DT_COLLECTOR;
	const char * pcolon;
	std::string pool;
	std::string name;
	std::string identity;
	std::string token_name;
	bool use_tokens_dir = false;
	std::vector<std::string> authz_list;
	long lifetime = -1;
	for (int i = 1; i < argc; i++) {
		if (is_dash_arg_prefix(argv[i], "authz", 1)) {
			i++;
			if (!argv[i]) {
				fprintf(stderr, "%s: -authz requires an authorization name argument\n", argv[0]);
				exit(1);
			}
			authz_list.push_back(argv[i]);
		} else if (is_dash_arg_prefix(argv[i], "lifetime", 1)) {
			i++;
			if (!argv[i]) {
				fprintf(stderr, "%s: -lifetime requires an integer in seconds.\n", argv[0]);
				exit(1);
			}
			try {
				lifetime = std::stol(argv[i]);
			} catch (...) {
				fprintf(stderr, "%s: Invalid argument for -lifetime: %s.\n", argv[0], argv[i]);
				exit(1);
			}
		} else if (is_dash_arg_prefix(argv[i], "identity", 1)) {
			i++;
			if (!argv[i]) {
				fprintf(stderr, "%s: -identity requires an argument.\n", argv[0]);
				exit(1);
			}
			identity = argv[i];
		} else if (is_dash_arg_prefix(argv[i], "pool", 1)) {
			i++;
			if (!argv[i]) {
				fprintf(stderr, "%s: -pool requires a pool name argument.\n", argv[0]);
				exit(1);
			}
			pool = argv[i];
		} else if (is_dash_arg_prefix(argv[i], "name", 1)) {
			i++;
			if (!argv[i]) {
				fprintf(stderr, "%s: -name requires a daemon name argument.\n", argv[0]);
				exit(1);
			}
			name = argv[i];
		} else if (is_dash_arg_prefix(argv[i], "token", 2)) {
			i++;
			if (!argv[i]) {
				fprintf(stderr, "%s: -token requires a file name argument.\n", argv[0]);
				exit(1);
			}
			token_name = argv[i];
			use_tokens_dir = true;
		} else if (is_dash_arg_prefix(argv[i], "file", 1)) {
			i++;
			if (!argv[i]) {
				fprintf(stderr, "%s: -file requires a file name argument.\n", argv[0]);
				exit(1);
			}
			token_name = argv[i];
			use_tokens_dir = false;
		} else if (is_dash_arg_prefix(argv[i], "type", 1)) {
			i++;
			if (!argv[i]) {
				fprintf(stderr, "%s: -type requires a daemon type argument.\n", argv[0]);
				exit(1);
			}
			dtype = stringToDaemonType(argv[i]);
			if( dtype == DT_NONE) {
				fprintf(stderr, "ERROR: unrecognized daemon type: %s\n", argv[i]);
				print_usage(argv[0]);
				exit(1);
			}
		} else if(is_dash_arg_colon_prefix(argv[i], "debug", &pcolon, 1)) {
			// dprintf to console
			dprintf_set_tool_debug("TOOL", (pcolon && pcolon[1]) ? pcolon+1 : nullptr);
		} else if (is_dash_arg_prefix(argv[i], "help", 1)) {
			print_usage(argv[0]);
			exit(1);
		} else {
			fprintf(stderr, "%s: Invalid command line argument: %s\n", argv[0], argv[i]);
			print_usage(argv[0]);
			exit(1);
		}
	}

	return request_remote_token(pool, name, dtype, authz_list, lifetime, identity, token_name, use_tokens_dir);
}
