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

#include <iostream>

void printRemainingRequests(std::unique_ptr<Daemon> daemon);

void print_usage(const char *argv0) {
	fprintf(stderr, "Usage: %s [-type TYPE] [-name NAME] [-pool POOL] [-lifetime LIFETIME] [-netblock NETBLOCK]\n\n"
		"Generates a new rule at specified daemon to automatically approve requests.\n"
		"\nOptions:\n"
		"    -netblock <netblock>            Approve requests coming from this network\n"
		"                                    Example: 192.168.0.0/24\n"
		"    -lifetime <val>                 Auto-approval lifetime in seconds\n"
		"Specifying target options:\n"
		"    -pool    <host>                 Query this collector\n"
		"    -name    <name>                 Find a daemon with this name\n"
		"    -type    <subsystem>            Type of daemon to contact (default: COLLECTOR)\n"
		"If not specified, the pool's collector is targeted.\n",
		argv0
	);
	exit(1);
}

int
auto_approve(const std::string &pool, const std::string &name, daemon_t dtype,
	const std::string netblock, time_t lifetime)
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

	if (!(daemon->locate(Daemon::LOCATE_FOR_ADMIN))) {
		if (!name.empty()) {
			fprintf(stderr, "ERROR: couldn't locate daemon %s!\n", name.c_str());
		} else {
			fprintf(stderr, "ERROR: couldn't locate default daemon type.\n");
		}
		exit(1);
	}

	CondorError err;

	if (!daemon->autoApproveTokens(netblock, lifetime, &err)) {
		fprintf(stderr, "Failed to create new auto-approval rule: %s\n",
			err.getFullText().c_str());
		exit(1);
	}

	printf("Successfully installed auto-approval rule for netblock %s with lifetime of %.2f hours\n",
		netblock.c_str(), static_cast<float>(lifetime)/3600 );

	printRemainingRequests(std::move(daemon));
	return 0;
}

void printRemainingRequests(std::unique_ptr<Daemon> daemon) {

	CondorError err;
	std::vector<classad::ClassAd> results;
	if (!daemon->listTokenRequest("", results, &err)) {
		fprintf(stderr, "Failed to list remaining token requests: %s\n", err.getFullText().c_str());
		return;
	}
	if (results.empty()) {
		fprintf(stderr, "Remote daemon reports no un-approved requests pending.\n");
		return;
	}

	printf("The following requests remain after auto-approval rule was installed:\n");
	printf("==================================================\n");
	for (const auto &request_ad : results) {
		classad::ClassAdUnParser unp;
		std::string req_contents_str;
		unp.SetOldClassAd(true);
		unp.Unparse(req_contents_str, &request_ad);

		printf("%s\n", req_contents_str.c_str());
	}
	printf("==================================================\n");
	printf("To approve these requests, please run condor_token_request_approve manually.\n");
}

int main(int argc, const char *argv[]) {

	set_priv_initialize();
	config();

	daemon_t dtype = DT_COLLECTOR;
	const char * pcolon;
	std::string pool;
	std::string name;
	std::string netblock;
	time_t lifetime = 3600;
	for (int i = 1; i < argc; i++) {
		if (is_dash_arg_prefix(argv[i], "lifetime", 1)) {
			i++;
			if (!argv[i]) {
				fprintf(stderr, "%s: -lifetime requires an argument.\n", argv[0]);
				exit(1);
			}
			try {
				lifetime = std::stol(argv[i]);
			} catch (...) {
				fprintf(stderr, "%s: Invalid argument for -lifetime: %s.\n", argv[0], argv[i]);
				exit(1);
			}
		} else if (is_dash_arg_prefix(argv[i], "netblock", 2)) {
			i++;
			if (!argv[i]) {
				fprintf(stderr, "%s: -netblock request an argument.\n", argv[0]);
				exit(1);
			}
			netblock = argv[i];
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

	if (netblock.empty()) {
		fprintf(stderr, "%s: -netblock argument is required.\n"
			"Example: %s -netblock 192.168.0.0/24\n",
			argv[0], argv[0]);
		exit(1);
	}

	return auto_approve(pool, name, dtype, netblock, lifetime);
}
