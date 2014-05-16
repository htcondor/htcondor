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

#include "condor_common.h"
#include "condor_config.h"
#include "match_prefix.h"
#include "param_info.h" // access to default params
#include "param_info_tables.h"
#include "condor_version.h"
#include <stdlib.h>

const char * check_configif = NULL;

void PREFAST_NORETURN
my_exit( int status )
{
	fflush( stdout );
	fflush( stderr );

	if ( ! status ) {
		clear_config();
	}

	exit( status );
}

// consume the next command line argument, otherwise return an error
// note that this function MAY change the value of i
//
static const char * use_next_arg(const char * arg, const char * argv[], int & i)
{
	if (argv[i+1]) {
		return argv[++i];
	}

	fprintf(stderr, "-%s requires an argument\n", arg);
	//usage();
	my_exit(1);
	return NULL;
}

int do_iftest(int /*out*/ &cTests);

bool dash_verbose = false;

int main(int argc, const char ** argv)
{
	bool do_test = true;

	int ix = 1;
	while (ix < argc) {
		if (is_dash_arg_prefix(argv[ix], "check-if", -1)) {
			check_configif = use_next_arg("check-if", argv, ix);
			do_test = false;
		} else if (is_dash_arg_prefix(argv[ix], "verbose", 1)) {
			dash_verbose = true;
		} else if (is_dash_arg_prefix(argv[ix], "memory-snapshot")) {
		#ifdef LINUX
			const char * filename = "tool";
			if (argv[ix+1]) filename = use_next_arg("memory-shapshot", argv, ix);
			char copy_smaps[300];
			pid_t pid = getpid();
			sprintf(copy_smaps, "cat /proc/%d/smaps > %s", pid, filename);
			int r = system(copy_smaps);
			fprintf(stdout, "%s returned %d\n", copy_smaps, r);
		#endif
		} else {
			fprintf(stderr, "unknown argument: %s\n", argv[ix]);
			my_exit(1);
		}
		++ix;
	}

	// handle check-if to valididate config's if/else parsing and help users to write
	// valid if conditions.
	if (check_configif) { 
		std::string err_reason;
		bool bb = false;
		bool valid = config_test_if_expression(check_configif, bb, err_reason);
		fprintf(stdout, "# %s: \"%s\" %s\n", 
			valid ? "ok" : "not supported", 
			check_configif, 
			valid ? (bb ? "\ntrue" : "\nfalse") : err_reason.c_str());
	}

	if (do_test) {
		printf("running standard config if tests\n");
		int cTests = 0;
		int cFail = do_iftest(cTests);
		printf("%d failures in %d tests\n", cFail, cTests);
		return cFail;
	}

	return 0;
}

static const char * const aBoolTrue[] = {
	"true", "True", "TRUE", "yes", "Yes", "YES", "t", "T", "1",
	"1.0", "0.1", ".1", "1.", "1e1", "1e10", "2.0e10",
	" true ", " 1 ",
};

static const char * const aBoolFalse[] = {
	"false", "False", "FALSE", "no", "No", "NO", "f", "F ",
	"0", "0.0", ".0", "0.", "0e1", "0.0e10", " false ", " 0 ",
};

#define CONDOR_SERIES_VERSION "8.2"
#define CONDOR_NEXT_VERSION "8.3"
static const char * const aVerTrue[] = {
	"version > 6.0", "!version >" CONDOR_SERIES_VERSION, "version > 8.1.1",
	"version > 8.1.4", "version > 7.24.29",
	"version >= " CONDOR_VERSION, "version == " CONDOR_SERIES_VERSION, "version != 8.0",
	"version == " CONDOR_VERSION, "version <= " CONDOR_SERIES_VERSION ".9",
	"version <= " CONDOR_SERIES_VERSION, "version < " CONDOR_SERIES_VERSION ".9", "version < " CONDOR_SERIES_VERSION ".16",
	"version < " CONDOR_SERIES_VERSION ".99", "version < " CONDOR_NEXT_VERSION, "version < 9.0",
	"version < 10.0", " VERSION < 10.0 ", " Version < 10.0"
};

static const char * const aVerFalse[] = {
	"version < 6.0", "version < " CONDOR_SERIES_VERSION, "version < " CONDOR_VERSION,
	"version < 8.1.4", " version < 8.1.4", "version < 8.1.4 ",
	"  version  <  8.1.4  ", "version < 7.24.29", " ! version <= " CONDOR_VERSION,
	"version == 8.0", "version == 8.0.6", "version <= 8.0.5",
	"!version >= " CONDOR_SERIES_VERSION, "version > " CONDOR_VERSION, "version > " CONDOR_SERIES_VERSION ".16",
	"version > " CONDOR_SERIES_VERSION ".99", "version > " CONDOR_SERIES_VERSION, "version > 9.0",
	"version > 10.0",
};

static const char * const aDefTrue[] = {
	"defined true", "defined false", "defined 1",
	"defined 0", "defined t", "defined f",
	"defined release_dir", "defined log",
	"defined LOG", "defined $(not_a_real_param:true)",
	"defined use ROLE", "defined use ROLE:", "defined use ROLE:Personal",
	"defined use feature", "defined use Feature:VMware",
};

static const char * const aDefFalse[] = {
	"defined", " defined ", "defined a",
	"defined not_a_real_param",
	"defined master.not_a_real_param",
	"defined $(not_a_real_param)",
	"defined use NOT", "defined use NOT:a_real_meta",
	"defined use",
};

static const char * const aBoolError[] = {
	"truthy", "falsify", "true dat",
	"0 0", "1 0", "1b1",
};

static const char * const aVerError[] = {
	"version", "version ", "version 99", "version <= aaa",
	"version =!= 8.1", "version <> 8.1", "Version < ",
	"version > 1.1", "version <= 1.1", " Ver < 9.9",
};

static const char * const aDefError[] = {
	"defined a b", "defined 11 99", "defined < 1.1",
	"defined use foo bar", // internal whitespace (or no :)
	"defined use ROLE: Personal", // internal whitespace
};

static const char * const aUnsupError[] = {
	"1 == 0", "false == false", "foo < bar", "$(foo) < $(bar)",
	"\"foo\" == \"bar\"", "$(foo) == $(bar)", "$(foo) != $(bar)",
};

#ifndef COUNTOF
  #define COUNTOF(aa)  (int)(sizeof(aa)/sizeof((aa)[0]))
#endif
#define TEST_TABLE(aa)  aa, (int)COUNTOF(aa)

typedef const char * PCSTR;
static const struct _test_set {
	const char * label;
	bool valid;
	bool result;
	PCSTR const * aTbl;
	int cTbl;
} aTestSets[] = {
	{ "bool",    true, true,  TEST_TABLE(aBoolTrue) },
	{ "bool",    true, false,  TEST_TABLE(aBoolFalse) },
	{ "version", true, true,  TEST_TABLE(aVerTrue) },
	{ "version", true, false,  TEST_TABLE(aVerFalse) },
	{ "defined", true, true,  TEST_TABLE(aDefTrue) },
	{ "defined", true, false,  TEST_TABLE(aDefFalse) },
	{ "bool",    false, false,  TEST_TABLE(aBoolError) },
	{ "version", false, false,  TEST_TABLE(aVerError) },
	{ "version", false, false,  TEST_TABLE(aDefError) },
	{ "complex", false, false, TEST_TABLE(aUnsupError) },
};

int do_iftest(int &cTests)
{
	int fail_count = 0;
	cTests = 0;

	std::string err_reason;
	bool bb = false;

	for (int ixSet = 0; ixSet < (int)COUNTOF(aTestSets); ++ixSet) {
		const struct _test_set & tset = aTestSets[ixSet];

		if (dash_verbose) {
			fprintf(stdout, "--- %s - expecting %s %s\n", 
				tset.label, tset.valid ? "ok" : "unsup", tset.valid ? (tset.result ? "true" : "false") : "");
		}

		for (int ix = 0; ix < tset.cTbl; ++ix) {
			++cTests;
			err_reason = "bogus";
			const char * cond = aTestSets[ixSet].aTbl[ix];
			bool valid = config_test_if_expression(cond, bb, err_reason);
			if ((valid != tset.valid) || (valid && (bb != tset.result))) {
				++fail_count;
				fprintf(stdout, "Test Failure: '%s' is (%s,%s) should be (%s,%s)\n",
					   cond,
					   valid ? "ok" : "unsup", bb ? "true" : "false",
					   tset.valid ? "ok" : "unsup", tset.result ? "true" : "false"
					   );
			} else if (dash_verbose) {
				fprintf(stdout, "# %s: \"%s\" %s\n", 
					valid ? "ok" : "not supported", 
					cond, valid ? (bb ? "\ntrue" : "\nfalse") : err_reason.c_str());
			}
		}
	}

	return fail_count;
}
