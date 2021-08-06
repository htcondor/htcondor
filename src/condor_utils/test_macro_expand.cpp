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

#include <stdio.h>
#include <vector>
#include <string>

// for testing the userMap classad function
extern int add_user_mapping(const char * mapname, char * mapdata);

//uncomment if   SUBSYS.LOCAL.KNOB is allowed.
//#define ALLOW_SUBSYS_LOCAL_HEIRARCHY 1

extern const MACRO_SOURCE DefaultMacro;

class YourString ystr; // most recent lookup
auto_free_ptr gstr;    // holds a pointer from the most recent lookup that should be freed
MyString gmstr;   // holds a formatted string that results from dumping a macro set

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
	if(! ( condition )) { \
		fprintf( stderr, "Failed %5d: %s\n", __LINE__, #condition ); \
		fprintf( stderr, "             : actual value: %s :\n", ystr.ptr() ? ystr.ptr() : "NULL"); \
		++fail_count; \
	} else if( verbose ) { \
		fprintf( stdout, "    OK %5d: %s \t# %s\n", __LINE__, #condition, ystr.ptr() ? ystr.ptr() : "NULL" ); \
	}

static MACRO_DEFAULTS TestingMacroDefaults = { 0, NULL, NULL };
static MACRO_SET TestingMacroSet = {
	0, 0,
	CONFIG_OPT_WANT_META,
	0, NULL, NULL, ALLOCATION_POOL(), std::vector<const char*>(), &TestingMacroDefaults, NULL };

MACRO_EVAL_CONTEXT def_ctx = { NULL, "TOOL", "/home/testing", false, 2, 0, 0 };
MACRO_SOURCE TestMacroSource = { false, false, 0, 0, -1, -2 };
MACRO_SOURCE FileMacroSource = { false, false, 0, 0, -1, -2 };

// helper functions for use within the REQUIRE macro
// these call various MACRO_SET functions that we wish to test
// and return a YourString so that the result can be easily
// compared to a string literal.

//
YourString & lookup(const char * name) {
	ystr = lookup_macro(name, TestingMacroSet, def_ctx);
	return ystr;
}

YourString & lookup_as(const char * prefix, const char * name) {
	MACRO_EVAL_CONTEXT ctx = def_ctx;
	ctx.subsys = prefix;
	ystr = lookup_macro(name, TestingMacroSet, ctx);
	return ystr;
}

YourString & lookup_lcl(const char * local, const char * name) {
	MACRO_EVAL_CONTEXT ctx = def_ctx;
	ctx.localname = local;
	ystr = lookup_macro(name, TestingMacroSet, ctx);
	return ystr;
}

YourString & lookup_lcl_as(const char * local, const char * prefix, const char * name) {
	MACRO_EVAL_CONTEXT ctx = def_ctx;
	ctx.subsys = prefix;
	ctx.localname = local;
	ystr = lookup_macro(name, TestingMacroSet, ctx);
	return ystr;
}

// The expand helpers return a ystr, and also store the malloc'ed return value
// in an auto_free_ptr so that the will be automatically freed by the next REQUIRE
//
YourString expand(const char * value) {
	gstr.set(expand_macro(value, TestingMacroSet, def_ctx));
	ystr = gstr.ptr();
	return ystr;
}

YourString expand_as(const char * prefix, const char * value) {
	MACRO_EVAL_CONTEXT ctx = def_ctx;
	ctx.subsys = prefix;
	gstr.set(expand_macro(value, TestingMacroSet, ctx));
	ystr = gstr.ptr();
	return ystr;
}

YourString expand_lcl(const char * local, const char * value) {
	MACRO_EVAL_CONTEXT ctx = def_ctx;
	ctx.localname = local;
	gstr.set(expand_macro(value, TestingMacroSet, ctx));
	ystr = gstr.ptr();
	return ystr;
}

YourString expand_lcl_as(const char * local, const char * prefix, const char * value) {
	MACRO_EVAL_CONTEXT ctx = def_ctx;
	ctx.subsys = prefix;
	ctx.localname = local;
	gstr.set(expand_macro(value, TestingMacroSet, ctx));
	ystr = gstr.ptr();
	return ystr;
}

YourString next$$(const char * value, int pos) {
	gstr.set(strdup(value));
	char* left, *name, *right;
	if (next_dollardollar_macro(gstr.ptr(), pos, &left, &name, &right)) {
		ystr = name;
	} else {
		ystr = NULL;
	}
	return ystr;
}

YourString next$$_left(const char * value, int pos) {
	gstr.set(strdup(value));
	char* left, *name, *right;
	if (next_dollardollar_macro(gstr.ptr(), pos, &left, &name, &right)) {
		ystr = left;
	} else {
		ystr = NULL;
	}
	return ystr;
}

YourString next$$_right(const char * value, int pos) {
	gstr.set(strdup(value));
	char* left, *name, *right;
	if (next_dollardollar_macro(gstr.ptr(), pos, &left, &name, &right)) {
		ystr = right;
	} else {
		ystr = NULL;
	}
	return ystr;
}

// Populate the testing MACRO_SET with values that we can lookup and expand
// in various ways.
//
static void insert_testing_macros(const char * local, const char * subsys)
{
	static const struct {
		const char * key;
		const char * val;
	} atbl[] = {
		{"FOO", "bar"},
		{"MASTER.foo", "mar"},
		{"MASTER.bar", "hi"},
		{"lower.bar", "'lo"},

		{"lower.pid_snapshot_interval", "12"},
		{"LOWER.VANILLA", "4"},
		{"MASTER.STANDARD", "2"},

		{"RELEASE_DIR", "/condor/test"},
		{"TILDE", "/condor/test"},
		{"LOWER.LOCAL_DIR", "/condor/lower"},
		{"MASTER.SPOOL", "$(LOCAL_DIR)/mspool"},
#ifdef ALLOW_SUBSYS_LOCAL_HEIRARCHY
		{"MASTER.lower.history", "$(SPOOL)/mlhistory"},
		{"lower.master.history", "$(SPOOL)/lmhistory"},
#endif

		// for $F tests
		{"fileBase", "base"},
		{"fileExt", "ex"},
		{"fileDirs", "/dur/der"},
		{"fileSimple", "simple.dat"},
		{"fileLong", "Now is the time for all good men."},
		{"fileCompound", "$(fileDirs)/$(fileBase).$(FileExt)"},
		{"fileAbs", "/one/two/three.for"},
		{"fileAbsDeep", "/six/five/four/three/two/one/file.ext"},
		{"dirDeep", "six/five/four/three/two/one/"},
		{"urlAbs", "file:/one/two/three.for"},
		{"fileAbsQuoted", "\"/one/two/three.for\""},
		{"fileAbsSQuoted", "'/one/two/three.for'"},
		{"fileRel", "ein/zwei/drei.fir"},
		{"fileRelDeep", "a/b/c/d/e/f.x"},
		{"fileRelSpaces", "\"ein/zw ei/dr ei.fir\""},
		{"fileCurRel", "./here"},
		{"fileUp1Rel", "../file"},
		{"fileOver1Rel", "../peer/file.dat"},
		{"fileCurRel2", "./uno/dos.tres"},
		{"wfileAbs", "c:\\one\\two\\three.for"},
		{"wfileAbsDeep", "c:\\six\\five\\four\\three\\two\\one\\file.ext"},
		{"wUNCfileAbs", "\\\\server\\share\\one\\two\\file.ext"},
		{"wfileAbsQuoted", "\"c:\\one\\two\\three.for\""},
		{"wfileRel", "ein\\zwei\\drei.fir"},
		{"wfileRelSpaces", "\"ein\\zw ei\\dr ei.fir\""},
		{"wfileCurRel", ".\\here"},
		{"wfileCurRel2", ".\\uno\\dos.tres"},
		{"wfileUp1Rel", "..\\file"},
		{"wfileOver1Rel", "..\\peer\\file.dat"},

		// for $F regressions
		{"Items5", "aa bb cc dd ee"},
		{"Items5Quoted", "\"aa bb cc dd ee\""},
		{"List6c", "aa,bb, cc,dd,ee,ff"},
		{"MASTER.List6c", "JMK,Vvv,XX,YY,ZKM,ZA"},
		{"List6cq", "$Fq(list6c)"},

		// for $INT and $REAL tests
		{"DoubleVanilla", "$(VANILLA)*2"},
		{"HalfVanilla", "$(VANILLA)/2.0"},
		{"StandardMinusVM", "$(STANDARD)-$(VM)"},

		// for $STRING and $EVAL tests
		{"Version","\"$Version: 8.5.6 May 20 2016 998822 $\""},
	};

	MACRO_EVAL_CONTEXT ctx = { local, subsys, "/home/testing", false, 2, 0, 0 };

	insert_source("Insert", TestingMacroSet, TestMacroSource);

	for (size_t ii = 0; ii < COUNTOF(atbl); ++ii) {
		TestMacroSource.line = ii;
		insert_macro(atbl[ii].key, atbl[ii].val, TestingMacroSet,  TestMacroSource, ctx);
	}

}

static void clear_macro_set(MACRO_SET & set)
{
	if (set.table) {
		memset(set.table, 0, sizeof(set.table[0]) * set.allocation_size);
	}
	if (set.metat) {
		memset(set.metat, 0, sizeof(set.metat[0]) * set.allocation_size);
	}
	set.size = 0;
	set.sorted = 0;
	set.apool.clear();
	// set.sources.clear();
	if (set.defaults && set.defaults->metat) {
		memset(set.defaults->metat, 0, sizeof(set.defaults->metat[0]) * set.defaults->size);
	}
}

static void dump_macro_set(MACRO_SET & set, MyString & str, const char * prefix) {
	str.clear();
	HASHITER it = hash_iter_begin(set, HASHITER_NO_DEFAULTS | HASHITER_SHOW_DUPS);
	while( ! hash_iter_done(it)) {
		const char *name = hash_iter_key(it);
		const char *val = hash_iter_value(it);
		//const MACRO_META *met = hash_iter_meta(it);
		if (prefix) str += prefix;
		str += name;
		str += "=";
		str += val ? val : "";
		str += "\n";
		hash_iter_next(it);
	}
	hash_iter_delete(&it);
}

// helper functions to test the parsing of config files and/or metaknobs
//
void testparse(int lineno, MACRO_SET & set, MACRO_EVAL_CONTEXT &ctx, MACRO_SOURCE & source, bool verbose, const char * tag, const char * params, const char * expected)
{
	int ret = Parse_config_string(source, 0, params, set, ctx);
	const char * hashout = NULL;
	if (ret < 0) {
		fprintf(stderr, "Failed %5d: test '%s' local=%s subsys=%s parse error %d\n",
			lineno, tag, ctx.localname ? ctx.localname : "", ctx.subsys ? ctx.subsys : "", ret);
		gmstr.clear();
		hashout = NULL;
	} else {
		dump_macro_set(set, gmstr, "\t");
		hashout = gmstr.c_str();
		if (gmstr != expected) {
			fprintf(stderr, "Failed %5d: test '%s' local=%s subsys=%s resulting hashtable does not match expected\n",
				lineno, tag, ctx.localname ? ctx.localname : "", ctx.subsys ? ctx.subsys : "");
			ret = -1;
		} else if (verbose) {
			fprintf(stdout, "    OK %5d: test '%s' local=%s subsys=%s\n",
				lineno, tag, ctx.localname ? ctx.localname : "", ctx.subsys ? ctx.subsys : "");
			ret = 0;
		}
	}
	if (verbose || ret) {
		fprintf(ret ? stderr : stdout, "\t---- parse input %d ----\n%s", lineno, params);
		if (hashout) fprintf(ret ? stderr : stdout, "\t---- resulting hash %d ----\n%s", lineno, hashout);
		if (ret)     fprintf(stderr, "\t---- expected hash %d ----\n%s", lineno, expected);
		fprintf(ret ? stderr : stdout, "\t---- end %d ----\n\n", lineno);
	}

	clear_macro_set(set);
}

void testparse_as(const char * prefix, int lineno, MACRO_SET & set, MACRO_EVAL_CONTEXT &_ctx, MACRO_SOURCE & source, bool verbose, const char * tag, const char * params, const char * expected)
{
	MACRO_EVAL_CONTEXT ctx = _ctx;
	ctx.subsys = prefix;
	testparse(lineno, set, ctx, source, verbose, tag, params, expected);
}

void testparse_lcl(const char * local, int lineno, MACRO_SET & set, MACRO_EVAL_CONTEXT &_ctx, MACRO_SOURCE & source, bool verbose, const char * tag, const char * params, const char * expected)
{
	MACRO_EVAL_CONTEXT ctx = _ctx;
	ctx.localname = local;
	testparse(lineno, set, ctx, source, verbose, tag, params, expected);
}

void testparse_lcl_as(const char * local, const char *prefix, int lineno, MACRO_SET & set, MACRO_EVAL_CONTEXT &_ctx, MACRO_SOURCE & source, bool verbose, const char * tag, const char * params, const char * expected)
{
	MACRO_EVAL_CONTEXT ctx = _ctx;
	ctx.localname = local;
	ctx.subsys = prefix;
	testparse(lineno, set, ctx, source, verbose, tag, params, expected);
}

//
//   UNIT TESTS FOLLOW THIS BANNER
//

void testing_parser(bool verbose)
{
	MACRO_SET set = {
		0, 0,
		CONFIG_OPT_WANT_META | CONFIG_OPT_DEFAULTS_ARE_PARAM_INFO,
		0, NULL, NULL, ALLOCATION_POOL(), std::vector<const char*>(), &TestingMacroDefaults, NULL
	};
	MACRO_SOURCE source = { false, false, 0, 0, -1, -2 };
	insert_source("parse", set, source);

	testparse(__LINE__, set, def_ctx, source, verbose, "last wins", "FOO= bar\nFOO = baz\n", "\tFOO=baz\n");
	testparse(__LINE__, set, def_ctx, source, verbose, "self sub", "FOO= bar\nFOO = $(FOO) baz\n", "\tFOO=bar baz\n");
	testparse(__LINE__, set, def_ctx, source, verbose,
		"self sub picks up subsys", "FOO=bar\nMASTER.FOO= MAR\nFOO = $(FOO) baz\n", "\tFOO=bar baz\n\tMASTER.FOO=MAR\n");
	testparse_as("MASTER", __LINE__, set, def_ctx, source, verbose,
		"self sub picks up subsys2", "FOO=bar\nMASTER.FOO= MAR\nFOO = $(FOO) baz\n", "\tFOO=MAR baz\n\tMASTER.FOO=MAR\n");
	testparse_lcl("LOWER", __LINE__, set, def_ctx, source, verbose,
		"self sub picks up subsys3", "FOO=bar\nMASTER.FOO= MAR\nFOO = $(FOO) baz\n", "\tFOO=bar baz\n\tMASTER.FOO=MAR\n");

	const char * n1 = "N1=$(NEGOTIATOR)\nN1_ARGS=-local-name N1\nN1.NEGOTIATOR_LOG=$(LOG)/NegotiatorLog.N1\nN1.SPOOL=$(SPOOL)/N1\n";
	const char * n1e1 = "\tN1=$(NEGOTIATOR)\n\tN1_ARGS=-local-name N1\n\tN1.NEGOTIATOR_LOG=$(LOG)/NegotiatorLog.N1\n\tN1.SPOOL=$(SPOOL)/N1\n";
#ifdef WIN32
	const char * n1e2 = "\tN1=$(NEGOTIATOR)\n\tN1_ARGS=-local-name N1\n\tN1.NEGOTIATOR_LOG=$(LOG)/NegotiatorLog.N1\n\tN1.SPOOL=$(LOCAL_DIR)\\spool/N1\n";
#else
	const char * n1e2 = "\tN1=$(NEGOTIATOR)\n\tN1_ARGS=-local-name N1\n\tN1.NEGOTIATOR_LOG=$(LOG)/NegotiatorLog.N1\n\tN1.SPOOL=$(LOCAL_DIR)/spool/N1\n";
#endif
	testparse(__LINE__, set, def_ctx, source, verbose, "subsys.self sub1", n1, n1e1);
	testparse_as("NEGOTIATOR", __LINE__, set, def_ctx, source, verbose, "subsys.self sub2", n1, n1e1);
	testparse_lcl("N1", __LINE__, set, def_ctx, source, verbose, "subsys.self sub3", n1, n1e2);
	testparse_lcl_as("N1", "NEGOTIATOR", __LINE__, set, def_ctx, source, verbose, "subsys.self sub4", n1, n1e2);
}

void testing_lookups(bool verbose)
{
	if (verbose) {
		fprintf( stdout, "\n----- testing_lookups ----\n\n");
	}

#ifdef WIN32
	REQUIRE( lookup("master") == "$(SBIN)\\condor_master.exe" );
	REQUIRE( lookup("mASter") == "$(SBIN)\\condor_master.exe" );
	REQUIRE( lookup("MASTER") == "$(SBIN)\\condor_master.exe" );
#else
	REQUIRE( lookup("master") == "$(SBIN)/condor_master" );
	REQUIRE( lookup("mASter") == "$(SBIN)/condor_master" );
	REQUIRE( lookup("MASTER") == "$(SBIN)/condor_master" );
#endif
	REQUIRE( lookup("MASTER_FLAG") == NULL );
	REQUIRE( lookup("FOO") == "bar" );

	// basic lookups with localname and with subsys
	REQUIRE( lookup("FOO") == "bar" );
	REQUIRE( lookup("BAR") == NULL );
	REQUIRE( lookup_as("MASTER", "FOO") == "mar" );
	REQUIRE( lookup_as("MiSTER", "FOO") == "bar" );
	REQUIRE( lookup_as("Malt", "FOO") == "bar" );
	REQUIRE( lookup_as("MASTER", "Bar") == "hi" );
	REQUIRE( lookup_lcl("LOWER", "Bar") == "'lo" );
	REQUIRE( lookup_lcl("UPPER", "Bar") == NULL );

	REQUIRE( lookup("PID_SNAPSHOT_INTERVAL") == "15" );
	REQUIRE( lookup("master.PID_SNAPSHOT_INTERVAL") == "60" );
	REQUIRE( lookup("lower.PID_SNAPSHOT_INTERVAL") == "12" );
	REQUIRE( lookup_as("MASTER", "PID_SNAPSHOT_INTERVAL") == "60" );
	REQUIRE( lookup_as("SCHEDD", "PID_SNAPSHOT_INTERVAL") == "15" );
	REQUIRE( lookup_lcl("Upper", "PID_SNAPSHOT_INTERVAL") == "15" );
	REQUIRE( lookup_lcl("lower", "PID_SNAPSHOT_INTERVAL") == "12" );

#ifdef WIN32
	REQUIRE( lookup("LOCAL_DIR") == "$(RELEASE_DIR)" );
	REQUIRE( lookup_as("MASTER", "LOCAL_DIR") == "$(RELEASE_DIR)" );
#else
	REQUIRE( lookup("LOCAL_DIR") == "$(TILDE)" );
	REQUIRE( lookup_as("MASTER", "LOCAL_DIR") == "$(TILDE)" );
#endif
	REQUIRE( lookup_as("LOWER", "LOCAL_DIR") == "/condor/lower" );
	REQUIRE( lookup("RELEASE_DIR") == "/condor/test" );

	REQUIRE( lookup("HISTORY") == "$(SPOOL)/history" );
#ifdef WIN32
	REQUIRE( lookup("SPOOL") == "$(LOCAL_DIR)\\spool" );
#else
	REQUIRE( lookup("SPOOL") == "$(LOCAL_DIR)/spool" );
#endif
	REQUIRE( lookup_lcl("lower", "HISTORY") == "$(SPOOL)/history" );
#ifdef WIN32
	REQUIRE( lookup_lcl("lower", "SPOOL") == "$(LOCAL_DIR)\\spool" );
#else
	REQUIRE( lookup_lcl("lower", "SPOOL") == "$(LOCAL_DIR)/spool" );
#endif
	REQUIRE( lookup_as("Master", "spool") == "$(LOCAL_DIR)/mspool" );
	REQUIRE( lookup_as("MASTER", "history") == "$(SPOOL)/history" );
}

void testing_$_expand(bool verbose)
{
	if (verbose) {
		fprintf( stdout, "\n----- testing_$_expand ----\n\n");
	}
	REQUIRE( expand("") == "" );
	REQUIRE( expand("foo") == "foo" );
	REQUIRE( expand("$(FOO)") == "bar" );
	REQUIRE( expand("$(DOES_NOT_EXIST)") == "" );
	REQUIRE( expand("$(DOES_NOT_EXIST:true)") == "true" );
	REQUIRE( expand("$(DOES_NOT_EXIST:0)") == "0" );

	REQUIRE( expand("$(FOO) $$([1+2]) $(BAR) ") == "bar $$([1+2])  " );
	REQUIRE( expand("_$(FOO)_$$([ 1 + 2 ])_$(BAR)_") == "_bar_$$([ 1 + 2 ])__" );
	REQUIRE( expand("$(FOO)$$(BAR)$(BAR:_)") == "bar$$(BAR)_" );

	REQUIRE( expand("history") == "history" );
#ifdef WIN32
	REQUIRE( expand("$(history)") == "/condor/test\\spool/history" );
	REQUIRE( expand_lcl("LOWER", "$(history)") == "/condor/lower\\spool/history" );
#else
	REQUIRE( expand("$(history)") == "/condor/test/spool/history" );
	REQUIRE( expand_lcl("LOWER", "$(history)") == "/condor/lower/spool/history" );
#endif

	REQUIRE( expand_as("MASTER", "$(spool)") == "/condor/test/mspool" );
	REQUIRE( expand_as("MASTER", "$(history)") == "/condor/test/mspool/history" );
#ifdef ALLOW_SUBSYS_LOCAL_HEIRARCHY
	REQUIRE( expand_lcl_as("LOWER", "MASTER", "$(history)") == "/condor/lower/mspool/mlhistory" );
#else
	REQUIRE( expand_lcl_as("LOWER", "MASTER", "$(history)") == "/condor/lower/mspool/history" );
#endif

	REQUIRE( expand("$(foo) $$(FOO) $(bar)") == "bar $$(FOO) " );
	REQUIRE( expand("$(DOLLAR)(FOO) $$(FOO) ") == "$(FOO) $$(FOO) " );
	REQUIRE( expand("$(DOLLAR)(FOO) $$(DOLLARDOLLAR) ") == "$(FOO) $$(DOLLARDOLLAR) " );
}

void testing_$$_expand(bool verbose)
{
	if (verbose) {
		fprintf( stdout, "\n----- testing_$$_expand ----\n\n");
	}

	REQUIRE( next$$("$$(STUFF)", 0) == "STUFF" );
	REQUIRE( next$$_left("$$(STUFF)", 0) == "" );
	REQUIRE( next$$_right("$$(STUFF)", 0) == "" );
	REQUIRE( next$$("_$$(STUFF:a,b,c) $(BAR)", 0) == "STUFF:a,b,c" );
	REQUIRE( next$$("$$(DOLLARDOLLAR)", 0) == "DOLLARDOLLAR" );

	REQUIRE( next$$("$$([1+2])", 0) == "[1+2]" );
	REQUIRE( next$$("$$([ 1 + 2*3 ])", 0) == "[ 1 + 2*3 ]" );
	REQUIRE( next$$("$$([ splitslotname(Target.Name)[0] ])", 0) == "[ splitslotname(Target.Name)[0] ]" );

	REQUIRE( next$$("$(FOO) $$(STUFF:2) $(BAR)", 0) == "STUFF:2" );
	REQUIRE( next$$_left("$(FOO) $$(STUFF:2) $(BAR)", 0) == "$(FOO) " );
	REQUIRE( next$$_right("$(FOO) $$(STUFF:2) $(BAR)", 0) == " $(BAR)" );

	REQUIRE( next$$("_$$(STUFF:2) $$([1+2])$$(BAR)", 0) == "STUFF:2" );
	REQUIRE( next$$("_$$(STUFF:2) $$([1+2])$$(BAR)", 2) == "[1+2]" );
	REQUIRE( next$$("_$$(STUFF:2) $$([1+2])$$(BAR)", 20) == "BAR" );

	REQUIRE( next$$_left("_$$(STUFF:2) $$([1+2])$$(BAR)", 0) == "_" );
	REQUIRE( next$$_left("_$$(STUFF:2) $$([1+2])$$(BAR)", 12) == "_$$(STUFF:2) " );
	REQUIRE( next$$_left("_$$(STUFF:2) $$([1+2])$$(BAR)", 20) == "_$$(STUFF:2) $$([1+2])" );

	REQUIRE( next$$_right("_$$(STUFF:2) $$([1+2])$$(BAR)", 0) == " $$([1+2])$$(BAR)" );
	REQUIRE( next$$_right("_$$(STUFF:2) $$([1+2])$$(BAR)", 2) == "$$(BAR)" );
	REQUIRE( next$$_right("_$$(STUFF:2) $$([1+2])$$(BAR)", 20) == "" );
}

void testing_$ENV_expand(bool verbose)
{
	if (verbose) {
		fprintf( stdout, "\n----- testing_$ENV_expand ----\n\n");
	}

	// linux want's these to be char's
	static char env1[] = "FOO=BAR";
	static char env2[] = "Declaration=We the people, in order to form a more perfect union...";
	// on Windows, putenv of this removes the environment var, but on *nix, it sets an empty var.
	//static char env3[] = "EMPTY=";
	putenv(env1);
	putenv(env2);

	// tests
	REQUIRE( expand("$ENV(FOO)") == "BAR" );
	REQUIRE( expand("$ENV(FOO:)") == "BAR" );
	REQUIRE( expand("$ENV(FOO:BAZ)") == "BAR" );
	REQUIRE( expand("$ENV(Declaration:)") == "We the people, in order to form a more perfect union..." );
	REQUIRE( expand("$ENV(EMPTY)") == "UNDEFINED" );
	REQUIRE( expand("$ENV(EMPTY:)") == "" );
	REQUIRE( expand("$ENV(EMPTY:BAZ)") == "BAZ" );
	REQUIRE( expand("$ENV(EMPTY:$(STUFF))") == "" );
}

void testing_$RAND_expand(bool verbose)
{
	if (verbose) {
		fprintf( stdout, "\n----- testing_$RAND_expand ----\n\n");
	}

	// tests
	REQUIRE( within(expand("$RANDOM_INTEGER(-30,30,1)"), -30, 30) );
	REQUIRE( within(expand("$RANDOM_INTEGER(2,2,1)"), 2, 2) );
	REQUIRE( within(expand("$RANDOM_INTEGER(-10,0,2)"), -10, 0) );
	REQUIRE( within(expand("$RANDOM_INTEGER(-1000,-99,3)"), -1000, -99) );
	REQUIRE( within(expand("$RANDOM_INTEGER(3,9,3)"), 3, 9) );
	REQUIRE( within(expand("$RANDOM_INTEGER(0,9,3)"), 0, 9) );

	REQUIRE( within(expand("$RANDOM_CHOICE(1,2,2,1)"), 1, 2) );
	REQUIRE( within(expand("$RANDOM_CHOICE(aa,bb,cc)"), "aa", "cc") );
	REQUIRE( within(expand("$RANDOM_CHOICE(List6c)"), "aa", "ff") );
	REQUIRE( within(expand_as("MASTER", "$RANDOM_CHOICE(List6c)"), "JMK", "ZKM") );
	REQUIRE( expand("$RANDOM_CHOICE(1)") == "1" );
	REQUIRE( expand("$RANDOM_CHOICE(aa bb cc)") == "aa bb cc" );
}

void testing_$F_expand(bool verbose)
{
	if (verbose) {
		fprintf( stdout, "\n----- testing_$F_expand ----\n\n");
	}

	// basic tests
	REQUIRE( expand("$Fn(fileBase)") == "base" );
	REQUIRE( expand("$Fx(fileBase)") == "" );
	REQUIRE( expand("$Fd(fileBase)") == "" );

	REQUIRE( expand("$Fdf(fileBase)") == "testing/" );
	REQUIRE( expand("$Fpf(fileBase)") == "/home/testing/" );

	REQUIRE( expand("$Fd(fileDirs)") == "dur/" );
	REQUIRE( expand("$Fdx(fileDirs)") == "dur/der" );
	REQUIRE( expand("$Fn(fileDirs)") == "der" );
	REQUIRE( expand("$Fpnx(fileDirs)") == "/dur/der" );

	REQUIRE( expand("$Fpnf(fileDirs)") == "/dur/der" );
	REQUIRE( expand("$Fdf(fileDirs)") == "dur/" );

	REQUIRE( expand("$Fdnx(fileSimple)") == "simple.dat" );
	REQUIRE( expand("$Fd(fileSimple)") == "" );
	REQUIRE( expand("$Fn(fileSimple)") == "simple" );
	REQUIRE( expand("$Fx(fileSimple)") == ".dat" );
	REQUIRE( expand("$Fnx(fileSimple)") == "simple.dat" );

	REQUIRE( expand("$Fdnx(fileLong)") == "Now is the time for all good men." );
	REQUIRE( expand("$Fd(fileLong)") == "" );
	REQUIRE( expand("$Fp(fileLong)") == "" );
	REQUIRE( expand("$Fn(fileLong)") == "Now is the time for all good men" );
	REQUIRE( expand("$Fx(fileLong)") == "." );
	REQUIRE( expand("$Fnx(fileLong)") == "Now is the time for all good men." );

	REQUIRE( expand("$F(fileCompound)") == "/dur/der/base.ex" );
	REQUIRE( expand("$Fdnx(fileCompound)") == "der/base.ex" );
	REQUIRE( expand("$Fd(fileCompound)") == "der/" );
	REQUIRE( expand("$Fdb(fileCompound)") == "der" );
	REQUIRE( expand("$Fddb(fileCompound)") == "dur/der" );
	REQUIRE( expand("$Fp(fileCompound)") == "/dur/der/" );
	REQUIRE( expand("$Fpw(fileCompound)") == "\\dur\\der\\" );
	REQUIRE( expand("$Ffqaw(fileCompound)") == "'\\dur\\der\\base.ex'" );
	REQUIRE( expand("$Fn(fileCompound)") == "base" );
	REQUIRE( expand("$Fx(fileCompound)") == ".ex" );
	REQUIRE( expand("$Fnx(fileCompound)") == "base.ex" );
	REQUIRE( expand("$Fpf(fileCompound)") == "/dur/der/" );
	REQUIRE( expand("$Fnf(fileCompound)") == "base" );
	REQUIRE( expand("$Fxf(fileCompound)") == ".ex" );
	REQUIRE( expand("$Fnxf(fileCompound)") == "base.ex" );

	REQUIRE( expand("$Fdnx(fileAbs)") == "two/three.for" );
	REQUIRE( expand("$Fpnx(fileAbs)") == "/one/two/three.for" );
	REQUIRE( expand("$Fqpnx(fileAbs)") == "\"/one/two/three.for\"" );
	REQUIRE( expand("$Fq(fileAbs)") == "\"/one/two/three.for\"" );
	REQUIRE( expand("$Fqapnx(fileAbs)") == "'/one/two/three.for'" );
	REQUIRE( expand("$Fqa(fileAbs)") == "'/one/two/three.for'" );
	REQUIRE( expand("$Fd(fileAbs)") == "two/" );
	REQUIRE( expand("$Fp(fileAbs)") == "/one/two/" );
	REQUIRE( expand("$Fn(fileAbs)") == "three" );
	REQUIRE( expand("$Fx(fileAbs)") == ".for" );
	REQUIRE( expand("$Fnx(fileAbs)") == "three.for" );
	REQUIRE( expand("$Fpnxf(fileAbs)") == "/one/two/three.for" );

	REQUIRE( expand("$Fd(fileAbsDeep)") == "one/" );
	REQUIRE( expand("$Fdd(fileAbsDeep)") == "two/one/" );
	REQUIRE( expand("$Fddd(fileAbsDeep)") == "three/two/one/" );
	REQUIRE( expand("$Fddddddd(fileAbsDeep)") == "/six/five/four/three/two/one/" );
	REQUIRE( expand("$Fddb(fileAbsDeep)") == "two/one" );

	REQUIRE( expand("$Fw(fileAbsDeep)") == "\\six\\five\\four\\three\\two\\one\\file.ext" );
	REQUIRE( expand("$Ffw(fileAbsDeep)") == "\\six\\five\\four\\three\\two\\one\\file.ext" );
	REQUIRE( expand("$Fpw(fileAbsDeep)") == "\\six\\five\\four\\three\\two\\one\\" );
	REQUIRE( expand("$Fddxw(fileAbsDeep)") == "two\\one\\file.ext" );
	REQUIRE( expand("$Fddnw(fileAbsDeep)") == "two\\one\\file" );

	REQUIRE( expand("$Fdnx(urlAbs)") == "two/three.for" );
	REQUIRE( expand("$Fpnx(urlAbs)") == "file:/one/two/three.for" );
	REQUIRE( expand("$Fqpnx(urlAbs)") == "\"file:/one/two/three.for\"" );
	REQUIRE( expand("$Fq(urlAbs)") == "\"file:/one/two/three.for\"" );
	REQUIRE( expand("$Fd(urlAbs)") == "two/" );
	REQUIRE( expand("$Fp(urlAbs)") == "file:/one/two/" );
	REQUIRE( expand("$Fn(urlAbs)") == "three" );
	REQUIRE( expand("$Fx(urlAbs)") == ".for" );
	REQUIRE( expand("$Fnx(urlAbs)") == "three.for" );

	REQUIRE( expand("$Fdnx(fileAbsQuoted)") == "two/three.for" );
	REQUIRE( expand("$Fd(fileAbsQuoted)") == "two/" );
	REQUIRE( expand("$Fp(fileAbsQuoted)") == "/one/two/" );
	REQUIRE( expand("$Fn(fileAbsQuoted)") == "three" );
	REQUIRE( expand("$Fx(fileAbsQuoted)") == ".for" );
	REQUIRE( expand("$Fdb(fileAbsQuoted)") == "two" );
	REQUIRE( expand("$Fpb(fileAbsQuoted)") == "/one/two" );
	REQUIRE( expand("$Fnb(fileAbsQuoted)") == "three" );
	REQUIRE( expand("$Fxb(fileAbsQuoted)") == "for" );
	REQUIRE( expand("$Fnx(fileAbsQuoted)") == "three.for" );
	REQUIRE( expand("$Fpf(fileAbsQuoted)") == "/one/two/" );
	REQUIRE( expand("$Fqn(fileAbsQuoted)") == "\"three\"" );
	REQUIRE( expand("$Fqan(fileAbsQuoted)") == "'three'" );

	REQUIRE( expand("$Fdnx(fileAbsSQuoted)") == "two/three.for" );
	REQUIRE( expand("$Fd(fileAbsSQuoted)") == "two/" );
	REQUIRE( expand("$Fp(fileAbsSQuoted)") == "/one/two/" );
	REQUIRE( expand("$Fn(fileAbsSQuoted)") == "three" );
	REQUIRE( expand("$Fx(fileAbsSQuoted)") == ".for" );
	REQUIRE( expand("$Fnx(fileAbsSQuoted)") == "three.for" );
	REQUIRE( expand("$Fqn(fileAbsSQuoted)") == "\"three\"" );
	REQUIRE( expand("$Fqan(fileAbsSQuoted)") == "'three'" );

	REQUIRE( expand("$Fpnx(fileRel)") == "ein/zwei/drei.fir" );
	REQUIRE( expand("$Fd(fileRel)") == "zwei/" );
	REQUIRE( expand("$Fp(fileRel)") == "ein/zwei/" );
	REQUIRE( expand("$Fn(fileRel)") == "drei" );
	REQUIRE( expand("$Fx(fileRel)") == ".fir" );
	REQUIRE( expand("$Fnx(fileRel)") == "drei.fir" );

	REQUIRE( expand("$Fnx(fileRelDeep)") == "f.x" );
	REQUIRE( expand("$Fdb(fileRelDeep)") == "e" );
	REQUIRE( expand("$Fddb(fileRelDeep)") == "d/e" );
	REQUIRE( expand("$Ff(fileRelDeep)") == "/home/testing/a/b/c/d/e/f.x" );
	REQUIRE( expand("$Ffq(fileRelDeep)") == "\"/home/testing/a/b/c/d/e/f.x\"" );
	REQUIRE( expand("$Ffddddd(fileRelDeep)") == "a/b/c/d/e/" );
	REQUIRE( expand("$Ffdddddd(fileRelDeep)") == "testing/a/b/c/d/e/" );
	REQUIRE( expand("$Ffw(fileRelDeep)") == "\\home\\testing\\a\\b\\c\\d\\e\\f.x" );
	REQUIRE( expand("$Ffwq(fileRelDeep)") == "\"\\home\\testing\\a\\b\\c\\d\\e\\f.x\"" );
	REQUIRE( expand("$Ffwqa(fileRelDeep)") == "'\\home\\testing\\a\\b\\c\\d\\e\\f.x'" );

	REQUIRE( expand("$Fdnx(fileRelSpaces)") == "zw ei/dr ei.fir" );
	REQUIRE( expand("$Fd(fileRelSpaces)") == "zw ei/" );
	REQUIRE( expand("$Fp(fileRelSpaces)") == "ein/zw ei/" );
	REQUIRE( expand("$Fn(fileRelSpaces)") == "dr ei" );
	REQUIRE( expand("$Fx(fileRelSpaces)") == ".fir" );
	REQUIRE( expand("$Fnx(fileRelSpaces)") == "dr ei.fir" );

	REQUIRE( expand("$Fdnx(fileCurRel)") == "./here" );
	REQUIRE( expand("$Fd(fileCurRel)") == "./" );
	REQUIRE( expand("$Fp(fileCurRel)") == "./" );
	REQUIRE( expand("$Fn(fileCurRel)") == "here" );
	REQUIRE( expand("$Fx(fileCurRel)") == "" );
	REQUIRE( expand("$Fnx(fileCurRel)") == "here" );
	REQUIRE( expand("$Ff(fileCurRel)") == "/home/testing/here" );

	REQUIRE( expand("$Fdnx(fileUp1Rel)") == "../file" );
	REQUIRE( expand("$Fd(fileUp1Rel)") == "../" );
	REQUIRE( expand("$Fp(fileUp1Rel)") == "../" );
	REQUIRE( expand("$Fn(fileUp1Rel)") == "file" );
	REQUIRE( expand("$Fx(fileUp1Rel)") == "" );
	REQUIRE( expand("$Fnx(fileUp1Rel)") == "file" );
	REQUIRE( expand("$Ff(fileUp1Rel)") == "/home/testing/../file" );

	REQUIRE( expand("$Fdnx(fileOver1Rel)") == "peer/file.dat" );
	REQUIRE( expand("$Fd(fileOver1Rel)") == "peer/" );
	REQUIRE( expand("$Fp(fileOver1Rel)") == "../peer/" );
	REQUIRE( expand("$Fn(fileOver1Rel)") == "file" );
	REQUIRE( expand("$Fx(fileOver1Rel)") == ".dat" );
	REQUIRE( expand("$Fnx(fileOver1Rel)") == "file.dat" );
	REQUIRE( expand("$Ff(fileOver1Rel)") == "/home/testing/../peer/file.dat" );

#ifdef WIN32
	REQUIRE( expand("$Ff(wfileCurRel)") == "/home/testing/here" );
	REQUIRE( expand("$Ffu(wfileCurRel)") == "/home/testing/here" );
#endif
	REQUIRE( expand("$Ffw(wfileCurRel)") == "\\home\\testing\\here" );
	REQUIRE( expand("$Ff(wfileUp1Rel)") == "/home/testing/..\\file" );
	REQUIRE( expand("$Ffu(wfileUp1Rel)") == "/home/testing/../file" );
	REQUIRE( expand("$Ffw(wfileUp1Rel)") == "\\home\\testing\\..\\file" );
	REQUIRE( expand("$Ffu(wfileOver1Rel)") == "/home/testing/../peer/file.dat" );
	REQUIRE( expand("$Ffw(wfileOver1Rel)") == "\\home\\testing\\..\\peer\\file.dat" );

	REQUIRE( expand("$Fpnx(fileCurRel2)") == "./uno/dos.tres" );
	REQUIRE( expand("$Fdnx(fileCurRel2)") == "uno/dos.tres" );
	REQUIRE( expand("$Fd(fileCurRel2)") == "uno/" );
	REQUIRE( expand("$Fp(fileCurRel2)") == "./uno/" );
	REQUIRE( expand("$Fn(fileCurRel2)") == "dos" );
	REQUIRE( expand("$Fx(fileCurRel2)") == ".tres" );
	REQUIRE( expand("$Fnx(fileCurRel2)") == "dos.tres" );
	REQUIRE( expand("$Fpnxf(fileCurRel2)") == "/home/testing/uno/dos.tres" );
	REQUIRE( expand("$Fdnxf(fileCurRel2)") == "uno/dos.tres" );
	REQUIRE( expand("$Fpf(fileCurRel2)") == "/home/testing/uno/" );

	REQUIRE( expand("$Ff(wfileRel)") == "/home/testing/ein\\zwei\\drei.fir" );
	REQUIRE( expand("$Ffw(wfileRel)") == "\\home\\testing\\ein\\zwei\\drei.fir" );
	REQUIRE( expand("$Ffu(wfileRel)") == "/home/testing/ein/zwei/drei.fir" );

#ifdef WIN32
	REQUIRE( expand("$Fdnx(wfileAbs)") == "two\\three.for" );
	REQUIRE( expand("$Fd(wfileAbs)") == "two\\" );
	REQUIRE( expand("$Fp(wfileAbs)") == "c:\\one\\two\\" );
	REQUIRE( expand("$Ff(wfileAbs)") == "c:\\one\\two\\three.for" );
	REQUIRE( expand("$Fn(wfileAbs)") == "three" );
	REQUIRE( expand("$Fx(wfileAbs)") == ".for" );
	REQUIRE( expand("$Fnx(wfileAbs)") == "three.for" );

	REQUIRE( expand("$Fdnx(wfileAbsDeep)") == "one\\file.ext" );
	REQUIRE( expand("$Fddnx(wfileAbsDeep)") == "two\\one\\file.ext" );
	REQUIRE( expand("$Fd(wfileAbsDeep)") == "one\\" );
	REQUIRE( expand("$Fdd(wfileAbsDeep)") == "two\\one\\" );
	REQUIRE( expand("$Fddd(wfileAbsDeep)") == "three\\two\\one\\" );
	REQUIRE( expand("$Fdddp(wfileAbsDeep)") == "three\\two\\one\\" );
	REQUIRE( expand("$Fp(wfileAbsDeep)") == "c:\\six\\five\\four\\three\\two\\one\\" );
	REQUIRE( expand("$Fpb(wfileAbsDeep)") == "c:\\six\\five\\four\\three\\two\\one" );
	REQUIRE( expand("$Fn(wfileAbsDeep)") == "file" );
	REQUIRE( expand("$Fx(wfileAbsDeep)") == ".ext" );
	REQUIRE( expand("$Fxb(wfileAbsDeep)") == "ext" );
	REQUIRE( expand("$Fnx(wfileAbsDeep)") == "file.ext" );

	REQUIRE( expand("$Fdnx(wUNCfileAbs)") == "two\\file.ext" );
	REQUIRE( expand("$Fddnx(wUNCfileAbs)") == "one\\two\\file.ext" );
	REQUIRE( expand("$Fd(wUNCfileAbs)") == "two\\" );
	REQUIRE( expand("$Fdd(wUNCfileAbs)") == "one\\two\\" );
	REQUIRE( expand("$Fddd(wUNCfileAbs)") == "share\\one\\two\\" );
	REQUIRE( expand("$Fdddd(wUNCfileAbs)") == "server\\share\\one\\two\\" );
	REQUIRE( expand("$Fddddd(wUNCfileAbs)") == "\\\\server\\share\\one\\two\\" );
	REQUIRE( expand("$Fpn(wUNCfileAbs)") == "\\\\server\\share\\one\\two\\file" );

	REQUIRE( expand("$Fdnxu(wUNCfileAbs)") == "two/file.ext" );
	REQUIRE( expand("$Fddnxu(wUNCfileAbs)") == "one/two/file.ext" );
	REQUIRE( expand("$Fdu(wUNCfileAbs)") == "two/" );
	REQUIRE( expand("$Fdub(wUNCfileAbs)") == "two" );
	REQUIRE( expand("$Fdb(wUNCfileAbs)") == "two" );
	REQUIRE( expand("$Fddu(wUNCfileAbs)") == "one/two/" );
	REQUIRE( expand("$Fu(wUNCfileAbs)") == "//server/share/one/two/file.ext" );

	REQUIRE( expand("$Fdnx(wfileAbsQuoted)") == "two\\three.for" );
	REQUIRE( expand("$Fd(wfileAbsQuoted)") == "two\\" );
	REQUIRE( expand("$Fp(wfileAbsQuoted)") == "c:\\one\\two\\" );
	REQUIRE( expand("$Fn(wfileAbsQuoted)") == "three" );
	REQUIRE( expand("$Fx(wfileAbsQuoted)") == ".for" );
	REQUIRE( expand("$Fnx(wfileAbsQuoted)") == "three.for" );

	REQUIRE( expand("$Fpnx(wfileRel)") == "ein\\zwei\\drei.fir" );
	REQUIRE( expand("$Fd(wfileRel)") == "zwei\\" );
	REQUIRE( expand("$Fp(wfileRel)") == "ein\\zwei\\" );
	REQUIRE( expand("$Fn(wfileRel)") == "drei" );
	REQUIRE( expand("$Fx(wfileRel)") == ".fir" );
	REQUIRE( expand("$Fnx(wfileRel)") == "drei.fir" );

	REQUIRE( expand("$Fdnx(wfileRelSpaces)") == "zw ei\\dr ei.fir" );
	REQUIRE( expand("$Fd(wfileRelSpaces)") == "zw ei\\" );
	REQUIRE( expand("$Fp(wfileRelSpaces)") == "ein\\zw ei\\" );
	REQUIRE( expand("$Fn(wfileRelSpaces)") == "dr ei" );
	REQUIRE( expand("$Fx(wfileRelSpaces)") == ".fir" );
	REQUIRE( expand("$Fnx(wfileRelSpaces)") == "dr ei.fir" );

	REQUIRE( expand("$Fdnx(wfileCurRel)") == ".\\here" );
	REQUIRE( expand("$Fd(wfileCurRel)") == ".\\" );
	REQUIRE( expand("$Fp(wfileCurRel)") == ".\\" );
	REQUIRE( expand("$Fn(wfileCurRel)") == "here" );
	REQUIRE( expand("$Fx(wfileCurRel)") == "" );
	REQUIRE( expand("$Fnx(wfileCurRel)") == "here" );

	REQUIRE( expand("$Fpnx(wfileCurRel2)") == ".\\uno\\dos.tres" );
	REQUIRE( expand("$Fdnx(wfileCurRel2)") == "uno\\dos.tres" );
	REQUIRE( expand("$Fd(wfileCurRel2)") == "uno\\" );
	REQUIRE( expand("$Fp(wfileCurRel2)") == ".\\uno\\" );
	REQUIRE( expand("$Fn(wfileCurRel2)") == "dos" );
	REQUIRE( expand("$Fx(wfileCurRel2)") == ".tres" );
	REQUIRE( expand("$Fnx(wfileCurRel2)") == "dos.tres" );
#endif
}

void testing_$F_regressions(bool verbose)
{
	if (verbose) {
		fprintf( stdout, "\n----- testing_$F_regressions ----\n\n");
	}
	// regressions
	REQUIRE( expand("$Fn(history)") == "history" );
	REQUIRE( expand("$Fqn(history)") == "\"history\"" );
	REQUIRE( expand_as("MASTER", "$F(spool)") == "/condor/test/mspool" );
	REQUIRE( expand_as("MASTER", "$F(history)") == "/condor/test/mspool/history" );
#ifdef ALLOW_SUBSYS_LOCAL_HEIRARCHY
	REQUIRE( expand_lcl_as("LOWER", "MASTER", "$F(history)") == "/condor/lower/mspool/mlhistory" );
#else
	REQUIRE( expand_lcl_as("LOWER", "MASTER", "$F(history)") == "/condor/lower/mspool/history" );
#endif
#ifdef WIN32
	REQUIRE( expand("$F(history)") == "/condor/test\\spool/history" );
#else
	REQUIRE( expand("$F(history)") == "/condor/test/spool/history" );
#endif

	REQUIRE( expand("$F(Items5) ff") == "aa bb cc dd ee ff" );
	REQUIRE( expand("\"$F(Items5Quoted) ff\"") == "\"aa bb cc dd ee ff\"" );
}

void testing_$CHOICE_expand(bool verbose)
{
	if (verbose) {
		fprintf( stdout, "\n----- testing_$CHOICE_expand ----\n\n");
	}

	REQUIRE( expand("$CHOICE(0,AA,BB,CC,DD)") == "AA" );
	REQUIRE( expand("$CHOICE(1,AA,BB,CC,DD)") == "BB" );
	REQUIRE( expand("$CHOICE(2,AA,BB,CC,DD)") == "CC" );
	REQUIRE( expand("$CHOICE(3,AA,BB,CC,DD)") == "DD" );
	//REQUIRE( expand("$CHOICE(4,AA,BB,CC,DD)") == "" );

	REQUIRE( expand("$CHOICE(0,List6c)") == "aa" );
	REQUIRE( expand("$CHOICE(2,List6c)") == "cc" );
	REQUIRE( expand("$CHOICE(3,List6c)") == "dd" );
	REQUIRE( expand("$CHOICE(5,List6c)") == "ff" );

	REQUIRE( expand("$CHOICE(VANILLA,List6c)") == "ff" );
	REQUIRE( expand_as("MASTER", "$CHOICE(VANILLA,List6c)") == "ZA" );
	REQUIRE( expand_lcl("LOWER", "$CHOICE(VANILLA,List6c)") == "ee" );
	REQUIRE( expand_lcl_as("LOWER", "MASTER", "$CHOICE(VANILLA,List6c)") == "ZKM" );

	insert_macro("CHOCOLATE", "$(VANILLA)-2", TestingMacroSet,  TestMacroSource, def_ctx);
	REQUIRE( lookup("CHOCOLATE") == "$(VANILLA)-2");
	REQUIRE( expand("$CHOICE(CHOCOLATE,List6c)") == "dd" );
	REQUIRE( expand_as("MASTER", "$CHOICE(CHOCOLATE,List6c)") == "YY" );
	REQUIRE( expand_lcl("LOWER", "$CHOICE(CHOCOLATE,List6c)") == "cc" );
	REQUIRE( expand_lcl_as("LOWER", "MASTER", "$CHOICE(CHOCOLATE,List6c)") == "XX" );
}

void testing_$SUBSTR_expand(bool verbose)
{
	if (verbose) {
		fprintf( stdout, "\n----- testing_$SUBSTR_expand ----\n\n");
	}
	REQUIRE( expand("$SUBSTR(FOO,2)") == "r" );
	REQUIRE( expand("$SUBSTR(FOO,1,1)") == "a" );
	REQUIRE( expand("$SUBSTR(ITEMS5,-2)") == "ee" );
	REQUIRE( expand("$SUBSTR(ITEMS5,2,-3)") == " bb cc dd" );
	REQUIRE( expand("$SUBSTR(ITEMS5,2,-3)") == " bb cc dd" );
	REQUIRE( expand("$SUBSTR(BAR,10)") == "" );
	REQUIRE( expand("$SUBSTR(BAR,2,-3)") == "" );
	REQUIRE( expand("$SUBSTR(list6c,VM)") == "ee,ff" );
	REQUIRE( expand("$SUBSTR(master.list6c,MPI)") == "XX,YY,ZKM,ZA" );
	REQUIRE( expand_as("MASTER", "$SUBSTR(list6c,vanilla,2)") == "vv" );
	REQUIRE( expand_lcl_as("UPPER", "MASTER", "$SUBSTR(list6c,11,vanilla)") == "YY,ZK" );
	REQUIRE( expand_lcl_as("LOWER", "MASTER", "$SUBSTR(list6c,11,vanilla)") == "YY,Z" );
	REQUIRE( expand("$SUBSTR(fileCompound,-5)") == "se.ex" );
	REQUIRE( expand_as("MASTER", "$SUBSTR(fileCompound,standard)") == "ur/der/base.ex" );
}

void testing_$STRING_expand(bool verbose)
{
	if (verbose) {
		fprintf( stdout, "\n----- testing_$STRING_expand ----\n\n");
	}

	insert_macro("items6", "strcat($(Items5Quoted),\" ff\")", TestingMacroSet,  TestMacroSource, def_ctx);
	insert_macro("VerNum", "split($(Version))[1]", TestingMacroSet,  TestMacroSource, def_ctx);
	insert_macro("FullVerNum", "strcat(split($(Version))[1],\".\",split($(Version))[5])", TestingMacroSet,  TestMacroSource, def_ctx);

	REQUIRE( expand("$STRING(ITEMS5quoted)") == "aa bb cc dd ee" );
	REQUIRE( expand("$STRING(Items6)") == "aa bb cc dd ee ff" );
	REQUIRE( expand("$STRING(VerNum)") == "8.5.6" );
	REQUIRE( expand("$STRING(FullVerNum)") == "8.5.6.998822" );

	REQUIRE( expand("$STRING(list6cq)") == "aa,bb, cc,dd,ee,ff" );
	REQUIRE( expand_as("MASTER", "$STRING(list6cq)") == "JMK,Vvv,XX,YY,ZKM,ZA" );

	// if param lookup fails, we parse as an expression, and a simple variable name
	// will parse as an attribute reference and then round trip unchanged. it's 
	// unclear that this is the correct thing to do.  but it's what happens right now.
	REQUIRE( expand("$STRING(BAR)") == "BAR" );

	// now do some formatting tests...
	REQUIRE( expand("$STRING(Items6,%20s)") == "   aa bb cc dd ee ff" );
	REQUIRE( expand("$STRING(Items6,%-20s)") == "aa bb cc dd ee ff   " );
	REQUIRE( expand("$STRING(Items6,%10.10s)") == "aa bb cc d" );
}

void testing_$INT_expand(bool verbose)
{
	if (verbose) {
		fprintf( stdout, "\n----- testing_$INT_expand ----\n\n");
	}
	REQUIRE( expand("$INT(VANILLA)") == "5" );
	REQUIRE( expand("$INT(STANDARD)") == "1" );
	REQUIRE( expand("$INT(StartIdleTime)") == "900" );
	REQUIRE( expand("$INT(highload)") == "0" );
	REQUIRE( expand("$INT(4+4)") == "8" );
	REQUIRE( expand_as("MASTER", "$INT(STANDARD)") == "2" );
	REQUIRE( expand("$INT(StandardMinusVM)") == "-12" );
	REQUIRE( expand_as("MASTER", "$INT(StandardMinusVM)") == "-11" );
	REQUIRE( expand("$INT(HalfVanilla)") == "2" );

	REQUIRE( expand("$INT(VANILLA,%03d)") == "005" );
	REQUIRE( expand("$INT(VANILLA,  %d)") == "  5" );
	REQUIRE( expand("$INT(VANILLA,_%04u_)") == "_0005_" );
	REQUIRE( expand("$INT(DoubleVanilla,%d)") == "10" );
	REQUIRE( expand_lcl("LOWER","$INT(DoubleVanilla,%02d)") == "08" );
}

void testing_$REAL_expand(bool verbose)
{
	if (verbose) {
		fprintf( stdout, "\n----- testing_$REAL_expand ----\n\n");
	}
	REQUIRE( expand("$REAL(4.56)") == "4.56" );
	REQUIRE( expand("$REAL(4.56,%.1f)") == "4.6" );
	REQUIRE( expand("$REAL(11/3.0, _%.3f_)") == " _3.667_" );
	REQUIRE( expand("$REAL(vanilla)") == "5" );
	REQUIRE( expand("$REAL(hALFvANILLA)") == "2.5" );
	REQUIRE( expand_lcl("LOWER", "$REAL(hALFvANILLA,%.2f)") == "2.00" );
	REQUIRE( expand("$REAL(standard,%2f)") == "1.000000" );
	REQUIRE( expand_as("MASTER","$REAL(standard,%06.3f)") == "02.000" );
	REQUIRE( expand("$REAL(BackgroundLoad)") == "0.3" );
	REQUIRE( expand("$REAL(BackgroundLoad,%g)") == "0.3" );
	REQUIRE( within(expand("$REAL(BackgroundLoad,%e)"), "3.000000e-0001", "3.000000e-01") );
}

void testing_$EVAL_expand(bool verbose)
{
	if (verbose) {
		fprintf( stdout, "\n----- testing_$EVAL_expand ----\n\n");
	}
	REQUIRE( expand("$EVAL(4)") == "4" );
	REQUIRE( expand("$EVAL(4+4)") == "8" );
	REQUIRE( expand("$EVAL(bar)") == "undefined" );

	insert_macro("SplitVer", "split($(Version))", TestingMacroSet,  TestMacroSource, def_ctx);
	REQUIRE( expand("$EVAL(SplitVer)") == "{ \"$Version:\",\"8.5.6\",\"May\",\"20\",\"2016\",\"998822\",\"$\" }" );

	auto_free_ptr mapping(strdup("* alice Security,MetalShop\n* bob Security,WoodShop\n"));
	add_user_mapping("grouptest", mapping.ptr());
	insert_macro("BobsGroups", "userMap(\"grouptest\",\"bob\")", TestingMacroSet,  TestMacroSource, def_ctx);
	insert_macro("BobsFirst", "userMap(\"grouptest\",\"bob\",undefined)", TestingMacroSet,  TestMacroSource, def_ctx);
	insert_macro("BobsShop", "userMap(\"grouptest\",\"bob\",\"woodshop\")", TestingMacroSet,  TestMacroSource, def_ctx);
	insert_macro("BobsUnshop", "userMap(\"grouptest\",\"bob\",\"shop\")", TestingMacroSet,  TestMacroSource, def_ctx);
	REQUIRE( expand("$EVAL(BobsGroups)") == "Security,WoodShop" );
	REQUIRE( expand("$EVAL(BobsFirst)") == "Security" );
	REQUIRE( expand("$EVAL(BobsShop)") == "WoodShop" );
	REQUIRE( expand("$EVAL(BobsUnShop)") == "Security" );

	insert_macro("AlicesGroups", "userMap(\"grouptest\",\"alice\")", TestingMacroSet,  TestMacroSource, def_ctx);
	insert_macro("AlicesFirst", "userMap(\"grouptest\",\"alice\",undefined)", TestingMacroSet,  TestMacroSource, def_ctx);
	insert_macro("AlicesShop", "userMap(\"grouptest\",\"alice\",\"metalshop\")", TestingMacroSet,  TestMacroSource, def_ctx);
	insert_macro("AlicesUnshop", "userMap(\"grouptest\",\"alice\",\"shop\",\"AutoShop\")", TestingMacroSet,  TestMacroSource, def_ctx);
	insert_macro("AlicesDefGroup", "userMap(\"grouptest\",\"alice\",undefined,\"AutoShop\")", TestingMacroSet,  TestMacroSource, def_ctx);
	REQUIRE( expand("$EVAL(AlicesGroups)") == "Security,MetalShop" );
	REQUIRE( expand("$EVAL(AlicesFirst)") == "Security" );
	REQUIRE( expand("$EVAL(AlicesShop)") == "MetalShop" );
	REQUIRE( expand("$EVAL(AlicesUnShop)") == "Security" );
	REQUIRE( expand("$EVAL(AlicesDefGroup)") == "Security" );

	insert_macro("JohnsGroups", "userMap(\"grouptest\",\"john\")", TestingMacroSet,  TestMacroSource, def_ctx);
	insert_macro("JohnsFirst", "userMap(\"grouptest\",\"john\",undefined)", TestingMacroSet,  TestMacroSource, def_ctx);
	insert_macro("JohnsShop", "userMap(\"grouptest\",\"john\",\"metalshop\")", TestingMacroSet,  TestMacroSource, def_ctx);
	insert_macro("JohnsUnShop", "userMap(\"grouptest\",\"john\",\"shop\",\"AutoShop\")", TestingMacroSet,  TestMacroSource, def_ctx);
	insert_macro("JohnsDefGroup", "userMap(\"grouptest\",\"john\",undefined,\"AutoShop\")", TestingMacroSet,  TestMacroSource, def_ctx);
	REQUIRE( expand("$EVAL(JohnsGroups)") == "undefined" );
	REQUIRE( expand("$EVAL(JohnsFirst)") == "undefined" );
	REQUIRE( expand("$EVAL(JohnsShop)") == "undefined" );
	REQUIRE( expand("$EVAL(JohnsUnShop)") == "AutoShop" );
	REQUIRE( expand("$EVAL(JohnsDefGroup)") == "AutoShop" );
}

// runs all of the tests in non-verbose mode by default (i.e. printing only failures)
// individual groups of tests can be run by using the -t:<tests> option where <tests>
// is one or more of 
//   l   testing_lookups
//   $   testing_$_expand
//   [   testing_$$_expand
//   e   testing_$ENV_expand
//   F   testing_$F_expand and testing_$F_regressions
//   c   testing_$CHOICE_expand
//   s   testing_$SUBSTR_expand
//   i   testing_$INT_expand
//   r   testing_$REAL_expand
//   n   testing_$INT_expand and testing_$REAL_expand
//   r   testing_$RAND_expand (RANDOM_INTEGER and RANDOM_CHOICE)
//   P   testing_parser  config/submit/metaknob parser.
//
int main( int /*argc*/, const char ** argv) {

	int test_flags = 0;
	const char * pcolon;

	for (int ii = 1; argv[ii]; ++ii) {
		const char *arg = argv[ii];
		if (is_dash_arg_prefix(arg, "verbose", 1)) {
			dash_verbose = 1;
		} else if (is_dash_arg_colon_prefix(arg, "test", &pcolon, 1)) {
			if (pcolon) {
				while (*++pcolon) {
					switch (*pcolon) {
					case 'l': test_flags |= 0x0001; break; // lookup
					case '$': test_flags |= 0x0002; break; // $
					case '[': test_flags |= 0x0004; break; // $$
					case 'e': test_flags |= 0x0008; break; // $ENV
					case 'F': test_flags |= 0x0010 | 0x0020; break;	 // $F
					case 'c': test_flags |= 0x0040; break; // $CHOICE
					case 's': test_flags |= 0x0080; break; // $SUBSTR, $STRING
					case 'i': test_flags |= 0x0100; break; // $INT
					case 'f': test_flags |= 0x0200; break; // $REAL
					case 'n': test_flags |= 0x0100 | 0x0200; break; // $INT, $REAL
					case 'v': test_flags |= 0x0400; break; // $EVAL
					case 'r': test_flags |= 0x0800; break; // $RANDOM_INTEGER and $RANDOM_CHOICE
					case 'p': test_flags |= 0x1000; break; // parse
					}
				}
			} else {
				test_flags = 3;
			}
		} else {
			fprintf(stderr, "unknown argument %s\n", arg);
			fprintf(stderr, "usage: %s [ -verbose ] [ -test[:<tests>] ]\n", argv[0]);
			fprintf(stderr,
				"   one or more single letters select individual tests:\n"
				"      l param lookups\n"
				"      $ $ expansion\n"
				"      [ $$ expansion\n"
				"      e $ENV expansion\n"
				"      F $F expansion\n"
				"      c $CHOICE expansion/evaluation\n"
				"      s $SUBSTR, $STRING expansion/evaluation\n"
				"      i $INT expansion/evaluation\n"
				"      f $REAL expansion/evaluation\n"
				"      n $INT, $REAL expansion/evaluation\n"
				"      v $EVAL expansion/evaluation\n"
				"      r $RANDOM_INTEGER and $RANDOM_CHOICE expansion/evaluation\n"
				"      p config parsing\n"
				"   default is to run all tests\n"
				"   If -verbose is specified, all tests show output, otherwise\n"
				"   only failed tests produce output\n"
				);
			return 1;
		}
	}
	if ( ! test_flags) test_flags = -1;

	ClassAdReconfig();

	TestingMacroSet.defaults->size = param_info_init((const void**)&TestingMacroSet.defaults->table);
	TestingMacroSet.options |= CONFIG_OPT_DEFAULTS_ARE_PARAM_INFO;

	insert_testing_macros(NULL, "TEST");

	optimize_macros(TestingMacroSet);

	if (test_flags & 0x0001) testing_lookups(dash_verbose);
	if (test_flags & 0x0002) testing_$_expand(dash_verbose);
	if (test_flags & 0x0004) testing_$$_expand(dash_verbose);
	if (test_flags & 0x0008) testing_$ENV_expand(dash_verbose);

	if (test_flags & 0x0010) testing_$F_expand(dash_verbose);
	if (test_flags & 0x0020) testing_$F_regressions(dash_verbose);

	if (test_flags & 0x0040) testing_$CHOICE_expand(dash_verbose);
	if (test_flags & 0x0080) testing_$SUBSTR_expand(dash_verbose);
	if (test_flags & 0x0080) testing_$STRING_expand(dash_verbose);

	if (test_flags & 0x0100) testing_$INT_expand(dash_verbose);
	if (test_flags & 0x0200) testing_$REAL_expand(dash_verbose);
	if (test_flags & 0x0400) testing_$EVAL_expand(dash_verbose);

	if (test_flags & 0x0800) testing_$RAND_expand(dash_verbose);

	if (test_flags & 0x1000) testing_parser(dash_verbose);

	return fail_count > 0;
}
