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
#include "classad_helpers.h"
#include "condor_regex.h"
#include "tokener.h"
#include <submit_utils.h>
#include <xform_utils.h>
#include <my_async_fread.h>

#include <string>
#include <set>
#include <deque>

#define USE_CLASSAD_ITERATOR 1
#define USE_CLASSAD_LIST_WRITER 1

bool dash_verbose = false;
bool dash_terse = false;
int  UnitTestOpts = 0;
int  DashConvertOldRoutes = 0;
const char * OldRoutesFilename = nullptr;
bool DumpLocalHash = false;
bool DumpClassAdToFile = false;
bool DumpFileIsStdout = false;
bool DashOutAttrsInHashOrder = false;
ClassAdFileParseType::ParseType DashOutFormat = ClassAdFileParseType::Parse_Unspecified;
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
	apply_transform_args(std::deque<MacroStreamXFormSource> & xfm, XFormHash & ms, FILE* out, CondorClassAdListWriter & writ)
		: xforms(xfm), mset(ms), checkpoint(NULL), outfile(out), writer(writ), input_helper(NULL) {}
	std::deque<MacroStreamXFormSource> & xforms;
	XFormHash & mset;
	MACRO_SET_CHECKPOINT_HDR* checkpoint;
	FILE* outfile;
	CondorClassAdListWriter & writer;
	CondorClassAdFileParseHelper *input_helper;
	std::string errmsg;
};



// forward function references
void Usage(FILE*);
void PrintRules(FILE*);
void PrintConvert(FILE*);
int DoUnitTests(int options, std::deque<input_file> & inputs);
int ApplyTransforms(void *pv, ClassAd * job);
bool ReportSuccess(const ClassAd * job, apply_transform_args & xform_args);
int DoTransforms(
	input_file & input,
	const char * constr,
	std::deque<MacroStreamXFormSource> & xforms,
	XFormHash & mset,
	FILE* outfile,
	CondorClassAdListWriter &writer);
bool LoadJobRouterDefaultsAd(ClassAd & ad, const std::string & router_defaults, bool merge_defaults);


//#define CONVERT_JRR_STYLE_1 0x0001
//#define CONVERT_JRR_STYLE_2 0x0002
int ConvertJobRouterRoutes(int options, const char * config_file);

MACRO_SOURCE FileMacroSource = { false, false, 0, 0, -1, -2 };

int
main( int argc, const char *argv[] )
{
	const char *pcolon = NULL;
	ClassAdFileParseType::ParseType def_ads_format = ClassAdFileParseType::Parse_Unspecified;
	std::deque<input_file> inputs;
	std::deque<const char *> rules;
	std::deque<const char *> cmdline_keys; // key=value pairs on the command line
	std::vector<std::string> config_rules; config_rules.reserve(argc);  // storage for "rules" that are really config references
	const char *job_match_constraint = NULL;

	MyName = condor_basename(argv[0]);
	if (argc == 1) {
		Usage(stderr);
		return 1;
	}

	setbuf( stdout, NULL );

	set_mySubSystem( MySubsys, false, SUBSYSTEM_TYPE_TOOL );

	set_priv_initialize(); // allow uid switching if root
	config();

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
			} else if (is_dash_arg_colon_prefix(ptr[0], "debug", &pcolon, 2)) {
				// dprintf to console
				dprintf_set_tool_debug("TOOL", (pcolon && pcolon[1]) ? pcolon+1 : nullptr);
			} else if (is_dash_arg_prefix(ptr[0], "rules", 1)) {
				const char * pfilearg = ptr[1];
				if ( ! pfilearg || (*pfilearg == '-' && (MATCH != strcmp(pfilearg,"-"))) ) {
					fprintf( stderr, "%s: -rules requires another argument\n", MyName );
					exit(1);
				}
				rules.push_back(pfilearg);
				++ixArg; ++ptr;
			} else if (is_dash_arg_prefix(ptr[0], "jobtransforms", 4)) {
				const char * xformlist = ptr[1];
				if ( ! xformlist || *xformlist == '-') {
					fprintf( stderr, "%s: -jobtransforms requires another argument\n", MyName );
					exit(1);
				}
				// argument is list of schedd transform names
				config_rules.push_back(std::string("config://JOB_TRANSFORM_%s/")+xformlist);
				rules.push_back(config_rules.back().c_str());
				++ixArg; ++ptr;
			} else if (is_dash_arg_prefix(ptr[0], "jobroute", 4)) {
				const char * route = ptr[1];
				if ( ! route || *route == '-') {
				fprintf( stderr, "%s: -jobroute requires another argument\n", MyName );
				exit(1);
				}
				// argument is list of schedd transform names
				config_rules.push_back(std::string("config://JOB_ROUTER_ROUTE_%s/")+route);
				rules.push_back(config_rules.back().c_str());
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
					for (auto& opt: StringTokenIterator(++pcolon)) {
						if (YourString(opt) == "nosort") {
							DashOutAttrsInHashOrder = true;
						} else {
							out_format = parseAdsFileFormat(opt.c_str(), out_format);
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
					if (inputs[ii].parse_format == ClassAdFileParseType::Parse_Unspecified) {
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
					for (auto& opt: StringTokenIterator(++pcolon)) {
						if (is_arg_colon_prefix(opt.c_str(), "repeat", &pcolon, 3)) {
							testing.repeat_count = (pcolon) ? atoi(++pcolon) : 100;
						} else if (is_arg_prefix(opt.c_str(), "nooutput", 5)) {
							testing.no_output = true;
						}
					}
				}
			} else if (is_dash_arg_prefix(ptr[0], "help", 1)) {
				if (ptr[1] && (MATCH == strcmp(ptr[1], "rules"))) {
					PrintRules(stdout);
					exit(0);
				} else if (ptr[1] && (is_arg_prefix(ptr[1], "convert", 4))) {
					PrintConvert(stdout);
				}
				Usage(stdout);
				exit(0);
			} else if (is_dash_arg_colon_prefix(ptr[0], "convertoldroutes", &pcolon, 4)) {
				if (pcolon) {
					DashConvertOldRoutes = atoi(++pcolon);
					if ( ! DashConvertOldRoutes) {
						for (const auto & str : StringTokenIterator(pcolon)) {
							if (is_arg_prefix(str.c_str(), "file")) {
								const char * pfilearg = ptr[1];
								DashConvertOldRoutes = 1;
								if ( ! pfilearg || (*pfilearg == '-' && (MATCH != strcmp(pfilearg,"-"))) ) {
									fprintf( stderr, "%s: -%s requires another argument\n", MyName, ptr[0] );
									PrintConvert(stderr);
									exit(1);
								}
								OldRoutesFilename = pfilearg;
								++ixArg; ++ptr;
							} else {
								fprintf( stderr, "%s: -%s:%s unknown option\n", MyName, ptr[0], str.c_str() );
								exit(1);
							}
						}
					}
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
			//std::string value(name.substr(ix+1)); trim(value);
			name = name.substr(0,ix);
			trim(name);
			if ( ! name.empty() && name[1] == '+') {
				name = "MY." + name.substr(1);
			}
			if (name.empty() || ! is_valid_param_name(name.c_str())) {
				fprintf( stderr, "%s: invalid attribute name '%s' for attrib=value assigment\n", MyName, name.c_str() );
				exit(1);
			}
			cmdline_keys.push_back(ptr[0]);
			//MACRO_EVAL_CONTEXT ctx; ctx.init(MySubsys,0);
			//xform_hash.set_arg_variable(name.c_str(), value.c_str(), ctx);
		} else {
			inputs.push_back(input_file(*ptr, def_ads_format));
		}
	}

	XFormHash xform_hash;
	xform_hash.init();

	// inject cmdline key=value pairs before we parse any transform rules
	if ( ! cmdline_keys.empty()) {
		MACRO_EVAL_CONTEXT ctx; ctx.init(MySubsys,0);
		for (auto kvp : cmdline_keys) {
			const char * eql = strchr(kvp, '=');
			std::string name(kvp, eql - kvp); trim(name);
			if (name[1] == '+') { name = "MY." + name.substr(1); }
			std::string value(kvp+1); trim(value);
			xform_hash.set_arg_variable(name.c_str(), value.c_str(), ctx);
		}
	}

	// the -convertoldroutes argument tells us to just read and
	if (DashConvertOldRoutes) { exit(ConvertJobRouterRoutes(DashConvertOldRoutes, OldRoutesFilename)); }

	// if there are any unit tests, do them and then exit.
	if (UnitTestOpts > 0) { exit(DoUnitTests(UnitTestOpts, inputs)); }

	std::deque<MacroStreamXFormSource> xforms;

	// read the transform rules into the MacroStreamXFormSource
	int rval = 0;
	std::string errmsg = "";
	if (rules.empty()) {
		fprintf(stderr, "ERROR : no transform rules specified.\n");
		Usage(stderr);
		exit(1);
	}

	// lookup context for config: rules so that
	// we can pick up a template from config or from a previous rules file.
	MACRO_EVAL_CONTEXT rules_ctx;
	rules_ctx.init(MySubsys);
	rules_ctx.also_in_config = true;
	// a temporary hashtable used when validating rules files
	XFormHash rules_hash;
	rules_hash.init();

	bool iterating_rules = false;
	for (auto ptr : rules) {
		// if we already have an iterating rule, then it is an error to have more rules
		if (iterating_rules) {
			fprintf(stderr, "Error at %s. Previous transform was an iterating transform\n", ptr);
			exit(1);
		}

		if (MATCH == strcmp("-", ptr)) {
			xform_hash.set_RulesFile("<stdin>", FileMacroSource);
			xforms.emplace_back("<stdin>");
			MacroStreamXFormSource & ms = xforms.back();
			rval = ms.load(stdin, FileMacroSource, errmsg);
		} else if (starts_with(ptr, "config://")) {
			ptr+= 9; // skip config://

			// knob pattern is between // and next /
			std::string knob, knob_pattern;
			const char * list = strchr(ptr,'/');
			if (list) { 
				knob_pattern.assign(ptr, list-ptr);
				++list;
			} else { 
				knob_pattern = ptr;
			}

			StringTokenIterator it(list); // note, list may be null here
			const char * name = it.first();
			if ( ! name || ! name[0]) name = "_"; // use a dummy name for an implict single item list

			for ( ; name != nullptr; name = it.next()) {
				formatstr(knob, knob_pattern.c_str(), name);
				// read transform statements from the config or from a previous rules file
				// if the rules_hash here does not find the desired transform, the rules_ctx
				// variable will tell the lookup to also look in the global config.
				const char * raw_text = rules_hash.lookup(knob.c_str(), rules_ctx);
				if (raw_text) {
					while (isspace(*raw_text)) ++raw_text;

					xforms.emplace_back(name);
					MacroStreamXFormSource & ms = xforms.back();

					std::string errmsg = "";
					int offset = 0;
					int rval = 0;
					if (*raw_text == '[') {
						// This is an old syntax (new-classad) transform. Reload the transform with $() expansions
						// then parse it as a classad
						std::string text;
						param(text, knob.c_str());
						classad::ClassAdParser parser;
						ClassAd transformAd;

						rval = 0;
						if (!parser.ParseClassAd(text, transformAd, offset)) {
							rval = -1;
							formatstr(errmsg,"Could not parse transform classad %s", name);
						} else {
							rval = XFormLoadFromClassadJobRouterRoute(ms,name,offset,transformAd,0);
						}
					} else {
						rval = ms.open(raw_text, offset, errmsg);
					}
					if (rval < 0) {
						fprintf(stderr, "%s invalid transform text, ignoring. (err=%d) %s\n",
							knob.c_str(), rval, errmsg.c_str() );
						xforms.pop_back();
					}
				}
			}
		} else {
			FILE *file = safe_fopen_wrapper_follow(ptr, "r");
			if (file == NULL) {
				fprintf(stderr, "Can't open rules file: %s\n", ptr);
				return -1;
			}
			xform_hash.set_RulesFile(ptr, FileMacroSource);

			xforms.emplace_back(ptr);
			MacroStreamXFormSource & ms = xforms.back();
			rval = ms.load(file, FileMacroSource, errmsg);
			if (rval < 0 || ! ms.close_when_done(true)) {
				fclose(file);
			}
		}

		// did we just add an iterating rule set?
		// then promote the xform hash to iterating
		// and set a global flag to remember that fact
		if ( ! xforms.empty()) {
			MacroStreamXFormSource & ms = xforms.back();
			if (ms.iterate_init_pending()) {
				xform_hash.set_flavor(XFormHash::Flavor::Iterating);
				iterating_rules = true;
			} else {
				// if the transform rules did not contain an iterator, then it might be a config file
				// We can figure out if it is by validating the transform rules. This will return a 
				// count of transform statements, and also populate the rules_hash with variable declarations
				// And we can use the rules_hash variables to populate a subsequent "config://" rule
				int step_count = 0;
				if ( ! ValidateXForm(ms, rules_hash, &step_count, errmsg)) {
					fprintf(stderr, "Invalid rules file %s : %s\n", ms.getName(), errmsg.c_str());
				}
				if ( ! step_count) {
					// this was just a config file, and the config statements have been
					// merged into the rules_hash so we don't need it anymore.
					xforms.pop_back();
				}
			}
		}
	}

	if (rval < 0) {
		fprintf(stderr, "ERROR reading transform rules from %s : %s\n",
			xform_hash.get_RulesFilename(),
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
	if (def_ads_format == ClassAdFileParseType::Parse_Unspecified) { def_ads_format = ClassAdFileParseType::Parse_auto; }
	// if no output parse format has been specified, choose long
	if (DashOutFormat == ClassAdFileParseType::Parse_Unspecified) { DashOutFormat = ClassAdFileParseType::Parse_long; }

	CondorClassAdListWriter writer(DashOutFormat);

	for (size_t ixInput = 0; ixInput < inputs.size(); ++ixInput) {
		// use default parse format if input file still has an unspecifed one.
		if (inputs[ixInput].parse_format == ClassAdFileParseType::Parse_Unspecified) { inputs[ixInput].parse_format = def_ads_format; }
		if (dash_verbose) { fprintf(stderr, "# Applying transforms to ads from %s\n", inputs[ixInput].filename); }
		rval = DoTransforms(inputs[ixInput], job_match_constraint, xforms, xform_hash, outfile, writer);
	}

	writer.writeFooter(outfile);
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
		"Usage: %s [transforms] [options] [<key>=<value>] <infile> [<infile>]\n"
		"    Read ClassAd(s) from <infile> and transform based on rules from <rules-file>\n\n", MyName);
	fprintf(out,
		"    [transforms] are one or more of\n"
		"\t-rules <rules-file>          Read transform rules from <rules-file>. see -help for the format\n"
		"\t-jobtransforms <xform-names> Apply the Schedd JOB_TRANSFORM_* transforms listed in <xform-names>\n"
		"\t-jobroute <route-name>       Apply the JOB_ROUTER_ROUTE_* transform listed in <route-name>\n"
		"\n    [options] are\n"
		"\t-help [rules | convert]\t\t Display this screen or rules or convert documentation and exit\n"
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
		"\t-verbose\t\t Verbose mode, echo transform rules to stderr as they are executed\n"
		//"\t-debug  \t\tDisplay debugging output\n"
		"\n    <key>=<value> Arguments are assigned before the rules file is parsed and can be used\n"
		"                  to pass arguments or enable optional behavior in the rules\n\n"
		"    If <rules-file> is config://<var>/ then the <var> configuration variable is read\n"
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
		"   REQUIREMENTS <expr>\t\t- Apply the transform only if <expr> evaluates to true\n"
		"   TRANSFORM [<N>] [<vars>] [in <list> | from <file> | matching <pattern>] - Do the Tranform\n"
		);

	fprintf(out,
		"\nIn the above commands <attr> must be a valid attribute name and <expr> a valid ClassAd\n"
		"expression or literal.  The various $() macros will be expanded in <expr>, <newattr> or\n"
		"<newattrs> before they are parsed as ClassAd expressions or attribute names.\n"
		"\nWhen a COPY,RENAME or DELETE with regex is used, regex capture groups are substituted in\n"
		"<newattrs> after $() expansion. \\0 will expand to the entire match, \\1 to the first capture, etc.\n"
		"\nIf a TRANSFORM command is used. it must be the last command in the rules file. It takes the same\n"
		"options as the QUEUE statement from a HTCONDOR submit file. There is an implicit TRANSFORM 1 at the\n"
		"end of a rules file if there is no explicit TRANSFORM command.\n"
		"A rules file may be a HTCondor configuration file that defines JOB_TRANSFORM_* or JOB_ROUTER_ROUTE_*\n"
		"variables.  A rules file of this form will be loaded, but will have no effect unless a later\n" 
		"-jobtransforms or -jobroute argument refers to one of the transforms it defines.\n"
		);

	fprintf(out, "\n");
}

void PrintConvert(FILE* out)
{
	fprintf(out, "\nConverting old JobRouter routes:\n"
		"\nTo convert old JobRouter routes use either \n"
		"   condor_transform_ads -convert\n"
		"   or\n"
		"   condor_transform_ads -convert:file {summary-file}\n"
		"\nThe first form converts routes from the current configuration. The second form\n"
		"convert routes from a file containing the summary of a foreign configuration.\n"
		"Use condor_config_val -summary > {summary-file} to capture the configuration summary file\n"
	);

	fprintf(out,
		"\nThe output of these commands will be the routes from the JOB_ROUTER_ENTRIES list\n"
		"printed as a set of JOB_ROUTER_ROUTE_<name> transforms.\n"
		"The converted routes will require hand editing to remove old syntax idioms like strcat()\n"
		"and temporary variables stored in the job classad. If the CE config has a customized\n"
		"JOB_ROUTER_DEFAULTS, the converted routes may be incomplete.\n"
	);

	fprintf(out, "\n");
}

#define TRANSFORM_IN_PLACE 1

// return true from the transform if no longer care about the job ad
// return false to take ownership of the job ad
int ApplyTransforms (
	void* pv,
	ClassAd * input_ad)
{
	if ( ! pv) return 1;
	apply_transform_args & args = *(apply_transform_args*)pv;
	//const ClassAd * job = input_ad;

	unsigned int flags = XFORM_UTILS_LOG_ERRORS | (dash_verbose ? XFORM_UTILS_LOG_STEPS : 0);

	// TODO: defer expansion of iterate args until the first SET statement?

	int rval = args.xforms.back().will_iterate(args.mset, args.errmsg);
	if (rval < 0) {
		fprintf(stderr, "ERROR: %s\n", args.errmsg.c_str()); return -1;
		return 1;
	}
	bool will_iterate = rval != 0;
	if (will_iterate) {
		will_iterate = args.xforms.back().first_iteration(args.mset);
	}

	if (will_iterate) {
		bool iterating = true;
		while (iterating) {
			ClassAd * ad_to_transform = new ClassAd(*input_ad);
			rval = 0;
			for (auto &xfm : args.xforms) {
				bool match = xfm.matches(ad_to_transform);
				if (dash_verbose) { fprintf(stderr, "#   Transform %s %s\n", xfm.getName(), match ? "" : "Skipped"); }
				if ( ! match) continue;
				int rv = TransformClassAd(ad_to_transform, xfm, args.mset, args.errmsg, flags);
				if (rv < 0) rval = rv;
			}
			if (rval >= 0) {
				ReportSuccess(ad_to_transform, args);
				iterating = args.xforms.back().next_iteration(args.mset);
			} else {
				iterating = false;
			}
			delete ad_to_transform;
		}
		args.xforms.back().clear_iteration(args.mset);
	} else {
		rval = 0;
		for (auto &xfm : args.xforms) {
			bool match = xfm.matches(input_ad);
			if (dash_verbose) { fprintf(stderr, "#   Transform %s %s\n", xfm.getName(), match ? "" : "Skipped"); }
			if ( ! match) continue;
			int rv = TransformClassAd(input_ad, xfm, args.mset, args.errmsg, flags);
			if (rv < 0) rval = rv;
		}
		if (rval) {
			return rval;
		}
		ReportSuccess(input_ad, args);
	}

	args.mset.rewind_to_state(args.checkpoint, false);

	return rval ? rval : 1; // we return non-zero to allow the job to be deleted
}

bool ReportSuccess(const ClassAd * job, apply_transform_args & xform_args)
{
	if ( ! job) return false;
	if (testing.no_output) return true;

	// if we have not yet picked and output format, do that now.
	if (DashOutFormat == ClassAdFileParseType::Parse_auto) {
		if (xform_args.input_helper) {
			fprintf(stderr, "input file format is %ld\n", (long)xform_args.input_helper->getParseType());
			DashOutFormat = xform_args.writer.autoSetFormat(*xform_args.input_helper);
		}
	}

	xform_args.writer.writeAd(*job, xform_args.outfile, NULL, DashOutAttrsInHashOrder);
	return true;
}

int LogAdIdentity(FILE * out, int num, const ClassAd & ad, const char * prefix)
{
	JOB_ID_KEY jid;
	std::string name;
	if (ad.LookupString(ATTR_NAME, name)) {
	} else if (ad.LookupInteger(ATTR_CLUSTER_ID, jid.cluster) && ad.LookupInteger(ATTR_PROC_ID, jid.proc)) {
		name = "Job ";
		name += jid;
	} else {
		formatstr(name, "Ad %d", num);
	}
	return fprintf(out, "%s %s\n", prefix, name.c_str());
}

int DoTransforms(
	input_file & input,
	const char * constr,
	std::deque<MacroStreamXFormSource> & xforms,
	XFormHash & mset,
	FILE* outfile,
	CondorClassAdListWriter &writer)
{
	int rval = 0, num_ads = 0;

	classad::ExprTree * constraint = NULL;
	if (constr) {
		if (ParseClassAdRvalExpr(constr, constraint)) {
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

	apply_transform_args args(xforms, mset, outfile, writer);
	args.checkpoint = mset.save_state();

	rval = 0;
	CondorClassAdFileIterator adIter;
	if ( ! adIter.begin(file, close_file, input.parse_format)) {
		rval = -1; // unexpected error.
		if (close_file) { fclose(file); file = NULL; }
	} else {
		ClassAd * ad;
		while ((ad = adIter.next(constraint))) {
			++num_ads;
			if (dash_verbose) { LogAdIdentity(stderr, num_ads, *ad, "#  "); }
			rval = ApplyTransforms(&args, ad);
			delete ad;
			if (rval < 0)
				break;
		}
	}
	file = NULL;

	mset.rewind_to_state(args.checkpoint, true);
	return rval;
}

bool LoadJobRouterDefaultsAd(ClassAd & ad, std::string & router_defaults, bool merge_defaults)
{
	bool routing_enabled = false;

	classad::ClassAd router_defaults_ad;
	if ( ! router_defaults.empty()) {
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

int ConvertJobRouterRoutes(int options, const char * config_file)
{
	ClassAd default_route_ad;
	ClassAd generated_default_route_ad;
	std::string routes_string;
	std::string routes_list;

	auto_free_ptr routing;

	MACRO_EVAL_CONTEXT ctx; ctx.init("JOB_ROUTER");
	XFormHash ceconfig(XFormHash::Flavor::ParamTable);
	if (config_file) {
		MacroStreamFile ms;
		std::string errmsg;
		if ( ! ms.open(config_file, false, ceconfig.macros(), errmsg)) {
			fprintf(stderr, "Failed to open ceconfig file '%s'", config_file);
			return 1;
		}
		int rval = Parse_macros(ms, 0, ceconfig.macros(), 0, &ctx, errmsg, nullptr, nullptr);
		ms.close(ceconfig.macros(), rval);

		options |= 4; //XForm_ConvertJobRouter_Old_CE;

		bool merge_defaults = ceconfig.local_param_bool("MERGE_JOB_ROUTER_DEFAULT_ADS", false, ctx);
		routing.set(ceconfig.local_param("JOB_ROUTER_DEFAULTS", ctx));
		if (routing) {
			std::string val(routing.ptr());
			LoadJobRouterDefaultsAd(default_route_ad, val, merge_defaults);
		}
		routing.set(ceconfig.local_param("JOB_ROUTER_DEFAULTS_GENERATED", ctx));
		if (routing) {
			std::string val(routing.ptr());
			LoadJobRouterDefaultsAd(generated_default_route_ad, val, false);
		}

	#if 0
		// TODO: compare generated defaults to defaults
		default_route_ad.Clear();
	#else
		for (auto & it : generated_default_route_ad) {
			ExprTree * tree = default_route_ad.Lookup(it.first);
			if (tree && *tree == *it.second) {
				default_route_ad.Delete(it.first);
			}
		}
	#endif

		const char * entries = ceconfig.lookup("JOB_ROUTER_ENTRIES", ctx);
		routing.set(strdup(entries?entries:""));
	} else {
		bool merge_defaults = param_boolean("MERGE_JOB_ROUTER_DEFAULT_ADS", false);
		std::string router_defaults;
		if (param(router_defaults, "JOB_ROUTER_DEFAULTS") && ! router_defaults.empty()) {
			LoadJobRouterDefaultsAd(default_route_ad, router_defaults, merge_defaults);
		}
		routing.set(param("JOB_ROUTER_ENTRIES"));
	}

	if (routing) {
		routes_string = routing.ptr();
		int offset = 0;
		int route_index = 1;
		while (offset < (int)routes_string.size()) {
			MacroStreamXFormSource xfm;
			if (XFormLoadFromClassadJobRouterRoute(xfm, routes_string, offset, default_route_ad, options) >= 0) {
				if (config_file) {
					std::string tag(xfm.getName());
					if ( ! tag.empty()) {
						cleanStringForUseAsAttr(tag);
						if (!routes_list.empty()) routes_list += ' ';
						routes_list += tag;
						fprintf(stdout, "\n##### Route %d\nJOB_ROUTER_ROUTE_%s @=jre\n", route_index, tag.c_str());
						// if the name matches the tag, clear the name so that text of the route doesn't have a NAME statement
						if (tag == xfm.getName()) { xfm.setName(""); }
						std::string xfm_text;
						fputs(xfm.getFormattedText(xfm_text,"   ", true), stdout);
						fputs("\n@jre\n", stdout);
						++route_index;
					} else {
						// fprintf(stderr,"%d,",offset);
					}
				} else {
					fprintf(stdout, "\n##### JOB_ROUTER_ENTRIES [%d] #####\n", route_index);
					std::string xfm_text;
					fputs(xfm.getFormattedText(xfm_text,"", true), stdout);
					fprintf(stdout, "\n");
					++route_index;
				}
			}
		}
	}

	if (config_file) {
		routing.set(ceconfig.local_param("JOB_ROUTER_ENTRIES_FILE", ctx));
		if (routing) { fprintf(stdout, "need JOB_ROUTER_ENTRIES_FILE %s\n", routing.ptr()); }
		routing.clear();
	} else {
		routing.set(param("JOB_ROUTER_ENTRIES_FILE"));
	}
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
				MacroStreamXFormSource xfm;
				if (XFormLoadFromClassadJobRouterRoute(xfm, routes_string, offset, default_route_ad, options) >= 0) {
					fprintf(stdout, "\n##### JOB_ROUTER_ENTRIES_FILE [%d] #####\n", route_index);
					std::string xfm_text;
					fputs(xfm.getFormattedText(xfm_text,"", true), stdout);
					fprintf(stdout, "\n");
				}
			}
		}
	}

	if (config_file && ! ceconfig.lookup("JOB_ROUTER_ROUTE_NAMES", ctx)) {
		fprintf(stdout, "\nJOB_ROUTER_ROUTE_NAMES = %s\n", routes_list.c_str());
	}

	return 0;
}

int DoUnitTests(int options, std::deque<input_file> & inputs)
{
	if (options) {
		std::vector<MyAsyncFileReader*> readers;
		readers.reserve(inputs.size());
		for (auto it = inputs.begin(); it != inputs.end(); ++it) {
			MyAsyncFileReader * reader = new MyAsyncFileReader();

			int rval = reader->open(it->filename);
			if (rval < 0) {
				fprintf(stdout, "async open failed : %d %s", reader->error_code(), strerror(reader->error_code()));
			} else {
				rval = reader->queue_next_read();
				if (rval < 0) {
					fprintf(stdout, "async read failed : %d %s", reader->error_code(), strerror(reader->error_code()));
				}
			}
			if (rval < 0) {
				delete reader;
			} else {
				readers.push_back(reader);
			}
		}

		int cpending = 0;
		const int max_pending = 10;
		int lineno = 0;
		while ( ! readers.empty()) {

			for (auto it = readers.begin(); it != readers.end(); /* advance in the loop*/) {
				MyAsyncFileReader* reader = *it;

				bool is_done = false;
				const char * p1;
				const char * p2;
				int c1, c2;
				if (reader->get_data(p1, c1, p2, c2)) {
					std::string tmp;
					bool got_line = reader->output().readLine(tmp);
					if (got_line) ++lineno;
					if (options & 1) {
						fprintf(stdout, "reader %p: get_data(%p, %d, %p, %d) %d [%3d]:%s\n", reader, p1, c1, p2, c2, got_line, lineno, tmp.c_str());
					} else {
						fprintf(stdout, "%s", tmp.c_str());
					}
					if ( ! got_line) {
						++cpending;
						if (cpending > max_pending) {
							is_done = true;
						}
					}
				} else if (reader->is_closed() || reader->error_code()) {
					is_done = true;
					int tot_reads=-1, tot_pending=-1;
					reader->get_stats(tot_reads, tot_pending);
					int err = reader->error_code();
					if (err) {
						fprintf(stdout, "reader %p: closed. reads=%d, pending=%d, error=%d %s\n",
							reader, tot_reads, tot_pending, err, strerror(err));
					} else {
						fprintf(stdout, "reader %p: closed. reads=%d, pending=%d\n", reader, tot_reads, tot_pending);
					}
				} else {
					fprintf(stdout, "reader %p: pending...\n", reader);
					sleep(1);
					++cpending;
					if (cpending > max_pending) {
						is_done = true;
					}
				}

				if (is_done) {
					delete *it;
					it = readers.erase(it);
				} else {
					++it;
				}
			}
		}
	}

	return 0;
}

