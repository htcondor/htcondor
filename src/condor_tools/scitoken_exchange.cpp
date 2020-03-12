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
#include "dc_schedd.h"
#include "directory.h"
#include "token_utils.h"

void print_usage(const char *argv0) {
	fprintf(stderr, "Usage: %s [-type TYPE] [-name NAME] [-pool POOL] -scitoken FILENAME [-token NAME]\n\n"
		"Exchanges a SciToken from a remote daemon and prints its contents to stdout.\n"
		"\nToken options:\n"
		"    -scitoken <val>                 File containing SciToken to exchange\n"
		"Specifying target options:\n"
		"    -pool    <host>                 Query this collector\n"
		"    -name    <name>                 Find a daemon with this name\n"
		"    -type    <subsystem>            Type of daemon to contact (default: SCHEDD)\n"
		"\nOther options:\n"
		"    -token    <NAME>                Name of token file\n", argv0);
	exit(1);
}


int
exchange_scitoken(const std::string &pool, const std::string &name, daemon_t dtype,
	const std::string &scitoken_filename, const std::string &token_name)
{
	std::unique_ptr<FILE, decltype(&::fclose)> f(
		safe_fopen_no_create(scitoken_filename.c_str(), "r"),
		&::fclose);

	if (!f.get()) {
		fprintf(stderr, "Failed to open file '%s' for reading: '%s' (%d).\n",
			scitoken_filename.c_str(), strerror(errno), errno);
		return 2;
	}

	std::string scitoken;
	for (std::string line; readLine(line, f.get(), false);) {
		trim(line);
		if (line.empty() || line[0] == '#') {continue;}
		scitoken = line;
		break;
	}
	if (scitoken.empty()) {
		fprintf(stderr, "File %s contains no scitokens.\n", scitoken_filename.c_str());
		return 2;
	}

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
	std::string token;
	if (!daemon->exchangeSciToken(scitoken, token, err)) {
		fprintf(stderr, "Failed to exchange SciToken: %s\n", err.getFullText().c_str());
		exit(1);
	}
	return htcondor::write_out_token(token_name, token, "");
}


int main(int argc, char *argv[]) {

	myDistro->Init( argc, argv );
	set_priv_initialize();
	config();

	daemon_t dtype = DT_SCHEDD;
	std::string pool;
	std::string name;
	std::string identity;
	std::string token_name;
	std::string scitoken_filename;
	for (int i = 1; i < argc; i++) {
		if (is_dash_arg_prefix(argv[i], "scitoken", 1)) {
			i++;
			if (!argv[i]) {
				fprintf(stderr, "%s: -scitoken requires a filename argument\n", argv[0]);
				exit(1);
			}
			scitoken_filename = argv[i];
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

	if (scitoken_filename.empty())
	{
		fprintf(stderr, "%s: Must provide -scitoken argument to exchange a token.\n", argv[0]);
		return 1;
	}

	return exchange_scitoken(pool, name, dtype, scitoken_filename, token_name);
}
