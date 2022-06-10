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
#include "compat_classad.h"

#include "condor_auth_passwd.h"
#include "match_prefix.h"
#include "CondorError.h"
#include "daemon.h"
#include "dc_collector.h"
#include "directory.h"

void print_usage(const char *argv0) {
	fprintf(stderr, "Usage: %s [-type TYPE] [-name NAME] [-pool POOL] [-reqid ID] [-json]\n\n"
		"Generates a token from a remote daemon and prints its contents to stdout.\n"
		"\nToken options:\n"
		"    -reqid <val>                    Token request identity\n"
                "\nOutput options:\n"
                "    -json                           Print out tokens as JSON, line-delimited (RFC 7464)\n"
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
list_tokens(const std::string &pool, const std::string &name, daemon_t dtype, std::string request_id, bool list_json)
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
		if (request_id.empty()) {
			fprintf(stderr, "Failed to list token requests: %s\n", err.getFullText().c_str());
		} else {
			fprintf(stderr, "Failed to locate request corresponding to ID %s: %s\n", request_id.c_str(),
				err.getFullText().c_str());
		}
		exit(1);
	}
	if (!request_id.empty() && results.size() != 1) {
		fprintf(stderr, "Remote daemon did not provide information for request ID %s.\n", request_id.c_str());
		exit(1);
	} else if (results.empty()) {
		fprintf(stderr, "Remote daemon reports no requests pending.\n");
		return 0;
	}

	CondorClassAdFileParseHelper::ParseType kind = list_json ?
		CondorClassAdFileParseHelper::Parse_json :
		CondorClassAdFileParseHelper::Parse_long;
	CondorClassAdListWriter output_writer(kind);
	for (const auto &request_ad : results) {
		output_writer.writeAd(request_ad, stdout);
	}
	if (output_writer.needsFooter()) {
		output_writer.writeFooter(stdout);
	}

	return 0;
}

int main(int argc, const char *argv[]) {
	set_priv_initialize();
	config();

	bool list_json = false;
	daemon_t dtype = DT_COLLECTOR;
	const char * pcolon;
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
		} else if (is_dash_arg_prefix(argv[i], "reqid", 1)) {
			i++;
			if (!argv[i]) {
				fprintf(stderr, "%s: -reqid requires a request ID argument.\n", argv[0]);
				exit(1);
			}
			reqid = argv[i];
		} else if(is_dash_arg_colon_prefix(argv[i], "debug", &pcolon, 1)) {
			// dprintf to console
			dprintf_set_tool_debug("TOOL", (pcolon && pcolon[1]) ? pcolon+1 : nullptr);
		} else if (!strcmp(argv[i], "-json")) {
			list_json = true;
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

	return list_tokens(pool, name, dtype, reqid, list_json);
}
