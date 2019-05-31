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

#include "condor_auth_passwd.h"
#include "match_prefix.h"
#include "CondorError.h"
#include "daemon.h"
#include "dc_collector.h"
#include "directory.h"

void print_usage(const char *argv0) {
	fprintf(stderr, "Usage: %s [-type TYPE] [-name NAME] [-pool POOL] [-reqid ID]\n\n"
		"Generates a token from a remote daemon and prints its contents to stdout.\n"
		"\nToken options:\n"
		"    -reqid <val>                    Token request identity\n"
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
approve_token(const std::string &pool, const std::string &name, daemon_t dtype)
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

	CondorError err;

	std::vector<classad::ClassAd> results;
	if (!daemon->listTokenRequest(request_id, results, &err)) {
		fprintf(stderr, "Failed to locate request corresponding to ID %s: %s\n", request_id.c_str(),
			err.getFullText().c_str());
		exit(1);
	}
	if (results.size() != 1) {
		fprintf(stderr, "Remote daemon did not provide information for request ID %s.\n", request_id.c_str());
		exit(1);
	}

	for (const auto &request_ad : results) {
		classad::ClassAdUnParser unp;
		std::string req_contents_str;
		unp.SetOldClassAd(true);
		unp.Unparse(req_contents_str, &req_contents);

		printf("%s\n", req_contents_str.c_str());
	}

	return 0;
}

int main(int argc, char *argv[]) {

	daemon_t dtype = DT_COLLECTOR;
	std::string pool;
	std::string name;
	std::string reqid;
	for (int i = 1; i < argc; i++) {
		if (is_dash_arg_prefix(argv[i], "pool", 1)) {
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
		} else if(!strcmp(argv[i],"-debug")) {
			// dprintf to console
			dprintf_set_tool_debug("TOOL", 0);
		} else if (is_dash_arg_prefix(argv[i], "help", 1)) {
			print_usage(argv[0]);
			exit(1);
		} else {
			fprintf(stderr, "%s: Invalid command line argument: %s\n", argv[0], argv[i]);
			print_usage(argv[0]);
			exit(1);
		}
	}

	config();

	return approve_token(pool, name, dtype, reqid);
}
