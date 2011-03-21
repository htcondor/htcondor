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

#ifndef _DAP_UTILITY_H
#define  _DAP_UTILITY_H

#include "condor_common.h"
#include "string"


#if 0
void parse_url( const std::string &url,
				std::string &protocol,
				std::string &host,
				std::string &filename);
#endif
void parse_url(const std::string &url,
			   char *protocol, char *host, char *filename);
void parse_url(const char *url,
			   char *protocol, char *host, char *filename);
char *strip_str(char *str);

// Create a predictable unique path, given a directory, basename, job id, and
// pid.  The return value points to a statically allocated string.  This
// function is not reentrant.
const char *
job_filepath(
		const char *basename,
		const char *suffix,
		const char *dap_id,
		pid_t pid
);

#endif
