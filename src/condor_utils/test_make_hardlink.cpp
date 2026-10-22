/***************************************************************
 *
 * Copyright (C) 2016, Condor Team, Computer Sciences Department,
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

// test for creating hardlinks

#include "condor_common.h"
#include "condor_config.h"
#include "param_info.h"
#include "match_prefix.h"

#include <stdio.h>
#include <vector>
#include <string>
#include <filesystem>

bool dash_verbose = false;

void Usage(const char * appname, FILE * out);


//
int main( int /*argc*/, const char ** argv) {

	const char * pcolon;
	const char * destfile = nullptr;
	const char * srcfile = nullptr;

	// if we don't init dprintf, calls to it will be will be malloc'ed and stored
	// for later. this form of init will match the fewest possible issues.
	dprintf_config_tool_on_error("D_ERROR");
	dprintf_OnExitDumpOnErrorBuffer(stderr);

	for (int ii = 1; argv[ii]; ++ii) {
		const char *arg = argv[ii];
		if (is_dash_arg_colon_prefix(arg, "verbose", &pcolon, 1)) {
			dash_verbose = 1;
		} else if (is_dash_arg_prefix(arg, "help", 1)) {
			Usage(argv[0], stdout);
			return 0;
		} else if (is_dash_arg_colon_prefix(arg, "debug", &pcolon, 3)) {
			// dprintf to console
			dprintf_set_tool_debug("TOOL", (pcolon && pcolon[1]) ? pcolon+1 : nullptr);
		} else if ( ! destfile) {
			destfile = arg;
		} else if ( ! srcfile) {
			srcfile = arg;
		} else {
			fprintf(stderr, "unknown argument %s\n", arg);
			Usage(argv[0], stderr);
			return 1;
		}
	}

	// init config, but don't fail if we can't find config files
	config_continue_if_no_config(true);
	set_priv_initialize(); // allow uid switching if root
	config();

	if (destfile && srcfile) {
		std::error_code ec;
		std::filesystem::create_hard_link(srcfile, destfile, ec);
		if( ec.value() != 0 ) {
			fprintf(stderr, "Failed to create_hard_link(%s, %s): %s (%d)\n",
				srcfile, destfile, ec.message().c_str(), ec.value() );
			return ec.value();
		}
		if (dash_verbose) {
			fprintf(stdout, "Created hardlink %s to existing file %s\n", destfile, srcfile);
		}

	}

	return 0;
}

void Usage(const char * appname, FILE * out)
{
	const char * p = appname;
	while (*p) {
		if (*p == '/' || *p == '\\') appname = p+1;
		++p;
	}
	fprintf(out,
		"Usage: %s [ <Optional> ] <destination-file> <source-file>\n", appname);
	fprintf(out, "Creates <destination-file> as a hard link to <source-file>\n"
		"  Optional arguments\n"
		"    -verbose\tverbose output\n"
		"    -debug\t\tdebug logging to stderr\n"
		"    -help\tPrint this message\n"
		);
}

