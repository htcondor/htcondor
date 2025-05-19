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

// unit tests for the functions related to the MACRO_SET data structure
// used by condor's configuration and submit language.

#include "condor_common.h"
#include "condor_config.h"
#include "param_info.h"
#include "match_prefix.h"
#include "daemon.h"
#include "dc_collector.h"
#include "dc_schedd.h"

#include <stdio.h>
#include <vector>
#include <string>

bool dash_verbose = false;

void Usage(const char * appname, FILE * out);


//
int main( int /*argc*/, const char ** argv) {

	const char * pcolon;
	bool stdin_file_stream = false;
	std::vector< std::pair<const char *, ClassAdFileParseType::ParseType> > slotad_files;
	std::vector<std::string> claim_ids;
	std::vector<ClassAd> slot_ads;
	std::string user;
	const char * schedd_name = nullptr;
	const char * pool_name = nullptr;
	int timeout = 120;

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
		} else if (is_dash_arg_colon_prefix(arg, "slotad",  &pcolon, 1)) {
			const char * file = argv[ii+1];
			// we allow the first filename to be -, but that can only happen once.
			if (file && (file[0] != '-' || file[1] != 0) && (slotad_files.empty() || file[0] != '-')) {
				ClassAdFileParseType::ParseType parse_type = ClassAdFileParseType::Parse_auto;
				if (pcolon) { parse_type = parseAdsFileFormat(++pcolon, ClassAdFileParseType::Parse_long); }
				if (slotad_files.empty()) {
					stdin_file_stream = (MATCH == strcmp(file, "-"));
				} else if (stdin_file_stream) {
					fprintf(stderr, "when -slotad is reading from stdin, only a single -slotad arg may be given.\n");
					return 1;
				}
				slotad_files.emplace_back(file, parse_type);
				++ii;
			} else {
				fprintf(stderr, "-slotad requires a filename argument\n");
				return 1;
			}
		} else if (is_dash_arg_prefix(arg, "claimid", 1)) {
			const char * claimid = argv[ii+1];
			if (claimid && claimid[0] != '-') {
				claim_ids.push_back(claimid);
				++ii;
			} else {
				fprintf(stderr, "-claimid requires a string argument\n");
				return 1;
			}
		} else if (is_dash_arg_prefix(arg, "user", 1)) {
			const char * username = argv[ii+1];
			if (username && username[0] != '-') {
				++ii;
				user = username;
			} else {
				fprintf(stderr, "-user requires a username argument\n");
				return 1;
			}
		} else if (is_dash_arg_prefix(arg, "schedd", 2)) {
			schedd_name = argv[ii+1];
			if (schedd_name && (schedd_name[0] != '-' || schedd_name[1] != 0)) {
				++ii;
			} else {
				fprintf(stderr, "-schedd requires a schedd name or address argument\n");
				return 1;
			}
		} else if (is_dash_arg_prefix(arg, "pool", 2)) {
			pool_name = argv[ii+1];
			if (pool_name && (pool_name[0] != '-' || pool_name[1] != 0)) {
				++ii;
			} else {
				fprintf(stderr, "-pool requires a collector name or address argument\n");
				return 1;
			}
		} else if (is_dash_arg_prefix(arg, "timeout", 1)) {
			const char * time = argv[ii+1];
			int sec = atoi(time?time:"");
			if (sec != 0) {
				timeout = sec;
				++ii;
			} else {
				fprintf(stderr, "-timeout requires a non-zero time argument\n");
				return 1;
			}
		} else if (is_dash_arg_colon_prefix(arg, "debug", &pcolon, 3)) {
			// dprintf to console
			dprintf_set_tool_debug("TOOL", (pcolon && pcolon[1]) ? pcolon+1 : nullptr);
		} else {
			fprintf(stderr, "unknown argument %s\n", arg);
			Usage(argv[0], stderr);
			return 1;
		}
	}

	if (slotad_files.empty()) {
		Usage(argv[0], stderr);
		return 1;
	}

	// init config, but don't fail if we can't find config files
	config_continue_if_no_config(true);
	set_priv_initialize(); // allow uid switching if root
	config();

	slot_ads.reserve(slotad_files.size());
	for (auto & [filename, parse_type] : slotad_files) {
		FILE* file = NULL;
		bool close_file = false;
		bool multi_ad_file = false;
		if (MATCH == strcmp(filename,"-")) {
			file = stdin;
			close_file = false;
			multi_ad_file = true;
		} else {
			file = safe_fopen_wrapper_follow(filename, "r");
			close_file = true;
			multi_ad_file = slotad_files.size() == 1;
		}

		if (file == NULL) {
			fprintf(stderr, "Can't open slotad file: %s\n", filename);
			return 1;
		}

		CondorClassAdFileIterator adIter;
		if ( ! adIter.begin(file, close_file, parse_type)) {
			fprintf(stderr, "Unexpected error reading file of ClassAds: %s\n", filename);
			if (close_file) { fclose(file); file = NULL; }
			return 1;
		}

		while (adIter.next(slot_ads.emplace_back()) > 0) {
			// in the multi-ad stream case, claim id ads may be interleaved with slot ads
			std::string claimid;
			if (slot_ads.back().LookupString("SplitClaimId", claimid) ||
				slot_ads.back().LookupString("ClaimId", claimid)) {
				claim_ids.push_back(claimid);
				slot_ads.pop_back();
			}
			if ( ! multi_ad_file) break;
		}
		if ( ! slot_ads.back().size()) { slot_ads.pop_back(); }
	}

	if (slot_ads.size() != claim_ids.size()) {
		fprintf(stderr, "A claim id must be specified for each slot, but there are %d slots and %d ids\n", 
			(int)slot_ads.size(), (int)claim_ids.size());
		Usage(argv[0], stderr);
		return 1;
	}

	int rval = NOT_OK;
	DCSchedd schedd(schedd_name, pool_name);
	schedd.locate();
	if (slot_ads.size() == 1) {
		rval = schedd.offerResource(claim_ids.front(), slot_ads.front(), user, timeout);
	} else {
		std::vector< std::pair<std::string, const ClassAd*> > resources;
		for (size_t ix = 0; ix < slot_ads.size(); ++ix) {
			resources.emplace_back(claim_ids[ix], &slot_ads[ix]);
		}
		rval = schedd.offerResources(resources, user, timeout);
	}

	if (rval != OK) {
		if (rval < 0) {
			fprintf(stderr, "failed to send offer to the schedd\n");
			return 1;
		} else {
			fprintf(stderr, "Schedd refused the offer\n");
			return 2;
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
	fprintf(out, "Usage: %s -slotad <file> [-claimid <file>] [-user <username>] [-schedd <schedd-name>] [-pool <collector>]\n"
		"  Sends a slot classad and claim id to the given Schedd so it can use them to run jobs\n"
		"  Multiple slotads and claim ids may be given\n"
		"  To read slot ads and claim ids from a file use a single -slotad argument\n"
		"  When reading multiple ads from a file, the filename can be - to indicate reading from stdin\n"
		"  Optional arguments\n"
		"    -user <username>\tSchedd should only run jobs owned by the given user on the resources\n"
		"    -schedd <name>\tSend the offer to the given schedd\n"
		"    -pool <collector>\tUse the given collector to locate the schedd\n"
		"    -timeout <seconds>\tTimeout the command in <seconds> seconds\n"
		"    -debug\t\tdebug logging to stderr\n"
		, appname);
}

