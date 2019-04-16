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

#include <string>
#include "condor_config.h"
#include "condor_string.h"
#include "directory.h"

int
write_out_token(const std::string &token_name, const std::string &token)
{
	if (token_name.empty()) {
		printf("%s\n", token.c_str());
		return 0;
	}
	std::string dirpath;
	if (!param(dirpath, "SEC_TOKEN_DIRECTORY")) {
		MyString file_location;
		if (!find_user_file(file_location, "tokens.d", false)) {
			param(dirpath, "SEC_TOKEN_SYSTEM_DIRECTORY");
		} else {
			dirpath = file_location;
		}
	}
	mkdir_and_parents_if_needed(dirpath.c_str(), 0700);

	std::string token_file = dirpath + DIR_DELIM_CHAR + token_name;
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
		return 1;
	}
	std::string newline = "\n";
	_condor_full_write(fd, newline.c_str(), 1);
	return 0;
}
