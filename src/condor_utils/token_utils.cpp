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
#include "ipv6_hostname.h"
#include "condor_config.h"
#include "condor_string.h"
#include "directory.h"
#include "condor_random_num.h"

#include <string>

bool
htcondor::write_out_token(const std::string &token_name, const std::string &token, const std::string &owner, bool use_tokens_dir, std::string* err_msg)
{
	if (token_name.empty()) {
		printf("%s\n", token.c_str());
		return true;
	}
	std::string local_err;
	if (err_msg == nullptr) {
		err_msg = &local_err;
	}

	TemporaryPrivSentry tps( !owner.empty() );
	auto subsys = get_mySubSystem();
	if (!owner.empty()) {
		if (!init_user_ids(owner.c_str(), NULL)) {
			formatstr(*err_msg, "Failed to switch to user priv");
			dprintf(D_ERROR, "write_out_token(%s): %s\n", token_name.c_str(), err_msg->c_str());
			return false;
		}
		set_user_priv();
	} else if (subsys->isDaemon()) {
		set_priv(PRIV_ROOT);
	}

	std::string token_file;
	if (use_tokens_dir) {
		// Use condor_basename on the token_name to avoid any '../../' issues; since
		// we may be writing out a token as root, we want to be certain we are writing it
		// in the dirpath we got out of the condor_config configuration.
		if (token_name != condor_basename(token_name.c_str())) {
			formatstr(*err_msg, "Token name isn't a plain filename");
			dprintf(D_FAILURE, "write_out_token(%s): %s\n", token_name.c_str(), err_msg->c_str());
			return false;
		}
		std::string dirpath;
		if (!owner.empty() || !param(dirpath, "SEC_TOKEN_DIRECTORY")) {
			std::string file_location;
			if (!find_user_file(file_location, "tokens.d", false, !owner.empty())) {
				if (!owner.empty()) {
					formatstr(*err_msg, "Unable to find token directory for owner %s", owner.c_str());
					dprintf(D_FULLDEBUG, "write_out_token(%s): %s\n",
						token_name.c_str(), err_msg->c_str());
					return false;
				}
				param(dirpath, "SEC_TOKEN_SYSTEM_DIRECTORY");
			} else {
				dirpath = file_location;
			}
		}
		mkdir_and_parents_if_needed(dirpath.c_str(), 0700);

		token_file = dirpath + DIR_DELIM_CHAR + token_name;
	} else {
		token_file = token_name;
	}
    int fd = safe_create_keep_if_exists(token_file.c_str(),
        O_CREAT | O_TRUNC | O_WRONLY, 0600);
	if (-1 == fd) {
		formatstr(*err_msg, "Cannot write token to %s: %s (errno=%d)",
			token_file.c_str(), strerror(errno), errno);
		dprintf(D_FAILURE, "write_out_token(%s): %s\n",
			token_name.c_str(), err_msg->c_str());
		return false;
	}
	auto result = full_write(fd, token.c_str(), token.size());
	if (result != static_cast<ssize_t>(token.size())) {
		formatstr(*err_msg, "Failed to write token to %s: %s (errno=%d)",
			token_file.c_str(), strerror(errno), errno);
		dprintf(D_FAILURE, "write_out_token(%s): %s\n",
			token_name.c_str(), err_msg->c_str());
		close(fd);
		return false;
	}
	std::string newline = "\n";
	full_write(fd, newline.c_str(), 1);
	close(fd);
	return true;
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
