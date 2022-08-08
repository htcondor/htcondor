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
#include "token_utils.h"

void print_usage(const char *argv0) {
	fprintf(stderr, "Usage: %s -identity USER@DOMAIN [-key KEYID] [-authz AUTHZ] [-lifetime VAL] [-token NAME]\n"
		"Generates a token from the local password.\n"
		"\nToken options:\n"
		"    -authz    <authz>                Whitelist one or more authorization\n"
		"    -lifetime <val>                  Max token lifetime, in seconds\n"
		"    -identity <USER@DOMAIN>          User identity\n"
		"    -key      <KEYID>                Key to use for signing\n"
		"\nOther options:\n"
		"    -token    <NAME>                 Name of token file\n", argv0);
	exit(1);
}

int main(int argc, const char *argv[]) {

	if (argc < 3) {
		print_usage(argv[0]);
	}

	set_priv_initialize();
	config();

	const char * pcolon;
	std::string pool;
	std::string name;
	std::string identity;
	std::string key = "POOL";
	std::string token_name;
	std::vector<std::string> authz_list;
	long lifetime = -1;
	for (int i = 1; i < argc; i++) {
		if (is_dash_arg_prefix(argv[i], "identity", 2)) {
			i++;
			if (!argv[i]) {
				fprintf(stderr, "%s: -identity requires a condor identity as an argument\n", argv[0]);
				exit(1);
			}
			identity = argv[i];
		} else if (is_dash_arg_prefix(argv[i], "key", 1)) {
			i++;
			if (!argv[i]) {
				fprintf(stderr, "%s: -key requires a key ID as an argument\n", argv[0]);
				exit(1);
			}
			key = argv[i];
		} else if (is_dash_arg_prefix(argv[i], "authz", 1)) {
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
		} else if (is_dash_arg_prefix(argv[i], "token", 2)) {
			i++;
			if (!argv[i]) {
				fprintf(stderr, "%s: -token requires a file name argument.\n", argv[0]);
				exit(1);
			}
			token_name = argv[i];
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

	CondorError err;
	std::string token;
	if (!Condor_Auth_Passwd::generate_token(identity, key, authz_list, lifetime, token, 0, &err)) {
		fprintf(stderr, "Failed to generate a token.\n");
		fprintf(stderr, "%s\n", err.getFullText(true).c_str());
		exit(2);
	}

	return htcondor::write_out_token(token_name, token, "");
}
