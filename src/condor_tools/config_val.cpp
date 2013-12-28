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


 

/***********************************************************************
*
* Print declarations from the condor config files, condor_config and
* condor_config.local.  Declarations in files specified by
* LOCAL_CONFIG_FILE override those in the global config file, so this
* prints the same declarations found by condor programs using the
* config() routine.
*
* In addition, the configuration of remote machines can be queried.
* You can specify either a name or sinful string you want to connect
* to, what kind of daemon you want to query, and what pool to query to
* find the specified daemon.
*
***********************************************************************/

#include "condor_common.h"
#include "condor_config.h"
#include "condor_io.h"
#include "condor_uid.h"
#include "match_prefix.h"
#include "string_list.h"
#include "condor_string.h"
#include "get_daemon_name.h"
#include "daemon.h"
#include "dc_collector.h"
#include "daemon_types.h"
#include "internet.h"
#include "condor_distribution.h"
#include "string_list.h"
#include "simplelist.h"
#include "subsystem_info.h"
#include "Regex.h"
#ifdef WIN32
 #include "exception_handling.WINDOWS.h"
#endif
#include "param_info.h" // access to default params
#include "param_info_tables.h"
#include "condor_version.h"

#include <sstream>
#include <algorithm> // for std::sort

const char * MyName;

StringList params;
daemon_t dt = DT_MASTER;
bool mixedcase = false;
bool diagnostic = false;

// The pure-tools (PureCoverage, Purify, etc) spit out a bunch of
// stuff to stderr, which is where we normally put our error
// messages.  To enable config_val.test to produce easily readable
// output, even with pure-tools, we just write everything to stdout.  
#if defined( PURE_DEBUG ) 
#	define stderr stdout
#endif

enum PrintType {CONDOR_OWNER, CONDOR_TILDE, CONDOR_NONE};
enum ModeType {CONDOR_QUERY, CONDOR_SET, CONDOR_UNSET,
			   CONDOR_RUNTIME_SET, CONDOR_RUNTIME_UNSET};

void PrintMetaParam(const char * name);

// On some systems, the output from config_val sometimes doesn't show
// up unless we explicitly flush before we exit.
void PREFAST_NORETURN
my_exit( int status )
{
	fflush( stdout );
	fflush( stderr );

	if ( ! status ) {
		clear_config();
		// this is here to validate that we can still param() with an empty param table.
		char *dummy = param("DUMMY"); if (dummy) free(dummy);
	}

	exit( status );
}


void
usage(int retval = 1)
{
#if 1
	fprintf(stderr, "Usage: %s <help>\n\n", MyName);
	fprintf(stderr, "       %s [<location>] <edit> \n\n", MyName);
	fprintf(stderr, "       %s [<location>] [<view>] <vars>\n\n", MyName);
	fprintf(stderr,
		"    where <edit> is one set/unset operation with one or more arguments\n"
		"\t-set \"<var> = <value>\"\tSet persistent <var> to <value>\n"
		"\t-unset <var>\t\tRevert persistent <var> to previous value\n"
		"\t-rset \"<var> = <value>\"\tSet runtime <var> to <value>\n"
		"\t-runset <var>\t\tRevert runtime <var> to previous value\n"
		"\n    where <vars> is <var> [<var>...]\tPrint the value of <var>. The\n"
		"\tvalue is expanded unless -raw, -evaluate, -default or -dump is\n"
		"\tspecified. When used with -dump, <var> is regular expression.\n"
		"\n    where <view> is one or more of\n"
		"\t-dump\t\tPrint values of all variables that match <var>\n"
		"\t\t\tThe value is raw unless -expand, -default, or -evaluate\n"
		"\t\t\tis specified. If no <vars>, Print all variables\n"
		"\t-default\tPrint default value\n"
		"\t-expand\t\tPrint expanded value\n"
		"\t-raw\t\tPrint raw value as it appears in the file\n"
		//"\t-stats\t\tPrint statistics of the configuration system\n"
		"\t-verbose\tPrint source, raw, expanded, and default values\n"
		"\t-debug[:<opts>] dprintf to stderr, optionally overiding TOOL_DEBUG\n"
		//"\t-diagnostic\t\tPrint diagnostic information about condor_config_val operation\n"
		"      these options apply when querying a daemon with a <location> argument\n"
		"\t-evaluate\tevaluate with respect to the <daemon> ClassAd\n"
		"\t-used\t\tPrint only variables used by the specified daemon\n"
		"\t-unused\t\tPrint only variables not used by the specified daemon\n"
		"      these options apply when reading configuration files\n"
		"\t-config\t\tPrint the locations of configuration files\n"
		"\t-writeconfig <file>\tWrite non-default configuration to <file>\n"
		//"\t-reconfig <file>\tReload configuration files and append <file>\n"
		"\t-mixedcase\tPrint variable names as originally specified\n"
		"\t-local-name <name>  Use local name for querying and expanding\n"
		"\t-subsystem <daemon> Subsystem/Daemon name for querying and\n"
		"\t\t\texpanding. The default subsystem is TOOL\n"
		//"\t-tilde\t\tReturn the path to the Condor home directory\n"
		//"\t-owner\t\tReturn the owner of the condor_config_val process\n"
		"\n    where <location> is one or more of\n"
		"\t-address <ip:port>\tConnect to the given ip/port\n"
		"\t-pool <hostname>\tUse the given central manager to find daemons\n"
		"\t-name <daemon_name>\tQuery the specified daemon\n"
		"\t-master\t\t\tQuery the master\n"
		"\t-schedd\t\t\tQuery the schedd\n"
		"\t-startd\t\t\tQuery the startd\n"
		"\t-collector\t\tQuery the collector\n"
		"\t-negotiator\t\tQuery the negotiator\n"
		"\n    where <help> is one of\n"
		"\t-help\t\tPrint this screen and exit\n"
		"\t-version\tPrint HTCondor version and exit\n"
		);
#else
	fprintf( stderr, "Usage: %s [options] variable [variable] ...\n", MyName );
	fprintf( stderr,
			 "   or: %s [options] -set string [string] ...\n",
			 MyName );
	fprintf( stderr,
			 "   or: %s [options] -rset string [string] ...\n",
			 MyName );
	fprintf( stderr, "   or: %s [options] -unset variable [variable] ...\n",
			 MyName );
	fprintf( stderr, "   or: %s [options] -runset variable [variable] ...\n",
			 MyName );
	fprintf( stderr, "   or: %s [options] -tilde\n", MyName );
	fprintf( stderr, "   or: %s [options] -owner\n", MyName );
	fprintf( stderr, "   or: %s -dump [-verbose] [-expand]\n", MyName );
	fprintf( stderr, "\n   Valid options are:\n" );
	fprintf( stderr, "   -name daemon_name\t(query the specified daemon for its configuration)\n" );
	fprintf( stderr, "   -pool hostname\t(use the given central manager to find daemons)\n" );
	fprintf( stderr, "   -address <ip:port>\t(connect to the given ip/port)\n" );
	fprintf( stderr, "   -set\t\t\t(set a persistent config file expression)\n" );
	fprintf( stderr, "   -rset\t\t(set a runtime config file expression\n" );

	fprintf( stderr, "   -unset\t\t(unset a persistent config file expression)\n" );
	fprintf( stderr, "   -runset\t\t(unset a runtime config file expression)\n" );

	fprintf( stderr, "   -master\t\t(query the master)\n" );
	fprintf( stderr, "   -schedd\t\t(query the schedd)\n" );
	fprintf( stderr, "   -startd\t\t(query the startd)\n" );
	fprintf( stderr, "   -collector\t\t(query the collector)\n" );
	fprintf( stderr, "   -negotiator\t\t(query the negotiator)\n" );
	fprintf( stderr, "   -tilde\t\t(return the path to the Condor home directory)\n" );
	fprintf( stderr, "   -owner\t\t(return the owner of the condor_config_val process)\n" );
	fprintf( stderr, "   -local-name name\t(Specify a local name for use with the config system)\n" );
	fprintf( stderr, "   -verbose\t\t(print information about where variables are defined)\n" );
	fprintf( stderr, "   -dump\t\t(print locally defined variables)\n" );
	fprintf( stderr, "   -expand\t\t(with -dump, expand macros from config files)\n" );
	fprintf( stderr, "   -evaluate\t\t(when querying <daemon>, evaluate param with respect to classad from <daemon>)\n" );
	fprintf( stderr, "   -config\t\t(print the locations of found config files)\n" );
	fprintf( stderr, "   -debug[:<flags>]\t\t(dprintf to stderr, optionally overiding the TOOL_DEBUG flags)\n" );
#endif
	my_exit(retval);
}

typedef union _write_config_options {
	unsigned int all;
	struct {
		unsigned int show_only         :1; // show only items indicated by next 7 flags
		unsigned int show_def          :1;
		unsigned int show_def_match    :1;
		unsigned int show_detected     :1;
		unsigned int show_env          :1;
		unsigned int show_dup          :1; // show default items hidden by file items
		unsigned int show_6            :1; // future
		unsigned int show_7            :1; // future
		unsigned int comment_source    :1;
		unsigned int comment_def       :1;
		unsigned int comment_def_match :1;
		unsigned int comment_detected  :1;
		unsigned int comment_env       :1;
		unsigned int comment_5         :1;
		unsigned int comment_6         :1;
		unsigned int comment_7         :1;
		unsigned int sort_name         :1;
		unsigned int hide_obsolete     :1;
		unsigned int hide_if_match     :1;
	};
} WRITE_CONFIG_OPTIONS;

char* GetRemoteParam( Daemon*, char* );
char* GetRemoteParamRaw(Daemon*, const char* name, bool & raw_supported, MyString & raw_value, MyString & file_and_line, MyString & def_value, MyString & usage_report);
int GetRemoteParamStats(Daemon* target, ClassAd & ad);
int   GetRemoteParamNamesMatching(Daemon*, const char* name, std::vector<std::string> & names);
void  SetRemoteParam( Daemon*, char*, ModeType );
static void PrintConfigSources(void);
static void do_dump_config_stats(FILE * fh, bool dump_sources, bool dump_strings);
static int do_write_config(const char* pathname, WRITE_CONFIG_OPTIONS opts);

static const char * param_type_names[] = {"STRING","INT","BOOL","DOUBLE","LONG"};

void print_as_type(const char * name, int as_type)
{
	if (as_type == PARAM_TYPE_BOOL) {
		int bvalid = -1;
		int bval = param_default_boolean(name, NULL, &bvalid);
		bool bb = param_boolean(name, bval, false);
		fprintf(stdout, " # eval: %s def=%d (%s)\n", bb ? "true" : "false", bval, bvalid ? "valid" : "invalid");
	} else if (as_type == PARAM_TYPE_INT) {
		int ivalid = -1, ilong = 0;
		int ival = param_default_integer(name, NULL, &ivalid, &ilong);
		fprintf(stdout, " # eval: %d (%s%s)\n", ival, ivalid ? "valid" : "invalid", ilong ? ", long" : "");
	} else if (as_type == PARAM_TYPE_DOUBLE) {
		int dvalid = -1;
		double dval = param_default_double(name, NULL, &dvalid);
		fprintf(stdout, " # eval: %.16g (%s)\n", dval, dvalid ? "valid" : "invalid");
	}
}

const char * report_config_source(MACRO_META * pmeta, MyString & source)
{
	source = config_source_by_id(pmeta->source_id);
	if (pmeta->source_line < 0) {
		if (pmeta->source_id == 1) {
			source.formatstr_cat(", item %d", pmeta->param_id);
		}
	} else {
		source.formatstr_cat(", line %d", pmeta->source_line);
	}
	return source.Value();
}

// increment the ref count of params referenced by default params
// but preserve the ref count of the param being queried.
// we use this to generate a pseudo used-param list for condor_config_val.
bool add_ref_callback(void* /*pv*/, HASHITER & it)
{
	MACRO_META * pmeta = hash_iter_meta(it);
	if ( ! pmeta) return true;

	if (pmeta->source_id < 4 && pmeta->param_id >= 0) {
		int ix = pmeta->param_id;

		// save use/ref count for this item
		macro_defaults::META tmp;
		if (it.is_def) {
			if (it.set.defaults && it.set.defaults->metat && ix < it.set.defaults->size) {
				tmp = it.set.defaults->metat[ix];
			}
		} else {
			tmp.ref_count = pmeta->ref_count;
			tmp.use_count = pmeta->use_count;
		}

		char * value = param(hash_iter_key(it));
		if (value) free(value);

		// restore item use/ref count.
		if (it.is_def) {
			if (it.set.defaults && it.set.defaults->metat && ix < it.set.defaults->size) {
				it.set.defaults->metat[ix] = tmp;
			}
		} else {
			pmeta->ref_count = tmp.ref_count;
			pmeta->use_count = tmp.use_count;
		}
	}
	return true;
}

bool dump_both_verbose = false;
bool dump_both_type = false;
bool dump_both_used_only = false;
int  dump_both_only_type = -1;
int  dump_both_count = 0;
bool dump_both_callback(void* pv, HASHITER & it)
{
	std::string * pstr = (std::string *)pv;

	MACRO_META * pmeta = hash_iter_meta(it);
	bool used = pmeta->use_count != 0 || pmeta->ref_count != 0;
	if (dump_both_used_only && ! used)
		return true;

	param_info_t_type_t ty = param_default_type_by_id(pmeta->param_id);
	int effective_type = (int)ty;
	if (dump_both_only_type == PARAM_TYPE_INT) {
		if (ty == PARAM_TYPE_LONG) effective_type = (int)PARAM_TYPE_INT;
	}
	if (dump_both_only_type > 0 && dump_both_only_type != effective_type)
		return true;

	const char * tname = "?";
	if (ty >= PARAM_TYPE_STRING && ty <= PARAM_TYPE_LONG) {
		tname = param_type_names[ty];
	}

	const char * name = hash_iter_key(it);
	const char * rawval = hash_iter_value(it);
	/* debugging code
	if ( ! dump_both_verbose) {
		const char * pre = "  ";
		switch (pmeta->flags & 6) {
			case 2: pre = " i"; break;
			case 4: pre = "d "; break;
			case 6: pre = "di"; break;
		}
		bool dup = (*pstr == name);
		fprintf(stdout, "%s%s", pre, dup ? "<" : " ");
	}
	*/
	fprintf(stdout, "%s = %s\n", name, rawval ? rawval : "");
	if (dump_both_verbose) {
		const char * filename = config_source_by_id(pmeta->source_id);
		if (pmeta->source_line < 0) {
			if (pmeta->source_id == 1) {
				fprintf(stdout, " # at: %s, item %d\n", filename, pmeta->param_id);
			} else {
				fprintf(stdout, " # at: %s\n", filename);
			}
		} else {
			fprintf(stdout, " # at: %s, line %d\n", filename, pmeta->source_line);
		}
		if (rawval && rawval[0]) {
			char * val = expand_param(rawval, NULL, 0);
			if (val) {
				fprintf(stdout, " # expanded: %s\n", val);
				free(val);
			}
		}
		if (dump_both_type) {
			if (tname[0] == '?') {
				fprintf(stdout, " # type: %s:%d\n", tname, ty);
			} else {
				fprintf(stdout, " # type: %s\n", tname);
			}
		}
		if (dump_both_only_type > 0) {
			print_as_type(name, dump_both_only_type);
		}

		//const char * def_val = param_default_string(name, subsys);
		//fprintf(stdout, " # default: %s\n", def_val);
		if (pmeta->ref_count) fprintf(stdout, " # use_count: %d / %d\n", pmeta->use_count, pmeta->ref_count);
		else fprintf(stdout, " # use_count: %d\n", pmeta->use_count);
	}

	pstr->assign(name);
	++dump_both_count;
	return true;
}


char* EvaluatedValue(char const* value, ClassAd const* ad) {
    classad::Value res;
    ClassAd empty;
    bool rc = (ad) ? ad->EvaluateExpr(value, res) : empty.EvaluateExpr(value, res);
    if (!rc) return NULL;
    std::stringstream ss;
    ss << res;
    return strdup(ss.str().c_str());
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

// remote queries return raw values as
// EFFECTIVE_NAME = value
// this function returns a pointer to the value part or
// it returns the input string if there is no = in the string.
//
static const char * RemoteRawValuePart(MyString & raw)
{
	const char * praw = raw.Value();
	while (*praw) {
		if (*praw == '=') {
			++praw;
			while (*praw == ' ') ++praw;
			return praw;
		}
		++praw;
	}
	return raw.Value();
}

int
main( int argc, const char* argv[] )
{
	char	*value = NULL, *tmp = NULL;
	const char * pcolon;
	const char *host = NULL;
	const char *addr = NULL;
	const char *name = NULL;
	const char *pool = NULL;
	const char *local_name = NULL;
	const char *subsys = "TOOL";
	const char *reconfig_source = NULL;
	bool	ask_a_daemon = false;
	bool    verbose = false;
	bool    dump_strings = false;
	bool    dash_usage = false;
	bool    dash_dump_both = false;
	bool    dump_all_variables = false;
	bool    dump_stats = false;
	bool    expand_dumped_variables = false;
	bool    show_by_usage = false;
	bool    show_by_usage_unused = false;
	bool    evaluate_daemon_vars = false;
	bool    print_config_sources = false;
	const char * write_config = NULL;
	WRITE_CONFIG_OPTIONS write_config_flags = {0};
	bool    dash_debug = false;
	bool    dash_raw = false;
	bool    dash_default = false;
	bool    stats_with_defaults = false;
	const char * debug_flags = NULL;

#ifdef WIN32
	// uncomment this if you need to debug crashes.
	//g_ExceptionHandler.TurnOff();
#endif

	PrintType pt = CONDOR_NONE;
	ModeType mt = CONDOR_QUERY;

	MyName = argv[0];
	myDistro->Init( argc, argv );

	for (int i = 1; i < argc; ++i) {
#if 1
		// arguments that don't begin with "-" are params to be looked up.
		if (*argv[i] != '-') {
			params.append(strdup(argv[i]));
			continue;
		}

		// if we get here, the arg begins with "-",
		const char * arg = argv[i]+1;
		if (is_arg_prefix(arg, "host", 1)) { // as of 8/27/2013 -host is a secret option used by condor_init
			host = use_next_arg("host", argv, i);
		} else if (is_arg_prefix(arg, "name", 1)) {
			const char * tmp = use_next_arg("name", argv, i);
			name = get_daemon_name(tmp);
			if ( ! name) {
				fprintf(stderr, "%s: unknown host %s\n", MyName, get_host_part(tmp));
				my_exit(1);
			}
		} else if (is_arg_prefix(arg, "address", 1)) {
			addr = use_next_arg("address", argv, i);
			if ( ! is_valid_sinful(addr)) {
				fprintf(stderr, "%s: invalid address %s\n"
						"Address must be of the form \"<111.222.333.444:555>\n"
						"   where 111.222.333.444 is the ip address and 555 is the port\n"
						"   you wish to connect to (the punctuation is important).\n", 
						MyName, addr);
			}
		} else if (is_arg_prefix(arg, "pool", 1)) {
			pool = use_next_arg("pool", argv, i);
		} else if (is_arg_prefix(arg, "local-name", 2)) {
			local_name = use_next_arg("local_name", argv, i);
		} else if (is_arg_prefix(arg, "subsystem", 3)) {
			subsys = use_next_arg("subsystem", argv, i);
		} else if (is_arg_prefix(arg, "owner", 1)) {
			pt = CONDOR_OWNER;
		} else if (is_arg_prefix(arg, "tilde", 1)) {
			pt = CONDOR_TILDE;
		} else if (is_arg_prefix(arg, "master", 2)) {
			dt = DT_MASTER;
			ask_a_daemon = true;
		} else if (is_arg_prefix(arg, "schedd", 2)) {
			dt = DT_SCHEDD;
		} else if (is_arg_prefix(arg, "startd", 2)) {
			dt = DT_STARTD;
		} else if (is_arg_prefix(arg, "collector", 3)) {
			dt = DT_COLLECTOR;
		} else if (is_arg_prefix(arg, "negotiator", 3)) {
			dt = DT_NEGOTIATOR;
		} else if (is_arg_prefix(arg, "set", 2)) {
			mt = CONDOR_SET;
		} else if (is_arg_prefix(arg, "unset", 3)) {
			mt = CONDOR_UNSET;
		} else if (is_arg_prefix(arg, "rset", 3)) {
			mt = CONDOR_RUNTIME_SET;
		} else if (is_arg_prefix(arg, "runset", 5)) {
			mt = CONDOR_RUNTIME_UNSET;
		} else if (is_arg_prefix(arg, "mixedcase", 2)) {
			mixedcase = true;
		} else if (is_arg_prefix(arg, "diagnostic", 4)) {
			diagnostic = true;
		} else if (is_arg_prefix(arg, "raw", 3)) {
			dash_raw = true;
		} else if (is_arg_prefix(arg, "default", 3)) {
			dash_default = true;
		} else if (is_arg_prefix(arg, "config", 2)) {
			print_config_sources = true;
		} else if (is_arg_prefix(arg, "reconfig", 5)) {
			reconfig_source = use_next_arg("reconfig", argv, i);
		} else if (is_arg_colon_prefix(arg, "verbose", &pcolon, 1)) {
			verbose = true;
			if (pcolon) {
				StringList opts(pcolon+1,":,");
				opts.rewind();
				while (NULL != (tmp = opts.next())) {
					if (is_arg_prefix(tmp, "usage", 2) ||
						is_arg_prefix(tmp, "use", -1)) {
						dash_usage = true;
					} else if (is_arg_prefix(tmp, "both", -1)) {
						dash_dump_both = true;
					} else if (is_arg_prefix(tmp, "terse", -1)) {
						verbose = false;
					} else if (is_arg_prefix(tmp, "strings", -1)) {
						dump_strings = true;
					} else if (is_arg_prefix(tmp, "type", 2)) {
						dash_dump_both = true;
						dump_both_type = true;
					} else if (is_arg_prefix(tmp, "int", -1)) {
						dash_dump_both = true;
						dump_both_type = true;
						dump_both_only_type = PARAM_TYPE_INT;
					} else if (is_arg_prefix(tmp, "bool", -1)) {
						dash_dump_both = true;
						dump_both_type = true;
						dump_both_only_type = PARAM_TYPE_BOOL;
					} else if (is_arg_prefix(tmp, "double", -1)) {
						dash_dump_both = true;
						dump_both_type = true;
						dump_both_only_type = PARAM_TYPE_DOUBLE;
					} else if (is_arg_prefix(tmp, "long", -1)) {
						dash_dump_both = true;
						dump_both_type = true;
						dump_both_only_type = PARAM_TYPE_LONG;
					}
				}
			}
		} else if (is_arg_prefix(arg, "dump", 1)) {
			dump_all_variables = true;
		} else if (is_arg_colon_prefix(arg, "stats", &pcolon, 4)) {
			dump_stats = true;
			if (pcolon && is_arg_prefix(pcolon+1, "keep_defaults", 2)) {
				stats_with_defaults = true;
			}
		} else if (is_arg_prefix(arg, "expanded", 2)) {
			expand_dumped_variables = true;
		} else if (is_arg_prefix(arg, "evaluate", 2)) {
			evaluate_daemon_vars = true;
		} else if (is_arg_prefix(arg, "unused", 4)) {
			show_by_usage = true;
			show_by_usage_unused = true;
		} else if (is_arg_prefix(arg, "used", 2)) {
			show_by_usage = true;
			show_by_usage_unused = false;
			//dash_usage = true;
		} else if (is_arg_colon_prefix(arg, "writeconfig", &pcolon, 3)) {
			write_config = use_next_arg("writeconfig", argv, i);
			write_config_flags.all = 0;
			if (pcolon) {
				StringList opts(pcolon+1,":,");
				opts.rewind();
				bool comment = false;
				while (NULL != (tmp = opts.next())) {
					if (is_arg_prefix(tmp, "comment", 3)) {
						comment = true;
					} else if (is_arg_prefix(tmp, "default", 3)) {
						write_config_flags.show_def = 1;
						write_config_flags.comment_def = comment;
					} else if (is_arg_prefix(tmp, "matches_default", 5)) {
						write_config_flags.show_def_match = 1;
						write_config_flags.comment_def_match = comment;
					} else if (is_arg_prefix(tmp, "detected", 3)) {
						write_config_flags.show_detected = 1;
						write_config_flags.comment_detected = comment;
					} else if (is_arg_prefix(tmp, "environment", 3)) {
						write_config_flags.show_env = 1;
						write_config_flags.comment_env = comment;
					} else if (is_arg_prefix(tmp, "only", -1)) {
						write_config_flags.show_only = 1;
					} else if (is_arg_prefix(tmp, "file", -1)) {
						write_config_flags.show_only = 0;
					} else if (is_arg_prefix(tmp, "duplicate", 3)) {
						write_config_flags.show_dup = 1;
					} else if (is_arg_prefix(tmp, "source", 3)) {
						write_config_flags.comment_source = 1;
					} else if (is_arg_prefix(tmp, "sort", 3)) {
						write_config_flags.sort_name = 1;
					} else if (is_arg_prefix(tmp, "upgrade", 3)) {
						write_config_flags.hide_obsolete = 1;
						write_config_flags.hide_if_match = 1;
					}
				}
			} else {
				write_config_flags.show_def_match = true;
				write_config_flags.comment_def_match = true;
				write_config_flags.sort_name = false;
			}
		} else if (is_arg_colon_prefix(arg, "debug", &pcolon, 2)) {
				// dprintf to console
			dash_debug = true;
			if (pcolon) { 
				debug_flags = ++pcolon;
				if (*debug_flags == '"') ++debug_flags;
			}
		} else if (is_dash_arg_prefix(argv[i], "help", 2)) {
			usage();
		} else if (is_dash_arg_prefix(argv[i], "version", 3)) {
			printf( "%s\n%s\n", CondorVersion(), CondorPlatform() );
			my_exit(0);
		} else {
			fprintf(stderr, "%s is not valid argument\n", argv[i]);
			usage();
		}
#else
		if( match_prefix( argv[i], "-host" ) ) {
			if( argv[i + 1] ) {
				host = strdup( argv[++i] );
			} else {
				usage();
			}
		} else if( match_prefix( argv[i], "-name" ) ) {
			if( argv[i + 1] ) {
				i++;
				name = get_daemon_name( argv[i] );
				if( ! name ) {
					fprintf( stderr, "%s: unknown host %s\n", MyName, 
							 get_host_part(argv[i]) );
					my_exit( 1 );
				}
			} else {
				usage();
			}
		} else if( match_prefix( argv[i], "-address" ) ) {
			if( argv[i + 1] ) {
				i++;
				if( is_valid_sinful(argv[i]) ) {
					addr = strdup( argv[i] );
				} else {
					fprintf( stderr, "%s: invalid address %s\n"
						 "Address must be of the form \"<111.222.333.444:555>\n"
						 "   where 111.222.333.444 is the ip address and 555 is the port\n"
						 "   you wish to connect to (the punctuation is important).\n", 
						 MyName, argv[i] );
					my_exit( 1 );
				}
			} else {
				usage();
			}
		} else if( match_prefix( argv[i], "-pool" ) ) {
			if( argv[i + 1] ) {
				i++;
				pool = argv[i];
			} else {
				usage();
			}
		} else if( match_prefix( argv[i], "-local-name" ) ) {
			if( argv[i + 1] ) {
				i++;
				local_name = argv[i];
			} else {
				usage();
			}
		} else if( match_prefix( argv[i], "-owner" ) ) {
			pt = CONDOR_OWNER;
		} else if( match_prefix( argv[i], "-tilde" ) ) {
			pt = CONDOR_TILDE;
		} else if( match_prefix( argv[i], "-master" ) ) {
			dt = DT_MASTER;
			ask_a_daemon = true;
		} else if( match_prefix( argv[i], "-schedd" ) ) {
			dt = DT_SCHEDD;
		} else if( match_prefix( argv[i], "-startd" ) ) {
			dt = DT_STARTD;
		} else if( match_prefix( argv[i], "-collector" ) ) {
			dt = DT_COLLECTOR;
		} else if( match_prefix( argv[i], "-negotiator" ) ) {
			dt = DT_NEGOTIATOR;
		} else if( match_prefix( argv[i], "-set" ) ) {
			mt = CONDOR_SET;
		} else if( match_prefix( argv[i], "-unset" ) ) {
			mt = CONDOR_UNSET;
		} else if( match_prefix( argv[i], "-rset" ) ) {
			mt = CONDOR_RUNTIME_SET;
		} else if( match_prefix( argv[i], "-runset" ) ) {
			mt = CONDOR_RUNTIME_UNSET;
		} else if( match_prefix( argv[i], "-mixedcase" ) ) {
			mixedcase = true;
		} else if( match_prefix( argv[i], "-config" ) ) {
			print_config_sources = true;
		} else if( match_prefix( argv[i], "-verbose" ) ) {
			verbose = true;
		} else if( match_prefix( argv[i], "-dump" ) ) {
			dump_all_variables = true;
		} else if( match_prefix( argv[i], "-expand" ) ) {
			expand_dumped_variables = true;
		} else if( match_prefix( argv[i], "-evaluate" ) ) {
			evaluate_daemon_vars = true;
		} else if( match_prefix( argv[i], "-writeconfig" ) ) {
			write_config = true;
		} else if( match_prefix( argv[i], "-debug" ) ) {
				// dprintf to console
			debug = true;
		} else if( match_prefix( argv[i], "-" ) ) {
			usage();
		} else {
			MyString str;
			str = argv[i];
			// remove any case sensitivity, this is done mostly so output
			// later can look nice. The param() subsystem inherently assumes
			// case insensitivity, so this is perfectly fine to do here.
			str.upper_case();
			params.append( strdup( str.Value() ) ) ;
		}
#endif
	}

	// Set subsystem to tool, and subsystem name to either "TOOL" or what was 
	// specified on the command line.
	set_mySubSystem(subsys, SUBSYSTEM_TYPE_TOOL);

		// Honor any local name we might want to use for the param system
		// while looking up variables.
	if( local_name != NULL ) {
		get_mySubSystem()->setLocalName( local_name );
	}

		// We need to do this before we call config().  A) we don't
		// need config() to find this out and B) config() will fail if
		// all the config sources aren't setup yet, and we use
		// "condor_config_val -owner" for condor_init.  Derek 9/23/99 
	if( pt == CONDOR_OWNER ) {
		printf( "%s\n", get_condor_username() );
		my_exit( 0 );
	}

		// We need to handle -tilde before we call config() for the
		// same reasons listed above for -owner. -Derek 2/16/00
	if( pt == CONDOR_TILDE ) {
		if( (tmp = get_tilde()) ) {
			printf( "%s\n", tmp );
			my_exit( 0 );
		} else {
			fprintf( stderr, "Error: Specified -tilde but can't find " 
					 "condor's home directory\n" );
			my_exit( 1 );
		}
	}		

		// Want to do this before we try to find the address of a
		// remote daemon, since if there's no -pool option, we need to
		// param() for the COLLECTOR_HOST to contact.
	int config_options = CONFIG_OPT_WANT_META;
	if (write_config || stats_with_defaults) {
		config_options |= CONFIG_OPT_KEEP_DEFAULTS;
	}
	config_host(host, 0, config_options);
	validate_config(false); // validate, but do not abort.
	if (print_config_sources) {
		PrintConfigSources();
	}

	if (reconfig_source) {
		if (dump_stats) {
			do_dump_config_stats(stdout, verbose, dump_strings);
		}

		extern const char * simulated_local_config;
		simulated_local_config = reconfig_source;
		config_host(host, 0, config_options);
		if (print_config_sources) {
			fprintf(stdout, "Reconfig with %s appended\n", reconfig_source);
			PrintConfigSources();
		}
	}


	if (dash_debug) {
		dprintf_set_tool_debug("TOOL", 0);
		if (debug_flags) set_debug_flags(debug_flags, 0);
	}

	// temporary, to get rid of build warning.
	if (dash_default) { fprintf(stderr, "-default not (yet) supported\n"); }

	if (write_config) {
		if (ask_a_daemon) {
			fprintf(stdout, "-writeconfig is not currently supported with -%s\n", daemonString(dt));
			my_exit(1);
		}
		int ret = do_write_config(write_config, write_config_flags);
		if (ret < 0) {
			my_exit(2);
		}
	}
	
	if( pool && ! name ) {
		fprintf( stderr, "Error: you must specify -name with -pool\n" );
		my_exit( 1 );
	}

	if( name || addr || mt != CONDOR_QUERY || dt != DT_MASTER ) {
		ask_a_daemon = true;
	}

	if (dump_all_variables && ! ask_a_daemon) {
		/* XXX -dump only currently spits out variables found through the
			configuration file(s) available to this tool. Environment overloads
			would be reflected. */

		char *source = NULL;
		char * hostname = param("FULL_HOSTNAME");
		if (hostname == NULL) {
			hostname = strdup("<unknown hostname>");
		}

		fprintf(stdout, "# Configuration from machine: %s\n", hostname);

		// if no param qualifiers were sent, print all.
		params.rewind();
		if( ! params.number()) {
			if (dash_dump_both) {
				std::string last;
				int opts = 0;
				opts = HASHITER_SHOW_DUPS;
				dump_both_verbose = verbose;
				dump_both_used_only = show_by_usage && ! show_by_usage_unused;
				dump_both_count = 0;
				foreach_param(opts, dump_both_callback, (void*)&last);
				fprintf(stdout, "%d items\n", dump_both_count);
			} else {
				params.append(strdup(""));
				params.rewind();
			}
		}

		if (params.number()) {
			while ((tmp = params.next()) != NULL) {
				if (tmp && tmp[0]) { fprintf(stdout, "\n# Parameters with names that match %s:\n", tmp); }
				std::vector<std::string> names;
				Regex re; int err = 0; const char * pszMsg = 0;
				if (re.compile(tmp, &pszMsg, &err, PCRE_CASELESS)) {
					if (show_by_usage) {
						// condor_config_val doesn't normally have any valid use counts
						// so to get some, we expand everthing in the default param table.
						foreach_param(0, add_ref_callback, NULL);
					}
					if (param_names_matching(re, names)) {
						for (int ii = 0; ii < (int)names.size(); ++ii) {

							const char * name = names[ii].c_str();
							MyString name_used, location;
							//int line_number, use_count, ref_count;
							const MACRO_META * pmet = NULL;
							const char * def_val = NULL;
							const char * rawval = param_get_info(name, subsys, local_name,
															name_used, &def_val, &pmet);
							if (name_used.empty())
								continue;
							if (pmet && show_by_usage) {
								if (show_by_usage_unused && pmet->use_count+pmet->ref_count > 0)
									continue;
								if ( ! show_by_usage_unused && pmet->use_count+pmet->ref_count <= 0)
									continue;
							}
							if (expand_dumped_variables) {
								MyString upname = name; //upname.upper_case();
								const char * val = param(name);
								fprintf(stdout, "%s = %s\n", upname.Value(), val ? val : "");
							} else {
								MyString upname = name_used;
								if (upname.IsEmpty()) upname = name;
								//upname.upper_case();
								fprintf(stdout, "%s = %s\n", upname.Value(), rawval ? rawval : "");
							}
							if (verbose) {
								param_get_location(pmet, location);
								fprintf(stdout, " # at: %s\n", location.c_str());
								if (expand_dumped_variables) {
									fprintf(stdout, " # raw: %s\n", rawval ? rawval : "");
								} else {
									const char * val = param(name);
									fprintf(stdout, " # expanded: %s\n", val ? val : "");
								}
								if (def_val) { fprintf(stdout, " # default: %s\n", def_val); }
								if (dash_usage && pmet) {
									if (pmet->ref_count) fprintf(stdout, " # use_count: %d / %d\n", pmet->use_count, pmet->ref_count);
									else fprintf(stdout, " # use_count: %d\n", pmet->use_count);
								}
							}
						}
					}
				}
			}
		}

		// if we didn't already print out source locations for each item
		// print the set of all config sources.
		if ( ! verbose) {
			fprintf(stdout, "# Contributing configuration file(s):\n");
			if (global_config_source.Length() > 0) {
				fprintf(stdout, "#\t%s\n", global_config_source.Value());
			}
			local_config_sources.rewind();
			while ((source = local_config_sources.next()) != NULL) {
				fprintf( stdout, "#\t%s\n", source);
			}
		}

		if (dump_stats) {
			do_dump_config_stats(stdout, verbose, dump_strings);
		}
		fflush( stdout );

		if (hostname != NULL) {
			free(hostname);
			hostname = NULL;
		}
		my_exit( 0 );

	} else if (dump_stats && ! ask_a_daemon) {
		do_dump_config_stats(stdout, verbose, dump_strings);
		fflush(stdout);
		my_exit(0);
	}

	params.rewind();
	if( ! params.number() && !print_config_sources) {
		if (dump_all_variables || dump_stats) {
			params.append(strdup(""));
			params.rewind();
			//if (diagnostic) fprintf(stderr, "querying all\n");
		} else if (write_config) {
			my_exit(0);
		} else {
			usage();
		}
	}

	Daemon* target = NULL;
	if( ask_a_daemon ) {
		if( addr ) {
			target = new Daemon( dt, addr, NULL );
		} else {
			char* collector_addr = NULL;
			if( pool ) {
				collector_addr = strdup(pool);
			} else { 
				CollectorList * collectors = CollectorList::create();
				Daemon * collector = NULL;
				ReliSock sock;
				while (collectors->next (collector)) {
					if (collector->locate() &&
					    sock.connect((char*) collector->addr(), 0)) {
						// Do something with the connection, 
						// such that we won't end up with 
						// noise in the collector log
						collector->startCommand( DC_NOP, &sock, 30 );
						sock.encode();
						sock.end_of_message();
						// If we can connect to the
						// collector, then we accept
						// it as valid
						break;
					}
				}
				if( (!collector) || (!collector->addr()) ) {
					fprintf( stderr, 
							 "%s, Unable to locate a collector\n", 
							 MyName);
					my_exit( 1 );
				}
				collector_addr = strdup(collector->addr());
				delete collectors;
			}
			target = new Daemon( dt, name, collector_addr );
			free( collector_addr );
		}
		if( ! target->locate() ) {
			fprintf( stderr, "Can't find address for this %s\n", 
					 daemonString(dt) );
			fprintf( stderr, "Perhaps you need to query another pool.\n" );
			my_exit( 1 );
		}
	}

    if (target && evaluate_daemon_vars && !target->daemonAd()) {
        // NULL is one possible return value from this method, for example startd
        fprintf(stderr, "Warning: Classad for %s daemon '%s' is null, will evaluate expressions against empty classad\n", 
                daemonString(target->type()), target->name());
    }

	if (target && dump_all_variables) {
		fprintf(stdout, "# Configuration from %s on %s %s\n", daemonString(dt), target->name(), target->addr());
	}

	while( (tmp = params.next()) ) {
		if( mt == CONDOR_SET || mt == CONDOR_RUNTIME_SET ||
			mt == CONDOR_UNSET || mt == CONDOR_RUNTIME_UNSET ) {
			SetRemoteParam( target, tmp, mt );
		} else {
			MyString name_used, raw_value, file_and_line, def_value, usage_report;
			bool raw_supported = false;
			//fprintf(stderr, "param = %s\n", tmp);
			if (tmp[0] == '$') {
				if (target) {
					fprintf(stderr, "remote query not supported for %s\n", tmp);
					my_exit(1);
				}
				PrintMetaParam(tmp);
				continue;
			}
			if (target) {
				if (dump_stats) {
					ClassAd stats;
					int iret = GetRemoteParamStats(target, stats);
					if (iret == -2) {
						fprintf(stderr, "# remote HTCondor version does not support -stats\n");
						my_exit(1);
					}
					fPrintAd (stdout, stats);
					fprintf (stdout, "\n");
					if ( ! dump_all_variables) continue;
				}
				//fprintf(stderr, "dump = %d\n", dump_all_variables);
				if (dump_all_variables) {
					if (tmp && tmp[0]) { fprintf(stdout, "\n# Parameters with names that match %s:\n", tmp); }
					value = NULL;
					std::vector<std::string> names;
					int iret = GetRemoteParamNamesMatching(target, tmp, names);
					if (iret == -2) {
						fprintf(stderr, "# remote HTCondor version does not support -dump\n");
						my_exit(1);
					}

					if (iret > 0) {
						std::map<std::string, int> sources;
						for (int ii = 0; ii < (int)names.size(); ++ii) {
							if (value) free(value);
							value = GetRemoteParamRaw(target, names[ii].c_str(), raw_supported, raw_value, file_and_line, def_value, usage_report);
							if (show_by_usage && ! usage_report.IsEmpty()) {
								if (show_by_usage_unused && usage_report != "0")
									continue;
								if ( ! show_by_usage_unused && usage_report == "0")
									continue;
							}
							if ( ! value && ! raw_supported)
								continue;

							name_used = names[ii];
							if (expand_dumped_variables || ! raw_supported) {
								printf("%s = %s\n", name_used.Value(), value ? value : "");
							} else {
								printf("%s = %s\n", name_used.Value(), RemoteRawValuePart(raw_value));
							}
							if ( ! raw_supported && (verbose || ! expand_dumped_variables)) {
								printf(" # remote HTCondor version does not support -verbose\n");
							}
							if (verbose) {
								if ( ! file_and_line.IsEmpty()) {
									printf(" # at: %s\n", file_and_line.Value());
								}
								if (expand_dumped_variables) {
									printf(" # raw: %s\n", raw_value.Value());
								} else {
									printf(" # expanded: %s\n", value);
								}
								if ( ! def_value.IsEmpty()) {
									printf(" # default: %s\n", def_value.Value());
								}
								if (dash_usage && ! usage_report.IsEmpty()) {
									printf(" # use_count: %s\n", usage_report.Value());
								}
							} else if ( ! file_and_line.empty()) {
								std::string source(file_and_line.c_str());
								size_t ix = source.find(", line");
								if (ix != std::string::npos) { 
									sources[source.substr(0,ix)] = 1; 
								} else {
									sources[source] = 1;
								}
							}
						}
						if (value) free(value);
						value = NULL;

						if ( ! verbose && ! sources.empty()) {
							fprintf(stdout, "# Contributing configuration file(s):\n");
							std::map<std::string, int>::iterator it;
							for (it = sources.begin(); it != sources.end(); it++) {
								fprintf(stdout, "#\t%s\n", it->first.c_str());
							}
						}
					}
					continue;
				} else if (dash_raw || verbose) {
					name_used = tmp;
					name_used.upper_case();
					value = GetRemoteParamRaw(target, tmp, raw_supported, raw_value, file_and_line, def_value, usage_report);
					if ( ! verbose && ! raw_value.IsEmpty()) {
						free(value);
						value = strdup(RemoteRawValuePart(raw_value));
					}
				} else {
					name_used = tmp;
					name_used.upper_case();
					value = GetRemoteParam(target, tmp);
				}
                if (value && evaluate_daemon_vars) {
                    char* ev = EvaluatedValue(value, target->daemonAd());
                    if (ev != NULL) {
                        free(value);
                        value = ev;
                    } else {
                        fprintf(stderr, "Warning: Failed to evaluate '%s', returning it as config value\n", value);
                    }
                }
			} else {
				const char * def_val;
				const MACRO_META * pmet = NULL;
				const char * val = param_get_info(tmp, subsys, local_name,
										name_used, &def_val, &pmet);
										//use_count, ref_count,
										//file_and_line, line_number);
				if ( ! name_used.empty()) {
					raw_supported = true;
					if (dash_raw) {
						value = strdup(val ? val : "");
					} else {
						value = param(name_used.c_str());
						raw_value = name_used;
						//raw_value.upper_case();
						raw_value += " = ";
						raw_value += val;
					}
					param_get_location(pmet, file_and_line);
					if (pmet->ref_count) {
						usage_report.formatstr("%d / %d", pmet->use_count, pmet->ref_count);
					} else {
						usage_report.formatstr("%d", pmet->use_count);
					}
				} else {
					name_used = tmp;
					name_used.upper_case();
					value = NULL;
				}
			}
			if( value == NULL ) {
				fprintf(stderr, "Not defined: %s\n", name_used.c_str());
				my_exit( 1 );
			} else {
				if (verbose) {
					printf("%s = %s\n", name_used.c_str(), value);
				} else {
					printf("%s\n", value);
				}
				free( value );
				if ( ! raw_supported && (dash_raw || verbose)) {
					printf(" # the remote HTCondor version does not support -raw or -verbose");
				}
				if (verbose) {
#if 1
					if ( ! file_and_line.IsEmpty()) {
						printf(" # at: %s\n", file_and_line.Value());
					}
					if ( ! raw_value.IsEmpty()) {
						printf(" # raw: %s\n", raw_value.Value());
					}
					if ( ! def_value.IsEmpty()) {
						printf(" # default: %s\n", def_value.Value());
					}
					if (dump_both_only_type > 0) {
						print_as_type(tmp, dump_both_only_type);
					}
					if (dash_usage && ! usage_report.IsEmpty()) {
						printf(" # use_count: %s\n", usage_report.Value());
					}
					printf("\n");
#else  // this is the old format, do we need to match this exactly?
					if (line_number == -1) {
						printf("  Defined in '%s'.\n\n", filename.Value());
					} else {
						printf("  Defined in '%s', line %d.\n\n",
							   filename.Value(), line_number);
					}
#endif
				}
			}
		}
	}
	my_exit( 0 );
	return 0;
}


char*
GetRemoteParam( Daemon* target, char* param_name ) 
{
    ReliSock s;
	s.timeout( 30 );
	char	*val = NULL;

		// note: printing these things out in each dprintf() is
		// stupid.  we should be using Daemon::idStr().  however,
		// since this code didn't used to use a Daemon object at all,
		// and since it's so hard for us to change the output of our
		// tools for fear that someone's ASCII parser will break, i'm
		// just cheating and being lazy here by replicating the old
		// behavior...
	char* addr;
	const char* name;
	bool connect_error = true;
	do {
		addr = target->addr();
		name = target->name();
		if( !name ) {
			name = "";
		}

		if( s.connect( addr, 0 ) ) {
			connect_error = false;
			break;
		}
	} while( target->nextValidCm() == true );

	if( connect_error == true ) {
		fprintf( stderr, "Can't connect to %s on %s %s\n", 
				 daemonString(dt), name, addr );
		my_exit(1);
	}

	target->startCommand( CONFIG_VAL, &s, 30 );

	s.encode();
	if( !s.code(param_name) ) {
		fprintf( stderr, "Can't send request (%s)\n", param_name );
		return NULL;
	}
	if( !s.end_of_message() ) {
		fprintf( stderr, "Can't send end of message\n" );
		return NULL;
	}

	s.decode();
	if( !s.code(val) ) {
		fprintf( stderr, "Can't receive reply from %s on %s %s\n",
				 daemonString(dt), name, addr );
		return NULL;
	}
	if( !s.end_of_message() ) {
		fprintf( stderr, "Can't receive end of message\n" );
		return NULL;
	}

	return val;
}

// returns NULL or allocated char *
char*
GetRemoteParamRaw(
	Daemon* target,
	const char * param_name,
	bool & raw_supported,
	MyString & raw_value,
	MyString & file_and_line,
	MyString & def_value,
	MyString & usage_report)
{
	ReliSock s;
	s.timeout(30);
	char *val = NULL;

	raw_supported = false;
	raw_value = "";
	file_and_line = "";
	def_value = "";
	usage_report = "";

	char* addr = NULL;
	const char* name = NULL;
	bool connect_error = true;
	do {
		addr = target->addr();
		name = target->name();
		if (s.connect(addr, 0)) {
			connect_error = false;
			break;
		}
	} while(target->nextValidCm());

	if (connect_error) {
		fprintf (stderr, "Can't connect to %s on %s %s\n", daemonString(dt), name ? name : "", addr);
		my_exit(1);
	}

	target->startCommand(DC_CONFIG_VAL, &s, 30);
	s.encode();

	if (diagnostic) fprintf(stderr, "sending %s\n", param_name);
	if ( ! s.code(const_cast<char*&>(param_name))) {
		fprintf(stderr, "Can't send request (%s)\n", param_name);
		return NULL;
	}
	if ( ! s.end_of_message()) {
		fprintf (stderr, "Can't send end of message\n");
		return NULL;
	}

	s.decode();
	if ( ! s.code(val)) {
		fprintf(stderr, "Can't receive reply from %s on %s %s\n", daemonString(dt), name ? name : "", addr );
		return NULL;
	}
	if (diagnostic) fprintf(stderr, "result: '%s'\n", val);

	// daemons from 8.1.2 and later will return more than one string from this query.
	// after the param value, we expect to see
	//    <used_name> = <raw_value>\0
	//    <file>, line <num>\0
	//
	int ix = 0;
	while ( ! s.peek_end_of_message()) {
		MyString dummy;
		MyString * ps = &dummy;
		if (0 == ix) ps = &raw_value;
		else if (1 == ix) ps = &file_and_line;
		else if (2 == ix) ps = &def_value;
		else if (3 == ix) ps = &usage_report;

		if ( ! s.code(*ps)) { 
			fprintf(stderr, "Can't read line %d\n", ix); 
			break; 
		}
		if (diagnostic) fprintf(stderr, "result[%d]: '%s'\n", ix, ps->Value());
		++ix;
		// if we got more than one string in the reply, then the remote daemon supports -raw
		raw_supported = true;
	}
	if ( ! s.end_of_message()) {
		fprintf( stderr, "Can't receive end of message\n" );
		if (val) free(val);
		return NULL;
	}

	return val;
}

int GetRemoteParamStats(Daemon* target, ClassAd & ad)
{
	ad.Clear();

	ReliSock s;
	s.timeout(30);

	char* addr = NULL;
	const char* name = NULL;
	bool connect_error = true;
	do {
		addr = target->addr();
		name = target->name();
		if (s.connect(addr, 0)) {
			connect_error = false;
			break;
		}
	} while(target->nextValidCm());

	if (connect_error) {
		fprintf (stderr, "Can't connect to %s on %s %s\n", daemonString(dt), name ? name : "", addr);
		my_exit(1);
	}

	target->startCommand(DC_CONFIG_VAL, &s, 30);
	s.encode();

	// if we do DC_CONFIG_VAL query of "?stats" we will either get
	// back a set of strings containing config stats, or "Not defined" if the daemon
	// is pre 8.1.2
	//
	MyString query("?stats");
	if (diagnostic) fprintf(stderr, "sending %s\n", query.Value());
	if ( ! s.code(query)) {
		fprintf(stderr, "Can't send request for stats'\n");
		return 0;
	}
	if ( ! s.end_of_message()) {
		fprintf( stderr, "Can't send end of message\n" );
		return 0;
	}

	s.decode();
	std::string val;
	if ( ! s.code(val)) {
		fprintf(stderr, "Can't receive reply from %s on %s %s\n", daemonString(dt), name ? name : "", addr );
		return 0;
	}

	// daemons from 8.1.2 and later will return a set of strings
	// containing statistics or
	// "!error:type:num: message"
	// daemons prior to 8.1.2 will return "Not Defined"
	//
	if (diagnostic) fprintf(stderr, "result: '%s'\n", val.c_str());
	if (val == "Not defined") {
		if ( ! s.end_of_message()) {
			fprintf(stderr, "Can't receive end of message\n");
		}
		return -2; // -2 for not supported.
	}

	// param names can't start with !, so if the first character is !
	// then the reply is an error message of the form "!error:type:num: message"
	if (val[0] == '!') {
		fprintf(stderr, "%s\n", val.substr(1).c_str());
		if ( ! s.end_of_message()) {
			fprintf(stderr, "Can't receive end of message\n");
		}
		return -1;
	}

	if ( ! s.peek_end_of_message()) {
		if ( ! getClassAd(&s, ad)) {
			fprintf(stderr, "Can't read stats ad from %s\n", name);
		}
	}
	ad.Remove("CurrentTime");
	return 0;
}


int
GetRemoteParamNamesMatching(Daemon* target, const char * param_pat, std::vector<std::string> & names)
{
	ReliSock s;
	s.timeout(30);

	char* addr = NULL;
	const char* name = NULL;
	bool connect_error = true;
	do {
		addr = target->addr();
		name = target->name();
		if (s.connect(addr, 0)) {
			connect_error = false;
			break;
		}
	} while(target->nextValidCm());

	if (connect_error) {
		fprintf (stderr, "Can't connect to %s on %s %s\n", daemonString(dt), name ? name : "", addr);
		my_exit(1);
	}

	target->startCommand(DC_CONFIG_VAL, &s, 30);
	s.encode();

	// if we do DC_CONFIG_VAL query of the param name "?names" we will either get
	// back a set of strings containing parameter names, or "Not defined" if the daemon
	// is pre 8.1.2
	//
	MyString query("?names");
	//if (param_pat && param_pat[0] == '*' && ! param_pat[1]) {
	//	; // ?names without a qualifier implies a ".*" pattern
	//} else
	if (param_pat && param_pat[0]) {
		query += ":";
		query += param_pat;
	}

	if (diagnostic) fprintf(stderr, "sending %s\n", query.Value());
	if ( ! s.code(query)) {
		fprintf(stderr, "Can't send request for names matching %s'\n", param_pat);
		return 0;
	}
	if ( ! s.end_of_message()) {
		fprintf( stderr, "Can't send end of message\n" );
		return 0;
	}

	s.decode();
	std::string val;
	if ( ! s.code(val)) {
		fprintf(stderr, "Can't receive reply from %s on %s %s\n", daemonString(dt), name ? name : "", addr );
		return 0;
	}

	// daemons from 8.1.2 and later will return a set of strings
	// containing parameter names or a single string of the form
	// "!error:type:num: message"
	// daemons prior to 8.1.2 will return "Not Defined"
	//
	if (diagnostic) fprintf(stderr, "result: '%s'\n", val.c_str());
	if (val == "Not defined") {
		if ( ! s.end_of_message()) {
			fprintf(stderr, "Can't receive end of message\n");
		}
		return -2; // -2 for not supported.
	}

	// param names can't start with !, so if the first character is !
	// then the reply is an error message of the form "!error:type:num: message"
	if (val[0] == '!') {
		fprintf(stderr, "%s\n", val.substr(1).c_str());
		if ( ! s.end_of_message()) {
			fprintf(stderr, "Can't receive end of message\n");
		}
		return -1;
	}

	// an empty string means that nothing matched the query.
	if ( ! val.empty()) {
		names.push_back(val);
	}

	while ( ! s.peek_end_of_message()) {
		if ( ! s.code(val)) { 
			fprintf(stderr, "Can't read name\n"); 
			break; 
		}
		if (diagnostic) fprintf(stderr, "result: '%s'\n", val.c_str());
		names.push_back(val);
	}
	if ( ! s.end_of_message()) {
		fprintf( stderr, "Can't receive end of message\n" );
		return 0;
	}

	if (names.size() > 1) {
		std::sort(names.begin(), names.end(), sort_ascending_ignore_case);
	}
	return names.size();
}

void
SetRemoteParam( Daemon* target, char* param_value, ModeType mt )
{
	int cmd = DC_NOP, rval;
	ReliSock s;
	bool set = false;

		// note: printing these things out in each dprintf() is
		// stupid.  we should be using Daemon::idStr().  however,
		// since this code didn't used to use a Daemon object at all,
		// and since it's so hard for us to change the output of our
		// tools for fear that someone's ASCII parser will break, i'm
		// just cheating and being lazy here by replicating the old
		// behavior...
	char* addr;
	const char* name;
	bool connect_error = true;

		// We need to know two things: what command to send, and (for
		// error messages) if we're setting or unsetting.  Since our
		// bool "set" defaults to false, we only hit the "set = true"
		// statements when we want to, and fall through to also set
		// the right commands in those cases... Derek Wright 9/1/99
	switch (mt) {
	case CONDOR_SET:
		set = true;
	case CONDOR_UNSET:
		cmd = DC_CONFIG_PERSIST;
		break;
	case CONDOR_RUNTIME_SET:
		set = true;
	case CONDOR_RUNTIME_UNSET:
		cmd = DC_CONFIG_RUNTIME;
		break;
	default:
		fprintf( stderr, "Unknown command type %d\n", (int)mt );
		my_exit( 1 );
	}

		// Now, process our strings for sanity.
	char* param_name = strdup( param_value );
	char* tmp = NULL;

	if( set ) {
		tmp = strchr( param_name, ':' );
		if( ! tmp ) {
			tmp = strchr( param_name, '=' );
		}
		if( ! tmp ) {
			fprintf( stderr, "%s: Can't set configuration value (\"%s\")\n" 
					 "You must specify \"macro_name = value\" or " 
					 "\"expr_name : value\"\n", MyName, param_name );
			my_exit( 1 );
		}
			// If we're still here, we found a ':' or a '=', so, now,
			// chop off everything except the attribute name
			// (including spaces), so we can send that seperately. 
		do {
			*tmp = '\0';
			tmp--;
		} while( *tmp == ' ' );
	} else {
			// Want to do different sanity checking.
		if( (tmp = strchr(param_name, ':')) || 
			(tmp = strchr(param_name, '=')) ) {
			fprintf( stderr, "%s: Can't unset configuration value (\"%s\")\n" 
					 "To unset, you only specify the name of the attribute\n", 
					 MyName, param_name );
			my_exit( 1 );
		}
		tmp = strchr( param_name, ' ' );
		if( tmp ) {
			*tmp = '\0';
		}
	}

		// At this point, in either set or unset mode, param_name
		// should hold a valid name, so do a final check to make sure
		// there are no spaces.
	if( !is_valid_param_name(param_name) ) {
		fprintf( stderr, 
				 "%s: Error: Configuration variable name (%s) is not valid, alphanumeric and _ only\n",
				 MyName, param_name );
		my_exit( 1 );
	}

	if (!mixedcase) {
		strlwr(param_name);		// make the config name case insensitive
	}

		// We need a version with a newline at the end to make
		// everything cool at the other end.
	char* buf = (char*)malloc( strlen(param_value) + 2 );
	ASSERT( buf != NULL );
	sprintf( buf, "%s\n", param_value );

	s.timeout( 30 );
	do {
		addr = target->addr();
		name = target->name();
		if( !name ) {
			name = "";
		}

		if( s.connect( addr, 0 ) ) {
			connect_error = false;
			break;
		}
	} while( target->nextValidCm() == true );

	if( connect_error == true ) {
		fprintf( stderr, "Can't connect to %s on %s %s\n", 
				 daemonString(dt), name, addr );
		my_exit(1);
	}

	target->startCommand( cmd, &s );

	s.encode();
	if( !s.code(param_name) ) {
		fprintf( stderr, "Can't send config name (%s)\n", param_name );
		my_exit(1);
	}
	if( set ) {
		if( !s.code(param_value) ) {
			fprintf( stderr, "Can't send config setting (%s)\n", param_value );
			my_exit(1);
		}
	} else {
		if( !s.put("") ) {
			fprintf( stderr, "Can't send config setting\n" );
			my_exit(1);
		}
	}
	if( !s.end_of_message() ) {
		fprintf( stderr, "Can't send end of message\n" );
		my_exit(1);
	}

	s.decode();
	if( !s.code(rval) ) {
		fprintf( stderr, "Can't receive reply from %s on %s %s\n",
				 daemonString(dt), name, addr );
		my_exit(1);
	}
	if( !s.end_of_message() ) {
		fprintf( stderr, "Can't receive end of message\n" );
		my_exit(1);
	}
	if (rval < 0) {
		if (set) {
			fprintf( stderr, "Attempt to set configuration \"%s\" on %s %s "
					 "%s failed.\n",
					 param_value, daemonString(dt), name, addr );
		} else {
			fprintf( stderr, "Attempt to unset configuration \"%s\" on %s %s "
					 "%s failed.\n",
					 param_value, daemonString(dt), name, addr );
		}
		my_exit(1);
	}
	if (set) {
		fprintf( stdout, "Successfully set configuration \"%s\" on %s %s "
				 "%s.\n",
				 param_value, daemonString(dt), name, addr );
	} else {
		fprintf( stdout, "Successfully unset configuration \"%s\" on %s %s "
				 "%s.\n",
				 param_value, daemonString(dt), name, addr );
	}

	free( buf );
	free( param_name );
}

static void PrintConfigSources(void)
{
		// print descriptive lines to stderr and config source names to
		// stdout, so that the output can be cleanly piped into
		// something like xargs...

	if (global_config_source.Length() > 0) {
		fprintf( stderr, "Configuration source:\n" );
		fflush( stderr );
		fprintf( stdout, "\t%s\n", global_config_source.Value() );
		fflush( stdout );
	} else {
		fprintf( stderr, "Can't find the configuration source.\n" );
	}

	unsigned int numSources = local_config_sources.number();
	if (numSources > 0) {
		if (numSources == 1) {
			fprintf( stderr, "Local configuration source:\n" );
		} else {
			fprintf( stderr, "Local configuration sources:\n" );
		}
		fflush( stderr );

		char *source;
		local_config_sources.rewind();
		while ( (source = local_config_sources.next()) != NULL ) {
			fprintf( stdout, "\t%s\n", source );
			fflush( stdout );
		}

		fprintf(stderr, "\n");
	}

	return;
}

static void do_dump_config_stats(FILE * fh, bool dump_sources, bool dump_strings)
{
	struct _macro_stats stats;
	get_config_stats(&stats);
	fprintf(fh, "Macros = %d\n", stats.cEntries);
	fprintf(fh, "Used = %d\n", stats.cUsed);
	fprintf(fh, "Referenced = %d\n", stats.cReferenced);
	fprintf(fh, "Files = %d\n", stats.cFiles);
	fprintf(fh, "StringBytes = %d\n", stats.cbStrings);
	fprintf(fh, "TablesBytes = %d\n", stats.cbTables);
	fprintf(fh, "IsSorted = %d\n", stats.cSorted);

	if (dump_sources) {
		fprintf(fh, "\nSources -----\n");
		config_dump_sources(fh, "\n");
	}

	if (dump_strings) {
		fprintf(fh, "\nStrings -----\n");
		config_dump_string_pool(fh, "\n");
		fprintf(fh, "\n");
	}
}

// encode file, line, & sub as sort key
typedef union {
	long long all; // 64 bits
	struct {
		unsigned long long sub   : 24;
		unsigned long long line  : 24;
		unsigned long long file  : 16;
	};
} source_sort_key;

struct _write_config_args {
	FILE * fh;
	WRITE_CONFIG_OPTIONS opt;
	int iter;
	const char * pszLast;
	StringList *obsolete;
	StringList *obsoleteif;
	std::map<unsigned long long, std::string> output;
};

// iterator callback that writes a single entry
bool write_config_callback(void* user, HASHITER & it) {
	struct _write_config_args * pargs = (struct _write_config_args *)user;
	FILE * fh = pargs->fh;
	bool show_file = !pargs->opt.show_only || !(pargs->opt.all & 0xFE);

	const char * comment_me = "";
	MACRO_META * pmeta = hash_iter_meta(it);
	if (pmeta->matches_default && ! (pmeta->inside)) {
		if ( ! pargs->opt.show_def_match)
			return true; // keep scanning
		if (pargs->opt.comment_def_match)
			comment_me = "#";
	} else if (pmeta->source_id == DetectedMacro.id) {
		if ( ! pargs->opt.show_detected && ! pargs->opt.show_def)
			return true;
		if (pargs->opt.comment_detected)
			comment_me = "#detected#";
		else if (pargs->opt.comment_def)
			comment_me = "#def#";
	} else if (pmeta->inside) {
		if ( ! pargs->opt.show_def)
			return true;
		if (pargs->opt.comment_def)
			comment_me = "#def#";
	} else if (pmeta->source_id == EnvMacro.id) {
		if (pargs->opt.show_only && ! pargs->opt.show_env)
			return true;
		if (pargs->opt.comment_env)
			comment_me = "#env#";
	} else if (pmeta->source_id > WireMacro.id) {
		// source is a file.
		if ( ! show_file) return true;

		// for complicated reasons, params explicitly set to <undef> in the
		// config file don't have matches_default set even when the same param
		// is in the defaults table but with no defined value.
		// but we want to treat these as matches_default anyway.
		if (pmeta->param_id >= 0) {
			const char * val = hash_iter_value(it);
			const char * dval = param_default_rawval_by_id(pmeta->param_id);
			if (( ! val || ! val[0]) && ( ! dval || ! dval[0])) {
				if ( ! pargs->opt.show_def_match)
					return true; // keep scanning
				if (pargs->opt.comment_def_match)
					comment_me = "#";
			}
		}

		if (pargs->opt.hide_obsolete && pargs->obsolete) {
			if (pargs->obsolete->contains_anycase(hash_iter_key(it))) {
				return true;
			}
		}

		if (pargs->opt.hide_if_match && pargs->obsoleteif) {
			MyString item(hash_iter_key(it));
			item.upper_case();
			item += "=";
			const char * val = hash_iter_value(it);
			if (val) item += hash_iter_value(it);
			if (pargs->obsoleteif->contains(item.c_str())) {
				return true;
			}
		}

	} else if (pargs->opt.show_only) {
		// we want to show only the params that matched one of the above conditions
		return true;
	}
	const char * name = hash_iter_key(it);
	const char * rawval = hash_iter_value(it);

	bool is_dup = (pargs->pszLast && (MATCH == strcasecmp(name, pargs->pszLast)));
	if (is_dup && ! pargs->opt.show_dup) {
		return true;
	}

	MyString source;
	if (pargs->opt.comment_source) {
		report_config_source(pmeta, source);
	}
	if (pargs->opt.sort_name) {
		// if sorting the output by name, we can just print it now.
		fprintf(fh, "%s%s = %s\n", comment_me, name, rawval ? rawval : "");
		if ( ! source.empty()) { fprintf(fh, " # at: %s\n", source.c_str()); }
	} else {
		MyString line;
		//the next line makes the difference between NULL and "" visible
		//if (rawval && !rawval[0]) rawval = "\"\"";
		line.formatstr("%s%s = %s", comment_me, name, rawval ? rawval : "");
		if ( ! source.empty()) { line += "\n"; line += source; }
		// generate a unique source sort key so that we end up sorting by
		// source file id, line, & meta-knob line in that order.
		source_sort_key key;
		key.all = 0;
		key.sub  = (pargs->iter++) & ((1<<24)-1);
		key.line = pmeta->source_line & ((1<<24)-1);

		unsigned short int id = (unsigned short int)pmeta->source_id;
		if (id == EnvMacro.id) { id = 0xFFFE; }
		else if (id == WireMacro.id) { id = 0xFFFF; }
		key.file = id;

		// save the output line(s) into the output map, this will implicitly sort them by source
		pargs->output[key.all] = line;
	}

	pargs->pszLast = name;
	return true;
}

void PrintMetaParam(const char * name)
{
	std::string temp(name);
	char * tmp = &temp[0];

	char * use = tmp;
	char * parm = tmp;
	char * pdot = NULL;
	while (*parm) {
		if (*parm == '=' || *parm == ':') {
			*parm++ = 0; // null terminate the name
			break;
		}
		// remember if we saw any dots so we can handle the common typo.
		if (!pdot && *parm == '.') pdot = parm;
		++parm;
	}
	// if the input has a '.' but no ':' or '=', assume that they intended to use :
	if ( ! *parm && pdot) {
		parm = pdot;
		*parm = ':';
		fprintf(stderr, "%s is not valid, assuming %s was intended\n", name, tmp);
		*parm++ = 0;
	}

	MACRO_TABLE_PAIR* ptable = param_meta_table(use+1);
	MACRO_DEF_ITEM * pdef = NULL;
	if (ptable) {
		// if only a metaknob category was passed, print out all of the 
		// knob names in that category.
		if ( ! *parm) {
			if (ptable->cElms > 0) {
				printf("$%s accepts\n", ptable->key);
				for (int ii = 0; ii < ptable->cElms; ++ii) {
					printf("  %s\n", ptable->aTable[ii].key);
				}
			}
			return;
		}
		// lookup the given metaknob name in that category
		pdef = param_meta_table_lookup(ptable, parm);
	}
	if (pdef) {
		printf("$%s:%s is\n%s\n", ptable->key, pdef->key, pdef->def->psz);
	} else {
		MyString name_used(use);
		if (ptable) { name_used.formatstr("%s:%s", use, parm); }
		name_used.upper_case();
		printf("Not defined: %s\n", name_used.c_str());
	}
}

#if 0 // code not yet ready for prime time.

void DumpRemoteParams(Daemon* target, const char * tmp)
{
	std::vector<std::string> names;

	// returns count of matches, negative values are errors
	int iret = GetRemoteParamNamesMatching(target, tmp, names);
	if (iret == -2) {
		fprintf(stderr, "-dump is not supported by the remote version of HTCondor\n");
		my_exit(1);
	}
	if (iret <= 0)
		return;

	MyString name_used, raw_value, file_and_line, def_value, usage_report;
	bool raw_supported = false;

	char * value = NULL;
	for (int ii = 0; ii < (int)names.size(); ++ii) {
		if (value) free(value);
		char * value = GetRemoteParamRaw(target, names[ii].c_str(), raw_supported, raw_value, file_and_line, def_value, usage_report);
		if (show_by_usage && ! usage_report.IsEmpty()) {
			if (show_by_usage_unused && usage_report != "0")
				continue;
			if ( ! show_by_usage_unused && usage_report == "0")
				continue;
		}
		if ( ! value && ! raw_supported)
			continue;

		name_used = names[ii];
		if (expand_dumped_variables || ! raw_supported) {
			printf("%s = %s\n", name_used.Value(), value ? value : "");
		} else {
			printf("%s = %s\n", name_used.Value(), RemoteRawValuePart(raw_value));
		}
		if ( ! raw_supported && (verbose || ! expand_dumped_variables)) {
			printf(" # remote HTCondor version does not support -verbose\n");
		}
		if (verbose) {
			if ( ! file_and_line.IsEmpty()) {
				printf(" # at: %s\n", file_and_line.Value());
			}
			if (expand_dumped_variables) {
				printf(" # raw: %s\n", raw_value.Value());
			} else {
				printf(" # expanded: %s\n", value);
			}
			if ( ! def_value.IsEmpty()) {
				printf(" # default: %s\n", def_value.Value());
			}
			if (dash_usage && ! usage_report.IsEmpty()) {
				printf(" # use_count: %s\n", usage_report.Value());
			}
		}
	}
	if (value) free(value);
}


void PrintParam(const char * tmp)
{
	MyString name_used, raw_value, file_and_line, def_value, usage_report;
	bool raw_supported = false;
	//fprintf(stderr, "param = %s\n", tmp);
	if( target ) {
		if (dump_stats) {
			ClassAd stats;
			int iret = GetRemoteParamStats(target, stats);
			if (iret == -2) {
				fprintf(stderr, "-stats is not supported by the remote version of HTCondor\n");
				my_exit(1);
			}
			fPrintAd (stdout, stats);
			fprintf (stdout, "\n");
			if ( ! dump_all_variables) continue;
		}
		//fprintf(stderr, "dump = %d\n", dump_all_variables);
		if (dump_all_variables) {
			?? DumpRemoteParams();
		} else if (dash_raw || verbose) {
			name_used = tmp;
			name_used.upper_case();
			value = GetRemoteParamRaw(target, tmp, raw_supported, raw_value, file_and_line, def_value, usage_report);
			if ( ! verbose && ! raw_value.IsEmpty()) {
				free(value);
				value = strdup(RemoteRawValuePart(raw_value));
			}
		} else {
			name_used = tmp;
			name_used.upper_case();
			value = GetRemoteParam(target, tmp);
		}
        if (value && evaluate_daemon_vars) {
            char* ev = EvaluatedValue(value, target->daemonAd());
            if (ev != NULL) {
                free(value);
                value = ev;
            } else {
                fprintf(stderr, "Warning: Failed to evaluate '%s', returning it as config value\n", value);
            }
        }
	} else {
		const char * def_val;
		const MACRO_META * pmet = NULL;
		const char * val = param_get_info(tmp, subsys, local_name,
								name_used, &def_val, &pmet);
								//use_count, ref_count,
								//file_and_line, line_number);
		if ( ! name_used.empty()) {
			raw_supported = true;
			if (dash_raw) {
				value = strdup(val ? val : "");
			} else {
				value = param(name_used.c_str());
				raw_value = name_used;
				//raw_value.upper_case();
				raw_value += " = ";
				raw_value += val;
			}
			param_get_location(pmet, file_and_line);
			if (pmet->ref_count) {
				usage_report.formatstr("%d / %d", pmet->use_count, pmet->ref_count);
			} else {
				usage_report.formatstr("%d", pmet->use_count);
			}
		} else {
			name_used = tmp;
			name_used.upper_case();
			value = NULL;
		}
	}
	if( value == NULL ) {
		fprintf(stderr, "Not defined: %s\n", name_used.c_str());
		my_exit( 1 );
	} else {
		if (verbose) {
			printf("%s = %s\n", name_used.c_str(), value);
		} else {
			printf("%s\n", value);
		}
		free( value );
		if ( ! raw_supported && (dash_raw || verbose)) {
			printf(" # the remote HTCondor version does not support -raw or -verbose");
		}
		if (verbose) {
			if ( ! file_and_line.IsEmpty()) {
				printf(" # at: %s\n", file_and_line.Value());
			}
			if ( ! raw_value.IsEmpty()) {
				printf(" # raw: %s\n", raw_value.Value());
			}
			if ( ! def_value.IsEmpty()) {
				printf(" # default: %s\n", def_value.Value());
			}
			if (dump_both_only_type > 0) {
				print_as_type(tmp, dump_both_only_type);
			}
			if (dash_usage && ! usage_report.IsEmpty()) {
				printf(" # use_count: %s\n", usage_report.Value());
			}
			printf("\n");
		}
	}
}
#endif

static int do_write_config(const char* pathname, WRITE_CONFIG_OPTIONS opts)
{
	FILE * fh = NULL;
	bool   close_file = false;
	if (MATCH == strcmp("-", pathname)) {
		fh = stdout;
		close_file = false;
	} else {
		fh = safe_fopen_wrapper_follow(pathname, "w");
		if ( ! fh) {
			fprintf(stderr, "Failed to create configuration file %s.\n", pathname);
			return -1;
		}
		close_file = true;
	}

	struct _write_config_args args;
	args.fh = fh;
	args.opt.all = opts.all;
	args.iter = 0;
	args.obsolete = NULL;
	args.obsoleteif = NULL;
	args.pszLast = NULL;

	StringList obsolete("","\n");
	if (opts.hide_obsolete) {
		const char * items = param_meta_table_string(param_meta_table("UPGRADE"), "DISCARD");
		if (items && items[0]) {
			//fprintf(stderr, "$UPGRADE.DISCARD=\n%s\n", items);
			obsolete.initializeFromString(items);
			//fprintf(stderr, "obsolete=\n%s\n", obsolete.print_to_delimed_string("\n"));
		}
		items = param_meta_table_string(param_meta_table("UPGRADE"), "DISCARDX");
		if (items && items[0]) {
			//fprintf(stderr, "$UPGRADE.DISCARD_OPSYS=\n%s\n", items);
			obsolete.initializeFromString(items);
		}
		args.obsolete = &obsolete;

		// fprintf(stderr, "obsolete=\n%s\n", obsolete.print_to_delimed_string("\n"));
	}

	StringList obsoleteif("","\n");
	if (opts.hide_if_match) {
		const char * items = param_meta_table_string(param_meta_table("UPGRADE"), "DISCARDIF");
		if (items && items[0]) {
			obsoleteif.initializeFromString(items);
			args.obsoleteif = &obsoleteif;
		}

		#ifdef WIN32
		// on Windows we shove in some defaults for VMWARE, we want to ignore them
		// for upgrade if the user isn't using vmware
		char * vm_type = param("VM_TYPE");
		if (vm_type) { free(vm_type); }
		else
		{
			const char * items = param_meta_table_string(param_meta_table("UPGRADE"), "DISCARDIF_VMWARE");
			if (items && items[0]) {
				obsoleteif.initializeFromString(items);
				args.obsoleteif = &obsoleteif;
			}
		}
		#endif
	}

	int iter_opts = HASHITER_SHOW_DUPS;
	foreach_param(iter_opts, write_config_callback, &args);
	//fprintf(fh, "\n<all done>\n");
	if ( ! args.output.empty()) {
		//fprintf(fh, "<not empty>\n");
		std::map<unsigned long long, std::string>::iterator it;
		long last_id = -1;
		for (it = args.output.begin(); it != args.output.end(); it++) {
			source_sort_key key;
			key.all = it->first;
			long source_id = key.file;
			if (source_id == 0xFFFE) source_id = EnvMacro.id;
			if (source_id == 0xFFFF) source_id = WireMacro.id;
			if (source_id != last_id) {
				const char * source_name = config_source_by_id(source_id);
				if (source_name) fprintf(fh, "\n#\n# from %s\n#\n", source_name);
				last_id = source_id;
			}
			fprintf(fh, "%s\n", it->second.c_str());
		}
	}

	if (close_file && fclose(fh)) {
		fprintf(stderr, "Error closing new configuration file %s.\n", pathname);
		return -1;
	}
	return 0;

}


