/***************************************************************
 *
 * Copyright (C) 1990-2014, Condor Team, Computer Sciences Department,
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
#include "condor_debug.h"
#include "condor_string.h" // for getline_trim
#include "subsystem_info.h"
#include <time.h>
#include "condor_classad.h"
#include "condor_attributes.h"
#include "condor_adtypes.h"
#include "condor_distribution.h"
#include "condor_ver_info.h"
#if !defined(WIN32)
#include <pwd.h>
#include <sys/stat.h>
#endif
#include "sig_install.h"
#include "match_prefix.h"
#include "condor_version.h"
#include "ad_printmask.h"
#include "Regex.h"
#include "tokener.h"
#include <submit_utils.h>
#include <xform_utils.h>

#include <string>
#include <set>

#define USE_XFORM_UTILS 1
#define USE_CLASSAD_ITERATOR 1
#define USE_CLASSAD_LIST_WRITER 1

#define UNSPECIFIED_PARSE_TYPE (ClassAdFileParseType::ParseType)-1

bool dash_verbose = false;
bool dash_terse = false;
int  UnitTestOpts = 0;
int  DashConvertOldRoutes = 0;
bool DumpLocalHash = false;
bool DumpClassAdToFile = false;
bool DumpFileIsStdout = false;
bool DashOutAttrsInHashOrder = false;
ClassAdFileParseType::ParseType DashOutFormat = UNSPECIFIED_PARSE_TYPE;
const char * DashOutName = NULL;
const char * MyName = NULL; // set from argc
const char * MySubsys = "XFORM";
int cNonEmptyOutputAds = 0; // incremented when whenever we print a non-empty ad
static struct _testing_options {
	int repeat_count;
	bool no_output;
	bool enabled;
} testing = {0, false, false};

class CondorQClassAdFileParseHelper;
class MacroStreamXFormSource;

class input_file {
public:
	input_file() : filename(NULL), parse_format(ClassAdFileParseType::Parse_long) {}
	input_file(const char * file, ClassAdFileParseType::ParseType format = ClassAdFileParseType::Parse_long)
		: filename(file), parse_format(format) {}
	const char * filename; // ptr to argv, not owned by this class
	ClassAdFileParseType::ParseType parse_format;
};

class apply_transform_args {
public:
#ifdef USE_XFORM_UTILS
#ifdef USE_CLASSAD_LIST_WRITER
	apply_transform_args(MacroStreamXFormSource& xfm, XFormHash & ms, FILE* out, CondorClassAdListWriter & writ)
		: xforms(xfm), mset(ms), checkpoint(NULL), outfile(out), writer(writ), input_helper(NULL) {}
#else
	apply_transform_args(MacroStreamXFormSource& xfm, XFormHash & ms, FILE* out)
		: xforms(xfm), mset(ms), checkpoint(NULL), outfile(out), input_helper(NULL) {}
#endif
#else
	apply_transform_args(MacroStreamXFormSource& xfm, MACRO_SET_CHECKPOINT_HDR* check, FILE* out)
		: xforms(xfm), checkpoint(check), outfile(out), input_helper(NULL) {}
#endif
	MacroStreamXFormSource & xforms;
#ifdef USE_XFORM_UTILS
	XFormHash & mset;
#endif
	MACRO_SET_CHECKPOINT_HDR* checkpoint;
	FILE* outfile;
#ifdef USE_CLASSAD_LIST_WRITER
	CondorClassAdListWriter & writer;
#endif
	compat_classad::CondorClassAdFileParseHelper *input_helper;
	std::string errmsg;
};



// forward function references
void Usage(FILE*);
void PrintRules(FILE*);
int DoUnitTests(int options);
int ApplyTransform(void *pv, ClassAd * job);
bool ReportSuccess(const ClassAd * job, apply_transform_args & xform_args);
#ifdef USE_XFORM_UTILS
#ifdef USE_CLASSAD_LIST_WRITER
int DoTransforms(input_file & input, const char * constr, MacroStreamXFormSource & xforms, XFormHash & mset, FILE* outfile, CondorClassAdListWriter &writer);
#else
int DoTransforms(input_file & input, const char * constraint, MacroStreamXFormSource & xforms, XFormHash & mset, FILE* outfile);
#endif
#else
int DoTransforms(input_file & input, const char * constraint, MacroStreamXFormSource & xforms, FILE* outfile);
int ParseRulesCallback(void* pv, MACRO_SOURCE& source, MACRO_SET& macro_set, char * line);
#endif
bool LoadJobRouterDefaultsAd(ClassAd & ad);
#ifdef USE_CLASSAD_LIST_WRITER
#else
void write_output_epilog(FILE* outfile, ClassAdFileParseType::ParseType out_format, int cNonEmptyOutputAds);
#endif

#if 0
char * local_param( const char* name, const char* alt_name );
char * local_param( const char* name ); // call param with NULL as the alt
void set_local_param( const char* name, const char* value);
#endif

//#define CONVERT_JRR_STYLE_1 0x0001
//#define CONVERT_JRR_STYLE_2 0x0002
int ConvertJobRouterRoutes(int options);

#ifdef USE_XFORM_UTILS
MACRO_SOURCE FileMacroSource = { false, false, 0, 0, -1, -2 };
#else

// declare enough of the condor_params structure definitions so that we can define submit hashtable defaults
namespace condor_params {
	typedef struct string_value { char * psz; int flags; } string_value;
	struct key_value_pair { const char * key; const string_value * def; };
}

// buffers used while processing the queue statement to inject $(Cluster), $(Process) and $(Step)
// into the submit hash table.  Note that these buffers are also used BEFORE the queue
// statement to set defaults for $(Cluster), $(Process) and $(Step).
static char ProcessString[20]="0", StepString[20]="0", RowString[20]="0", IteratingString[10] = "0";
static condor_params::string_value ProcessMacroDef = { ProcessString, 0 };
static condor_params::string_value StepMacroDef = { StepString, 0 };
static condor_params::string_value RowMacroDef = { RowString, 0 };
static condor_params::string_value IteratingMacroDef = { IteratingString, 0 };

// values for submit hashtable defaults, these are declared as char rather than as const char to make g++ on fedora shut up.
static char OneString[] = "1", ZeroString[] = "0";
static char UnsetString[] = "", EmptyItemString[] = "";

static condor_params::string_value ArchMacroDef = { UnsetString, 0 };
static condor_params::string_value OpsysMacroDef = { UnsetString, 0 };
static condor_params::string_value OpsysVerMacroDef = { UnsetString, 0 };
static condor_params::string_value OpsysMajorVerMacroDef = { UnsetString, 0 };
static condor_params::string_value OpsysAndVerMacroDef = { UnsetString, 0 };
static condor_params::string_value SpoolMacroDef = { UnsetString, 0 };
#ifdef WIN32
static condor_params::string_value IsLinuxMacroDef = { ZeroString, 0 };
static condor_params::string_value IsWinMacroDef = { OneString, 0 };
#elif defined LINUX
static condor_params::string_value IsLinuxMacroDef = { OneString, 0 };
static condor_params::string_value IsWinMacroDef = { ZeroString, 0 };
#else
static condor_params::string_value IsLinuxMacroDef = { ZeroString, 0 };
static condor_params::string_value IsWinMacroDef = { ZeroString, 0 };
#endif
static condor_params::string_value RulesFileMacroDef = { EmptyItemString, 0 };


// Table of submit macro 'defaults'. This provides a convenient way to inject things into the submit
// hashtable without actually running a bunch of code to stuff the table.
// Because they are defaults they will be ignored when scanning for unreferenced macros.
// NOTE: TABLE MUST BE SORTED BY THE FIRST FIELD!!
static MACRO_DEF_ITEM LocalMacroDefaults[] = {
	{ "ARCH",      &ArchMacroDef },
	{ "IsLinux",   &IsLinuxMacroDef },
	{ "IsWindows", &IsWinMacroDef },
	{ "ItemIndex", &RowMacroDef },
	{ "Iterating", &IteratingMacroDef },
	{ "OPSYS",           &OpsysMacroDef },
	{ "OPSYSANDVER",     &OpsysAndVerMacroDef },
	{ "OPSYSMAJORVER",   &OpsysMajorVerMacroDef },
	{ "OPSYSVER",        &OpsysVerMacroDef },
	{ "Row",       &RowMacroDef },
	{ "Rules_file", &RulesFileMacroDef },
	{ "Spool",     &SpoolMacroDef },
	{ "Step",      &StepMacroDef },
	{ "XFormId",    &ProcessMacroDef },
};

static MACRO_DEFAULTS LocalMacroDefaultSet = {
	COUNTOF(LocalMacroDefaults), LocalMacroDefaults, NULL
};

// the submit file is read into this macro table
//
static MACRO_SET LocalMacroSet = {
	0, 0,
	CONFIG_OPT_WANT_META | CONFIG_OPT_KEEP_DEFAULTS | CONFIG_OPT_SUBMIT_SYNTAX,
	0, NULL, NULL, ALLOCATION_POOL(), std::vector<const char*>(),
	&LocalMacroDefaultSet, NULL };

static MACRO_EVAL_CONTEXT_EX LocalContext;

// these are used to keep track of the source of various macros in the table.
const MACRO_SOURCE DefaultMacro = { true, false, 1, -2, -1, -2 }; // for macros set by default
const MACRO_SOURCE ArgumentMacro = { true, false, 2, -2, -1, -2 }; // for macros set by command line
const MACRO_SOURCE LiveMacro = { true, false, 3, -2, -1, -2 };    // for macros use as queue loop variables
MACRO_SOURCE FileMacroSource = { false, false, 0, 0, -1, -2 };

/** Given a universe in string form, return the number

Passing a universe in as a null terminated string in univ.  This can be
a case-insensitive word ("standard", "java"), or the associated number (1, 7).
Returns the integer of the universe.  In the event a given universe is not
understood, returns 0.

(The "Ex"tra functionality over CondorUniverseNumber is that it will
handle a string of "1".  This is primarily included for backward compatility
with the old icky way of specifying a Remote_Universe.
*/
static int CondorUniverseNumberEx(const char * univ)
{
	if( univ == 0 ) {
		return 0;
	}

	if( atoi(univ) != 0) {
		return atoi(univ);
	}

	return CondorUniverseNumber(univ);
}

extern "C" {
	int
	DoCleanup(int,int,const char*)
	{
		return 0; // For historical reasons...
	}
} /* extern "C" */


void
init_local_params()
{
	MyString method;

	if (LocalMacroSet.errors) delete LocalMacroSet.errors;
	LocalMacroSet.errors = new CondorError();

	ArchMacroDef.psz = param( "ARCH" );
	if ( ! ArchMacroDef.psz) ArchMacroDef.psz = UnsetString;
	OpsysMacroDef.psz = param( "OPSYS" );
	if ( ! OpsysMacroDef.psz) OpsysMacroDef.psz = UnsetString;

	// also pick up the variations on opsys if they are defined.
	OpsysAndVerMacroDef.psz = param( "OPSYSANDVER" );
	if ( ! OpsysAndVerMacroDef.psz) OpsysAndVerMacroDef.psz = UnsetString;
	OpsysMajorVerMacroDef.psz = param( "OPSYSMAJORVER" );
	if ( ! OpsysMajorVerMacroDef.psz) OpsysMajorVerMacroDef.psz = UnsetString;
	OpsysVerMacroDef.psz = param( "OPSYSVER" );
	if ( ! OpsysVerMacroDef.psz) OpsysVerMacroDef.psz = UnsetString;

	// submit did this, do we need to?
	SpoolMacroDef.psz = param( "SPOOL" );
	if ( ! SpoolMacroDef.psz) SpoolMacroDef.psz = UnsetString;

	// Insert at least 1 macro, to prime the pump, as it were
	insert_macro("CondorVersion", CondorVersion(), LocalMacroSet, DetectedMacro, LocalContext);
}


class MacroStreamXFormSource : public MacroStreamCharSource
{
public:
	MacroStreamXFormSource() 
		: checkpoint(NULL), fp_iter(NULL), fp_lineno(0), step(0), row(0), has_iterate(false), close_fp_when_done(false) {}
	virtual ~MacroStreamXFormSource() {}
	// returns:
	//   < 0 = error, outmsg is error message
	//   > 0 = number of statements in MacroStreamCharSource, outmsg is TRANSFORM statement or empty
	// 
	int load(FILE* fp, MACRO_SOURCE & source);
	bool has_pending_fp() { return fp_iter != NULL; }
	bool close_when_done(bool close) { close_fp_when_done = close; return has_pending_fp(); }
	int  parse_iterate_args(char * pargs, int expand_options, MACRO_SET &set, std::string & errmsg);
	bool will_iterate() { return has_iterate; }
	int  first_iteration(MACRO_SET &set);
	bool next_iteration(MACRO_SET &set);
	void revert_to_checkpoint(MACRO_SET &set);
	void reset(MACRO_SET &set);
protected:
	MACRO_SET_CHECKPOINT_HDR * checkpoint;
	FILE* fp_iter; // when load stops at an TRANSFORM line, this holds the fp until parse_iterate_args is called
	int   fp_lineno;
	int   step;
	int   row;
	bool  has_iterate;
	bool  close_fp_when_done;
	SubmitForeachArgs oa;
	auto_free_ptr curr_item; // so we can destructively edit the current item from the items list

	void set_live_value(MACRO_SET &set, const char * name, const char * live_value);
	int set_iter_item(MACRO_SET &set, const char* item);
	int report_empty_items(MACRO_SET& set, std::string errmsg);
};

#endif


int
main( int argc, const char *argv[] )
{
	const char *pcolon = NULL;
	ClassAdFileParseType::ParseType def_ads_format = UNSPECIFIED_PARSE_TYPE;
	std::vector<input_file> inputs;
	std::vector<const char *> rules;
	const char *job_match_constraint = NULL;

#ifdef USE_XFORM_UTILS
#else
	LocalContext.init(MySubsys, 3);
#endif

	MyName = condor_basename(argv[0]);
	if (argc == 1) {
		Usage(stderr);
		return 1;
	}

	setbuf( stdout, NULL );

	set_mySubSystem( MySubsys, SUBSYSTEM_TYPE_TOOL );

	myDistro->Init( argc, argv );
	config();

#ifdef USE_XFORM_UTILS
	XFormHash xform_hash;
	xform_hash.init();
#else
	init_local_params();
#endif

	set_debug_flags(NULL, D_EXPR);

#if !defined(WIN32)
	install_sig_handler(SIGPIPE, (SIG_HANDLER)SIG_IGN );
#endif

	for( int ixArg = 1; ixArg < argc; ++ixArg ) {
		const char ** ptr = argv+ixArg;
		if( ptr[0][0] == '-' ) {
			if (MATCH == strcmp(ptr[0], "-")) { // treat a bare - as a jobs filename, it means read from stdin.
				inputs.push_back(input_file(ptr[0], def_ads_format));
			} else if (is_dash_arg_prefix(ptr[0], "verbose", 1)) {
				dash_verbose = true; dash_terse = false;
			} else if (is_dash_arg_prefix(ptr[0], "terse", 3)) {
				dash_terse = true; dash_verbose = false;
			} else if (is_dash_arg_prefix(ptr[0], "debug", 2)) {
				// dprintf to console
				dprintf_set_tool_debug("TOOL", 0);
			} else if (is_dash_arg_prefix(ptr[0], "rules", 1)) {
				const char * pfilearg = ptr[1];
				if ( ! pfilearg || (*pfilearg == '-' && (MATCH != strcmp(pfilearg,"-"))) ) {
					fprintf( stderr, "%s: -rules requires another argument\n", MyName );
					exit(1);
				}
				rules.push_back(pfilearg);
				++ixArg; ++ptr;
			} else if (is_dash_arg_colon_prefix(ptr[0], "in", &pcolon, 1)) {
				const char * pfilearg = ptr[1];
				if ( ! pfilearg || (*pfilearg == '-' && (MATCH != strcmp(pfilearg,"-"))) ) {
					fprintf( stderr, "%s: -in requires another argument\n", MyName );
					exit(1);
				}
				++ixArg; ++ptr;
				ClassAdFileParseType::ParseType in_format = def_ads_format;
				if (pcolon) {
					in_format = parseAdsFileFormat(pcolon, in_format);
				}
				inputs.push_back(input_file(pfilearg, in_format));
			} else if (is_dash_arg_colon_prefix(ptr[0], "out", &pcolon, 1)) {
				const char * pfilearg = ptr[1];
				if ( ! pfilearg || (*pfilearg == '-' && (MATCH != strcmp(pfilearg,"-"))) ) {
					fprintf( stderr, "%s: -out requires another argument\n", MyName );
					exit(1);
				}
				DashOutName = pfilearg;
				++ixArg; ++ptr;

				ClassAdFileParseType::ParseType out_format = DashOutFormat;
				if (pcolon) {
					StringList opts(++pcolon);
					for (const char * opt = opts.first(); opt; opt = opts.next()) {
						if (YourString(opt) == "nosort") {
							DashOutAttrsInHashOrder = true;
						} else {
							out_format = parseAdsFileFormat(opt, out_format);
						}
					}
				}
				DashOutFormat = out_format;
			} else if (is_dash_arg_prefix(ptr[0], "long", 1)
					|| is_dash_arg_prefix(ptr[0], "json", 2)
					|| is_dash_arg_prefix(ptr[0], "xml", 3)) {
				// Specify the default parse type for input files, this will also set the output format
				// if it is Parse_auto at the time we begin writing to the file.
				def_ads_format = ClassAdFileParseType::Parse_long;
				if (is_dash_arg_prefix(ptr[0], "json", 2)) {
					def_ads_format = ClassAdFileParseType::Parse_json;
				} else if (is_dash_arg_prefix(ptr[0], "xml", 3)) {
					def_ads_format = ClassAdFileParseType::Parse_xml;
				}
				// fixup parse format for inputs that have an unspecified parse type
				for (size_t ii = 0; ii < inputs.size(); ++ii) {
					if (inputs[ii].parse_format == UNSPECIFIED_PARSE_TYPE) {
						inputs[ii].parse_format = def_ads_format;
					}
				}
			} else if (is_dash_arg_colon_prefix(ptr[0], "unit-test", &pcolon, 4)) {
				UnitTestOpts = 1;
				if (pcolon) { 
					int opt = atoi(pcolon+1);
					if (opt > 1) {  UnitTestOpts =  opt; }
					else if (MATCH == strcmp(pcolon+1, "hash")) {
						DumpLocalHash = 1;
					}
				}
			} else if (is_dash_arg_colon_prefix(ptr[0], "testing", &pcolon, 4)) {
				testing.enabled = true;
				if (pcolon) { 
					StringList opts(++pcolon);
					for (const char * opt = opts.first(); opt; opt = opts.next()) {
						if (is_arg_colon_prefix(opt, "repeat", &pcolon, 3)) {
							testing.repeat_count = (pcolon) ? atoi(++pcolon) : 100;
						} else if (is_arg_prefix(opt, "nooutput", 5)) {
							testing.no_output = true;
						}
					}
				}
			} else if (is_dash_arg_prefix(ptr[0], "help", 1)) {
				if (ptr[1] && (MATCH == strcmp(ptr[1], "rules"))) {
					PrintRules(stdout);
					exit(0);
				}
				Usage(stdout);
				exit(0);
			} else if (is_dash_arg_colon_prefix(ptr[0], "convertoldroutes", &pcolon, 4)) {
				if (pcolon) {
					DashConvertOldRoutes = atoi(++pcolon);
				}
				if ( ! DashConvertOldRoutes) DashConvertOldRoutes = 1;
			} else {
				Usage(stderr);
				exit(1);
			}
		} else if (strchr(ptr[0],'=')) {
			// loose arguments that contain '=' are attrib=value pairs.
			// so we split on the = into name and value and insert into the submit hashtable
			// we do this BEFORE parsing the submit file so that they can be referred to by submit.
			std::string name(ptr[0]);
			size_t ix = name.find('=');
			std::string value(name.substr(ix+1));
			name = name.substr(0,ix);
			trim(name); trim(value);
			if ( ! name.empty() && name[1] == '+') {
				name = "MY." + name.substr(1);
			}
			if (name.empty() || ! is_valid_param_name(name.c_str())) {
				fprintf( stderr, "%s: invalid attribute name '%s' for attrib=value assigment\n", MyName, name.c_str() );
				exit(1);
			}
#ifdef USE_XFORM_UTILS
			MACRO_EVAL_CONTEXT ctx; ctx.init(MySubsys,0);
			xform_hash.set_arg_variable(name.c_str(), value.c_str(), ctx);
#else
			MACRO_EVAL_CONTEXT ctx; ctx.init(MySubsys,0);
			insert_macro(name.c_str(), value.c_str(), LocalMacroSet, ArgumentMacro, ctx);
#endif
		} else {
			inputs.push_back(input_file(*ptr, def_ads_format));
		}
	}

	// the -convertoldroutes argument tells us to just read and
	if (DashConvertOldRoutes) { exit(ConvertJobRouterRoutes(DashConvertOldRoutes)); }

	// the -dry argument takes a qualifier that I'm hijacking to do queue parsing unit tests for now the 8.3 series.
	if (UnitTestOpts > 0) { exit(DoUnitTests(UnitTestOpts)); }

	// read the transform rules into the MacroStreamXFormSource
	int rval = 0;
	MacroStreamXFormSource ms;
	if (rules.empty()) {
		fprintf(stderr, "ERROR : no transform rules file specified.\n");
		Usage(stderr);
		exit(1);
	} else if (rules.size() > 1) {
		// TODO: allow multiple rules files?
		fprintf(stderr, "ERROR : too many transform rules file specified.\n");
		Usage(stderr);
		exit(1);
	} else if (MATCH == strcmp("-", rules[0])) {
#ifdef USE_XFORM_UTILS
		xform_hash.set_RulesFile("<stdin>", FileMacroSource);
#else
		insert_source("<stdin>", LocalMacroSet, FileMacroSource);
#endif
		rval = ms.load(stdin, FileMacroSource);
	} else {
		FILE *file = safe_fopen_wrapper_follow(rules[0], "r");
		if (file == NULL) {
			fprintf(stderr, "Can't open rules file: %s\n", rules[0]);
			return -1;
		}
#ifdef USE_XFORM_UTILS
		xform_hash.set_RulesFile(rules[0], FileMacroSource);
#else
		insert_source(rules[0], LocalMacroSet, FileMacroSource);
		RulesFileMacroDef.psz = const_cast<char*>(rules[0]);
#endif
		rval = ms.load(file, FileMacroSource);
		if (rval < 0 || ! ms.close_when_done(true)) {
			fclose(file);
		}
	}

	if (rval < 0) {
		fprintf(stderr, "ERROR reading transform rules from %s : %s\n",
#ifdef USE_XFORM_UTILS
			xform_hash.get_RulesFilename(),
#else
			macro_source_filename(FileMacroSource, LocalMacroSet),
#endif
			strerror(errno));
		return rval;
	}

	if (inputs.empty()) {
		fprintf(stderr, "Warning: no input file specified - nothing to transform.\n");
		exit(1);
	}

	FILE* outfile = stdout;
	bool  close_outfile = false;
	if (DashOutName && ! (YourString(DashOutName) == "-")) {
		outfile = safe_fopen_wrapper_follow(DashOutName,"w");
		if ( ! outfile) {
			fprintf( stderr, "ERROR: Failed to open output file (%s)\n", strerror(errno));
			exit(1);
		}
		close_outfile = true;
	}

	// if no default parse format has been specified for the input files, choose auto
	if (def_ads_format == UNSPECIFIED_PARSE_TYPE) { def_ads_format = ClassAdFileParseType::Parse_auto; }
	// if no output parse format has been specified, choose long
	if (DashOutFormat == UNSPECIFIED_PARSE_TYPE) { DashOutFormat = ClassAdFileParseType::Parse_long; }

#ifdef USE_CLASSAD_LIST_WRITER
	CondorClassAdListWriter writer(DashOutFormat);
#endif

	for (size_t ixInput = 0; ixInput < inputs.size(); ++ixInput) {
		// use default parse format if input file still has an unspecifed one.
		if (inputs[ixInput].parse_format == UNSPECIFIED_PARSE_TYPE) { inputs[ixInput].parse_format = def_ads_format; }
#ifdef USE_XFORM_UTILS
#ifdef USE_CLASSAD_LIST_WRITER
		rval = DoTransforms(inputs[ixInput], job_match_constraint, ms, xform_hash, outfile, writer);
#else
		rval = DoTransforms(inputs[ixInput], job_match_constraint, ms, xform_hash, outfile);
#endif
#else
		rval = DoTransforms(inputs[ixInput], job_match_constraint, ms, outfile);
#endif
	}

#ifdef USE_CLASSAD_LIST_WRITER
	writer.writeFooter(outfile);
#else
	write_output_epilog(outfile, DashOutFormat, cNonEmptyOutputAds);
#endif
	if (close_outfile) {
		fclose(outfile);
		outfile = NULL;
	}

	return rval;
}

void
Usage(FILE * out)
{
	fprintf(out,
		"Usage: %s -rules <rules-file> [options] [<key>=<value>] <infile> [<infile>]\n"
		"    Read ClassAd(s) from <infile> and transform based on rules from <rules-file>\n\n", MyName);
	fprintf(out,
		"    [options] are\n"
		"\t-help [rules]\t\t Display this screen or rules documentation and exit\n"
		"\t-out[:<form>,nosort] <outfile>\n"
		"\t\t\t\t Write transformed ClassAd(s) to <outfile> in format <form>\n"
		"\t           ClassAd(s) are sorted by attribute unless nosort is specified\n"
		"\t           <form> can be is one of:\n"
		"\t       long    The traditional -long form. This is the default\n"
		"\t       xml     XML form, the same as -xml\n"
		"\t       json    JSON classad form, the same as -json\n"
		"\t       new     'new' classad form without newlines\n"
		"\t       auto    For input, guess the format from reading the input stream\n"
		"\t               For output, use the same format as the first input\n"
		"\t-in[:<form>] <infile>\t Read ClassAd(s) to transform from <infile> in format <form>\n"
		"\t-long\t\t\t Use long form for both input and output ClassAd(s)\n"
		"\t-json\t\t\t Use JSON form for both input and output ClassAd(s)\n"
		"\t-xml \t\t\t Use XML form for both input and output ClassAd(s)\n"
		"\t-verbose\t\t Verbose mode, echo transform rules as they are executed\n"
		//"\t-debug  \t\tDisplay debugging output\n"
		"\n    <key>=<value> Arguments are assigned before the rules file is parsed and can be used\n"
		"                  to pass arguments or enable optional behavior in the rules\n\n"
		"    If <rules-file> is -, transform rules are read from stdin until the TRANSFORM rule\n"
		"    If <infile> is -, ClassAd(s) are read from stdin.\n"
		"    Transformed ads are written to stdout unless an -out argument is provided\n"
		);
}

void PrintRules(FILE* out)
{
	fprintf(out, "\ncondor_transform_ads rules:\n"
		"\nTransform rules files consist of lines containing key=value pairs or\n"
		"transform commands such as SET, RENAME, etc. Transform commands execute\n"
		"as they are read and can make use of values set up until that point using\n"
		"the $(key) macro substitution commands that HTCondor configuration files and\n"
		"condor_submit files use. Most constructs that work in these files will also\n"
		"work in rules files such as if/else. Macro substitution will fetch attributes of\n"
		"the ClassAd to be transformed when $(MY.attr) is used.\n"
		"\nThe transform commands are:\n\n");

	fprintf(out,
		"   SET     <attr> <expr>\t- Set <attr> to <expr>\n"
		"   EVALSET <attr> <expr>\t- Evaluate <expr> and then set <attr> to the result\n"
		"   DEFAULT <attr> <expr>\t- Set <attr> to <expr> if <attr> is undefined or missing\n"
		"   COPY    <attr> <newattr>\t- Copy the value of <attr> to <newattr>\n"
		"   COPY    /<regex>/ <newattrs>\t- Copy the values of attributes matching <regex> to <newattrs>\n"
		"   RENAME  <attr> <newattr>\t- Rename <attr> to <newattr>\n"
		"   RENAME  /<regex>/ <newattrs>\t- Rename attributes matching <regex> to <newattrs>\n"
		"   DELETE  <attr> <newattr>\t- Delete <attr>\n"
		"   DELETE  /<regex>/\t\t- Delete attributes matching <regex>\n"
		"   EVALMACRO <key> <expr>\t- Evaluate <expr> and then insert it as a transform macro value\n"
		"   TRANSFORM [<N>] [<vars>] [in <list> | from <file> | matching <pattern>] - Do the Tranform\n"
		);

	fprintf(out,
		"\nIn the above commands <attr> must be a valid attribute name and <expr> a valid ClassAd\n"
		"expression or literal.  The various $() macros will be expanded in <expr>, <newattr> or\n"
		"<newattrs> before they are parsed as ClassAd expressions or attribute names.\n"
		"\nWhen a COPY,RENAME or DELETE with regex is used, regex capture groups are substituted in\n"
		"<newattrs> after $() expansion. \\0 will expand to the entire match, \\1 to the first capture, etc.\n"
		"\nA TRANSFORM command must be the last command in the rules file. It takes the same options as the\n"
		"QUEUE statement from a HTCONDOR submit file. There is an implicit TRANSFORM 1 at the end of a rules file\n"
		"if there is no explicit TRANSFORM command.\n"
		);

	fprintf(out, "\n");
}

#ifdef USE_XFORM_UTILS
#else

// returns a pointer to the iteration args if this line is an TRANSFORM statement
// returns NULL if it is not.
static char * is_transform_statement(char * line, const char * keyword)
{
	const int cchKey = strlen(keyword);
	while (*line && isspace(*line)) ++line;
	if (starts_with_ignore_case(line, keyword) && isspace(line[cchKey])) {
		char * pargs = line+cchKey;
		while (*pargs && isspace(*pargs)) ++pargs;
		if (*pargs == '=' || *pargs == ':') return NULL;
		return pargs;
	}
	return NULL;
}

int MacroStreamXFormSource::load(FILE* fp, MACRO_SOURCE & FileSource)
{
	StringList lines;

	while (true) {
		int lineno = FileSource.line;
		char * line = getline_trim(fp, FileSource.line);
		if ( ! line) {
			if (ferror(fp)) return -1;
			break;
		}

		/*
		// discard comment lines
		if (testing.enabled) {
			if (*line == '#') continue;
			lineno = FileSource.line-1; // disable linenumber corrections
		}
		*/

		if (FileSource.line != lineno+1) {
			// if we read more than a single line, comment the new linenumber
			MyString buf; buf.formatstr("#opt:lineno:%d", FileSource.line-1);
			lines.append(buf.c_str());
		}
		lines.append(line);

		if (is_transform_statement(line, "transform")) {
			// if this is an TRANSFORM statement, then we don't read any more statements.
			// because we can't expand the TRANSFORM yet, and we may need to treat
			// the rest of the file as the arguments list. so store the fp until we
			// can later parse the expanded TRANSFORM args
			fp_iter = fp;
			fp_lineno = FileSource.line;
			has_iterate = true;
			break;
		}
	}

	file_string.set(lines.print_to_delimed_string("\n"));
	open(file_string, FileSource);
	rewind();
	return lines.number();
}

int MacroStreamXFormSource::parse_iterate_args(char * pargs, int expand_options, MACRO_SET &set, std::string & errmsg)
{
	int citems = 0;
	int begin_lineno = fp_lineno;

	FILE * fp = fp_iter;
	fp_iter = NULL; // so we cant end up here again.

	// parse it using the same parser that condor submit uses for queue statements
	int rval = oa.parse_queue_args(pargs);
	if (rval < 0) {
		formatstr(errmsg, "invalid TRANSFORM statement");
		if (close_fp_when_done && fp) { fclose(fp); }
		return rval;
	}

	// if no loop variable specified, but a foreach mode is used. use "Item" for the loop variable.
	if (oa.vars.isEmpty() && (oa.foreach_mode != foreach_not)) { oa.vars.append("Item"); }

	// fill in the items array from a file
	if ( ! oa.items_filename.empty()) {
		if (oa.items_filename == "<") {
			// if reading iterate items from the input file, we have to read them now
			if ( ! fp) {
				errmsg = "unexpected error while attempting to read TRANSFORM items from xform file.";
				return -1;
			}

			// read items from submit file until we see the closing brace on a line by itself.
			bool saw_close_brace = false;
			for (;;) {
				char *line = getline_trim(fp, fp_lineno);
				if ( ! line) break; // null indicates end of file
				if (line[0] == '#') continue; // skip comments.
				if (line[0] == ')') { saw_close_brace = true; break; }
				if (oa.foreach_mode == foreach_from) {
					oa.items.append(line);
				} else {
					oa.items.initializeFromString(line);
				}
			}
			if (close_fp_when_done) { fclose(fp); fp = NULL; }
			if ( ! saw_close_brace) {
				formatstr(errmsg, "Reached end of file without finding closing brace ')'"
					" for TRANSFORM command on line %d", begin_lineno);
				return -1;
			}
		} else if (oa.items_filename == "-") {
			int lineno = 0;
			for (char* line=NULL;;) {
				line = getline_trim(stdin, lineno);
				if ( ! line) break;
				if (oa.foreach_mode == foreach_from) {
					oa.items.append(line);
				} else {
					oa.items.initializeFromString(line);
				}
			}
		} else {
			MACRO_SOURCE ItemsSource;
			fp = Open_macro_source(ItemsSource, oa.items_filename.Value(), false, set, errmsg);
			if ( ! fp) {
				return -1;
			}
			for (char* line=NULL;;) {
				line = getline_trim(fp, ItemsSource.line);
				if ( ! line) break;
				oa.items.append(line);
			}
			rval = Close_macro_source(fp, ItemsSource, set, 0);
		}
	}

	if (close_fp_when_done && fp) { fclose(fp); fp = NULL; }

	// fill in the items array from a glob
	//
	switch (oa.foreach_mode) {
	case foreach_in:
	case foreach_from:
		// itemlist is already correct
		// PRAGMA_REMIND("do argument validation here?")
		citems = oa.items.number();
		break;

	case foreach_matching:
	case foreach_matching_files:
	case foreach_matching_dirs:
	case foreach_matching_any:
		if (oa.foreach_mode == foreach_matching_files) {
			expand_options &= ~EXPAND_GLOBS_TO_DIRS;
			expand_options |= EXPAND_GLOBS_TO_FILES;
		} else if (oa.foreach_mode == foreach_matching_dirs) {
			expand_options &= ~EXPAND_GLOBS_TO_FILES;
			expand_options |= EXPAND_GLOBS_TO_DIRS;
		} else if (oa.foreach_mode == foreach_matching_any) {
			expand_options &= ~(EXPAND_GLOBS_TO_FILES|EXPAND_GLOBS_TO_DIRS);
		}
		citems = submit_expand_globs(oa.items, expand_options, errmsg);
		if ( ! errmsg.empty()) {
			fprintf(stderr, "\n%s: %s", citems >= 0 ? "WARNING" : "ERROR", errmsg.c_str());
			errmsg.clear();
		}
		if (citems < 0) return citems;
		break;

	default:
	case foreach_not:
		// to simplify the loop below, set a single empty item into the itemlist.
		citems = 1;
		break;
	}

	return citems;
}

void MacroStreamXFormSource::set_live_value(MACRO_SET &set, const char * name, const char * live_value)
{
	MACRO_ITEM * pitem = find_macro_item(name, NULL, set);
	if ( ! pitem) {
		insert_macro(name, "", set, WireMacro, LocalContext);
		pitem = find_macro_item(name, NULL, set);
		if (set.metat) {
			MACRO_META * pmeta = &set.metat[pitem - set.table];
			pmeta->live = true;
		}
	}
	ASSERT(pitem);
	//const char * old_value = pitem->raw_value;
	pitem->raw_value = live_value;
}

int MacroStreamXFormSource::set_iter_item(MACRO_SET &set, const char* item)
{
	if (oa.vars.isEmpty()) return 0;

	// make a copy of the item so we can destructively edit it.
	char * data;
	if (item) { 
		data = strdup(item);
		curr_item.set(data);
	} else {
		data = EmptyItemString;
		EmptyItemString[0] = 0;
		curr_item.clear();
	}

	// set the first loop variable unconditionally, we set it initially to the whole item
	// we may later truncate that item when we assign fields to other loop variables.
	char * var = oa.vars.first();
	set_live_value(set, var, data);

	const char* token_seps = ", \t";
	const char* token_ws = " \t";

	// if there is more than a single loop variable, then assign them as well
	// we do this by destructively null terminating the item for each var
	// the last var gets all of the remaining item text (if any)
	while ((var = oa.vars.next())) {
		// scan for next token separator
		while (*data && ! strchr(token_seps, *data)) ++data;
		// null terminate the previous token and advance to the start of the next token.
		if (*data) {
			*data++ = 0;
			// skip leading separators and whitespace
			while (*data && strchr(token_ws, *data)) ++data;
			set_live_value(set, var, data);
		}
	}
	return curr_item.ptr() != NULL;
}

int MacroStreamXFormSource::report_empty_items(MACRO_SET& set, std::string errmsg)
{
	int count = 0;
	std::string empties;

	for (char * var = oa.vars.first(); var; var = oa.vars.next()) {
		MACRO_ITEM* pitem = find_macro_item(var, NULL, set);
		if ( ! pitem || (pitem->raw_value != EmptyItemString && 0 == strlen(pitem->raw_value))) {
			if ( ! empties.empty()) empties += ",";
			empties += var;
			++count;
		}
	}
	if (count) {
		formatstr(errmsg, "Empty value(s) for %s at ItemIndex=%d", empties.c_str(), row);
		return count;
	}

	return 0;
}

int  MacroStreamXFormSource::first_iteration(MACRO_SET &set)
{
	// if there is no iterator, or it is trivial, there is no need to go again.
	if ( ! has_iterate || (oa.foreach_mode == foreach_not && oa.queue_num == 1)) {
		return 0;
	}

	oa.items.rewind();
	for (char* var = oa.vars.first(); var; var = oa.vars.next()) {
		set_live_value(set, var, EmptyItemString);
	}

	checkpoint = checkpoint_macro_set(set);

	step = row = 0;
	(void)sprintf(StepString, "%d", step);
	(void)sprintf(RowString, "%d", row);
	(void)sprintf(IteratingString, "%d", has_iterate);
	set_iter_item(set, oa.items.first());
	return 1;
}

bool MacroStreamXFormSource::next_iteration(MACRO_SET &set)
{
	if (checkpoint) rewind_macro_set(set, checkpoint, false);

	bool has_next_item = false;
	if (step+1 < oa.queue_num) {
		++step;
		has_next_item = true;
		(void)sprintf(StepString, "%d", step);
	} else {
		step = 0;
		(void)sprintf(StepString, "%d", step);
		++row;
		(void)sprintf(RowString, "%d", row);
		has_next_item = set_iter_item(set, oa.items.next());
	}
	return has_next_item;
}

void MacroStreamXFormSource::revert_to_checkpoint(MACRO_SET &set)
{
	if (checkpoint) rewind_macro_set(set, checkpoint, false);
	curr_item.clear();
	oa.items.rewind();
}

void MacroStreamXFormSource::reset(MACRO_SET &set)
{
	if (checkpoint) rewind_macro_set(set, checkpoint, true);
	checkpoint = NULL;
	curr_item.clear();
	oa.clear();
}

#endif

#ifdef USE_CLASSAD_ITERATOR




#else

#define PROCESS_ADS_CALLBACK_IS_KEEPING_AD 0x7B8B9BAB
typedef int (*FNPROCESS_ADS_CALLBACK)(void* pv, ClassAd * ad);

static bool read_classad_file (
	const char *filename,
	ClassAdFileParseHelper& parse_help,
	FNPROCESS_ADS_CALLBACK callback, void*pv,
	const char * constr)
{
	bool success = false;
	bool close_file = true;

	FILE* file;
	if (MATCH == strcmp(filename, "-")) {
		file = stdin;
		close_file = false;
	} else {
		file = safe_fopen_wrapper_follow(filename, "r");
		if (file == NULL) {
			fprintf(stderr, "Can't open file of ClassAds: %s\n", filename);
			return false;
		}
		close_file = true;
	}

	for (;;) {
		ClassAd* classad = new ClassAd();

		int error;
		bool is_eof;
		int cAttrs = classad->InsertFromFile(file, is_eof, error, &parse_help);

		bool include_classad = cAttrs > 0 && error >= 0;
		if (include_classad && constr) {
			classad::Value val;
			if (classad->EvaluateExpr(constr,val)) {
				if ( ! val.IsBooleanValueEquiv(include_classad)) {
					include_classad = false;
				}
			}
		}

		int result = 0;
		if (include_classad && testing.enabled) {
			ClassAdList ads;
			int num_iters = MAX(1,testing.repeat_count);
			for (int ii = 0; ii < num_iters; ++ii) { ads.Insert(new ClassAd(*classad)); }
			ads.Open();
			double tmBegin = _condor_debug_get_time_double();
			while(ClassAd *ad = ads.Next()) { callback(pv, ad); }
			double elapsed = _condor_debug_get_time_double() - tmBegin;
			fprintf(stderr, "%d repetitions of the transform took %.3f ms (output %s)\n",
				num_iters, elapsed*1000.0, testing.no_output ? "disabled" : "enabled");
			ads.Close();
			ads.Clear();
			include_classad = false;
		}
		if ( ! include_classad || ((result = callback(pv, classad)) != PROCESS_ADS_CALLBACK_IS_KEEPING_AD) ) {
			// delete the classad if we didn't pass it to the callback, or if
			// the callback didn't take ownership of it.
			delete classad;
		}
		if (result < 0) {
			success = false;
			error = result;
		}
		if (is_eof) {
			success = true;
			break;
		}
		if (error < 0) {
			success = false;
			break;
		}
	}

	if (close_file) fclose(file);
	file = NULL;

	return success;
}

class CondorQClassAdFileParseHelper : public compat_classad::CondorClassAdFileParseHelper
{
 public:
	CondorQClassAdFileParseHelper(ParseType typ=Parse_long)
		: CondorClassAdFileParseHelper("\n", typ)
		, is_schedd(false), is_submitter(false)
	{}
	virtual int PreParse(std::string & line, ClassAd & ad, FILE* file);
	virtual int OnParseError(std::string & line, ClassAd & ad, FILE* file);
	std::string schedd_name;
	std::string schedd_addr;
	bool is_schedd;
	bool is_submitter;
};

// this method is called before each line is parsed. 
// return 0 to skip (is_comment), 1 to parse line, 2 for end-of-classad, -1 for abort
int CondorQClassAdFileParseHelper::PreParse(std::string & line, ClassAd & /*ad*/, FILE* /*file*/)
{
	// treat blank lines as delimiters.
	if (line.size() <= 0) {
		return 2; // end of classad.
	}

	// standard delimitors are ... and ***
	if (starts_with(line,"\n") || starts_with(line,"...") || starts_with(line,"***")) {
		return 2; // end of classad.
	}

	// the normal output of condor_q -long is "-- schedd-name <addr>"
	// we want to treat that as a delimiter, and also capture the schedd name and addr
	if (starts_with(line, "-- ")) {
		is_schedd = starts_with(line.substr(3), "Schedd:");
		is_submitter = starts_with(line.substr(3), "Submitter:");
		if (is_schedd || is_submitter) {
			size_t ix1 = schedd_name.find(':');
			schedd_name = line.substr(ix1+1);
			ix1 = schedd_name.find_first_of(": \t\n");
			if (ix1 != std::string::npos) {
				size_t ix2 = schedd_name.find_first_not_of(": \t\n", ix1);
				if (ix2 != std::string::npos) {
					schedd_addr = schedd_name.substr(ix2);
					ix2 = schedd_addr.find_first_of(" \t\n");
					if (ix2 != std::string::npos) {
						schedd_addr = schedd_addr.substr(0,ix2);
					}
				}
				schedd_name = schedd_name.substr(0,ix1);
			}
		}
		return 2;
	}


	// check for blank lines or lines whose first character is #
	// tell the parser to skip those lines, otherwise tell the parser to
	// parse the line.
	for (size_t ix = 0; ix < line.size(); ++ix) {
		if (line[ix] == '#' || line[ix] == '\n')
			return 0; // skip this line, but don't stop parsing.
		if (line[ix] != ' ' && line[ix] != '\t')
			break;
	}
	return 1; // parse this line
}

// this method is called when the parser encounters an error
// return 0 to skip and continue, 1 to re-parse line, 2 to quit parsing with success, -1 to abort parsing.
int CondorQClassAdFileParseHelper::OnParseError(std::string & line, ClassAd & ad, FILE* file)
{
	// when we get a parse error, skip ahead to the start of the next classad.
	int ee = this->PreParse(line, ad, file);
	while (1 == ee) {
		if ( ! readLine(line, file, false) || feof(file)) {
			ee = 2;
			break;
		}
		ee = this->PreParse(line, ad, file);
	}
	return ee;
}

#endif

#define TRANSFORM_IN_PLACE 1

#ifdef USE_XFORM_UTILS

// return true from the transform if no longer care about the job ad
// return false to take ownership of the job ad
int ApplyTransform (
	void* pv,
	ClassAd * input_ad)
{
	if ( ! pv) return 1;
	apply_transform_args & args = *(apply_transform_args*)pv;
	//const ClassAd * job = input_ad;

	unsigned int flags = 1 | (dash_verbose ? 2 : 0);

	// TODO: defer expansion of iterate args until the first SET statement?

	int rval = args.xforms.will_iterate(args.mset, args.errmsg);
	if (rval < 0) {
		fprintf(stderr, "ERROR: %s\n", args.errmsg.c_str()); return -1;
		return 1;
	}
	bool will_iterate = rval != 0;
	if (will_iterate) {
		will_iterate = args.xforms.first_iteration(args.mset);
	}

	if (will_iterate) {
		bool iterating = true;
		while (iterating) {
			ClassAd * ad_to_transform = new ClassAd(*input_ad);
			rval = TransformClassAd(ad_to_transform, args.xforms, args.mset, args.errmsg, flags);
			if (rval >= 0) {
				ReportSuccess(ad_to_transform, args);
				iterating = args.xforms.next_iteration(args.mset);
			} else {
				iterating = false;
			}
			delete ad_to_transform;
		}
		args.xforms.clear_iteration(args.mset);
	} else {
		rval = TransformClassAd(input_ad, args.xforms, args.mset, args.errmsg, flags);
		if (rval) {
			return rval;
		}
		ReportSuccess(input_ad, args);
	}

	args.mset.rewind_to_state(args.checkpoint, false);

	return rval ? rval : 1; // we return non-zero to allow the job to be deleted
}

#else

// delete an attribute of the given classad.
// returns 0  if the delete did not happen
// returns 1  if the delete happened.
int DoDeleteAttr(ClassAd * ad, const std::string & attr)
{
	if (dash_verbose) fprintf(stdout, "DELETE %s\n", attr.c_str());
	return ad->Delete(attr) ? 1 : 0;
}

// rename an attribute of the given classad.
// returns -1 if the new attribute is invalid
// returns 0  if the rename did not happen
// returns 1  if the rename happened.
int DoRenameAttr(ClassAd * ad, const std::string & attr, const char * attrNew)
{
	if (dash_verbose) fprintf(stdout, "RENAME %s to %s\n", attr.c_str(), attrNew);
	if ( ! IsValidAttrName(attrNew)) {
		fprintf(stderr, "ERROR: RENAME %s new name %s is not valid\n", attr.c_str(), attrNew);
		return -1;
	} else {
		ExprTree * tree = ad->Remove(attr);
		if (tree) {
			if (ad->Insert(attrNew, tree, false)) {
				return 1;
			} else {
				fprintf(stderr, "ERROR: could not rename %s to %s\n", attr.c_str(), attrNew);
				if ( ! ad->Insert(attr, tree, false)) {
					delete tree;
				}
			}
		}
	}
	return 0;
}

// copy the expr tree of an attribute to a new attribute name.
// returns -1 if the new attribute is invalid
// returns 0  if the copy did not happen
// returns 1  if the copy happened.
int DoCopyAttr(ClassAd * ad, const std::string & attr, const char * attrNew)
{
	if (dash_verbose) fprintf(stdout, "COPY %s to %s\n", attr.c_str(), attrNew);
	if ( ! IsValidAttrName(attrNew)) {
		fprintf(stderr, "ERROR: COPY %s new name %s is not valid\n", attr.c_str(), attrNew);
		return -1;
	} else {
		ExprTree * tree = ad->Lookup(attr);
		if (tree) {
			tree = tree->Copy();
			if (ad->Insert(attrNew, tree, false)) {
				return 1;
			} else {
				fprintf(stderr, "ERROR: could not copy %s to %s\n", attr.c_str(), attrNew);
				delete tree;
			}
		}
	}
	return 0;
}


typedef struct {
	const char * key;
	int          value;
	int          options;
} Keyword;
typedef nocase_sorted_tokener_lookup_table<Keyword> KeywordTable;

#define kw_opt_argcount_mask 0x0F
#define kw_opt_regex         0x10

enum {
	kw_COPY=1,
	kw_DEFAULT,
	kw_DELETE,
	kw_EVALMACRO,
	kw_EVALSET,
	kw_NAME,
	kw_RENAME,
	kw_REQUIREMENTS,
	kw_SET,
	kw_TRANSFORM,
	kw_UNIVERSE,
};

// This must be declared in string sorted order, they do not have to be in enum order.
#define KW(a, f) { #a, kw_##a, f }
static const Keyword ActionKeywordItems[] = {
	KW(COPY, 2 | kw_opt_regex),
	KW(DEFAULT, 2),
	KW(DELETE, 1 | kw_opt_regex),
	KW(EVALMACRO, 2),
	KW(EVALSET, 2),
	KW(NAME, 0),
	KW(RENAME, 2 | kw_opt_regex),
	KW(REQUIREMENTS, 0),
	KW(SET, 2),
	KW(TRANSFORM, 0),
	KW(UNIVERSE, 1),
};
#undef KW
static const KeywordTable ActionKeywords = SORTED_TOKENER_TABLE(ActionKeywordItems);


struct _parse_rules_args {
	ClassAd * ad;
	ClassAd * targetAd;
	MacroStreamXFormSource * xforms;
};

// substitute regex groups into a string, appending the output to a std::string
//
const char * append_substituted_regex (
	std::string &output,      // substituted regex will be appended to this
	const char * input,       // original input passed to pcre_exec (ovector has offsets into this)
	int ovector[], // output vector from pcre_exec
	int cvec,      // output count from pcre_exec
	const char * replacement, // replacement template string
	char tagChar)  // char that introduces a subtitution in replacement string, usually \ or $
{
	const char * p = replacement;
	const char * lastp = p; // points to the start of the last point we copied from.
	while (*p) {
		if (p[0] == tagChar && p[1] >= '0' && p[1] < '0' + cvec) {
			if (p > lastp) { output.append(lastp, p-lastp); }
			int ix = p[1] - '0';
			int ix1 = ovector[ix*2];
			int ix2 = ovector[ix*2+1];
			output.append(&input[ix1], ix2 - ix1);
			++p;
			lastp = p+1; // skip over digit
		}
		++p;
	}
	if (p > lastp) { output.append(lastp, p-lastp); }
	return output.c_str();
}

// Handle kw_COPY, kw_RENAME and kw_DELETE when the first argument is a regex.
// 
int DoRegexAttrOp(int kw_value, ClassAd *ad, pcre* re, int re_options, const char * replacement)
{
	const int max_group_count = 11; // only \0 through \9 allowed.
	int ovector[3 * (max_group_count + 1)]; // +1 for the string itself
	std::string newAttr;
	const char tagChar = '\\';
	newAttr.reserve(100);

	// pcre_exec doesn't like to see these flags.
	re_options &= ~(PCRE_CASELESS | PCRE_UNGREEDY | PCRE_MULTILINE);

	// keep track of matching attributes, we will only modify the input ad
	// after we are done iterating through it.
	std::map<std::string, std::string> matched;

	for (ClassAd::iterator it = ad->begin(); it != ad->end(); ++it) {
		const char * input = it->first.c_str();
		int cchin = it->first.length();
		int cvec = pcre_exec(re, NULL, input, cchin, 0, re_options, ovector, (int)COUNTOF(ovector));
		if (cvec <= 0)
			continue; // does not match

		newAttr = "";
		if (kw_value != kw_DELETE) {
			append_substituted_regex(newAttr, input, ovector, cvec, replacement, tagChar);
		}
		matched[it->first] = newAttr;
	}

	// now modify the input ad
	for (std::map<std::string, std::string>::iterator it = matched.begin(); it != matched.end(); ++it) {
		switch (kw_value) {
			case kw_DELETE: DoDeleteAttr(ad, it->first); break;
			case kw_RENAME: DoRenameAttr(ad, it->first, it->second.c_str()); break;
			case kw_COPY:   DoCopyAttr(ad, it->first, it->second.c_str()); break;
		}
	}

	return matched.size();
}


static const char * XFormValueToString(classad::Value & val, std::string & tmp)
{
	if ( ! val.IsStringValue(tmp)) {
		classad::ClassAdUnParser unp;
		tmp.clear(); // because Unparse appends.
		unp.Unparse(tmp, val);
	}
	return tmp.c_str();
}

static ExprTree * XFormCopyValueToTree(classad::Value & val)
{
	ExprTree *tree = NULL;
	classad::Value::ValueType vtyp = val.GetType();
	if (vtyp == classad::Value::LIST_VALUE) {
		classad::ExprList * list = NULL;
		val.IsListValue(list);
		tree = list->Copy();
	} else if (vtyp == classad::Value::SLIST_VALUE) {
		classad_shared_ptr<classad::ExprList> list;
		val.IsSListValue(list);
		tree = list.get()->Copy();
	} else if (vtyp == classad::Value::CLASSAD_VALUE) {
		classad::ClassAd* aval;
		val.IsClassAdValue(aval);
		tree = aval->Copy();
	} else {
		tree = classad::Literal::MakeLiteral(val);
	}
	return tree;
}

// this gets called while parsing the submit file to process lines
// that don't look like valid key=value pairs.  
// return 0 to keep scanning the file, ! 0 to stop scanning. non-zero return values will
// be passed back out of Parse_macros
//
int ParseRulesCallback(void* pv, MACRO_SOURCE& source, MACRO_SET& macro_set, char * line, std::string & errmsg)
{
	struct _parse_rules_args * pargs = (struct _parse_rules_args*)pv;
	ClassAd * ad = pargs->ad;
	classad::ClassAdParser parser;
	std::string tmp3;

	parser.SetOldClassAd(true);

	// give the line to our tokener so we can parse it.
	tokener toke(line);
	if ( ! toke.next()) return 0; // keep scanning
	if (toke.matches("#")) return 0; // not expecting this, but in case...

	// get keyword
	const Keyword * pkw = ActionKeywords.find_match(toke);
	if ( ! pkw) {
		std::string tok; toke.copy_token(tok);
		formatstr(errmsg, "%s is not a valid transform keyword\n", tok.c_str());
		return -1;
	}
	// there must be something after the keyword
	if ( ! toke.next()) return -1;

	toke.mark_after(); // in case we want to capture the remainder of the line.

	// the first argument will always be an attribute name
	// in some cases, that attribute name is is allowed to be a regex,
	// if it is a regex it will begin with a /
	std::string attr;
	bool attr_is_regex = false;
	int regex_flags = 0;
	if ((pkw->options & kw_opt_regex) && toke.is_regex()) {
		std::string opts;
		attr_is_regex = true;
		if ( ! toke.copy_regex(attr, regex_flags)) {
			errmsg = "invalid regex";
			return -1;
		}
		regex_flags |= PCRE_CASELESS;
	} else {
		toke.copy_token(attr);
		// if attr ends with , or =, just strip those off. the tokener only splits on whitespace, not other characters
		if (attr.size() > 0 && (attr[attr.size()-1] == ',' || attr[attr.size()-1] == '=')) { attr[attr.size()-1] = 0; }
	}

	// for 2 argument keywords, consume the attribute token and move on to the next one
	if ((pkw->options & kw_opt_argcount_mask) == 2) {
		toke.next();
		if (toke.matches("=") || toke.matches(",")) {
			toke.next();  // consume the keyword token
		}
	}

	// if there is a remainder of the line, macro expand it.
	int off = toke.offset(); // current pointer
	auto_free_ptr rhs(NULL);
	if (off > 0) {
		if (toke.is_quoted_string()) --off;
		rhs.set(expand_macro(line + off, macro_set, LocalContext));
	}

	// at this point
	// pkw   identifies the keyword
	// attr  contains the first word after the keyword, thisis usually an attribute name
	// rhs   has the macro-expanded remainder of the line

	//std::string expr_string;
	//if ( ! toke.at_end()) { toke.copy_to_end(expr_string); }

	switch (pkw->value) {
	// these keywords are envelope information
	case kw_NAME:
		if (dash_verbose) fprintf(stdout, "NAME %s\n", rhs.ptr());
		break;
	case kw_UNIVERSE:
		if (dash_verbose) fprintf(stdout, "UNIVERSE %d\n", CondorUniverseNumberEx(attr.c_str()));
		break;
	case kw_REQUIREMENTS:
		if (dash_verbose) fprintf(stdout, "REQUIREMENTS %s\n", rhs.ptr());
		break;

		// evaluate an expression, then set a value into the ad.
	case kw_EVALMACRO:
		if (dash_verbose) fprintf(stdout, "EVALMACRO %s to %s\n", attr.c_str(), rhs.ptr());
		if ( ! rhs) {
			fprintf(stderr, "ERROR: EVALMACRO %s has no value", attr.c_str());
		} else {
			classad::Value val;
			if ( ! ad->EvaluateExpr(rhs.ptr(), val)) {
				fprintf(stderr, "ERROR: EVALMACRO %s could not evaluate : %s\n", attr.c_str(), rhs.ptr());
			} else {
				XFormValueToString(val, tmp3);
				insert_macro(attr.c_str(), tmp3.c_str(), macro_set, source, LocalContext);
				if (dash_verbose) { fprintf(stdout, "          %s = %s\n", attr.c_str(), tmp3.c_str()); }
			}
		}
		break;

	case kw_TRANSFORM:
		if (pargs->xforms->has_pending_fp()) {
			std::string errmsg;
			// EXPAND_GLOBS_TO_FILES | EXPAND_GLOBS_TO_DIRS
			// EXPAND_GLOBS_WARN_EMPTY | EXPAND_GLOBS_FAIL_EMPTY
			// EXPAND_GLOBS_WARN_DUPS | EXPAND_GLOBS_ALLOW_DUPS 
			int expand_options = EXPAND_GLOBS_WARN_EMPTY;
			int rval = pargs->xforms->parse_iterate_args(rhs.ptr(), expand_options, macro_set, errmsg);
			if (rval < 0) {
				fprintf(stderr, "ERROR: %s\n", errmsg.c_str()); return -1;
			}
		}
		break;

	// these keywords manipulate the ad
	case kw_DEFAULT:
		if (dash_verbose) fprintf(stdout, "DEFAULT %s to %s\n", attr.c_str(), rhs.ptr());
		if (ad->Lookup(attr) != NULL) {
			// this attribute already has a value.
			break;
		}
		// fall through
	case kw_SET:
		if (dash_verbose) fprintf(stdout, "SET %s to %s\n", attr.c_str(), rhs.ptr());
		if ( ! rhs) {
			fprintf(stderr, "ERROR: SET %s has no value", attr.c_str());
		} else {
			ExprTree * expr = NULL;
			if ( ! parser.ParseExpression(rhs.ptr(), expr, true)) {
				fprintf(stderr, "ERROR: SET %s invalid expression : %s\n", attr.c_str(), rhs.ptr());
			} else {
				const bool cache_it = false;
				if ( ! ad->Insert(attr, expr, cache_it)) {
					fprintf(stderr, "ERROR: could not set %s to %s\n", attr.c_str(), rhs.ptr());
					delete expr;
				}
			}
		}
		break;

	case kw_EVALSET:
		if (dash_verbose) fprintf(stdout, "EVALSET %s to %s\n", attr.c_str(), rhs.ptr());
		if ( ! rhs) {
			fprintf(stderr, "ERROR: EVALSET %s has no value", attr.c_str());
		} else {
			classad::Value val;
			if ( ! ad->EvaluateExpr(rhs.ptr(), val)) {
				fprintf(stderr, "ERROR: EVALSET %s could not evaluate : %s\n", attr.c_str(), rhs.ptr());
			} else {
				ExprTree * tree = XFormCopyValueToTree(val);
				const bool cache_it = false;
				if ( ! ad->Insert(attr, tree, cache_it)) {
					fprintf(stderr, "ERROR: could not set %s to %s\n", attr.c_str(), XFormValueToString(val, tmp3));
					delete tree;
				} else if (dash_verbose) {
					fprintf(stdout, "    SET %s to %s\n", attr.c_str(), XFormValueToString(val, tmp3));
				}
			}
		}
		break;

	case kw_DELETE:
	case kw_RENAME:
	case kw_COPY:
		if (attr_is_regex) {
			const char *errptr;
			int erroffset;
			pcre * re = pcre_compile(attr.c_str(), regex_flags, &errptr, &erroffset, NULL);
			if (! re) {
				fprintf(stderr, "ERROR: Error compiling regex '%s'. %s. this entry will be ignored.\n", attr.c_str(), errptr);
			} else {
				DoRegexAttrOp(pkw->value, ad, re, regex_flags, rhs.ptr());
				pcre_free(re);
			}
		} else {
			switch (pkw->value) {
				case kw_DELETE: DoDeleteAttr(ad, attr); break;
				case kw_RENAME: DoRenameAttr(ad, attr, rhs.ptr()); break;
				case kw_COPY:   DoCopyAttr(ad, attr, rhs.ptr()); break;
			}
		}
	}

	return 0; // line handled, keep scanning.
}


// return true from the transform if no longer care about the job ad
// return false to take ownership of the job ad
int ApplyTransform (
	void* pv,
	ClassAd * input_ad)
{
	if ( ! pv) return 1;
	apply_transform_args & xform_args = *(apply_transform_args*)pv;
	const ClassAd * job = input_ad;

	MacroStreamXFormSource & xforms = xform_args.xforms;
	MACRO_SET_CHECKPOINT_HDR* pvParamCheckpoint = xform_args.checkpoint;
	//FILE* outfile = xform_args.outfile;

	_parse_rules_args args = { NULL, NULL, &xforms };

#ifdef TRANSFORM_IN_PLACE
	args.ad = input_ad;
	if (xforms.will_iterate()) { args.ad = new ClassAd(*job); }
#else
	args.ad = new ClassAd(*job);
#endif

	LocalContext.ad = args.ad;
	LocalContext.adname = "MY.";

	// indicate that we are not yet iterating
	(void)sprintf(IteratingString, "%d", 0);

	// the first parse is a keeper only if there is no TRANSFORM statement.
	xforms.rewind();
	int rval = Parse_macros(xforms, 0, LocalMacroSet, READ_MACROS_SUBMIT_SYNTAX, &LocalContext, xform_args.errmsg, ParseRulesCallback, &args);
	if (rval) {
		fprintf(stderr, "Transform of ad %s failed!\n", "");
		return rval;
	}

	// if there is an TRANSFORM, then we want to reparse again with the iterator variables set.
	bool iterating = xforms.first_iteration(LocalMacroSet);
	if ( ! iterating) {
		ReportSuccess(args.ad, xform_args);
	} else {
		while (iterating) {
			delete args.ad;
			LocalContext.ad = args.ad = new ClassAd(*job);

			xforms.rewind();
			rval = Parse_macros(xforms, 0, LocalMacroSet, READ_MACROS_SUBMIT_SYNTAX, &LocalContext, xform_args.errmsg, ParseRulesCallback, &args);
			if (rval < 0)
				break;

			ReportSuccess(args.ad, xform_args);
			iterating = xforms.next_iteration(LocalMacroSet);
		}
	}

#ifdef TRANSFORM_IN_PLACE
	if (xforms.will_iterate()) { delete args.ad; }
#endif
	LocalContext.ad = NULL;
	rewind_macro_set(LocalMacroSet, pvParamCheckpoint, false);

	return rval ? rval : 1; // we return non-zero to allow the job to be deleted
}

#endif // USE_XFORM_UTILS

#ifdef USE_CLASSAD_LIST_WRITER
bool ReportSuccess(const ClassAd * job, apply_transform_args & xform_args)
{
	if ( ! job) return false;
	if (testing.no_output) return true;

	// if we have not yet picked and output format, do that now.
	if (DashOutFormat == ClassAdFileParseType::Parse_auto) {
		if (xform_args.input_helper) {
			fprintf(stderr, "input file format is %d\n", xform_args.input_helper->getParseType());
			DashOutFormat = xform_args.writer.autoSetFormat(*xform_args.input_helper);
		}
	}

	xform_args.writer.writeAd(*job, xform_args.outfile, NULL, DashOutAttrsInHashOrder);
	return true;
}
#else
void write_output_epilog(
	FILE* outfile,
	ClassAdFileParseType::ParseType out_format,
	int cNonEmptyOutputAds)
{
	std::string output;
	switch (out_format) {
	case ClassAdFileParseType::Parse_xml:
		AddClassAdXMLFileFooter(output);
		break;
	case ClassAdFileParseType::Parse_new:
		if (cNonEmptyOutputAds) output = "}\n";
		break;
	case ClassAdFileParseType::Parse_json:
		if (cNonEmptyOutputAds) output = "]\n";
	default:
		// nothing to do.
		break;
	}
	if ( ! output.empty()) { fputs(output.c_str(), outfile); }
}

static int cchReserveForPrintingAds = 16384;
bool ReportSuccess(const ClassAd * job, apply_transform_args & xform_args)
{
	if ( ! job) return false;
	if (testing.no_output) return true;

	StringList * whitelist = NULL;

	// if we have not yet picked and output format, do that now.
	if (DashOutFormat == ClassAdFileParseType::Parse_auto) {
		if (xform_args.input_helper) {
			fprintf(stderr, "input file format is %d\n", xform_args.input_helper->getParseType());
			DashOutFormat = xform_args.input_helper->getParseType();
		}
	}
	if (DashOutFormat < ClassAdFileParseType::Parse_long || DashOutFormat > ClassAdFileParseType::Parse_new) {
		DashOutFormat = ClassAdFileParseType::Parse_long;
	}

	std::string output;
	output.reserve(cchReserveForPrintingAds);

	classad::References attrs;
	classad::References *print_order = NULL;
	if ( ! DashOutAttrsInHashOrder) {
		sGetAdAttrs(attrs, *job, false, whitelist);
		print_order = &attrs;
	}
	switch (DashOutFormat) {
	default:
	case ClassAdFileParseType::Parse_long: {
			if (print_order) {
				sPrintAdAttrs(output, *job, *print_order);
			} else {
				sPrintAd(output, *job);
			}
			if ( ! output.empty()) { output += "\n"; }
		} break;

	case ClassAdFileParseType::Parse_json: {
			classad::ClassAdJsonUnParser  unparser;
			output = cNonEmptyOutputAds ? ",\n" : "[\n";
			if (print_order) {
				PRAGMA_REMIND("fix to call call Unparse with projection when it exists")
				//unparser.Unparse(output, job, print_order);
				unparser.Unparse(output, job);
			} else {
				unparser.Unparse(output, job);
			}
			if (output.size() > 2) {
				output += "\n";
			} else {
				output.clear();
			}
		} break;

	case ClassAdFileParseType::Parse_new: {
			classad::ClassAdUnParser  unparser;
			output = cNonEmptyOutputAds ? ",\n" : "{\n";
			if (print_order) {
				PRAGMA_REMIND("fix to call call Unparse with projection when it exists")
				//unparser.Unparse(output, job, print_order);
				unparser.Unparse(output, job);
			} else {
				unparser.Unparse(output, job);
			}
			if (output.size() > 2) {
				output += "\n";
			} else {
				output.clear();
			}
		} break;

	case ClassAdFileParseType::Parse_xml: {
			classad::ClassAdXMLUnParser  unparser;
			unparser.SetCompactSpacing(false);
			if (0 == cNonEmptyOutputAds) {
				AddClassAdXMLFileHeader(output);
			}
			if (print_order) {
				PRAGMA_REMIND("fix to call call Unparse with projection when it exists")
				//unparser.Unparse(output, job, print_order);
				unparser.Unparse(output, job);
			} else {
				unparser.Unparse(output, job);
			}
			// no extra newlines for xml
			// if ( ! output.empty()) { output += "\n"; }
		} break;
	}

	if ( ! output.empty()) {
		fputs(output.c_str(), xform_args.outfile);
		++cNonEmptyOutputAds;
		cchReserveForPrintingAds = MAX(cchReserveForPrintingAds, (int)output.size());
	}
	return true;
}
#endif


#ifdef USE_XFORM_UTILS
#ifdef USE_CLASSAD_ITERATOR
#ifdef USE_CLASSAD_LIST_WRITER
int DoTransforms(input_file & input, const char * constr, MacroStreamXFormSource & xforms, XFormHash & mset, FILE* outfile, CondorClassAdListWriter &writer)
#else
int DoTransforms(input_file & input, const char * constr, MacroStreamXFormSource & xforms, XFormHash & mset, FILE* outfile)
#endif
{
	int rval = 0;

	classad::ExprTree * constraint = NULL;
	if (constr) {
		if (ParseClassAdRvalExpr(constr, constraint, 0)) {
			fprintf (stderr, "Error parsing constraint expression: %s", constr);
			return -1;
		}
	}

	FILE* file;
	bool close_file = false;
	if (MATCH == strcmp(input.filename, "-")) {
		file = stdin;
		close_file = false;
	} else {
		file = safe_fopen_wrapper_follow(input.filename, "r");
		if (file == NULL) {
			fprintf(stderr, "Can't open file of ClassAds: %s\n", input.filename);
			return false;
		}
		close_file = true;
	}

#ifdef USE_CLASSAD_LIST_WRITER
	apply_transform_args args(xforms, mset, outfile, writer);
#else
	apply_transform_args args(xforms, mset, outfile);
#endif
	args.checkpoint = mset.save_state();

	// TODO: add code to pass arguments between transforms?

	rval = 0;
	CondorClassAdFileIterator adIter;
	if ( ! adIter.begin(file, close_file, input.parse_format)) {
		rval = -1; // unexpected error.
		if (close_file) { fclose(file); file = NULL; }
	} else {
		ClassAd * ad;
		while ((ad = adIter.next(constraint))) {
			rval = ApplyTransform(&args, ad);
			delete ad;
			if (rval < 0)
				break;
		}
	}
	file = NULL;

	mset.rewind_to_state(args.checkpoint, true);
	return rval;
}
#else
int DoTransforms(input_file & input, const char * constraint, MacroStreamXFormSource & xforms, XFormHash & mset, FILE* outfile)
{
	int rval = 0;

	apply_transform_args args(xforms, mset, outfile);
	args.checkpoint = mset.save_state();

	// TODO: add code to pass arguments between transforms?

	CondorQClassAdFileParseHelper jobads_file_parse_helper(input.parse_format);
	args.input_helper = &jobads_file_parse_helper;

	if ( ! read_classad_file(input.filename, jobads_file_parse_helper, ApplyTransform, &args, constraint)) {
		return -1;
	}

	return rval;
}
#endif
#else
int DoTransforms(input_file & input, const char * constraint, MacroStreamXFormSource & xforms, FILE* outfile)
{
	int rval = 0;

	apply_transform_args args(xforms, NULL, outfile);
	args.checkpoint = checkpoint_macro_set(LocalMacroSet);

	// TODO: add code to pass arguments between transforms?

	CondorQClassAdFileParseHelper jobads_file_parse_helper(input.parse_format);
	args.input_helper = &jobads_file_parse_helper;

	if ( ! read_classad_file(input.filename, jobads_file_parse_helper, ApplyTransform, &args, constraint)) {
		return -1;
	}

	return rval;
}
#endif

#ifdef USE_XFORM_UTILS
#else
// attributes of interest to the code that converts old jobrouter routes
enum {
	atr_NAME=1,
	atr_UNIVERSE,
	atr_TARGETUNIVERSE,
	atr_GRIDRESOURCE,
	atr_REQUIREMENTS,
	atr_ENVIRONMENT,
	atr_OSG_ENVIRONMENT,
	atr_ORIG_ENVIRONMENT,
	atr_REQUESTMEMORY,
	atr_REQUESTCPUS,
	atr_ONEXITHOLD,
	atr_ONEXITHOLDREASON,
	atr_ONEXITHOLDSUBCODE,
	atr_REMOTE_QUEUE,
	atr_REMOTE_CEREQUIREMENTS,
	atr_REMOTE_NODENUMBER,
	atr_REMOTE_SMPGRANULARITY,
	atr_INPUTRSL,
	atr_GLOBUSRSL,
	atr_XCOUNT,
	atr_DEFAULT_XCOUNT,
	atr_QUEUE,
	atr_DEFAULT_QUEUE,
	atr_MAXMEMORY,
	atr_DEFAULT_MAXMEMORY,
	atr_MAXWALLTIME,
	atr_DEFAULT_MAXWALLTIME,
	atr_MINWALLTIME,
};

// This must be declared in string sorted order, they do not have to be in enum order.
#define ATR(a, f) { #a, atr_##a, f }
static const Keyword RouterAttrItems[] = {
	ATR(DEFAULT_MAXMEMORY, 0),
	ATR(DEFAULT_MAXWALLTIME, 0),
	ATR(DEFAULT_QUEUE, 0),
	ATR(DEFAULT_XCOUNT, 0),
	ATR(ENVIRONMENT, 0),
	ATR(INPUTRSL, 0),
	ATR(GLOBUSRSL, 0),
	ATR(GRIDRESOURCE, 0),
	ATR(MAXMEMORY, 0),
	ATR(MAXWALLTIME, 0),
	ATR(MINWALLTIME, 0),
	ATR(NAME, 0),
	ATR(ONEXITHOLD, 0),
	ATR(ONEXITHOLDREASON, 0),
	ATR(ONEXITHOLDSUBCODE, 0),
	ATR(ORIG_ENVIRONMENT, 0),
	ATR(OSG_ENVIRONMENT, 0),
	ATR(QUEUE, 0),
	ATR(REMOTE_CEREQUIREMENTS, 0),
	ATR(REMOTE_NODENUMBER, 0),
	ATR(REMOTE_QUEUE, 0),
	ATR(REMOTE_SMPGRANULARITY, 0),
	ATR(REQUESTCPUS, 0),
	ATR(REQUESTMEMORY, 0),
	ATR(REQUIREMENTS, 0),
	ATR(TARGETUNIVERSE, 0),
	ATR(UNIVERSE, 0),
	ATR(XCOUNT, 0),
};
#undef KW
static const KeywordTable RouterAttrs = SORTED_TOKENER_TABLE(RouterAttrItems);

int is_interesting_route_attr(const std::string & attr) {
	tokener toke(attr.c_str()); toke.next();
	const Keyword * patr = RouterAttrs.find_match(toke);
	if (patr) return patr->value;
	return 0;
}

bool same_refs(StringList & refs, const char * check_str)
{
	if (refs.isEmpty()) return false;
	StringList check(check_str);
	return check.contains_list(refs, true);
}

typedef std::map<std::string, std::string, classad::CaseIgnLTStr> STRING_MAP;

int ConvertNextJobRouterRoute(std::string & routing_string, int & offset, const ClassAd & default_route_ad, StringList & statements, int /*options*/)
{
	classad::ClassAdParser parser;
	classad::ClassAdUnParser unparser;

	//bool style_2 = (options & 0x0F) == 2;
	int  has_set_xcount = 0, has_set_queue = 0, has_set_maxMemory = 0, has_set_maxWallTime = 0, has_set_minWallTime = 0;
	//bool has_def_RequestMemory = false, has_def_RequestCpus = false, has_def_remote_queue = false;
	bool has_def_onexithold = false;

	classad::ClassAd ad;
	if ( ! parser.ParseClassAd(routing_string, ad, offset)) {
		return 0;
	}
	ClassAd route_ad(default_route_ad);
	route_ad.Update(ad);

	std::string name, grid_resource, requirements;
	int target_universe = 0;


	STRING_MAP assignments;
	STRING_MAP copy_cmds;
	STRING_MAP delete_cmds;
	STRING_MAP set_cmds;
	STRING_MAP defset_cmds;
	STRING_MAP evalset_cmds;
	classad::References evalset_myrefs;
	classad::References string_assignments;

	for (ClassAd::iterator it = route_ad.begin(); it != route_ad.end(); ++it) {
		std::string rhs;
		if (starts_with(it->first, "copy_")) {
			std::string attr = it->first.substr(5);
			if (route_ad.EvaluateAttrString(it->first, rhs)) {
				copy_cmds[attr] = rhs;
			}
		} else if (starts_with(it->first, "delete_")) {
			std::string attr = it->first.substr(7);
			delete_cmds[attr] = "";
		} else if (starts_with(it->first, "set_")) {
			std::string attr = it->first.substr(4);
			int atrid = is_interesting_route_attr(attr);
			if (atrid == atr_INPUTRSL) {
				// just eat this.
				continue;
			}
			rhs.clear();
			ExprTree * tree = route_ad.Lookup(it->first);
			if (tree) {
				unparser.Unparse(rhs, tree);
				if ( ! rhs.empty()) {
					set_cmds[attr] = rhs;
				}
			}
		} else if (starts_with(it->first, "eval_set_")) {
			std::string attr = it->first.substr(9);
			int atrid = is_interesting_route_attr(attr);
			ExprTree * tree = route_ad.Lookup(it->first);
			if (tree) {
				rhs.clear();
				unparser.Unparse(rhs, tree);
				if ( ! rhs.empty()) {

					StringList myrefs;
					StringList target;
					route_ad.GetExprReferences(rhs.c_str(), &myrefs, &target);
					if (atrid == atr_REQUESTMEMORY || 
						atrid == atr_REQUESTCPUS || atrid == atr_REMOTE_NODENUMBER || atrid == atr_REMOTE_SMPGRANULARITY ||
						atrid == atr_ONEXITHOLD || atrid == atr_ONEXITHOLDREASON || atrid == atr_ONEXITHOLDSUBCODE ||
						atrid == atr_REMOTE_QUEUE) {

						ExprTree * def_tree = default_route_ad.Lookup(it->first);
						bool is_def_expr = (def_tree && *tree == *def_tree);

						/*
						target.create_union(myrefs, true);
						target.remove("null");
						target.remove("InputRSL.maxMemory");
						target.remove("InputRSL.xcount");
						target.remove("InputRSL.queue");
						target.remove("InputRSL");
						*/

						if (atrid == atr_REQUESTMEMORY && is_def_expr) {
							//has_def_RequestMemory = true;
							set_cmds[attr] = "$(maxMemory:2000)";
						} else if ((atrid == atr_REQUESTCPUS || atrid == atr_REMOTE_NODENUMBER || atrid == atr_REMOTE_SMPGRANULARITY) &&
								   is_def_expr) {
							//has_def_RequestCpus = true;
							set_cmds[attr] = "$(xcount:1)";
						} else if ((atrid == atr_ONEXITHOLD || atrid == atr_ONEXITHOLDREASON || atrid == atr_ONEXITHOLDSUBCODE) && is_def_expr) {
							has_def_onexithold = true;
							//evalset_myrefs.insert("minWallTime");
							//evalset_cmds[attr] = rhs;
						} else if (atrid == atr_REMOTE_QUEUE && is_def_expr) {
							//has_def_remote_queue = true;
							set_cmds[attr] = "$Fq(queue)";
						} else {
							evalset_cmds[attr] = rhs;
							for (char* str = myrefs.first(); str; str = myrefs.next()) { evalset_myrefs.insert(str); }
						}
					} else {
						evalset_cmds[attr] = rhs;
						for (char* str = myrefs.first(); str; str = myrefs.next()) { evalset_myrefs.insert(str); }
					}
				}
			}
		} else {
			int atrid = is_interesting_route_attr(it->first);
			switch (atrid) {
				case atr_NAME: route_ad.EvaluateAttrString( it->first, name ); break;
				case atr_TARGETUNIVERSE: route_ad.EvaluateAttrInt( it->first, target_universe ); break;

				case atr_GRIDRESOURCE: {
					route_ad.EvaluateAttrString( it->first, grid_resource );
					ExprTree * tree = route_ad.Lookup(it->first);
					if (tree) {
						rhs.clear();
						unparser.Unparse(rhs, tree);
						if ( ! rhs.empty()) { assignments[it->first] = rhs; string_assignments.insert(it->first); }
					}
				} break;

				case atr_REQUIREMENTS: {
					requirements.clear();
					ExprTree * tree = route_ad.Lookup(it->first);
					if (tree) { unparser.Unparse(requirements, tree); }
				} break;

				default: {
					ExprTree * tree = route_ad.Lookup(it->first);
					bool is_string = true;
					if (tree) {
						rhs.clear();
						if ( ! ExprTreeIsLiteralString(tree, rhs)) {
							is_string = false;
							unparser.Unparse(rhs, tree);
						}
						if ( ! rhs.empty()) { assignments[it->first] = rhs; if (is_string) string_assignments.insert(it->first); }
					}
					switch (atrid) {
						case atr_MAXMEMORY: has_set_maxMemory = 1; break;
						case atr_DEFAULT_MAXMEMORY: if (!has_set_maxMemory) has_set_maxMemory = 2; break;

						case atr_XCOUNT: has_set_xcount = 1; break;
						case atr_DEFAULT_XCOUNT: if(!has_set_xcount) has_set_xcount = 2; break;

						case atr_QUEUE: has_set_queue = 1; break;
						case atr_DEFAULT_QUEUE: if (!has_set_queue) has_set_queue = 2; break;

						case atr_MAXWALLTIME: has_set_maxWallTime = 1; break;
						case atr_DEFAULT_MAXWALLTIME: if (!has_set_maxWallTime) has_set_maxWallTime = 2; break;

						case atr_MINWALLTIME: has_set_minWallTime = 1; break;
					}
				} break;
			}
		}
	}

	// no need to manipulate the job's OnExitHold expression if a minWallTime was not set.
	if (has_def_onexithold /*&& (assignments.find("minWallTime") == assignments.end())*/) {
		// we won't be needing to copy the OnExitHold reason
		copy_cmds.erase("OnExitHold"); evalset_cmds.erase("OnExitHold");
		copy_cmds.erase("OnExitHoldReason"); evalset_cmds.erase("OnExitHoldReason");
		copy_cmds.erase("OnExitHoldSubCode"); evalset_cmds.erase("OnExitHoldSubCode");
	}

	std::string buf;
	formatstr(buf, "# autoconversion of route '%s' from new-classad syntax", name.empty() ? "" : name.c_str());
	statements.append(buf.c_str());
	if ( ! name.empty()) { 
		formatstr(buf, "NAME %s", name.c_str());
		statements.append(buf.c_str());
	}
	if (target_universe) { formatstr(buf, "UNIVERSE %d", target_universe); statements.append(buf.c_str()); }
	if (requirements.empty()) { formatstr(buf, "REQUIREMENTS %s", requirements.c_str()); statements.append(buf.c_str()); }

	statements.append("");
	for (STRING_MAP::iterator it = assignments.begin(); it != assignments.end(); ++it) {
		formatstr(buf, "%s = %s", it->first.c_str(), it->second.c_str());
		statements.append(buf.c_str());
	}

// evaluation order of route rules:
//1. copy_* 
//2. delete_* 
//3. set_* 
//4. eval_set_* 
	statements.append("");
	statements.append("# copy_* rules");
	for (STRING_MAP::iterator it = copy_cmds.begin(); it != copy_cmds.end(); ++it) {
		formatstr(buf, "COPY %s %s", it->first.c_str(), it->second.c_str());
		statements.append(buf.c_str());
	}

	statements.append("");
	statements.append("# delete_* rules");
	for (STRING_MAP::iterator it = delete_cmds.begin(); it != delete_cmds.end(); ++it) {
		formatstr(buf, "DELETE %s", it->first.c_str());
		statements.append(buf.c_str());
	}

	statements.append("");
	statements.append("# set_* rules");
	for (STRING_MAP::iterator it = set_cmds.begin(); it != set_cmds.end(); ++it) {
		formatstr(buf, "SET %s %s", it->first.c_str(), it->second.c_str());
		statements.append(buf.c_str());
	}

	if (has_def_onexithold) {
		// emit new boilerplate on_exit_hold
		if (has_set_minWallTime) {
			statements.append("");
			statements.append("# modify OnExitHold for minWallTime");
			statements.append("if defined MY.OnExitHold");
			statements.append("  COPY OnExitHold orig_OnExitHold");
			statements.append("  COPY OnExitHoldSubCode orig_OnExitHoldSubCode");
			statements.append("  COPY OnExitHoldReason orig_OnExitHoldReason");
			statements.append("  DEFAULT orig_OnExitHoldReason strcat(\"The on_exit_hold expression (\", unparse(orig_OnExitHold), \") evaluated to TRUE.\")");
			statements.append("  SET OnExitHoldMinWallTime ifThenElse(RemoteWallClockTime isnt undefined, RemoteWallClockTime < 60*$(minWallTime), false)");
			statements.append("  SET OnExitHoldReasonMinWallTime strcat(\"The job's wall clock time\", int(RemoteWallClockTime/60), \"min, is is less than the minimum specified by the job ($(minWallTime))\")");
			statements.append("  SET OnExitHold orig_OnExitHold || OnExitHoldMinWallTime");
			statements.append("  SET OnExitHoldSubCode ifThenElse(orig_OnExitHold, $(My.orig_OnExitHoldSubCode:1), 42)");
			statements.append("  SET OnExitHoldReason ifThenElse(orig_OnExitHold, orig_OnExitHoldReason, ifThenElse(OnExitHoldMinWallTime, OnExitHoldReasonMinWallTime, \"Job held for unknown reason.\"))");
			statements.append("else");
			statements.append("  SET OnExitHold ifThenElse(RemoteWallClockTime isnt undefined, RemoteWallClockTime < 60*$(minWallTime), false)");
			statements.append("  SET OnExitHoldSubCode 42");
			statements.append("  SET OnExitHoldReason strcat(\"The job's wall clock time\", int(RemoteWallClockTime/60), \"min, is is less than the minimum specified by the job ($(minWallTime))\")");
			statements.append("endif");
		}
	}

	// we can only do this after we have read the entire input ad.
	int cSpecial = 0;
	for (classad::References::const_iterator it = evalset_myrefs.begin(); it != evalset_myrefs.end(); ++it) {
		if (assignments.find(*it) != assignments.end() && set_cmds.find(*it) == set_cmds.end()) {
			if ( ! cSpecial) { statements.append(""); statements.append("# temporarily SET attrs because eval_set_ rules refer to them"); }
			++cSpecial;
			if (string_assignments.find(*it) != string_assignments.end()) {
				formatstr(buf, "SET %s \"$(%s)\"", it->c_str(), it->c_str());
			} else {
				formatstr(buf, "SET %s $(%s)", it->c_str(), it->c_str());
			}
			statements.append(buf.c_str());
		}
	}

	statements.append("");
	statements.append("# eval_set_* rules");
	for (STRING_MAP::iterator it = evalset_cmds.begin(); it != evalset_cmds.end(); ++it) {
		formatstr(buf, "EVALSET %s %s", it->first.c_str(), it->second.c_str());
		statements.append(buf.c_str());
	}

	if (cSpecial) {
		statements.append("");
		statements.append("# remove temporary attrs");
		for (classad::References::const_iterator it = evalset_myrefs.begin(); it != evalset_myrefs.end(); ++it) {
			if (assignments.find(*it) != assignments.end() && set_cmds.find(*it) == set_cmds.end()) {
				formatstr(buf, "DELETE %s", it->c_str());
				statements.append(buf.c_str());
			}
		}
	}

	return 1;
}

#endif

bool LoadJobRouterDefaultsAd(ClassAd & ad)
{
	bool merge_defaults = param_boolean("MERGE_JOB_ROUTER_DEFAULT_ADS", false);
	bool routing_enabled = false;

	classad::ClassAd router_defaults_ad;
	std::string router_defaults;
	if (param(router_defaults, "JOB_ROUTER_DEFAULTS") && ! router_defaults.empty()) {
		// if the param doesn't start with [, then wrap it in [] before parsing, so that the parser knows to expect new classad syntax.
		if (router_defaults[0] != '[') {
			router_defaults.insert(0, "[ ");
			router_defaults.append(" ]");
			merge_defaults = false;
		}
		int length = (int)router_defaults.size();
		classad::ClassAdParser parser;
		int offset = 0;
		if ( ! parser.ParseClassAd(router_defaults, router_defaults_ad, offset)) {
			dprintf(D_ALWAYS|D_ERROR,"JobRouter CONFIGURATION ERROR: Disabling job routing, failed to parse at offset %d in %s classad:\n%s\n",
				offset, "JOB_ROUTER_DEFAULTS", router_defaults.c_str());
			routing_enabled = false;
		} else if (merge_defaults && (offset < length)) {
			// whoh! we appear to have received multiple classads as a hacky way to append to the defaults ad
			// so go ahead and parse the remaining ads and merge them into the defaults ad.
			do {
				// skip trailing whitespace and ] and look for an open [
				bool parse_err = false;
				for ( ; offset < length; ++offset) {
					int ch = router_defaults[offset];
					if (ch == '[') break;
					if ( ! isspace(router_defaults[offset]) && ch != ']') {
						parse_err = true;
						break;
					}
					// TODO: skip comments?
				}

				if (offset < length && ! parse_err) {
					classad::ClassAd other_ad;
					if ( ! parser.ParseClassAd(router_defaults, other_ad, offset)) {
						parse_err = true;
					} else {
						router_defaults_ad.Update(other_ad);
					}
				}

				if (parse_err) {
					routing_enabled = false;
					dprintf(D_ALWAYS|D_ERROR,"JobRouter CONFIGURATION ERROR: Disabling job routing, failed to parse at offset %d in %s ad : \n%s\n",
							offset, "JOB_ROUTER_DEFAULTS", router_defaults.substr(offset).c_str());
					break;
				}
			} while (offset < length);
		}
	}
	ad.CopyFrom(router_defaults_ad);
	return routing_enabled;
}

int ConvertJobRouterRoutes(int options)
{
	ClassAd default_route_ad;
	std::string routes_string;

	LoadJobRouterDefaultsAd(default_route_ad);

	auto_free_ptr routing(param("JOB_ROUTER_ENTRIES"));
	if (routing) {
		routes_string = routing.ptr();
		int offset = 0;
		int route_index = 0;
		while (offset < (int)routes_string.size()) {
			++route_index;
#ifdef USE_XFORM_UTILS
			MacroStreamXFormSource xfm;
			if (XFormLoadFromJobRouterRoute(xfm, routes_string, offset, default_route_ad, options)) {
				fprintf(stdout, "\n##### JOB_ROUTER_ENTRIES [%d] #####\n", route_index);
				std::string xfm_text;
				fputs(xfm.getFormattedText(xfm_text,"", true), stdout);
#else
			StringList lines;
			if (XFormLoadFromJobRouterRoute(routes_string, offset, default_route_ad, lines, options)) {
				fprintf(stdout, "##### JOB_ROUTER_ENTRIES [%d] #####\n", route_index);
				for (char * line = lines.first(); line; line = lines.next()) {
					fprintf(stdout, "%s\n", line);
				}
#endif
				fprintf(stdout, "\n");
			}
		}
	}

	routing.set(param("JOB_ROUTER_ENTRIES_FILE"));
	if (routing) {
		FILE *fp = safe_fopen_wrapper_follow(routing.ptr(),"r");
		if( !fp ) {
			fprintf(stderr, "Failed to open '%s' file specified for JOB_ROUTER_ENTRIES_FILE", routing.ptr());
		} else {
			routes_string.clear();
			char buf[200];
			int n;
			while( (n=fread(buf,1,sizeof(buf)-1,fp)) > 0 ) {
				buf[n] = '\0';
				routes_string += buf;
			}
			fclose( fp );

			int offset = 0;
			int route_index = 0;
			while (offset < (int)routes_string.size()) {
				++route_index;
#ifdef USE_XFORM_UTILS
				MacroStreamXFormSource xfm;
				if (XFormLoadFromJobRouterRoute(xfm, routes_string, offset, default_route_ad, options)) {
					fprintf(stdout, "\n##### JOB_ROUTER_ENTRIES_FILE [%d] #####\n", route_index);
					std::string xfm_text;
					fputs(xfm.getFormattedText(xfm_text,"", true), stdout);
#else
				StringList lines;
				if (XFormLoadFromJobRouterRoute(routes_string, offset, default_route_ad, lines, options)) {
					fprintf(stdout, "##### JOB_ROUTER_ENTRIES_FILE [%d] #####\n", route_index);
					for (char * line = lines.first(); line; line = lines.next()) {
						fprintf(stdout, "%s\n", line);
					}
#endif
					fprintf(stdout, "\n");
				}
			}
		}
	}

	return 0;
}

int DoUnitTests(int /*options*/)
{
	return 0;
}

