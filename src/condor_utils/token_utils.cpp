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

#include "token_utils.h"

#include "store_cred.h"
#include "subsystem_info.h"
#include "condor_netdb.h"
#include "condor_config.h"
#include "condor_string.h"
#include "directory.h"
#include "condor_random_num.h"

#include <string>

int
htcondor::write_out_token(const std::string &token_name, const std::string &token, const std::string &owner)
{
	if (token_name.empty()) {
		printf("%s\n", token.c_str());
		return 0;
	}

	TemporaryPrivSentry tps( !owner.empty() );
	auto subsys = get_mySubSystem();
	if (!owner.empty()) {
		if (!init_user_ids(owner.c_str(), NULL)) {
			dprintf(D_FAILURE, "write_out_token(%s): Failed to switch to user priv\n", owner.c_str());
			return 0;
		}
		set_user_priv();
	} else if (subsys->isDaemon()) {
		set_priv(PRIV_ROOT);
	}

	std::string dirpath;
	if (!owner.empty() || !param(dirpath, "SEC_TOKEN_DIRECTORY")) {
		std::string file_location;
		if (!find_user_file(file_location, "tokens.d", false, !owner.empty())) {
			if (!owner.empty()) {
				dprintf(D_FULLDEBUG, "write_out_token(%s): Unable to find token file for owner.\n",
					owner.c_str());
				return 0;
			}
			param(dirpath, "SEC_TOKEN_SYSTEM_DIRECTORY");
		} else {
			dirpath = file_location;
		}
	}
	mkdir_and_parents_if_needed(dirpath.c_str(), 0700);

	// Use condor_basename on the token_name to avoid any '../../' issues; since
	// we may be writing out a token as root, we want to be certain we are writing it
	// in the dirpath we got out of the condor_config configuration.
	std::string token_file = dirpath + DIR_DELIM_CHAR + condor_basename( token_name.c_str() );
    int fd = safe_create_keep_if_exists(token_file.c_str(),
        O_CREAT | O_APPEND | O_WRONLY, 0600);
	if (-1 == fd) {
		fprintf(stderr, "Cannot write token to %s: %s (errno=%d)\n",
			token_file.c_str(), strerror(errno), errno);
		return 1;
	}
	auto result = _condor_full_write(fd, token.c_str(), token.size());
	if (result != static_cast<ssize_t>(token.size())) {
		fprintf(stderr, "Failed to write token to %s: %s (errno=%d)\n",
			token_file.c_str(), strerror(errno), errno);
		close(fd);
		return 1;
	}
	std::string newline = "\n";
	_condor_full_write(fd, newline.c_str(), 1);
	close(fd);
	return 0;
}


std::string
htcondor::generate_client_id()
{
	std::string subsys_name = get_mySubSystemName();

	char hostname[MAXHOSTNAMELEN];
	if (condor_gethostname(hostname, sizeof(hostname))) { // returns 0 or -1
		hostname[0] = 0;
	}

	return subsys_name + "-" + std::string(hostname) + "-" +
		std::to_string(get_csrng_uint() % 100000);
}


std::string
htcondor::get_token_signing_key(CondorError &err) {
	auto_free_ptr key_name(param("SEC_TOKEN_ISSUER_KEY"));
	if (key_name) {
		if (hasTokenSigningKey(key_name.ptr(), &err)) {
			return key_name.ptr();
		}
	} else if (hasTokenSigningKey("POOL", &err)) {
		return "POOL";
	}
	err.push("TOKEN_UTILS", 4, "Server does not have a signing key configured.");
	return "";
}
