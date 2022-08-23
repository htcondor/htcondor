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
		clear_global_config_table();
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

	init_global_config_table(0); // initialize the config defaults

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
			if (r != 0) {
				fprintf(stdout, "%d return copy smaps", r);
			}
			//fprintf(stdout, "%s returned %d\n", copy_smaps, r);
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
		bool valid = config_test_if_expression(check_configif, bb, NULL, NULL, err_reason);
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

//#define CONDOR_SERIES_VERSION "%D.%S" // Decade.Series
//#define CONDOR_NEXT_SERIES "%D.%T"   // Decade.Series+1
//#define CONDOR_NEXT_VERSION "%D.%S.%N" // Decade.Series.Minor+1
static const char * const aVerTrue[] = {
	"version > 6.0", "!version >%D.%S", "version > 8.1.1",
	"version > 8.1.4", "version > 7.24.29",
	"version >= " CONDOR_VERSION, "version == %D.%S", "version != 8.0",
	"version == " CONDOR_VERSION, "version <= %D.%S.%N", "version <= %D.%S.%P", "version >= %D.%S.%L",
	"version <= %D.%S", "version < %D.%S.%P", "version < %D.%S.%N",
	"version > %D.%S.%L", "version > %D.%R.%M", "version > %D.%R",
	"version < %D.%S.99", "version < %D.%T", "version < %E.0",
	"version < 12.0", " VERSION < 12.0 ", " Version < 12.0"
};

static const char * const aVerFalse[] = {
	"version < 6.0", "version < %D.%S", "version < " CONDOR_VERSION,
	"version < 8.1.4", " version < 8.1.4", "version < 8.1.4 ",
	"  version  <  8.1.4  ", "version < 7.24.29", " ! version <= " CONDOR_VERSION,
	"version == 8.0", "version == 8.0.6", "version <= 8.0.5",
	"!version >= %D.%S", "version > " CONDOR_VERSION, "version > %D.%S.%N",
	"version < %D.%S.%L", "version < %D.%R.%M", "version < %D.%R",
	"version > %D.%S.99", "version > %D.%S", "version > %E.0",
	"version > 12.0",
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


// copy the incoming condition, changing %x to to one of the fields of condor version
// where x is D, S or M for first second and third digit of the version
// and x may be the next or previous letter to indicate ver-1 or ver+1
// For instance, if the condor_version is 8.3.5  then %D.%T.%L translates as 8.4.4
// 
const char* fixup_version(const char * cond, char* buf, int cbBuf)
{
	static char vers[] = CONDOR_VERSION;
	static long vD=0, vS=0, vM=0;

	char * p;
	if ( ! vD) {
		vD = strtol(vers, &p, 10);
		vS = strtol(++p, &p, 10);
		vM = strtol(++p, &p, 10);
	}

	p = buf;
	for (int ix = 0; cond[ix]; ++ix) {
		*p = cond[ix];
		if (*p == '%') {
			long v = -99;
			int ch = cond[ix+1];
			if (ch >= 'D'-2 && ch <= 'D'+2) { v = vD + ch - 'D'; }		// BC D EF
			else if (ch >= 'S'-2 && ch <= 'S'+2) { v = vS + ch - 'S'; } // QR S TU
			else if (ch >= 'M'-3 && ch <= 'M'+3) { v = vM + ch - 'M'; } // JKL M NOP
			if (v > -10) {
				if (v < 0) return NULL; // version cannot be fixed up!
				++ix;
				sprintf(p, "%d", (int)v);
				p += strlen(p);
			}
		} else {
			++p;
		}
		ASSERT((int)(p-buf) < cbBuf);
	}
	*p = 0;
	return buf;
}

int do_iftest(int &cTests)
{
	int fail_count = 0;
	cTests = 0;

	std::string err_reason;
	char buffer[256];
	bool bb = false;

	for (int ixSet = 0; ixSet < (int)COUNTOF(aTestSets); ++ixSet) {
		const struct _test_set & tset = aTestSets[ixSet];
		bool needs_fixup = ! strcmp("version", tset.label);

		if (dash_verbose) {
			fprintf(stdout, "--- %s - expecting %s %s\n", 
				tset.label, tset.valid ? "ok" : "unsup", tset.valid ? (tset.result ? "true" : "false") : "");
		}

		for (int ix = 0; ix < tset.cTbl; ++ix) {
			++cTests;
			err_reason = "bogus";
			const char * cond = aTestSets[ixSet].aTbl[ix];
			if (needs_fixup) {
				cond = fixup_version(cond, buffer, sizeof(buffer));
				if ( ! cond) { // version cannot be fixed up!
					if (dash_verbose) {
						fprintf(stdout, "# %s: \"%s\" %s\n",
							"skipped", aTestSets[ixSet].aTbl[ix], bb ? "\ntrue" : "\nfalse");
					}
					continue;
				}
			}
			bool valid = config_test_if_expression(cond, bb, NULL, NULL, err_reason);
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
