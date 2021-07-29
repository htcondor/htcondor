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
#include <my_async_fread.h>

#include <string>
#include <set>

#define USE_CLASSAD_ITERATOR 1
#define USE_CLASSAD_LIST_WRITER 1

bool dash_verbose = false;
bool dash_terse = false;
int  UnitTestOpts = 0;
int  DashConvertOldRoutes = 0;
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
#ifdef USE_CLASSAD_LIST_WRITER
	apply_transform_args(MacroStreamXFormSource& xfm, XFormHash & ms, FILE* out, CondorClassAdListWriter & writ)
		: xforms(xfm), mset(ms), checkpoint(NULL), outfile(out), writer(writ), input_helper(NULL) {}
#else
	apply_transform_args(MacroStreamXFormSource& xfm, XFormHash & ms, FILE* out)
		: xforms(xfm), mset(ms), checkpoint(NULL), outfile(out), input_helper(NULL) {}
#endif
	MacroStreamXFormSource & xforms;
	XFormHash & mset;
	MACRO_SET_CHECKPOINT_HDR* checkpoint;
	FILE* outfile;
#ifdef USE_CLASSAD_LIST_WRITER
	CondorClassAdListWriter & writer;
#endif
	CondorClassAdFileParseHelper *input_helper;
	std::string errmsg;
};



// forward function references
void Usage(FILE*);
void PrintRules(FILE*);
int DoUnitTests(int options, std::vector<input_file> & inputs);
int ApplyTransform(void *pv, ClassAd * job);
bool ReportSuccess(const ClassAd * job, apply_transform_args & xform_args);
#ifdef USE_CLASSAD_LIST_WRITER
int DoTransforms(input_file & input, const char * constr, MacroStreamXFormSource & xforms, XFormHash & mset, FILE* outfile, CondorClassAdListWriter &writer);
#else
int DoTransforms(input_file & input, const char * constraint, MacroStreamXFormSource & xforms, XFormHash & mset, FILE* outfile);
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

MACRO_SOURCE FileMacroSource = { false, false, 0, 0, -1, -2 };

int
main( int argc, const char *argv[] )
{
	const char *pcolon = NULL;
	ClassAdFileParseType::ParseType def_ads_format = ClassAdFileParseType::Parse_Unspecified;
	std::vector<input_file> inputs;
	std::vector<const char *> rules;
	const char *job_match_constraint = NULL;

	MyName = condor_basename(argv[0]);
	if (argc == 1) {
		Usage(stderr);
		return 1;
	}

	setbuf( stdout, NULL );

	set_mySubSystem( MySubsys, SUBSYSTEM_TYPE_TOOL );

	myDistro->Init( argc, argv );
	set_priv_initialize(); // allow uid switching if root
	config();

	XFormHash xform_hash;
	xform_hash.init();

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
			MACRO_EVAL_CONTEXT ctx; ctx.init(MySubsys,0);
			xform_hash.set_arg_variable(name.c_str(), value.c_str(), ctx);
		} else {
			inputs.push_back(input_file(*ptr, def_ads_format));
		}
	}

	// the -convertoldroutes argument tells us to just read and
	if (DashConvertOldRoutes) { exit(ConvertJobRouterRoutes(DashConvertOldRoutes)); }

	// the -dry argument takes a qualifier that I'm hijacking to do queue parsing unit tests for now the 8.3 series.
	if (UnitTestOpts > 0) { exit(DoUnitTests(UnitTestOpts, inputs)); }

	// read the transform rules into the MacroStreamXFormSource
	int rval = 0;
	std::string errmsg = "";
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
		xform_hash.set_RulesFile("<stdin>", FileMacroSource);
		rval = ms.load(stdin, FileMacroSource, errmsg);
	} else {
		FILE *file = safe_fopen_wrapper_follow(rules[0], "r");
		if (file == NULL) {
			fprintf(stderr, "Can't open rules file: %s\n", rules[0]);
			return -1;
		}
		xform_hash.set_RulesFile(rules[0], FileMacroSource);
		rval = ms.load(file, FileMacroSource, errmsg);
		if (rval < 0 || ! ms.close_when_done(true)) {
			fclose(file);
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

#ifdef USE_CLASSAD_LIST_WRITER
	CondorClassAdListWriter writer(DashOutFormat);
#endif

	for (size_t ixInput = 0; ixInput < inputs.size(); ++ixInput) {
		// use default parse format if input file still has an unspecifed one.
		if (inputs[ixInput].parse_format == ClassAdFileParseType::Parse_Unspecified) { inputs[ixInput].parse_format = def_ads_format; }
#ifdef USE_CLASSAD_LIST_WRITER
		rval = DoTransforms(inputs[ixInput], job_match_constraint, ms, xform_hash, outfile, writer);
#else
		rval = DoTransforms(inputs[ixInput], job_match_constraint, ms, xform_hash, outfile);
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
		int cAttrs = InsertFromFile(file, *classad, is_eof, error, &parse_help);

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

class CondorQClassAdFileParseHelper : public CondorClassAdFileParseHelper
{
 public:
	CondorQClassAdFileParseHelper(ParseType typ=Parse_long)
		: CondorClassAdFileParseHelper("\n", typ)
		, is_schedd(false), is_submitter(false)
	{}
	virtual int PreParse(std::string & line, classad::ClassAd & ad, FILE* file);
	virtual int OnParseError(std::string & line, classad::ClassAd & ad, FILE* file);
	std::string schedd_name;
	std::string schedd_addr;
	bool is_schedd;
	bool is_submitter;
};

// this method is called before each line is parsed. 
// return 0 to skip (is_comment), 1 to parse line, 2 for end-of-classad, -1 for abort
int CondorQClassAdFileParseHelper::PreParse(std::string & line, classad::ClassAd & /*ad*/, FILE* /*file*/)
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
int CondorQClassAdFileParseHelper::OnParseError(std::string & line, classad::ClassAd & ad, FILE* file)
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
		sGetAdAttrs(attrs, *job);
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
				//PRAGMA_REMIND("fix to call call Unparse with projection when it exists")
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
				//PRAGMA_REMIND("fix to call call Unparse with projection when it exists")
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
				//PRAGMA_REMIND("fix to call call Unparse with projection when it exists")
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
			MacroStreamXFormSource xfm;
			if (XFormLoadFromClassadJobRouterRoute(xfm, routes_string, offset, default_route_ad, options) >= 0) {
				fprintf(stdout, "\n##### JOB_ROUTER_ENTRIES [%d] #####\n", route_index);
				std::string xfm_text;
				fputs(xfm.getFormattedText(xfm_text,"", true), stdout);
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

	return 0;
}

int DoUnitTests(int options, std::vector<input_file> & inputs)
{
	if (options) {
		std::vector<MyAsyncFileReader*> readers;
		for (std::vector<input_file>::iterator it = inputs.begin(); it != inputs.end(); ++it) {
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

			for (std::vector<MyAsyncFileReader*>::iterator it = readers.begin(); it != readers.end(); /* advance in the loop*/) {
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

