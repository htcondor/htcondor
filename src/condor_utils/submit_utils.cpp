/***************************************************************
 *
 * Copyright (C) 1990-2016, Condor Team, Computer Sciences Department,
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
//#include "condor_network.h"
#include "condor_string.h"
#include "spooled_job_files.h" // for GetSpooledExecutablePath()
//#include "subsystem_info.h"
//#include "env.h"
#include "basename.h"
#include "condor_getcwd.h"
//#include <time.h>
//#include "write_user_log.h"
#include "condor_classad.h"
#include "condor_attributes.h"
#include "condor_adtypes.h"
//#include "condor_io.h"
//#include "condor_distribution.h"
//#include "condor_ver_info.h"
#if !defined(WIN32)
//#include <pwd.h>
//#include <sys/stat.h>
#endif
//#include "store_cred.h"
//#include "internet.h"
//#include "my_hostname.h"
#include "domain_tools.h"
//#include "get_daemon_name.h"
#include "sig_install.h"
//#include "access.h"
#include "daemon.h"
//#include "match_prefix.h"
//
//#include "extArray.h"
//#include "HashTable.h"
//#include "MyString.h"
#include "string_list.h"
#include "which.h"
#include "sig_name.h"
#include "print_wrapped_text.h"
//#include "dc_schedd.h"
//#include "dc_collector.h"
#include "my_username.h" // for my_domainname
#include "globus_utils.h" // for 
#include "enum_utils.h" // for shouldtransferfiles_t
//#include "setenv.h"
//#include "classad_hashtable.h"
#include "directory.h"
#include "filename_tools.h"
#include "fs_util.h"
#include "dc_transferd.h"  // for StdoutRemapName
//#include "condor_ftp.h"
#include "condor_crontab.h"
//#include <scheduler.h>
#include "file_transfer.h"
#include "condor_holdcodes.h"
#include "condor_url.h"
#include "condor_version.h"
#include "NegotiationUtils.h"
#include "submit_utils.h"
//#include "submit_internal.h"
#define PLUS_ATTRIBS_IN_CLUSTER_AD 1

#include "list.h"
#include "condor_vm_universe_types.h"
#include "vm_univ_utils.h"
#include "condor_md.h"
#include "my_popen.h"
#include "condor_base64.h"
#include "zkm_base64.h"

#include <algorithm>
#include <string>
#include <set>

#ifdef WIN32
#define CLIPPED 1
#endif

#define ABORT_AND_RETURN(v) abort_code=v; return abort_code
#define RETURN_IF_ABORT() if (abort_code) return abort_code

#define exit(n)  poison_exit(n)

class DeltaClassAd
{
public:
	DeltaClassAd(ClassAd & _ad) : ad(_ad) {}
	virtual ~DeltaClassAd() {};

	bool Insert(const std::string & attr, ExprTree * tree) { return ad.Insert(attr, tree); }
	bool Assign(const char* attr, bool val) { return ad.Assign(attr, val); }
	bool Assign(const char* attr, double val) { return ad.Assign(attr, val); }
	bool Assign(const char* attr, long long val) { return ad.Assign(attr, val); }
	bool Assign(const char* attr, const char * val) { return ad.Assign(attr, val); }
	ExprTree * LookupExpr(const char * attr) { return ad.LookupExpr(attr); }
	ExprTree * Lookup(const std::string & attr) { return ad.Lookup(attr); }
	int LookupString(const char * attr, MyString & val) { return ad.LookupString(attr, val); }
	int LookupString(const char * attr, std::string & val) { return ad.LookupString(attr, val); }
	int LookupBool(const char * attr, bool & val) { return ad.LookupBool(attr, val); }

protected:
	ClassAd& ad;
};

bool SubmitHash::AssignJobVal(const char * attr, bool val) { return job->Assign(attr, val); }
bool SubmitHash::AssignJobVal(const char * attr, double val) { return job->Assign(attr, val); }
bool SubmitHash::AssignJobVal(const char * attr, long long val) { return job->Assign(attr, val); }
//bool SubmitHash::AssignJobVal(const char * attr, int val) { return job->Assign(attr, val); }
//bool SubmitHash::AssignJobVal(const char * attr, long val) { return job->Assign(attr, val); }
//bool SubmitHash::AssignJobVal(const char * attr, time_t val) { return job->Assign(attr, val); }


// declare enough of the condor_params structure definitions so that we can define submit hashtable defaults
namespace condor_params {
	typedef struct string_value { char * psz; int flags; } string_value;
	struct key_value_pair { const char * key; const string_value * def; };
}

// values for submit hashtable defaults, these are declared as char rather than as const char to make g++ on fedora shut up.
static char OneString[] = "1", ZeroString[] = "0";
//static char ParallelNodeString[] = "#pArAlLeLnOdE#";
static char UnsetString[] = "";


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
static condor_params::string_value UnliveSubmitFileMacroDef = { UnsetString, 0 };
static condor_params::string_value UnliveNodeMacroDef = { UnsetString, 0 };
static condor_params::string_value UnliveClusterMacroDef = { OneString, 0 };
static condor_params::string_value UnliveProcessMacroDef = { ZeroString, 0 };
static condor_params::string_value UnliveStepMacroDef = { ZeroString, 0 };
static condor_params::string_value UnliveRowMacroDef = { ZeroString, 0 };

static char rc[] = "$(Request_CPUs)";
static condor_params::string_value VMVCPUSMacroDef = { rc, 0 };
static char rm[] = "$(Request_Memory)";
static condor_params::string_value VMMemoryMacroDef = { rm, 0 };

// Necessary so that the user who sets request_memory instead of
// RequestMemory doesn't miss out on the default value for VM_MEMORY.
static char rem[] = "$(RequestMemory)";
static condor_params::string_value RequestMemoryMacroDef = { rem, 0 };
// The same for CPUs.
static char rec[] = "$(RequestCPUs)";
static condor_params::string_value RequestCPUsMacroDef = { rec, 0 };

static char StrictFalseMetaKnob[] = 
	"SubmitWarnEmptyMatches=false\n"
	"SubmitFailEmptyMatches=false\n"
	"SubmitWarnDuplicateMatches=false\n"
	"SubmitFailEmptyFields=false\n"
	"SubmitWarnEmptyFields=false\n";
static char StrictTrueMetaKnob[] = 
	"SubmitWarnEmptyMatches=true\n"
	"SubmitFailEmptyMatches=true\n"
	"SubmitWarnDuplicateMatches=true\n"
	"SubmitFailEmptyFields=false\n"
	"SubmitWarnEmptyFields=true\n";
static char StrictHarshMetaKnob[] = 
	"SubmitWarnEmptyMatches=true\n"
	"SubmitFailEmptyMatches=true\n"
	"SubmitWarnDuplicateMatches=true\n"
	"SubmitFailEmptyFields=true\n"
	"SubmitWarnEmptyFields=true\n";
static condor_params::string_value StrictFalseMetaDef = { StrictFalseMetaKnob, 0 };
static condor_params::string_value StrictTrueMetaDef = { StrictTrueMetaKnob, 0 };
static condor_params::string_value StrictHarshMetaDef = { StrictHarshMetaKnob, 0 };

// Table of submit macro 'defaults'. This provides a convenient way to inject things into the submit
// hashtable without actually running a bunch of code to stuff the table.
// Because they are defaults they will be ignored when scanning for unreferenced macros.
// NOTE: TABLE MUST BE SORTED BY THE FIRST FIELD!!
static MACRO_DEF_ITEM SubmitMacroDefaults[] = {
	{ "$STRICT.FALSE", &StrictFalseMetaDef },
	{ "$STRICT.HARSH", &StrictHarshMetaDef },
	{ "$STRICT.TRUE", &StrictTrueMetaDef },
	{ "ARCH",      &ArchMacroDef },
	{ "Cluster",   &UnliveClusterMacroDef },
	{ "ClusterId", &UnliveClusterMacroDef },
	{ "IsLinux",   &IsLinuxMacroDef },
	{ "IsWindows", &IsWinMacroDef },
	{ "ItemIndex", &UnliveRowMacroDef },
	{ "Node",      &UnliveNodeMacroDef },
	{ "OPSYS",           &OpsysMacroDef },
	{ "OPSYSANDVER",     &OpsysAndVerMacroDef },
	{ "OPSYSMAJORVER",   &OpsysMajorVerMacroDef },
	{ "OPSYSVER",        &OpsysVerMacroDef },
	{ "Process",   &UnliveProcessMacroDef },
	{ "ProcId",    &UnliveProcessMacroDef },
	{ "Request_CPUs", &RequestCPUsMacroDef },
	{ "Request_Memory", &RequestMemoryMacroDef },
	{ "Row",       &UnliveRowMacroDef },
	{ "SPOOL",     &SpoolMacroDef },
	{ "Step",      &UnliveStepMacroDef },
	{ "SUBMIT_FILE", &UnliveSubmitFileMacroDef },
	{ "VM_MEMORY", &VMMemoryMacroDef },
	{ "VM_VCPUS",  &VMVCPUSMacroDef },
};


// these are used to keep track of the source of various macros in the table.
//const MACRO_SOURCE DetectedMacro = { true,  false, 0, -2, -1, -2 };
const MACRO_SOURCE DefaultMacro = { true, false, 1, -2, -1, -2 }; // for macros set by default
const MACRO_SOURCE ArgumentMacro = { true, false, 2, -2, -1, -2 }; // for macros set by command line
const MACRO_SOURCE LiveMacro = { true, false, 3, -2, -1, -2 };    // for macros use as queue loop variables

bool is_required_request_resource(const char * name) {
	return MATCH == strcasecmp(name, SUBMIT_KEY_RequestCpus)
		|| MATCH == strcasecmp(name, SUBMIT_KEY_RequestDisk)
		|| MATCH == strcasecmp(name, SUBMIT_KEY_RequestMemory);
}

// parse an expression string, and validate it, then wrap it in parens
// if needed in preparation for appending another expression string with the given operator
static bool check_expr_and_wrap_for_op(std::string &expr_str, classad::Operation::OpKind op)
{
	ExprTree * tree = NULL;
	bool valid_expr = (0 == ParseClassAdRvalExpr(expr_str.c_str(), tree));
	if (valid_expr && tree) { // figure out if we need to add parens
		ExprTree * expr = WrapExprTreeInParensForOp(tree, op);
		if (expr != tree) {
			tree = expr;
			expr_str.clear();
			ExprTreeToString(tree, expr_str);
		}
	}
	delete tree;
	return valid_expr;
}

condor_params::string_value * allocate_live_default_string(MACRO_SET &set, const condor_params::string_value & Def, int cch)
{
	condor_params::string_value * NewDef = reinterpret_cast<condor_params::string_value*>(set.apool.consume(sizeof(condor_params::string_value), sizeof(void*)));
	NewDef->flags = Def.flags;
	NewDef->psz = set.apool.consume(cch, sizeof(void*));
	memset(NewDef->psz, 0, cch);
	if (Def.psz) strcpy(NewDef->psz, Def.psz);

	// change the defaults table pointers
	condor_params::key_value_pair *pdi = const_cast<condor_params::key_value_pair *>(set.defaults->table);
	for (int ii = 0; ii < set.defaults->size; ++ii) {
		if (pdi[ii].def == &Def) { pdi[ii].def = NewDef; }
	}

	// return the live string
	return NewDef;
}

// setup a MACRO_DEFAULTS table for the macro set, we have to re-do this each time we clear
// the macro set, because we allocate the defaults from the ALLOCATION_POOL.
void SubmitHash::setup_macro_defaults()
{
	// make an instance of the defaults table that is private to this function. 
	// we do this because of the 'live' keys in the 
	struct condor_params::key_value_pair* pdi = reinterpret_cast<struct condor_params::key_value_pair*> (SubmitMacroSet.apool.consume(sizeof(SubmitMacroDefaults), sizeof(void*)));
	memcpy((void*)pdi, SubmitMacroDefaults, sizeof(SubmitMacroDefaults));
	SubmitMacroSet.defaults = reinterpret_cast<MACRO_DEFAULTS*>(SubmitMacroSet.apool.consume(sizeof(MACRO_DEFAULTS), sizeof(void*)));
	SubmitMacroSet.defaults->size = COUNTOF(SubmitMacroDefaults);
	SubmitMacroSet.defaults->table = pdi;
	SubmitMacroSet.defaults->metat = NULL;

	// allocate space for the 'live' macro default string_values and for the strings themselves.
	LiveNodeString = allocate_live_default_string(SubmitMacroSet, UnliveNodeMacroDef, 24)->psz;
	LiveClusterString = allocate_live_default_string(SubmitMacroSet, UnliveClusterMacroDef, 24)->psz;
	LiveProcessString = allocate_live_default_string(SubmitMacroSet, UnliveProcessMacroDef, 24)->psz;
	LiveRowString = allocate_live_default_string(SubmitMacroSet, UnliveRowMacroDef, 24)->psz;
	LiveStepString = allocate_live_default_string(SubmitMacroSet, UnliveStepMacroDef, 24)->psz;
}

// set the value that $(SUBMIT_FILE) will expand to. (set into the defaults table, not the submit hash table)
void SubmitHash::insert_submit_filename(const char * filename, MACRO_SOURCE & source)
{
	insert_source(filename, source);

	// if the defaults table pointer for SUBMIT_FILE is unset, set it to point to the filename we just inserted.
	condor_params::key_value_pair *pdi = const_cast<condor_params::key_value_pair *>(SubmitMacroSet.defaults->table);
	for (int ii = 0; ii < SubmitMacroSet.defaults->size; ++ii) {
		if (pdi[ii].def == &UnliveSubmitFileMacroDef) { 
			condor_params::string_value * NewDef = reinterpret_cast<condor_params::string_value*>(SubmitMacroSet.apool.consume(sizeof(condor_params::string_value), sizeof(void*)));
			NewDef->flags = UnliveSubmitFileMacroDef.flags;
			NewDef->psz = const_cast<char*>(macro_source_filename(source, SubmitMacroSet));
			pdi[ii].def = NewDef;
		}
	}
}


SubmitHash::SubmitHash()
	: clusterAd(NULL)
	, procAd(NULL)
	, job(NULL)
	, submit_time(0)
	, abort_code(0)
	, abort_macro_name(NULL)
	, abort_raw_macro_val(NULL)
	, DisableFileChecks(true)
	, FakeFileCreationChecks(false)
	, IsInteractiveJob(false)
	, IsRemoteJob(false)
	, FnCheckFile(NULL)
	, CheckFileArg(NULL)
	, LiveNodeString(NULL)
	, LiveClusterString(NULL)
	, LiveProcessString(NULL)
	, LiveRowString(NULL)
	, LiveStepString(NULL)
	, should_transfer((ShouldTransferFiles_t)-1)
	, JobUniverse(CONDOR_UNIVERSE_MIN)
	, JobIwdInitialized(false)
	, IsNiceUser(false)
	, IsDockerJob(false)
	, JobDisableFileChecks(false)
	, NeedsJobDeferral(false)
	, NeedsPerFileEncryption(false)
	, HasEncryptExecuteDir(false)
	, HasTDP(false)
	, UserLogSpecified(false)
	, StreamStdout(false)
	, StreamStderr(false)
	, RequestMemoryIsZero(false)
	, RequestDiskIsZero(false)
	, RequestCpusIsZeroOrOne(false)
	, already_warned_requirements_disk(false)
	, already_warned_requirements_mem(false)
	, already_warned_job_lease_too_small(false)
	, already_warned_notification_never(false)
	, ExecutableSizeKb(0)
	, TransferInputSizeKb(0)

{
	memset(&SubmitMacroSet, 0, sizeof(SubmitMacroSet));
	SubmitMacroSet.options = CONFIG_OPT_WANT_META | CONFIG_OPT_KEEP_DEFAULTS | CONFIG_OPT_SUBMIT_SYNTAX;
	SubmitMacroSet.apool = ALLOCATION_POOL();
	SubmitMacroSet.sources = std::vector<const char*>();
	SubmitMacroSet.errors = new CondorError();
#if 1
	setup_macro_defaults();
#else
	SubmitMacroSet.defaults = &SubmitMacroDefaultSet;
#endif

	mctx.init("SUBMIT", 3);
}


SubmitHash::~SubmitHash()
{
	if (SubmitMacroSet.errors) delete SubmitMacroSet.errors;
	SubmitMacroSet.errors = NULL;

	delete job; job = NULL;
	delete procAd; procAd = NULL;

	// detach but do not delete the cluster ad
	//PRAGMA_REMIND("tj: should we copy/delete the cluster ad?")
	clusterAd = NULL;
}

void SubmitHash::push_error(FILE * fh, const char* format, ... ) //CHECK_PRINTF_FORMAT(3,4);
{
	va_list ap;
	va_start(ap, format);
	int cch = vprintf_length(format, ap);
	char * message = (char*)malloc(cch + 1);
	if (message) {
		vsprintf ( message, format, ap );
	}
	va_end(ap);

	if (SubmitMacroSet.errors) {
		SubmitMacroSet.errors->push("Submit", -1, message);
	} else {
		fprintf(fh, "\nERROR: %s", message ? message : "");
	}
	if (message) {
		free(message);
	}
}

void SubmitHash::push_warning(FILE * fh, const char* format, ... ) //CHECK_PRINTF_FORMAT(3,4);
{
	va_list ap;
	va_start(ap, format);
	int cch = vprintf_length(format, ap);
	char * message = (char*)malloc(cch + 1);
	if (message) {
		vsprintf ( message, format, ap );
	}
	va_end(ap);

	if (SubmitMacroSet.errors) {
		SubmitMacroSet.errors->push("Submit", 0, message);
	} else {
		fprintf(fh, "\nWARNING: %s", message ? message : "");
	}
	if (message) {
		free(message);
	}
}


static char * trim_and_strip_quotes_in_place(char * str)
{
	char * p = str;
	while (isspace(*p)) ++p;
	char * pe = p + strlen(p);
	while (pe > p && isspace(pe[-1])) --pe;
	*pe = 0;

	if (*p == '"' && pe > p && pe[-1] == '"') {
		// if we also had both leading and trailing quotes, strip them.
		if (pe > p && pe[-1] == '"') {
			*--pe = 0;
			++p;
		}
	}
	return p;
}

static int has_whitespace( const char *str)
{
	while( *str ) {
		if( isspace(*str++) ) {
			return( 1 );
		}
	}

	return( 0 );
}

static void compress_path( MyString &path )
{
	char	*src, *dst;
	char *str = strdup(path.Value());

	src = str;
	dst = str;

#ifdef WIN32
	if (*src) {
		*dst++ = *src++;	// don't compress WIN32 "share" path
	}
#endif

	while( *src ) {
		*dst++ = *src++;
		while( (*(src - 1) == '\\' || *(src - 1) == '/') && (*src == '\\' || *src == '/') ) {
			src++;
		}
	}
	*dst = '\0';

	path = str;
	free( str );
}


/* check_path_length() has been deprecated in favor of 
 * check_and_universalize_path(), which not only checks
 * the length of the path string but also, on windows, it
 * takes any network drive paths and converts them to UNC
 * paths.
 */
int SubmitHash::check_and_universalize_path( MyString &path )
{
	(void) path;

	int retval = 0;
#ifdef WIN32
	/*
	 * On Windows, we need to convert all drive letters (mappings) 
	 * to their UNC equivalents, and make sure these network devices
	 * are accessable by the submitting user (not mapped as someone
	 * else).
	 */

	char volume[8];
	char netuser[80];
	unsigned char name_info_buf[MAX_PATH+1];
	DWORD name_info_buf_size = MAX_PATH+1;
	DWORD netuser_size = 80;
	DWORD result;
	UNIVERSAL_NAME_INFO *uni;

	result = 0;
	volume[0] = '\0';
	if ( path[0] && (path[1]==':') ) {
		sprintf(volume,"%c:",path[0]);
	}

	if (volume[0] && (GetDriveType(volume)==DRIVE_REMOTE))
	{
		char my_name[255];
		char *tmp, *net_name;

			// check if the user is the submitting user.
		
		result = WNetGetUser(volume, netuser, &netuser_size);
		tmp = my_username();
		strncpy(my_name, tmp, sizeof(my_name));
		free(tmp);
		tmp = NULL;
		getDomainAndName(netuser, tmp, net_name);

		if ( result == NOERROR ) {
			// We compare only the name (and not the domain) since
			// it's likely that the same account across different domains
			// has the same password, and is therefore a valid path for us
			// to use. It would be nice to check that the passwords truly
			// are the same on both accounts too, but since that would 
			// basically involve creating a whole separate mapping just to
			// test it, the expense is not worth it.
			
			if (strcasecmp(my_name, net_name) != 0 ) {
				push_error(stderr, "The path '%s' is associated with\n"
				"\tuser '%s', but you're '%s', so Condor can\n"
			    "\tnot access it. Currently Condor only supports network\n"
			    "\tdrives that are associated with the submitting user.\n", 
					path.Value(), net_name, my_name);
				return -1;
			}
		} else {
			push_error(stderr, "Unable to get name of user associated with \n"
				"the following network path (err=%d):"
				"\n\t%s\n", result, path.Value());
			return -1;
		}

		result = WNetGetUniversalName(path.Value(), UNIVERSAL_NAME_INFO_LEVEL, 
			name_info_buf, &name_info_buf_size);
		if ( result != NO_ERROR ) {
			push_error(stderr, "Unable to get universal name for path (err=%d):"
				"\n\t%s\n", result, path.Value());
			return -1;
		} else {
			uni = (UNIVERSAL_NAME_INFO*)&name_info_buf;
			path = uni->lpUniversalName;
			retval = 1; // signal that we changed somthing
		}
	}
	
#endif

	return retval;
}


// parse a input string for an int64 value optionally followed by K,M,G,or T
// as a scaling factor, then divide by a base scaling factor and return the
// result by ref. base is expected to be a multiple of 2 usually  1, 1024 or 1024*1024.
// result is truncated to the next largest value by base.
//
// Return value is true if the input string contains only a valid int, false if
// there are any unexpected characters other than whitespace.  value is
// unmodified when false is returned.
//
// this function exists to regularize the former ad-hoc parsing of integers in the
// submit file, it expands parsing to handle 64 bit ints and multiplier suffixes.
// Note that new classads will interpret the multiplier suffixes without
// regard for the fact that the defined units of many job ad attributes are
// in Kbytes or Mbytes. We need to parse them in submit rather than
// passing the expression on to the classad code to be parsed to preserve the
// assumption that the base units of the output is not bytes.
//
bool parse_int64_bytes(const char * input, int64_t & value, int base)
{
	const char * tmp = input;
	while (isspace(*tmp)) ++tmp;

	char * p;
#ifdef WIN32
	int64_t val = _strtoi64(tmp, &p, 10);
#else
	int64_t val = strtol(tmp, &p, 10);
#endif

	// allow input to have a fractional part, so "2.2M" would be valid input.
	// this doesn't have to be very accurate, since we round up to base anyway.
	double fract = 0;
	if (*p == '.') {
		++p;
		if (isdigit(*p)) { fract += (*p - '0') / 10.0; ++p; }
		if (isdigit(*p)) { fract += (*p - '0') / 100.0; ++p; }
		if (isdigit(*p)) { fract += (*p - '0') / 1000.0; ++p; }
		while (isdigit(*p)) ++p;
	}

	// if the first non-space character wasn't a number
	// then this isn't a simple integer, return false.
	if (p == tmp)
		return false;

	while (isspace(*p)) ++p;

	// parse the multiplier postfix
	int64_t mult = 1;
	if (!*p) mult = base;
	else if (*p == 'k' || *p == 'K') mult = 1024;
	else if (*p == 'm' || *p == 'M') mult = 1024*1024;
	else if (*p == 'g' || *p == 'G') mult = (int64_t)1024*1024*1024;
	else if (*p == 't' || *p == 'T') mult = (int64_t)1024*1024*1024*1024;
	else return false;

	val = (int64_t)((val + fract) * mult + base-1) / base;

	// if we to to here and we are at the end of the string
	// then the input is valid, return true;
	if (!*p || !p[1]) { 
		value = val;
		return true; 
	}

	// Tolerate a b (as in Kb) and whitespace at the end, anything else and return false)
	if (p[1] == 'b' || p[1] == 'B') p += 2;
	while (isspace(*p)) ++p;
	if (!*p) {
		value = val;
		return true;
	}

	return false;
}

/*
** Make a wild guess at the size of the image represented by this a.out.
** Should add up the text, data, and bss sizes, then allow something for
** the stack.  But how we gonna do that if the executable is for some
** other architecture??  Our answer is in kilobytes.
*/
int64_t SubmitHash::calc_image_size_kb( const char *name)
{
	struct stat	buf;

	if ( IsUrl( name ) ) {
		return 0;
	}

	if( stat(full_path(name),&buf) < 0 ) {
		// EXCEPT( "Cannot stat \"%s\"", name );
		return 0;
	}
	if( buf.st_mode & S_IFDIR ) {
		Directory dir(full_path(name));
		return ((int64_t)dir.GetDirectorySize() + 1023) / 1024;
	}
	return ((int64_t)buf.st_size + 1023) / 1024;
}


// check directories for input and output.  for now we do nothing
// other than to make sure that it actually IS a directory on Windows
// On Linux we already know that by the error code from safe_open below
//
static bool check_directory( const char* pathname, int /*flags*/, int err )
{
#if defined(WIN32)
	// Make sure that it actually is a directory
	DWORD dwAttribs = GetFileAttributes(pathname);
	if (INVALID_FILE_ATTRIBUTES == dwAttribs)
		return false;
	return (dwAttribs & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
	// will just do nothing here and leave
	// it up to the runtime to nicely report errors.
	(void)pathname;
	return (err == EISDIR);
#endif
}


const char * SubmitHash::full_path(const char *name, bool use_iwd /*=true*/)
{
	char const *p_iwd;
	MyString realcwd;

	if ( use_iwd ) {
		ASSERT(JobIwd.Length());
		p_iwd = JobIwd.Value();
	} else if (clusterAd) {
		// if there is a cluster ad, we NEVER want to use the current working directory
		// instead we want to treat the saved working directory of submit as the cwd.
		realcwd = submit_param_mystring("FACTORY.Iwd", "FACTORY.Iwd");
		p_iwd = realcwd.Value();
	} else {
		condor_getcwd(realcwd);
		p_iwd = realcwd.Value();
	}

#if defined(WIN32)
	if ( name[0] == '\\' || name[0] == '/' || (name[0] && name[1] == ':') ) {
		TempPathname = name;
	} else {
		TempPathname.formatstr( "%s\\%s", p_iwd, name );
	}
#else

	if( name[0] == '/' ) {	/* absolute wrt whatever the root is */
		TempPathname.formatstr( "%s%s", JobRootdir.Value(), name );
	} else {	/* relative to iwd which is relative to the root */
		TempPathname.formatstr( "%s/%s/%s", JobRootdir.Value(), p_iwd, name );
	}
#endif

	compress_path( TempPathname );

	return TempPathname.Value();
}


// this function parses and checks xen disk parameters
static bool validate_disk_param(const char *pszDisk, int min_params, int max_params)
{
	if( !pszDisk ) {
		return false;
	}

	const char *ptr = pszDisk;
	// skip leading white spaces
	while( *ptr && ( *ptr == ' ' )) {
		ptr++;
	}

	// parse each disk
	// e.g.) disk = filename1:hda1:w, filename2:hda2:w
	StringList disk_files(ptr, ",");
	if( disk_files.isEmpty() ) {
		return false;
	}

	disk_files.rewind();
	const char *one_disk = NULL;
	while( (one_disk = disk_files.next() ) != NULL ) {

		// found disk file
		StringList single_disk_file(one_disk, ":");
        int iNumDiskParams = single_disk_file.number();
		if( iNumDiskParams < min_params || iNumDiskParams > max_params ) {
			return false;
		}
	}
	return true;
}


// lookup and expand an item from the submit hashtable
//
char * SubmitHash::submit_param( const char* name)
{
	return submit_param(name, NULL);
}

// lookup and expand an item from the submit hashtable
//
char * SubmitHash::submit_param( const char* name, const char* alt_name )
{
	if (abort_code) return NULL;

	bool used_alt = false;
	const char *pval = lookup_macro(name, SubmitMacroSet, mctx);
	char * pval_expanded = NULL;

#ifdef SUBMIT_ATTRS_IS_ALSO_CONDOR_PARAM
	// TODO: change this to use the defaults table from SubmitMacroSet
	static classad::References submit_attrs;
	static bool submit_attrs_initialized = false;
	if ( ! submit_attrs_initialized) {
		param_and_insert_attrs("SUBMIT_ATTRS", submit_attrs);
		param_and_insert_attrs("SUBMIT_EXPRS", submit_attrs);
		param_and_insert_attrs("SYSTEM_SUBMIT_ATTRS", submit_attrs);
		submit_attrs_initialized = true;
	}
#endif

	if( ! pval && alt_name ) {
		pval = lookup_macro(alt_name, SubmitMacroSet, mctx);
		used_alt = true;
	}

	if( ! pval ) {
#ifdef SUBMIT_ATTRS_IS_ALSO_CONDOR_PARAM
			// if the value isn't in the submit file, check in the
			// submit_exprs list and use that as a default.  
		if ( ! submit_attrs.empty()) {
			if (submit_attrs.find(name) != submit_attrs.end()) {
				return param(name);
			}
			if (submit_attrs.find(alt_name) != submit_attrs.end()) {
				return param(alt_name);
			}
		}
#endif
		return NULL;
	}

	abort_macro_name = used_alt ? alt_name : name;
	abort_raw_macro_val = pval;

	pval_expanded = expand_macro(pval);

	if( pval == NULL ) {
		push_error(stderr, "Failed to expand macros in: %s\n",
				 used_alt ? alt_name : name );
		abort_code = 1;
		return NULL;
	}

	if( * pval_expanded == '\0' ) {
		free( pval_expanded );
		return NULL;
	}

	abort_macro_name = NULL;
	abort_raw_macro_val = NULL;

	return  pval_expanded;
}

bool SubmitHash::submit_param_exists(const char* name, const char * alt_name, std::string & value)
{
	auto_free_ptr result(submit_param(name, alt_name));
	if ( ! result)
		return false;

	value = result.ptr();
	return true;
}

void SubmitHash::set_submit_param( const char *name, const char *value )
{
	MACRO_EVAL_CONTEXT ctx = this->mctx; ctx.use_mask = 2;
	insert_macro(name, value, SubmitMacroSet, DefaultMacro, ctx);
}

MyString SubmitHash::submit_param_mystring( const char * name, const char * alt_name )
{
	char * result = submit_param(name, alt_name);
	MyString ret = result;
	free(result);
	return ret;
}

bool SubmitHash::submit_param_long_exists(const char* name, const char * alt_name, long long & value, bool int_range /*=false*/)
{
	auto_free_ptr result(submit_param(name, alt_name));
	if ( ! result)
		return false;

	if ( ! string_is_long_param(result.ptr(), value) ||
		(int_range && (value < INT_MIN || value >= INT_MAX)) ) {
		push_error(stderr, "%s=%s is invalid, must eval to an integer.\n", name, result.ptr());
		abort_code = 1;
		return false;
	}

	return true;
}

int SubmitHash::submit_param_int(const char* name, const char * alt_name, int def_value)
{
	long long value = def_value;
	if ( ! submit_param_long_exists(name, alt_name, value, true)) {
		value = def_value;
	}
	return (int)value;
}


int SubmitHash::non_negative_int_fail(const char * Name, char * Value)
{

	int iTemp=0;
	if (strstr(Value,".") || 
		(sscanf(Value, "%d", &iTemp) > 0 && iTemp < 0))
	{
		push_error(stderr, "'%s'='%s' is invalid, must eval to a non-negative integer.\n", Name, Value );
		ABORT_AND_RETURN(1);
	}
	
	return 0;
}

int SubmitHash::submit_param_bool(const char* name, const char * alt_name, bool def_value, bool * pexists)
{
	char * result = submit_param(name, alt_name);
	if ( ! result) {
		if (pexists) *pexists = false;
		return def_value;
	}
	if (pexists) *pexists = true;

	bool value = def_value;
	if (*result) {
		if ( ! string_is_boolean_param(result, value)) {
			push_error(stderr, "%s=%s is invalid, must eval to a boolean.\n", name, result);
			ABORT_AND_RETURN(1);
		}
	}
	free(result);
	return value;
}

void SubmitHash::set_arg_variable(const char* name, const char * value)
{
	MACRO_EVAL_CONTEXT ctx = mctx; ctx.use_mask = 0;
	insert_macro(name, value, SubmitMacroSet, ArgumentMacro, ctx);
}

// stuff a live value into submit's hashtable.
// IT IS UP TO THE CALLER TO INSURE THAT live_value IS VALID FOR THE LIFE OF THE HASHTABLE
// this function is intended for use during queue iteration to stuff
// changing values like $(Cluster) and $(Process) into the hashtable.
// The pointer passed in as live_value may be changed at any time to
// affect subsequent macro expansion of name.
// live values are automatically marked as 'used'.
//
void SubmitHash::set_live_submit_variable( const char *name, const char *live_value, bool force_used /*=true*/ )
{
	MACRO_EVAL_CONTEXT ctx = mctx; ctx.use_mask = 2;
	MACRO_ITEM* pitem = find_macro_item(name, NULL, SubmitMacroSet);
	if ( ! pitem) {
		insert_macro(name, "", SubmitMacroSet, LiveMacro, ctx);
		pitem = find_macro_item(name, NULL, SubmitMacroSet);
	}
	ASSERT(pitem);
	pitem->raw_value = live_value;
	if (SubmitMacroSet.metat && force_used) {
		MACRO_META* pmeta = &SubmitMacroSet.metat[pitem - SubmitMacroSet.table];
		pmeta->use_count += 1;
	}
}

void SubmitHash::set_submit_param_used( const char *name ) 
{
	increment_macro_use_count(name, SubmitMacroSet);
}



int SubmitHash::InsertJobExpr (MyString const &expr)
{
	return InsertJobExpr(expr.Value());
}

int SubmitHash::InsertJobExpr (const char *expr, const char * source_label /*=NULL*/)
{
	std::string attr;
	ExprTree *tree = NULL;
	if ( ! ParseLongFormAttrValue(expr, attr, tree) || ! tree) {
		push_error(stderr, "Parse error in expression: \n\t%s\n\t", expr);
		if ( ! SubmitMacroSet.errors) {
			fprintf(stderr,"Error in %s\n", source_label ? source_label : "submit file");
		}
		ABORT_AND_RETURN( 1 );
	}

	if (!job->Insert (attr, tree)) {
		push_error(stderr, "Unable to insert expression: %s\n", expr);
		ABORT_AND_RETURN( 1 );
	}

	return 0;
}


int SubmitHash::InsertJobExprInt(const char * name, int val)
{
	ASSERT(name);
	MyString buf;
	buf.formatstr("%s = %d", name, val);
	return InsertJobExpr(buf.Value());
}

int SubmitHash::InsertJobExprString(const char * name, const char * val)
{
	ASSERT(name);
	ASSERT(val);
	MyString buf;
	std::string esc;
	buf.formatstr("%s = %s", name, QuoteAdStringValue(val, esc));
	return InsertJobExpr(buf.Value());
}

const char * init_submit_default_macros()
{
	static bool initialized = false;
	if (initialized)
		return NULL;
	initialized = true;

	const char * ret = NULL; // null return is success.

	//PRAGMA_REMIND("tj: fix to reconfig properly")

	ArchMacroDef.psz = param( "ARCH" );
	if ( ! ArchMacroDef.psz) {
		ArchMacroDef.psz = UnsetString;
		ret = "ARCH not specified in config file";
	}

	OpsysMacroDef.psz = param( "OPSYS" );
	if( OpsysMacroDef.psz == NULL ) {
		OpsysMacroDef.psz = UnsetString;
		ret = "OPSYS not specified in config file";
	}
	// also pick up the variations on opsys if they are defined.
	OpsysAndVerMacroDef.psz = param( "OPSYSANDVER" );
	if ( ! OpsysAndVerMacroDef.psz) OpsysAndVerMacroDef.psz = UnsetString;
	OpsysMajorVerMacroDef.psz = param( "OPSYSMAJORVER" );
	if ( ! OpsysMajorVerMacroDef.psz) OpsysMajorVerMacroDef.psz = UnsetString;
	OpsysVerMacroDef.psz = param( "OPSYSVER" );
	if ( ! OpsysVerMacroDef.psz) OpsysVerMacroDef.psz = UnsetString;

	SpoolMacroDef.psz = param( "SPOOL" );
	if( SpoolMacroDef.psz == NULL ) {
		SpoolMacroDef.psz = UnsetString;
		ret = "SPOOL not specified in config file";
	}

	return ret;
}

void SubmitHash::init()
{
	clear();
	SubmitMacroSet.sources.push_back("<Detected>");
	SubmitMacroSet.sources.push_back("<Default>");
	SubmitMacroSet.sources.push_back("<Argument>");
	SubmitMacroSet.sources.push_back("<Live>");

	// in case this hasn't happened already.
	init_submit_default_macros();

	// Should we even init this?  condor_submit didn't...
	should_transfer = STF_IF_NEEDED;
	JobRequirements.clear();
	JobIwd.clear();
	mctx.cwd = NULL;
}

void SubmitHash::clear()
{
	if (SubmitMacroSet.table) {
		memset(SubmitMacroSet.table, 0, sizeof(SubmitMacroSet.table[0]) * SubmitMacroSet.allocation_size);
	}
	if (SubmitMacroSet.metat) {
		memset(SubmitMacroSet.metat, 0, sizeof(SubmitMacroSet.metat[0]) * SubmitMacroSet.allocation_size);
	}
	if (SubmitMacroSet.defaults && SubmitMacroSet.defaults->metat) {
		memset(SubmitMacroSet.defaults->metat, 0, sizeof(SubmitMacroSet.defaults->metat[0]) * SubmitMacroSet.defaults->size);
	}
	SubmitMacroSet.size = 0;
	SubmitMacroSet.sorted = 0;
	SubmitMacroSet.apool.clear();
	SubmitMacroSet.sources.clear();
	setup_macro_defaults(); // setup a defaults table for the macro_set. have to re-do this because we cleared the apool
}


int SubmitHash::SetPerFileEncryption()
{
	RETURN_IF_ABORT();

	auto_free_ptr files;
	files.set(submit_param( SUBMIT_KEY_EncryptInputFiles, "EncryptInputFiles"));
	if (files) {
		InsertJobExprString(ATTR_ENCRYPT_INPUT_FILES, files.ptr());
		NeedsPerFileEncryption = true;
	}
	RETURN_IF_ABORT();

	files.set(submit_param( SUBMIT_KEY_EncryptOutputFiles, "EncryptOutputFiles"));
	if (files) {
		InsertJobExprString( ATTR_ENCRYPT_OUTPUT_FILES, files.ptr());
		NeedsPerFileEncryption = true;
	}
	RETURN_IF_ABORT();

	files.set(submit_param( SUBMIT_KEY_DontEncryptInputFiles, "DontEncryptInputFiles"));
	if (files) {
		InsertJobExprString( ATTR_DONT_ENCRYPT_INPUT_FILES, files.ptr());
		NeedsPerFileEncryption = true;
	}
	RETURN_IF_ABORT();

	files.set(submit_param( SUBMIT_KEY_DontEncryptOutputFiles, "DontEncryptOutputFiles"));
	if (files) {
		InsertJobExprString( ATTR_DONT_ENCRYPT_OUTPUT_FILES, files.ptr());
		NeedsPerFileEncryption = true;
	}
	RETURN_IF_ABORT();

	return 0;
}


int SubmitHash::SetFetchFiles()
{
	RETURN_IF_ABORT();

	auto_free_ptr files(submit_param(SUBMIT_KEY_FetchFiles, ATTR_FETCH_FILES ));
	if (files) {
		InsertJobExprString(ATTR_FETCH_FILES, files.ptr());
	}

	return abort_code;
}

int SubmitHash::SetCompressFiles()
{
	RETURN_IF_ABORT();
	char *value;

	value = submit_param( SUBMIT_KEY_CompressFiles, ATTR_COMPRESS_FILES );
	if(value) {
		InsertJobExprString(ATTR_COMPRESS_FILES,value);
	}

	return 0;
}

int SubmitHash::SetAppendFiles()
{
	RETURN_IF_ABORT();
	char *value;

	value = submit_param( SUBMIT_KEY_AppendFiles, ATTR_APPEND_FILES );
	if(value) {
		InsertJobExprString(ATTR_APPEND_FILES,value);
	}

	return 0;
}

int SubmitHash::SetLocalFiles()
{
	RETURN_IF_ABORT();
	char *value;

	value = submit_param( SUBMIT_KEY_LocalFiles, ATTR_LOCAL_FILES );
	if(value) {
		InsertJobExprString(ATTR_LOCAL_FILES,value);
	}

	return 0;
}

int SubmitHash::SetJarFiles()
{
	RETURN_IF_ABORT();
	char *value;

	value = submit_param( SUBMIT_KEY_JarFiles, ATTR_JAR_FILES );
	if(value) {
		InsertJobExprString(ATTR_JAR_FILES,value);
	}

	return 0;
}

int SubmitHash::SetJavaVMArgs()
{
	RETURN_IF_ABORT();

	ArgList args;
	MyString error_msg;
	MyString strbuffer;
	MyString value;
	char *args1 = submit_param(SUBMIT_KEY_JavaVMArgs); // for backward compatibility
	char *args1_ext=submit_param(SUBMIT_KEY_JavaVMArguments1,ATTR_JOB_JAVA_VM_ARGS1);
		// NOTE: no ATTR_JOB_JAVA_VM_ARGS2 in the following,
		// because that is the same as JavaVMArguments1.
	char *args2 = submit_param( SUBMIT_KEY_JavaVMArguments2 );
	bool allow_arguments_v1 = submit_param_bool(SUBMIT_KEY_AllowArgumentsV1, NULL, false);

	if(args1_ext && args1) {
		push_error(stderr, "you specified a value for both %s and %s.\n",
				SUBMIT_KEY_JavaVMArgs, SUBMIT_KEY_JavaVMArguments1);
		ABORT_AND_RETURN( 1 );
	}
	RETURN_IF_ABORT();

	if(args1_ext) {
		free(args1);
		args1 = args1_ext;
		args1_ext = NULL;
	}

	if(args2 && args1 && ! allow_arguments_v1) {
		push_error(stderr, "If you wish to specify both 'java_vm_arguments' and\n"
		 "'java_vm_arguments2' for maximal compatibility with different\n"
		 "versions of Condor, then you must also specify\n"
		 "allow_arguments_v1=true.\n");
		ABORT_AND_RETURN(1);
	}

	bool args_success = true;

	if(args2) {
		args_success = args.AppendArgsV2Quoted(args2,&error_msg);
	}
	else if(args1) {
		args_success = args.AppendArgsV1WackedOrV2Quoted(args1,&error_msg);
	}

	if(!args_success) {
		push_error(stderr,"failed to parse java VM arguments: %s\n"
				"The full arguments you specified were %s\n",
				error_msg.Value(),args2 ? args2 : args1);
		ABORT_AND_RETURN( 1 );
	}

	// since we don't care about what version the schedd needs if it
	// is not present, we just default to no.... this only happens
	// in the case when we are dumping to a file.
	bool MyCondorVersionRequiresV1 = args.InputWasV1() || args.CondorVersionRequiresV1(getScheddVersion());
	if( MyCondorVersionRequiresV1 ) {
		args_success = args.GetArgsStringV1Raw(&value,&error_msg);
		if(!value.IsEmpty()) {
			strbuffer.formatstr("%s = \"%s\"",ATTR_JOB_JAVA_VM_ARGS1,
						   value.EscapeChars("\"",'\\').Value());
			InsertJobExpr (strbuffer);
		}
	}
	else {
		args_success = args.GetArgsStringV2Raw(&value,&error_msg);
		if(!value.IsEmpty()) {
			strbuffer.formatstr("%s = \"%s\"",ATTR_JOB_JAVA_VM_ARGS2,
						   value.EscapeChars("\"",'\\').Value());
			InsertJobExpr (strbuffer);
		}
	}

	if(!args_success) {
		push_error(stderr, "failed to insert java vm arguments into "
				"ClassAd: %s\n",error_msg.Value());
		ABORT_AND_RETURN( 1 );
	}

	free(args1);
	free(args2);
	free(args1_ext);

	return 0;
}

int SubmitHash::SetParallelStartupScripts() //JDB
{
	RETURN_IF_ABORT();

	char *value;

	value = submit_param( SUBMIT_KEY_ParallelScriptShadow, ATTR_PARALLEL_SCRIPT_SHADOW );
	if(value) {
		InsertJobExprString(ATTR_PARALLEL_SCRIPT_SHADOW,value);
	}
	value = submit_param( SUBMIT_KEY_ParallelScriptStarter,
						  ATTR_PARALLEL_SCRIPT_STARTER );
	if(value) {
		InsertJobExprString(ATTR_PARALLEL_SCRIPT_STARTER,value);
	}

	return 0;
}

int SubmitHash::check_open(_submit_file_role role,  const char *name, int flags )
{
	MyString strPathname;
	StringList *list;

		/* The user can disable file checks on a per job basis, in such a
		   case we avoid adding the files to CheckFilesWrite/Read.
		*/
	if ( JobDisableFileChecks ) return 0;

	/* No need to check for existence of the Null file. */
	if( strcmp(name, NULL_FILE) == MATCH ) {
		return 0;
	}

	if ( IsUrl( name ) || strstr(name, "$$(") ) {
		return 0;
	}

	strPathname = full_path(name);

	// is the last character a path separator?
	int namelen = (int)strlen(name);
	bool trailing_slash = namelen > 0 && IS_ANY_DIR_DELIM_CHAR(name[namelen-1]);

		/* This is only for MPI.  We test for our string that
		   we replaced "$(NODE)" with, and replace it with "0".  Thus, 
		   we will really only try and access the 0th file only */
	if ( JobUniverse == CONDOR_UNIVERSE_MPI ) {
		strPathname.replaceString("#MpInOdE#", "0");
	} else if ( JobUniverse == CONDOR_UNIVERSE_PARALLEL ) {
		strPathname.replaceString("#pArAlLeLnOdE#", "0");
	}


	/* If this file as marked as append-only, do not truncate it here */

	auto_free_ptr append_files(submit_param( SUBMIT_KEY_AppendFiles, ATTR_APPEND_FILES ));
	if (append_files.ptr()) {
		list = new StringList(append_files.ptr(), ",");
		if(list->contains_withwildcard(name)) {
			flags = flags & ~O_TRUNC;
		}
		delete list;
	}

	bool dryrun_create = FakeFileCreationChecks && ((flags & (O_CREAT|O_TRUNC)) != 0);
	if (FakeFileCreationChecks) {
		flags = flags & ~(O_CREAT|O_TRUNC);
	}

	if ( !DisableFileChecks ) {
		int fd = safe_open_wrapper_follow(strPathname.Value(),flags | O_LARGEFILE,0664);
		if ((fd < 0) && (errno == ENOENT) && dryrun_create) {
			// we are doing dry-run, and the input flags were to create/truncate a file
			// we stripped the create/truncate flags, now we treate a 'file does not exist' error
			// as success since O_CREAT would have made it (probably).
		} else if ( fd < 0 ) {
			// note: Windows does not set errno to EISDIR for directories, instead you get back EACCESS (or ENOENT?)
			if( (trailing_slash || errno == EISDIR || errno == EACCES) &&
	                   check_directory( strPathname.Value(), flags, errno ) ) {
					// Entries in the transfer output list may be
					// files or directories; no way to tell in
					// advance.  When there is already a directory by
					// the same name, it is not obvious what to do.
					// Therefore, we will just do nothing here and leave
					// it up to the runtime to nicely report errors.
				return 0;
			}
			push_error(stderr, "Can't open \"%s\"  with flags 0%o (%s)\n",
					 strPathname.Value(), flags, strerror( errno ) );
			ABORT_AND_RETURN( 1 );
		} else {
			(void)close( fd );
		}
	}

#if 1
	if (FnCheckFile) {
		FnCheckFile(CheckFileArg, this, role, strPathname.Value(), flags);
	}
#else
	// Queue files for testing access if not already queued
	int junk;
	if( flags & O_WRONLY )
	{
		if ( CheckFilesWrite.lookup(strPathname,junk) < 0 ) {
			// this file not found in our list; add it
			CheckFilesWrite.insert(strPathname,junk);
		}
	}
	else
	{
		if ( CheckFilesRead.lookup(strPathname,junk) < 0 ) {
			// this file not found in our list; add it
			CheckFilesRead.insert(strPathname,junk);
		}
	}
#endif
	return 0;
}


int SubmitHash::SetStdFile( int which_file )
{
	RETURN_IF_ABORT();

	bool	transfer_it = true;
	bool	stream_it = false;
	char	*macro_value = NULL;
	char	*macro_value2 = NULL;
	const char	*generic_name;
	MyString	 strbuffer;

	switch( which_file ) 
	{
	case 0:
		generic_name = SUBMIT_KEY_Input;
		macro_value = submit_param( SUBMIT_KEY_TransferInput, ATTR_TRANSFER_INPUT );
		macro_value2 = submit_param( SUBMIT_KEY_StreamInput, ATTR_STREAM_INPUT );
		break;
	case 1:
		generic_name = SUBMIT_KEY_Output;
		macro_value = submit_param( SUBMIT_KEY_TransferOutput, ATTR_TRANSFER_OUTPUT );
		macro_value2 = submit_param( SUBMIT_KEY_StreamOutput, ATTR_STREAM_OUTPUT );
		break;
	case 2:
		generic_name = SUBMIT_KEY_Error;
		macro_value = submit_param( SUBMIT_KEY_TransferError, ATTR_TRANSFER_ERROR );
		macro_value2 = submit_param( SUBMIT_KEY_StreamError, ATTR_STREAM_ERROR );
		break;
	default:
		push_error(stderr, "Unknown standard file descriptor (%d)\n",
				 which_file ); 
		ABORT_AND_RETURN( 1 );
	}
	RETURN_IF_ABORT();

	if ( macro_value ) {
		if ( macro_value[0] == 'F' || macro_value[0] == 'f' ) {
			transfer_it = false;
		}
		free( macro_value );
	}

	if ( macro_value2 ) {
		if ( macro_value2[0] == 'T' || macro_value2[0] == 't' ) {
			stream_it = true;
		} else if( macro_value2[0] == 'F' || macro_value2[0] == 'f' ) {
			stream_it = false;
		}
		free( macro_value2 );
	}

	macro_value = submit_param( generic_name, NULL );

	/* Globus jobs are allowed to specify urls */
	if(JobUniverse == CONDOR_UNIVERSE_GRID && is_globus_friendly_url(macro_value)) {
		transfer_it = false;
		stream_it = false;
	}
	
	if( !macro_value || *macro_value == '\0') 
	{
		transfer_it = false;
		stream_it = false;
		// always canonicalize to the UNIX null file (i.e. /dev/null)
		macro_value = strdup(UNIX_NULL_FILE);
	}else if( strcmp(macro_value,UNIX_NULL_FILE)==0 ) {
		transfer_it = false;
		stream_it = false;
	}else {
		if( JobUniverse == CONDOR_UNIVERSE_VM ) {
			push_error(stderr, "You cannot use input, ouput, "
					"and error parameters in the submit description "
					"file for vm universe\n");
			ABORT_AND_RETURN( 1 );
		}
	}
	
	if( has_whitespace(macro_value) ) 
	{
		push_error(stderr, "The '%s' takes exactly one argument (%s)\n", 
				 generic_name, macro_value );
		free(macro_value);
		ABORT_AND_RETURN( 1 );
	}	

	MyString tmp = macro_value;
	if ( check_and_universalize_path(tmp) != 0 ) {
		// we changed the value, so we have to update macro_value
		free(macro_value);
		macro_value = strdup(tmp.Value());
	}
	
	switch( which_file ) 
	{
	case 0:
		strbuffer.formatstr( "%s = \"%s\"", ATTR_JOB_INPUT, macro_value);
		InsertJobExpr (strbuffer);
		if ( transfer_it ) {
			check_open( SFR_STDIN, macro_value, O_RDONLY );
			strbuffer.formatstr( "%s = %s", ATTR_STREAM_INPUT, stream_it ? "TRUE" : "FALSE" );
			InsertJobExpr( strbuffer.Value() );
		} else {
			strbuffer.formatstr( "%s = FALSE", ATTR_TRANSFER_INPUT );
			InsertJobExpr( strbuffer.Value() );
		}
		break;
	case 1:
		strbuffer.formatstr( "%s = \"%s\"", ATTR_JOB_OUTPUT, macro_value);
		InsertJobExpr (strbuffer);
		if ( transfer_it ) {
			check_open( SFR_STDOUT, macro_value, O_WRONLY|O_CREAT|O_TRUNC );
			strbuffer.formatstr( "%s = %s", ATTR_STREAM_OUTPUT, stream_it ? "TRUE" : "FALSE" );
			InsertJobExpr( strbuffer.Value() );
			StreamStdout = stream_it;
		} else {
			strbuffer.formatstr( "%s = FALSE", ATTR_TRANSFER_OUTPUT );
			InsertJobExpr( strbuffer.Value() );
		}
		break;
	case 2:
		strbuffer.formatstr( "%s = \"%s\"", ATTR_JOB_ERROR, macro_value);
		InsertJobExpr (strbuffer);
		if ( transfer_it ) {
			check_open( SFR_STDERR, macro_value, O_WRONLY|O_CREAT|O_TRUNC );
			strbuffer.formatstr( "%s = %s", ATTR_STREAM_ERROR, stream_it ? "TRUE" : "FALSE" );
			InsertJobExpr( strbuffer.Value() );
			StreamStderr = stream_it;
		} else {
			strbuffer.formatstr( "%s = FALSE", ATTR_TRANSFER_ERROR );
			InsertJobExpr( strbuffer.Value() );
		}
		break;
	}
		
	if( macro_value ) {
		free(macro_value);
	}

	return 0;
}


class SubmitHashEnvFilter : public Env
{
public:
	SubmitHashEnvFilter( const char *env1, const char *env2 )
			: m_env1( env1 ),
			  m_env2( env2 ) {
		return;
	};
	virtual ~SubmitHashEnvFilter( void ) { };
	virtual bool ImportFilter( const MyString &var,
							   const MyString &val ) const;
private:
	const char	*m_env1;
	const char	*m_env2;
};
bool SubmitHashEnvFilter::ImportFilter( const MyString & var, const MyString &val ) const
{
	if( !m_env2 && m_env1 && !IsSafeEnvV1Value(val.Value())) {
		// We silently filter out anything that is not expressible
		// in the 'environment1' syntax.  This avoids breaking
		// our ability to submit jobs to older startds that do
		// not support 'environment2' syntax.
		return false;
	}
	if( !IsSafeEnvV2Value(val.Value()) ) {
		// Silently filter out environment values containing
		// unsafe characters.  Example: newlines cause the
		// schedd to EXCEPT in 6.8.3.
		return false;
	}
	MyString existing_val;
	if ( GetEnv( var, existing_val ) ) {
		// Don't override submit file environment settings --
		// check if environment variable is already set.
		return false;
	}
	return true;
}

int SubmitHash::SetEnvironment()
{
	RETURN_IF_ABORT();

	char *env1 = submit_param( SUBMIT_KEY_Environment1, ATTR_JOB_ENVIRONMENT1 );
		// NOTE: no ATTR_JOB_ENVIRONMENT2 in the following,
		// because that is the same as Environment1
	char *env2 = submit_param( SUBMIT_KEY_Environment2 );
	bool allow_v1 = submit_param_bool( SUBMIT_KEY_AllowEnvironmentV1, NULL, false );
	char *shouldgetenv = submit_param( SUBMIT_KEY_GetEnvironment, "get_env" );
	char *allowscripts = submit_param( SUBMIT_KEY_AllowStartupScript, "AllowStartupScript" );
	SubmitHashEnvFilter envobject( env1, env2 );
    MyString varname;

	RETURN_IF_ABORT();
	if( env1 && env2 && !allow_v1 ) {
		push_error(stderr, "If you wish to specify both 'environment' and\n"
		 "'environment2' for maximal compatibility with different\n"
		 "versions of Condor, then you must also specify\n"
		 "allow_environment_v1=true.\n");
		ABORT_AND_RETURN(1);
	}

	bool env_success;
	char const *environment_string = env2 ? env2 : env1;
	MyString error_msg;
	if(env2) {
		env_success = envobject.MergeFromV2Quoted(env2,&error_msg);
	}
	else {
		env_success = envobject.MergeFromV1RawOrV2Quoted(env1,&error_msg);
	}

	if(!env_success) {
		push_error(stderr, "%s\nThe environment you specified was: '%s'\n",
				error_msg.Value(),environment_string);
		ABORT_AND_RETURN(1);
	}

	if (allowscripts && (*allowscripts=='T' || *allowscripts=='t') ) {
		envobject.SetEnv("_CONDOR_NOCHECK","1");
	}

	// grab user's environment if getenv == TRUE
	if ( shouldgetenv && (toupper(shouldgetenv[0]) == 'T') ) {
		envobject.Import( );
	}

	//There may already be environment info in the ClassAd from SUBMIT_ATTRS.
	//Check for that now.

	bool ad_contains_env1 = job->LookupExpr(ATTR_JOB_ENVIRONMENT1);
	bool ad_contains_env2 = job->LookupExpr(ATTR_JOB_ENVIRONMENT2);

	bool MyCondorVersionRequiresV1 = envobject.InputWasV1() || envobject.CondorVersionRequiresV1(getScheddVersion());
	bool insert_env1 = MyCondorVersionRequiresV1;
	bool insert_env2 = !insert_env1;

	if(!env1 && !env2 && envobject.Count() == 0 && \
	   (ad_contains_env1 || ad_contains_env2)) {
			// User did not specify any environment, but SUBMIT_ATTRS did.
			// Do not do anything (i.e. avoid overwriting with empty env).
		insert_env1 = insert_env2 = false;
	}

	// If we are going to write new environment into the ad and if there
	// is already environment info in the ad, make sure we overwrite
	// whatever is there.  Instead of doing things this way, we could
	// remove the existing environment settings, but there is currently no
	// function that undoes all the effects of InsertJobExpr().
	if(insert_env1 && ad_contains_env2) insert_env2 = true;
	if(insert_env2 && ad_contains_env1) insert_env1 = true;

	env_success = true;

	if(insert_env1 && env_success) {
		MyString newenv;
		MyString newenv_raw;

		env_success = envobject.getDelimitedStringV1Raw(&newenv_raw,&error_msg);
		newenv.formatstr("%s = \"%s\"",
					   ATTR_JOB_ENVIRONMENT1,
					   newenv_raw.EscapeChars("\"",'\\').Value());
		InsertJobExpr(newenv);

		// Record in the JobAd the V1 delimiter that is being used.
		// This way remote submits across platforms have a prayer.
		MyString delim_assign;
		delim_assign.formatstr("%s = \"%c\"",
							 ATTR_JOB_ENVIRONMENT1_DELIM,
							 envobject.GetEnvV1Delimiter());
		InsertJobExpr(delim_assign);
	}

	if(insert_env2 && env_success) {
		MyString newenv;
		MyString newenv_raw;

		env_success = envobject.getDelimitedStringV2Raw(&newenv_raw,&error_msg);
		newenv.formatstr("%s = \"%s\"",
					   ATTR_JOB_ENVIRONMENT2,
					   newenv_raw.EscapeChars("\"",'\\').Value());
		InsertJobExpr(newenv);
	}

	if(!env_success) {
		push_error(stderr, "failed to insert environment into job ad: %s\n",
				error_msg.Value());
		ABORT_AND_RETURN(1);
	}

	free(env2);
	free(env1);

	if( allowscripts ) {
		free(allowscripts);
	}
	if( shouldgetenv ) {
		free(shouldgetenv);
	}
	return 0;
}

int SubmitHash::SetRequirements()
{
	RETURN_IF_ABORT();

	char *requirements = submit_param( SUBMIT_KEY_Requirements, NULL );
	MyString tmp;
	MyString buffer;
	if( requirements == NULL ) 
	{
		JobRequirements = "";
	} 
	else 
	{
		JobRequirements = requirements;
		free(requirements);
	}

	check_requirements( JobRequirements.Value(), tmp );
	buffer.formatstr( "%s = %s", ATTR_REQUIREMENTS, tmp.Value());
	JobRequirements = tmp;

	InsertJobExpr (buffer);
	RETURN_IF_ABORT();
	
	MyString fs_domain;
	if( (should_transfer == STF_NO || should_transfer == STF_IF_NEEDED) 
		&& ! job->LookupString(ATTR_FILE_SYSTEM_DOMAIN, fs_domain) ) {
		param(fs_domain, "FILESYSTEM_DOMAIN");
		buffer.formatstr( "%s = \"%s\"", ATTR_FILE_SYSTEM_DOMAIN, fs_domain.c_str()); 
		InsertJobExpr( buffer );
		RETURN_IF_ABORT();
	}
	return 0;
}

int SubmitHash::SetEncryptExecuteDir()
{
	RETURN_IF_ABORT();
	HasEncryptExecuteDir = submit_param_bool( SUBMIT_KEY_EncryptExecuteDir, ATTR_ENCRYPT_EXECUTE_DIRECTORY, false );
	RETURN_IF_ABORT();

	MyString buf;
	buf.formatstr("%s = %s", ATTR_ENCRYPT_EXECUTE_DIRECTORY,
			HasEncryptExecuteDir ? "True" : "False");
	InsertJobExpr( buf.Value() );
	return 0;
}

int SubmitHash::SetTDP()
{
	RETURN_IF_ABORT();

		// tdp_cmd and tdp_input are global since those effect
		// SetRequirements() an SetTransferFiles(), too. 
	tdp_cmd.set(submit_param( SUBMIT_KEY_ToolDaemonCmd, ATTR_TOOL_DAEMON_CMD ));
	tdp_input.set(submit_param( SUBMIT_KEY_ToolDaemonInput, ATTR_TOOL_DAEMON_INPUT ));
	char* tdp_args1 = submit_param( SUBMIT_KEY_ToolDaemonArgs );
	char* tdp_args1_ext = submit_param( SUBMIT_KEY_ToolDaemonArguments1, ATTR_TOOL_DAEMON_ARGS1 );
		// NOTE: no ATTR_TOOL_DAEMON_ARGS2 in the following,
		// because that is the same as ToolDaemonArguments1.
	char* tdp_args2 = submit_param( SUBMIT_KEY_ToolDaemonArguments2 );
	bool allow_arguments_v1 = submit_param_bool(SUBMIT_KEY_AllowArgumentsV1, NULL, false);
	char* tdp_error = submit_param( SUBMIT_KEY_ToolDaemonError, ATTR_TOOL_DAEMON_ERROR );
	char* tdp_output = submit_param( SUBMIT_KEY_ToolDaemonOutput, 
									 ATTR_TOOL_DAEMON_OUTPUT );
	bool suspend_at_exec_exists = false;
	bool suspend_at_exec = submit_param_bool( SUBMIT_KEY_SuspendJobAtExec,
										  ATTR_SUSPEND_JOB_AT_EXEC, false, &suspend_at_exec_exists );
	RETURN_IF_ABORT();

	MyString buf;
	MyString path;

	if( tdp_cmd ) {
		HasTDP = true;
		path = tdp_cmd.ptr();
		check_and_universalize_path( path );
		buf.formatstr( "%s = \"%s\"", ATTR_TOOL_DAEMON_CMD, path.Value() );
		InsertJobExpr( buf.Value() );
	}
	if( tdp_input ) {
		path = tdp_input.ptr();
		check_and_universalize_path( path );
		buf.formatstr( "%s = \"%s\"", ATTR_TOOL_DAEMON_INPUT, path.Value() );
		InsertJobExpr( buf.Value() );
	}
	if( tdp_output ) {
		path = tdp_output;
		check_and_universalize_path( path );
		buf.formatstr( "%s = \"%s\"", ATTR_TOOL_DAEMON_OUTPUT, path.Value() );
		InsertJobExpr( buf.Value() );
		free( tdp_output );
		tdp_output = NULL;
	}
	if( tdp_error ) {
		path = tdp_error;
		check_and_universalize_path( path );
		buf.formatstr( "%s = \"%s\"", ATTR_TOOL_DAEMON_ERROR, path.Value() );
		InsertJobExpr( buf.Value() );
		free( tdp_error );
		tdp_error = NULL;
	}

	bool args_success = true;
	MyString error_msg;
	ArgList args;

	if(tdp_args1_ext && tdp_args1) {
		push_error(stderr, "you specified both tdp_daemon_args and tdp_daemon_arguments\n");
		ABORT_AND_RETURN(1);
	}
	if(tdp_args1_ext) {
		free(tdp_args1);
		tdp_args1 = tdp_args1_ext;
		tdp_args1_ext = NULL;
	}

	if(tdp_args2 && tdp_args1 && ! allow_arguments_v1) {
		push_error(stderr, "If you wish to specify both 'tool_daemon_arguments' and\n"
		 "'tool_daemon_arguments2' for maximal compatibility with different\n"
		 "versions of Condor, then you must also specify\n"
		 "allow_arguments_v1=true.\n");
		ABORT_AND_RETURN(1);
	}

	if( tdp_args2 ) {
		args_success = args.AppendArgsV2Quoted(tdp_args2,&error_msg);
	}
	else if( tdp_args1 ) {
		args_success = args.AppendArgsV1WackedOrV2Quoted(tdp_args1,&error_msg);
	}

	if(!args_success) {
		push_error(stderr,"failed to parse tool daemon arguments: %s\n"
				"The arguments you specified were: %s\n",
				error_msg.Value(),
				tdp_args2 ? tdp_args2 : tdp_args1);
		ABORT_AND_RETURN(1);
	}

	MyString args_value;
	bool MyCondorVersionRequiresV1 = args.InputWasV1() || args.CondorVersionRequiresV1(getScheddVersion());
	if(MyCondorVersionRequiresV1) {
		args_success = args.GetArgsStringV1Raw(&args_value,&error_msg);
		if(!args_value.IsEmpty()) {
			buf.formatstr("%s = \"%s\"",ATTR_TOOL_DAEMON_ARGS1,
						args_value.EscapeChars("\"",'\\').Value());
			InsertJobExpr( buf );
		}
	}
	else if(args.Count()) {
		args_success = args.GetArgsStringV2Raw(&args_value,&error_msg);
		if(!args_value.IsEmpty()) {
			buf.formatstr("%s = \"%s\"",ATTR_TOOL_DAEMON_ARGS2,
						args_value.EscapeChars("\"",'\\').Value());
			InsertJobExpr( buf );
		}
	}

	if(!args_success) {
		push_error(stderr, "failed to insert tool daemon arguments: %s\n",
				error_msg.Value());
		ABORT_AND_RETURN(1);
	}

	if( suspend_at_exec_exists ) {
		job->Assign(ATTR_SUSPEND_JOB_AT_EXEC, suspend_at_exec);
	}

	free(tdp_args1);
	free(tdp_args2);
	free(tdp_args1_ext);
	return 0;
}

int SubmitHash::SetRunAsOwner()
{
	RETURN_IF_ABORT();
	bool defined = false;
	bool bRunAsOwner = submit_param_bool(SUBMIT_KEY_RunAsOwner, ATTR_JOB_RUNAS_OWNER, false, &defined);
	RETURN_IF_ABORT();
	if ( ! defined) return 0;

	job->Assign(ATTR_JOB_RUNAS_OWNER, bRunAsOwner);

#if defined(WIN32)
	// make sure we have a CredD
	// (RunAsOwner is global for use in SetRequirements(),
	//  the memory is freed() there)
	if( bRunAsOwner ) {
		RunAsOwnerCredD.set(param("CREDD_HOST"));
		if ( ! RunAsOwnerCredD) {
			push_error(stderr, "run_as_owner requires a valid CREDD_HOST configuration macro\n");
			ABORT_AND_RETURN(1);
		}
	}
#endif
	return 0;
}

int  SubmitHash::SetLoadProfile()
{
	RETURN_IF_ABORT();
	bool load_profile = submit_param_bool (SUBMIT_KEY_LoadProfile,  ATTR_JOB_LOAD_PROFILE, false );
	RETURN_IF_ABORT();

	if (load_profile ) {
		job->Assign(ATTR_JOB_LOAD_PROFILE, true);
	}

    /* If we've been called it means that SetRunAsOwner() has already 
    made sure we have a CredD.  This will become more important when
    we allow random users to load their profile (which will only at 
    that point be the user's registry).  The limitation is to stop
    heavy weight users profiles; you know the ones: they have20+ GBs 
    of music, thousands of pictures and a few random movies from 
    caching their profile on the local machine (which may be someone's
    laptop, which may already be running low on disk-space). */

	return 0;
}

int SubmitHash::SetRank()
{
	RETURN_IF_ABORT();

	MyString rank;
	char *orig_pref = submit_param( SUBMIT_KEY_Preferences, NULL );
	char *orig_rank = submit_param( SUBMIT_KEY_Rank, NULL );
	char *default_rank = NULL;
	char *append_rank = NULL;
	MyString buffer;

	switch( JobUniverse ) {
	case CONDOR_UNIVERSE_STANDARD:
		default_rank = param( "DEFAULT_RANK_STANDARD" );
		append_rank = param( "APPEND_RANK_STANDARD" );
		break;
	case CONDOR_UNIVERSE_VANILLA:
		default_rank = param( "DEFAULT_RANK_VANILLA" );
		append_rank = param( "APPEND_RANK_VANILLA" );
		break;
	default:
		default_rank = NULL;
		append_rank = NULL;
	}

		// If they're not yet defined, or they're defined but empty,
		// try the generic, non-universe-specific versions.
	if( ! default_rank || ! default_rank[0]  ) {
		if (default_rank) { free(default_rank); default_rank = NULL; }
		default_rank = param("DEFAULT_RANK");
	}
	if( ! append_rank || ! append_rank[0]  ) {
		if (append_rank) { free(append_rank); append_rank = NULL; }
		append_rank = param("APPEND_RANK");
	}

		// If any of these are defined but empty, treat them as
		// undefined, or else, we get nasty errors.  -Derek W. 8/21/98
	if( default_rank && !default_rank[0] ) {
		free(default_rank);
		default_rank = NULL;
	}
	if( append_rank && !append_rank[0] ) {
		free(append_rank);
		append_rank = NULL;
	}

		// If we've got a rank to append to something that's already 
		// there, we need to enclose the origional in ()'s
	if( append_rank && (orig_rank || orig_pref || default_rank) ) {
		rank += "(";
	}		

	if( orig_pref && orig_rank ) {
		push_error(stderr, "%s and %s may not both be specified "
				 "for a job\n", SUBMIT_KEY_Preferences, SUBMIT_KEY_Rank );
		ABORT_AND_RETURN( 1 );
	} else if( orig_rank ) {
		rank +=  orig_rank;
	} else if( orig_pref ) {
		rank += orig_pref;
	} else if( default_rank ) {
		rank += default_rank;
	} 

	if( append_rank ) {
		if( rank.Length() > 0 ) {
				// We really want to add this expression to our
				// existing Rank expression, not && it, since Rank is
				// just a float.  If we && it, we're forcing the whole
				// expression to now be a bool that evaluates to
				// either 0 or 1, which is obviously not what we
				// want.  -Derek W. 8/21/98
			rank += ") + (";
		} else {
			rank +=  "(";
		}
		rank += append_rank;
		rank += ")";
	}
				
	if( rank.Length() == 0 ) {
		buffer.formatstr( "%s = 0.0", ATTR_RANK );
		InsertJobExpr( buffer );
	} else {
		buffer.formatstr( "%s = %s", ATTR_RANK, rank.Value() );
		InsertJobExpr( buffer );
	}

	if ( orig_pref ) 
		free(orig_pref);
	if ( orig_rank )
		free(orig_rank);

	if (default_rank) {
		free(default_rank);
		default_rank = NULL;
	}
	if (append_rank) {
		free(append_rank);
		append_rank = NULL;
	}
	return 0;
}

int SubmitHash::SetUserLog()
{
	RETURN_IF_ABORT();

	static const char* const submit_names[] = { SUBMIT_KEY_UserLogFile, SUBMIT_KEY_DagmanLogFile, 0 };
	static const char* const jobad_attribute_names[] = { ATTR_ULOG_FILE, ATTR_DAGMAN_WORKFLOW_LOG, 0 };
	for(const char* const* p = &submit_names[0], *const*q = &jobad_attribute_names[0];
			*p && *q; ++p, ++q) {
		char *ulog_entry = submit_param( *p, *q );

		if ( ulog_entry && strcmp( ulog_entry, "" ) != 0 ) {
			std::string buffer;
				// Note:  I don't think the return value here can ever
				// be NULL.  wenger 2016-10-07
			const char* ulog_pcc = full_path( ulog_entry );
			if(ulog_pcc) {
#if 1
				if (FnCheckFile) {
					int rval = FnCheckFile(CheckFileArg, this, SFR_LOG, ulog_pcc, O_APPEND);
					if (rval) { ABORT_AND_RETURN( rval ); }
				}
#else
				std::string ulog(ulog_pcc);
				if ( !DumpClassAdToFile && !DashDryRun ) {
					// check that the log is a valid path
					if ( !DisableFileChecks ) {
						FILE* test = safe_fopen_wrapper_follow(ulog.c_str(), "a+", 0664);
						if (!test) {
							push_error(stderr, "Invalid log file: \"%s\" (%s)\n", ulog.c_str(),
									strerror(errno));
							ABORT_AND_RETURN( 1 );
						} else {
							fclose(test);
						}
					}

					// Check that the log file isn't on NFS
					bool nfs_is_error = param_boolean("LOG_ON_NFS_IS_ERROR", false);
					bool nfs = false;

					if ( nfs_is_error ) {
						if ( fs_detect_nfs( ulog.c_str(), &nfs ) != 0 ) {
							push_warning(stderr,
									"Can't determine whether log file %s is on NFS\n",
									ulog.c_str() );
						} else if ( nfs ) {

							push_error(stderr, "Log file %s is on NFS.\nThis could cause"
									" log file corruption. Condor has been configured to"
									" prohibit log files on NFS.\n",
									ulog.c_str() );

							ABORT_AND_RETURN( 1 );

						}
					}
				}
#endif

				MyString mulog(ulog_pcc);
				check_and_universalize_path(mulog);
				buffer += mulog.Value();
				UserLogSpecified = true;
			}
			std::string logExpr(*q);
			logExpr += " = ";
			logExpr += "\"";
			logExpr += buffer;
			logExpr += "\"";
			InsertJobExpr(logExpr.c_str());
			free(ulog_entry);
		}
	}
	return 0;
}

int SubmitHash::SetUserLogXML()
{
	RETURN_IF_ABORT();
	bool xml_exists;
	bool use_xml = submit_param_bool(SUBMIT_KEY_UseLogUseXML, ATTR_ULOG_USE_XML, false, &xml_exists);
	if (xml_exists) {
		AssignJobVal(ATTR_ULOG_USE_XML, use_xml);
	}
	return 0;
}


int SubmitHash::SetCoreSize()
{
	RETURN_IF_ABORT();
	char *size = submit_param( SUBMIT_KEY_CoreSize, "core_size" );
	RETURN_IF_ABORT();

	long coresize = 0;
	MyString buffer;

	if (size == NULL) {
#if defined(HPUX) || defined(WIN32) /* RLIMIT_CORE not supported */
		size = "";
#else
		struct rlimit rl;
		if ( getrlimit( RLIMIT_CORE, &rl ) == -1) {
			push_error(stderr, "getrlimit failed");
			abort_code = 1;
			return abort_code;
		}

		// this will effectively become the hard limit for core files when
		// the job is executed
		coresize = (long)rl.rlim_cur;
#endif
	} else {
		coresize = atoi(size);
		free(size);
	}

	buffer.formatstr( "%s = %ld", ATTR_CORE_SIZE, coresize);
	InsertJobExpr(buffer);
	return 0;
}


int SubmitHash::SetJobLease()
{
	RETURN_IF_ABORT();

	long lease_duration = 0;
	auto_free_ptr tmp(submit_param( "job_lease_duration", ATTR_JOB_LEASE_DURATION ));
	if( ! tmp ) {
		if( universeCanReconnect(JobUniverse)) {
				/*
				  if the user didn't define a lease duration, but is
				  submitting a job from a universe that supports
				  reconnect and is NOT trying to use streaming I/O, we
				  want to define a default of 20 minutes so that
				  reconnectable jobs can survive schedd crashes and
				  the like...
				*/
			tmp.set(param("JOB_DEFAULT_LEASE_DURATION"));
		} else {
				// not defined and can't reconnect, we're done.
			return 0;
		}
	}

	// first try parsing as an integer
	if (tmp)
	{
		char *endptr = NULL;
		lease_duration = strtol(tmp.ptr(), &endptr, 10);
		if (endptr != tmp.ptr()) {
			while (isspace(*endptr)) {
				endptr++;
			}
		}
		bool is_number = (endptr != tmp.ptr() && *endptr == '\0');
		if ( ! is_number) {
			lease_duration = 0; // set to zero to indicate it's an expression
		} else {
			if (lease_duration == 0) {
				// User explicitly didn't want a lease, so we're done.
				return 0;
			}
			if (lease_duration < 20) {
				if (! already_warned_job_lease_too_small) { 
					push_warning(stderr, "%s less than 20 seconds is not allowed, using 20 instead\n",
							ATTR_JOB_LEASE_DURATION);
					already_warned_job_lease_too_small = true;
				}
				lease_duration = 20;
			}
		}
	}
	// if lease duration was an integer, lease_duration will have the value.
	if (lease_duration) {
		AssignJobVal(ATTR_JOB_LEASE_DURATION, lease_duration);
	} else if (tmp) {
		// lease is defined but not an int, try setting it as an expression
		MyString buffer = ATTR_JOB_LEASE_DURATION;
		buffer += "=";
		buffer += tmp.ptr();
		InsertJobExpr(buffer.c_str());
	}
	return 0;
}


int SubmitHash::SetJobStatus()
{
	RETURN_IF_ABORT();

	bool hold = submit_param_bool( SUBMIT_KEY_Hold, NULL, false );
	MyString buffer;

#ifdef PLUS_ATTRIBS_IN_CLUSTER_AD
#else
	// we will be inserting "JobStatus = <something>" into the jobad, but we DON'T
	// want that to be written into the cluster ad, it must always go into the proc ad.
	SetNoClusterAttr(ATTR_JOB_STATUS);
#endif

	if (hold) {
		if ( IsRemoteJob ) {
			push_error(stderr, "Cannot set '%s' to 'true' when using -remote or -spool\n", 
					 SUBMIT_KEY_Hold );
			ABORT_AND_RETURN( 1 );
		}
		buffer.formatstr( "%s = %d", ATTR_JOB_STATUS, HELD);
		InsertJobExpr (buffer);

		buffer.formatstr( "%s=\"submitted on hold at user's request\"", 
					   ATTR_HOLD_REASON );
		InsertJobExpr( buffer );

		buffer.formatstr( "%s = %d", ATTR_HOLD_REASON_CODE,
				 CONDOR_HOLD_CODE_SubmittedOnHold );
		InsertJobExpr( buffer );
	} else 
	if ( IsRemoteJob ) {
		buffer.formatstr( "%s = %d", ATTR_JOB_STATUS, HELD);
		InsertJobExpr (buffer);

		buffer.formatstr( "%s=\"Spooling input data files\"", 
					   ATTR_HOLD_REASON );
		InsertJobExpr( buffer );

		buffer.formatstr( "%s = %d", ATTR_HOLD_REASON_CODE,
				 CONDOR_HOLD_CODE_SpoolingInput );
		InsertJobExpr( buffer );
	} else {
		buffer.formatstr( "%s = %d", ATTR_JOB_STATUS, IDLE);
		InsertJobExpr (buffer);
	}

	AssignJobVal(ATTR_ENTERED_CURRENT_STATUS, submit_time);
	return 0;
}

int SubmitHash::SetPriority()
{
	RETURN_IF_ABORT();

	int prioval = submit_param_int( SUBMIT_KEY_Priority, ATTR_PRIO, 0 );
	RETURN_IF_ABORT();

	AssignJobVal(ATTR_JOB_PRIO, prioval);

	IsNiceUser = submit_param_bool( SUBMIT_KEY_NiceUser, ATTR_NICE_USER, false );
	RETURN_IF_ABORT();

	AssignJobVal(ATTR_NICE_USER, IsNiceUser);
	return 0;
}

int SubmitHash::SetMaxJobRetirementTime()
{
	RETURN_IF_ABORT();

	// Assume that SetPriority() has been called before getting here.
	const char *value = NULL;

	value = submit_param( SUBMIT_KEY_MaxJobRetirementTime, ATTR_MAX_JOB_RETIREMENT_TIME );
	if(!value && (IsNiceUser || JobUniverse == 1)) {
		// Regardless of the startd graceful retirement policy,
		// nice_user and standard universe jobs that do not specify
		// otherwise will self-limit their retirement time to 0.  So
		// the user plays nice by default, but they have the option to
		// override this (assuming, of course, that the startd policy
		// actually gives them any retirement time to play with).

		value = "0";
	}
	if(value) {
		MyString expr;
		expr.formatstr("%s = %s",ATTR_MAX_JOB_RETIREMENT_TIME, value);
		InsertJobExpr(expr);
	}
	return 0;
}

int SubmitHash::SetPeriodicHoldCheck()
{
	RETURN_IF_ABORT();

	char *phc = submit_param(SUBMIT_KEY_PeriodicHoldCheck, ATTR_PERIODIC_HOLD_CHECK);
	MyString buffer;

	if (phc == NULL)
	{
		/* user didn't have one, so add one */
		buffer.formatstr( "%s = FALSE", ATTR_PERIODIC_HOLD_CHECK );
	}
	else
	{
		/* user had a value for it, leave it alone */
		buffer.formatstr( "%s = %s", ATTR_PERIODIC_HOLD_CHECK, phc );
		free(phc);
	}

	InsertJobExpr( buffer );

	phc = submit_param(SUBMIT_KEY_PeriodicHoldReason, ATTR_PERIODIC_HOLD_REASON);
	if( phc ) {
		buffer.formatstr( "%s = %s", ATTR_PERIODIC_HOLD_REASON, phc );
		InsertJobExpr( buffer );
		free(phc);
	}

	phc = submit_param(SUBMIT_KEY_PeriodicHoldSubCode, ATTR_PERIODIC_HOLD_SUBCODE);
	if( phc ) {
		buffer.formatstr( "%s = %s", ATTR_PERIODIC_HOLD_SUBCODE, phc );
		InsertJobExpr( buffer );
		free(phc);
	}

	phc = submit_param(SUBMIT_KEY_PeriodicReleaseCheck, ATTR_PERIODIC_RELEASE_CHECK);

	if (phc == NULL)
	{
		/* user didn't have one, so add one */
		buffer.formatstr( "%s = FALSE", ATTR_PERIODIC_RELEASE_CHECK );
	}
	else
	{
		/* user had a value for it, leave it alone */
		buffer.formatstr( "%s = %s", ATTR_PERIODIC_RELEASE_CHECK, phc );
		free(phc);
	}

	InsertJobExpr( buffer );
	RETURN_IF_ABORT();

	return 0;
}

int SubmitHash::SetPeriodicRemoveCheck()
{
	RETURN_IF_ABORT();

	char *prc = submit_param(SUBMIT_KEY_PeriodicRemoveCheck, ATTR_PERIODIC_REMOVE_CHECK);
	MyString buffer;

	if (prc == NULL)
	{
		/* user didn't have one, so add one */
		buffer.formatstr( "%s = FALSE", ATTR_PERIODIC_REMOVE_CHECK );
	}
	else
	{
		/* user had a value for it, leave it alone */
		buffer.formatstr( "%s = %s", ATTR_PERIODIC_REMOVE_CHECK, prc );
		free(prc);
	}

	prc = submit_param(SUBMIT_KEY_OnExitHoldReason, ATTR_ON_EXIT_HOLD_REASON);
	if( prc ) {
		buffer.formatstr( "%s = %s", ATTR_ON_EXIT_HOLD_REASON, prc );
		InsertJobExpr( buffer );
		free(prc);
	}

	prc = submit_param(SUBMIT_KEY_OnExitHoldSubCode, ATTR_ON_EXIT_HOLD_SUBCODE);
	if( prc ) {
		buffer.formatstr( "%s = %s", ATTR_ON_EXIT_HOLD_SUBCODE, prc );
		InsertJobExpr( buffer );
		free(prc);
	}

	InsertJobExpr( buffer );
	RETURN_IF_ABORT();

	return 0;
}

#if 0 
int SubmitHash::SetExitHoldCheck()
{
	RETURN_IF_ABORT();

	char *ehc = submit_param(SUBMIT_KEY_OnExitHoldCheck, ATTR_ON_EXIT_HOLD_CHECK);
	MyString buffer;

	if (ehc == NULL)
	{
		/* user didn't have one, so add one */
		buffer.formatstr( "%s = FALSE", ATTR_ON_EXIT_HOLD_CHECK );
	}
	else
	{
		/* user had a value for it, leave it alone */
		buffer.formatstr( "%s = %s", ATTR_ON_EXIT_HOLD_CHECK, ehc );
		free(ehc);
	}

	InsertJobExpr( buffer );
	RETURN_IF_ABORT();

	return 0;
}
#endif

int SubmitHash::SetLeaveInQueue()
{
	RETURN_IF_ABORT();

	char *erc = submit_param(SUBMIT_KEY_LeaveInQueue, ATTR_JOB_LEAVE_IN_QUEUE);
	MyString buffer;

	if (erc == NULL)
	{
		/* user didn't have one, so add one */
		if ( ! IsRemoteJob) {
			buffer.formatstr( "%s = FALSE", ATTR_JOB_LEAVE_IN_QUEUE );
		} else {
				/* if remote spooling, leave in the queue after the job completes
				   for up to 10 days, so user can grab the output.
				 */
			buffer.formatstr( 
				"%s = %s == %d && (%s =?= UNDEFINED || %s == 0 || ((time() - %s) < %d))",
				ATTR_JOB_LEAVE_IN_QUEUE,
				ATTR_JOB_STATUS,
				COMPLETED,
				ATTR_COMPLETION_DATE,
				ATTR_COMPLETION_DATE,
				ATTR_COMPLETION_DATE,
				60 * 60 * 24 * 10 
				);
		}
	}
	else
	{
		/* user had a value for it, leave it alone */
		buffer.formatstr( "%s = %s", ATTR_JOB_LEAVE_IN_QUEUE, erc );
		free(erc);
	}

	InsertJobExpr( buffer );
	RETURN_IF_ABORT();

	return 0;
}

#if 0
int SubmitHash::SetExitRemoveCheck()
{
	RETURN_IF_ABORT();

	char *erc = submit_param(SUBMIT_KEY_OnExitRemoveCheck, ATTR_ON_EXIT_REMOVE_CHECK);
	MyString buffer;

	if (erc == NULL)
	{
		/* user didn't have one, so add one */
		buffer.formatstr( "%s = TRUE", ATTR_ON_EXIT_REMOVE_CHECK );
	}
	else
	{
		/* user had a value for it, leave it alone */
		buffer.formatstr( "%s = %s", ATTR_ON_EXIT_REMOVE_CHECK, erc );
		free(erc);
	}

	InsertJobExpr( buffer );
	RETURN_IF_ABORT();

	return 0;
}
#endif

int SubmitHash::SetNoopJob()
{
	RETURN_IF_ABORT();
	MyString buffer;

	auto_free_ptr noop(submit_param(SUBMIT_KEY_Noop, ATTR_JOB_NOOP));
	if (noop) {
		/* user had a value for it, leave it alone */
		buffer.formatstr( "%s = %s", ATTR_JOB_NOOP, noop.ptr() );
		InsertJobExpr( buffer );
		RETURN_IF_ABORT();
	}

	noop.set(submit_param(SUBMIT_KEY_NoopExitSignal, ATTR_JOB_NOOP_EXIT_SIGNAL));
	if (noop) {
		buffer.formatstr( "%s = %s", ATTR_JOB_NOOP_EXIT_SIGNAL, noop.ptr() );
		InsertJobExpr( buffer );
		RETURN_IF_ABORT();
	}

	noop.set(submit_param(SUBMIT_KEY_NoopExitCode, ATTR_JOB_NOOP_EXIT_CODE));
	if (noop) {
		buffer.formatstr( "%s = %s", ATTR_JOB_NOOP_EXIT_CODE, noop.ptr() );
		InsertJobExpr( buffer );
		RETURN_IF_ABORT();
	}

	return 0;
}


int SubmitHash::SetWantRemoteIO()
{
	RETURN_IF_ABORT();

	bool param_exists;
	bool remote_io = submit_param_bool( SUBMIT_KEY_WantRemoteIO, ATTR_WANT_REMOTE_IO, true, &param_exists );
	RETURN_IF_ABORT();

	AssignJobVal(ATTR_WANT_REMOTE_IO, remote_io);
	return 0;
}


#if !defined(WIN32)

int SubmitHash::ComputeRootDir(bool check_access /*=true*/)
{
	RETURN_IF_ABORT();

	char *rootdir = submit_param( SUBMIT_KEY_RootDir, ATTR_JOB_ROOT_DIR );

	if( rootdir == NULL ) 
	{
		JobRootdir = "/";
	} 
	else 
	{
		if (check_access) {
			if( access(rootdir, F_OK|X_OK) < 0 ) {
				push_error(stderr, "No such directory: %s\n",
						 rootdir );
				ABORT_AND_RETURN( 1 );
			}
		}

		MyString rootdir_str = rootdir;
		check_and_universalize_path(rootdir_str);
		JobRootdir = rootdir_str;
		free(rootdir);
	}

	return 0;
}

int SubmitHash::SetRootDir(bool check_access /*=true*/)
{
	RETURN_IF_ABORT();

	MyString buffer;
	ComputeRootDir(check_access);
	buffer.formatstr( "%s = \"%s\"", ATTR_JOB_ROOT_DIR, JobRootdir.Value());
	InsertJobExpr (buffer);
	return 0;
}
#endif

const char * SubmitHash::getIWD()
{
	ASSERT(JobIwdInitialized);
	return JobIwd.c_str();
}

int SubmitHash::ComputeIWD(bool check_access /*=true*/)
{
	char	*shortname;
	MyString	iwd;
	MyString	cwd;

	shortname = submit_param( SUBMIT_KEY_InitialDir, ATTR_JOB_IWD );
	if( ! shortname ) {
			// neither "initialdir" nor "iwd" were there, try some
			// others, just to be safe:
		shortname = submit_param( "initial_dir", "job_iwd" );
	}

	// for factories initalized with a cluster ad, we NEVER want to use the current working directory
	// as IWD, instead use FACTORY.Iwd
	if ( ! shortname && clusterAd) {
		shortname = submit_param("FACTORY.Iwd");
	}

#if !defined(WIN32)
	ComputeRootDir(check_access);
	if( JobRootdir != "/" )	{	/* Rootdir specified */
		if( shortname ) {
			iwd = shortname;
		} 
		else {
			iwd = "/";
		}
	} 
	else 
#endif
	{  //for WIN32, this is a block to make {}'s match. For unix, else block.
		if( shortname  ) {
#if defined(WIN32)
			// if a drive letter or share is specified, we have a full pathname
			if( shortname[1] == ':' || (shortname[0] == '\\' && shortname[1] == '\\')) 
#else
			if( shortname[0] == '/' ) 
#endif
			{
				iwd = shortname;
			} 
			else {
				condor_getcwd( cwd );
				iwd.formatstr( "%s%c%s", cwd.Value(), DIR_DELIM_CHAR, shortname );
			}
		} 
		else {
			condor_getcwd( iwd );
		}
	}

	compress_path( iwd );
	check_and_universalize_path(iwd);

	// when doing late materialization, we only want to do an access check
	// for the first Iwd, otherwise we can skip the check if the iwd has not changed.
	if ( ! JobIwdInitialized || ( ! clusterAd && iwd != JobIwd)) {

	#if defined(WIN32)
		if (access(iwd.Value(), F_OK|X_OK) < 0) {
			push_error(stderr, "No such directory: %s\n", iwd.Value());
			ABORT_AND_RETURN(1);
		}
	#else
		MyString pathname;
		pathname.formatstr( "%s/%s", JobRootdir.Value(), iwd.Value() );
		compress_path( pathname );

		if( access(pathname.Value(), F_OK|X_OK) < 0 ) {
			push_error(stderr, "No such directory: %s\n", pathname.Value() );
			ABORT_AND_RETURN(1);
		}
	#endif
	}
	JobIwd = iwd;
	JobIwdInitialized = true;
	if ( ! JobIwd.empty()) { mctx.cwd = JobIwd.Value(); }

	if ( shortname )
		free(shortname);

	return 0;
}

int SubmitHash::SetIWD(bool check_access /*=true*/)
{
	RETURN_IF_ABORT();
	if (ComputeIWD(check_access)) { ABORT_AND_RETURN(1); }
	MyString buffer;
	buffer.formatstr( "%s = \"%s\"", ATTR_JOB_IWD, JobIwd.Value());
	InsertJobExpr (buffer);
	RETURN_IF_ABORT();
	return 0;
}


int SubmitHash::SetGSICredentials()
{
	RETURN_IF_ABORT();

	MyString buffer;

		// Find the X509 user proxy
		// First param for it in the submit file. If it's not there
		// and the job type requires an x509 proxy (globus, nordugrid),
		// then check the usual locations (as defined by GSI) and
		// bomb out if we can't find it.

	char *proxy_file = submit_param( SUBMIT_KEY_X509UserProxy );
	bool use_proxy = submit_param_bool( SUBMIT_KEY_UseX509UserProxy, NULL, false );

	YourStringNoCase gridType(JobGridType.Value());
	if (JobUniverse == CONDOR_UNIVERSE_GRID &&
		(gridType == "gt2" ||
		 gridType == "gt5" ||
		 gridType == "cream" ||
		 gridType == "nordugrid" ) )
	{
		use_proxy = true;
	}

	if ( proxy_file == NULL && use_proxy ) {

		proxy_file = get_x509_proxy_filename();
		if ( proxy_file == NULL ) {

			push_error(stderr, "Can't determine proxy filename\nX509 user proxy is required for this job.\n" );
			ABORT_AND_RETURN( 1 );
		}
	}

	if (proxy_file != NULL) {
		char *full_proxy_file = strdup( full_path( proxy_file ) );
		free( proxy_file );
		proxy_file = full_proxy_file;
#if defined(HAVE_EXT_GLOBUS)
// this code should get torn out at some point (8.7.0) since the SchedD now
// manages these attributes securely and the values provided by submit should
// not be trusted.  in the meantime, though, we try to provide some cross
// compatibility between the old and new.  i also didn't indent this properly
// so as not to churn the old code.  -zmiller
// Exception: Checking the proxy lifetime and throwing an error if it's
// too short should remain. - jfrey

		// Starting in 8.5.8, schedd clients can't set X509-related attributes
		// other than the name of the proxy file.
		bool submit_sends_x509 = true;
		CondorVersionInfo cvi(getScheddVersion());
		if (cvi.built_since_version(8, 5, 8)) {
			submit_sends_x509 = false;
		}

		globus_gsi_cred_handle_t proxy_handle;
		proxy_handle = x509_proxy_read( proxy_file );
		if ( proxy_handle == NULL ) {
			push_error(stderr, "%s\n", x509_error_string() );
			ABORT_AND_RETURN( 1 );
		}

		/* Insert the proxy expiration time into the ad */
		time_t proxy_expiration;
		proxy_expiration = x509_proxy_expiration_time(proxy_handle);
		if (proxy_expiration == -1) {
			push_error(stderr, "%s\n", x509_error_string() );
			x509_proxy_free( proxy_handle );
			ABORT_AND_RETURN( 1 );
		} else if ( proxy_expiration < submit_time ) {
			push_error( stderr, "proxy has expired\n" );
			x509_proxy_free( proxy_handle );
			ABORT_AND_RETURN( 1 );
		} else if ( proxy_expiration < submit_time + param_integer( "CRED_MIN_TIME_LEFT" ) ) {
			push_error( stderr, "proxy lifetime too short\n" );
			x509_proxy_free( proxy_handle );
			ABORT_AND_RETURN( 1 );
		}

		if(submit_sends_x509) {

			(void) buffer.formatstr( "%s=%li", ATTR_X509_USER_PROXY_EXPIRATION, 
						   proxy_expiration);
			InsertJobExpr(buffer);	
	

			/* Insert the proxy subject name into the ad */
			char *proxy_subject;
			proxy_subject = x509_proxy_identity_name(proxy_handle);

			if ( !proxy_subject ) {
				push_error(stderr, "%s\n", x509_error_string() );
				x509_proxy_free( proxy_handle );
				ABORT_AND_RETURN( 1 );
			}

			(void) buffer.formatstr( "%s=\"%s\"", ATTR_X509_USER_PROXY_SUBJECT, 
						   proxy_subject);
			InsertJobExpr(buffer);	
			free( proxy_subject );

			/* Insert the proxy email into the ad */
			char *proxy_email;
			proxy_email = x509_proxy_email(proxy_handle);

			if ( proxy_email ) {
				InsertJobExprString(ATTR_X509_USER_PROXY_EMAIL, proxy_email);
				free( proxy_email );
			}

			/* Insert the VOMS attributes into the ad */
			char *voname = NULL;
			char *firstfqan = NULL;
			char *quoted_DN_and_FQAN = NULL;

			int error = extract_VOMS_info( proxy_handle, 0, &voname, &firstfqan, &quoted_DN_and_FQAN);
			if ( error ) {
				if (error == 1) {
					// no attributes, skip silently.
				} else {
					// log all other errors
					push_warning(stderr, "unable to extract VOMS attributes (proxy: %s, erro: %i). continuing \n", proxy_file, error );
				}
			} else {
				InsertJobExprString(ATTR_X509_USER_PROXY_VONAME, voname);	
				free( voname );

				InsertJobExprString(ATTR_X509_USER_PROXY_FIRST_FQAN, firstfqan);	
				free( firstfqan );

				InsertJobExprString(ATTR_X509_USER_PROXY_FQAN, quoted_DN_and_FQAN);	
				free( quoted_DN_and_FQAN );
			}

			// When new classads arrive, all this should be replaced with a
			// classad holding the VOMS atributes.  -zmiller
		}

		x509_proxy_free( proxy_handle );
// this is the end of the big, not-properly indented block (see above) that
// causes submit to send the x509 attributes only when talking to older
// schedds.  at some point, probably 8.7.0, this entire block should be ripped
// out. -zmiller
#endif

		(void) buffer.formatstr( "%s=\"%s\"", ATTR_X509_USER_PROXY,
					   proxy_file);
		InsertJobExpr(buffer);
		free( proxy_file );
	}

	char* tmp = submit_param(SUBMIT_KEY_DelegateJobGSICredentialsLifetime,ATTR_DELEGATE_JOB_GSI_CREDENTIALS_LIFETIME);
	if( tmp ) {
		char *endptr=NULL;
		int lifetime = strtol(tmp,&endptr,10);
		if( !endptr || *endptr != '\0' ) {
			push_error(stderr, "invalid integer setting %s = %s\n",SUBMIT_KEY_DelegateJobGSICredentialsLifetime,tmp);
			ABORT_AND_RETURN( 1 );
		}
		InsertJobExprInt(ATTR_DELEGATE_JOB_GSI_CREDENTIALS_LIFETIME,lifetime);
		free(tmp);
	}

	//ckireyev: MyProxy-related crap
	if ((tmp = submit_param (ATTR_MYPROXY_HOST_NAME))) {
		buffer.formatstr(  "%s = \"%s\"", ATTR_MYPROXY_HOST_NAME, tmp );
		free( tmp );
		InsertJobExpr ( buffer );
	}

	if ((tmp = submit_param (ATTR_MYPROXY_SERVER_DN))) {
		buffer.formatstr(  "%s = \"%s\"", ATTR_MYPROXY_SERVER_DN, tmp );
		free( tmp );
		InsertJobExpr ( buffer );
	}

	if ((tmp = submit_param (ATTR_MYPROXY_CRED_NAME))) {
		buffer.formatstr(  "%s = \"%s\"", ATTR_MYPROXY_CRED_NAME, tmp );
		free( tmp );
		InsertJobExpr ( buffer );
	}

	// if the password wasn't passed in when we initialized, check and see if it's in the submit file.
	if (MyProxyPassword.empty()) {
		tmp = submit_param (ATTR_MYPROXY_PASSWORD);
		MyProxyPassword = tmp; // tmp may be null here, that's ok because it's a mystring
		if (tmp) free(tmp);
	}

	if ( ! MyProxyPassword.empty()) {
		// note that the right hand side is NOT QUOTED. this seems wrong but the schedd
		// depends on this behavior, so we must preserve it.
		buffer.formatstr(  "%s = %s", ATTR_MYPROXY_PASSWORD, MyProxyPassword.Value());
		InsertJobExpr (buffer);
	}

	if ((tmp = submit_param (ATTR_MYPROXY_REFRESH_THRESHOLD))) {
		buffer.formatstr(  "%s = %s", ATTR_MYPROXY_REFRESH_THRESHOLD, tmp );
		free( tmp );
		InsertJobExpr ( buffer );
	}

	if ((tmp = submit_param (ATTR_MYPROXY_NEW_PROXY_LIFETIME))) { 
		buffer.formatstr(  "%s = %s", ATTR_MYPROXY_NEW_PROXY_LIFETIME, tmp );
		free( tmp );
		InsertJobExpr ( buffer );
	}

	// END MyProxy-related crap
	return 0;
}


#if !defined(WIN32)
// this allocates memory, free() it when you're done.
char* SubmitHash::findKillSigName( const char* submit_name, const char* attr_name )
{
	char *sig = submit_param( submit_name, attr_name );
	char *signame = NULL;
	const char *tmp;
	int signo;

	if (sig) {
		signo = atoi(sig);
		if( signo ) {
				// looks like they gave us an actual number, map that
				// into a string for the classad:
			tmp = signalName( signo );
			if( ! tmp ) {
				push_error(stderr, "invalid signal %s\n", sig );
				free(sig);
				abort_code=1;
				return NULL;
			}
			free(sig);
			signame = strdup( tmp );
		} else {
				// should just be a string, let's see if it's valid:
			signo = signalNumber( sig );
			if( signo == -1 ) {
				push_error(stderr, "invalid signal %s\n", sig );
				abort_code=1;
				free(sig);
				return NULL;
			}
				// cool, just use what they gave us.
			signame = strupr(sig);
		}
	}
	return signame;
}


int SubmitHash::SetKillSig()
{
	RETURN_IF_ABORT();

	char* sig_name;
	char* timeout;
	MyString buffer;

	sig_name = findKillSigName( SUBMIT_KEY_KillSig, ATTR_KILL_SIG );
	RETURN_IF_ABORT();
	if( ! sig_name ) {
		switch(JobUniverse) {
		case CONDOR_UNIVERSE_STANDARD:
			sig_name = strdup( "SIGTSTP" );
			break;
		case CONDOR_UNIVERSE_VANILLA:
			// Don't define sig_name for Vanilla Universe
			sig_name = NULL;
			break;
		default:
			sig_name = strdup( "SIGTERM" );
			break;
		}
	}

	if ( sig_name ) {
		buffer.formatstr( "%s=\"%s\"", ATTR_KILL_SIG, sig_name );
		InsertJobExpr( buffer );
		free( sig_name );
	}

	sig_name = findKillSigName( SUBMIT_KEY_RmKillSig, ATTR_REMOVE_KILL_SIG );
	RETURN_IF_ABORT();
	if( sig_name ) {
		buffer.formatstr( "%s=\"%s\"", ATTR_REMOVE_KILL_SIG, sig_name );
		InsertJobExpr( buffer );
		free( sig_name );
		sig_name = NULL;
	}

	sig_name = findKillSigName( SUBMIT_KEY_HoldKillSig, ATTR_HOLD_KILL_SIG );
	RETURN_IF_ABORT();
	if( sig_name ) {
		buffer.formatstr( "%s=\"%s\"", ATTR_HOLD_KILL_SIG, sig_name );
		InsertJobExpr( buffer );
		free( sig_name );
		sig_name = NULL;
	}

	timeout = submit_param( SUBMIT_KEY_KillSigTimeout, ATTR_KILL_SIG_TIMEOUT );
	if( timeout ) {
		buffer.formatstr( "%s=%d", ATTR_KILL_SIG_TIMEOUT, atoi(timeout) );
		InsertJobExpr( buffer );
		free( timeout );
		sig_name = NULL;
	}
	return 0;
}
#endif  // of ifndef WIN32


int SubmitHash::SetForcedAttributes()
{
	RETURN_IF_ABORT();
	MyString buffer;

	for (classad::References::const_iterator cit = forcedSubmitAttrs.begin(); cit != forcedSubmitAttrs.end(); ++cit) {
		char * value = param(cit->c_str());
		if ( ! value)
			continue;
		buffer.formatstr( "%s = %s", cit->c_str(), value);
		InsertJobExpr(buffer.c_str(), "SUBMIT_ATTRS or SUBMIT_EXPRS value");
		free(value);
	}

	HASHITER it = hash_iter_begin(SubmitMacroSet);
	for( ; ! hash_iter_done(it); hash_iter_next(it)) {
		const char *my_name = hash_iter_key(it);
		if ( ! starts_with_ignore_case(my_name, "MY.")) continue;
		const char * name = my_name+sizeof("MY.")-1;

		char * value = submit_param(my_name); // lookup and expand macros.
	#ifdef PLUS_ATTRIBS_IN_CLUSTER_AD
		buffer.formatstr( "%s = %s", name, (value && value[0]) ? value : "undefined" );
		InsertJobExpr(buffer);
		RETURN_IF_ABORT();
	#else
		if (value && value[0]) {
			buffer.formatstr( "%s = %s", name, value);

			// Call InserJobExpr with checkcluster set to false.
			// This will result in forced attributes always going
			// into the proc ad, not the cluster ad.  This allows
			// us to easily remove attributes with the "-" command.
			InsertJobExpr(buffer);
			SetNoClusterAttr(name);
		} else {
			job->Delete( name );
		}
	#endif
		if (value) free(value);
	}
	hash_iter_delete(&it);
	return 0;
}

int SubmitHash::SetGridParams()
{
	RETURN_IF_ABORT();
	char *tmp;
	MyString buffer;
	FILE* fp;

	if ( JobUniverse != CONDOR_UNIVERSE_GRID )
		return 0;

	tmp = submit_param( SUBMIT_KEY_GridResource, ATTR_GRID_RESOURCE );
	if ( tmp ) {
			// TODO validate number of fields in grid_resource?

		buffer.formatstr( "%s = \"%s\"", ATTR_GRID_RESOURCE, tmp );
		InsertJobExpr( buffer );

		if ( strstr(tmp,"$$") ) {
				// We need to perform matchmaking on the job in order
				// to fill GridResource.
			buffer.formatstr("%s = FALSE", ATTR_JOB_MATCHED);
			InsertJobExpr (buffer);
			buffer.formatstr("%s = 0", ATTR_CURRENT_HOSTS);
			InsertJobExpr (buffer);
			buffer.formatstr("%s = 1", ATTR_MAX_HOSTS);
			InsertJobExpr (buffer);
		}

		if ( strcasecmp( tmp, "ec2" ) == 0 ) {
			push_error(stderr, "EC2 grid jobs require a "
					"service URL\n");
			ABORT_AND_RETURN( 1 );
		}
		

		free( tmp );

	} else {
			// TODO Make this allowable, triggering matchmaking for
			//   GridResource
		push_error(stderr, "No resource identifier was found.\n" );
		ABORT_AND_RETURN( 1 );
	}

	YourStringNoCase gridType(JobGridType.Value());
	if ( gridType == NULL ||
		 gridType == "gt2" ||
		 gridType == "gt5" ||
		 gridType == "nordugrid" ) {

		if( (tmp = submit_param(SUBMIT_KEY_GlobusResubmit,ATTR_GLOBUS_RESUBMIT_CHECK)) ) {
			buffer.formatstr( "%s = %s", ATTR_GLOBUS_RESUBMIT_CHECK, tmp );
			free(tmp);
			InsertJobExpr (buffer);
		} else {
			buffer.formatstr( "%s = FALSE", ATTR_GLOBUS_RESUBMIT_CHECK);
			InsertJobExpr (buffer);
		}
	}

	if ( (tmp = submit_param(SUBMIT_KEY_GridShell, ATTR_USE_GRID_SHELL)) ) {

		if( tmp[0] == 't' || tmp[0] == 'T' ) {
			buffer.formatstr( "%s = TRUE", ATTR_USE_GRID_SHELL );
			InsertJobExpr( buffer );
		}
		free(tmp);
	}

	if ( gridType == NULL ||
		 gridType == "gt2" ||
		 gridType == "gt5" ) {

		buffer.formatstr( "%s = %d", ATTR_GLOBUS_STATUS,
				 GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNSUBMITTED );
		InsertJobExpr (buffer);

		buffer.formatstr( "%s = 0", ATTR_NUM_GLOBUS_SUBMITS );
		InsertJobExpr (buffer);
	}

	buffer.formatstr( "%s = False", ATTR_WANT_CLAIMING );
	InsertJobExpr(buffer);

	if( (tmp = submit_param(SUBMIT_KEY_GlobusRematch,ATTR_REMATCH_CHECK)) ) {
		buffer.formatstr( "%s = %s", ATTR_REMATCH_CHECK, tmp );
		free(tmp);
		InsertJobExpr (buffer);
	}

	if( (tmp = submit_param(SUBMIT_KEY_GlobusRSL, ATTR_GLOBUS_RSL)) ) {
		buffer.formatstr( "%s = \"%s\"", ATTR_GLOBUS_RSL, tmp );
		free( tmp );
		InsertJobExpr ( buffer );
	}

	if( (tmp = submit_param(SUBMIT_KEY_NordugridRSL, ATTR_NORDUGRID_RSL)) ) {
		buffer.formatstr( "%s = \"%s\"", ATTR_NORDUGRID_RSL, tmp );
		free( tmp );
		InsertJobExpr ( buffer );
	}

	if( (tmp = submit_param(SUBMIT_KEY_CreamAttributes, ATTR_CREAM_ATTRIBUTES)) ) {
		InsertJobExprString ( ATTR_CREAM_ATTRIBUTES, tmp );
		free( tmp );
	}

	if( (tmp = submit_param(SUBMIT_KEY_BatchQueue, ATTR_BATCH_QUEUE)) ) {
		InsertJobExprString ( ATTR_BATCH_QUEUE, tmp );
		free( tmp );
	}

	if ( (tmp = submit_param( SUBMIT_KEY_KeystoreFile, ATTR_KEYSTORE_FILE )) ) {
		buffer.formatstr( "%s = \"%s\"", ATTR_KEYSTORE_FILE, tmp );
		InsertJobExpr( buffer );
		free( tmp );
	} else if ( gridType == "unicore" ) {
		push_error(stderr, "Unicore grid jobs require a \"%s\" "
				"parameter\n", SUBMIT_KEY_KeystoreFile );
		ABORT_AND_RETURN( 1 );
	}

	if ( (tmp = submit_param( SUBMIT_KEY_KeystoreAlias, ATTR_KEYSTORE_ALIAS )) ) {
		buffer.formatstr( "%s = \"%s\"", ATTR_KEYSTORE_ALIAS, tmp );
		InsertJobExpr( buffer );
		free( tmp );
	} else if ( gridType == "unicore" ) {
		push_error(stderr, "Unicore grid jobs require a \"%s\" "
				"parameter\n",SUBMIT_KEY_KeystoreAlias );
		ABORT_AND_RETURN( 1 );
	}

	if ( (tmp = submit_param( SUBMIT_KEY_KeystorePassphraseFile,
							  ATTR_KEYSTORE_PASSPHRASE_FILE )) ) {
		buffer.formatstr( "%s = \"%s\"", ATTR_KEYSTORE_PASSPHRASE_FILE, tmp );
		InsertJobExpr( buffer );
		free( tmp );
	} else if ( gridType == "unicore" ) {
		push_error(stderr, "Unicore grid jobs require a \"%s\" "
				"parameter\n", SUBMIT_KEY_KeystorePassphraseFile );
		ABORT_AND_RETURN( 1 );
	}
	
	//
	// EC2 grid-type submit attributes
	//
	if ( (tmp = submit_param( SUBMIT_KEY_EC2AccessKeyId, ATTR_EC2_ACCESS_KEY_ID )) ) {
		// check public key file can be opened
		if ( !DisableFileChecks ) {
			if( ( fp=safe_fopen_wrapper_follow(full_path(tmp),"r") ) == NULL ) {
				push_error(stderr, "Failed to open public key file %s (%s)\n", 
							 full_path(tmp), strerror(errno));
				ABORT_AND_RETURN( 1 );
			}
			fclose(fp);

			StatInfo si(full_path(tmp));
			if (si.IsDirectory()) {
				push_error(stderr, "%s is a directory\n", full_path(tmp));
				ABORT_AND_RETURN( 1 );
			}
		}
		buffer.formatstr( "%s = \"%s\"", ATTR_EC2_ACCESS_KEY_ID, full_path(tmp) );
		InsertJobExpr( buffer.Value() );
		free( tmp );
	} else if ( gridType == "ec2" ) {
		push_error(stderr, "EC2 jobs require a \"%s\" parameter\n", SUBMIT_KEY_EC2AccessKeyId );
		ABORT_AND_RETURN( 1 );
	}
	
	if ( (tmp = submit_param( SUBMIT_KEY_EC2SecretAccessKey, ATTR_EC2_SECRET_ACCESS_KEY )) ) {
		// check private key file can be opened
		if ( !DisableFileChecks ) {
			if( ( fp=safe_fopen_wrapper_follow(full_path(tmp),"r") ) == NULL ) {
				push_error(stderr, "Failed to open private key file %s (%s)\n", 
							 full_path(tmp), strerror(errno));
				ABORT_AND_RETURN( 1 );
			}
			fclose(fp);

			StatInfo si(full_path(tmp));
			if (si.IsDirectory()) {
				push_error(stderr, "%s is a directory\n", full_path(tmp));
				ABORT_AND_RETURN( 1 );
			}
		}
		buffer.formatstr( "%s = \"%s\"", ATTR_EC2_SECRET_ACCESS_KEY, full_path(tmp) );
		InsertJobExpr( buffer.Value() );
		free( tmp );
	} else if ( gridType == "ec2" ) {
		push_error(stderr, "EC2 jobs require a \"%s\" parameter\n", SUBMIT_KEY_EC2SecretAccessKey );
		ABORT_AND_RETURN( 1 );
	}
	
	bool bKeyPairPresent=false;
	
	// EC2KeyPair is not a necessary parameter
	if( (tmp = submit_param( SUBMIT_KEY_EC2KeyPair, ATTR_EC2_KEY_PAIR )) ||
		(tmp = submit_param( SUBMIT_KEY_EC2KeyPairAlt, ATTR_EC2_KEY_PAIR ))) {
		buffer.formatstr( "%s = \"%s\"", ATTR_EC2_KEY_PAIR, tmp );
		free( tmp );
		InsertJobExpr( buffer.Value() );
		bKeyPairPresent=true;
	}
	
	// EC2KeyPairFile is not a necessary parameter
	if( (tmp = submit_param( SUBMIT_KEY_EC2KeyPairFile, ATTR_EC2_KEY_PAIR_FILE )) ||
		(tmp = submit_param( SUBMIT_KEY_EC2KeyPairFileAlt, ATTR_EC2_KEY_PAIR_FILE ))) {
	    if (bKeyPairPresent)
	    {
	      push_warning(stderr, "EC2 job(s) contain both ec2_keypair && ec2_keypair_file, ignoring ec2_keypair_file\n");
	    }
	    else
	    {
	      // for the relative path, the keypair output file will be written to the IWD
	      buffer.formatstr( "%s = \"%s\"", ATTR_EC2_KEY_PAIR_FILE, full_path(tmp) );
	      free( tmp );
	      InsertJobExpr( buffer.Value() );
	    }
	}

	// Optional.
	if( (tmp = submit_param( SUBMIT_KEY_EC2SecurityGroups, ATTR_EC2_SECURITY_GROUPS )) ) {
		buffer.formatstr( "%s = \"%s\"", ATTR_EC2_SECURITY_GROUPS, tmp );
		free( tmp );
		InsertJobExpr( buffer.Value() );
	}

	// Optional.
	if( (tmp = submit_param( SUBMIT_KEY_EC2SecurityIDs, ATTR_EC2_SECURITY_IDS )) ) {
		buffer.formatstr( "%s = \"%s\"", ATTR_EC2_SECURITY_IDS, tmp );
		free( tmp );
		InsertJobExpr( buffer.Value() );
	}

	if ( (tmp = submit_param( SUBMIT_KEY_EC2AmiID, ATTR_EC2_AMI_ID )) ) {
		buffer.formatstr( "%s = \"%s\"", ATTR_EC2_AMI_ID, tmp );
		InsertJobExpr( buffer.Value() );
		free( tmp );
	} else if ( gridType == "ec2"  ) {
		push_error(stderr, "EC2 jobs require a \"%s\" parameter\n", SUBMIT_KEY_EC2AmiID );
		ABORT_AND_RETURN( 1 );
	}
	
	// EC2InstanceType is not a necessary parameter
	if( (tmp = submit_param( SUBMIT_KEY_EC2InstanceType, ATTR_EC2_INSTANCE_TYPE )) ) {
		buffer.formatstr( "%s = \"%s\"", ATTR_EC2_INSTANCE_TYPE, tmp );
		free( tmp );
		InsertJobExpr( buffer.Value() );
	}
	
	// EC2VpcSubnet is not a necessary parameter
	if( (tmp = submit_param( SUBMIT_KEY_EC2VpcSubnet, ATTR_EC2_VPC_SUBNET )) ) {
		buffer.formatstr( "%s = \"%s\"",ATTR_EC2_VPC_SUBNET , tmp );
		free( tmp );
		InsertJobExpr( buffer.Value() );
	}
	
	// EC2VpcIP is not a necessary parameter
	if( (tmp = submit_param( SUBMIT_KEY_EC2VpcIP, ATTR_EC2_VPC_IP )) ) {
		buffer.formatstr( "%s = \"%s\"",ATTR_EC2_VPC_IP , tmp );
		free( tmp );
		InsertJobExpr( buffer.Value() );
	}
		
	// EC2ElasticIP is not a necessary parameter
    if( (tmp = submit_param( SUBMIT_KEY_EC2ElasticIP, ATTR_EC2_ELASTIC_IP )) ) {
        buffer.formatstr( "%s = \"%s\"", ATTR_EC2_ELASTIC_IP, tmp );
        free( tmp );
        InsertJobExpr( buffer.Value() );
    }
	
	bool HasAvailabilityZone=false;
	// EC2AvailabilityZone is not a necessary parameter
    if( (tmp = submit_param( SUBMIT_KEY_EC2AvailabilityZone, ATTR_EC2_AVAILABILITY_ZONE )) ) {
        buffer.formatstr( "%s = \"%s\"", ATTR_EC2_AVAILABILITY_ZONE, tmp );
        free( tmp );
        InsertJobExpr( buffer.Value() );
		HasAvailabilityZone=true;
    }
	
	// EC2EBSVolumes is not a necessary parameter
    if( (tmp = submit_param( SUBMIT_KEY_EC2EBSVolumes, ATTR_EC2_EBS_VOLUMES )) ) {
		if( validate_disk_param(tmp, 2, 2) == false ) 
        {
			push_error(stderr, "'ec2_ebs_volumes' has incorrect format.\n"
					"The format shoud be like "
					"\"<instance_id>:<devicename>\"\n"
					"e.g.> For single volume: ec2_ebs_volumes = vol-35bcc15e:hda1\n"
					"      For multiple disks: ec2_ebs_volumes = "
					"vol-35bcc15e:hda1,vol-35bcc16f:hda2\n");
			ABORT_AND_RETURN(1);
		}
		
		if (!HasAvailabilityZone)
		{
			push_error(stderr, "'ec2_ebs_volumes' requires 'ec2_availability_zone'\n");
			ABORT_AND_RETURN(1);
		}
		
        buffer.formatstr( "%s = \"%s\"", ATTR_EC2_EBS_VOLUMES, tmp );
        free( tmp );
        InsertJobExpr( buffer.Value() );
    }

	// EC2SpotPrice is not a necessary parameter
	if( (tmp = submit_param( SUBMIT_KEY_EC2SpotPrice, ATTR_EC2_SPOT_PRICE )) ) {
		buffer.formatstr( "%s = \"%s\"", ATTR_EC2_SPOT_PRICE, tmp);
		free( tmp );
		InsertJobExpr( buffer.Value() );
	}

	// EC2BlockDeviceMapping is not a necessary parameter
	if( (tmp = submit_param( SUBMIT_KEY_EC2BlockDeviceMapping, ATTR_EC2_BLOCK_DEVICE_MAPPING )) ) {
		buffer.formatstr( "%s = \"%s\"", ATTR_EC2_BLOCK_DEVICE_MAPPING, tmp );
		free( tmp );
		InsertJobExpr( buffer.Value() );
		bKeyPairPresent=true;
	}

	// EC2UserData is not a necessary parameter
	if( (tmp = submit_param( SUBMIT_KEY_EC2UserData, ATTR_EC2_USER_DATA )) ) {
		buffer.formatstr( "%s = \"%s\"", ATTR_EC2_USER_DATA, tmp);
		free( tmp );
		InsertJobExpr( buffer.Value() );
	}

	// EC2UserDataFile is not a necessary parameter
	if( (tmp = submit_param( SUBMIT_KEY_EC2UserDataFile, ATTR_EC2_USER_DATA_FILE )) ) {
		// check user data file can be opened
		if ( !DisableFileChecks ) {
			if( ( fp=safe_fopen_wrapper_follow(full_path(tmp),"r") ) == NULL ) {
				push_error(stderr, "Failed to open user data file %s (%s)\n", 
								 full_path(tmp), strerror(errno));
				ABORT_AND_RETURN( 1 );
			}
			fclose(fp);
		}
		buffer.formatstr( "%s = \"%s\"", ATTR_EC2_USER_DATA_FILE, 
				full_path(tmp) );
		free( tmp );
		InsertJobExpr( buffer.Value() );
	}

	// You can only have one IAM [Instance] Profile, so you can only use
	// one of the ARN or the Name.
	bool bIamProfilePresent = false;
	if( (tmp = submit_param( SUBMIT_KEY_EC2IamProfileArn, ATTR_EC2_IAM_PROFILE_ARN )) ) {
		bIamProfilePresent = true;
		buffer.formatstr( "%s = \"%s\"", ATTR_EC2_IAM_PROFILE_ARN, tmp );
		InsertJobExpr( buffer.Value() );
		free( tmp );
	}

	if( (tmp = submit_param( SUBMIT_KEY_EC2IamProfileName, ATTR_EC2_IAM_PROFILE_NAME )) ) {
		if( bIamProfilePresent ) {
			push_warning( stderr, "EC2 job(s) contain both %s and %s; ignoring %s.\n",
				SUBMIT_KEY_EC2IamProfileArn, SUBMIT_KEY_EC2IamProfileName, SUBMIT_KEY_EC2IamProfileName );
		} else {
			buffer.formatstr( "%s = \"%s\"", ATTR_EC2_IAM_PROFILE_NAME, tmp );
			InsertJobExpr( buffer.Value() );
		}
		free( tmp );
	}

	//
	// Handle arbitrary EC2 RunInstances parameters.
	//
	StringList paramNames;
	if( (tmp = submit_param( SUBMIT_KEY_EC2ParamNames, ATTR_EC2_PARAM_NAMES )) ) {
		paramNames.initializeFromString( tmp );
		free( tmp );
	}

	unsigned int prefixLength = (unsigned int)strlen( SUBMIT_KEY_EC2ParamPrefix );
	HASHITER smsIter = hash_iter_begin( SubmitMacroSet );
	for( ; ! hash_iter_done( smsIter ); hash_iter_next( smsIter ) ) {
		const char * key = hash_iter_key( smsIter );

		if( strcasecmp( key, SUBMIT_KEY_EC2ParamNames ) == 0 ) {
			continue;
		}

		if( strncasecmp( key, SUBMIT_KEY_EC2ParamPrefix, prefixLength ) != 0 ) {
			continue;
		}

		const char * paramName = &key[prefixLength];
		const char * paramValue = hash_iter_value( smsIter );
		buffer.formatstr( "%s_%s = \"%s\"", ATTR_EC2_PARAM_PREFIX, paramName, paramValue );
		InsertJobExpr( buffer.Value() );
		set_submit_param_used( key );

		bool found = false;
		paramNames.rewind();
		char * existingPN = NULL;
		while( (existingPN = paramNames.next()) != NULL ) {
			std::string converted = existingPN;
			std::replace( converted.begin(), converted.end(), '.', '_' );
			if( strcasecmp( converted.c_str(), paramName ) == 0 ) {
				found = true;
				break;
			}
		}
		if( ! found ) {
			paramNames.append( paramName );
		}
	}
	hash_iter_delete( & smsIter );

	if( ! paramNames.isEmpty() ) {
		char * paramNamesStr = paramNames.print_to_delimed_string( ", " );
		buffer.formatstr( "%s = \"%s\"", ATTR_EC2_PARAM_NAMES, paramNamesStr );
		free( paramNamesStr );
		InsertJobExpr( buffer.Value() );
	}


		//
		// Handle EC2 tags - don't require user to specify the list of tag names
		//
		// Collect all the EC2 tag names, then param for each
		//
		// EC2TagNames is needed because EC2 tags are case-sensitive
		// and ClassAd attribute names are not. We build it for the
		// user, but also let the user override entries in it with
		// their own case preference. Ours will always be lower-cased.
		//

	StringList tagNames;
	if ((tmp = submit_param(SUBMIT_KEY_EC2TagNames, ATTR_EC2_TAG_NAMES))) {
		tagNames.initializeFromString(tmp);
		free(tmp); tmp = NULL;
	}

	HASHITER it = hash_iter_begin(SubmitMacroSet);
	int prefix_len = (int)strlen(ATTR_EC2_TAG_PREFIX);
	for (;!hash_iter_done(it); hash_iter_next(it)) {
		const char *key = hash_iter_key(it);
		const char *name = NULL;
		if (!strncasecmp(key, ATTR_EC2_TAG_PREFIX, prefix_len) &&
			key[prefix_len]) {
			name = &key[prefix_len];
		} else if (!strncasecmp(key, "ec2_tag_", 8) &&
				   key[8]) {
			name = &key[8];
		} else {
			continue;
		}

		if (strncasecmp(name, "Names", 5) &&
			!tagNames.contains_anycase(name)) {
			tagNames.append(name);
		}
	}
	hash_iter_delete(&it);

	std::stringstream ss;
	char *tagName;
	tagNames.rewind();
	while ((tagName = tagNames.next())) {
			// XXX: Check that tagName does not contain an equal sign (=)
		std::string tag;
		std::string tagAttr(ATTR_EC2_TAG_PREFIX); tagAttr.append(tagName);
		std::string tagCmd("ec2_tag_"); tagCmd.append(tagName);
		char *value = NULL;
		if ((value = submit_param(tagCmd.c_str(), tagAttr.c_str()))) {
			buffer.formatstr("%s = \"%s\"", tagAttr.c_str(), value);
			InsertJobExpr(buffer.Value());
			free(value); value = NULL;
		} else {
				// XXX: Should never happen, we just searched for the names, error or something
		}
	}

		// For compatibility with the AWS Console, set the Name tag to
		// be the executable, which is just a label for EC2 jobs
	tagNames.rewind();
	if (!tagNames.contains_anycase("Name")) {
		if (JobUniverse == CONDOR_UNIVERSE_GRID && gridType == "ec2") {
			bool wantsNameTag = submit_param_bool( "WantNameTag", NULL, true );
			if( wantsNameTag ) {
				char *ename = submit_param(SUBMIT_KEY_Executable, ATTR_JOB_CMD); // !NULL by now
				tagNames.append("Name");
				buffer.formatstr("%sName = \"%s\"", ATTR_EC2_TAG_PREFIX, ename);
				InsertJobExpr(buffer);
				free(ename); ename = NULL;
			}
		}
	}

	if ( !tagNames.isEmpty() ) {
		buffer.formatstr("%s = \"%s\"",
					ATTR_EC2_TAG_NAMES, tagNames.print_to_delimed_string(","));
		InsertJobExpr(buffer.Value());
	}

	if ( (tmp = submit_param( SUBMIT_KEY_BoincAuthenticatorFile,
							  ATTR_BOINC_AUTHENTICATOR_FILE )) ) {
		// check authenticator file can be opened
		if ( !DisableFileChecks ) {
			if( ( fp=safe_fopen_wrapper_follow(full_path(tmp),"r") ) == NULL ) {
				push_error(stderr, "Failed to open authenticator file %s (%s)\n", 
								 full_path(tmp), strerror(errno));
				ABORT_AND_RETURN( 1 );
			}
			fclose(fp);
		}
		buffer.formatstr( "%s = \"%s\"", ATTR_BOINC_AUTHENTICATOR_FILE,
						  full_path(tmp) );
		InsertJobExpr( buffer.Value() );
		free( tmp );
	} else if ( gridType == "boinc" ) {
		push_error(stderr, "BOINC jobs require a \"%s\" parameter\n", SUBMIT_KEY_BoincAuthenticatorFile );
		ABORT_AND_RETURN( 1 );
	}

	//
	// GCE grid-type submit attributes
	//
	if ( (tmp = submit_param( SUBMIT_KEY_GceAuthFile, ATTR_GCE_AUTH_FILE )) ) {
		// check auth file can be opened
		if ( !DisableFileChecks ) {
			if( ( fp=safe_fopen_wrapper_follow(full_path(tmp),"r") ) == NULL ) {
				push_error(stderr, "Failed to open auth file %s (%s)\n", 
						 full_path(tmp), strerror(errno));
				ABORT_AND_RETURN( 1 );
			}
			fclose(fp);

			StatInfo si(full_path(tmp));
			if (si.IsDirectory()) {
				push_error(stderr, "%s is a directory\n", full_path(tmp));
				ABORT_AND_RETURN( 1 );
			}
		}
		buffer.formatstr( "%s = \"%s\"", ATTR_GCE_AUTH_FILE, full_path(tmp) );
		InsertJobExpr( buffer.Value() );
		free( tmp );
	} else if ( gridType == "gce" ) {
		push_error(stderr, "GCE jobs require a \"%s\" parameter\n", SUBMIT_KEY_GceAuthFile );
		ABORT_AND_RETURN( 1 );
	}

	if ( (tmp = submit_param( SUBMIT_KEY_GceImage, ATTR_GCE_IMAGE )) ) {
		buffer.formatstr( "%s = \"%s\"", ATTR_GCE_IMAGE, tmp );
		InsertJobExpr( buffer.Value() );
		free( tmp );
	} else if ( gridType == "gce" ) {
		push_error(stderr, "GCE jobs require a \"%s\" parameter\n", SUBMIT_KEY_GceImage );
		ABORT_AND_RETURN( 1 );
	}

	if ( (tmp = submit_param( SUBMIT_KEY_GceMachineType, ATTR_GCE_MACHINE_TYPE )) ) {
		buffer.formatstr( "%s = \"%s\"", ATTR_GCE_MACHINE_TYPE, tmp );
		InsertJobExpr( buffer.Value() );
		free( tmp );
	} else if ( gridType == "gce" ) {
		push_error(stderr, "GCE jobs require a \"%s\" parameter\n", SUBMIT_KEY_GceMachineType );
		ABORT_AND_RETURN( 1 );
	}

	// GceMetadata is not a necessary parameter
	// This is a comma-separated list of name/value pairs
	if( (tmp = submit_param( SUBMIT_KEY_GceMetadata, ATTR_GCE_METADATA )) ) {
		StringList list( tmp, "," );
		char *list_str = list.print_to_string();
		buffer.formatstr( "%s = \"%s\"", ATTR_GCE_METADATA, list_str );
		InsertJobExpr( buffer.Value() );
		free( list_str );
	}

	// GceMetadataFile is not a necessary parameter
	if( (tmp = submit_param( SUBMIT_KEY_GceMetadataFile, ATTR_GCE_METADATA_FILE )) ) {
		// check metadata file can be opened
		if ( !DisableFileChecks ) {
			if( ( fp=safe_fopen_wrapper_follow(full_path(tmp),"r") ) == NULL ) {
				push_error(stderr, "Failed to open metadata file %s (%s)\n", 
								 full_path(tmp), strerror(errno));
				ABORT_AND_RETURN( 1 );
			}
			fclose(fp);
		}
		buffer.formatstr( "%s = \"%s\"", ATTR_GCE_METADATA_FILE, 
				full_path(tmp) );
		free( tmp );
		InsertJobExpr( buffer.Value() );
	}

	// GcePreemptible is not a necessary parameter
	bool exists = false;
	bool bool_val = submit_param_bool( SUBMIT_KEY_GcePreemptible, ATTR_GCE_PREEMPTIBLE, false, &exists );
	if( exists ) {
		buffer.formatstr( "%s = %s", ATTR_GCE_PREEMPTIBLE, bool_val ? "True" : "False" );
		InsertJobExpr( buffer.Value() );
	}

	// GceJsonFile is not a necessary parameter
	if( (tmp = submit_param( SUBMIT_KEY_GceJsonFile, ATTR_GCE_JSON_FILE )) ) {
		// check json file can be opened
		if ( !DisableFileChecks ) {
			if( ( fp=safe_fopen_wrapper_follow(full_path(tmp),"r") ) == NULL ) {
				fprintf( stderr, "\nERROR: Failed to open json file %s (%s)\n",
								 full_path(tmp), strerror(errno));
				ABORT_AND_RETURN( 1 );
			}
			fclose(fp);
		}
		InsertJobExprString( ATTR_GCE_JSON_FILE, full_path( tmp ) );
		free( tmp );
	}

	//
	// Azure grid-type submit attributes
	//
	if ( (tmp = submit_param( SUBMIT_KEY_AzureAuthFile, ATTR_AZURE_AUTH_FILE )) ) {
		// check auth file can be opened
		if ( !DisableFileChecks ) {
			if( ( fp=safe_fopen_wrapper_follow(full_path(tmp),"r") ) == NULL ) {
				push_error(stderr, "\nERROR: Failed to open auth file %s (%s)\n", 
				           full_path(tmp), strerror(errno));
				ABORT_AND_RETURN(1);
			}
			fclose(fp);

			StatInfo si(full_path(tmp));
			if (si.IsDirectory()) {
				push_error(stderr, "\nERROR: %s is a directory\n", full_path(tmp));
				ABORT_AND_RETURN( 1 );
			}
		}
		buffer.formatstr( "%s = \"%s\"", ATTR_AZURE_AUTH_FILE, full_path(tmp) );
		InsertJobExpr( buffer.Value() );
		free( tmp );
	} else if ( gridType == "azure" ) {
		push_error(stderr, "\nERROR: Azure jobs require an \"%s\" parameter\n", SUBMIT_KEY_AzureAuthFile );
		ABORT_AND_RETURN( 1 );
	}

	if ( (tmp = submit_param( SUBMIT_KEY_AzureImage, ATTR_AZURE_IMAGE )) ) {
		buffer.formatstr( "%s = \"%s\"", ATTR_AZURE_IMAGE, tmp );
		InsertJobExpr( buffer.Value() );
		free( tmp );
	} else if ( gridType == "azure" ) {
		push_error(stderr, "\nERROR: Azure jobs require an \"%s\" parameter\n", SUBMIT_KEY_AzureImage );
		ABORT_AND_RETURN( 1 );
	}

	if ( (tmp = submit_param( SUBMIT_KEY_AzureLocation, ATTR_AZURE_LOCATION )) ) {
		buffer.formatstr( "%s = \"%s\"", ATTR_AZURE_LOCATION, tmp );
		InsertJobExpr( buffer.Value() );
		free( tmp );
	} else if ( gridType == "azure" ) {
		push_error(stderr, "\nERROR: Azure jobs require an \"%s\" parameter\n", SUBMIT_KEY_AzureLocation );
		ABORT_AND_RETURN( 1 );
	}

	if ( (tmp = submit_param( SUBMIT_KEY_AzureSize, ATTR_AZURE_SIZE )) ) {
		buffer.formatstr( "%s = \"%s\"", ATTR_AZURE_SIZE, tmp );
		InsertJobExpr( buffer.Value() );
		free( tmp );
	} else if ( gridType == "azure" ) {
		push_error(stderr, "\nERROR: Azure jobs require an \"%s\" parameter\n", SUBMIT_KEY_AzureSize );
		ABORT_AND_RETURN( 1 );
	}

	if ( (tmp = submit_param( SUBMIT_KEY_AzureAdminUsername, ATTR_AZURE_ADMIN_USERNAME )) ) {
		buffer.formatstr( "%s = \"%s\"", ATTR_AZURE_ADMIN_USERNAME, tmp );
		InsertJobExpr( buffer.Value() );
		free( tmp );
	} else if ( gridType == "azure" ) {
		push_error(stderr, "\nERROR: Azure jobs require an \"%s\" parameter\n", SUBMIT_KEY_AzureAdminUsername );
		ABORT_AND_RETURN( 1 );
	}

	if ( (tmp = submit_param( SUBMIT_KEY_AzureAdminKey, ATTR_AZURE_ADMIN_KEY )) ) {
		buffer.formatstr( "%s = \"%s\"", ATTR_AZURE_ADMIN_KEY, tmp );
		InsertJobExpr( buffer.Value() );
		free( tmp );
	} else if ( gridType == "azure" ) {
		push_error(stderr, "\nERROR: Azure jobs require an \"%s\" parameter\n", SUBMIT_KEY_AzureAdminKey );
		ABORT_AND_RETURN( 1 );
	}

	// CREAM clients support an alternate representation for resources:
	//   host.edu:8443/cream-batchname-queuename
	// Transform this representation into our regular form:
	//   host.edu:8443/ce-cream/services/CREAM2 batchname queuename
	if ( gridType == "cream" ) {
		tmp = submit_param( SUBMIT_KEY_GridResource, ATTR_GRID_RESOURCE );
		MyString resource = tmp;
		free( tmp );

		int pos = resource.FindChar( ' ', 0 );
		if ( pos >= 0 && resource.FindChar( ' ', pos + 1 ) < 0 ) {
			int pos2 = resource.find( "://", pos + 1 );
			if ( pos2 < 0 ) {
				pos2 = pos + 1;
			} else {
				pos2 = pos2 + 3;
			}
			if ( ( pos = resource.find( "/cream-", pos2 ) ) >= 0 ) {
				// We found the shortened form
				resource.replaceString( "/cream-", "/ce-cream/services/CREAM2 ", pos );
				pos += 26;
				if ( ( pos2 = resource.find( "-", pos ) ) >= 0 ) {
					resource.setAt( pos2, ' ' );
				}

				buffer.formatstr( "%s = \"%s\"", ATTR_GRID_RESOURCE,
								resource.Value() );
				InsertJobExpr( buffer );
			}
		}
	}
	return 0;
}


int SubmitHash::SetNotification()
{
	RETURN_IF_ABORT();
	char *how = submit_param( SUBMIT_KEY_Notification, ATTR_JOB_NOTIFICATION );
	int notification;
	MyString buffer;
	
	if( how == NULL ) {
		how = param ( "JOB_DEFAULT_NOTIFICATION" );		
	}
	if( (how == NULL) || (strcasecmp(how, "NEVER") == 0) ) {
		notification = NOTIFY_NEVER;
	} 
	else if( strcasecmp(how, "COMPLETE") == 0 ) {
		notification = NOTIFY_COMPLETE;
	} 
	else if( strcasecmp(how, "ALWAYS") == 0 ) {
		notification = NOTIFY_ALWAYS;
	} 
	else if( strcasecmp(how, "ERROR") == 0 ) {
		notification = NOTIFY_ERROR;
	} 
	else {
		push_error(stderr, "Notification must be 'Never', "
				 "'Always', 'Complete', or 'Error'\n" );
		ABORT_AND_RETURN( 1 );
	}

	buffer.formatstr( "%s = %d", ATTR_JOB_NOTIFICATION, notification);
	InsertJobExpr (buffer);

	if ( how ) {
		free(how);
	}
	return 0;
}

int SubmitHash::SetNotifyUser()
{
	RETURN_IF_ABORT();
	bool needs_warning = false;
	MyString buffer;

	char *who = submit_param( SUBMIT_KEY_NotifyUser, ATTR_NOTIFY_USER );

	if (who) {
		if( ! already_warned_notification_never ) {
			if( !strcasecmp(who, "false") ) {
				needs_warning = true;
			}
			if( !strcasecmp(who, "never") ) {
				needs_warning = true;
			}
		}
		if( needs_warning && ! already_warned_notification_never ) {
			auto_free_ptr tmp(param("UID_DOMAIN"));

			push_warning( stderr, "You used \"%s = %s\" in your submit file.\n"
					"This means notification email will go to user \"%s@%s\".\n"
					"This is probably not what you expect!\n"
					"If you do not want notification email, put \"notification = never\"\n"
					"into your submit file, instead.\n",
					SUBMIT_KEY_NotifyUser, who, who, tmp.ptr() );
			already_warned_notification_never = true;
		}
		buffer.formatstr( "%s = \"%s\"", ATTR_NOTIFY_USER, who);
		InsertJobExpr (buffer);
		free(who);
	}
	return 0;
}

int SubmitHash::SetWantGracefulRemoval()
{
	RETURN_IF_ABORT();
	char *how = submit_param( SUBMIT_KEY_WantGracefulRemoval, ATTR_WANT_GRACEFUL_REMOVAL );
	MyString buffer;

	if( how ) {
		buffer.formatstr( "%s = %s", ATTR_WANT_GRACEFUL_REMOVAL, how );
		InsertJobExpr (buffer);
		free( how );
	}
	return 0;
}

int SubmitHash::SetJobMaxVacateTime()
{
	RETURN_IF_ABORT();
	char *expr = submit_param( SUBMIT_KEY_JobMaxVacateTime, ATTR_JOB_MAX_VACATE_TIME );
	MyString buffer;

	if( expr ) {
		buffer.formatstr( "%s = %s", ATTR_JOB_MAX_VACATE_TIME, expr );
		InsertJobExpr (buffer);
		free( expr );
	}
	return 0;
}

int SubmitHash::SetEmailAttributes()
{
	RETURN_IF_ABORT();
	char *attrs = submit_param( SUBMIT_KEY_EmailAttributes, ATTR_EMAIL_ATTRIBUTES );

	if ( attrs ) {
		StringList attr_list( attrs );

		if ( !attr_list.isEmpty() ) {
			char *tmp;
			MyString buffer;

			tmp = attr_list.print_to_string();
			buffer.formatstr( "%s = \"%s\"", ATTR_EMAIL_ATTRIBUTES, tmp );
			InsertJobExpr( buffer );
			free( tmp );
		}

		free( attrs );
	}
	return 0;
}

/**
 * We search to see if the ad has any CronTab attributes defined
 * If so, then we will check to make sure they are using the 
 * proper syntax and then stuff them in the ad
 **/
int SubmitHash::SetCronTab()
{
	RETURN_IF_ABORT();
	MyString buffer;
		//
		// For convienence I put all the attributes in array
		// and just run through the ad looking for them
		//
	const char* attributes[] = { SUBMIT_KEY_CronMinute,
								 SUBMIT_KEY_CronHour,
								 SUBMIT_KEY_CronDayOfMonth,
								 SUBMIT_KEY_CronMonth,
								 SUBMIT_KEY_CronDayOfWeek,
								};
	const int CronFields = COUNTOF(attributes);

	int ctr;
	char *param = NULL;
	CronTab::initRegexObject();
	for ( ctr = 0; ctr < CronFields; ctr++ ) {
		param = submit_param( attributes[ctr], CronTab::attributes[ctr] );
		if ( param != NULL ) {
				//
				// We'll try to be nice and validate it first
				//
			MyString error;
			if ( ! CronTab::validateParameter( ctr, param, error ) ) {
				push_error( stderr, "%s\n", error.Value() );
				ABORT_AND_RETURN( 1 );
			}
				//
				// Go ahead and stuff it in the job ad now
				// The parameters must be wrapped in quotation marks
				//				
			buffer.formatstr( "%s = \"%s\"", CronTab::attributes[ctr], param );
			InsertJobExpr (buffer);
			free( param );
			NeedsJobDeferral = true;
		}		
	} // FOR
		//
		// Validation
		// Because the scheduler universe doesn't use a Starter,
		// we can't let them use the CronTab scheduling which needs 
		// to be able to use the job deferral feature
		//
	if ( NeedsJobDeferral && JobUniverse == CONDOR_UNIVERSE_SCHEDULER ) {
		push_error( stderr, "CronTab scheduling does not work for scheduler universe jobs.\n"
						"Consider submitting this job using the local universe, instead\n");
		ABORT_AND_RETURN( 1 );
	} // validation	
	return 0;
}

int SubmitHash::SetMatchListLen()
{
	RETURN_IF_ABORT();
	MyString buffer;
	int len = 0;
	char* tmp = submit_param( SUBMIT_KEY_LastMatchListLength,
							  ATTR_LAST_MATCH_LIST_LENGTH );
	if( tmp ) {
		len = atoi(tmp);
		buffer.formatstr( "%s = %d", ATTR_LAST_MATCH_LIST_LENGTH, len );
		InsertJobExpr( buffer );
		free( tmp );
	}
	return 0;
}

int SubmitHash::SetDAGNodeName()
{
	RETURN_IF_ABORT();
	char* name = submit_param( ATTR_DAG_NODE_NAME_ALT, ATTR_DAG_NODE_NAME );
	MyString buffer;
	if( name ) {
		buffer.formatstr( "%s = \"%s\"", ATTR_DAG_NODE_NAME, name );
		InsertJobExpr( buffer );
		free( name );
	}
	return 0;
}

int SubmitHash::SetDAGManJobId()
{
	RETURN_IF_ABORT();
	char* id = submit_param(SUBMIT_KEY_DAGManJobId, ATTR_DAGMAN_JOB_ID );
	MyString buffer;
	if( id ) {
		(void) buffer.formatstr( "%s = \"%s\"", ATTR_DAGMAN_JOB_ID, id );
		InsertJobExpr( buffer );
		free( id );
	}
	return 0;
}

int SubmitHash::SetLogNotes()
{
	RETURN_IF_ABORT();
	char * LogNotesVal = submit_param( SUBMIT_KEY_LogNotesCommand, ATTR_SUBMIT_EVENT_NOTES );
	if ( LogNotesVal ) {
		InsertJobExprString( ATTR_SUBMIT_EVENT_NOTES, LogNotesVal );
		free( LogNotesVal );
	}
	return 0;
}

int SubmitHash::SetUserNotes()
{
	RETURN_IF_ABORT();
	char * UserNotesVal = submit_param( SUBMIT_KEY_UserNotesCommand, ATTR_SUBMIT_EVENT_USER_NOTES );
	if ( UserNotesVal ) {
		InsertJobExprString( ATTR_SUBMIT_EVENT_USER_NOTES, UserNotesVal );
		free( UserNotesVal );
	}
	return 0;
}

int SubmitHash::SetStackSize()
{
	RETURN_IF_ABORT();
	char * StackSizeVal = submit_param(SUBMIT_KEY_StackSize,ATTR_STACK_SIZE);
	MyString buffer;
	if( StackSizeVal ) {
		(void) buffer.formatstr( "%s = %s", ATTR_STACK_SIZE, StackSizeVal);
		InsertJobExpr(buffer);
		free( StackSizeVal );
	}
	return 0;
}

int SubmitHash::SetRemoteInitialDir()
{
	RETURN_IF_ABORT();
	char *who = submit_param( SUBMIT_KEY_RemoteInitialDir, ATTR_JOB_REMOTE_IWD );
	MyString buffer;
	if (who) {
		buffer.formatstr( "%s = \"%s\"", ATTR_JOB_REMOTE_IWD, who);
		InsertJobExpr (buffer);
		free(who);
	}
	return 0;
}

int SubmitHash::SetExitRequirements()
{
	RETURN_IF_ABORT();
	char *who = submit_param( SUBMIT_KEY_ExitRequirements,
							  ATTR_JOB_EXIT_REQUIREMENTS );

	if (who) {
		push_error(stderr, "%s is deprecated.\n"
			"Please use on_exit_remove or on_exit_hold.\n", 
			SUBMIT_KEY_ExitRequirements );
		free(who);
		ABORT_AND_RETURN( 1 );
	}
	return 0;
}
	
int SubmitHash::SetOutputDestination()
{
	RETURN_IF_ABORT();
	char *od = submit_param( SUBMIT_KEY_OutputDestination, ATTR_OUTPUT_DESTINATION );
	MyString buffer;
	if (od) {
		buffer.formatstr( "%s = \"%s\"", ATTR_OUTPUT_DESTINATION, od);
		InsertJobExpr (buffer);
		free(od);
	}
	return 0;
}

int SubmitHash::SetArguments()
{
	RETURN_IF_ABORT();
	ArgList arglist;
	char	*args1 = submit_param( SUBMIT_KEY_Arguments1, ATTR_JOB_ARGUMENTS1 );
		// NOTE: no ATTR_JOB_ARGUMENTS2 in the following,
		// because that is the same as Arguments1
	char    *args2 = submit_param( SUBMIT_KEY_Arguments2 );
	bool allow_arguments_v1 = submit_param_bool( SUBMIT_KEY_AllowArgumentsV1, NULL, false );
	bool args_success = true;
	MyString error_msg;

	if(args2 && args1 && ! allow_arguments_v1 ) {
		push_error(stderr, "If you wish to specify both 'arguments' and\n"
		 "'arguments2' for maximal compatibility with different\n"
		 "versions of Condor, then you must also specify\n"
		 "allow_arguments_v1=true.\n");
		ABORT_AND_RETURN(1);
	}

	if(args2) {
		args_success = arglist.AppendArgsV2Quoted(args2,&error_msg);
	}
	else if(args1) {
		args_success = arglist.AppendArgsV1WackedOrV2Quoted(args1,&error_msg);
	}

	if(!args_success) {
		if(error_msg.IsEmpty()) {
			error_msg = "ERROR in arguments.";
		}
		push_error(stderr, "%s\nThe full arguments you specified were: %s\n",
				error_msg.Value(),
				args2 ? args2 : args1);
		ABORT_AND_RETURN(1);
	}

	MyString strbuffer;
	MyString value;
	bool MyCondorVersionRequiresV1 = arglist.InputWasV1() || arglist.CondorVersionRequiresV1(getScheddVersion());
	if(MyCondorVersionRequiresV1) {
		args_success = arglist.GetArgsStringV1Raw(&value,&error_msg);
		strbuffer.formatstr("%s = \"%s\"",ATTR_JOB_ARGUMENTS1,
					   value.EscapeChars("\"",'\\').Value());
	}
	else {
		args_success = arglist.GetArgsStringV2Raw(&value,&error_msg);
		strbuffer.formatstr("%s = \"%s\"",ATTR_JOB_ARGUMENTS2,
					   value.EscapeChars("\"",'\\').Value());
	}

	if(!args_success) {
		push_error(stderr, "failed to insert arguments: %s\n",
				error_msg.Value());
		ABORT_AND_RETURN(1);
	}

	InsertJobExpr (strbuffer);

	if( JobUniverse == CONDOR_UNIVERSE_JAVA && arglist.Count() == 0)
	{
		push_error(stderr, "In Java universe, you must specify the class name to run.\nExample:\n\narguments = MyClass\n\n");
		ABORT_AND_RETURN( 1 );
	}

	if(args1) free(args1);
	if(args2) free(args2);
	return 0;
}

//
// SetDeferral()
// Inserts the job deferral time into the ad if present
// This needs to be called before SetRequirements()
//
int SubmitHash::SetJobDeferral()
{
	RETURN_IF_ABORT();

		// Job Deferral Time
		// Only update the job ad if they provided a deferral time
		// We will only be able to validate the deferral time when the
		// Starter evaluates it and tries to set the timer for it
		//
	MyString buffer;
	char *temp = submit_param( SUBMIT_KEY_DeferralTime, ATTR_DEFERRAL_TIME );
	if ( temp != NULL ) {
		// make certain the input is valid
		if (non_negative_int_fail(SUBMIT_KEY_DeferralTime, temp)) return abort_code;
			
		buffer.formatstr( "%s = %s", ATTR_DEFERRAL_TIME, temp );
		InsertJobExpr (buffer);
		free( temp );
		NeedsJobDeferral = true;
	}
	
		//
		// If this job needs the job deferral functionality, we
		// need to make sure we always add in the DeferralWindow
		// and the DeferralPrepTime attributes.
		// We have a separate if clause because SetCronTab() can
		// also set NeedsJobDeferral
		//
	if ( NeedsJobDeferral ) {		
			//
			// Job Deferral Window
			// The window allows for some slack if the Starter
			// misses the exact execute time for a job
			//
			// NOTE: There are two separate attributes, CronWindow and
			// DeferralWindow, but they are mapped to the same attribute
			// in the job's classad (ATTR_DEFERRAL_WINDOW). This is just
			// it is less confusing for users that are using one feature but
			// not the other. CronWindow overrides DeferralWindow if they
			// both are set. The manual should talk about this.
			//
		const char *windowParams[] = { SUBMIT_KEY_CronWindow, SUBMIT_KEY_DeferralWindow };
		const char *windowAltParams[]  = { ATTR_CRON_WINDOW, ATTR_DEFERRAL_WINDOW };
		int ctr;
		for (ctr = 0; ctr < 2; ctr++) {
			temp = submit_param( windowParams[ctr], windowAltParams[ctr] );
			if ( temp != NULL ) {
				break;
			}
		} // FOR
			//
			// If we have a parameter from the job file, use that value
			//
		if ( temp != NULL ){
			
			// make certain the input is valid
			if (non_negative_int_fail(SUBMIT_KEY_DeferralWindow, temp)) return abort_code;
			
			buffer.formatstr(  "%s = %s", ATTR_DEFERRAL_WINDOW, temp );	
			free( temp );
			//
			// Otherwise, use the default value
			//
		} else {
			buffer.formatstr( "%s = %d",
				   ATTR_DEFERRAL_WINDOW, JOB_DEFERRAL_WINDOW_DEFAULT );
		}
		InsertJobExpr (buffer);
		
			//
			// Job Deferral Prep Time
			// This is how many seconds before the job should run it is
			// sent over to the starter
			//
			// NOTE: There are two separate attributes, CronPrepTime and
			// DeferralPrepTime, but they are mapped to the same attribute
			// in the job's classad (ATTR_DEFERRAL_PREP_TIME). This is just
			// it is less confusing for users that are using one feature but
			// not the other. CronPrepTime overrides DeferralPrepTime if they
			// both are set. The manual should talk about this.
			//
		const char *prepParams[] = { SUBMIT_KEY_CronPrepTime, SUBMIT_KEY_DeferralPrepTime };
		const char *prepAltParams[]  = { ATTR_CRON_PREP_TIME, ATTR_DEFERRAL_PREP_TIME };
		for (ctr = 0; ctr < 2; ctr++) {
			temp = submit_param( prepParams[ctr], prepAltParams[ctr] );
			if ( temp != NULL ) {
				break;
			}
		} // FOR
			//
			// If we have a parameter from the job file, use that value
			//
		if ( temp != NULL ){
			// make certain the input is valid
			if (non_negative_int_fail(SUBMIT_KEY_DeferralPrepTime, temp)) return abort_code;
			
			buffer.formatstr(  "%s = %s", ATTR_DEFERRAL_PREP_TIME, temp );	
			free( temp );
			//
			// Otherwise, use the default value
			//
		} else {
			buffer.formatstr( "%s = %d",
				   ATTR_DEFERRAL_PREP_TIME, JOB_DEFERRAL_PREP_TIME_DEFAULT );
		}
		InsertJobExpr (buffer);
		
			//
			// Schedd Polling Interval
			// We need make sure we have this in the job ad as well
			// because it is used by the Requirements expression generated
			// in check_requirements() for job deferral 
			//
		temp = param( "SCHEDD_INTERVAL" );
		if ( temp != NULL ) {
			buffer.formatstr( "%s = %s", ATTR_SCHEDD_INTERVAL, temp );
			free( temp );
		} else {
			buffer.formatstr( "%s = %d", ATTR_SCHEDD_INTERVAL,
										SCHEDD_INTERVAL_DEFAULT );
		}
		InsertJobExpr (buffer);
	
			//
			// Validation
			// Because the scheduler universe doesn't use a Starter,
			// we can't let them use the job deferral feature
			//
		if ( JobUniverse == CONDOR_UNIVERSE_SCHEDULER ) {
			push_error( stderr, "Job deferral scheduling does not work for scheduler universe jobs.\n"
							"Consider submitting this job using the local universe, instead\n");
			ABORT_AND_RETURN( 1 );
		} // validation	
	}

	return 0;
}


int SubmitHash::SetJobRetries()
{
	RETURN_IF_ABORT();

	std::string erc, ehc;
	submit_param_exists(SUBMIT_KEY_OnExitRemoveCheck, ATTR_ON_EXIT_REMOVE_CHECK, erc);
	submit_param_exists(SUBMIT_KEY_OnExitHoldCheck, ATTR_ON_EXIT_HOLD_CHECK, ehc);

	long long num_retries = param_integer("DEFAULT_JOB_MAX_RETRIES", 10);
	long long success_code = 0;
	std::string retry_until;

	bool enable_retries = false;
	if (submit_param_long_exists(SUBMIT_KEY_MaxRetries, ATTR_JOB_MAX_RETRIES, num_retries)) { enable_retries = true; }
	if (submit_param_long_exists(SUBMIT_KEY_SuccessExitCode, ATTR_JOB_SUCCESS_EXIT_CODE, success_code, true)) { enable_retries = true; }
	if (submit_param_exists(SUBMIT_KEY_RetryUntil, NULL, retry_until)) { enable_retries = true; }
	if ( ! enable_retries)
	{
		// if none of these knobs are defined, then there are no retries.
		// Just insert the default on-exit-hold and on-exit-remove expressions
		if (erc.empty()) {
			AssignJobVal (ATTR_ON_EXIT_REMOVE_CHECK, true);
		} else {
			erc.insert(0, ATTR_ON_EXIT_REMOVE_CHECK "=");
			InsertJobExpr(erc.c_str());
		}
		if (ehc.empty()) {
			AssignJobVal (ATTR_ON_EXIT_HOLD_CHECK, false);
		} else {
			ehc.insert(0, ATTR_ON_EXIT_HOLD_CHECK "=");
			InsertJobExpr(ehc.c_str());
		}
		RETURN_IF_ABORT();
		return 0;
	}

	// if there is a retry_until value, figure out of it is an fultility exit code or an expression
	// and validate it.
	if ( ! retry_until.empty()) {
		ExprTree * tree = NULL;
		bool valid_retry_until = (0 == ParseClassAdRvalExpr(retry_until.c_str(), tree));
		if (valid_retry_until && tree) {
			ClassAd tmp;
			StringList refs;
			tmp.GetExprReferences(retry_until.c_str(), &refs, &refs);
			long long futility_code;
			if (refs.isEmpty() && string_is_long_param(retry_until.c_str(), futility_code)) {
				if (futility_code < INT_MIN || futility_code > INT_MAX) {
					valid_retry_until = false;
				} else {
					retry_until.clear();
					formatstr(retry_until, ATTR_ON_EXIT_CODE " == %d", (int)futility_code);
				}
			} else {
				ExprTree * expr = WrapExprTreeInParensForOp(tree, classad::Operation::LOGICAL_OR_OP);
				if (expr != tree) {
					tree = expr; // expr now owns tree
					retry_until.clear();
					ExprTreeToString(tree, retry_until);
				}
			}
		}
		delete tree;

		if ( ! valid_retry_until) {
			push_error(stderr, "%s=%s is invalid, it must be an integer or boolean expression.\n", SUBMIT_KEY_RetryUntil, retry_until.c_str());
			ABORT_AND_RETURN( 1 );
		}
	}

	AssignJobVal(ATTR_JOB_MAX_RETRIES, num_retries);

	// Build the appropriate OnExitRemove expression, we will fill in success exit status value and other clauses later.
	const char * basic_exit_remove_expr = ATTR_ON_EXIT_REMOVE_CHECK " = "
		ATTR_NUM_JOB_COMPLETIONS " > " ATTR_JOB_MAX_RETRIES " || " ATTR_ON_EXIT_CODE " == ";

	// build the sub expression that checks for exit codes that should end retries
	std::string code_check;
	if (success_code != 0) {
		AssignJobVal(ATTR_JOB_SUCCESS_EXIT_CODE, success_code);
		code_check = ATTR_JOB_SUCCESS_EXIT_CODE;
	} else {
		formatstr(code_check, "%d", (int)success_code);
	}
	if ( ! retry_until.empty()) {
		code_check += " || ";
		code_check += retry_until;
	}

	// paste up the final OnExitRemove expression
	std::string onexitrm(basic_exit_remove_expr);
	onexitrm += code_check;

	// if the user supplied an on_exit_remove expression, || it in
	if ( ! erc.empty()) {
		if ( ! check_expr_and_wrap_for_op(erc, classad::Operation::LOGICAL_OR_OP)) {
			push_error(stderr, "%s=%s is invalid, it must be a boolean expression.\n", SUBMIT_KEY_OnExitRemoveCheck, erc.c_str());
			ABORT_AND_RETURN( 1 );
		}
		onexitrm += " || ";
		onexitrm += erc;
	}
	// Insert the final OnExitRemove expression into the job
	InsertJobExpr(onexitrm.c_str());
	RETURN_IF_ABORT();

	// paste up the final OnExitHold expression and insert it into the job.
	if (ehc.empty()) {
		// TODO: remove this trvial default when it is no longer needed
		AssignJobVal (ATTR_ON_EXIT_HOLD_CHECK, false);
	} else {
		ehc.insert(0, ATTR_ON_EXIT_HOLD_CHECK "=");
		InsertJobExpr(ehc.c_str());
	}

	RETURN_IF_ABORT();
	return 0;
}


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


/*
	Walk the list of submit commands (as stored in the
	insert() table) looking for a handful of Remote_FOO
	attributes we want to special case.  Translate them (if necessary)
	and stuff them into the ClassAd.
*/
int SubmitHash::SetRemoteAttrs()
{
	RETURN_IF_ABORT();
	const int REMOTE_PREFIX_LEN = (int)strlen(SUBMIT_KEY_REMOTE_PREFIX);

	struct ExprItem {
		const char * submit_expr;
		const char * special_expr;
		const char * job_expr;
	};

	ExprItem tostringize[] = {
		{ SUBMIT_KEY_GlobusRSL, "globus_rsl", ATTR_GLOBUS_RSL },
		{ SUBMIT_KEY_NordugridRSL, "nordugrid_rsl", ATTR_NORDUGRID_RSL },
		{ SUBMIT_KEY_GridResource, 0, ATTR_GRID_RESOURCE },
	};
	const int tostringizesz = sizeof(tostringize) / sizeof(tostringize[0]);


	HASHITER it = hash_iter_begin(SubmitMacroSet);
	for( ; ! hash_iter_done(it); hash_iter_next(it)) {

		const char * key = hash_iter_key(it);
		int remote_depth = 0;
		while(strncasecmp(key, SUBMIT_KEY_REMOTE_PREFIX, REMOTE_PREFIX_LEN) == 0) {
			remote_depth++;
			key += REMOTE_PREFIX_LEN;
		}

		if(remote_depth == 0) {
			continue;
		}

		MyString preremote = "";
		for(int i = 0; i < remote_depth; ++i) {
			preremote += SUBMIT_KEY_REMOTE_PREFIX;
		}

		if(strcasecmp(key, SUBMIT_KEY_Universe) == 0 || strcasecmp(key, ATTR_JOB_UNIVERSE) == 0) {
			MyString Univ1 = preremote + SUBMIT_KEY_Universe;
			MyString Univ2 = preremote + ATTR_JOB_UNIVERSE;
			MyString val = submit_param_mystring(Univ1.Value(), Univ2.Value());
			int univ = CondorUniverseNumberEx(val.Value());
			if(univ == 0) {
				push_error(stderr, "Unknown universe of '%s' specified\n", val.Value());
				ABORT_AND_RETURN( 1 );
			}
			MyString attr = preremote + ATTR_JOB_UNIVERSE;
			dprintf(D_FULLDEBUG, "Adding %s = %d\n", attr.Value(), univ);
			InsertJobExprInt(attr.Value(), univ);

		} else {

			for(int i = 0; i < tostringizesz; ++i) {
				ExprItem & item = tostringize[i];

				if(	strcasecmp(key, item.submit_expr) &&
					(item.special_expr == NULL || strcasecmp(key, item.special_expr)) &&
					strcasecmp(key, item.job_expr)) {
					continue;
				}
				MyString key1 = preremote + item.submit_expr;
				MyString key2 = preremote + item.special_expr;
				MyString key3 = preremote + item.job_expr;
				const char * ckey1 = key1.Value();
				const char * ckey2 = key2.Value();
				if(item.special_expr == NULL) { ckey2 = NULL; }
				const char * ckey3 = key3.Value();
				char * val = submit_param(ckey1, ckey2);
				if( val == NULL ) {
					val = submit_param(ckey3);
				}
				ASSERT(val); // Shouldn't have gotten here if it's missing.
				dprintf(D_FULLDEBUG, "Adding %s = %s\n", ckey3, val);
				InsertJobExprString(ckey3, val);
				free(val);
				break;
			}
		}
	}
	hash_iter_delete(&it);

	return 0;
}

int SubmitHash::SetJobMachineAttrs()
{
	RETURN_IF_ABORT();
	MyString job_machine_attrs = submit_param_mystring( "job_machine_attrs", ATTR_JOB_MACHINE_ATTRS );
	MyString history_len_str = submit_param_mystring( "job_machine_attrs_history_length", ATTR_JOB_MACHINE_ATTRS_HISTORY_LENGTH );
	MyString buffer;

	if( job_machine_attrs.Length() ) {
		InsertJobExprString(ATTR_JOB_MACHINE_ATTRS,job_machine_attrs.Value());
	}
	if( history_len_str.Length() ) {
		char *endptr=NULL;
		long history_len = strtol(history_len_str.Value(),&endptr,10);
		if( history_len > INT_MAX || history_len < 0 || *endptr) {
			push_error(stderr, "job_machine_attrs_history_length=%s is out of bounds 0 to %d\n",history_len_str.Value(),INT_MAX);
			ABORT_AND_RETURN( 1 );
		}
		AssignJobVal(ATTR_JOB_MACHINE_ATTRS_HISTORY_LENGTH, history_len);
	}
	return 0;
}

static char * check_docker_image(char * docker_image)
{
	// trim leading & trailing whitespace and remove surrounding "" if any.
	docker_image = trim_and_strip_quotes_in_place(docker_image);

	// TODO: add code here to validate docker image argument (if possible)
	return docker_image;
}


int SubmitHash::SetExecutable()
{
	RETURN_IF_ABORT();
	bool	transfer_it = true;
	bool	ignore_it = false;
	char	*ename = NULL;
	char	*macro_value = NULL;
	_submit_file_role role = SFR_EXECUTABLE;
	MyString	full_ename;
	MyString buffer;

	YourStringNoCase gridType(JobGridType.Value());

	// In vm universe and ec2/boinc grid jobs, 'Executable'
	// parameter is not a real file but just the name of job.
	if ( JobUniverse == CONDOR_UNIVERSE_VM ||
		 ( JobUniverse == CONDOR_UNIVERSE_GRID &&
		   ( gridType == "ec2" ||
			 gridType == "gce"  ||
			 gridType == "azure"  ||
			 gridType == "boinc" ) ) ) {
		ignore_it = true;
		role = SFR_PSEUDO_EXECUTABLE;
	}

	if (IsDockerJob) {
		char * docker_image = submit_param(SUBMIT_KEY_DockerImage, ATTR_DOCKER_IMAGE);
		if ( ! docker_image) {
			push_error(stderr, "docker jobs require a docker_image\n");
			ABORT_AND_RETURN(1);
		}
		char * image = check_docker_image(docker_image);
		if ( ! image || ! image[0]) {
			push_error(stderr, "'%s' is not a valid docker_image\n", docker_image);
			ABORT_AND_RETURN(1);
		}
		buffer.formatstr("%s = \"%s\"", ATTR_DOCKER_IMAGE, image);
		InsertJobExpr(buffer);
		free(docker_image);
		role = SFR_PSEUDO_EXECUTABLE;
	}

	ename = submit_param( SUBMIT_KEY_Executable, ATTR_JOB_CMD );
	if( ename == NULL ) {
		if (IsDockerJob) {
			// docker jobs don't require an executable.
			ignore_it = true;
			role = SFR_PSEUDO_EXECUTABLE;
		} else {
			push_error(stderr, "No '%s' parameter was provided\n", SUBMIT_KEY_Executable);
			ABORT_AND_RETURN( 1 );
		}
	}

	macro_value = submit_param( SUBMIT_KEY_TransferExecutable, ATTR_TRANSFER_EXECUTABLE );
	if ( macro_value ) {
		if ( macro_value[0] == 'F' || macro_value[0] == 'f' ) {
			AssignJobVal(ATTR_TRANSFER_EXECUTABLE, false);
			transfer_it = false;
		}
		free( macro_value );
	} else {
		// For Docker Universe, if xfer_exe not set at all, and we have an exe
		// heuristically set xfer_exe to false if is a absolute path
		if (IsDockerJob && ename && ename[0] == '/') {
			AssignJobVal(ATTR_TRANSFER_EXECUTABLE, false);
			transfer_it = false;
			ignore_it = true;
		}
	}

	if ( ignore_it ) {
		if( transfer_it == true ) {
			AssignJobVal(ATTR_TRANSFER_EXECUTABLE, false);
			transfer_it = false;
		}
	}

	// If we're not transfering the executable, leave a relative pathname
	// unresolved. This is mainly important for the Globus universe.
	if ( transfer_it ) {
		full_ename = full_path( ename, false );
	} else {
		full_ename = ename;
	}
	if ( !ignore_it ) {
		check_and_universalize_path(full_ename);
	}

	buffer.formatstr( "%s = \"%s\"", ATTR_JOB_CMD, full_ename.Value());
	InsertJobExpr (buffer);

		/* MPI REALLY doesn't like these! */
	if ( JobUniverse != CONDOR_UNIVERSE_MPI ) {
		InsertJobExpr ("MinHosts = 1");
		InsertJobExpr ("MaxHosts = 1");
	} 

	if ( JobUniverse == CONDOR_UNIVERSE_PARALLEL) {
		InsertJobExpr ("WantIOProxy = TRUE");
		buffer.formatstr("%s = TRUE", ATTR_JOB_REQUIRES_SANDBOX);
		InsertJobExpr (buffer);
	}

	InsertJobExpr ("CurrentHosts = 0");

	switch(JobUniverse) 
	{
	case CONDOR_UNIVERSE_STANDARD:
		buffer.formatstr( "%s = TRUE", ATTR_WANT_REMOTE_SYSCALLS);
		InsertJobExpr (buffer);
		buffer.formatstr( "%s = TRUE", ATTR_WANT_CHECKPOINT);
		InsertJobExpr (buffer);
		break;
	case CONDOR_UNIVERSE_MPI:  // for now
		/*
		if(!use_condor_mpi_universe) {
			push_error(stderr, "mpi universe no longer suppported. Please use parallel universe.\n"
					"You can submit mpi jobs using parallel universe. Most likely, a substitution of\n"
					"\nuniverse = parallel\n\n"
					"in place of\n"
					"\nuniverse = mpi\n\n"
					"in you submit description file will suffice.\n"
					"See the HTCondor Manual Parallel Applications section (2.9) for further details.\n");
			ABORT_AND_RETURN( 1 );
		}
		*/
		//Purposely fall through if use_condor_mpi_universe is true
	case CONDOR_UNIVERSE_VANILLA:
	case CONDOR_UNIVERSE_LOCAL:
	case CONDOR_UNIVERSE_SCHEDULER:
	case CONDOR_UNIVERSE_PARALLEL:
	case CONDOR_UNIVERSE_GRID:
	case CONDOR_UNIVERSE_JAVA:
	case CONDOR_UNIVERSE_VM:
		buffer.formatstr( "%s = FALSE", ATTR_WANT_REMOTE_SYSCALLS);
		InsertJobExpr (buffer);
		buffer.formatstr( "%s = FALSE", ATTR_WANT_CHECKPOINT);
		InsertJobExpr (buffer);
		break;
	default:
		push_error(stderr, "Unknown universe %d (%s)\n", JobUniverse, CondorUniverseName(JobUniverse) );
		ABORT_AND_RETURN( 1 );
	}

#if 1
	if (FnCheckFile) {
		int rval = FnCheckFile(CheckFileArg, this, role, ename, (transfer_it ? 1 : 0));
		if (rval) { ABORT_AND_RETURN( rval ); }
	}
#else
	// In vm universe and ec2, there is no executable file.
	bool copy_to_spool = false;
	if ( ignore_it ) {
		copy_to_spool = false;
	}else {
		bool param_exists;
		copy_to_spool = submit_param_bool( SUBMIT_KEY_CopyToSpool, "CopyToSpool", false, &param_exists );
		if ( ! param_exists) {
			if ( JobUniverse == CONDOR_UNIVERSE_STANDARD ) {
					// Standard universe jobs can't restore from a checkpoint
					// if the executable changes.  Therefore, it is deemed
					// too risky to have copy_to_spool=false by default
					// for standard universe.
				copy_to_spool = true;
			}
			else {
					// In so many cases, copy_to_spool=true would just add
					// needless overhead.  Therefore, (as of 6.9.3), the
					// default is false.
				copy_to_spool = false;
			}
		}
	}


	// generate initial checkpoint file
	// This is ignored by the schedd in 7.5.5+.  Prior to that, the
	// basename must match the name computed by the schedd.
	char *IckptName = GetSpooledExecutablePath(jid.cluster, "");

	// ensure the executables exist and spool them only if no 
	// $$(arch).$$(opsys) are specified  (note that if we are simply
	// dumping the class-ad to a file, we won't actually transfer
	// or do anything [nothing that follows will affect the ad])
	if ( transfer_it && !DumpClassAdToFile && !strstr(ename,"$$") ) {

		StatInfo si(ename);
		if ( SINoFile == si.Error () ) {
			push_error(stderr, "Executable file %s does not exist\n", ename );
			ABORT_AND_RETURN( 1 );
		}
			    
		if (!si.Error() && (si.GetFileSize() == 0)) {
			push_error(stderr, "Executable file %s has zero length\n", 
					 ename );
			ABORT_AND_RETURN( 1 );
		}

		// spool executable if necessary
		if ( copy_to_spool && jid.proc == 0 ) {

			bool try_ickpt_sharing = false;
			CondorVersionInfo cvi(getScheddVersion());
			if (cvi.built_since_version(7, 3, 0)) {
				try_ickpt_sharing = param_boolean("SHARE_SPOOLED_EXECUTABLES",
				                                  true);
			}

			MyString md5;
			if (try_ickpt_sharing) {
				Condor_MD_MAC cmm;
				unsigned char* md5_raw;
				if (!cmm.addMDFile(ename)) {
					dprintf(D_ALWAYS,
					        "SHARE_SPOOLED_EXECUTABLES will not be used: "
					            "MD5 of file %s failed\n",
					        ename);
				}
				else if ((md5_raw = cmm.computeMD()) == NULL) {
					dprintf(D_ALWAYS,
					        "SHARE_SPOOLED_EXECUTABLES will not be used: "
					            "no MD5 support in this Condor build\n");
				}
				else {
					for (int i = 0; i < MAC_SIZE; i++) {
						md5.formatstr_cat("%02x", static_cast<int>(md5_raw[i]));
					}
					free(md5_raw);
				}
			}
			int ret;
			if (!md5.IsEmpty()) {
				ClassAd tmp_ad;
				tmp_ad.Assign(ATTR_OWNER, owner);
				tmp_ad.Assign(ATTR_JOB_CMD_MD5, md5.Value());
				ret = MyQ->send_SpoolFileIfNeeded(tmp_ad);
			}
			else {
				ret = MyQ->send_SpoolFile(IckptName);
			}

			if (ret < 0) {
				push_error(stderr, "Request to transfer executable %s failed\n",
				         IckptName );
				ABORT_AND_RETURN( 1 );
			}

			free(IckptName); IckptName = NULL;

			// ret will be 0 if the SchedD gave us the go-ahead to send
			// the file. if it's not, the SchedD is using ickpt sharing
			// and already has a copy, so no need
			//
			if ((ret == 0) && MyQ->send_SpoolFileBytes(full_path(ename,false)) < 0) {
				push_error(stderr, "failed to transfer executable file %s\n", 
						 ename );
				ABORT_AND_RETURN( 1 );
			}
		}

	}
	if (IckptName) free( IckptName );

#endif
	if (ename) free(ename);
	return 0;
}

int SubmitHash::SetDescription()
{
	RETURN_IF_ABORT();

	char* description = submit_param( SUBMIT_KEY_Description, ATTR_JOB_DESCRIPTION );
	if ( description ){
		InsertJobExprString(ATTR_JOB_DESCRIPTION, description);
		free(description);
	}
	else if ( IsInteractiveJob ){
		InsertJobExprString(ATTR_JOB_DESCRIPTION, "interactive job");
	}

	MyString batch_name = submit_param_mystring(SUBMIT_KEY_BatchName, ATTR_JOB_BATCH_NAME);
	if ( ! batch_name.empty()) {
		batch_name.trim_quotes("\"'"); // in case they supplied a quoted string, trim the quotes
		InsertJobExprString(ATTR_JOB_BATCH_NAME, batch_name.c_str());
	}
	return 0;
}

static bool mightTransfer( int universe )
{
	switch( universe ) {
	case CONDOR_UNIVERSE_VANILLA:
	case CONDOR_UNIVERSE_MPI:
	case CONDOR_UNIVERSE_PARALLEL:
	case CONDOR_UNIVERSE_JAVA:
	case CONDOR_UNIVERSE_VM:
		return true;
		break;
	default:
		return false;
		break;
	}
	return false;
}


int SubmitHash::SetUniverse()
{
	RETURN_IF_ABORT();
	MyString buffer;

	auto_free_ptr univ(submit_param(SUBMIT_KEY_Universe, ATTR_JOB_UNIVERSE));
	if ( ! univ) {
		// get a default universe from the config file
		univ.set(param("DEFAULT_UNIVERSE"));
	}

	IsDockerJob = false;
	JobUniverse = 0;
	JobGridType.clear();
	VMType.clear();

	if (univ) {
		JobUniverse = CondorUniverseNumberEx(univ.ptr());
		if ( ! JobUniverse) {
			// maybe it's a topping?
			if (MATCH == strcasecmp(univ.ptr(), "docker")) {
				JobUniverse = CONDOR_UNIVERSE_VANILLA;
				IsDockerJob = true;
			}
		}
	} else {
		// if nothing else, it must be a vanilla universe
		//  *changed from "standard" for 7.2.0*
		JobUniverse = CONDOR_UNIVERSE_VANILLA;
	}

	// set the universe into the job
	AssignJobVal(ATTR_JOB_UNIVERSE, JobUniverse);

	// for "scheduler" or "local" or "parallel" universe, this is all we need to do
	if (JobUniverse == CONDOR_UNIVERSE_SCHEDULER ||
		JobUniverse == CONDOR_UNIVERSE_LOCAL ||
		JobUniverse == CONDOR_UNIVERSE_PARALLEL ||
		JobUniverse == CONDOR_UNIVERSE_MPI ||
		JobUniverse == CONDOR_UNIVERSE_JAVA)
	{
		return 0;
	}

	// for vanilla universe, we have some special cases for toppings...
	if (JobUniverse == CONDOR_UNIVERSE_VANILLA) {
		if (IsDockerJob) {
			// TODO: remove this when the docker starter no longer requires it.
			InsertJobExpr("WantDocker=true");
		}
		return 0;
	}

	if (JobUniverse == CONDOR_UNIVERSE_STANDARD) {
#if defined( CLIPPED )
		push_error(stderr, "You are trying to submit a \"%s\" job to Condor. "
				 "However, this installation of Condor does not support the "
				 "Standard Universe.\n%s\n%s\n",
				 univ.ptr(), CondorVersion(), CondorPlatform() );
		ABORT_AND_RETURN( 1 );
#else
		// Standard universe needs file checks disabled to create stdout and stderr
		DisableFileChecks = 0;
		return 0;
#endif
	};


	// "globus" or "grid" universe
	if (JobUniverse == CONDOR_UNIVERSE_GRID) {
	
		// Set JobGridType
		// Check grid_resource. If it starts
		// with '$$(', then we're matchmaking and don't know the grid-type.
		JobGridType = submit_param_mystring( SUBMIT_KEY_GridResource, ATTR_GRID_RESOURCE );
		if (JobGridType.empty()) {
			push_error(stderr, "%s attribute not defined for grid universe job\n",  SUBMIT_KEY_GridResource);
			ABORT_AND_RETURN( 1 );
		}

		if ( starts_with(JobGridType.Value(), "$$(")) {
			JobGridType.clear();
		} else {
			// truncate at the first space
			int ix = JobGridType.FindChar(' ', 0);
			if (ix >= 0) {
				JobGridType.truncate(ix);
			}
		}

		if ( ! JobGridType.empty() ) {
			YourStringNoCase gridType(JobGridType.Value());

			// Validate
			// Valid values are (as of 7.5.1): nordugrid, globus,
			//    gt2, gt5, blah, pbs, lsf, nqs, naregi, condor,
			//    unicore, cream, ec2, sge

			// CRUFT: grid-type 'blah' is deprecated. Now, the specific batch
			//   system names should be used (pbs, lsf). Glite are the only
			//   people who care about the old value. This changed happend in
			//   Condor 6.7.12.
			if (gridType == "gt2" ||
				gridType == "gt5" ||
				gridType == "blah" ||
				gridType == "batch" ||
				gridType == "pbs" ||
				gridType == "sge" ||
				gridType == "lsf" ||
				gridType == "nqs" ||
				gridType == "naregi" ||
				gridType == "condor" ||
				gridType == "nordugrid" ||
				gridType == "ec2" ||
				gridType == "gce" ||
				gridType == "azure" ||
				gridType == "unicore" ||
				gridType == "boinc" ||
				gridType == "cream"){
				// We're ok	
				// Values are case-insensitive for gridmanager, so we don't need to change case			
			} else if ( gridType == "globus" ) {
				JobGridType = "gt2";
				gridType = JobGridType.Value();
			} else {

				push_error(stderr, "Invalid value '%s' for grid type\n"
					"Must be one of: gt2, gt5, pbs, lsf, sge, nqs, condor, nordugrid, unicore, ec2, gce, azure, cream, or boinc\n",
					JobGridType.Value() );
				ABORT_AND_RETURN( 1 );
			}
		}			
		
		return 0;
	};


	if (JobUniverse == CONDOR_UNIVERSE_VM) {

		// virtual machine type ( xen, vmware )
		VMType = submit_param_mystring(SUBMIT_KEY_VM_Type, ATTR_JOB_VM_TYPE);
		if (VMType.empty()) {
			push_error(stderr, "'%s' cannot be found.\n"
					"Please specify '%s' for vm universe "
					"in your submit description file.\n", SUBMIT_KEY_VM_Type, SUBMIT_KEY_VM_Type);
			ABORT_AND_RETURN(1);
		}
		VMType.lower_case();

		// also lookup checkpoint an network so we can force file transfer submit knobs.
		bool VMCheckpoint = submit_param_bool(SUBMIT_KEY_VM_Checkpoint, ATTR_JOB_VM_CHECKPOINT, false);
		if( VMCheckpoint ) {
			bool VMNetworking = submit_param_bool(SUBMIT_KEY_VM_Networking, ATTR_JOB_VM_NETWORKING, false);
			if( VMNetworking ) {
				/*
				 * User explicitly requested vm_checkpoint = true, 
				 * but they also turned on vm_networking, 
				 * For now, vm_networking is not conflict with vm_checkpoint.
				 * If user still wants to use both vm_networking 
				 * and vm_checkpoint, they explicitly need to define 
				 * when_to_transfer_output = ON_EXIT_OR_EVICT.
				 */
				FileTransferOutput_t when_output = FTO_NONE;
				auto_free_ptr vm_tmp(submit_param( ATTR_WHEN_TO_TRANSFER_OUTPUT, "when_to_transfer_output" ));
				if ( vm_tmp ) {
					when_output = getFileTransferOutputNum(vm_tmp.ptr());
				}
				if( when_output != FTO_ON_EXIT_OR_EVICT ) {
					MyString err_msg;
					err_msg = "\nERROR: You explicitly requested "
						"both VM checkpoint and VM networking. "
						"However, VM networking is currently conflict "
						"with VM checkpoint. If you still want to use "
						"both VM networking and VM checkpoint, "
						"you explicitly must define "
						"\"when_to_transfer_output = ON_EXIT_OR_EVICT\"\n";
					print_wrapped_text( err_msg.Value(), stderr );
					ABORT_AND_RETURN( 1 );
				}
			}
			// For vm checkpoint, we turn on condor file transfer
			set_submit_param( ATTR_SHOULD_TRANSFER_FILES, "YES");
			set_submit_param( ATTR_WHEN_TO_TRANSFER_OUTPUT, "ON_EXIT_OR_EVICT");
		} else {
			// Even if we don't use vm_checkpoint, 
			// we always turn on condor file transfer for VM universe.
			// Here there are several reasons.
			// For example, because we may use snapshot disks in vmware, 
			// those snapshot disks need to be transferred back 
			// to this submit machine.
			// For another example, a job user want to transfer vm_cdrom_files 
			// but doesn't want to transfer disk files.
			// If we need the same file system domain, 
			// we will add the requirement of file system domain later as well.
			set_submit_param( ATTR_SHOULD_TRANSFER_FILES, "YES");
			set_submit_param( ATTR_WHEN_TO_TRANSFER_OUTPUT, "ON_EXIT");
		}

		return 0;
	};

	// If we get to here, this is an unknown or unsupported universe.
	if (univ && ! JobUniverse) {
		push_error(stderr, "I don't know about the '%s' universe.\n", univ.ptr());
		ABORT_AND_RETURN( 1 );
	} else if (JobUniverse) {
		push_error(stderr, "'%s' is not a supported universe.\n", CondorUniverseNameUcFirst(JobUniverse));
		ABORT_AND_RETURN( 1 );
	}

	return 0;
}

int SubmitHash::SetMachineCount()
{
	RETURN_IF_ABORT();
	char	*mach_count;
	MyString buffer;
	int		request_cpus = 0;

	bool wantParallel = submit_param_bool("WantParallelScheduling", NULL, false);
	if (wantParallel) {
		AssignJobVal("WantParallelScheduling", true);
	}
 
	if (JobUniverse == CONDOR_UNIVERSE_MPI ||
		JobUniverse == CONDOR_UNIVERSE_PARALLEL || wantParallel) {

		mach_count = submit_param( SUBMIT_KEY_MachineCount, "MachineCount" );
		if( ! mach_count ) { 
				// try an alternate name
			mach_count = submit_param( "node_count", "NodeCount" );
		}
		int tmp;
		if ( mach_count != NULL ) {
			tmp = atoi(mach_count);
			free(mach_count);
		}
		else {
			push_error(stderr, "No machine_count specified!\n" );
			ABORT_AND_RETURN( 1 );
		}

		buffer.formatstr( "%s = %d", ATTR_MIN_HOSTS, tmp);
		InsertJobExpr (buffer);
		buffer.formatstr( "%s = %d", ATTR_MAX_HOSTS, tmp);
		InsertJobExpr (buffer);

		request_cpus = 1;
		RequestCpusIsZeroOrOne = true;
	} else {
		mach_count = submit_param( SUBMIT_KEY_MachineCount, "MachineCount" );
		if( mach_count ) { 
			int tmp = atoi(mach_count);
			free(mach_count);

			if( tmp < 1 ) {
				push_error(stderr, "machine_count must be >= 1\n" );
				ABORT_AND_RETURN( 1 );
			}

			buffer.formatstr( "%s = %d", ATTR_MACHINE_COUNT, tmp);
			InsertJobExpr (buffer);

			request_cpus = tmp;
			RequestCpusIsZeroOrOne = (request_cpus == 0 || request_cpus == 1);
		}
	}

	if ((mach_count = submit_param(SUBMIT_KEY_RequestCpus, ATTR_REQUEST_CPUS))) {
		if (MATCH == strcasecmp(mach_count, "undefined")) {
			RequestCpusIsZeroOrOne = true;
		} else {
			buffer.formatstr("%s = %s", ATTR_REQUEST_CPUS, mach_count);
			InsertJobExpr(buffer);
			RequestCpusIsZeroOrOne = ((MATCH == strcmp(mach_count, "0")) || (MATCH == strcmp(mach_count, "1")));
		}
		free(mach_count);
	} else 
	if (request_cpus > 0) {
		buffer.formatstr("%s = %d", ATTR_REQUEST_CPUS, request_cpus);
		InsertJobExpr(buffer);
	} else 
	if ((mach_count = param("JOB_DEFAULT_REQUESTCPUS"))) {
		if (MATCH == strcasecmp(mach_count, "undefined")) {
			RequestCpusIsZeroOrOne = true;
		} else {
			buffer.formatstr("%s = %s", ATTR_REQUEST_CPUS, mach_count);
			InsertJobExpr(buffer);
			RequestCpusIsZeroOrOne = ((MATCH == strcmp(mach_count, "0")) || (MATCH == strcmp(mach_count, "1")));
		}
		free(mach_count);
	}
	return 0;
}


/* This function is used to handle submit file commands that are inserted
 * into the job ClassAd verbatim, with no special treatment.
 */
struct SimpleExprInfo {
	char const *ad_attr_name;
	char const *name1;
	char const *name2;
	char const *default_value;
	bool quote_it;
};

int SubmitHash::SetSimpleJobExprs()
{
	RETURN_IF_ABORT();
	SimpleExprInfo simple_exprs[] = {
		{ATTR_NEXT_JOB_START_DELAY, SUBMIT_KEY_NextJobStartDelay, ATTR_NEXT_JOB_START_DELAY, NULL, false},
		{ATTR_JOB_KEEP_CLAIM_IDLE, "KeepClaimIdle", "keep_claim_idle", NULL, false},
		{ATTR_JOB_AD_INFORMATION_ATTRS, "JobAdInformationAttrs", "job_ad_information_attrs", NULL, true},
		{NULL,NULL,NULL,NULL,false}
	};

	SimpleExprInfo *i = simple_exprs;
	for( i=simple_exprs; i->name1; i++) {
		char *expr;
		expr = submit_param( i->name1, i->name2 );
		RETURN_IF_ABORT();
		if( !expr ) {
			if( !i->default_value ) {
				continue;
			}
			expr = strdup( i->default_value );
			ASSERT( expr );
		}

		MyString buffer;
		if( i->quote_it ) {
			std::string expr_buf;
			QuoteAdStringValue( expr, expr_buf );
			buffer.formatstr( "%s = %s", i->ad_attr_name, expr_buf.c_str());
		}
		else {
			buffer.formatstr( "%s = %s", i->ad_attr_name, expr);
		}

		InsertJobExpr (buffer);

		free( expr );
		RETURN_IF_ABORT();
	}
	return 0;
}

// Note: you must call SetTransferFiles() *before* calling SetImageSize().
int SubmitHash::SetImageSize()
{
	RETURN_IF_ABORT();

	char	*tmp;
	MyString buffer;

	int64_t exe_disk_size_kb = 0; // disk needed for the exe or vm memory
	int64_t executable_size_kb = 0; // calculated size of the exe
	int64_t image_size_kb = 0;      // same as exe size unless user specified.

	if (JobUniverse == CONDOR_UNIVERSE_VM) {
		// In vm universe, when a VM is suspended, 
		// memory being used by the VM will be saved into a file. 
		// So we need as much disk space as the memory.
		// We call this the 'executable_size' for VM jobs, otherwise
		// ExecutableSize is the size of the cmd
		exe_disk_size_kb = ExecutableSizeKb;
	} else {
		// we should only call calc_image_size_kb on the first
		// proc in the cluster, since the executable cannot change.
		if ( jid.proc < 1 || ExecutableSizeKb <= 0 ) {
			ASSERT (job->LookupString (ATTR_JOB_CMD, buffer));
			ExecutableSizeKb = calc_image_size_kb( buffer.c_str() );
		}
		executable_size_kb = ExecutableSizeKb;
		image_size_kb = ExecutableSizeKb;
		exe_disk_size_kb = ExecutableSizeKb;
	}


	// if the user specifies an initial image size, use that instead 
	// of the calculated 
	tmp = submit_param( SUBMIT_KEY_ImageSize, ATTR_IMAGE_SIZE );
	if( tmp ) {
		if ( ! parse_int64_bytes(tmp, image_size_kb, 1024)) {
			push_error(stderr, "'%s' is not valid for Image Size\n", tmp);
			image_size_kb = 0;
		}
		free( tmp );
		if( image_size_kb < 1 ) {
			push_error(stderr, "Image Size must be positive\n" );
			ABORT_AND_RETURN( 1 );
		}
	}

	/* It's reasonable to expect the image size will be at least the
		physical memory requirement, so make sure it is. */
	  
	// At one time there was some stuff to attempt to gleen this from
	// the requirements line, but that caused many problems.
	// Jeff Ballard 11/4/98

	AssignJobVal(ATTR_IMAGE_SIZE, image_size_kb);
	AssignJobVal(ATTR_EXECUTABLE_SIZE, executable_size_kb);

	// set an initial value for memory usage
	//
	tmp = submit_param( SUBMIT_KEY_MemoryUsage, ATTR_MEMORY_USAGE );
	if (tmp) {
		int64_t memory_usage_mb = 0;
		if ( ! parse_int64_bytes(tmp, memory_usage_mb, 1024*1024) ||
			memory_usage_mb < 0) {
			push_error(stderr, "'%s' is not valid for Memory Usage\n", tmp);
			ABORT_AND_RETURN( 1 );
		}
		free(tmp);
		AssignJobVal(ATTR_MEMORY_USAGE, memory_usage_mb);
	}

	// set an initial value for disk usage based on the size of the input sandbox.
	//
	int64_t disk_usage_kb = 0;
	tmp = submit_param( SUBMIT_KEY_DiskUsage, ATTR_DISK_USAGE );
	if( tmp ) {
		if ( ! parse_int64_bytes(tmp, disk_usage_kb, 1024) || disk_usage_kb < 1) {
			push_error(stderr, "'%s' is not valid for disk_usage. It must be >= 1\n", tmp);
			ABORT_AND_RETURN( 1 );
		}
		free( tmp );
	} else {
		disk_usage_kb = exe_disk_size_kb + TransferInputSizeKb;
	}
	AssignJobVal(ATTR_DISK_USAGE, disk_usage_kb);

	AssignJobVal(ATTR_TRANSFER_INPUT_SIZE_MB, (executable_size_kb + TransferInputSizeKb)/1024);

	// set an intial value for RequestMemory
	tmp = submit_param(SUBMIT_KEY_RequestMemory, ATTR_REQUEST_MEMORY);
	if (tmp) {
		// if input is an integer followed by K,M,G or T, scale it MB and 
		// insert it into the jobAd, otherwise assume it is an expression
		// and insert it as text into the jobAd.
		int64_t req_memory_mb = 0;
		if (parse_int64_bytes(tmp, req_memory_mb, 1024*1024)) {
			buffer.formatstr("%s = %" PRId64, ATTR_REQUEST_MEMORY, req_memory_mb);
			RequestMemoryIsZero = (req_memory_mb == 0);
		} else if (MATCH == strcasecmp(tmp,"undefined")) {
			RequestMemoryIsZero = true;
		} else {
			buffer.formatstr("%s = %s", ATTR_REQUEST_MEMORY, tmp);
		}
		free(tmp);
		InsertJobExpr(buffer);
	} else if ( (tmp = submit_param(SUBMIT_KEY_VM_Memory)) || (tmp = submit_param(ATTR_JOB_VM_MEMORY)) ) {
		push_warning(stderr, "'%s' was NOT specified.  Using %s = %s. \n", ATTR_REQUEST_MEMORY,ATTR_JOB_VM_MEMORY, tmp );
		buffer.formatstr("%s = MY.%s", ATTR_REQUEST_MEMORY, ATTR_JOB_VM_MEMORY);
		free(tmp);
		InsertJobExpr(buffer);
	} else if ( (tmp = param("JOB_DEFAULT_REQUESTMEMORY")) ) {
		if (MATCH == strcasecmp(tmp,"undefined")) {
			RequestMemoryIsZero = true;
		} else {
			buffer.formatstr("%s = %s", ATTR_REQUEST_MEMORY, tmp);
			RequestMemoryIsZero = (MATCH == strcmp(tmp, "0"));
			InsertJobExpr(buffer);
		}
		free(tmp);
	}

	// set an initial value for RequestDisk
	if ((tmp = submit_param(SUBMIT_KEY_RequestDisk, ATTR_REQUEST_DISK))) {
		// if input is an integer followed by K,M,G or T, scale it MB and 
		// insert it into the jobAd, otherwise assume it is an expression
		// and insert it as text into the jobAd.
		int64_t req_disk_kb = 0;
		if (parse_int64_bytes(tmp, req_disk_kb, 1024)) {
			buffer.formatstr("%s = %" PRId64, ATTR_REQUEST_DISK, req_disk_kb);
			RequestDiskIsZero = (req_disk_kb == 0);
		} else if (MATCH == strcasecmp(tmp,"undefined")) {
			RequestDiskIsZero = true;
		} else {
			buffer.formatstr("%s = %s", ATTR_REQUEST_DISK, tmp);
		}
		free(tmp);
		InsertJobExpr(buffer);
	} else if ( (tmp = param("JOB_DEFAULT_REQUESTDISK")) ) {
		if (MATCH == strcasecmp(tmp,"undefined")) {
			RequestDiskIsZero = true;
		} else {
			buffer.formatstr("%s = %s", ATTR_REQUEST_DISK, tmp);
			RequestDiskIsZero = (MATCH == strcmp(tmp, "0"));
			InsertJobExpr(buffer);
		}
		free(tmp);
	}
	return 0;
}

int SubmitHash::SetFileOptions()
{
	RETURN_IF_ABORT();
	char *tmp;
	MyString strbuffer;

	tmp = submit_param( SUBMIT_KEY_FileRemaps, ATTR_FILE_REMAPS );
	if(tmp) {
		strbuffer.formatstr("%s = %s",ATTR_FILE_REMAPS,tmp);
		InsertJobExpr(strbuffer);
		free(tmp);
	}

	tmp = submit_param( SUBMIT_KEY_BufferFiles, ATTR_BUFFER_FILES );
	if(tmp) {
		strbuffer.formatstr("%s = %s",ATTR_BUFFER_FILES,tmp);
		InsertJobExpr(strbuffer);
		free(tmp);
	}

	/* If no buffer size is given, use 512 KB */

	tmp = submit_param( SUBMIT_KEY_BufferSize, ATTR_BUFFER_SIZE );
	if(!tmp) {
		tmp = param("DEFAULT_IO_BUFFER_SIZE");
		if (!tmp) {
			tmp = strdup("524288");
		}
	}
	strbuffer.formatstr("%s = %s",ATTR_BUFFER_SIZE,tmp);
	InsertJobExpr(strbuffer);
	free(tmp);

	/* If not buffer block size is given, use 32 KB */

	tmp = submit_param( SUBMIT_KEY_BufferBlockSize, ATTR_BUFFER_BLOCK_SIZE );
	if(!tmp) {
		tmp = param("DEFAULT_IO_BUFFER_BLOCK_SIZE");
		if (!tmp) {
			tmp = strdup("32768");
		}
	}
	strbuffer.formatstr("%s = %s",ATTR_BUFFER_BLOCK_SIZE,tmp);
	InsertJobExpr(strbuffer.Value());
	free(tmp);
	return 0;
}


int SubmitHash::SetRequestResources()
{
    RETURN_IF_ABORT();

    HASHITER it = hash_iter_begin(SubmitMacroSet);
    for (;  !hash_iter_done(it);  hash_iter_next(it)) {
        const char * key = hash_iter_key(it);
        // if key is not of form "request_xxx", ignore it:
        if ( ! starts_with_ignore_case(key, SUBMIT_KEY_RequestPrefix)) continue;
        // if key is one of the predefined request_cpus, request_memory, etc, also ignore it,
        // those have their own special handling:
        if (is_required_request_resource(key)) continue;
        const char * rname = key + strlen(SUBMIT_KEY_RequestPrefix);
        // resource name should be nonempty
        if ( ! *rname) continue;
        // could get this from 'it', but this prevents unused-line warnings:
        char * val = submit_param(key);
        std::string assign;
        formatstr(assign, "%s%s = %s", ATTR_REQUEST_PREFIX, rname, val);
        
        if (val[0]=='\"')
        {
            stringReqRes.insert(rname);
        }
        
        InsertJobExpr(assign.c_str()); 
        RETURN_IF_ABORT();
    }
    hash_iter_delete(&it);
    return 0;
}

// generate effective requirements expression by merging default requirements
// with the specified requirements.
//
bool SubmitHash::check_requirements( char const *orig, MyString &answer )
{
	bool	checks_opsys = false;
	bool	checks_arch = false;
	bool	checks_disk = false;
	bool	checks_cpus = false;
	bool	checks_mem = false;
	//bool	checks_reqmem = false;
	bool	checks_fsdomain = false;
	bool	checks_ckpt_arch = false;
	bool	checks_file_transfer = false;
	bool	checks_file_transfer_plugin_methods = false;
	bool	checks_per_file_encryption = false;
	bool	checks_mpi = false;
	bool	checks_tdp = false;
	bool	checks_encrypt_exec_dir = false;
#if defined(WIN32)
	bool	checks_credd = false;
#endif
	char	*ptr;
	MyString ft_clause;

	if( strlen(orig) ) {
		answer.formatstr( "(%s)", orig );
	} else {
		answer = "";
	}

	switch( JobUniverse ) {
	case CONDOR_UNIVERSE_VANILLA:
		ptr = param( "APPEND_REQ_VANILLA" );
		break;
	case CONDOR_UNIVERSE_STANDARD:
		ptr = param( "APPEND_REQ_STANDARD" );
		break;
	case CONDOR_UNIVERSE_VM:
		ptr = param( "APPEND_REQ_VM" );
		break;
	default:
		ptr = NULL;
		break;
	} 
	if( ptr == NULL ) {
			// Didn't find a per-universe version, try the generic,
			// non-universe specific one:
		ptr = param( "APPEND_REQUIREMENTS" );
	}

	if( ptr != NULL ) {
			// We found something to append.  
		if( answer.Length() ) {
				// We've already got something in requirements, so we
				// need to append an AND clause.
			answer += " && (";
		} else {
				// This is the first thing in requirements, so just
				// put this as the first clause.
			answer += "(";
		}
		answer += ptr;
		answer += ")";
		free( ptr );
	}

	if ( JobUniverse == CONDOR_UNIVERSE_GRID ) {
		// We don't want any defaults at all w/ Globus...
		// If we don't have a req yet, set to TRUE
		if ( answer[0] == '\0' ) {
			answer = "TRUE";
		}
		return true;
	}

	ClassAd req_ad;
	StringList job_refs;      // job attrs referenced by requirements
	StringList machine_refs;  // machine attrs referenced by requirements

		// Insert dummy values for attributes of the job to which we
		// want to detect references.  Otherwise, unqualified references
		// get classified as external references.
	req_ad.Assign(ATTR_REQUEST_MEMORY,0);
	req_ad.Assign(ATTR_CKPT_ARCH,"");

	req_ad.GetExprReferences(answer.Value(),&job_refs,&machine_refs);

	checks_arch = IsDockerJob || machine_refs.contains_anycase( ATTR_ARCH );
	checks_opsys = IsDockerJob || machine_refs.contains_anycase( ATTR_OPSYS ) ||
		machine_refs.contains_anycase( ATTR_OPSYS_AND_VER ) ||
		machine_refs.contains_anycase( ATTR_OPSYS_LONG_NAME ) ||
		machine_refs.contains_anycase( ATTR_OPSYS_SHORT_NAME ) ||
		machine_refs.contains_anycase( ATTR_OPSYS_NAME ) ||
		machine_refs.contains_anycase( ATTR_OPSYS_LEGACY );
	checks_disk =  machine_refs.contains_anycase( ATTR_DISK );
	checks_cpus =   machine_refs.contains_anycase( ATTR_CPUS );
	checks_tdp =  machine_refs.contains_anycase( ATTR_HAS_TDP );
	checks_encrypt_exec_dir = machine_refs.contains_anycase( ATTR_ENCRYPT_EXECUTE_DIRECTORY );
#if defined(WIN32)
	checks_credd = machine_refs.contains_anycase( ATTR_LOCAL_CREDD );
#endif

	if( JobUniverse == CONDOR_UNIVERSE_STANDARD ) {
		checks_ckpt_arch = job_refs.contains_anycase( ATTR_CKPT_ARCH );
	}
	if( JobUniverse == CONDOR_UNIVERSE_MPI ) {
		checks_mpi = machine_refs.contains_anycase( ATTR_HAS_MPI );
	}
	if( mightTransfer(JobUniverse) ) { 
		switch( should_transfer ) {
		case STF_IF_NEEDED:
		case STF_NO:
			checks_fsdomain = machine_refs.contains_anycase(
										  ATTR_FILE_SYSTEM_DOMAIN ); 
			break;
		case STF_YES:
			checks_file_transfer = machine_refs.contains_anycase(
											   ATTR_HAS_FILE_TRANSFER );
			checks_file_transfer_plugin_methods = machine_refs.contains_anycase(
											   ATTR_HAS_FILE_TRANSFER_PLUGIN_METHODS );
			checks_per_file_encryption = machine_refs.contains_anycase(
										   ATTR_HAS_PER_FILE_ENCRYPTION );
			break;
		}
	}

	checks_mem = machine_refs.contains_anycase(ATTR_MEMORY);
	//checks_reqmem = job_refs.contains_anycase(ATTR_REQUEST_MEMORY);

	if( JobUniverse == CONDOR_UNIVERSE_JAVA ) {
		if( answer[0] ) {
			answer += " && ";
		}
		answer += "TARGET." ATTR_HAS_JAVA;
	} else if ( JobUniverse == CONDOR_UNIVERSE_VM ) {
		// For vm universe, we require the same archicture.
		if( !checks_arch ) {
			if( answer[0] ) {
				answer += " && ";
			}
			answer += "(TARGET.Arch == \"";
			answer += ArchMacroDef.psz;
			answer += "\")";
		}
		// add HasVM to requirements
		bool checks_vm = false;
		checks_vm = machine_refs.contains_anycase( ATTR_HAS_VM );
		if( !checks_vm ) {
			answer += "&& (TARGET.";
			answer += ATTR_HAS_VM;
			answer += " =?= true)";
		}
		// add vm_type to requirements
		bool checks_vmtype = false;
		checks_vmtype = machine_refs.contains_anycase( ATTR_VM_TYPE);
		if( !checks_vmtype ) {
			answer += " && (TARGET.";
			answer += ATTR_VM_TYPE;
			answer += " == \"";
			answer += VMType.Value();
			answer += "\")";
		}
		// check if the number of executable VM is more than 0
		bool checks_avail = false;
		checks_avail = machine_refs.contains_anycase(ATTR_VM_AVAIL_NUM);
		if( !checks_avail ) {
			answer += " && (TARGET.";
			answer += ATTR_VM_AVAIL_NUM;
			answer += " > 0)";
		}
	} else if (IsDockerJob) {
			if( answer[0] ) {
				answer += " && ";
			}
			answer += "TARGET.HasDocker";
	} else {
		if( !checks_arch ) {
			if( answer[0] ) {
				answer += " && ";
			}
			answer += "(TARGET.Arch == \"";
			answer += ArchMacroDef.psz;
			answer += "\")";
		}

		if( !checks_opsys ) {
			answer += " && (TARGET.OpSys == \"";
			answer += OpsysMacroDef.psz;
			answer += "\")";
		}
	}

	if ( JobUniverse == CONDOR_UNIVERSE_STANDARD && !checks_ckpt_arch ) {
		answer += " && ((CkptArch == TARGET.Arch) ||";
		answer += " (CkptArch =?= UNDEFINED))";
		answer += " && ((CkptOpSys == TARGET.OpSys) ||";
		answer += "(CkptOpSys =?= UNDEFINED))";
	}

	if( !checks_disk ) {
		if (job->Lookup(ATTR_REQUEST_DISK)) {
			if ( ! RequestDiskIsZero) {
				answer += " && (TARGET.Disk >= RequestDisk)";
			}
		}
		else if ( JobUniverse == CONDOR_UNIVERSE_VM ) {
			// VM universe uses Total Disk 
			// instead of Disk for Condor slot
			answer += " && (TARGET.TotalDisk >= DiskUsage)";
		}else {
			answer += " && (TARGET.Disk >= DiskUsage)";
		}
	} else {
		if (JobUniverse != CONDOR_UNIVERSE_VM) {
			if ( ! RequestDiskIsZero && job->Lookup(ATTR_REQUEST_DISK)) {
				answer += " && (TARGET.Disk >= RequestDisk)";
			}
			if ( ! already_warned_requirements_disk && param_boolean("ENABLE_DEPRECATION_WARNINGS", false)) {
				push_warning(stderr,
						"Your Requirements expression refers to TARGET.Disk. "
						"This is obsolete. Set request_disk and condor_submit will modify the "
						"Requirements expression as needed.\n");
				already_warned_requirements_disk = true;
			}
		}
	}

	if ( JobUniverse != CONDOR_UNIVERSE_VM ) {
		// The memory requirement for VM universe will be 
		// added in SetVMRequirements 
#if 1
		if ( ! RequestMemoryIsZero && job->Lookup(ATTR_REQUEST_MEMORY)) {
			answer += " && (TARGET.Memory >= RequestMemory)";
		}
		if (checks_mem) {
			if ( ! already_warned_requirements_mem && param_boolean("ENABLE_DEPRECATION_WARNINGS", false)) {
				push_warning(stderr,
						"your Requirements expression refers to TARGET.Memory. "
						"This is obsolete. Set request_memory and condor_submit will modify the "
						"Requirements expression as needed.\n");
				already_warned_requirements_mem = true;
			}
		}
#else
		if ( !checks_mem ) {
			answer += " && ( (TARGET.Memory * 1024) >= ImageSize ) ";
		}
		if ( !checks_reqmem ) {
			answer += " && ( ( RequestMemory * 1024 ) >= ImageSize ) ";
		}
#endif
	}

	if ( JobUniverse != CONDOR_UNIVERSE_GRID ) {
		if ( ! checks_cpus && ! RequestCpusIsZeroOrOne && job->Lookup(ATTR_REQUEST_CPUS)) {
			answer += " && (TARGET.Cpus >= RequestCpus)";
		}
	}

    // identify any custom pslot resource reqs and add them in:
    HASHITER it = hash_iter_begin(SubmitMacroSet);
    for (;  !hash_iter_done(it);  hash_iter_next(it)) {
        const char * key = hash_iter_key(it);
        // if key is not of form "request_xxx", ignore it:
        if ( ! starts_with_ignore_case(key, SUBMIT_KEY_RequestPrefix)) continue;
        // if key is one of the predefined request_cpus, request_memory, etc, also ignore it,
        // those have their own special handling:
        if (is_required_request_resource(key)) continue;
        const char * rname = key + strlen(SUBMIT_KEY_RequestPrefix);
        // resource name should be nonempty
        if ( ! *rname) continue;
        std::string clause;
        if (stringReqRes.count(rname) > 0)
            formatstr(clause, " && regexp(%s%s, TARGET.%s)", ATTR_REQUEST_PREFIX, rname, rname);
        else
            formatstr(clause, " && (TARGET.%s%s >= %s%s)", "", rname, ATTR_REQUEST_PREFIX, rname);
        answer += clause;
    }
    hash_iter_delete(&it);

	if( HasTDP && !checks_tdp ) {
		answer += " && (TARGET.";
		answer += ATTR_HAS_TDP;
		answer += ")";
	}

	if( HasEncryptExecuteDir && !checks_encrypt_exec_dir ) {
		answer += " && (TARGET.";
		answer += ATTR_HAS_ENCRYPT_EXECUTE_DIRECTORY;
		answer += ")";
	}

	if( JobUniverse == CONDOR_UNIVERSE_MPI ) {
		if( ! checks_mpi ) {
			answer += " && (TARGET.";
			answer += ATTR_HAS_MPI;
			answer += ")";
		}
	}


	if( mightTransfer(JobUniverse) ) {
			/* 
			   This is a kind of job that might be using file transfer
			   or a shared filesystem.  so, tack on the appropriate
			   clause to make sure we're either at a machine that
			   supports file transfer, or that we're in the same file
			   system domain.
			*/

		switch( should_transfer ) {
		case STF_NO:
				// no file transfer used.  if there's nothing about
				// the FileSystemDomain yet, tack on a clause for
				// that. 
			if( ! checks_fsdomain ) {
				answer += " && (TARGET.";
				answer += ATTR_FILE_SYSTEM_DOMAIN;
				answer += " == MY.";
				answer += ATTR_FILE_SYSTEM_DOMAIN;
				answer += ")";
			} 
			break;
			
		case STF_YES:
				// we're definitely going to use file transfer.  
			if( ! checks_file_transfer ) {
				answer += " && (TARGET.";
				answer += ATTR_HAS_FILE_TRANSFER;
				if (!checks_per_file_encryption && NeedsPerFileEncryption) {
					answer += " && TARGET.";
					answer += ATTR_HAS_PER_FILE_ENCRYPTION;
				}

				if( (!checks_file_transfer_plugin_methods) ) {
					// check input
					char* file_list = submit_param( SUBMIT_KEY_TransferInputFiles, "TransferInputFiles" );
					char* tmp_ptr;
					if(file_list) {
						StringList files(file_list, ",");
						files.rewind();
						while ( (tmp_ptr=files.next()) ) {
							if (IsUrl(tmp_ptr)){
								MyString plugintype = getURLType(tmp_ptr);
								answer += " && stringListMember(\"";
								answer += plugintype;
								answer += "\",HasFileTransferPluginMethods)";
							}
						}
						free(file_list);
					}

					// check output
					tmp_ptr = submit_param( SUBMIT_KEY_OutputDestination, "OutputDestination" );
					if (tmp_ptr) {
						if (IsUrl(tmp_ptr)){
							MyString plugintype = getURLType(tmp_ptr);
							answer += " && stringListMember(\"";
							answer += plugintype;
							answer += "\",HasFileTransferPluginMethods)";
						}
						free (tmp_ptr);
					}
				}

				// close of the file transfer requirements
				answer += ")";
			}
			break;
			
		case STF_IF_NEEDED:
				// we may or may not use file transfer, so require
				// either the same FS domain OR the ability to file
				// transfer.  if the user already refered to fs
				// domain, but explictly turned on IF_NEEDED, assume
				// they know what they're doing. 
			if( ! checks_fsdomain ) {
				ft_clause = " && ((TARGET.";
				ft_clause += ATTR_HAS_FILE_TRANSFER;
				if (NeedsPerFileEncryption) {
					ft_clause += " && TARGET.";
					ft_clause += ATTR_HAS_PER_FILE_ENCRYPTION;
				}
				ft_clause += ") || (TARGET.";
				ft_clause += ATTR_FILE_SYSTEM_DOMAIN;
				ft_clause += " == MY.";
				ft_clause += ATTR_FILE_SYSTEM_DOMAIN;
				ft_clause += "))";
				answer += ft_clause.Value();
			}
			break;
		}
	}

		//
		// Job Deferral
		// If the flag was set true by SetJobDeferral() then we will
		// add the requirement in for this feature
		//
	if ( NeedsJobDeferral ) {
			//
			// Add the HasJobDeferral to the attributes so that it will
			// match with a Starter that can actually provide this feature
			// We only need to do this if the job's universe isn't local
			// This is because the schedd doesn't pull out the Starter's
			// machine ad when trying to match local universe jobs
			//
			//
		if ( JobUniverse != CONDOR_UNIVERSE_LOCAL ) {
			answer += " && TARGET." ATTR_HAS_JOB_DEFERRAL;
		}
		
			//
			//
			// Prepare our new requirement attribute
			// This nifty expression will evaluate to true when
			// the job's prep start time is before the next schedd
			// polling interval. We also have a nice clause to prevent
			// our job from being matched and executed after it could
			// possibly run
			//
			//	( ( time() + ATTR_SCHEDD_INTERVAL ) >= 
			//	  ( ATTR_DEFERRAL_TIME - ATTR_DEFERRAL_PREP_TIME ) )
			//  &&
			//    ( time() < 
			//    ( ATTR_DEFFERAL_TIME + ATTR_DEFERRAL_WINDOW ) )
			//  )
			//
		MyString attrib;
		attrib.formatstr( "( ( time() + %s ) >= ( %s - %s ) ) && "\
						"( time() < ( %s + %s ) )",
						ATTR_SCHEDD_INTERVAL,
						ATTR_DEFERRAL_TIME,
						ATTR_DEFERRAL_PREP_TIME,
						ATTR_DEFERRAL_TIME,
						ATTR_DEFERRAL_WINDOW );
		answer += " && (";
		answer += attrib.Value();
		answer += ")";			
	} // !seenBefore

#if defined(WIN32)
		//
		// Windows CredD
		// run_as_owner jobs on Windows require that the remote starter can
		// reach the same credd as the submit machine. We add the following:
		//   - HasWindowsRunAsOwner
		//   - LocalCredd == <CREDD_HOST> (if LocalCredd not found)
		//
	if ( RunAsOwnerCredD ) {
		MyString tmp_rao = " && (TARGET.";
		tmp_rao += ATTR_HAS_WIN_RUN_AS_OWNER;
		if (!checks_credd) {
			tmp_rao += " && (TARGET.";
			tmp_rao += ATTR_LOCAL_CREDD;
			tmp_rao += " =?= \"";
			tmp_rao += RunAsOwnerCredD.ptr();
			tmp_rao += "\")";
		}
		tmp_rao += ")";
		answer += tmp_rao.Value();
	}
#endif

	/* if the user specified they want this feature, add it to the requirements */
	return true;
}

// this function should be called after SetVMParams() and SetRequirements
int SubmitHash::SetVMRequirements(bool VMCheckpoint, bool VMNetworking, MyString &VMNetworkType, bool VMHardwareVT, bool vm_need_fsdomain)
{
	RETURN_IF_ABORT();
	MyString buffer;
	if ( JobUniverse != CONDOR_UNIVERSE_VM ) {
		return 0;
	}


	MyString vmanswer;
	vmanswer = "(";
	vmanswer += JobRequirements;
	vmanswer += ")";

	ClassAd req_ad;
	StringList job_refs;      // job attrs referenced by requirements
	StringList machine_refs;  // machine attrs referenced by requirements

		// Insert dummy values for attributes of the job to which we
		// want to detect references.  Otherwise, unqualified references
		// get classified as external references.
	req_ad.Assign(ATTR_CKPT_ARCH,"");
	req_ad.Assign(ATTR_VM_CKPT_MAC,"");

	req_ad.GetExprReferences(vmanswer.Value(),&job_refs,&machine_refs);

	// check file system domain
	if( vm_need_fsdomain ) {
		// some files don't use file transfer.
		// so we need the same file system domain
		bool checks_fsdomain = false;
		checks_fsdomain = machine_refs.contains_anycase( ATTR_FILE_SYSTEM_DOMAIN ); 

		if( !checks_fsdomain ) {
			vmanswer += " && (TARGET.";
			vmanswer += ATTR_FILE_SYSTEM_DOMAIN;
			vmanswer += " == MY.";
			vmanswer += ATTR_FILE_SYSTEM_DOMAIN;
			vmanswer += ")";
		}
		MyString my_fsdomain;
		if(job->LookupString(ATTR_FILE_SYSTEM_DOMAIN, my_fsdomain) != 1) {
			param(my_fsdomain, "FILESYSTEM_DOMAIN" );
			buffer.formatstr( "%s = \"%s\"", ATTR_FILE_SYSTEM_DOMAIN, my_fsdomain.c_str() ); 
			InsertJobExpr( buffer );
			RETURN_IF_ABORT();
		}
	}

	if( strcasecmp(VMType.Value(), CONDOR_VM_UNIVERSE_XEN) != MATCH ) {
		// For most virtual machine programs except Xen, 
		// it's reasonable to expect the physical memory is 
		// larger than the memory for VM.
		// Especially VM universe uses Total Memory 
		// instead of Memory for Condor slot.
		// The reason why we use Total Memory is that 
		// we may want to run smaller VM universe jobs than the number of 
		// Condor slots.
		// The maximum number of VM universe jobs on a execute machine is 
		// the number of ATTR_VM_AVAIL_NUM.
		// Generally ATTR_VM_AVAIL_NUM must be less than the number of 
		// Condor slot.
		vmanswer += " && (TARGET.";
		vmanswer += ATTR_TOTAL_MEMORY;
		vmanswer += " >= MY.";
		vmanswer += ATTR_JOB_VM_MEMORY;
		vmanswer += ")";
	}

	// add vm_memory to requirements
	bool checks_vmmemory = false;
	checks_vmmemory = machine_refs.contains_anycase(ATTR_VM_MEMORY);
	if( !checks_vmmemory ) {
		vmanswer += " && (TARGET.";
		vmanswer += ATTR_VM_MEMORY;
		vmanswer += " >= MY.";
		vmanswer += ATTR_JOB_VM_MEMORY;
		vmanswer += ")";
	}

	if( VMHardwareVT ) {
		bool checks_hardware_vt = false;
		checks_hardware_vt = machine_refs.contains_anycase(ATTR_VM_HARDWARE_VT);

		if( !checks_hardware_vt ) {
			// add hardware vt to requirements
			vmanswer += " && (TARGET.";
			vmanswer += ATTR_VM_HARDWARE_VT;
			vmanswer += ")";
		}
	}

	if( VMNetworking ) {
		bool checks_vmnetworking = false;
		checks_vmnetworking = machine_refs.contains_anycase(ATTR_VM_NETWORKING);

		if( !checks_vmnetworking ) {
			// add vm_networking to requirements
			vmanswer += " && (TARGET.";
			vmanswer += ATTR_VM_NETWORKING;
			vmanswer += ")";
		}

		if(	VMNetworkType.IsEmpty() == false ) {
			// add vm_networking_type to requirements
			vmanswer += " && ( stringListIMember(\"";
			vmanswer += VMNetworkType.Value();
			vmanswer += "\",";
			vmanswer += "TARGET.";
			vmanswer += ATTR_VM_NETWORKING_TYPES;
			vmanswer += ",\",\")) ";
		}
	}
			
	if( VMCheckpoint ) {
		bool checks_ckpt_arch = false;
		bool checks_vm_ckpt_mac = false;
		checks_ckpt_arch = job_refs.contains_anycase( ATTR_CKPT_ARCH );
		checks_vm_ckpt_mac = job_refs.contains_anycase( ATTR_VM_CKPT_MAC );

		if( !checks_ckpt_arch ) {
			// VM checkpoint files created on AMD 
			// can not be used in INTEL. vice versa.
			vmanswer += " && ((MY.CkptArch == Arch) ||";
			vmanswer += " (MY.CkptArch =?= UNDEFINED))";
		}
		if( !checks_vm_ckpt_mac ) { 
			// VMs with the same MAC address cannot run 
			// on the same execute machine
			vmanswer += " && ((MY.VM_CkptMac =?= UNDEFINED) || ";
			vmanswer += "(TARGET.VM_All_Guest_Macs =?= UNDEFINED) || ";
			vmanswer += "( stringListIMember(MY.VM_CkptMac, ";
			vmanswer += "TARGET.VM_All_Guest_Macs, \",\") == FALSE )) ";
		}
	}

	buffer.formatstr( "%s = %s", ATTR_REQUIREMENTS, vmanswer.Value());
	JobRequirements = vmanswer;
	InsertJobExpr (buffer);
	RETURN_IF_ABORT();
	return 0;
}

int SubmitHash::SetConcurrencyLimits()
{
	RETURN_IF_ABORT();
	MyString tmp = submit_param_mystring(SUBMIT_KEY_ConcurrencyLimits, NULL);
	MyString tmp2 = submit_param_mystring(SUBMIT_KEY_ConcurrencyLimitsExpr, NULL);

	if (!tmp.IsEmpty()) {
		if (!tmp2.IsEmpty()) {
			push_error( stderr, "%s and %s can't be used together\n",
					 SUBMIT_KEY_ConcurrencyLimits, SUBMIT_KEY_ConcurrencyLimitsExpr );
			ABORT_AND_RETURN( 1 );
		}
		char *str;

		tmp.lower_case();

		StringList list(tmp.Value());

		char *limit;
		list.rewind();
		while ( (limit = list.next()) ) {
			double increment;
			char *limit_cpy = strdup( limit );

			if ( !ParseConcurrencyLimit(limit_cpy, increment) ) {
				push_error(stderr, "Invalid concurrency limit '%s'\n",
						 limit );
				ABORT_AND_RETURN( 1 );
			}
			free( limit_cpy );
		}

		list.qsort();

		str = list.print_to_string();
		if ( str ) {
			tmp.formatstr("%s = \"%s\"", ATTR_CONCURRENCY_LIMITS, str);
			InsertJobExpr(tmp.Value());

			free(str);
		}
	} else if (!tmp2.IsEmpty()) {
		std::string expr;
		formatstr( expr, "%s = %s", ATTR_CONCURRENCY_LIMITS, tmp2.Value() );
		InsertJobExpr( expr.c_str() );
	}

	return 0;
}


int SubmitHash::SetAccountingGroup()
{
	RETURN_IF_ABORT();

    // is a group setting in effect?
    char* group = submit_param(SUBMIT_KEY_AcctGroup, ATTR_ACCOUNTING_GROUP);

    // look for the group user setting, or default to owner
    std::string group_user;
    char* gu = submit_param(SUBMIT_KEY_AcctGroupUser, ATTR_ACCT_GROUP_USER);
    if ((group == NULL) && (gu == NULL)) {
        return 0; // nothing set, give up
    }

    if (NULL == gu) {
        group_user = submit_owner.Value();
    } else {
        group_user = gu;
        free(gu);
    }

	if ( group && !IsValidSubmitterName(group) ) {
		push_error(stderr, "Invalid %s: %s\n", SUBMIT_KEY_AcctGroup, group);
		ABORT_AND_RETURN( 1 );
	}
	if ( !IsValidSubmitterName(group_user.c_str()) ) {
		push_error(stderr, "Invalid %s: %s\n", SUBMIT_KEY_AcctGroupUser, group_user.c_str());
		ABORT_AND_RETURN( 1 );
	}

    // set attributes AcctGroup, AcctGroupUser and AccountingGroup on the job ad:
    MyString buffer;

    if (group) {
        // If we have a group, must also specify user
        buffer.formatstr("%s = \"%s.%s\"", ATTR_ACCOUNTING_GROUP, group, group_user.c_str()); 
    } else {
        // If not, this is accounting group as user alias, just set name
        buffer.formatstr("%s = \"%s\"", ATTR_ACCOUNTING_GROUP, group_user.c_str()); 
    }
    InsertJobExpr(buffer.c_str());

	if (group) {
        buffer.formatstr("%s = \"%s\"", ATTR_ACCT_GROUP, group);
        InsertJobExpr(buffer.c_str());
    }

    buffer.formatstr("%s = \"%s\"", ATTR_ACCT_GROUP_USER, group_user.c_str());
    InsertJobExpr(buffer.c_str());

    if (group) free(group);
	return 0;
}

// add a vm file to transfer_input_files
void SubmitHash::transfer_vm_file(const char *filename, long long & accumulate_size_kb)
{
	MyString fixedname;
	MyString buffer;

	if( !filename ) {
		return;
	}

	fixedname = delete_quotation_marks(filename);

	StringList transfer_file_list(NULL, ",");
	MyString transfer_input_files;

	// check whether the file is in transfer_input_files
	if( job->LookupString(ATTR_TRANSFER_INPUT_FILES,transfer_input_files) 
				== 1 ) {
		transfer_file_list.initializeFromString(transfer_input_files.Value());
		if( filelist_contains_file(fixedname.Value(), 
					&transfer_file_list, true) ) {
			// this file is already in transfer_input_files
			return;
		}
	}

	// we need to add it
	char *tmp_ptr = NULL;
	check_and_universalize_path(fixedname);
	check_open(SFR_VM_INPUT, fixedname.Value(), O_RDONLY);
	accumulate_size_kb += calc_image_size_kb(fixedname.Value());

	transfer_file_list.append(fixedname.Value());
	tmp_ptr = transfer_file_list.print_to_string();

	buffer.formatstr( "%s = \"%s\"", ATTR_TRANSFER_INPUT_FILES, tmp_ptr);
	InsertJobExpr(buffer);
	free(tmp_ptr);

	SetImageSize();
}



// this function must be called after SetUniverse
int SubmitHash::SetVMParams()
{
	RETURN_IF_ABORT();

	if ( JobUniverse != CONDOR_UNIVERSE_VM ) {
		return 0;
	}

	bool VMCheckpoint = false;
	bool VMNetworking = false;
	long long VMInputSizeKb = 0;
	int VMVCPUS = 0;
	bool VMVNC=false;
	MyString VMNetworkType;
	bool VMHardwareVT = false;
	bool vm_need_fsdomain = false;

	char* tmp_ptr = NULL;
	MyString buffer;

	// by the time we get here, we have already verified that vmtype is non-empty
	VMType = submit_param_mystring(SUBMIT_KEY_VM_Type, ATTR_JOB_VM_TYPE);
	VMType.lower_case();

	// VM type is already set in SetUniverse
	buffer.formatstr( "%s = \"%s\"", ATTR_JOB_VM_TYPE, VMType.Value());
	InsertJobExpr(buffer);
	RETURN_IF_ABORT();

	// need vm checkpoint?
	VMCheckpoint = submit_param_bool(SUBMIT_KEY_VM_Checkpoint, ATTR_JOB_VM_CHECKPOINT, false);
	AssignJobVal(ATTR_JOB_VM_CHECKPOINT, VMCheckpoint);

	VMNetworking = submit_param_bool(SUBMIT_KEY_VM_Networking, ATTR_JOB_VM_NETWORKING, false);
	AssignJobVal(ATTR_JOB_VM_NETWORKING, VMNetworking);

	VMVNC = submit_param_bool(SUBMIT_KEY_VM_VNC, ATTR_JOB_VM_VNC, false);
	AssignJobVal(ATTR_JOB_VM_VNC, VMVNC);
	
	// Here we need to set networking type
	if( VMNetworking ) {
		VMNetworkType = submit_param_mystring(SUBMIT_KEY_VM_Networking_Type, ATTR_JOB_VM_NETWORKING_TYPE);
		if ( ! VMNetworkType.empty() ) {
			buffer.formatstr( "%s = \"%s\"", ATTR_JOB_VM_NETWORKING_TYPE,  VMNetworkType.Value());
			InsertJobExpr(buffer);
		} else {
			VMNetworkType = "";
		}
	}

	// Set memory for virtual machine
	tmp_ptr = submit_param(SUBMIT_KEY_VM_Memory);
	if( !tmp_ptr ) {
		tmp_ptr = submit_param(ATTR_JOB_VM_MEMORY);
	}
	if( !tmp_ptr ) {
		push_error(stderr, "'%s' cannot be found.\n"
				"Please specify '%s' for vm universe "
				"in your submit description file.\n", SUBMIT_KEY_VM_Memory, SUBMIT_KEY_VM_Memory);
		ABORT_AND_RETURN(1);
	}

	int64_t vm_mem;
	parse_int64_bytes(tmp_ptr, vm_mem, 1024*1024);
	if( vm_mem <= 0 ) {
		push_error(stderr, "'%s' is incorrectly specified\n"
				"For example, for vm memroy of 128 Megabytes,\n"
				"you need to use 128 in your submit description file.\n",
				SUBMIT_KEY_VM_Memory);
		ABORT_AND_RETURN(1);
	}
	AssignJobVal(ATTR_JOB_VM_MEMORY, vm_mem);
	ExecutableSizeKb = vm_mem * 1024; // we will need this for ImageSize

	/* 
	 * Set the number of VCPUs for this virtual machine
	 */
	tmp_ptr = submit_param(SUBMIT_KEY_VM_VCPUS, ATTR_JOB_VM_VCPUS);
	if(tmp_ptr)
	  {
	    VMVCPUS = (int)strtol(tmp_ptr, (char**)NULL,10);
	    dprintf(D_FULLDEBUG, "VCPUS = %s", tmp_ptr);
	    free(tmp_ptr);
	  }
	if(VMVCPUS <= 0)
	  {
	    VMVCPUS = 1;
	  }
	buffer.formatstr("%s = %d", ATTR_JOB_VM_VCPUS, VMVCPUS);
	InsertJobExpr(buffer);

	/*
	 * Set the MAC address for this VM.
	 */
	tmp_ptr = submit_param(SUBMIT_KEY_VM_MACAddr, ATTR_JOB_VM_MACADDR);
	if(tmp_ptr)
	  {
	    buffer.formatstr("%s = \"%s\"", ATTR_JOB_VM_MACADDR, tmp_ptr);
	    InsertJobExpr(buffer);
	  }

	/* 
	 * When the parameter of "vm_no_output_vm" is TRUE, 
	 * Condor will not transfer VM files back to a job user. 
	 * This parameter could be used if a job user uses 
	 * explict method to get output files from VM. 
	 * For example, if a job user uses a ftp program 
	 * to send output files inside VM to his/her dedicated machine for ouput, 
	 * Maybe the job user doesn't want to get modified VM files back. 
	 * So the job user can use this parameter
	 */
	bool vm_no_output_vm = submit_param_bool("vm_no_output_vm", NULL, false);
	if( vm_no_output_vm ) {
		buffer.formatstr( "%s = TRUE", VMPARAM_NO_OUTPUT_VM);
		InsertJobExpr( buffer );
	}

	if( (strcasecmp(VMType.Value(), CONDOR_VM_UNIVERSE_XEN) == MATCH) ||
		(strcasecmp(VMType.Value(), CONDOR_VM_UNIVERSE_KVM) == MATCH) ) {
		bool real_xen_kernel_file = false;
		bool need_xen_root_device = false;

		if ( strcasecmp(VMType.Value(), CONDOR_VM_UNIVERSE_XEN) == MATCH )
		{
			// xen_kernel is a required parameter
			char *xen_kernel = NULL;
			xen_kernel = submit_param("xen_kernel");
			if( !xen_kernel ) {
				push_error(stderr, "'xen_kernel' cannot be found.\n"
						"Please specify 'xen_kernel' for the xen virtual machine "
						"in your submit description file.\n"
						"xen_kernel must be one of "
						"\"%s\", \"%s\", <file-name>.\n", 
						XEN_KERNEL_INCLUDED, XEN_KERNEL_HW_VT);
				ABORT_AND_RETURN(1);
			}else {
				if ( strcasecmp(xen_kernel, XEN_KERNEL_INCLUDED) == 0) {
					// kernel image is included in a disk image file
					// so we will use bootloader(pygrub etc.) defined 
					// in a vmgahp config file on an excute machine 
					real_xen_kernel_file = false;
					need_xen_root_device = false;
				}else if ( strcasecmp(xen_kernel, XEN_KERNEL_HW_VT) == 0) {
					// A job user want to use an unmodified OS in Xen.
					// so we require hardware virtualization.
					real_xen_kernel_file = false;
					need_xen_root_device = false;
					VMHardwareVT = true;
					buffer.formatstr( "%s = TRUE", ATTR_JOB_VM_HARDWARE_VT);
					InsertJobExpr( buffer );
				}else {
					// real kernel file
					real_xen_kernel_file = true;
					need_xen_root_device = true;
				}
				InsertJobExprString(VMPARAM_XEN_KERNEL, xen_kernel);
				free(xen_kernel);
			}

			
			// xen_initrd is an optional parameter
			char *xen_initrd = NULL;
			xen_initrd = submit_param("xen_initrd");
			if( xen_initrd ) {
				if( !real_xen_kernel_file ) {
					push_error(stderr, "To use xen_initrd, "
							"xen_kernel should be a real kernel file.\n");
					ABORT_AND_RETURN(1);
				}
				InsertJobExprString(VMPARAM_XEN_INITRD, xen_initrd);
				free(xen_initrd);
			}

			if( need_xen_root_device ) {
				char *xen_root = NULL;
				xen_root = submit_param("xen_root");
				if( !xen_root ) {
					push_error(stderr, "'%s' cannot be found.\n"
							"Please specify '%s' for the xen virtual machine "
							"in your submit description file.\n", "xen_root", 
							"xen_root");
					ABORT_AND_RETURN(1);
				}else {
					InsertJobExprString(VMPARAM_XEN_ROOT, xen_root);
					free(xen_root);
				}
			}
		}// xen only params

		// <x>_disk is a required parameter
		char *disk = submit_param("vm_disk");

		if( !disk ) {
			push_error(stderr, "'%s' cannot be found.\n"
					"Please specify '%s' for the virtual machine "
					"in your submit description file.\n", 
					"<vm>_disk", "<vm>_disk");
			ABORT_AND_RETURN(1);
		}else {
			if( validate_disk_param(disk,3,4) == false ) 
            {
				push_error(stderr, "'vm_disk' has incorrect format.\n"
						"The format shoud be like "
						"\"<filename>:<devicename>:<permission>\"\n"
						"e.g.> For single disk: <vm>_disk = filename1:hda1:w\n"
						"      For multiple disks: <vm>_disk = "
						"filename1:hda1:w,filename2:hda2:w\n");
				ABORT_AND_RETURN(1);
			}
			InsertJobExprString( VMPARAM_VM_DISK, disk );
			free(disk);
		}

		if ( strcasecmp(VMType.Value(), CONDOR_VM_UNIVERSE_XEN) == MATCH )
		{
			// xen_kernel_params is a optional parameter
			char *xen_kernel_params = NULL;
			xen_kernel_params = submit_param("xen_kernel_params");
			if( xen_kernel_params ) {
				MyString fixedvalue = delete_quotation_marks(xen_kernel_params);
				InsertJobExprString( VMPARAM_XEN_KERNEL_PARAMS, xen_kernel_params );
				free(xen_kernel_params);
			}
		}

	}else if( strcasecmp(VMType.Value(), CONDOR_VM_UNIVERSE_VMWARE) == MATCH ) {
		bool knob_exists = false;
		bool vmware_should_transfer_files = submit_param_bool("vmware_should_transfer_files", NULL, false, &knob_exists);
		if( knob_exists ) {
			MyString err_msg;
			err_msg = "\nERROR: You must explicitly specify "
				"\"vmware_should_transfer_files\" "
				"in your submit description file. "
				"You need to define either: "
				"\"vmware_should_transfer_files = YES\" or "
				" \"vmware_should_transfer_files = NO\". "
				"If you define \"vmware_should_transfer_files = YES\", " 
				"vmx and vmdk files in the directory of \"vmware_dir\" "
				"will be transfered to an execute machine. "
				"If you define \"vmware_should_transfer_files = NO\", "
				"all files in the directory of \"vmware_dir\" should be "
				"accessible with a shared file system\n";
			print_wrapped_text( err_msg.Value(), stderr );
			ABORT_AND_RETURN(1);
		}

		AssignJobVal(VMPARAM_VMWARE_TRANSFER, vmware_should_transfer_files);

		if( vmware_should_transfer_files == false ) {
			vm_need_fsdomain = true;
		}

		/* In default, vmware vm universe uses snapshot disks.
		 * However, when a user job is so IO-sensitive that 
		 * the user doesn't want to use snapshot, 
		 * the user must use both 
		 * vmware_should_transfer_files = YES and 
		 * vmware_snapshot_disk = FALSE
		 * In vmware_should_transfer_files = NO 
		 * snapshot disks is always used.
		 */
		bool vmware_snapshot_disk = submit_param_bool("vmware_snapshot_disk", NULL, false);
		if ( ! vmware_should_transfer_files && ! vmware_snapshot_disk) {
			MyString err_msg;
			err_msg = "\nERROR: You should not use both "
				"vmware_should_transfer_files = FALSE and "
				"vmware_snapshot_disk = FALSE. "
				"Not using snapshot disk in a shared file system may cause "
				"problems when multiple jobs share the same disk\n";
			print_wrapped_text( err_msg.Value(), stderr );
			ABORT_AND_RETURN( 1 );
		}

		AssignJobVal(VMPARAM_VMWARE_SNAPSHOTDISK, vmware_snapshot_disk);

		// vmware_dir is a directory that includes vmx file and vmdk files.
		char *vmware_dir = NULL;
		vmware_dir = submit_param("vmware_dir");
		if ( vmware_dir ) {
			MyString f_dirname = delete_quotation_marks(vmware_dir);
			free(vmware_dir);

			f_dirname = full_path(f_dirname.Value(), false);
			check_and_universalize_path(f_dirname);

			buffer.formatstr( "%s = \"%s\"", VMPARAM_VMWARE_DIR, f_dirname.Value());
			InsertJobExpr( buffer );

			Directory dir( f_dirname.Value() );
			dir.Rewind();
			while ( dir.Next() ) {
				if ( has_suffix( dir.GetFullPath(), ".vmx" ) ||
					 vmware_should_transfer_files ) {
					// The .vmx file is always transfered.
					transfer_vm_file( dir.GetFullPath(), VMInputSizeKb );
				}
			}
		}

		// Look for .vmx and .vmdk files in transfer_input_files
		StringList vmx_files;
		StringList vmdk_files;
		StringList input_files( NULL, "," );
		MyString input_files_str;
		job->LookupString( ATTR_TRANSFER_INPUT_FILES, input_files_str );
		input_files.initializeFromString( input_files_str.Value() );
		input_files.rewind();
		const char *file;
		while ( (file = input_files.next()) ) {
			if ( has_suffix( file, ".vmx" ) ) {
				vmx_files.append( condor_basename( file ) );
			} else if ( has_suffix( file, ".vmdk" ) ) {
				vmdk_files.append( condor_basename( file ) );
			}
		}

		if ( vmx_files.number() == 0 ) {
			push_error(stderr, "no vmx file for vmware can be found.\n" );
			ABORT_AND_RETURN(1);
		} else if ( vmx_files.number() > 1 ) {
			push_error(stderr, "multiple vmx files exist. "
					 "Only one vmx file should be present.\n" );
			ABORT_AND_RETURN(1);
		} else {
			vmx_files.rewind();
			buffer.formatstr( "%s = \"%s\"", VMPARAM_VMWARE_VMX_FILE,
					condor_basename(vmx_files.next()));
			InsertJobExpr( buffer );
		}

		tmp_ptr = vmdk_files.print_to_string();
		if ( tmp_ptr ) {
			buffer.formatstr( "%s = \"%s\"", VMPARAM_VMWARE_VMDK_FILES, tmp_ptr);
			InsertJobExpr( buffer );
			free( tmp_ptr );
		}
	}

	// Now all VM parameters are set
	// So we need to add necessary VM attributes to Requirements
	return SetVMRequirements(VMCheckpoint, VMNetworking, VMNetworkType, VMHardwareVT, vm_need_fsdomain);
}


int SubmitHash::InsertFileTransAttrs( FileTransferOutput_t when_output )
{
	MyString should = ATTR_SHOULD_TRANSFER_FILES;
	should += " = \"";
	MyString when = ATTR_WHEN_TO_TRANSFER_OUTPUT;
	when += " = \"";
	
	should += getShouldTransferFilesString( should_transfer );
	should += '"';
	if( should_transfer != STF_NO ) {
		if( ! when_output ) {
			push_error(stderr, "InsertFileTransAttrs() called we might transfer "
					   "files but when_output hasn't been set" );
			abort_code = 1;
			return abort_code;
		}
		when += getFileTransferOutputString( when_output );
		when += '"';
	}
	InsertJobExpr( should.Value() );
	if( should_transfer != STF_NO ) {
		InsertJobExpr( when.Value() );
	}
	return abort_code;
}

void SubmitHash::process_input_file_list(StringList * input_list, MyString *input_files, bool * files_specified, long long & accumulate_size_kb)
{
	int count;
	MyString tmp;
	char* tmp_ptr;

	if( ! input_list->isEmpty() ) {
		input_list->rewind();
		count = 0;
		while ( (tmp_ptr=input_list->next()) ) {
			count++;
			tmp = tmp_ptr;
			if ( check_and_universalize_path(tmp) != 0) {
				// path was universalized, so update the string list
				input_list->deleteCurrent();
				input_list->insert(tmp.Value());
			}
			check_open(SFR_INPUT, tmp.Value(), O_RDONLY);
			accumulate_size_kb += calc_image_size_kb(tmp.Value());
		}
		if ( count ) {
			tmp_ptr = input_list->print_to_string();
			input_files->formatstr( "%s = \"%s\"",
				ATTR_TRANSFER_INPUT_FILES, tmp_ptr );
			free( tmp_ptr );
			*files_specified = true;
		}
	}
}

// Note: SetTransferFiles() sets a global variable TransferInputSizeKb which
// is the size of all the transferred input files.  This variable is used
// by SetImageSize().  So, SetTransferFiles() must be called _before_ calling
// SetImageSize().
// SetTransferFiles also sets a global "should_transfer", which is 
// used by SetRequirements().  So, SetTransferFiles must be called _before_
// calling SetRequirements() as well.
// If we are transfering files, and stdout or stderr contains
// path information, SetTransferFiles renames the output file to a plain
// file (and stores the original) in the ClassAd, so SetStdFile() should
// be called before getting here too.
int SubmitHash::SetTransferFiles()
{
	RETURN_IF_ABORT();

	char *macro_value;
	int count;
	MyString tmp;
	char* tmp_ptr;
	bool in_files_specified = false;
	bool out_files_specified = false;
	MyString	 input_files;
	MyString	 output_files;
	StringList input_file_list(NULL, ",");
	StringList output_file_list(NULL,",");
	MyString output_remaps;

	macro_value = submit_param( SUBMIT_KEY_TransferInputFiles, "TransferInputFiles" ) ;
	TransferInputSizeKb = 0;
	if( macro_value ) {
		// as a special case transferinputfiles="" will produce an empty list of input files, not a syntax error
		// PRAGMA_REMIND("replace this special case with code that correctly parses any input wrapped in double quotes")
		if (macro_value[0] == '"' && macro_value[1] == '"' && macro_value[2] == 0) {
			input_file_list.clearAll();
		} else {
			input_file_list.initializeFromString( macro_value );
		}
	}
	RETURN_IF_ABORT();


#if defined( WIN32 )
	if( JobUniverse == CONDOR_UNIVERSE_MPI ) {
			// On NT, if we're an MPI job, we need to find the
			// mpich.dll file and automatically include that in the
			// transfer input files
		MyString dll_name( "mpich.dll" );

			// first, check to make sure the user didn't already
			// specify mpich.dll in transfer_input_files
		if( ! input_file_list.contains(dll_name.Value()) ) {
				// nothing there yet, try to find it ourselves
			MyString dll_path = which( dll_name );
			if( dll_path.Length() == 0 ) {
					// File not found, fatal error.
				push_error(stderr, "Condor cannot find the "
						 "\"mpich.dll\" file it needs to run your MPI job.\n"
						 "Please specify the full path to this file in the "
						 "\"transfer_input_files\"\n"
						 "setting in your submit description file.\n" );
				ABORT_AND_RETURN( 1 );
			}
				// If we made it here, which() gave us a real path.
				// so, now we just have to append that to our list of
				// files. 
			input_file_list.append( dll_path.Value() );
		}
	}
#endif /* WIN32 */

	if( ! input_file_list.isEmpty() ) {
		process_input_file_list(&input_file_list, &input_files, &in_files_specified, TransferInputSizeKb);
	}
	RETURN_IF_ABORT();

		// also account for the size of the stdin file, if any
	bool transfer_stdin = true;
	job->LookupBool(ATTR_TRANSFER_INPUT,transfer_stdin);
	if( transfer_stdin ) {
		std::string stdin_fname;
		job->LookupString(ATTR_JOB_INPUT,stdin_fname);
		if( !stdin_fname.empty() ) {
			TransferInputSizeKb += calc_image_size_kb(stdin_fname.c_str());
		}
	}

	macro_value = submit_param( SUBMIT_KEY_TransferOutputFiles, "TransferOutputFiles" ); 
	if( macro_value ) 
	{
		// as a special case transferoutputfiles="" will produce an empty list of output files, not a syntax error
		// PRAGMA_REMIND("replace this special case with code that correctly parses any input wrapped in double quotes")
		if (macro_value[0] == '"' && macro_value[1] == '"' && macro_value[2] == 0) {
			output_file_list.clearAll();
			output_files = ATTR_TRANSFER_OUTPUT_FILES " = \"\"";
		} else {
			output_file_list.initializeFromString(macro_value);
		}
		output_file_list.rewind();
		count = 0;
		while ( (tmp_ptr=output_file_list.next()) ) {
			count++;
			tmp = tmp_ptr;
			if ( check_and_universalize_path(tmp) != 0) 
			{
				// we universalized the path, so update the string list
				output_file_list.deleteCurrent();
				output_file_list.insert(tmp.Value());
			}
		}
		tmp_ptr = output_file_list.print_to_string();
		if ( count ) {
			(void) output_files.formatstr ("%s = \"%s\"", 
				ATTR_TRANSFER_OUTPUT_FILES, tmp_ptr);
			free(tmp_ptr);
			tmp_ptr = NULL;
			out_files_specified = true;
		}
		free(macro_value);
	}
	RETURN_IF_ABORT();

	//
	// START FILE TRANSFER VALIDATION
	//
		// now that we've gathered up all the possible info on files
		// the user explicitly wants to transfer, see if they set the
		// right attributes controlling if and when to do the
		// transfers.  if they didn't tell us what to do, in some
		// cases, we can give reasonable defaults, but in others, it's
		// a fatal error.
		//
		// SHOULD_TRANSFER_FILES (STF) defaults to IF_NEEDED (STF_IF_NEEDED)
		// WHEN_TO_TRANSFER_OUTPUT (WTTO) defaults to ON_EXIT (FTO_ON_EXIT)
		// 
		// Error if:
		//  (A) bad user input - getShouldTransferFilesNum fails
		//  (B) bas user input - getFileTransferOutputNum fails
		//  (C) STF is STF_NO and WTTO is not FTO_NONE
		//  (D) STF is not STF_NO and WTTO is FTO_NONE
		//  (E) STF is STF_IF_NEEDED and WTTO is FTO_ON_EXIT_OR_EVICT
		//  (F) STF is STF_NO and transfer_input_files or transfer_output_files specified
	const char *should = "INTERNAL ERROR";
	const char *when = "INTERNAL ERROR";
	bool default_should;
	bool default_when;
	FileTransferOutput_t when_output;
	MyString err_msg;
	
	should = submit_param(ATTR_SHOULD_TRANSFER_FILES, 
						  "should_transfer_files");
	if (!should) {
		should = "IF_NEEDED";
		should_transfer = STF_IF_NEEDED;
		default_should = true;
	} else {
		should_transfer = getShouldTransferFilesNum(should);
		if (should_transfer < 0) { // (A)
			err_msg = "\nERROR: invalid value (\"";
			err_msg += should;
			err_msg += "\") for ";
			err_msg += ATTR_SHOULD_TRANSFER_FILES;
			err_msg += ".  Please either specify \"YES\", \"NO\", or ";
			err_msg += "\"IF_NEEDED\" and try again.";
			print_wrapped_text(err_msg.Value(), stderr);
			ABORT_AND_RETURN( 1 );
		}
		default_should = false;
	}

	if (should_transfer == STF_NO &&
		(in_files_specified || out_files_specified)) { // (F)
		err_msg = "\nERROR: you specified files you want Condor to "
			"transfer via \"";
		if( in_files_specified ) {
			err_msg += "transfer_input_files";
			if( out_files_specified ) {
				err_msg += "\" and \"transfer_output_files\",";
			} else {
				err_msg += "\",";
			}
		} else {
			ASSERT( out_files_specified );
			err_msg += "transfer_output_files\",";
		}
		err_msg += " but you disabled should_transfer_files.";
		print_wrapped_text(err_msg.Value(), stderr);
		ABORT_AND_RETURN( 1 );
	}

	when = submit_param(ATTR_WHEN_TO_TRANSFER_OUTPUT, 
						"when_to_transfer_output");
	if (!when) {
		when = "ON_EXIT";
		when_output = FTO_ON_EXIT;
		default_when = true;
	} else {
		when_output = getFileTransferOutputNum(when);
		if (when_output < 0) { // (B)
			err_msg = "\nERROR: invalid value (\"";
			err_msg += when;
			err_msg += "\") for ";
			err_msg += ATTR_WHEN_TO_TRANSFER_OUTPUT;
			err_msg += ".  Please either specify \"ON_EXIT\", or ";
			err_msg += "\"ON_EXIT_OR_EVICT\" and try again.";
			print_wrapped_text(err_msg.Value(), stderr);
			ABORT_AND_RETURN( 1 );
		}
		default_when = false;
	}

		// for backward compatibility and user convenience -
		// if the user specifies should_transfer_files = NO and has
		// not specified when_to_transfer_output, we'll change
		// when_to_transfer_output to NEVER and avoid an unhelpful
		// error message later.
	if (!default_should && default_when &&
		should_transfer == STF_NO) {
		when = "NEVER";
		when_output = FTO_NONE;
	}

	if ((should_transfer == STF_NO && when_output != FTO_NONE) || // (C)
		(should_transfer != STF_NO && when_output == FTO_NONE)) { // (D)
		err_msg = "\nERROR: ";
		err_msg += ATTR_WHEN_TO_TRANSFER_OUTPUT;
		err_msg += " specified as \"";
		err_msg += when;
		err_msg += "\"";
		err_msg += " yet ";
		err_msg += ATTR_SHOULD_TRANSFER_FILES;
		err_msg += " defined as \"";
		err_msg += should;
		err_msg += "\".  Please remove this contradiction from ";
		err_msg += "your submit file and try again.";
		print_wrapped_text(err_msg.Value(), stderr);
		ABORT_AND_RETURN( 1 );
	}

		// for backward compatibility and user convenience -
		// if the user specifies only when_to_transfer_output =
		// ON_EXIT_OR_EVICT, which is incompatible with the default
		// should_transfer_files of IF_NEEDED, we'll change
		// should_transfer_files to YES for them.
	if (default_should &&
		when_output == FTO_ON_EXIT_OR_EVICT &&
		should_transfer == STF_IF_NEEDED) {
		should = "YES";
		should_transfer = STF_YES;
	}

	if (when_output == FTO_ON_EXIT_OR_EVICT && 
		should_transfer == STF_IF_NEEDED) { // (E)
			// error, these are incompatible!
		err_msg = "\nERROR: \"when_to_transfer_output = ON_EXIT_OR_EVICT\" "
			"and \"should_transfer_files = IF_NEEDED\" are incompatible.  "
			"The behavior of these two settings together would produce "
			"incorrect file access in some cases.  Please decide which "
			"one of those two settings you're more interested in. "
			"If you really want \"IF_NEEDED\", set "
			"\"when_to_transfer_output = ON_EXIT\".  If you really want "
			"\"ON_EXIT_OR_EVICT\", please set \"should_transfer_files = "
			"YES\".  After you have corrected this incompatibility, "
			"please try running condor_submit again.\n";
		print_wrapped_text(err_msg.Value(), stderr);
		ABORT_AND_RETURN( 1 );
	}

	InsertFileTransAttrs(when_output);
	//
	// END FILE TRANSFER VALIDATION
	//

		/*
		  If we're dealing w/ TDP and we might be transfering files,
		  we want to make sure the tool binary and input file (if
		  any) are included in the transfer_input_files list.
		*/
	if( should_transfer!=STF_NO && HasTDP ) {
		MyString file_list;
		bool changed_it = false;
		if(job->LookupString(ATTR_TRANSFER_INPUT_FILES,file_list)!=1) {
			file_list = "";
		}
		MyString file_list_tdp;
		file_list_tdp += file_list;
		if( tdp_cmd && (!strstr(file_list.c_str(), tdp_cmd.ptr())) ) {
			TransferInputSizeKb += calc_image_size_kb(tdp_cmd.ptr());
			if(file_list[0]) {
				file_list_tdp += ",";
			}
			file_list_tdp += tdp_cmd.ptr();
			changed_it = true;
		}
		if( tdp_input && (!strstr(file_list.c_str(), tdp_input.ptr())) ) {
			TransferInputSizeKb += calc_image_size_kb(tdp_input.ptr());
			if(file_list[0]) {
				file_list_tdp += ",";
				file_list_tdp += tdp_input.ptr();
			} else {
				file_list_tdp += tdp_input.ptr();
			}
			changed_it = true;
		}
		if( changed_it ) {
			InsertJobExprString(ATTR_TRANSFER_INPUT_FILES, file_list_tdp.Value() );
		}
	}


	/*
	  In the Java universe, if we might be transfering files, we want
	  to automatically transfer the "executable" (i.e. the entry
	  class) and any requested .jar files.  However, the executable is
	  put directly into TransferInputFiles with TransferExecutable set
	  to false, because the FileTransfer object happily renames
	  executables left and right, which we don't want.
	*/

	/* 
	  We need to have our best guess of file transfer needs processed before
	  we change things to deal with the jar files and class file which
	  is the executable and should not be renamed. But we can't just
	  change and set ATTR_TRANSFER_INPUT_FILES as it is done later as the saved
	  settings in input_files and output_files is dumped out undoing our
	  careful efforts. So we will append to the input_file_list and regenerate
	  input_files. bt
	*/

	if( should_transfer!=STF_NO && JobUniverse==CONDOR_UNIVERSE_JAVA ) {
		macro_value = submit_param( SUBMIT_KEY_Executable, ATTR_JOB_CMD );

		if(macro_value) {
			MyString executable_str = macro_value;
			input_file_list.append(executable_str.Value());
			free(macro_value);
		}

		macro_value = submit_param( SUBMIT_KEY_JarFiles, ATTR_JAR_FILES );
		if(macro_value) {
			StringList files(macro_value, ",");
			files.rewind();
			while ( (tmp_ptr=files.next()) ) {
				tmp = tmp_ptr;
				input_file_list.append(tmp.Value());
			}
			free(macro_value);
		}

		if( ! input_file_list.isEmpty() ) {
			process_input_file_list(&input_file_list, &input_files, &in_files_specified, TransferInputSizeKb);
		}

		InsertJobExprString( ATTR_JOB_CMD, "java");

		MyString b;
		b.formatstr("%s = FALSE", ATTR_TRANSFER_EXECUTABLE);
		InsertJobExpr( b.Value() );
	}

	// If either stdout or stderr contains path information, and the
	// file is being transfered back via the FileTransfer object,
	// substitute a safe name to use in the sandbox and stash the
	// original name in the output file "remaps".  The FileTransfer
	// object will take care of transferring the data back to the
	// correct path.

	// Starting with Condor 7.7.2, we only do this remapping if we're
	// spooling files to the schedd. The shadow/starter will do any
	// required renaming in the non-spooling case.
	CondorVersionInfo cvi(getScheddVersion());
	if ( (!cvi.built_since_version(7, 7, 2) && should_transfer != STF_NO &&
		  JobUniverse != CONDOR_UNIVERSE_GRID &&
		  JobUniverse != CONDOR_UNIVERSE_STANDARD) ||
		 IsRemoteJob ) {

		MyString output;
		MyString error;

		job->LookupString(ATTR_JOB_OUTPUT,output);
		job->LookupString(ATTR_JOB_ERROR,error);

		if(output.Length() && output != condor_basename(output.Value()) && 
		   strcmp(output.Value(),"/dev/null") != 0 && !StreamStdout)
		{
			char const *working_name = StdoutRemapName;
				//Force setting value, even if we have already set it
				//in the cluster ad, because whatever was in the
				//cluster ad may have been overwritten (e.g. by a
				//filename containing $(Process)).  At this time, the
				//check in InsertJobExpr() is not smart enough to
				//notice that.
			InsertJobExprString(ATTR_JOB_OUTPUT, working_name);

			if(!output_remaps.IsEmpty()) output_remaps += ";";
			output_remaps.formatstr_cat("%s=%s",working_name,output.EscapeChars(";=\\",'\\').Value());
		}

		if(error.Length() && error != condor_basename(error.Value()) && 
		   strcmp(error.Value(),"/dev/null") != 0 && !StreamStderr)
		{
			char const *working_name = StderrRemapName;

			if(error == output) {
				//stderr will use same file as stdout
				working_name = StdoutRemapName;
			}
				//Force setting value, even if we have already set it
				//in the cluster ad, because whatever was in the
				//cluster ad may have been overwritten (e.g. by a
				//filename containing $(Process)).  At this time, the
				//check in InsertJobExpr() is not smart enough to
				//notice that.
			InsertJobExprString(ATTR_JOB_ERROR, working_name);

			if(!output_remaps.IsEmpty()) output_remaps += ";";
			output_remaps.formatstr_cat("%s=%s",working_name,error.EscapeChars(";=\\",'\\').Value());
		}
	}

	// if we might be using file transfer, insert in input/output
	// exprs  
	if( should_transfer != STF_NO ) {
		if ( input_files.Length() > 0 ) {
			InsertJobExpr (input_files);
		}
#ifdef HAVE_HTTP_PUBLIC_FILES
		char *public_input_files = 
			submit_param(SUBMIT_KEY_PublicInputFiles, ATTR_PUBLIC_INPUT_FILES);
		if (public_input_files) {
			StringList pub_inp_file_list(NULL, ",");
			pub_inp_file_list.initializeFromString(public_input_files);
			MyString unusedstr;
			bool unusedbool = false;
			// Process file list, but output string is for ATTR_TRANSFER_INPUT_FILES,
			// so it's not used here.
			process_input_file_list(&pub_inp_file_list, &unusedstr, &unusedbool, TransferInputSizeKb);
			if (pub_inp_file_list.isEmpty() == false) {
				char *inp_file_str = pub_inp_file_list.print_to_string();
				if (inp_file_str) {
					InsertJobExprString(ATTR_PUBLIC_INPUT_FILES, inp_file_str);
					free(inp_file_str);
				}
			}
			free(public_input_files);
		} 
#endif
		if ( output_files.Length() > 0 ) {
			InsertJobExpr (output_files);
		}
	}

	if( should_transfer == STF_NO && 
		JobUniverse != CONDOR_UNIVERSE_GRID &&
		JobUniverse != CONDOR_UNIVERSE_JAVA &&
		JobUniverse != CONDOR_UNIVERSE_VM )
	{
		char *transfer_exe;
		transfer_exe = submit_param( SUBMIT_KEY_TransferExecutable,
									 ATTR_TRANSFER_EXECUTABLE );
		if(transfer_exe && *transfer_exe != 'F' && *transfer_exe != 'f') {
			//User explicitly requested transfer_executable = true,
			//but they did not turn on transfer_files, so they are
			//going to be confused when the executable is _not_
			//transfered!  Better bail out.
			err_msg = "\nERROR: You explicitly requested that the "
				"executable be transfered, but for this to work, you must "
				"enable Condor's file transfer functionality.  You need "
				"to define either: \"when_to_transfer_output = ON_EXIT\" "
				"or \"when_to_transfer_output = ON_EXIT_OR_EVICT\".  "
				"Optionally, you can define \"should_transfer_files = "
				"IF_NEEDED\" if you do not want any files to be transfered "
				"if the job executes on a machine in the same "
				"FileSystemDomain.  See the Condor manual for more "
				"details.";
			print_wrapped_text( err_msg.Value(), stderr );
			ABORT_AND_RETURN( 1 );
		}

		free(transfer_exe);
	}

	macro_value = submit_param( SUBMIT_KEY_TransferOutputRemaps,ATTR_TRANSFER_OUTPUT_REMAPS);
	if(macro_value) {
		if(*macro_value != '"' || macro_value[1] == '\0' || macro_value[strlen(macro_value)-1] != '"') {
			push_error(stderr, "transfer_output_remaps must be a quoted string, not: %s\n",macro_value);
			ABORT_AND_RETURN( 1 );
		}

		macro_value[strlen(macro_value)-1] = '\0';  //get rid of terminal quote

		if(!output_remaps.IsEmpty()) output_remaps += ";";
		output_remaps += macro_value+1; // add user remaps to auto-generated ones

		free(macro_value);
	}

	if(!output_remaps.IsEmpty()) {
		MyString expr;
		expr.formatstr("%s = \"%s\"",ATTR_TRANSFER_OUTPUT_REMAPS,output_remaps.Value());
		InsertJobExpr(expr);
	}

		// Check accessibility of output files.
	output_file_list.rewind();
	char const *output_file;
	while ( (output_file=output_file_list.next()) ) {
		output_file = condor_basename(output_file);
		if( !output_file || !output_file[0] ) {
				// output_file may be empty if the entry in the list is
				// a path ending with a slash.  Since a path ending in a
				// slash means to get the contents of a directory, and we
				// don't know in advance what names will exist in the
				// directory, we can't do any check now.
			continue;
		}
		// Apply filename remaps if there are any.
		MyString remap_fname;
		if(filename_remap_find(output_remaps.Value(),output_file,remap_fname)) {
			output_file = remap_fname.Value();
		}

		check_open(SFR_OUTPUT, output_file, O_WRONLY|O_CREAT|O_TRUNC );
	}

	char *MaxTransferInputExpr = submit_param(SUBMIT_KEY_MaxTransferInputMB,ATTR_MAX_TRANSFER_INPUT_MB);
	char *MaxTransferOutputExpr = submit_param(SUBMIT_KEY_MaxTransferOutputMB,ATTR_MAX_TRANSFER_OUTPUT_MB);
	if( MaxTransferInputExpr ) {
		std::string max_expr;
		formatstr(max_expr,"%s = %s",ATTR_MAX_TRANSFER_INPUT_MB,MaxTransferInputExpr);
		InsertJobExpr(max_expr.c_str());
		free( MaxTransferInputExpr );
	}
	if( MaxTransferOutputExpr ) {
		std::string max_expr;
		formatstr(max_expr,"%s = %s",ATTR_MAX_TRANSFER_OUTPUT_MB,MaxTransferOutputExpr);
		InsertJobExpr(max_expr.c_str());
		free( MaxTransferOutputExpr );
	}

	return 0;
}

int SubmitHash::FixupTransferInputFiles()
{
	RETURN_IF_ABORT();

		// See the comment in the function body of ExpandInputFileList
		// for an explanation of what is going on here.

	if ( ! IsRemoteJob ) {
		return 0;
	}

	MyString input_files;
	if( job->LookupString(ATTR_TRANSFER_INPUT_FILES,input_files) != 1 ) {
		return 0; // nothing to do
	}

	if (ComputeIWD()) { ABORT_AND_RETURN(1); }

	MyString error_msg;
	MyString expanded_list;
	bool success = FileTransfer::ExpandInputFileList(input_files.c_str(),JobIwd.c_str(),expanded_list,error_msg);
	if (success) {
		if (expanded_list != input_files) {
			dprintf(D_FULLDEBUG,"Expanded input file list: %s\n",expanded_list.Value());
			job->Assign(ATTR_TRANSFER_INPUT_FILES,expanded_list.c_str());
		}
	} else {
		MyString err_msg;
		err_msg.formatstr( "\n%s\n",error_msg.Value());
		print_wrapped_text( err_msg.Value(), stderr );
		ABORT_AND_RETURN( 1 );
	}
	return 0;
}



void SubmitHash::delete_job_ad()
{
	delete job;
	job = NULL;
	delete procAd;
	procAd = NULL;
}

#ifdef WIN32
void publishWindowsOSVersionInfo(ClassAd & ad)
{
	// Publish the version of Windows we are running
	OSVERSIONINFOEX os_version_info;
	ZeroMemory ( &os_version_info, sizeof ( OSVERSIONINFOEX ) );
	os_version_info.dwOSVersionInfoSize = sizeof ( OSVERSIONINFOEX );
	BOOL ok = GetVersionEx ( (OSVERSIONINFO*) &os_version_info );
	if ( !ok ) {
		os_version_info.dwOSVersionInfoSize =
			sizeof ( OSVERSIONINFO );
		ok = GetVersionEx ( (OSVERSIONINFO*) &os_version_info );
		if ( !ok ) {
			dprintf ( D_FULLDEBUG, "Submit: failed to get Windows version information\n" );
		}
	}

	if ( ok ) {
		ad.Assign(ATTR_WINDOWS_MAJOR_VERSION, os_version_info.dwMajorVersion);
		ad.Assign(ATTR_WINDOWS_MINOR_VERSION, os_version_info.dwMinorVersion);
		ad.Assign(ATTR_WINDOWS_BUILD_NUMBER, os_version_info.dwBuildNumber);
		// publish the extended Windows version information if we have it at our disposal
		if (sizeof(OSVERSIONINFOEX) == os_version_info.dwOSVersionInfoSize) {
			ad.Assign(ATTR_WINDOWS_SERVICE_PACK_MAJOR, os_version_info.wServicePackMajor);
			ad.Assign(ATTR_WINDOWS_SERVICE_PACK_MINOR, os_version_info.wServicePackMinor);
			ad.Assign(ATTR_WINDOWS_PRODUCT_TYPE, os_version_info.wProductType);
		}
	}
}
#endif

int SubmitHash::set_cluster_ad(ClassAd * ad)
{
	delete job;
	job = NULL;
	delete procAd;
	procAd = NULL;

	MACRO_EVAL_CONTEXT ctx = mctx; mctx.use_mask = 0;
	ad->LookupString (ATTR_OWNER, submit_owner);
	ad->LookupInteger(ATTR_CLUSTER_ID, jid.cluster);
	ad->LookupInteger(ATTR_PROC_ID, jid.proc);
	ad->LookupInteger(ATTR_Q_DATE, submit_time);
	if (ad->LookupString(ATTR_JOB_IWD, JobIwd) && ! JobIwd.empty()) {
		JobIwdInitialized = true;
		insert_macro("FACTORY.Iwd", JobIwd.c_str(), SubmitMacroSet, DetectedMacro, ctx);
	}

	this->clusterAd = ad;
	// Force the cluster IWD to be computed, so we can safely call getIWD and full_path
	ComputeIWD(false);
	return 0;
}

int SubmitHash::init_cluster_ad(time_t submit_time_in, const char * owner)
{
	MyString buffer;
	ASSERT(owner);
	submit_owner = owner;

	delete job; job = NULL;
	delete procAd; procAd = NULL;

	// set up types of the ad
	SetMyTypeName (baseJob, JOB_ADTYPE);
	SetTargetTypeName (baseJob, STARTD_ADTYPE);

	if (submit_time_in) {
		submit_time = submit_time_in;
	} else {
		submit_time = time(NULL);
	}

	// all jobs should end up with the same qdate, so we only query time once.
	baseJob.Assign(ATTR_Q_DATE, submit_time);
	baseJob.Assign(ATTR_COMPLETION_DATE, 0);

	//PRAGMA_REMIND("should not be setting owner at all...")
	baseJob.Assign(ATTR_OWNER, owner);

#ifdef WIN32
	// put the NT domain into the ad as well
	auto_free_ptr ntdomain(my_domainname());
	if (ntdomain) baseJob.Assign(ATTR_NT_DOMAIN, ntdomain.ptr());

	// Publish the version of Windows we are running
	if (param_boolean("SUBMIT_PUBLISH_WINDOWS_OSVERSIONINFO", false)) {
		publishWindowsOSVersionInfo(baseJob);
	}
#endif

	baseJob.Assign(ATTR_JOB_REMOTE_WALL_CLOCK, 0.0);
	baseJob.Assign(ATTR_JOB_LOCAL_USER_CPU,    0.0);
	baseJob.Assign(ATTR_JOB_LOCAL_SYS_CPU,     0.0);
	baseJob.Assign(ATTR_JOB_REMOTE_USER_CPU,   0.0);
	baseJob.Assign(ATTR_JOB_REMOTE_SYS_CPU,    0.0);
	baseJob.Assign(ATTR_JOB_CUMULATIVE_REMOTE_USER_CPU,   0.0);
	baseJob.Assign(ATTR_JOB_CUMULATIVE_REMOTE_SYS_CPU,    0.0);

	baseJob.Assign(ATTR_JOB_EXIT_STATUS, 0);
	baseJob.Assign(ATTR_NUM_CKPTS, 0);
	baseJob.Assign(ATTR_NUM_JOB_STARTS, 0);
	baseJob.Assign(ATTR_NUM_JOB_COMPLETIONS, 0);
	baseJob.Assign(ATTR_NUM_RESTARTS, 0);
	baseJob.Assign(ATTR_NUM_SYSTEM_HOLDS, 0);
	baseJob.Assign(ATTR_JOB_COMMITTED_TIME, 0);
	baseJob.Assign(ATTR_COMMITTED_SLOT_TIME, 0);
	baseJob.Assign(ATTR_CUMULATIVE_SLOT_TIME, 0);
	baseJob.Assign(ATTR_TOTAL_SUSPENSIONS, 0);
	baseJob.Assign(ATTR_LAST_SUSPENSION_TIME, 0);
	baseJob.Assign(ATTR_CUMULATIVE_SUSPENSION_TIME, 0);
	baseJob.Assign(ATTR_COMMITTED_SUSPENSION_TIME, 0);
	baseJob.Assign(ATTR_ON_EXIT_BY_SIGNAL, false);

#if 0
	// can't use this because of +attrs and My.attrs in SUBMIT_ATTRS
	config_fill_ad( job );
#else
	classad::References submit_attrs;
	param_and_insert_attrs("SUBMIT_ATTRS", submit_attrs);
	param_and_insert_attrs("SUBMIT_EXPRS", submit_attrs);
	param_and_insert_attrs("SYSTEM_SUBMIT_ATTRS", submit_attrs);

	if ( ! submit_attrs.empty()) {
		MyString buffer;

		for (classad::References::const_iterator it = submit_attrs.begin(); it != submit_attrs.end(); ++it) {
			if (starts_with(*it,"+")) {
				forcedSubmitAttrs.insert(it->substr(1));
				continue;
			} else if (starts_with_ignore_case(*it, "MY.")) {
				forcedSubmitAttrs.insert(it->substr(3));
				continue;
			}

			auto_free_ptr expr(param(it->c_str()));
			if ( ! expr) continue;
			ExprTree *tree = NULL;
			bool valid_expr = (0 == ParseClassAdRvalExpr(expr.ptr(), tree)) && tree != NULL;
			if ( ! valid_expr) {
				dprintf(D_ALWAYS, "could not insert SUBMIT_ATTR %s. did you forget to quote a string value?\n", it->c_str());
				//push_warning(stderr, "could not insert SUBMIT_ATTR %s. did you forget to quote a string value?\n", it->c_str());
			} else {
				baseJob.Insert(it->c_str(), tree);
			}
		}
	}
	
	/* Insert the version into the ClassAd */
	baseJob.Assign( ATTR_VERSION, CondorVersion() );
	baseJob.Assign( ATTR_PLATFORM, CondorPlatform() );
#endif

#if 0 // not used right now..
	// if there is an Attrs ad, copy from it into this ad.
	if (from_ad) {

		const char * key;
		ExprTree * rhs;
		from_ad->ResetExpr();
		while( from_ad->NextExpr(key, rhs) ) {
			ExprTree * tree = rhs->Copy();
			if ( ! tree || ! baseJob.Insert(key, tree, false)) {
				// error message here?
				abort_code = 1;
				break;
			}
		}
	}
#endif

	return abort_code;
}

ClassAd* SubmitHash::make_job_ad (
	JOB_ID_KEY job_id, // ClusterId and ProcId
	int item_index, // Row or ItemIndex
	int step,       // Step
	bool interactive,
	bool remote,
	int (*check_file)(void*pv, SubmitHash * sub, _submit_file_role role, const char * name, int flags),
	void* pv_check_arg)
{
	jid = job_id;
	IsInteractiveJob = interactive;
	IsRemoteJob = remote;
	// save these for use by check_open
	FnCheckFile = check_file;
	CheckFileArg = pv_check_arg;

	strcpy(LiveNodeString,"");
	(void)sprintf(LiveClusterString, "%d", job_id.cluster);
	(void)sprintf(LiveProcessString, "%d", job_id.proc);
	(void)sprintf(LiveRowString, "%d", item_index);
	(void)sprintf(LiveStepString, "%d", step);

	// calling this function invalidates the job returned from the previous call
	delete job; job = NULL;
	delete procAd; procAd = NULL;

	// we only set the universe once per cluster.
	if (JobUniverse <= CONDOR_UNIVERSE_MIN || job_id.proc <= 0) {
		ClassAd universeAd;
		DeltaClassAd tmpDelta(universeAd);
		procAd = &universeAd;
		job =  &tmpDelta;
		SetUniverse();
		baseJob.Update(universeAd);
		if (clusterAd) {
			int uni = CONDOR_UNIVERSE_MIN;
			if ( ! clusterAd->LookupInteger(ATTR_JOB_UNIVERSE, uni) || uni != JobUniverse) {
				clusterAd->Update(universeAd);
			}
		}
		job = NULL;
		procAd = NULL;
	}

	// Now that we know the universe, we can set the default NODE macro, note that dagman also uses NODE
	// but since this is a default macro, the dagman node will take precedence.
	// we set it to #pArAlLeLnOdE# for PARALLEL universe, for universe MPI, we need it to expand as "#MpInOdE#" instead.
	if (JobUniverse == CONDOR_UNIVERSE_PARALLEL) {
		strcpy(LiveNodeString, "#pArAlLeLnOdE#");
	} else if (JobUniverse == CONDOR_UNIVERSE_MPI) {
		strcpy(LiveNodeString, "#MpInOdE#");
	}

#if 1
	if (clusterAd) {
		procAd = new ClassAd();
		procAd->ChainToAd(clusterAd);
	} else {
		procAd = new ClassAd(baseJob);
	}
	job = new DeltaClassAd(*procAd);
#else
	ClassAd * baseAd = clusterAd ? clusterAd : &baseJob;
	job = new ClassAd(baseJob);
#endif

#if !defined(WIN32)
	SetRootDir(!clusterAd);	// must be called very early
#endif
	SetIWD(!clusterAd);		// must be called very early

	SetExecutable();

	SetDescription();
	SetMachineCount();

	// Note: Due to the unchecked use of global variables everywhere in
	// condor_submit, the order that we call the below Set* functions
	// is very important!
	SetJobStatus();
	SetPriority();
	SetMaxJobRetirementTime();
	SetEnvironment();
	SetNotification();
	SetWantRemoteIO();
	SetNotifyUser();
	SetEmailAttributes();
	SetRemoteInitialDir();
	SetExitRequirements();
	SetOutputDestination();
	SetWantGracefulRemoval();
	SetJobMaxVacateTime();

    // really a command, needs to happen before any calls to check_open
	JobDisableFileChecks = submit_param_bool("skip_filechecks", NULL, false);
	PRAGMA_REMIND("TODO: several bits of grid code are ignoring JobDisableFileChecks and bypassing FnCheckFile, check to see if that is kosher.")

	SetUserLog();
	SetUserLogXML();
	SetCoreSize();
#if !defined(WIN32)
	SetKillSig();
#endif
	SetRank();
	SetStdFile( 0 );
	SetStdFile( 1 );
	SetStdFile( 2 );
	SetFileOptions();
	SetFetchFiles();
	SetCompressFiles();
	SetAppendFiles();
	SetLocalFiles();
	SetEncryptExecuteDir();
	SetTDP();			// before SetTransferFile() and SetRequirements()
	SetTransferFiles();	 // must be called _before_ SetImageSize() 
	SetRunAsOwner();
    SetLoadProfile();
	SetPerFileEncryption();  // must be called _before_ SetRequirements()
	SetImageSize();		// must be called _after_ SetTransferFiles()
    SetRequestResources();

	SetSimpleJobExprs();

		//
		// SetCronTab() must be called before SetJobDeferral()
		// Both of these need to be called before SetRequirements()
		//
	SetCronTab();
	SetJobDeferral();
	SetJobRetries();
		
		//
		// Must be called _after_ SetTransferFiles(), SetJobDeferral(),
		// SetCronTab(), and SetPerFileEncryption()
		//
	SetRequirements();
		
	SetJobLease();		// must be called _after_ SetStdFile(0,1,2)

	SetRemoteAttrs();
	SetJobMachineAttrs();

	SetPeriodicHoldCheck();
	SetPeriodicRemoveCheck();
	SetNoopJob();
	SetLeaveInQueue();
	SetArguments();
	SetGridParams();
	SetGSICredentials();
	SetMatchListLen();
	SetDAGNodeName();
	SetDAGManJobId();
	SetJarFiles();
	SetJavaVMArgs();
	SetParallelStartupScripts(); //JDB
	SetConcurrencyLimits();
	SetAccountingGroup();
	SetVMParams();
	SetLogNotes();
	SetUserNotes();
	SetStackSize();

		// This must come after all things that modify the input file list
	FixupTransferInputFiles();

		// SetForcedAttributes should be last so that it trumps values
		// set by normal submit attributes
	SetForcedAttributes();

	// if we aborted in any of the steps above, then delete the job and return NULL.
	if (abort_code) {
		delete job;
		job = NULL;
		delete procAd;
		procAd = NULL;
	}
	return procAd;
}

void SubmitHash::insert_source(const char * filename, MACRO_SOURCE & source)
{
	::insert_source(filename, SubmitMacroSet, source);
}

// Check to see if this is a queue statement, if it is, return a pointer to the queue arguments.
// 
static const char * is_queue_statement(const char * line)
{
	const int cchQueue = sizeof("queue")-1;
	if (starts_with_ignore_case(line, "queue") && (0 == line[cchQueue] || isspace(line[cchQueue]))) {
		const char * pqargs = line+cchQueue;
		while (*pqargs && isspace(*pqargs)) ++pqargs;
		return pqargs;
	}
	return NULL;
}


// set the slice by parsing a string [x:y:z], where
// the enclosing [] are required
// x,y & z are integers, y and z are optional
char *qslice::set(char* str) {
	flags = 0;
	if (*str == '[') {
		char * p = str;
		char * pend=NULL;
		flags |= 1;
		int val = (int)strtol(p+1, &pend, 10);
		if ( ! pend || (*pend != ':' && *pend != ']')) { flags = 0; return str; }
		start = val; if (pend > p+1) flags |= 2;
		p = pend;
		if (*p == ']') return p;
		val = (int)strtol(p+1, &pend, 10);
		if ( ! pend || (*pend != ':' && *pend != ']')) { flags = 0; return str; }
		end = val; if (pend > p+1) flags |= 4;
		p = pend;
		if (*p == ']') return p;
		val = (int)strtol(p+1, &pend, 10);
		if ( ! pend || *pend != ']') { flags = 0; return str; }
		step = val; if (pend > p+1) flags |= 8;
		return pend+1;
	}
	return str;
}

// convert ix based on slice start & step, returns true if translated ix is within slice start and length.
// input ix is assumed to be 0 based and increasing.
bool qslice::translate(int & ix, int len) {
	if (!(flags & 1)) return ix >= 0 && ix < len;
	int im = (flags&8) ? step : 1;
	if (im <= 0) {
		ASSERT(0); // TODO: implement negative iteration.
	} else {
		int is = 0;   if (flags&2) { is = (start < 0) ? start+len : start; }
		int ie = len; if (flags&4) { ie = is + ((end < 0) ? end+len : end); }
		int iy = is + (ix*im);
		ix = iy;
		return ix >= is && ix < ie;
	}
}

// check to see if ix is selected for by the slice. negative iteration is ignored 
bool qslice::selected(int ix, int len) {
	if (!(flags&1)) return ix >= 0 && ix < len;
	int is = 0; if (flags&2) { is = (start < 0) ? start+len : start; }
	int ie = len; if (flags&4) { ie = (end < 0) ? end+len : end; }
	return ix >= is && ix < ie && ( !(flags&8) || (0 == ((ix-is) % step)) );
}

// returns number of selected items for a list of the given length, result is never negative
// negative step values NOT handled correctly
int qslice::length_for(int len) {
	if (!(flags&1)) return len;
	int is = 0; if (flags&2) { is = (start < 0) ? start+len : start; }
	int ie = len; if (flags&4) { ie = (end < 0) ? end+len : end; }
	int ret = ie - is;
	if ((flags&8) && step > 1) { 
		ret = (ret + step -1) / step;
	}
	// bound the return value to the range of 0 to len
	ret = MAX(0, ret);
	return MIN(ret, len);
}


int qslice::to_string(char * buf, int cch) {
	char sz[16*3];
	if ( ! (flags&1)) return 0;
	char * p = sz;
	*p++  = '[';
	if (flags&2) { p += sprintf(p,"%d", start); }
	*p++ = ':';
	if (flags&4) { p += sprintf(p,"%d", end); }
	*p++ = ':';
	if (flags&8) { p += sprintf(p,"%d", step); }
	*p++ = ']';
	*p = 0;
	strncpy(buf, sz, cch); buf[cch-1] = 0;
	return (int)(p - sz);
}


// scan for a keyword from set of tokens, the keywords must be surrounded by whitespace
// or terminated by (.  if a token is found token_id is set, otherwise it is left untouched.
// the return value is a pointer to where scanning left off.

struct _qtoken { const char * name; int id; };

static char * queue_token_scan(char * ptr, const struct _qtoken tokens[], int ctokens, char** pptoken, int & token_id, bool scan_until_match)
{
	char *ptok = NULL;   // pointer to start of current token in pqargs when scanning for keyword
	char tokenbuf[sizeof("matching")+1] = ""; // temporary buffer to hold a potential keyword while scanning
	int  maxtok = (int)sizeof(tokenbuf)-1;
	int  cchtok = 0;

	char * p = ptr;

	while (*p) {
		int ch = *p;
		if (isspace(ch) || ch == '(') {
			if (cchtok >= 1 && cchtok <= maxtok) {
				tokenbuf[cchtok] = 0;
				int ix = 0;
				for (ix = 0; ix < ctokens; ++ix) {
					if (MATCH == strcasecmp(tokenbuf, tokens[ix].name))
						break;
				}
				if (ix < ctokens) { token_id = tokens[ix].id; *pptoken = ptok; break; }
			}
			if ( ! scan_until_match) { *pptoken = ptok; break; }
			cchtok = 0;
		} else {
			if ( ! cchtok) { ptok = p; }
			if (cchtok < maxtok) { tokenbuf[cchtok] = ch; }
			++cchtok;
		}
		++p;
	}

	return p;
}

// returns number of selected items
// the items member must have been populated
// or the mode must be foreach_not
// the return does not take queue_num into account.
int SubmitForeachArgs::item_len()
{
	if (foreach_mode == foreach_not) return 1;
	return slice.length_for(items.number());
}

enum {
	PARSE_ERROR_INVALID_QNUM_EXPR = -2,
	PARSE_ERROR_QNUM_OUT_OF_RANGE = -3,
	PARSE_ERROR_UNEXPECTED_KEYWORD = -4,
	PARSE_ERROR_BAD_SLICE = -5,
};

// parse a the arguments for a Queue statement. this will be of the form
//
//    [<num-expr>] [[<var>[,<var2>]] in|from|matching [<slice>][<tokening>] (<items>)]
// 
//  {} indicates optional, <> indicates argument type rather than literal text, | is either or
//
//  <num-expr> is any classad expression that parses to an int it defines the number of
//             procs to queue per item in <items>.  If not present 1 is used.
//  <var>      is a variable name, case insensitive, case preserving, must begin with alpha and contain only alpha, numbers and _
//  in|from|matching  only one of these case-insensitive keywords may occur, these control interpretation of <items>
//  <slice>    is a python style slice controlling the start,end & step through the <items>
//  <tokening> arguments that control tokenizing <items>.
//  <items>    is a list of items to iterate and queue. the () surrounding items are optional, if they exist then
//             items may span multiple lines, in which case the final ) must be on a line by itself.
//
// The basic parsing strategy is:
//    1) find the in|from|matching keyword by scanning for whitespace or ( delimited words
//       if NOT FOUND, set end_num to end of input string and goto step 5 (because the whole input is <num-expr>)
//       else set end_num to start of keyword.
//    2) parse forwards from end of keyword looking for (
//       if no ( is found, parse remainder of line as single-line itemlist
//       if ( is found look to see if last char on line is )
//          if found both ( and ) parse content as a single-line itemlist
//          else set items_filename appropriately based on keyword.
//    3) FUTURE WORK: parse characters between keyword and ( as <slice> and <tokening>
//    4) parse backwards from start of keyword while you see valid VAR,VAR2,etc (basically look for bare numbers or non-alphanum chars)
//       set end_num to first char that cannot be VAR,VAR2
//       if VARS found, parse into vars stringlist.
///   4) eval from start of line to end_num and set queue_num.
//
int SubmitForeachArgs::parse_queue_args (
	char * pqargs)      // in:  queue line, THIS MAY BE MODIFIED!! \0 will be inserted to delimit things
{
	foreach_mode = foreach_not;
	vars.clearAll();
	items.clearAll();
	items_filename.clear();

	while (isspace(*pqargs)) ++pqargs;

	// empty queue args means queue 1
	if ( ! *pqargs) {
		queue_num = 1;
		return 0;
	}

	char *p = pqargs;    // pointer to current char while scanning
	char *ptok = NULL;   // pointer to start of current token in pqargs when scanning for keyword
	static const struct _qtoken foreach_tokens[] = { {"in", foreach_in }, {"from", foreach_from}, {"matching", foreach_matching} };
	p = queue_token_scan(p, foreach_tokens, COUNTOF(foreach_tokens), &ptok, foreach_mode, true);

	// for now assume that p points to the end of the queue count expression.
	char * pnum_end = p;

	// if we found a in,from, or matching keyword. we use that to anchor the rest of the parse
	// before the keyword is the optional vars list, and before that is the optional queue count.
	// after the keyword is the start of the items list.
	if (foreach_mode != foreach_not) {
		// if we found a keyword, then p points to the start of the itemlist
		// and we have to scan backwords to find the end of the queue count.
		while (*p && isspace(*p)) ++p;

		// check for qualifiers after the foreach keyword
		if (*p != '(') {
			static const struct _qtoken quals[] = { {"files", 1 }, {"dirs", 2}, {"any", 3} };
			for (;;) {
				char * p2 = p;
				int qual = -1;
				char * ptmp = NULL;
				p2 = queue_token_scan(p2, quals, COUNTOF(quals), &ptmp, qual, false);
				if (ptmp && *ptmp == '[') { qual = 4; }
				if (qual <= 0)
					break;

				switch (qual)
				{
				case 1:
					if (foreach_mode == foreach_matching) foreach_mode = foreach_matching_files;
					else return PARSE_ERROR_UNEXPECTED_KEYWORD;
					break;

				case 2:
					if (foreach_mode == foreach_matching) foreach_mode = foreach_matching_dirs;
					else return PARSE_ERROR_UNEXPECTED_KEYWORD;
					break;

				case 3:
					if (foreach_mode == foreach_matching) foreach_mode = foreach_matching_any;
					else return PARSE_ERROR_UNEXPECTED_KEYWORD;
					break;

				case 4:
					p2 = slice.set(ptmp);
					if ( ! slice.initialized()) return PARSE_ERROR_BAD_SLICE;
					if (*p2 == ']') ++p2;
					break;

				default:
					break;
				}

				if (p == p2) break; // just in case we don't advance.
				p = p2;
				while (*p && isspace(*p)) ++p;
			}
		}

		// parse the itemlist. this can be all on a single line, or spanning multiple lines.
		// we only parse the first line here, and set items_filename to indicate how to proceed
		char * plist = p;
		bool one_line_list = false;
		if (*plist == '(') {
			int cch = (int)strlen(plist);
			if (plist[cch-1] == ')') { plist[cch-1] = 0; ++plist; one_line_list = true; }
		}
		if (*plist == '(') {
			// this is a multiline list, if it is to be read from fp_submit, we can't do that here 
			// because reading from fp_submit will invalidate pqargs. instead we append whatever we
			// find on the current line, and then set the filename to "<" to tell the caller to read
			// the remainder from the submit file.
			++plist;
			while (isspace(*plist)) ++plist;
			if (*plist) {
				if (foreach_mode == foreach_from) {
					items.append(plist);
				} else {
					items.initializeFromString(plist);
				}
			}
			items_filename = "<";
		} else if (foreach_mode == foreach_from) {
			while (isspace(*plist)) ++plist;
			if (one_line_list) {
				items.append(plist);
			} else {
				items_filename = plist;
				items_filename.trim();
			}
		} else {
			while (isspace(*plist)) ++plist;
			items.initializeFromString(plist);
		}

		// trim trailing whitespace before the in,from, or matching keyword.
		char * pvars = ptok;
		while (pvars > pqargs && isspace(pvars[-1])) { --pvars; }

		// walk backwards until we get to something that can't be loop variable.
		// so we scan backwards over alpha, command and space, but only scan over
		// numbers if they are preceeded by alpha. this allows variables to be VAR1 but not 1VAR
		if (pvars > pqargs) {
			*pvars = 0; // null terminate the vars
			char * pt = pvars;
			while (pt > pqargs) {
				char ch = pt[-1];
				if (isdigit(ch)) {
					--pt;
					while (pt > pqargs) {
						ch = pt[-1];
						if (isalpha(ch)) { break; }
						if ( ! isdigit(ch)) { ch = '!'; break; } // force break out of outer loop
						--pt;
					}
				}
				if (isspace(ch) || isalpha(ch) || ch == ',' || ch == '_' || ch == '.') {
					pvars = pt-1;
				} else {
					break;
				}
				--pt;
			}
			// pvars should now point to a null-terminated string that is the var set.
			vars.initializeFromString(pvars);
		}
		// whatever remains from pvars to the start of the args is the queue count.
		pnum_end = pvars;
	}

	// parse the queue count.
	while (pnum_end > pqargs && isspace(pnum_end[-1])) { --pnum_end; }
	if (pnum_end > pqargs) {
		*pnum_end = 0;
		long long value = -1;
		if ( ! string_is_long_param(pqargs, value)) {
			return PARSE_ERROR_INVALID_QNUM_EXPR;
		} else if (value < 0 || value >= INT_MAX) {
			return PARSE_ERROR_QNUM_OUT_OF_RANGE;
		}
		queue_num = (int)value;
	} else {
		queue_num = 1;
	}

	return 0;
}


// parse the arguments after the Queue statement and populate a SubmitForeachArgs
// as much as possible without globbing or reading any files.
// if queue_args is "", then that is interpreted as Queue 1 just like condor_submit
int SubmitHash::parse_q_args(
	const char * queue_args,               // IN: arguments after Queue statement before macro expansion
	SubmitForeachArgs & o,                 // OUT: options & items from parsing the queue args
	std::string & errmsg)                  // OUT: error message if return value is not 0
{
	int rval = 0;

	auto_free_ptr expanded_queue_args(expand_macro(queue_args));
	char * pqargs = expanded_queue_args.ptr();
	ASSERT(pqargs);

	// skip whitespace before queue arguments (if any)
	while (isspace(*pqargs)) ++pqargs;

	// parse the queue arguments, handling the count and finding the in,from & matching keywords
	// on success pqargs will point to to \0 or to just after the keyword.
	rval = o.parse_queue_args(pqargs);
	if (rval < 0) {
		errmsg = "invalid Queue statement";
		return rval;
	}

	return 0;
}

// finish populating the items in a SubmitForeachArgs if they can be populated from the submit file itself.
//
int SubmitHash::load_inline_q_foreach_items (
	MacroStream & ms,
	SubmitForeachArgs & o,
	std::string & errmsg)
{
	bool items_are_external = false;

	// if no loop variable specified, but a foreach mode is used. use "Item" for the loop variable.
	if (o.vars.isEmpty() && (o.foreach_mode != foreach_not)) { o.vars.append("Item"); }

	if ( ! o.items_filename.empty()) {
		if (o.items_filename == "<") {
			MACRO_SOURCE & source = ms.source();
			if ( ! source.id) {
				errmsg = "unexpected error while attempting to read queue items from submit file.";
				return -1;
			}
			// read items from submit file until we see the closing brace on a line by itself.
			bool saw_close_brace = false;
			int item_list_begin_line = source.line;
			for(char * line=NULL; ; ) {
				line = getline_trim(ms);
				if ( ! line) break; // null indicates end of file
				if (line[0] == '#') continue; // skip comments.
				if (line[0] == ')') { saw_close_brace = true; break; }
				if (o.foreach_mode == foreach_from) {
					o.items.append(line);
				} else {
					o.items.initializeFromString(line);
				}
			}
			if ( ! saw_close_brace) {
				formatstr(errmsg, "Reached end of file without finding closing brace ')'"
					" for Queue command on line %d", item_list_begin_line);
				return -1;
			}
		} else {
			// items from an external source.
			items_are_external = true;
		}
	}

	switch (o.foreach_mode) {
	case foreach_in:
	case foreach_from:
		// itemlist is correct unless items were external
		break;

	case foreach_matching:
	case foreach_matching_files:
	case foreach_matching_dirs:
	case foreach_matching_any:
		items_are_external = true;
		break;

	default:
	case foreach_not:
		break;
	}

	return items_are_external ? 1 : 0;
}


// finish populating the items in a SubmitForeachArgs by reading files and/or globbing.
//
int SubmitHash::load_external_q_foreach_items (
	SubmitForeachArgs & o,                 // OUT: options & items from parsing the queue args
	std::string & errmsg)                  // OUT: error message if return value is not 0
{
	// if no loop variable specified, but a foreach mode is used. use "Item" for the loop variable.
	if (o.vars.isEmpty() && (o.foreach_mode != foreach_not)) { o.vars.append("Item"); }

	// set glob expansion options from submit statements.
	int expand_options = 0;
	if (submit_param_bool("SubmitWarnEmptyMatches", "submit_warn_empty_matches", true)) {
		expand_options |= EXPAND_GLOBS_WARN_EMPTY;
	}
	if (submit_param_bool("SubmitFailEmptyMatches", "submit_fail_empty_matches", false)) {
		expand_options |= EXPAND_GLOBS_FAIL_EMPTY;
	}
	if (submit_param_bool("SubmitWarnDuplicateMatches", "submit_warn_duplicate_matches", true)) {
		expand_options |= EXPAND_GLOBS_WARN_DUPS;
	}
	if (submit_param_bool("SubmitAllowDuplicateMatches", "submit_allow_duplicate_matches", false)) {
		expand_options |= EXPAND_GLOBS_ALLOW_DUPS;
	}
	char* parm = submit_param("SubmitMatchDirectories", "submit_match_directories");
	if (parm) {
		if (MATCH == strcasecmp(parm, "never") || MATCH == strcasecmp(parm, "no") || MATCH == strcasecmp(parm, "false")) {
			expand_options |= EXPAND_GLOBS_TO_FILES;
		} else if (MATCH == strcasecmp(parm, "only")) {
			expand_options |= EXPAND_GLOBS_TO_DIRS;
		} else if (MATCH == strcasecmp(parm, "yes") || MATCH == strcasecmp(parm, "true")) {
			// nothing to do.
		} else {
			errmsg = parm;
			errmsg += " is not a valid value for SubmitMatchDirectories";
			return -1;
		}
		free(parm); parm = NULL;
	}

	if ( ! o.items_filename.empty()) {
		if (o.items_filename == "<") {
			// items should have been loaded already by a call to load_inline_q_foreach_items
		} else if (o.items_filename == "-") {
			int lineno = 0;
			for (char* line=NULL;;) {
				line = getline_trim(stdin, lineno);
				if ( ! line) break;
				if (o.foreach_mode == foreach_from) {
					o.items.append(line);
				} else {
					o.items.initializeFromString(line);
				}
			}
		} else {
			MACRO_SOURCE ItemsSource;
			FILE * fp = Open_macro_source(ItemsSource, o.items_filename.Value(), false, SubmitMacroSet, errmsg);
			if ( ! fp) {
				return -1;
			}
			for (char* line=NULL;;) {
				line = getline_trim(fp, ItemsSource.line);
				if ( ! line) break;
				o.items.append(line);
			}
			Close_macro_source(fp, ItemsSource, SubmitMacroSet, 0);
		}
	}

	int citems = 0;
	switch (o.foreach_mode) {
	case foreach_in:
	case foreach_from:
		// itemlist is already correct
		// PRAGMA_REMIND("do argument validation here?")
		// citems = o.items.number();
		break;

	case foreach_matching:
	case foreach_matching_files:
	case foreach_matching_dirs:
	case foreach_matching_any:
		if (o.foreach_mode == foreach_matching_files) {
			expand_options &= ~EXPAND_GLOBS_TO_DIRS;
			expand_options |= EXPAND_GLOBS_TO_FILES;
		} else if (o.foreach_mode == foreach_matching_dirs) {
			expand_options &= ~EXPAND_GLOBS_TO_FILES;
			expand_options |= EXPAND_GLOBS_TO_DIRS;
		} else if (o.foreach_mode == foreach_matching_any) {
			expand_options &= ~(EXPAND_GLOBS_TO_FILES|EXPAND_GLOBS_TO_DIRS);
		}
		citems = submit_expand_globs(o.items, expand_options, errmsg);
		if ( ! errmsg.empty()) {
			if (citems >= 0) {
				push_warning(stderr, "%s", errmsg.c_str());
			} else {
				push_error(stderr, "%s", errmsg.c_str());
			}
			errmsg.clear();
		}
		if (citems < 0) return citems;
		break;

	default:
	case foreach_not:
		// there is an implicit, single, empty item when the mode is foreach_not
		break;
	}

	return 0; // success
}

// parse a submit file from fp using the given parse_q callback for handling queue statements
int SubmitHash::parse_file(FILE* fp, MACRO_SOURCE & source, std::string & errmsg, FNSUBMITPARSE parse_q /*=NULL*/, void* parse_pv /*=NULL*/)
{
	MACRO_EVAL_CONTEXT ctx = mctx; ctx.use_mask = 2;
	MacroStreamYourFile ms(fp, source);

	return Parse_macros(ms,
		0, SubmitMacroSet, READ_MACROS_SUBMIT_SYNTAX,
		&ctx, errmsg, parse_q, parse_pv);
}

// parse a submit file from memory buffer using the given parse_q callback for handling queue statements
int SubmitHash::parse_mem(MacroStreamMemoryFile &fp, std::string & errmsg, FNSUBMITPARSE parse_q, void* parse_pv)
{
	MACRO_EVAL_CONTEXT ctx = mctx; ctx.use_mask = 2;
	return Parse_macros(fp,
		0, SubmitMacroSet, READ_MACROS_SUBMIT_SYNTAX,
		&ctx, errmsg, parse_q, parse_pv);
}


int SubmitHash::process_q_line(MACRO_SOURCE & source, char* line, std::string & errmsg, FNSUBMITPARSE parse_q, void* parse_pv)
{
	return parse_q(parse_pv, source, SubmitMacroSet, line, errmsg);
}

struct _parse_up_to_q_callback_args { char * line; int source_id; };

// static function to call the class method above....
static int parse_q_callback(void* pv, MACRO_SOURCE& source, MACRO_SET& /*macro_set*/, char * line, std::string & errmsg)
{
	struct _parse_up_to_q_callback_args * pargs = (struct _parse_up_to_q_callback_args *)pv;
	char * queue_args = const_cast<char*>(is_queue_statement(line));
	if ( ! queue_args) {
		// not actually a queue line, so stop parsing and return error
		pargs->line = line;
		return -1;
	}
	if (source.id != pargs->source_id) {
		errmsg = "Queue statement not allowed in include file or command";
		return -5;
	}
	pargs->line = line;
	return 1; // stop scanning, return success
}
int SubmitHash::parse_up_to_q_line(MacroStream &ms, std::string & errmsg, char** qline)
{
	struct _parse_up_to_q_callback_args args = { NULL, ms.source().id };

	*qline = NULL;

	MACRO_EVAL_CONTEXT ctx = mctx; ctx.use_mask = 2;

	PRAGMA_REMIND("move firstread (used by Parse_macros) and at_eof() into MacroStream class")

	int err = Parse_macros(ms,
		0, SubmitMacroSet, READ_MACROS_SUBMIT_SYNTAX,
		&ctx, errmsg, parse_q_callback, &args);
	if (err < 0)
		return err;

	PRAGMA_REMIND("TJ:TODO qline is a pointer to a global (static) variable here, it should be instanced instead.")
	*qline = args.line;
	return 0;
}
int SubmitHash::parse_file_up_to_q_line(FILE* fp, MACRO_SOURCE & source, std::string & errmsg, char** qline)
{
#if 1
	MacroStreamYourFile ms(fp, source);
	return parse_up_to_q_line(ms, errmsg, qline);
#else
	struct _parse_up_to_q_callback_args args = { NULL, source.id };

	*qline = NULL;

	MACRO_EVAL_CONTEXT ctx = mctx; ctx.use_mask = 2;
	MacroStreamYourFile ms(fp, source);

	int err = Parse_macros(ms,
		0, SubmitMacroSet, READ_MACROS_SUBMIT_SYNTAX,
		&ctx, errmsg, parse_q_callback, &args);
	if (err < 0)
		return err;

	*qline = args.line;
	return 0;
#endif
}

void SubmitHash::warn_unused(FILE* out, const char *app)
{
	if ( ! app) app = "condor_submit";

	// Force non-zero ref count for DAG_STATUS and FAILED_COUNT
	// these are specified for all DAG node jobs (see dagman_submit.cpp).
	// wenger 2012-03-26 (refactored by TJ 2015-March)
	increment_macro_use_count("DAG_STATUS", SubmitMacroSet);
	increment_macro_use_count("FAILED_COUNT", SubmitMacroSet);
	increment_macro_use_count("FACTORY.Iwd", SubmitMacroSet);

	HASHITER it = hash_iter_begin(SubmitMacroSet);
	for ( ; !hash_iter_done(it); hash_iter_next(it) ) {
		MACRO_META * pmeta = hash_iter_meta(it);
		if (pmeta && !pmeta->use_count && !pmeta->ref_count) {
			const char *key = hash_iter_key(it);
			if (*key && (*key=='+' || starts_with_ignore_case(key, "MY."))) { continue; }
			if (pmeta->source_id == LiveMacro.id) {
				push_warning(out, "the Queue variable '%s' was unused by %s. Is it a typo?\n", key, app);
			} else {
				const char *val = hash_iter_value(it);
				push_warning(out, "the line '%s = %s' was unused by %s. Is it a typo?\n", key, val, app);
			}
		}
	}
	hash_iter_delete(&it);
}

void SubmitHash::dump(FILE* out, int flags)
{
	HASHITER it = hash_iter_begin(SubmitMacroSet, flags);
	for ( ; ! hash_iter_done(it); hash_iter_next(it)) {
		const char * key = hash_iter_key(it);
		if (key && key[0] == '$') continue; // dont dump meta params.
		const char * val = hash_iter_value(it);
		//fprintf(out, "%s%s = %s\n", it.is_def ? "d " : "  ", key, val ? val : "NULL");
		fprintf(out, "  %s = %s\n", key, val ? val : "NULL");
	}
	hash_iter_delete(&it);
}

const char* SubmitHash::to_string(std::string & out, int flags)
{
	out.reserve(SubmitMacroSet.size * 80); // make a guess at how much space we need.

	HASHITER it = hash_iter_begin(SubmitMacroSet, flags);
	for ( ; ! hash_iter_done(it); hash_iter_next(it)) {
		const char * key = hash_iter_key(it);
		if (key && key[0] == '$') continue; // dont dump meta params.
		const char * val = hash_iter_value(it);
		out += key;
		out += "=";
		if (val) { out += val; }
		out += "\n";
	}
	hash_iter_delete(&it);
	return out.c_str();
}

const char* SubmitHash::make_digest(std::string & out, int cluster_id, StringList & vars, int /*options*/)
{
	int flags = HASHITER_NO_DEFAULTS;
	out.reserve(SubmitMacroSet.size * 80); // make a guess at how much space we need.

	std::string rhs;
	classad::References skip_knobs;
	skip_knobs.insert("Process");
	skip_knobs.insert("ProcId");
	skip_knobs.insert("Step");
	skip_knobs.insert("Row");
	skip_knobs.insert("Node");
	skip_knobs.insert("Item");
	if ( ! vars.isEmpty()) {
		for (const char * var = vars.first(); var != NULL; var = vars.next()) {
			skip_knobs.insert(var);
		}
	}

	if (cluster_id > 0) {
		(void)sprintf(LiveClusterString, "%d", cluster_id);
	} else {
		skip_knobs.insert("Cluster");
		skip_knobs.insert("ClusterId");
	}

	HASHITER it = hash_iter_begin(SubmitMacroSet, flags);
	for ( ; ! hash_iter_done(it); hash_iter_next(it)) {
		const char * key = hash_iter_key(it);
		if (key && key[0] == '$') continue; // dont dump meta params.
		const char * val = hash_iter_value(it);
		out += key;
		out += "=";
		if (val) {
			rhs = val;
			selective_expand_macro(rhs, skip_knobs, SubmitMacroSet, mctx);
			out += rhs;
		}
		out += "\n";
	}
	hash_iter_delete(&it);
	return out.c_str();
}


