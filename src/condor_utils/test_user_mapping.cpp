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
#include "condor_random_num.h" // so we can force the random seed.
#include "MyString.h"
#include "MapFile.h"

#include <stdio.h>
#include <vector>
#include <string>

// for testing the userMap classad function
extern int add_user_mapping(const char * mapname, const char * mapdata);

// temporary, for testing.
extern void get_mapfile_re_info(size_t *info); // pass an array of 4 elements
extern void clear_mapfile_re_info();

class YourString ystr; // most recent lookup
std::string gmstr;   // holds the result of a mapfile lookup
MapFile* gmf = NULL;  // global mapfile used for lookup testing.
bool   grequire_failed;
double gstart_time; // start time for REQUIRE condition
double gelapsed_time;  // elapsed time for REQUIRE condition

bool dash_verbose = false;
int fail_count;

template <class c>
bool within(YourString /*ys*/, c /*lb*/, c /*hb*/) { return false; }

template <> bool within(YourString ys, int lb, int hb) {
	int v = atoi(ys.Value());
	//fprintf(stderr, "within '%s' -> %d\n", ys.Value(), v);
	return v >= lb && v <= hb;
}

template <> bool within(YourString ys, const char* lb, const char* hb) {
	return strcmp(ys.Value(),lb) >= 0 && strcmp(ys.Value(),hb) <= 0;
}

#define REQUIRE( condition ) \
	gstart_time = _condor_debug_get_time_double(); \
	grequire_failed = ! ( condition ); \
	gelapsed_time = _condor_debug_get_time_double() - gstart_time; \
	if (grequire_failed) { \
		fprintf( stderr, "Failed %5d: %s\n", __LINE__, #condition ); \
		fprintf( stderr, "             : actual value: %s :\n", ystr.ptr() ? ystr.ptr() : "NULL"); \
		++fail_count; \
	} else if( verbose ) { \
		fprintf( stdout, "    OK %5d: %s \t# %s\n", __LINE__, #condition, ystr.ptr() ? ystr.ptr() : "NULL" ); \
	}

//
YourString & lookup(const char * method, const char * input) {
	gmstr.clear();
	gmf->GetCanonicalization(method, input, gmstr);
	ystr = gmstr.c_str();
	return ystr;
}

YourString & lookup_user(const char * input) {
	gmstr.clear();
	gmf->GetUser(input, gmstr);
	ystr = gmstr.c_str();
	return ystr;
}

//
//   UNIT TESTS FOLLOW THIS BANNER
//
char regex_canon[] =
	"FS (.*) \\1\n"
	"KERBEROS (.*) \\1\n"
	"CLAIMTOBE john john\n"
	"CLAIMTOBE billg billg\n"
	"CLAIMTOBE pjfry pjfry\n"
	"CLAIMTOBE smiller.* smiller\n"
	"CLAIMTOBE swinger swinger\n"
	"CLAIMTOBE j.*r jackmiller\n"
	"CLAIMTOBE (.*) some_\\1\n"
	"GSI /C=US/ST=Wisconsin/L=Madison/O=CHTC/O=TEST/CN=CA jmiller\n"
	"GSI /C=US/ST=Wisconsin/L=Madison/O=CHTC/O=TEST/CN=CERT jmiller\n"
	"GSI \"/DC=com/DC=DigiCert-Grid/O=Open Science Grid/OU=People/CN=Jack Miller\" jmiller\n"
	;

char hash_canon[] =
	"FS /(.*)/ \\1\n"
	"KERBEROS /.*/ \\0\n"
	"CLAIMTOBE john john\n"
	"CLAIMTOBE billg billg\n"
	"CLAIMTOBE pjfry pjfry\n"
	"CLAIMTOBE swinger swinger\n"
	"CLAIMTOBE /smiller.*/ smiller\n"
	"CLAIMTOBE /j.*r/ jackmiller\n"
	"CLAIMTOBE /.*/ some_\\0\n"
	"GSI \"/C=US/ST=Wisconsin/L=Madison/O=CHTC/O=TEST/CN=CA\" jmiller\n"
	"GSI \"/C=US/ST=Wisconsin/L=Madison/O=CHTC/O=TEST/CN=CERT\" jmiller\n"
	"GSI \"/DC=com/DC=DigiCert-Grid/O=Open Science Grid/OU=People/CN=Jack Miller\" jmiller\n"
	;

char prefix_canon[] =
	"osdf	/example.vo/example.fs	/cephfs/example.fs\n"
	"dav	dav.example.vo/private	/nfs/private\n"
	"https	squid.example.vo/priv	squid_priv\n"
	"https	squid.example.vo/		squid_public\n"
	;


void print_usage(FILE * out, MapFile * mf, double elapsed_time)
{
	MapFileUsage useage;
	int size = mf->size(&useage);
	fprintf(out, "\t\t%.3f millisec,  %.2f KB\n", elapsed_time*1000, (useage.cbStrings + useage.cbStructs + useage.cbWaste)/1024.0);
	fprintf(out, "\t\tSize:         %8d\n", size);
	fprintf(out, "\t\tcMethods:     %8d\n", useage.cMethods);
	fprintf(out, "\t\tcRegex:       %8d\n", useage.cRegex);
	fprintf(out, "\t\tcHash:        %8d\n", useage.cHash);
	fprintf(out, "\t\tcEntries:     %8d\n", useage.cEntries);
	fprintf(out, "\t\tcAllocations: %8d\n", useage.cAllocations);
	fprintf(out, "\t\tcbStrings:    %8d\n", useage.cbStrings);
	fprintf(out, "\t\tcbStructs:    %8d\n", useage.cbStructs);
	fprintf(out, "\t\tcbWaste:      %8d\n", useage.cbWaste);

	size_t info[4];
	get_mapfile_re_info(info);
	fprintf(out, "\t\tnum_re:       %8d\n", (int)info[0]);
	fprintf(out, "\t\tnum_zero_re:  %8d\n", (int)info[1]);
	fprintf(out, "\t\tmin_re_size:  %8d\n", (int)info[2]);
	fprintf(out, "\t\tmax_re_size:  %8d\n", (int)info[3]);
	clear_mapfile_re_info();
}

void testing_parser(bool verbose, const char * mode)
{
	YourString m(mode);
	if (verbose) {
		fprintf( stdout, "\n----- testing_parser (%s)----\n\n", mode);
	}

	const bool assume_regex = false;
	MyStringCharSource * src = NULL;

	if (m == "regex") {
		delete gmf; gmf = NULL;
		gmf = new MapFile;
		src = new MyStringCharSource(regex_canon, false);
		REQUIRE(gmf->ParseCanonicalization(*src, "regex_canon", assume_regex) == 0);
		delete src; src = NULL;
	}

	const bool assume_hash = true;
	if (m == "hash") {
		delete gmf; gmf = NULL;
		gmf = new MapFile;
		src = new MyStringCharSource(hash_canon, false);
		REQUIRE(gmf->ParseCanonicalization(*src, "hash_canon", assume_hash) == 0);
		delete src; src = NULL;
		if (verbose) {
			print_usage(stdout, gmf, gelapsed_time);
		}
	}

	if (m == "prefix") {
		delete gmf; gmf = NULL;
		gmf = new MapFile;
		src = new MyStringCharSource(prefix_canon, false);
		REQUIRE(gmf->ParseCanonicalization(*src, "prefix_canon", assume_hash, true, true) == 0);
		gmf->dump(stderr);
		delete src; src = NULL;
		if (verbose) {
			print_usage(stdout, gmf, gelapsed_time);
		}
	}
}


void testing_lookups(bool verbose, const char * mode)
{
	YourString m(mode);
	if (verbose) {
		fprintf( stdout, "\n----- testing_lookups (%s)----\n\n", mode);
	}

	if( 0 == strcmp(mode, "prefix") ) {
		double dstart = _condor_debug_get_time_double();

		REQUIRE( lookup("osdf", "/example.vo/example.fs/example") == "/cephfs/example.fs" );
		REQUIRE( lookup("osdf", "prefix/example.vo/example.fs/example") == "" );

		REQUIRE( lookup("dav", "dav.example.vo/private/file") == "/nfs/private");
		REQUIRE( lookup("dav", "dav.other.vo/private/file") == "");
		REQUIRE( lookup("dav", "private-dav.example.vo/private/file") == "");

		REQUIRE( lookup("https", "squid.example.vo/priv/file") == "squid_priv" );
		REQUIRE( lookup("https", "prefix-squid.example.vo/priv/file") == "" );
		REQUIRE( lookup("https", "squid.example.vo/file") == "squid_public" );
		REQUIRE( lookup("https", "prefix-squid.example.vo/file") == "" );

		if (verbose) {
			double elapsed_time = _condor_debug_get_time_double() - dstart;
			fprintf(stdout, "    %.3f millisec\n", elapsed_time*1000);
		}
		return;
	}

	double dstart = _condor_debug_get_time_double();
	REQUIRE( lookup("FS", "Alice") == "Alice" );
	REQUIRE( lookup("FS", "Bob") == "Bob" );
	REQUIRE( lookup("FS", "john") == "john" );
	REQUIRE( lookup("FS", "billg") == "billg" );
	REQUIRE( lookup("FS", "pjfry") == "pjfry" );
	REQUIRE( lookup("FS", "uncle") == "uncle" );
	REQUIRE( lookup("FS", "swinger") == "swinger" );
	REQUIRE( lookup("FS", "jmiller") == "jmiller" );

	REQUIRE( lookup("KERBEROS", "Alice") == "Alice" );
	REQUIRE( lookup("KERBEROS", "Bob") == "Bob" );
	REQUIRE( lookup("KERBEROS", "john") == "john" );
	REQUIRE( lookup("KERBEROS", "billg") == "billg" );
	REQUIRE( lookup("KERBEROS", "pjfry") == "pjfry" );
	REQUIRE( lookup("KERBEROS", "uncle") == "uncle" );
	REQUIRE( lookup("KERBEROS", "swinger") == "swinger" );
	REQUIRE( lookup("KERBEROS", "jmiller") == "jmiller" );

	REQUIRE( lookup("CLAIMTOBE", "Alice") == "some_Alice" );
	REQUIRE( lookup("CLAIMTOBE", "Bob") == "some_Bob" );
	REQUIRE( lookup("CLAIMTOBE", "john") == "john" );
	REQUIRE( lookup("CLAIMTOBE", "billg") == "billg" );
	REQUIRE( lookup("CLAIMTOBE", "pjfry") == "pjfry" );
	REQUIRE( lookup("CLAIMTOBE", "uncle") == "some_uncle" );
	REQUIRE( lookup("CLAIMTOBE", "swinger") == "swinger" );
	REQUIRE( lookup("CLAIMTOBE", "jmiller") == "jackmiller" );

	REQUIRE( lookup("GSI", "/C=US/ST=Wisconsin/L=Madison/O=CHTC/O=TEST/CN=CA") == "jmiller" );
	REQUIRE( lookup("GSI", "/C=US/ST=Wisconsin/L=Madison/O=CHTC/O=TEST/CN=CERT") == "jmiller" );
	REQUIRE( lookup("GSI", "/DC=com/DC=DigiCert-Grid/O=Open Science Grid/OU=People/CN=Jack Miller") == "jmiller" );

	REQUIRE( lookup("GSI", "/DC=com/DC=DigiCert-Grid/O=Open Science Grid/OU=People/CN=John Miller") == "" );

	if (verbose) {
		double elapsed_time = _condor_debug_get_time_double() - dstart;
		fprintf(stdout, "    %.3f millisec\n", elapsed_time*1000);
	}
}

int read_mapfile(const char * mapfile, bool assume_hash, bool dump_it, const char * lookup_method, const char * user)
{
	int rval = 0;
	double dstart;

	const char * mode = assume_hash ? "hash" : "regex";

	delete gmf; gmf = NULL;
	gmf = new MapFile;
	if (YourString(mapfile) == "-") {
		MyStringFpSource mystdin(stdin, false);
		if (dash_verbose) { fprintf(stdout, "Loading mapping data (%s) from stdin\n", mode); }
		dstart = _condor_debug_get_time_double();
		rval = gmf->ParseCanonicalization(mystdin, "stdin", assume_hash);
	} else {
		if (dash_verbose) { fprintf(stdout, "Loading (%s) mapfile %s\n", mode, mapfile); }
		dstart = _condor_debug_get_time_double();
		rval = gmf->ParseCanonicalizationFile(mapfile, assume_hash);
	}
	if (dash_verbose) {
		double elapsed_time = _condor_debug_get_time_double() - dstart;
		print_usage(stdout, gmf, elapsed_time);
	}

	if (dump_it) {
		fprintf(stdout, "Resulting map:\n===========\n");
		gmf->dump(stdout);
		fprintf(stdout, "\n===========\n\n");
	}

	if (user) {
		YourString m(lookup_method);
		if (m == "#") lookup_method = "*"; // because * is onconvenient to put on the command line in *nix

		YourString u(user);
		if (u == "-") {
			if (u == mapfile) {
				fprintf(stderr, "ERROR: Cannot read both map and user from stdin\n");
				exit(1);
			}
			MyStringFpSource src(stdin, false);
			dstart = _condor_debug_get_time_double();
			int cLookups = 0;
			while ( ! src.isEof()) {
				std::string input_line;
				readLine(input_line, src); // Result ignored, we already monitor EOF
				trim(input_line);
				if (input_line.empty() || input_line[0] == '#') {
					continue;
				}
				gmstr.clear();
				gmf->GetCanonicalization(lookup_method, input_line, gmstr);
				if (dash_verbose) { fprintf(stdout, "%s = ", input_line.c_str()); }
				fprintf(stdout, "%s\n", gmstr.c_str());
				++cLookups;
			}
			double elapsed_time = _condor_debug_get_time_double() - dstart;
			if (dash_verbose) { fprintf(stdout, " %d Lookups %.3f ms", cLookups, elapsed_time*1000); }
		} else {
			gmstr.clear();
			dstart = _condor_debug_get_time_double();
			gmf->GetCanonicalization(lookup_method, user, gmstr);
			double elapsed_time = _condor_debug_get_time_double() - dstart;
			if (dash_verbose) { fprintf(stdout, "%s = ", user); }
			fprintf(stdout, "%s\n", gmstr.c_str());
			if (dash_verbose) { fprintf(stdout, " 1 Lookups %.3f ms", elapsed_time*1000); }
		}
	}

	return rval;
}

int read_gridmap(const char * mapfile, bool assume_hash, const char * user)
{
	int rval = 0;
	double dstart;

	const char * mode = assume_hash ? "hash" : "regex";

	delete gmf; gmf = NULL;
	gmf = new MapFile;
	if (YourString(mapfile) == "-") {
		MyStringFpSource mystdin(stdin, false);
		if (dash_verbose) { fprintf(stdout, "Loading grid-mapping data (%s) from stdin\n", mode); }
		dstart = _condor_debug_get_time_double();
		rval = gmf->ParseCanonicalization(mystdin, "stdin", assume_hash);
	} else {
		if (dash_verbose) { fprintf(stdout, "Loading (%s) grid-map %s\n", mode, mapfile); }
		dstart = _condor_debug_get_time_double();
		rval = gmf->ParseCanonicalizationFile(mapfile, assume_hash);
	}
	if (dash_verbose) {
		double elapsed_time = _condor_debug_get_time_double() - dstart;
		print_usage(stdout, gmf, elapsed_time);
	}

	if (user) {
		YourString u(user);
		if (u == "-") {
			if (u == mapfile) {
				fprintf(stderr, "ERROR: Cannot read both grid-map and user list from stdin\n");
				exit(1);
			}
			MyStringFpSource src(stdin, false);
			dstart = _condor_debug_get_time_double();
			int cLookups = 0;
			while ( ! src.isEof()) {
				std::string input_line;
				readLine(input_line, src); // Result ignored, we already monitor EOF
				trim(input_line);
				if (input_line.empty() || input_line[0] == '#') {
					continue;
				}
				gmstr.clear();
				gmf->GetUser(input_line, gmstr);
				if (dash_verbose) { fprintf(stdout, "%s = ", input_line.c_str()); }
				fprintf(stdout, "%s\n", gmstr.c_str());
				++cLookups;
			}
			double elapsed_time = _condor_debug_get_time_double() - dstart;
			if (dash_verbose) { fprintf(stdout, " %d Lookups %.3f ms", cLookups, elapsed_time*1000); }
		} else {
			gmstr.clear();
			dstart = _condor_debug_get_time_double();
			gmf->GetUser(user, gmstr);
			double elapsed_time = _condor_debug_get_time_double() - dstart;
			if (dash_verbose) { fprintf(stdout, "%s = ", user); }
			fprintf(stdout, "%s\n", gmstr.c_str());
			if (dash_verbose) { fprintf(stdout, " 1 Lookups %.3f ms", elapsed_time*1000); }
		}
	}

	return rval;
}

void timed_lookups(bool verbose, const char * lookup_method, StringList & users)
{
	double elapsed_time;
	int cLookups = 0;
	int cFailed = 0;
	if (lookup_method) {
		std::string meth(lookup_method);
		double dstart = _condor_debug_get_time_double();
		for (const char * user = users.first(); user; user = users.next()) {
			gmstr.clear();
			int rv = gmf->GetCanonicalization(meth, user, gmstr);
			cFailed -= rv;
			if (verbose && (rv < 0)) { fprintf(stderr, "NOT FOUND: %s\n", user); }
			++cLookups;
		}
		elapsed_time = _condor_debug_get_time_double() - dstart;
	} else {
		double dstart = _condor_debug_get_time_double();
		for (const char * user = users.first(); user; user = users.next()) {
			gmstr.clear();
			int rv = gmf->GetUser(user, gmstr);
			cFailed -= rv;
			if (verbose && (rv < 0)) { fprintf(stderr, "NOT FOUND: %s\n", user); }
			++cLookups;
		}
		elapsed_time = _condor_debug_get_time_double() - dstart;
	}
	fprintf(stdout, " %d Lookups, %d Failed %.3f ms\n", cLookups, cFailed, elapsed_time*1000);
}

void Usage(const char * appname, FILE * out)
{
	const char * p = appname;
	while (*p) {
		if (*p == '/' || *p == '\\') appname = p+1;
		++p;
	}
	fprintf(out, "Usage: %s [<opts>] [-test[:<tests>] | <queryopts>]\n"
		"  Where <opts> is one of\n"
		"    -help\tPrint this message\n"
		"    -verbose\tMore verbose output\n\n"
		"  <tests> is one or more letters choosing specific subtests\n"
		"    l  lookup tests\n"
		"    p  parse tests\n"
		"  If no arguments are given (or just -verbose) all tests are run.\n"
		"  Unless -verbose is specified, only failed tests produce output.\n"
		"  This program returns 0 if all tests succeed, 1 if any tests fails.\n\n"
		"  Instead of running tests, this program can validate map files\n"
		"  and map users against them. Use the <queryopts> for this.\n\n"
		"    -file <mapfile>\t Parse <mapfile> as a 3 field mapfile\n"
		"    -gridfile <gridfile> Parse <gridfile> as a 2 field user map\n"
		"        If <gridfile> or <mapfile> is -, read from stdin\n"
		"    -hash\t\t Field 1 of 2 field file, or field 2 of a 3 field file\n"
		"         \t\t is not a regex unless it starts and ends with /\n"
		"    -user <name>\t Map user against <gridfile> or <mapfile> and print the result\n"
		"        If <name> is -, read a list of users from stdin and map each of them\n"
		"    -method <method>\t Map user against <mapfile> using <method>\n"
		"        when mapping users against a 3 field <mapfile> this argument is required\n"
		"        use # to get the default method for classad userMap lookups\n"
		"    -timelist <file>\t Map each line from <file> against <mapfile> or <gridfile>\n"
		"                         and report total time spent doing the mapping. This option does\n"
		"                         not print the results, it is just a timing test\n"
		"    -debug[:<flags>]\t Send log messages to stdout as map is read. set flags to D_FULLDEBUG\n"
		"                         to see success messages for each file and map entry\n"
		, appname);
}

// runs all of the tests in non-verbose mode by default (i.e. printing only failures)
// individual groups of tests can be run by using the -t:<tests> option where <tests>
// is one or more of 
//   l   testing_lookups
//   P   testing_parser  config/submit/metaknob parser.
//
int main( int /*argc*/, const char ** argv) {

	int test_flags = 0;
	const char * pcolon;
	bool other_arg = false;
	bool assume_hash = false;
	bool dump_map = false;
	bool show_failed_lookups = false;
	const char * mapfile = NULL;
	const char * gridfile = NULL;
	const char * userfile = NULL;
	const char * user = NULL;
	const char * lookup_method = NULL;

	// if we don't init dprintf, calls to it will be will be malloc'ed and stored
	// for later. this form of init will match the fewest possible issues.
	dprintf_config_tool_on_error("D_ERROR");
	dprintf_OnExitDumpOnErrorBuffer(stderr);

	for (int ii = 1; argv[ii]; ++ii) {
		const char *arg = argv[ii];
		if (is_dash_arg_colon_prefix(arg, "verbose", &pcolon, 1)) {
			dash_verbose = 1;
			if (pcolon && pcolon[1]=='2') { show_failed_lookups = true; }
		} else if (is_dash_arg_prefix(arg, "help", 1)) {
			Usage(argv[0], stdout);
			return 0;
		} else if (is_dash_arg_colon_prefix(arg, "test", &pcolon, 1)) {
			if (pcolon) {
				while (*++pcolon) {
					switch (*pcolon) {
					case 'l': test_flags |= 0x0111; break; // lookup
					case 'p': test_flags |= 0x0222; break; // parse
					}
				}
			} else {
				test_flags = 3;
			}
		} else if (is_dash_arg_prefix(arg, "file", 1)) {
			mapfile = argv[ii+1];
			if (mapfile && (mapfile[0] != '-' || mapfile[1] != 0)) {
				++ii;
			} else {
				fprintf(stderr, "-file requires a filename argument\n");
				return 1;
			}
			other_arg = true;
		} else if (is_dash_arg_prefix(arg, "gridfile", 1)) {
			gridfile = argv[ii+1];
			if (gridfile && (gridfile[0] != '-' || gridfile[1] != 0)) {
				++ii;
			} else {
				fprintf(stderr, "-gridfile requires a filename argument\n");
				return 1;
			}
			other_arg = true;
		} else if (is_dash_arg_prefix(arg, "timelist", 5)) {
			userfile = argv[ii+1];
			if (userfile && userfile[0] != '-') {
				++ii;
			} else {
				fprintf(stderr, "-timelist requires a filename argument\n");
				return 1;
			}
		} else if (is_dash_arg_prefix(arg, "user", 1)) {
			user = argv[ii+1];
			if (user && (user[0] != '-' || user[1] != 0)) {
				++ii;
			} else {
				fprintf(stderr, "-user requires a username argument\n");
				return 1;
			}
		} else if (is_dash_arg_prefix(arg, "method", 1)) {
			lookup_method = argv[ii+1];
			if (lookup_method && lookup_method[0] != '-') {
				++ii;
			} else {
				fprintf(stderr, "-method requires a method argument\n");
				return 1;
			}
		} else if (is_dash_arg_prefix(arg, "hash", 2)) {
			assume_hash = true;
		} else if (is_dash_arg_prefix(arg, "dump", 2)) {
			dump_map = true;
		} else if (is_dash_arg_colon_prefix(arg, "debug", &pcolon, 3)) {
			// dprintf to console
			dprintf_set_tool_debug("TOOL", (pcolon && pcolon[1]) ? pcolon+1 : nullptr);
		} else {
			fprintf(stderr, "unknown argument %s\n", arg);
			Usage(argv[0], stderr);
			return 1;
		}
	}
	if ( ! test_flags && ! other_arg) test_flags = -1;

	if (test_flags & 0x0001) testing_parser(dash_verbose, "regex");
	if (test_flags & 0x0002) testing_lookups(dash_verbose, "regex");
	if (test_flags & 0x0010) testing_parser(dash_verbose, "hash");
	if (test_flags & 0x0020) testing_lookups(dash_verbose, "hash");
	if (test_flags & 0x0100) testing_parser(dash_verbose, "prefix");
	if (test_flags & 0x0200) testing_lookups(dash_verbose, "prefix");

	if (mapfile) {
		int rval = read_mapfile(mapfile, assume_hash, dump_map, lookup_method, user);
		if (rval < 0) {
			fprintf(stderr, "mapfile read returned %d\n", rval);
		}
	}
	if (gridfile) {
		int rval = read_gridmap(gridfile, assume_hash, user);
		if (rval < 0) {
			fprintf(stderr, "grid-map read returned %d\n", rval);
		}
	}
	if (userfile) {
		if (gridfile) {
			if (lookup_method) {
				fprintf(stderr, "WARNING: -method argument is ignored when doing lookups against a -grid file\n");
			}
		} else if (mapfile) {
			if ( ! lookup_method || (lookup_method[0] == '#')) lookup_method = "*";
		} else {
			fprintf(stderr, "ERROR: -userfile requires either -file -or -grid to do lookups against\n");
			return 1;
		}
		FILE *file = safe_fopen_wrapper_follow(userfile, "r");
		if (NULL == file) {
			fprintf(stderr, "ERROR: Could not open user lookups file '%s' : %s\n", userfile, strerror(errno));
			return 1;
		}

		MyStringFpSource src(file, true);
		StringList users;
		while ( ! src.isEof()) {
			std::string line;
			readLine(line, src); // Result ignored, we already monitor EOF
			chomp(line);
			if (line.empty()) continue;
			users.append(line.c_str());
		}
		timed_lookups(show_failed_lookups, lookup_method, users);
	}

	delete gmf; gmf = NULL; // so we can valgrind.
	return fail_count > 0;
}
