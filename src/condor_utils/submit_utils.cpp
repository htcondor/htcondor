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
#include "condor_string.h"
#include "spooled_job_files.h" // for GetSpooledExecutablePath()
#include "basename.h"
#include "condor_getcwd.h"
#include "condor_classad.h"
#include "condor_attributes.h"
#include "condor_adtypes.h"
#include "domain_tools.h"
#include "sig_install.h"
#include "daemon.h"
#include "string_list.h"
#include "which.h"
#include "sig_name.h"
#include "print_wrapped_text.h"
#include "my_username.h" // for my_domainname
#include "globus_utils.h" // for 
#include "enum_utils.h" // for shouldtransferfiles_t
#include "directory.h"
#include "filename_tools.h"
#include "fs_util.h"
#include "condor_crontab.h"
#include "file_transfer.h"
#include "condor_holdcodes.h"
#include "condor_url.h"
#include "condor_version.h"
#include "NegotiationUtils.h"
#include "param_info.h" // for BinaryLookup
#include "submit_utils.h"

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

/* Disable gcc warnings about floating point comparisons */
GCC_DIAG_OFF(float-equal)

#define ABORT_AND_RETURN(v) abort_code=v; return abort_code
#define RETURN_IF_ABORT() if (abort_code) return abort_code

#define exit(n)  poison_exit(n)

// When this class is wrapped around a classad that has a chained parent ad
// inserts and assignments will check to see if the value being assigned
// is the same as the value in the chained parent, and if so will NOT do
// the assigment, but let the parents value show through.
//
// This has the effect of leaving the ad containing only the differences from the parent
//
// Thus if this class is wrapped around a job ad chained to a cluster ad, the job ad
// will contain only those values that should be sent to the procAd in the schedd.
class DeltaClassAd
{
public:
	DeltaClassAd(ClassAd & _ad) : ad(_ad) {}
	virtual ~DeltaClassAd() {};

	bool Insert(const std::string & attr, ExprTree * tree);
	bool Assign(const char* attr, bool val);
	bool Assign(const char* attr, double val);
	bool Assign(const char* attr, long long val);
	bool Assign(const char* attr, const char * val);

	ExprTree * LookupExpr(const char * attr) { return ad.LookupExpr(attr); }
	ExprTree * Lookup(const std::string & attr) { return ad.Lookup(attr); }
	int LookupString(const char * attr, MyString & val) { return ad.LookupString(attr, val); }
	int LookupString(const char * attr, std::string & val) { return ad.LookupString(attr, val); }
	int LookupBool(const char * attr, bool & val) { return ad.LookupBool(attr, val); }

protected:
	ClassAd& ad;

	ExprTree * HasParentTree(const std::string & attr, classad::ExprTree::NodeKind kind);
	const classad::Value * HasParentValue(const std::string & attr, classad::Value::ValueType vt);
};

// returns the expr tree from the parent ad if it is of the given node kind.
// otherwise returns NULL.
ExprTree * DeltaClassAd::HasParentTree(const std::string & attr, classad::ExprTree::NodeKind kind)
{
	classad::ClassAd * parent = ad.GetChainedParentAd();
	if (parent) {
		ExprTree * expr = parent->Lookup(attr);
		if (expr) {
			expr = SkipExprEnvelope(expr);
			if (kind == expr->GetKind()) {
				return expr;
			}
		}
	}
	return NULL;
}

// returns a pointer to the value from the parent ad if the parent ad has a Literal node
// of the given value type.
const classad::Value * DeltaClassAd::HasParentValue(const std::string & attr, classad::Value::ValueType vt)
{
	ExprTree * expr = HasParentTree(attr, ExprTree::NodeKind::LITERAL_NODE);
	if ( ! expr)
		return NULL;
	classad::Value::NumberFactor f;
	const classad::Value * pval = &dynamic_cast<classad::Literal*>(expr)->getValue(f);
	if (pval->GetType() != vt)
		return NULL;
	return pval;
}

bool DeltaClassAd::Insert(const std::string & attr, ExprTree * tree)
{
	ExprTree * t2 = HasParentTree(attr, tree->GetKind());
	if (t2 && tree->SameAs(t2)) {
		delete tree;
		ad.PruneChildAttr(attr, false);
		return true;
	}
	return ad.Insert(attr, tree);
}

bool DeltaClassAd::Assign(const char* attr, bool val)
{
	bool bval = ! val;
	const classad::Value * pval = HasParentValue(attr, classad::Value::BOOLEAN_VALUE);
	if (pval && pval->IsBooleanValue(bval) && (val == bval)) {
		ad.PruneChildAttr(attr, false);
		return true;
	}
	return ad.Assign(attr, val);
}

bool DeltaClassAd::Assign(const char* attr, double val)
{
	double dval = -val;
	const classad::Value * pval = HasParentValue(attr, classad::Value::REAL_VALUE);
	if (pval && pval->IsRealValue(dval) && (val == dval)) {
		ad.PruneChildAttr(attr, false);
		return true;
	}
	return ad.Assign(attr, val);
}

bool DeltaClassAd::Assign(const char* attr, long long val)
{
	long long ival = -val;
	const classad::Value * pval = HasParentValue(attr, classad::Value::INTEGER_VALUE);
	if (pval && pval->IsIntegerValue(ival) && (val == ival)) {
		ad.PruneChildAttr(attr, false);
		return true;
	}
	return ad.Assign(attr, val);
}

bool DeltaClassAd::Assign(const char* attr, const char * val)
{
	if ( ! val) {
		const classad::Value * pval = HasParentValue(attr, classad::Value::UNDEFINED_VALUE);
		if (pval) {
			ad.PruneChildAttr(attr, false);
			return true;
		}
	} else {
		const char * cstr = NULL;
		const classad::Value * pval = HasParentValue(attr, classad::Value::STRING_VALUE);
		if (pval && pval->IsStringValue(cstr) && cstr && (MATCH == strcmp(cstr, val))) {
			ad.PruneChildAttr(attr, false);
			return true;
		}
	}
	return ad.Assign(attr, val);
}


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
		|| MATCH == strcasecmp(name, SUBMIT_KEY_RequestMemory)
		|| MATCH == strcasecmp(name, "request_cpu") // by putting this here, we prevent the CPU (i.e. not the plural) from quietly becoming an additional custom resource
		;
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

/* order dependencies
ComputeRootDir >> ComputeIWD
ComputeIWD >> FixupTransferInputFiles

SetUniverse >> SetArguments SetCronTab SetExecutable SetGSICredentials SetGridParams SetImageSize SetJobDeferral SetJobLease SetKillSig SetMachineCount SetMaxJobRetirementTime SetRank SetRequirements SetTransferFiles SetVMParams

SetPriority >> SetMaxJobRetirementTime

SetTDP >> SetTransferFiles
SetStderr >> SetTransferFiles
SetStdout >> SetTransferFiles

SetTransferFiles >> SetImageSize

SetCronTab >> SetJobDeferral

SetPerFileEncryption >> SetRequirements
SetMachineCount >> SetRequirements
SetImageSize >> SetRequirements
SetTransferFiles >> SetRequirements
SetEncryptExecuteDir >> SetRequirements

*/

SubmitHash::SubmitHash()
	: clusterAd(NULL)
	, procAd(NULL)
	, job(NULL)
	, submit_time(0)
	, abort_code(0)
	, abort_macro_name(NULL)
	, abort_raw_macro_val(NULL)
	, base_job_is_cluster_ad(false)
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
	, JobUniverse(CONDOR_UNIVERSE_MIN)
	, JobIwdInitialized(false)
	, IsDockerJob(false)
	, JobDisableFileChecks(false)
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
	setup_macro_defaults();

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
	vsprintf ( message, format, ap );
	va_end(ap);

	if (SubmitMacroSet.errors) {
		SubmitMacroSet.errors->push("Submit", 0, message);
	} else {
		fprintf(fh, "\nWARNING: %s", message);
	}
	free(message);
}

// used to declare tables for use with submit_param calls and their associated AssignJob calls
struct SimpleExprInfo {
	char const *key;  // primary submit key
	char const *alt;  // alternate submit key
	char const *attr; // job attribute
	char const *default_value; // default value
	bool quote_it;    // whether to quote as a string before parsing.
};

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
		realcwd = submit_param_mystring("FACTORY.Iwd", NULL);
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

void SubmitHash::unset_live_submit_variable(const char *name)
{
	MACRO_ITEM* pitem = find_macro_item(name, NULL, SubmitMacroSet);
	if (pitem) { pitem->raw_value = UnsetString; }
}


void SubmitHash::set_submit_param_used( const char *name ) 
{
	increment_macro_use_count(name, SubmitMacroSet);
}


int SubmitHash::AssignJobExpr (const char * attr, const char *expr, const char * source_label /*=NULL*/)
{
	ExprTree *tree = NULL;
	if (ParseClassAdRvalExpr(expr, tree)!=0 || ! tree) {
		push_error(stderr, "Parse error in expression: \n\t%s = %s\n\t", attr, expr);
		if ( ! SubmitMacroSet.errors) {
			fprintf(stderr,"Error in %s\n", source_label ? source_label : "submit file");
		}
		ABORT_AND_RETURN( 1 );
	}

	if (!job->Insert (attr, tree)) {
		push_error(stderr, "Unable to insert expression: %s = %s\n", attr, expr);
		ABORT_AND_RETURN( 1 );
	}

	return 0;
}

bool SubmitHash::AssignJobString(const char * attr, const char * val)
{
	ASSERT(attr);
	ASSERT(val);

	if ( ! job->Assign(attr, val)) {
		push_error(stderr, "Unable to insert expression: %s = \"%s\"\n", attr, val);
		abort_code = 1;
		return false;
	}

	return true;
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
		AssignJobString(ATTR_ENCRYPT_INPUT_FILES, files.ptr());
	}
	RETURN_IF_ABORT();

	files.set(submit_param( SUBMIT_KEY_EncryptOutputFiles, "EncryptOutputFiles"));
	if (files) {
		AssignJobString( ATTR_ENCRYPT_OUTPUT_FILES, files.ptr());
	}
	RETURN_IF_ABORT();

	files.set(submit_param( SUBMIT_KEY_DontEncryptInputFiles, "DontEncryptInputFiles"));
	if (files) {
		AssignJobString( ATTR_DONT_ENCRYPT_INPUT_FILES, files.ptr());
	}
	RETURN_IF_ABORT();

	files.set(submit_param( SUBMIT_KEY_DontEncryptOutputFiles, "DontEncryptOutputFiles"));
	if (files) {
		AssignJobString( ATTR_DONT_ENCRYPT_OUTPUT_FILES, files.ptr());
	}
	RETURN_IF_ABORT();

	return 0;
}


int SubmitHash::SetFetchFiles()
{
	RETURN_IF_ABORT();

	auto_free_ptr files(submit_param(SUBMIT_KEY_FetchFiles, ATTR_FETCH_FILES ));
	if (files) {
		AssignJobString(ATTR_FETCH_FILES, files);
	}

	return abort_code;
}

int SubmitHash::SetCompressFiles()
{
	RETURN_IF_ABORT();

	auto_free_ptr files(submit_param(SUBMIT_KEY_CompressFiles, ATTR_COMPRESS_FILES));
	if (files) {
		AssignJobString(ATTR_COMPRESS_FILES, files);
	}

	return 0;
}

int SubmitHash::SetAppendFiles()
{
	RETURN_IF_ABORT();

	auto_free_ptr files(submit_param(SUBMIT_KEY_AppendFiles, ATTR_APPEND_FILES));
	if (files) {
		AssignJobString(ATTR_APPEND_FILES, files);
	}

	return 0;
}

int SubmitHash::SetLocalFiles()
{
	RETURN_IF_ABORT();

	auto_free_ptr files(submit_param(SUBMIT_KEY_LocalFiles, ATTR_LOCAL_FILES));
	if (files) {
		AssignJobString(ATTR_LOCAL_FILES, files);
	}

	return 0;
}

int SubmitHash::SetJarFiles()
{
	RETURN_IF_ABORT();

	auto_free_ptr files(submit_param(SUBMIT_KEY_JarFiles, ATTR_JAR_FILES));
	if (files) {
		AssignJobString(ATTR_JAR_FILES, files);
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
	bool allow_arguments_v1 = submit_param_bool(SUBMIT_CMD_AllowArgumentsV1, NULL, false);

	if(args1_ext && args1) {
		push_error(stderr, "you specified a value for both " SUBMIT_KEY_JavaVMArgs " and " SUBMIT_KEY_JavaVMArguments1 ".\n");
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
			AssignJobString(ATTR_JOB_JAVA_VM_ARGS1, value.c_str());
		}
	}
	else {
		args_success = args.GetArgsStringV2Raw(&value,&error_msg);
		if(!value.IsEmpty()) {
			AssignJobString(ATTR_JOB_JAVA_VM_ARGS2, value.c_str());
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

	auto_free_ptr value(submit_param(SUBMIT_KEY_ParallelScriptShadow, ATTR_PARALLEL_SCRIPT_SHADOW));
	if (value) {
		AssignJobString(ATTR_PARALLEL_SCRIPT_SHADOW, value);
	}
	value.set(submit_param(SUBMIT_KEY_ParallelScriptStarter, ATTR_PARALLEL_SCRIPT_STARTER));
	if (value) {
		AssignJobString(ATTR_PARALLEL_SCRIPT_STARTER, value);
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

	if (FnCheckFile) {
		FnCheckFile(CheckFileArg, this, role, strPathname.Value(), flags);
	}
	return 0;
}


// Check that a stdin, stdout or stderr submit settings and files are valid
//
int SubmitHash::CheckStdFile(
	_submit_file_role role,
	const char * value, // in: filename to use, may be NULL
	int access,         // in: desired access if checking for file accessiblity
	MyString & file,    // out: filename, possibly fixed up.
	bool & transfer_it, // in,out: whether we expect to transfer it or not
	bool & stream_it)   // in,out: whether we expect to stream it or not
{
	file = value;
	if (file.empty())
	{
		transfer_it = false;
		stream_it = false;
		// always canonicalize to the UNIX null file (i.e. /dev/null)
		file = UNIX_NULL_FILE;
	} else if (file == UNIX_NULL_FILE) {
		transfer_it = false;
		stream_it = false;
	} else {
		if( JobUniverse == CONDOR_UNIVERSE_VM ) {
			push_error(stderr, "You cannot use input, ouput, "
					"and error parameters in the submit description "
					"file for vm universe\n");
			ABORT_AND_RETURN( 1 );
		}

		/* Globus jobs are allowed to specify urls */
		if (JobUniverse == CONDOR_UNIVERSE_GRID && is_globus_friendly_url(file.c_str())) {
			transfer_it = false;
			stream_it = false;
		} else {
			if (check_and_universalize_path(file) != 0) {
				ABORT_AND_RETURN( 1 );
			}

			if (transfer_it && ! JobDisableFileChecks) {
				check_open(role, file.Value(), access);
				RETURN_IF_ABORT();
			}
		}
	}
	return 0;
}

int SubmitHash::SetStdin()
{
	bool transfer_it = submit_param_bool(SUBMIT_KEY_TransferInput, ATTR_TRANSFER_INPUT, true);
	bool stream_it = submit_param_bool(SUBMIT_KEY_StreamInput, ATTR_STREAM_INPUT, false);
	auto_free_ptr value(submit_param(SUBMIT_KEY_Input, SUBMIT_KEY_Stdin));
	MyString file;
	if (CheckStdFile(SFR_INPUT, value, O_RDONLY, file, transfer_it, stream_it) != 0) {
		ABORT_AND_RETURN( 1 );
	}

	AssignJobString(ATTR_JOB_INPUT, file.c_str());
	RETURN_IF_ABORT();

	if (transfer_it) {
		AssignJobVal(ATTR_STREAM_INPUT, stream_it);
	} else {
		AssignJobVal(ATTR_TRANSFER_INPUT, false);
	}
	return 0;
}

int SubmitHash::SetStdout()
{
	bool transfer_it = submit_param_bool(SUBMIT_KEY_TransferOutput, ATTR_TRANSFER_OUTPUT, true);
	bool stream_it = submit_param_bool(SUBMIT_KEY_StreamOutput, ATTR_STREAM_OUTPUT, false);
	auto_free_ptr value(submit_param(SUBMIT_KEY_Output, SUBMIT_KEY_Stdout));
	MyString file;
	if (CheckStdFile(SFR_STDOUT, value, O_WRONLY|O_CREAT|O_TRUNC, file, transfer_it, stream_it) != 0) {
		ABORT_AND_RETURN( 1 );
	}

	AssignJobString(ATTR_JOB_OUTPUT, file.c_str());
	RETURN_IF_ABORT();

	if (transfer_it) {
		AssignJobVal(ATTR_STREAM_OUTPUT, stream_it);
	} else {
		AssignJobVal(ATTR_TRANSFER_OUTPUT, false);
	}
	return 0;
}

int SubmitHash::SetStderr()
{
	bool transfer_it = submit_param_bool(SUBMIT_KEY_TransferError, ATTR_TRANSFER_ERROR, true);
	bool stream_it = submit_param_bool(SUBMIT_KEY_StreamError, ATTR_STREAM_ERROR, false);
	auto_free_ptr value(submit_param(SUBMIT_KEY_Error, SUBMIT_KEY_Stderr));
	MyString file;
	if (CheckStdFile(SFR_STDERR, value, O_WRONLY|O_CREAT|O_TRUNC, file, transfer_it, stream_it) != 0) {
		ABORT_AND_RETURN( 1 );
	}

	AssignJobString(ATTR_JOB_ERROR, file.c_str());
	RETURN_IF_ABORT();

	if (transfer_it) {
		AssignJobVal(ATTR_STREAM_ERROR, stream_it);
	} else {
		AssignJobVal(ATTR_TRANSFER_ERROR, false);
	}
	return 0;
}


class SubmitHashEnvFilter : public Env
{
public:
	SubmitHashEnvFilter( bool env1, bool env2 )
			: m_env1( env1 ),
			  m_env2( env2 ) {
		return;
	};
	virtual ~SubmitHashEnvFilter( void ) { };
	virtual bool ImportFilter( const MyString &var,
							   const MyString &val ) const;
private:
	bool m_env1;
	bool m_env2;
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
#ifdef WIN32
	// on Windows, we have to do case-insensitive check for existance, luckily
	// we have m_sorted_varnames for just this purpose
	if (m_sorted_varnames.find(var.Value()) != m_sorted_varnames.end())
#else
	MyString existing_val;
	if ( GetEnv( var, existing_val ) )
#endif
	{
		// Don't override submit file environment settings --
		// check if environment variable is already set.
		return false;
	}
	return true;
}

int SubmitHash::SetEnvironment()
{
	RETURN_IF_ABORT();

	// Most users will specify "environment" or "env" which is v1 or v2 quoted
	// It is permitted to specify "environment2" which must be v2, and it is also allowed
	// to specify both (for maximum backward compatibility)
	auto_free_ptr env1(submit_param(SUBMIT_KEY_Environment1, ATTR_JOB_ENVIRONMENT1));
	auto_free_ptr env2(submit_param(SUBMIT_KEY_Environment2)); // no alt keyword for env2
	bool allow_v1 = submit_param_bool( SUBMIT_CMD_AllowEnvironmentV1, NULL, false );

	RETURN_IF_ABORT();
	if( env1 && env2 && !allow_v1 ) {
		push_error(stderr, "If you wish to specify both 'environment' and\n"
		 "'environment2' for maximal compatibility with different\n"
		 "versions of Condor, then you must also specify\n"
		 "allow_environment_v1=true.\n");
		ABORT_AND_RETURN(1);
	}

	SubmitHashEnvFilter envobject(env1, env2);

	MyString error_msg;
	bool env_success = true; // specifying no env is allowed.

	const ClassAd * clusterAd = get_cluster_ad();
	if (clusterAd) {
		if ( ! env1 && ! env2) {
			// If we have a cluster ad, and no environment keywords
			// we just use whatever the clusterad has.
			return 0;
		}
		// if there is a cluster ad, initialize from it
		// this will happen when we are materializing jobs in the Schedd
		// but not when materializing in condor_submit (at present)
		env_success = envobject.MergeFrom(clusterAd, &error_msg);
	}

	if (env2) {
		env_success = envobject.MergeFromV2Quoted(env2,&error_msg);
	} else if (env1) {
		env_success = envobject.MergeFromV1RawOrV2Quoted(env1,&error_msg);
	}
	if ( ! env_success) {
		push_error(stderr, "%s\nThe environment you specified was: '%s'\n",
				error_msg.Value(),
				env2 ? env2.ptr() : env1.ptr());
		ABORT_AND_RETURN(1);
	}

		// For standard universe, we set a special environment variable to tell it to skip the
	// check that the exec was linked with the standard universe runtime. The usual reason
	// to do that is that the executable is a script
	if (JobUniverse == CONDOR_UNIVERSE_STANDARD) {
		if (submit_param_bool(SUBMIT_CMD_AllowStartupScript, SUBMIT_CMD_AllowStartupScriptAlt, false)) {
			envobject.SetEnv("_CONDOR_NOCHECK", "1");
		}
	}

	// if getenv == TRUE, merge the variables from the user's environment that
	// are not already set in the envobject
	if (submit_param_bool(SUBMIT_CMD_GetEnvironment, SUBMIT_CMD_GetEnvironmentAlt, false)) {
		envobject.Import( );
	}

	// There may already be environment info in the ClassAd from SUBMIT_ATTRS.
	// or in the chained parent ad when doing late materialization.
	// Check for that now.

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
		MyString newenv_raw;

		env_success = envobject.getDelimitedStringV1Raw(&newenv_raw,&error_msg);
		AssignJobString(ATTR_JOB_ENVIRONMENT1, newenv_raw.c_str());

		// Record in the JobAd the V1 delimiter that is being used.
		// This way remote submits across platforms have a prayer.
		char delim[2];
		delim[0] = envobject.GetEnvV1Delimiter();
		delim[1] = 0;
		AssignJobString(ATTR_JOB_ENVIRONMENT1_DELIM, delim);
	}

	if(insert_env2 && env_success) {
		MyString newenv_raw;

		env_success = envobject.getDelimitedStringV2Raw(&newenv_raw,&error_msg);
		AssignJobString(ATTR_JOB_ENVIRONMENT2, newenv_raw.c_str());
	}

	if(!env_success) {
		push_error(stderr, "failed to insert environment into job ad: %s\n",
				error_msg.Value());
		ABORT_AND_RETURN(1);
	}

	return 0;
}

int SubmitHash::SetEncryptExecuteDir()
{
	RETURN_IF_ABORT();
	bool encrypt_it = submit_param_bool( SUBMIT_KEY_EncryptExecuteDir, ATTR_ENCRYPT_EXECUTE_DIRECTORY, false );
	RETURN_IF_ABORT();

	AssignJobVal(ATTR_ENCRYPT_EXECUTE_DIRECTORY, encrypt_it);
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
	bool allow_arguments_v1 = submit_param_bool(SUBMIT_CMD_AllowArgumentsV1, NULL, false);
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
		path = tdp_cmd.ptr();
		check_and_universalize_path( path );
		AssignJobString(ATTR_TOOL_DAEMON_CMD, path.Value());
	}
	if( tdp_input ) {
		path = tdp_input.ptr();
		check_and_universalize_path( path );
		AssignJobString(ATTR_TOOL_DAEMON_INPUT, path.Value());
	}
	if( tdp_output ) {
		path = tdp_output;
		check_and_universalize_path( path );
		AssignJobString(ATTR_TOOL_DAEMON_OUTPUT, path.Value());
		free( tdp_output );
		tdp_output = NULL;
	}
	if( tdp_error ) {
		path = tdp_error;
		check_and_universalize_path( path );
		AssignJobString(ATTR_TOOL_DAEMON_ERROR, path.Value());
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
			AssignJobString(ATTR_TOOL_DAEMON_ARGS1, args_value.c_str());
		}
	}
	else if(args.Count()) {
		args_success = args.GetArgsStringV2Raw(&args_value,&error_msg);
		if(!args_value.IsEmpty()) {
			AssignJobString(ATTR_TOOL_DAEMON_ARGS2, args_value.c_str());
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
		push_error(stderr, SUBMIT_KEY_Preferences " and " SUBMIT_KEY_Rank " may not both be specified for a job\n");
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
		AssignJobVal(ATTR_RANK, 0.0);
	} else {
		AssignJobExpr(ATTR_RANK, rank.Value());
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

	static const SimpleExprInfo logs[] = {
		/*submit_param*/ {SUBMIT_KEY_UserLogFile, ATTR_ULOG_FILE,
				ATTR_ULOG_FILE, NULL, true},
		/*submit_param*/ {SUBMIT_KEY_DagmanLogFile, ATTR_DAGMAN_WORKFLOW_LOG,
				ATTR_DAGMAN_WORKFLOW_LOG, NULL, true},
		{NULL,NULL,NULL,NULL,false}
	};

	for(const SimpleExprInfo * si = &logs[0]; si->key; ++si) {
		char *ulog_entry = submit_param( si->key, si->alt );

		if ( ulog_entry && strcmp( ulog_entry, "" ) != 0 ) {

				// Note:  I don't think the return value here can ever
				// be NULL.  wenger 2016-10-07
			MyString mulog(full_path(ulog_entry));
			if ( ! mulog.empty()) {
				if (FnCheckFile) {
					int rval = FnCheckFile(CheckFileArg, this, SFR_LOG, mulog.Value(), O_APPEND);
					if (rval) { ABORT_AND_RETURN( rval ); }
				}

				check_and_universalize_path(mulog);
			}
			AssignJobString(si->attr, mulog.Value());
			free(ulog_entry);
		}
	}

	RETURN_IF_ABORT();
	bool xml_exists;
	bool use_xml = submit_param_bool(SUBMIT_KEY_UserLogUseXML, ATTR_ULOG_USE_XML, false, &xml_exists);
	if (xml_exists) {
		AssignJobVal(ATTR_ULOG_USE_XML, use_xml);
	}

	return 0;
}


int SubmitHash::SetCoreSize()
{
	RETURN_IF_ABORT();
	char *size = submit_param( ATTR_CORE_SIZE, SUBMIT_KEY_CoreSize ); // attr before submit key for backward compat
	RETURN_IF_ABORT();

	long coresize = 0;

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

	AssignJobVal(ATTR_CORE_SIZE, coresize);
	return 0;
}


int SubmitHash::SetJobLease()
{
	RETURN_IF_ABORT();

	long lease_duration = 0;
	auto_free_ptr tmp(submit_param( SUBMIT_KEY_JobLeaseDuration, ATTR_JOB_LEASE_DURATION ));
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
		AssignJobExpr(ATTR_JOB_LEASE_DURATION, tmp.ptr());
	}
	return 0;
}


int SubmitHash::SetJobStatus()
{
	RETURN_IF_ABORT();

	bool hold = submit_param_bool( SUBMIT_KEY_Hold, NULL, false );
	MyString buffer;

	if (hold) {
		if ( IsRemoteJob ) {
			push_error(stderr, "Cannot set " SUBMIT_KEY_Hold " to 'true' when using -remote or -spool\n");
			ABORT_AND_RETURN( 1 );
		}
		AssignJobVal(ATTR_JOB_STATUS, HELD);
		AssignJobVal(ATTR_HOLD_REASON_CODE, CONDOR_HOLD_CODE_SubmittedOnHold);

		AssignJobString(ATTR_HOLD_REASON, "submitted on hold at user's request");
	} else 
	if ( IsRemoteJob ) {
		AssignJobVal(ATTR_JOB_STATUS, HELD);
		AssignJobVal(ATTR_HOLD_REASON_CODE, CONDOR_HOLD_CODE_SpoolingInput);

		AssignJobString(ATTR_HOLD_REASON, "Spooling input data files");
	} else {
		AssignJobVal(ATTR_JOB_STATUS, IDLE);
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
	return 0;
}

int SubmitHash::SetNiceUser()
{

	bool is_nice = submit_param_bool( SUBMIT_KEY_NiceUser, ATTR_NICE_USER, false );
	RETURN_IF_ABORT();

	AssignJobVal(ATTR_NICE_USER, is_nice);

	// Nice users get a default MaxJobRetirementTime of 0
	if (is_nice && ! job->Lookup(ATTR_MAX_JOB_RETIREMENT_TIME)) {
		// we assign a default of 0 here, but a user-specified value will overwrite this
		// when SetMaxJobRetirementTime is called.
		AssignJobVal(ATTR_MAX_JOB_RETIREMENT_TIME, 0);
	}

	return 0;
}

int SubmitHash::SetMaxJobRetirementTime()
{
	RETURN_IF_ABORT();

	auto_free_ptr value(submit_param(SUBMIT_KEY_MaxJobRetirementTime, ATTR_MAX_JOB_RETIREMENT_TIME));
	if (value) {
		AssignJobExpr(ATTR_MAX_JOB_RETIREMENT_TIME, value.ptr());
	} else if (JobUniverse == CONDOR_UNIVERSE_STANDARD) {
		// Regardless of the startd graceful retirement policy,
		// nice_user and standard universe jobs that do not specify
		// otherwise will self-limit their retirement time to 0.  So
		// the user plays nice by default, but they have the option to
		// override this (assuming, of course, that the startd policy
		// actually gives them any retirement time to play with).
		AssignJobVal(ATTR_MAX_JOB_RETIREMENT_TIME, 0);
	}
	return 0;
}

int SubmitHash::SetPeriodicHoldCheck()
{
	RETURN_IF_ABORT();

	char *phc = submit_param(SUBMIT_KEY_PeriodicHoldCheck, ATTR_PERIODIC_HOLD_CHECK);
	if (phc == NULL)
	{
		/* user didn't have one, so add one */
		AssignJobVal(ATTR_PERIODIC_HOLD_CHECK, false);
	}
	else
	{
		/* user had a value for it, leave it alone */
		AssignJobExpr(ATTR_PERIODIC_HOLD_CHECK, phc);
		free(phc);
	}

	phc = submit_param(SUBMIT_KEY_PeriodicHoldReason, ATTR_PERIODIC_HOLD_REASON);
	if( phc ) {
		AssignJobExpr(ATTR_PERIODIC_HOLD_REASON, phc);
		free(phc);
	}

	phc = submit_param(SUBMIT_KEY_PeriodicHoldSubCode, ATTR_PERIODIC_HOLD_SUBCODE);
	if( phc ) {
		AssignJobExpr(ATTR_PERIODIC_HOLD_SUBCODE, phc);
		free(phc);
	}

	phc = submit_param(SUBMIT_KEY_PeriodicReleaseCheck, ATTR_PERIODIC_RELEASE_CHECK);
	if (phc == NULL)
	{
		/* user didn't have one, so add one */
		AssignJobVal(ATTR_PERIODIC_RELEASE_CHECK, false);
	}
	else
	{
		/* user had a value for it, leave it alone */
		AssignJobExpr(ATTR_PERIODIC_RELEASE_CHECK, phc);
		free(phc);
	}

	RETURN_IF_ABORT();

	return 0;
}

int SubmitHash::SetPeriodicRemoveCheck()
{
	RETURN_IF_ABORT();

	char *prc = submit_param(SUBMIT_KEY_PeriodicRemoveCheck, ATTR_PERIODIC_REMOVE_CHECK);

	if (prc == NULL)
	{
		/* user didn't have one, so add one */
		AssignJobVal(ATTR_PERIODIC_REMOVE_CHECK, false);
	}
	else
	{
		/* user had a value for it, leave it alone */
		AssignJobExpr(ATTR_PERIODIC_REMOVE_CHECK, prc);
		free(prc);
	}

	prc = submit_param(SUBMIT_KEY_OnExitHoldReason, ATTR_ON_EXIT_HOLD_REASON);
	if( prc ) {
		AssignJobExpr(ATTR_ON_EXIT_HOLD_REASON, prc);
		free(prc);
	}

	prc = submit_param(SUBMIT_KEY_OnExitHoldSubCode, ATTR_ON_EXIT_HOLD_SUBCODE);
	if( prc ) {
		AssignJobExpr(ATTR_ON_EXIT_HOLD_SUBCODE, prc);
		free(prc);
	}

	RETURN_IF_ABORT();

	return 0;
}


int SubmitHash::SetLeaveInQueue()
{
	RETURN_IF_ABORT();

	char *erc = submit_param(SUBMIT_KEY_LeaveInQueue, ATTR_JOB_LEAVE_IN_QUEUE);
	MyString buffer;

	if (erc == NULL)
	{
		/* user didn't have one, so add one */
		if ( ! IsRemoteJob) {
			AssignJobVal(ATTR_JOB_LEAVE_IN_QUEUE, false );
		} else {
				/* if remote spooling, leave in the queue after the job completes
				   for up to 10 days, so user can grab the output.
				 */
			buffer.formatstr( 
				"%s == %d && (%s =?= UNDEFINED || %s == 0 || ((time() - %s) < %d))",
				ATTR_JOB_STATUS,
				COMPLETED,
				ATTR_COMPLETION_DATE,
				ATTR_COMPLETION_DATE,
				ATTR_COMPLETION_DATE,
				60 * 60 * 24 * 10 
				);
			AssignJobExpr(ATTR_JOB_LEAVE_IN_QUEUE, buffer.c_str());
		}
	}
	else
	{
		/* user had a value for it, leave it alone */
		AssignJobExpr(ATTR_JOB_LEAVE_IN_QUEUE, erc);
		free(erc);
	}

	RETURN_IF_ABORT();

	return 0;
}


int SubmitHash::SetNoopJob()
{
	RETURN_IF_ABORT();
	MyString buffer;

	auto_free_ptr noop(submit_param(SUBMIT_KEY_Noop, ATTR_JOB_NOOP));
	if (noop) {
		/* user had a value for it, leave it alone */
		AssignJobExpr(ATTR_JOB_NOOP, noop.ptr());
		RETURN_IF_ABORT();
	}

	noop.set(submit_param(SUBMIT_KEY_NoopExitSignal, ATTR_JOB_NOOP_EXIT_SIGNAL));
	if (noop) {
		AssignJobExpr(ATTR_JOB_NOOP_EXIT_SIGNAL, noop.ptr());
		RETURN_IF_ABORT();
	}

	noop.set(submit_param(SUBMIT_KEY_NoopExitCode, ATTR_JOB_NOOP_EXIT_CODE));
	if (noop) {
		AssignJobExpr(ATTR_JOB_NOOP_EXIT_CODE, noop.ptr());
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

int SubmitHash::ComputeRootDir()
{
	RETURN_IF_ABORT();

	JobRootdir = submit_param_mystring( SUBMIT_KEY_RootDir, ATTR_JOB_ROOT_DIR );
	if( JobRootdir.empty() )
	{
		JobRootdir = "/";
	}

	return 0;
}

int SubmitHash::check_root_dir_access()
{
	if ( ! JobRootdir.empty() && JobRootdir != "/")
	{
		if( access(JobRootdir.c_str(), F_OK|X_OK) < 0 ) {
			push_error(stderr, "No such directory: %s\n", JobRootdir.c_str());
			ABORT_AND_RETURN( 1 );
		}
	}
	return 0;
}


int SubmitHash::SetRootDir()
{
	RETURN_IF_ABORT();
	if (ComputeRootDir()) { ABORT_AND_RETURN(1); }
	AssignJobString (ATTR_JOB_ROOT_DIR, JobRootdir.c_str());
	return 0;
}


#endif

const char * SubmitHash::getIWD()
{
	ASSERT(JobIwdInitialized);
	return JobIwd.c_str();
}

int SubmitHash::ComputeIWD()
{
	char	*shortname;
	MyString	iwd;
	MyString	cwd;

	shortname = submit_param( SUBMIT_KEY_InitialDir, ATTR_JOB_IWD );
	if( ! shortname ) {
			// neither "initialdir" nor "iwd" were there, try some
			// others, just to be safe:
		shortname = submit_param( SUBMIT_KEY_InitialDirAlt, SUBMIT_KEY_JobIwd );
	}

	// for factories initalized with a cluster ad, we NEVER want to use the current working directory
	// as IWD, instead use FACTORY.Iwd
	if ( ! shortname && clusterAd) {
		shortname = submit_param("FACTORY.Iwd");
	}

#if !defined(WIN32)
	ComputeRootDir();
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
				if (clusterAd) {
					cwd = submit_param_mystring("FACTORY.Iwd",NULL);
				} else {
					condor_getcwd( cwd );
				}
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

int SubmitHash::SetIWD()
{
	RETURN_IF_ABORT();
	if (ComputeIWD()) { ABORT_AND_RETURN(1); }
	AssignJobString(ATTR_JOB_IWD, JobIwd.Value());
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

			AssignJobVal(ATTR_X509_USER_PROXY_EXPIRATION, proxy_expiration);

			/* Insert the proxy subject name into the ad */
			char *proxy_subject;
			proxy_subject = x509_proxy_identity_name(proxy_handle);

			if ( !proxy_subject ) {
				push_error(stderr, "%s\n", x509_error_string() );
				x509_proxy_free( proxy_handle );
				ABORT_AND_RETURN( 1 );
			}

			(void) AssignJobString(ATTR_X509_USER_PROXY_SUBJECT, proxy_subject);
			free( proxy_subject );

			/* Insert the proxy email into the ad */
			char *proxy_email;
			proxy_email = x509_proxy_email(proxy_handle);

			if ( proxy_email ) {
				AssignJobString(ATTR_X509_USER_PROXY_EMAIL, proxy_email);
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
				AssignJobString(ATTR_X509_USER_PROXY_VONAME, voname);
				free( voname );

				AssignJobString(ATTR_X509_USER_PROXY_FIRST_FQAN, firstfqan);
				free( firstfqan );

				AssignJobString(ATTR_X509_USER_PROXY_FQAN, quoted_DN_and_FQAN);
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

		AssignJobString(ATTR_X509_USER_PROXY, proxy_file);
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
		AssignJobVal(ATTR_DELEGATE_JOB_GSI_CREDENTIALS_LIFETIME,lifetime);
		free(tmp);
	}

	//ckireyev: MyProxy-related crap
	if ((tmp = submit_param (ATTR_MYPROXY_HOST_NAME))) {
		AssignJobString(ATTR_MYPROXY_HOST_NAME, tmp);
		free( tmp );
	}

	if ((tmp = submit_param (ATTR_MYPROXY_SERVER_DN))) {
		AssignJobString(ATTR_MYPROXY_SERVER_DN, tmp);
		free( tmp );
	}

	if ((tmp = submit_param (ATTR_MYPROXY_CRED_NAME))) {
		AssignJobString(ATTR_MYPROXY_CRED_NAME, tmp);
		free( tmp );
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
		AssignJobExpr(ATTR_MYPROXY_PASSWORD, MyProxyPassword.Value());
	}

	if ((tmp = submit_param (ATTR_MYPROXY_REFRESH_THRESHOLD))) {
		AssignJobExpr(ATTR_MYPROXY_REFRESH_THRESHOLD, tmp);
		free( tmp );
	}

	if ((tmp = submit_param (ATTR_MYPROXY_NEW_PROXY_LIFETIME))) { 
		AssignJobExpr(ATTR_MYPROXY_NEW_PROXY_LIFETIME, tmp);
		free( tmp );
	}

	// END MyProxy-related crap
	return 0;
}


#if !defined(WIN32)
// modifies the passed in kill sig name, possibly freeing it and allocating a new buffer
// free() the bufer when you are done.
char* SubmitHash::fixupKillSigName( char* sig )
{
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

	sig_name = fixupKillSigName(submit_param(SUBMIT_KEY_KillSig, ATTR_KILL_SIG));
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
		AssignJobString(ATTR_KILL_SIG, sig_name);
		free( sig_name );
	}

	sig_name = fixupKillSigName(submit_param(SUBMIT_KEY_RmKillSig, ATTR_REMOVE_KILL_SIG));
	RETURN_IF_ABORT();
	if( sig_name ) {
		AssignJobString(ATTR_REMOVE_KILL_SIG, sig_name);
		free( sig_name );
		sig_name = NULL;
	}

	sig_name = fixupKillSigName(submit_param(SUBMIT_KEY_HoldKillSig, ATTR_HOLD_KILL_SIG));
	RETURN_IF_ABORT();
	if( sig_name ) {
		AssignJobString(ATTR_HOLD_KILL_SIG, sig_name);
		free( sig_name );
		sig_name = NULL;
	}

	timeout = submit_param( SUBMIT_KEY_KillSigTimeout, ATTR_KILL_SIG_TIMEOUT );
	if( timeout ) {
		AssignJobVal(ATTR_KILL_SIG_TIMEOUT, atoi(timeout));
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
		AssignJobExpr(cit->c_str(), value, "SUBMIT_ATTRS or SUBMIT_EXPRS value");
		free(value);
	}

	HASHITER it = hash_iter_begin(SubmitMacroSet);
	for( ; ! hash_iter_done(it); hash_iter_next(it)) {
		const char *name = hash_iter_key(it);
		const char *raw_value = hash_iter_value(it);
		// submit will never generate +attr entries, but the python bindings can
		// so treat them the same as the canonical MY.attr entries
		if (*name == '+') {
			++name;
		} else if (starts_with_ignore_case(name, "MY.")) {
			name += sizeof("MY.")-1;
		} else {
			continue;
		}

		char * value = NULL;
		if (raw_value && raw_value[0]) {
			value = expand_macro(raw_value);
		}
		AssignJobExpr(name, (value && value[0]) ? value : "undefined" );
		RETURN_IF_ABORT();

		if (value) free(value);
	}
	hash_iter_delete(&it);

	// force clusterid and procid attributes.
	// we force the clusterid only for the proc=0 ad and the cluster ad (proc=-1)
	// for other jobs, the clusterid should be picked up by chaining with the cluster ad.
	if (jid.proc < 0) {
		AssignJobVal(ATTR_CLUSTER_ID, jid.cluster);
	} else {
		AssignJobVal(ATTR_PROC_ID, jid.proc);
	}
	return 0;
}

int SubmitHash::SetGridParams()
{
	RETURN_IF_ABORT();
	char *tmp;
	FILE* fp;

	if ( JobUniverse != CONDOR_UNIVERSE_GRID )
		return 0;

	tmp = submit_param( SUBMIT_KEY_GridResource, ATTR_GRID_RESOURCE );
	if ( tmp ) {
			// TODO validate number of fields in grid_resource?

		AssignJobString(ATTR_GRID_RESOURCE, tmp);

		if ( strstr(tmp,"$$") ) {
				// We need to perform matchmaking on the job in order
				// to fill GridResource.
			AssignJobVal(ATTR_JOB_MATCHED, false);
			AssignJobVal(ATTR_CURRENT_HOSTS, 0);
			AssignJobVal(ATTR_MAX_HOSTS, 1);
		}

		if ( strcasecmp( tmp, "ec2" ) == 0 ) {
			push_error(stderr, "EC2 grid jobs require a service URL\n");
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
			AssignJobExpr(ATTR_GLOBUS_RESUBMIT_CHECK, tmp);
			free(tmp);
		} else {
			AssignJobVal(ATTR_GLOBUS_RESUBMIT_CHECK, false);
		}
	}

	if ( (tmp = submit_param(SUBMIT_KEY_GridShell, ATTR_USE_GRID_SHELL)) ) {

		if( tmp[0] == 't' || tmp[0] == 'T' ) {
			AssignJobVal(ATTR_USE_GRID_SHELL, true);
		}
		free(tmp);
	}

	if ( gridType == NULL ||
		 gridType == "gt2" ||
		 gridType == "gt5" ) {
		AssignJobVal(ATTR_GLOBUS_STATUS, GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNSUBMITTED);
		AssignJobVal(ATTR_NUM_GLOBUS_SUBMITS, 0);
	}

	AssignJobVal(ATTR_WANT_CLAIMING, false);

	if( (tmp = submit_param(SUBMIT_KEY_GlobusRematch,ATTR_REMATCH_CHECK)) ) {
		AssignJobExpr(ATTR_REMATCH_CHECK, tmp);
		free(tmp);
	}

	if( (tmp = submit_param(SUBMIT_KEY_GlobusRSL, ATTR_GLOBUS_RSL)) ) {
		AssignJobString(ATTR_GLOBUS_RSL, tmp);
		free( tmp );
	}

	if( (tmp = submit_param(SUBMIT_KEY_NordugridRSL, ATTR_NORDUGRID_RSL)) ) {
		AssignJobString(ATTR_NORDUGRID_RSL, tmp);
		free( tmp );
	}

	if( (tmp = submit_param(SUBMIT_KEY_CreamAttributes, ATTR_CREAM_ATTRIBUTES)) ) {
		AssignJobString ( ATTR_CREAM_ATTRIBUTES, tmp );
		free( tmp );
	}

	if( (tmp = submit_param(SUBMIT_KEY_BatchQueue, ATTR_BATCH_QUEUE)) ) {
		AssignJobString ( ATTR_BATCH_QUEUE, tmp );
		free( tmp );
	}

	if ( (tmp = submit_param( SUBMIT_KEY_KeystoreFile, ATTR_KEYSTORE_FILE )) ) {
		AssignJobString(ATTR_KEYSTORE_FILE, tmp);
		free( tmp );
	} else if ( gridType == "unicore" ) {
		push_error(stderr, "Unicore grid jobs require a " SUBMIT_KEY_KeystoreFile " parameter\n");
		ABORT_AND_RETURN( 1 );
	}

	if ( (tmp = submit_param( SUBMIT_KEY_KeystoreAlias, ATTR_KEYSTORE_ALIAS )) ) {
		AssignJobString(ATTR_KEYSTORE_ALIAS, tmp);
		free( tmp );
	} else if ( gridType == "unicore" ) {
		push_error(stderr, "Unicore grid jobs require a " SUBMIT_KEY_KeystoreAlias " parameter\n");
		ABORT_AND_RETURN( 1 );
	}

	if ( (tmp = submit_param( SUBMIT_KEY_KeystorePassphraseFile,
							  ATTR_KEYSTORE_PASSPHRASE_FILE )) ) {
		AssignJobString(ATTR_KEYSTORE_PASSPHRASE_FILE, tmp);
		free( tmp );
	} else if ( gridType == "unicore" ) {
		push_error(stderr, "Unicore grid jobs require a " SUBMIT_KEY_KeystorePassphraseFile " parameter\n");
		ABORT_AND_RETURN( 1 );
	}

	//
	// EC2 grid-type submit attributes
	//
	if ( (tmp = submit_param( SUBMIT_KEY_EC2AccessKeyId, ATTR_EC2_ACCESS_KEY_ID )) ) {
		if( MATCH == strcasecmp( tmp, USE_INSTANCE_ROLE_MAGIC_STRING ) ) {
			AssignJobString(ATTR_EC2_ACCESS_KEY_ID, USE_INSTANCE_ROLE_MAGIC_STRING);
			AssignJobString(ATTR_EC2_SECRET_ACCESS_KEY, USE_INSTANCE_ROLE_MAGIC_STRING);
			free( tmp );
		} else {
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
			AssignJobString(ATTR_EC2_ACCESS_KEY_ID, full_path(tmp));
			free( tmp );
		}
	}

	if ( (tmp = submit_param( SUBMIT_KEY_EC2SecretAccessKey, ATTR_EC2_SECRET_ACCESS_KEY )) ) {
		if( MATCH == strcasecmp( tmp, USE_INSTANCE_ROLE_MAGIC_STRING ) ) {
			AssignJobString(ATTR_EC2_ACCESS_KEY_ID, USE_INSTANCE_ROLE_MAGIC_STRING);
			AssignJobString(ATTR_EC2_SECRET_ACCESS_KEY, USE_INSTANCE_ROLE_MAGIC_STRING);
			free( tmp );
		} else {
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
			AssignJobString(ATTR_EC2_SECRET_ACCESS_KEY, full_path(tmp));
			free( tmp );
		}
	}

	if ( gridType == "ec2" ) {
		if(! job->Lookup( ATTR_EC2_ACCESS_KEY_ID )) {
			push_error(stderr, "EC2 jobs require a '" SUBMIT_KEY_EC2AccessKeyId "' parameter\n" );
			ABORT_AND_RETURN( 1 );
		}
		if(! job->Lookup( ATTR_EC2_SECRET_ACCESS_KEY )) {
			push_error(stderr, "EC2 jobs require a '" SUBMIT_KEY_EC2SecretAccessKey "' parameter\n");
			ABORT_AND_RETURN( 1 );
		}
	}


	bool bKeyPairPresent=false;

	// EC2KeyPair is not a necessary parameter
	if( (tmp = submit_param( SUBMIT_KEY_EC2KeyPair, ATTR_EC2_KEY_PAIR )) ||
		(tmp = submit_param( SUBMIT_KEY_EC2KeyPairAlt, ATTR_EC2_KEY_PAIR ))) {
		AssignJobString(ATTR_EC2_KEY_PAIR, tmp);
		free( tmp );
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
	      AssignJobString(ATTR_EC2_KEY_PAIR_FILE, full_path(tmp));
	      free( tmp );
	    }
	}

	// Optional.
	if( (tmp = submit_param( SUBMIT_KEY_EC2SecurityGroups, ATTR_EC2_SECURITY_GROUPS )) ) {
		AssignJobString(ATTR_EC2_SECURITY_GROUPS, tmp);
		free( tmp );
	}

	// Optional.
	if( (tmp = submit_param( SUBMIT_KEY_EC2SecurityIDs, ATTR_EC2_SECURITY_IDS )) ) {
		AssignJobString(ATTR_EC2_SECURITY_IDS, tmp);
		free( tmp );
	}

	if ( (tmp = submit_param( SUBMIT_KEY_EC2AmiID, ATTR_EC2_AMI_ID )) ) {
		AssignJobString(ATTR_EC2_AMI_ID, tmp);
		free( tmp );
	} else if ( gridType == "ec2"  ) {
		push_error(stderr, "EC2 jobs require a \"%s\" parameter\n", SUBMIT_KEY_EC2AmiID );
		ABORT_AND_RETURN( 1 );
	}

	// EC2InstanceType is not a necessary parameter
	if( (tmp = submit_param( SUBMIT_KEY_EC2InstanceType, ATTR_EC2_INSTANCE_TYPE )) ) {
		AssignJobString(ATTR_EC2_INSTANCE_TYPE, tmp);
		free( tmp );
	}

	// EC2VpcSubnet is not a necessary parameter
	if( (tmp = submit_param( SUBMIT_KEY_EC2VpcSubnet, ATTR_EC2_VPC_SUBNET )) ) {
		AssignJobString(ATTR_EC2_VPC_SUBNET , tmp);
		free( tmp );
	}

	// EC2VpcIP is not a necessary parameter
	if( (tmp = submit_param( SUBMIT_KEY_EC2VpcIP, ATTR_EC2_VPC_IP )) ) {
		AssignJobString(ATTR_EC2_VPC_IP , tmp);
		free( tmp );
	}

	// EC2ElasticIP is not a necessary parameter
	if( (tmp = submit_param( SUBMIT_KEY_EC2ElasticIP, ATTR_EC2_ELASTIC_IP )) ) {
		AssignJobString(ATTR_EC2_ELASTIC_IP, tmp);
		free( tmp );
	}

	bool HasAvailabilityZone=false;
	// EC2AvailabilityZone is not a necessary parameter
	if( (tmp = submit_param( SUBMIT_KEY_EC2AvailabilityZone, ATTR_EC2_AVAILABILITY_ZONE )) ) {
		AssignJobString(ATTR_EC2_AVAILABILITY_ZONE, tmp);
		free( tmp );
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

		AssignJobString(ATTR_EC2_EBS_VOLUMES, tmp);
		free( tmp );
	}

	// EC2SpotPrice is not a necessary parameter
	if( (tmp = submit_param( SUBMIT_KEY_EC2SpotPrice, ATTR_EC2_SPOT_PRICE )) ) {
		AssignJobString(ATTR_EC2_SPOT_PRICE, tmp);
		free( tmp );
	}

	// EC2BlockDeviceMapping is not a necessary parameter
	if( (tmp = submit_param( SUBMIT_KEY_EC2BlockDeviceMapping, ATTR_EC2_BLOCK_DEVICE_MAPPING )) ) {
		AssignJobString(ATTR_EC2_BLOCK_DEVICE_MAPPING, tmp);
		free( tmp );
		bKeyPairPresent=true;
	}

	// EC2UserData is not a necessary parameter
	if( (tmp = submit_param( SUBMIT_KEY_EC2UserData, ATTR_EC2_USER_DATA )) ) {
		AssignJobString(ATTR_EC2_USER_DATA, tmp);
		free( tmp );
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
		AssignJobString(ATTR_EC2_USER_DATA_FILE, full_path(tmp));
		free( tmp );
	}

	// You can only have one IAM [Instance] Profile, so you can only use
	// one of the ARN or the Name.
	bool bIamProfilePresent = false;
	if( (tmp = submit_param( SUBMIT_KEY_EC2IamProfileArn, ATTR_EC2_IAM_PROFILE_ARN )) ) {
		bIamProfilePresent = true;
		AssignJobString(ATTR_EC2_IAM_PROFILE_ARN, tmp);
		free( tmp );
	}

	if( (tmp = submit_param( SUBMIT_KEY_EC2IamProfileName, ATTR_EC2_IAM_PROFILE_NAME )) ) {
		if( bIamProfilePresent ) {
			push_warning( stderr, "EC2 job(s) contain both " SUBMIT_KEY_EC2IamProfileArn " and " SUBMIT_KEY_EC2IamProfileName "; ignoring " SUBMIT_KEY_EC2IamProfileName ".\n");
		} else {
			AssignJobString(ATTR_EC2_IAM_PROFILE_NAME, tmp);
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

	std::string ec2attr;
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
		ec2attr = ATTR_EC2_PARAM_PREFIX "_";
		ec2attr += paramName;
		AssignJobString(ec2attr.c_str(), paramValue);
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
		AssignJobString(ATTR_EC2_PARAM_NAMES, paramNamesStr);
		free( paramNamesStr );
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

	char *tagName;
	tagNames.rewind();
	while ((tagName = tagNames.next())) {
			// XXX: Check that tagName does not contain an equal sign (=)
		std::string tag;
		std::string tagAttr(ATTR_EC2_TAG_PREFIX); tagAttr.append(tagName);
		std::string tagCmd("ec2_tag_"); tagCmd.append(tagName);
		char *value = NULL;
		if ((value = submit_param(tagCmd.c_str(), tagAttr.c_str()))) {
			AssignJobString(tagAttr.c_str(), value);
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
			bool wantsNameTag = submit_param_bool(SUBMIT_KEY_WantNameTag, NULL, true );
			if( wantsNameTag ) {
				char *ename = submit_param(SUBMIT_KEY_Executable, ATTR_JOB_CMD); // !NULL by now
				AssignJobString(ATTR_EC2_TAG_PREFIX "Name", ename);
				free(ename); ename = NULL;
			}
		}
	}

	if ( !tagNames.isEmpty() ) {
		auto_free_ptr names(tagNames.print_to_delimed_string(","));
		AssignJobString(ATTR_EC2_TAG_NAMES, names);
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
		AssignJobString(ATTR_BOINC_AUTHENTICATOR_FILE, full_path(tmp));
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
		AssignJobString(ATTR_GCE_AUTH_FILE, full_path(tmp));
		free( tmp );
	}

	if ( (tmp = submit_param( SUBMIT_KEY_GceAccount, ATTR_GCE_ACCOUNT )) ) {
		AssignJobString(ATTR_GCE_ACCOUNT, tmp);
		free( tmp );
	}

	if ( (tmp = submit_param( SUBMIT_KEY_GceImage, ATTR_GCE_IMAGE )) ) {
		AssignJobString(ATTR_GCE_IMAGE, tmp);
		free( tmp );
	} else if ( gridType == "gce" ) {
		push_error(stderr, "GCE jobs require a \"%s\" parameter\n", SUBMIT_KEY_GceImage );
		ABORT_AND_RETURN( 1 );
	}

	if ( (tmp = submit_param( SUBMIT_KEY_GceMachineType, ATTR_GCE_MACHINE_TYPE )) ) {
		AssignJobString(ATTR_GCE_MACHINE_TYPE, tmp);
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
		AssignJobString(ATTR_GCE_METADATA, list_str);
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
		AssignJobString(ATTR_GCE_METADATA_FILE, full_path(tmp));
		free( tmp );
	}

	// GcePreemptible is not a necessary parameter
	bool exists = false;
	bool bool_val = submit_param_bool( SUBMIT_KEY_GcePreemptible, ATTR_GCE_PREEMPTIBLE, false, &exists );
	if( exists ) {
		AssignJobVal(ATTR_GCE_PREEMPTIBLE, bool_val);
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
		AssignJobString( ATTR_GCE_JSON_FILE, full_path( tmp ) );
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
		AssignJobString(ATTR_AZURE_AUTH_FILE, full_path(tmp));
		free( tmp );
	} else if ( gridType == "azure" ) {
		push_error(stderr, "\nERROR: Azure jobs require an \"%s\" parameter\n", SUBMIT_KEY_AzureAuthFile );
		ABORT_AND_RETURN( 1 );
	}

	if ( (tmp = submit_param( SUBMIT_KEY_AzureImage, ATTR_AZURE_IMAGE )) ) {
		AssignJobString(ATTR_AZURE_IMAGE, tmp);
		free( tmp );
	} else if ( gridType == "azure" ) {
		push_error(stderr, "\nERROR: Azure jobs require an \"%s\" parameter\n", SUBMIT_KEY_AzureImage );
		ABORT_AND_RETURN( 1 );
	}

	if ( (tmp = submit_param( SUBMIT_KEY_AzureLocation, ATTR_AZURE_LOCATION )) ) {
		AssignJobString(ATTR_AZURE_LOCATION, tmp);
		free( tmp );
	} else if ( gridType == "azure" ) {
		push_error(stderr, "\nERROR: Azure jobs require an \"%s\" parameter\n", SUBMIT_KEY_AzureLocation );
		ABORT_AND_RETURN( 1 );
	}

	if ( (tmp = submit_param( SUBMIT_KEY_AzureSize, ATTR_AZURE_SIZE )) ) {
		AssignJobString(ATTR_AZURE_SIZE, tmp);
		free( tmp );
	} else if ( gridType == "azure" ) {
		push_error(stderr, "\nERROR: Azure jobs require an \"%s\" parameter\n", SUBMIT_KEY_AzureSize );
		ABORT_AND_RETURN( 1 );
	}

	if ( (tmp = submit_param( SUBMIT_KEY_AzureAdminUsername, ATTR_AZURE_ADMIN_USERNAME )) ) {
		AssignJobString(ATTR_AZURE_ADMIN_USERNAME, tmp);
		free( tmp );
	} else if ( gridType == "azure" ) {
		push_error(stderr, "\nERROR: Azure jobs require an \"%s\" parameter\n", SUBMIT_KEY_AzureAdminUsername );
		ABORT_AND_RETURN( 1 );
	}

	if ( (tmp = submit_param( SUBMIT_KEY_AzureAdminKey, ATTR_AZURE_ADMIN_KEY )) ) {
		AssignJobString(ATTR_AZURE_ADMIN_KEY, tmp);
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

				AssignJobString(ATTR_GRID_RESOURCE, resource.Value() );
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

	AssignJobVal(ATTR_JOB_NOTIFICATION, notification);

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

			push_warning( stderr, "You used  SUBMIT_KEY_NotifyUser=%s  in your submit file.\n"
					"This means notification email will go to user \"%s@%s\".\n"
					"This is probably not what you expect!\n"
					"If you do not want notification email, put \"notification = never\"\n"
					"into your submit file, instead.\n",
					who, who, tmp.ptr() );
			already_warned_notification_never = true;
		}
		AssignJobString(ATTR_NOTIFY_USER, who);
		free(who);
	}
	return 0;
}

int SubmitHash::SetWantGracefulRemoval()
{
	RETURN_IF_ABORT();
	char *how = submit_param( SUBMIT_KEY_WantGracefulRemoval, ATTR_WANT_GRACEFUL_REMOVAL );

	if( how ) {
		AssignJobExpr(ATTR_WANT_GRACEFUL_REMOVAL, how);
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
		AssignJobExpr(ATTR_JOB_MAX_VACATE_TIME, expr);
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
			AssignJobString(ATTR_EMAIL_ATTRIBUTES, tmp);
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

	static const SimpleExprInfo fields[] = {
		/*submit_param*/ {SUBMIT_KEY_CronMinute, ATTR_CRON_MINUTES,
				ATTR_CRON_MINUTES, NULL, true},
		/*submit_param*/ {SUBMIT_KEY_CronHour, ATTR_CRON_HOURS,
				ATTR_CRON_HOURS, NULL, true},
		/*submit_param*/ {SUBMIT_KEY_CronDayOfMonth, ATTR_CRON_DAYS_OF_MONTH,
				ATTR_CRON_DAYS_OF_MONTH, NULL, true},
		/*submit_param*/ {SUBMIT_KEY_CronMonth, ATTR_CRON_MONTHS,
				ATTR_CRON_MONTHS, NULL, true},
		/*submit_param*/ {SUBMIT_KEY_CronDayOfWeek, ATTR_CRON_DAYS_OF_WEEK,
				ATTR_CRON_DAYS_OF_WEEK, NULL, true},
		{NULL,NULL,NULL,NULL,false}
	};

	bool has_cron = false;

	CronTab::initRegexObject();
	for (int ctr = 0; fields[ctr].key != NULL; ctr++ ) {
		char *param = submit_param( fields[ctr].key, fields[ctr].alt );
		if (param) {
				//
				// We'll try to be nice and validate it first
				//
			MyString error;
			if ( ! CronTab::validateParameter( param, fields[ctr].attr, error ) ) {
				push_error( stderr, "%s\n", error.Value() );
				ABORT_AND_RETURN( 1 );
			}
				//
				// Go ahead and stuff it in the job ad now
				// The parameters all all strings
				//
			has_cron = true;
			AssignJobString(fields[ctr].attr, param);
			free( param );
		}
	} // for
		//
		// Validation
		// Because the scheduler universe doesn't use a Starter,
		// we can't let them use the CronTab scheduling which needs 
		// to be able to use the job deferral feature
		//
	if ( has_cron && JobUniverse == CONDOR_UNIVERSE_SCHEDULER ) {
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
		AssignJobVal(ATTR_LAST_MATCH_LIST_LENGTH, len);
		free( tmp );
	}
	return 0;
}

int SubmitHash::SetDAGNodeName()
{
	RETURN_IF_ABORT();
	char* name = submit_param( ATTR_DAG_NODE_NAME_ALT, ATTR_DAG_NODE_NAME );
	if( name ) {
		AssignJobString(ATTR_DAG_NODE_NAME, name );
		free( name );
	}
	return 0;
}

int SubmitHash::SetDAGManJobId()
{
	RETURN_IF_ABORT();
	char* id = submit_param(SUBMIT_KEY_DAGManJobId, ATTR_DAGMAN_JOB_ID );
	if( id ) {
		AssignJobString(ATTR_DAGMAN_JOB_ID, id);
		free( id );
	}
	return 0;
}

int SubmitHash::SetLogNotes()
{
	RETURN_IF_ABORT();
	char * LogNotesVal = submit_param( SUBMIT_KEY_LogNotesCommand, ATTR_SUBMIT_EVENT_NOTES );
	if ( LogNotesVal ) {
		AssignJobString( ATTR_SUBMIT_EVENT_NOTES, LogNotesVal );
		free( LogNotesVal );
	}
	return 0;
}

int SubmitHash::SetUserNotes()
{
	RETURN_IF_ABORT();
	char * UserNotesVal = submit_param( SUBMIT_KEY_UserNotesCommand, ATTR_SUBMIT_EVENT_USER_NOTES );
	if ( UserNotesVal ) {
		AssignJobString( ATTR_SUBMIT_EVENT_USER_NOTES, UserNotesVal );
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
		AssignJobExpr(ATTR_STACK_SIZE, StackSizeVal);
		free( StackSizeVal );
	}
	return 0;
}

int SubmitHash::SetRemoteInitialDir()
{
	RETURN_IF_ABORT();
	char *who = submit_param( SUBMIT_KEY_RemoteInitialDir, ATTR_JOB_REMOTE_IWD );
	if (who) {
		AssignJobString(ATTR_JOB_REMOTE_IWD, who);
		free(who);
	}
	return 0;
}

int SubmitHash::SetExitRequirements()
{
	RETURN_IF_ABORT();
	char *who = submit_param( SUBMIT_KEY_ExitRequirements, ATTR_JOB_EXIT_REQUIREMENTS );
	if (who) {
		push_error(stderr, SUBMIT_KEY_ExitRequirements " is deprecated.\n"
			"Please use on_exit_remove or on_exit_hold.\n");
		free(who);
		ABORT_AND_RETURN( 1 );
	}
	return 0;
}
	
int SubmitHash::SetOutputDestination()
{
	RETURN_IF_ABORT();
	char *od = submit_param( SUBMIT_KEY_OutputDestination, ATTR_OUTPUT_DESTINATION );
	if (od) {
		AssignJobString(ATTR_OUTPUT_DESTINATION, od);
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
	bool allow_arguments_v1 = submit_param_bool( SUBMIT_CMD_AllowArgumentsV1, NULL, false );
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

	MyString value;
	bool MyCondorVersionRequiresV1 = arglist.InputWasV1() || arglist.CondorVersionRequiresV1(getScheddVersion());
	if(MyCondorVersionRequiresV1) {
		args_success = arglist.GetArgsStringV1Raw(&value,&error_msg);
		AssignJobString(ATTR_JOB_ARGUMENTS1, value.c_str());
	}
	else {
		args_success = arglist.GetArgsStringV2Raw(&value,&error_msg);
		AssignJobString(ATTR_JOB_ARGUMENTS2, value.c_str());
	}

	if(!args_success) {
		push_error(stderr, "failed to insert arguments: %s\n",
				error_msg.Value());
		ABORT_AND_RETURN(1);
	}

	if( JobUniverse == CONDOR_UNIVERSE_JAVA && arglist.Count() == 0)
	{
		push_error(stderr, "In Java universe, you must specify the class name to run.\nExample:\n\narguments = MyClass\n\n");
		ABORT_AND_RETURN( 1 );
	}

	if(args1) free(args1);
	if(args2) free(args2);
	return 0;
}


static bool is_non_negative_int(const char * Value, long long & lval)
{
	lval = 0;
	if (strstr(Value,".")) {
		return false;
	}
	char *endptr=NULL;
	lval = strtoll(Value,&endptr,10);
	if (lval < 0 || endptr == Value || *endptr) {
		lval = 0;
		return false;
	}

	return true;
}

//
// returns true if the job has one of the attributes set that requires job deferral
// used by SetRequirements and some of the validation code.
//
bool SubmitHash::NeedsJobDeferral()
{
	static const char * const attrs[] = {
		ATTR_CRON_MINUTES, ATTR_CRON_HOURS, ATTR_CRON_DAYS_OF_MONTH, ATTR_CRON_MONTHS, ATTR_CRON_DAYS_OF_WEEK,
		ATTR_DEFERRAL_TIME,
	};
	for (size_t ii = 0; ii < COUNTOF(attrs); ++ii) {
		if (job->Lookup(attrs[ii])) {
			return true;
		}
	}
	return false;
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
		long long dtime = 0;
		if (is_non_negative_int(temp, dtime)) {
			AssignJobVal(ATTR_DEFERRAL_TIME, dtime);
		} else {
			push_error(stderr, SUBMIT_KEY_DeferralTime " = %s is invalid, must eval to a non-negative integer.\n", temp );
			ABORT_AND_RETURN( 1 );
		}
			
		free( temp );
	}
	
		//
		// If this job needs the job deferral functionality, we
		// need to make sure we always add in the DeferralWindow
		// and the DeferralPrepTime attributes.
		// We have a separate if clause because SetCronTab() can
		// also set attributes that trigger this.
		//
	if (NeedsJobDeferral()) {
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
		temp = submit_param( SUBMIT_KEY_CronWindow, ATTR_CRON_WINDOW );
		if ( ! temp) {
			temp = submit_param( SUBMIT_KEY_DeferralWindow, ATTR_DEFERRAL_WINDOW );
		}
			//
			// If we have a parameter from the job file, use that value
			//
		if ( temp != NULL ){
			
			// make certain the input is valid
			long long dtime = 0;
			if (is_non_negative_int(temp, dtime)) {
				AssignJobVal(ATTR_DEFERRAL_WINDOW, dtime);
			} else {
				push_error(stderr, SUBMIT_KEY_DeferralWindow " = %s is invalid, must eval to a non-negative integer.\n", temp );
				ABORT_AND_RETURN( 1 );
			}
			free( temp );
			//
			// Otherwise, use the default value
			//
		} else {
			AssignJobVal(ATTR_DEFERRAL_WINDOW, JOB_DEFERRAL_WINDOW_DEFAULT);
		}
		
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
		temp = submit_param(SUBMIT_KEY_CronPrepTime, ATTR_CRON_PREP_TIME);
		if ( ! temp) {
			temp = submit_param(SUBMIT_KEY_DeferralPrepTime, ATTR_DEFERRAL_PREP_TIME);
		}
			//
			// If we have a parameter from the job file, use that value
			//
		if ( temp != NULL ){
			// make certain the input is valid
			long long dtime = 0;
			if (is_non_negative_int(temp, dtime)) {
				AssignJobVal(ATTR_DEFERRAL_PREP_TIME, dtime);
			} else {
				push_error(stderr, SUBMIT_KEY_DeferralPrepTime " = %s is invalid, must eval to a non-negative integer.\n", temp );
				ABORT_AND_RETURN( 1 );
			}
			free( temp );
			//
			// Otherwise, use the default value
			//
		} else {
			AssignJobVal(ATTR_DEFERRAL_PREP_TIME, JOB_DEFERRAL_PREP_TIME_DEFAULT);
		}
		
			//
			// Schedd Polling Interval
			// We need make sure we have this in the job ad as well
			// because it is used by the Requirements expression generated
			// in SetRequirements() for job deferral
			//
		temp = param( "SCHEDD_INTERVAL" );
		if ( temp != NULL ) {
			AssignJobExpr(ATTR_SCHEDD_INTERVAL, temp);
			free( temp );
		} else {
			AssignJobVal(ATTR_SCHEDD_INTERVAL, SCHEDD_INTERVAL_DEFAULT);
		}
	
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

	long long num_retries = param_integer("DEFAULT_JOB_MAX_RETRIES", 2);
	long long success_code = 0;
	std::string retry_until;

	bool enable_retries = false;
	bool success_exit_code_set = false;
	if (submit_param_long_exists(SUBMIT_KEY_MaxRetries, ATTR_JOB_MAX_RETRIES, num_retries)) { enable_retries = true; }
	if (submit_param_long_exists(SUBMIT_KEY_SuccessExitCode, ATTR_JOB_SUCCESS_EXIT_CODE, success_code, true)) { enable_retries = true; success_exit_code_set = true; }
	if (submit_param_exists(SUBMIT_KEY_RetryUntil, NULL, retry_until)) { enable_retries = true; }
	if ( ! enable_retries)
	{
		// if none of these knobs are defined, then there are no retries.
		// Just insert the default on-exit-hold and on-exit-remove expressions
		if (erc.empty()) {
			AssignJobVal (ATTR_ON_EXIT_REMOVE_CHECK, true);
		} else {
			AssignJobExpr (ATTR_ON_EXIT_REMOVE_CHECK, erc.c_str());
		}
		if (ehc.empty()) {
			AssignJobVal (ATTR_ON_EXIT_HOLD_CHECK, false);
		} else {
			AssignJobExpr(ATTR_ON_EXIT_HOLD_CHECK, ehc.c_str());
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
			classad::References refs;
			GetExprReferences(retry_until.c_str(), tmp, &refs, &refs);
			long long futility_code;
			if (refs.empty() && string_is_long_param(retry_until.c_str(), futility_code)) {
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
	const char * basic_exit_remove_expr = ATTR_NUM_JOB_COMPLETIONS " > " ATTR_JOB_MAX_RETRIES " || " ATTR_ON_EXIT_CODE " == ";

	// build the sub expression that checks for exit codes that should end retries
	std::string code_check;
	if (success_exit_code_set) {
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
	AssignJobExpr(ATTR_ON_EXIT_REMOVE_CHECK, onexitrm.c_str());
	RETURN_IF_ABORT();

	// paste up the final OnExitHold expression and insert it into the job.
	if (ehc.empty()) {
		// TODO: remove this trvial default when it is no longer needed
		AssignJobVal (ATTR_ON_EXIT_HOLD_CHECK, false);
	} else {
		AssignJobExpr(ATTR_ON_EXIT_HOLD_CHECK, ehc.c_str());
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
			AssignJobVal(attr.Value(), univ);

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
				AssignJobString(ckey3, val);
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
	MyString job_machine_attrs = submit_param_mystring( SUBMIT_KEY_JobMachineAttrs, ATTR_JOB_MACHINE_ATTRS );
	MyString history_len_str = submit_param_mystring( SUBMIT_KEY_JobMachineAttrsHistoryLength, ATTR_JOB_MACHINE_ATTRS_HISTORY_LENGTH );
	MyString buffer;

	if( job_machine_attrs.Length() ) {
		AssignJobString(ATTR_JOB_MACHINE_ATTRS,job_machine_attrs.Value());
	}
	if( history_len_str.Length() ) {
		char *endptr=NULL;
		long history_len = strtol(history_len_str.Value(),&endptr,10);
		if( history_len > INT_MAX || history_len < 0 || *endptr) {
			push_error(stderr, SUBMIT_KEY_JobMachineAttrsHistoryLength "=%s is out of bounds 0 to %d\n",history_len_str.Value(),INT_MAX);
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
		AssignJobString(ATTR_DOCKER_IMAGE, image);
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

	AssignJobString (ATTR_JOB_CMD, full_ename.Value());

		/* MPI REALLY doesn't like these! */
	if ( JobUniverse != CONDOR_UNIVERSE_MPI ) {
		AssignJobVal(ATTR_MIN_HOSTS, 1);
		AssignJobVal (ATTR_MAX_HOSTS, 1);
	} 

	if ( JobUniverse == CONDOR_UNIVERSE_PARALLEL) {
		AssignJobVal(ATTR_WANT_IO_PROXY, true);
		AssignJobVal(ATTR_JOB_REQUIRES_SANDBOX, true);
	}

	AssignJobVal (ATTR_CURRENT_HOSTS, 0);

	switch(JobUniverse) 
	{
	case CONDOR_UNIVERSE_STANDARD:
		AssignJobVal (ATTR_WANT_REMOTE_SYSCALLS, true);
		AssignJobVal (ATTR_WANT_CHECKPOINT, true);
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
		AssignJobVal (ATTR_WANT_REMOTE_SYSCALLS, false);
		AssignJobVal (ATTR_WANT_CHECKPOINT, false);
		break;
	default:
		push_error(stderr, "Unknown universe %d (%s)\n", JobUniverse, CondorUniverseName(JobUniverse) );
		ABORT_AND_RETURN( 1 );
	}

	if (FnCheckFile) {
		int rval = FnCheckFile(CheckFileArg, this, role, ename, (transfer_it ? 1 : 0));
		if (rval) { ABORT_AND_RETURN( rval ); }
	}
	if (ename) free(ename);
	return 0;
}

int SubmitHash::SetDescription()
{
	RETURN_IF_ABORT();

	char* description = submit_param( SUBMIT_KEY_Description, ATTR_JOB_DESCRIPTION );
	if ( description ){
		AssignJobString(ATTR_JOB_DESCRIPTION, description);
		free(description);
	}
	else if ( IsInteractiveJob ){
		AssignJobString(ATTR_JOB_DESCRIPTION, "interactive job");
	}

	MyString batch_name = submit_param_mystring(SUBMIT_KEY_BatchName, ATTR_JOB_BATCH_NAME);
	if ( ! batch_name.empty()) {
		batch_name.trim_quotes("\"'"); // in case they supplied a quoted string, trim the quotes
		AssignJobString(ATTR_JOB_BATCH_NAME, batch_name.c_str());
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
			AssignJobVal(ATTR_WANT_DOCKER, true);
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
			push_error(stderr, SUBMIT_KEY_VM_Type " cannot be found.\nPlease specify " SUBMIT_KEY_VM_Type
				" for vm universe in your submit description file.\n");
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
				auto_free_ptr vm_tmp(submit_param( ATTR_WHEN_TO_TRANSFER_OUTPUT, SUBMIT_KEY_WhenToTransferOutput ));
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

	bool wantParallel = submit_param_bool(ATTR_WANT_PARALLEL_SCHEDULING, NULL, false);
	if (wantParallel) {
		AssignJobVal(ATTR_WANT_PARALLEL_SCHEDULING, true);
	}
 
	if (JobUniverse == CONDOR_UNIVERSE_MPI ||
		JobUniverse == CONDOR_UNIVERSE_PARALLEL || wantParallel) {

		mach_count = submit_param( SUBMIT_KEY_MachineCount, ATTR_MACHINE_COUNT );
		if( ! mach_count ) { 
				// try an alternate name
			mach_count = submit_param( SUBMIT_KEY_NodeCount, SUBMIT_KEY_NodeCountAlt );
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

		AssignJobVal(ATTR_MIN_HOSTS, tmp);
		AssignJobVal(ATTR_MAX_HOSTS, tmp);

		request_cpus = 1;
	} else {
		mach_count = submit_param( SUBMIT_KEY_MachineCount, ATTR_MACHINE_COUNT );
		if( mach_count ) { 
			int tmp = atoi(mach_count);
			free(mach_count);

			if( tmp < 1 ) {
				push_error(stderr, "machine_count must be >= 1\n" );
				ABORT_AND_RETURN( 1 );
			}

			AssignJobVal(ATTR_MACHINE_COUNT, tmp);

			request_cpus = tmp;
		}
	}

	if ((mach_count = submit_param(SUBMIT_KEY_RequestCpus, ATTR_REQUEST_CPUS))) {
		if (MATCH == strcasecmp(mach_count, "undefined")) {
		} else {
			AssignJobExpr(ATTR_REQUEST_CPUS, mach_count);
		}
		free(mach_count);
	} else {
		// no request_cpus keyword, did they just misspell it?
		std::string cpuval;
		if (submit_param_exists("request_cpu", NULL, cpuval)) {
			push_warning(stderr, "request_cpu is not a valid submit keyword, did you mean request_cpus?\n");
		}
		if (request_cpus > 0) {
			AssignJobVal(ATTR_REQUEST_CPUS, request_cpus);
		} else
		if ((mach_count = param("JOB_DEFAULT_REQUESTCPUS"))) {
			if (MATCH == strcasecmp(mach_count, "undefined")) {
			} else {
				AssignJobExpr(ATTR_REQUEST_CPUS, mach_count);
			}
			free(mach_count);
		}
	}
	return 0;
}


/* This function is used to handle submit file commands that are inserted
 * into the job ClassAd verbatim, with no special treatment.
 */

int SubmitHash::SetSimpleJobExprs()
{
	RETURN_IF_ABORT();
	static const SimpleExprInfo simple_exprs[] = {
		/*submit_param*/ {SUBMIT_KEY_NextJobStartDelay, ATTR_NEXT_JOB_START_DELAY,
				ATTR_NEXT_JOB_START_DELAY, NULL, false},
		/*submit_param*/ {ATTR_JOB_KEEP_CLAIM_IDLE, SUBMIT_KEY_KeepClaimIdle,
				ATTR_JOB_KEEP_CLAIM_IDLE, NULL, false},
		/*submit_param*/ {ATTR_JOB_AD_INFORMATION_ATTRS, SUBMIT_KEY_JobAdInformationAttrs,
				ATTR_JOB_AD_INFORMATION_ATTRS, NULL, true},
		/*submit_param*/ {SUBMIT_KEY_JobMaterializeMaxIdle, ATTR_JOB_MATERIALIZE_MAX_IDLE,
				ATTR_JOB_MATERIALIZE_MAX_IDLE, NULL, false},
		{NULL,NULL,NULL,NULL,false}
	};

	const SimpleExprInfo *i = simple_exprs;
	for( i=simple_exprs; i->key; i++) {
		char *expr;
		expr = submit_param( i->key, i->alt );
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
			AssignJobString(i->attr, expr);
		}
		else {
			AssignJobExpr(i->attr, expr);
		}

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
			AssignJobVal(ATTR_REQUEST_MEMORY, req_memory_mb);
		} else if (MATCH == strcasecmp(tmp,"undefined")) {
		} else {
			AssignJobExpr(ATTR_REQUEST_MEMORY, tmp);
		}
		free(tmp);
	} else if ( (tmp = submit_param(SUBMIT_KEY_VM_Memory)) || (tmp = submit_param(ATTR_JOB_VM_MEMORY)) ) {
		push_warning(stderr, "'%s' was NOT specified.  Using %s = %s. \n", ATTR_REQUEST_MEMORY,ATTR_JOB_VM_MEMORY, tmp );
		AssignJobExpr(ATTR_REQUEST_MEMORY, "MY." ATTR_JOB_VM_MEMORY);
		free(tmp);
	} else if ( (tmp = param("JOB_DEFAULT_REQUESTMEMORY")) ) {
		if (MATCH == strcasecmp(tmp,"undefined")) {
		} else {
			AssignJobExpr(ATTR_REQUEST_MEMORY, tmp);
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
			AssignJobVal(ATTR_REQUEST_DISK, req_disk_kb);
		} else if (MATCH == strcasecmp(tmp,"undefined")) {
		} else {
			AssignJobExpr(ATTR_REQUEST_DISK, tmp);
		}
		free(tmp);
	} else if ( (tmp = param("JOB_DEFAULT_REQUESTDISK")) ) {
		if (MATCH == strcasecmp(tmp,"undefined")) {
		} else {
			AssignJobExpr(ATTR_REQUEST_DISK, tmp);
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
		AssignJobExpr(ATTR_FILE_REMAPS,tmp);
		free(tmp);
	}

	tmp = submit_param( SUBMIT_KEY_BufferFiles, ATTR_BUFFER_FILES );
	if(tmp) {
		AssignJobExpr(ATTR_BUFFER_FILES,tmp);
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
	AssignJobExpr(ATTR_BUFFER_SIZE,tmp);
	free(tmp);

	/* If not buffer block size is given, use 32 KB */

	tmp = submit_param( SUBMIT_KEY_BufferBlockSize, ATTR_BUFFER_BLOCK_SIZE );
	if(!tmp) {
		tmp = param("DEFAULT_IO_BUFFER_BLOCK_SIZE");
		if (!tmp) {
			tmp = strdup("32768");
		}
	}
	AssignJobExpr(ATTR_BUFFER_BLOCK_SIZE,tmp);
	free(tmp);
	return 0;
}


int SubmitHash::SetRequestResources()
{
    RETURN_IF_ABORT();

    std::string attr;

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
        if (val[0]=='\"')
        {
            stringReqRes.insert(rname);
        }

        attr = ATTR_REQUEST_PREFIX; attr.append(rname);
        AssignJobExpr(attr.c_str(), val);
        RETURN_IF_ABORT();
    }
    hash_iter_delete(&it);
    return 0;
}

// generate effective requirements expression by merging default requirements
// with the specified requirements.
//
int SubmitHash::SetRequirements()
{
	RETURN_IF_ABORT();

	char	*ptr;

	MyString answer;
	auto_free_ptr orig(submit_param(SUBMIT_KEY_Requirements));
	if (orig) {
		answer.formatstr( "(%s)", orig.ptr() );
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
		JobRequirements = answer;
		AssignJobExpr (ATTR_REQUIREMENTS, JobRequirements.c_str());
		return abort_code;
	}

	ClassAd req_ad;
	classad::References job_refs;      // job attrs referenced by requirements
	classad::References machine_refs;  // machine attrs referenced by requirements

		// Insert dummy values for attributes of the job to which we
		// want to detect references.  Otherwise, unqualified references
		// get classified as external references.
	req_ad.Assign(ATTR_REQUEST_MEMORY,0);
	req_ad.Assign(ATTR_CKPT_ARCH,"");

	GetExprReferences(answer.Value(),req_ad,&job_refs,&machine_refs);

	bool checks_arch = IsDockerJob || machine_refs.count( ATTR_ARCH );
	bool checks_opsys = IsDockerJob || machine_refs.count( ATTR_OPSYS ) ||
		machine_refs.count( ATTR_OPSYS_AND_VER ) ||
		machine_refs.count( ATTR_OPSYS_LONG_NAME ) ||
		machine_refs.count( ATTR_OPSYS_SHORT_NAME ) ||
		machine_refs.count( ATTR_OPSYS_NAME ) ||
		machine_refs.count( ATTR_OPSYS_LEGACY );
	bool checks_disk =  machine_refs.count( ATTR_DISK );
	bool checks_cpus =   machine_refs.count( ATTR_CPUS );
	bool checks_tdp =  machine_refs.count( ATTR_HAS_TDP );
	bool checks_encrypt_exec_dir = machine_refs.count( ATTR_ENCRYPT_EXECUTE_DIRECTORY );
#if defined(WIN32)
	bool checks_credd = machine_refs.count( ATTR_LOCAL_CREDD );
#endif
	bool	checks_fsdomain = false;
	bool	checks_ckpt_arch = false;
	bool	checks_file_transfer = false;
	bool	checks_file_transfer_plugin_methods = false;
	bool	checks_per_file_encryption = false;
	bool	checks_mpi = false;

	if( JobUniverse == CONDOR_UNIVERSE_STANDARD ) {
		checks_ckpt_arch = job_refs.count( ATTR_CKPT_ARCH );
	}
	if( JobUniverse == CONDOR_UNIVERSE_MPI ) {
		checks_mpi = machine_refs.count( ATTR_HAS_MPI );
	}
	if( mightTransfer(JobUniverse) ) { 
		checks_fsdomain = machine_refs.count(ATTR_FILE_SYSTEM_DOMAIN);
		checks_file_transfer = machine_refs.count(ATTR_HAS_FILE_TRANSFER);
		checks_file_transfer_plugin_methods = machine_refs.count(ATTR_HAS_FILE_TRANSFER_PLUGIN_METHODS);
		checks_per_file_encryption = machine_refs.count(ATTR_HAS_PER_FILE_ENCRYPTION);
	}

	bool checks_mem = machine_refs.count(ATTR_MEMORY);
	//bool checks_reqmem = job_refs.count(ATTR_REQUEST_MEMORY);

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
		bool checks_vm = machine_refs.count( ATTR_HAS_VM );
		if( !checks_vm ) {
			answer += "&& (TARGET." ATTR_HAS_VM " =?= true)";
		}
		// add vm_type to requirements
		bool checks_vmtype = machine_refs.count( ATTR_VM_TYPE);
		if( !checks_vmtype ) {
			answer += " && (TARGET." ATTR_VM_TYPE " == MY." ATTR_JOB_VM_TYPE ")";
		}
		// check if the number of executable VM is more than 0
		bool checks_avail = machine_refs.count(ATTR_VM_AVAIL_NUM);
		if( !checks_avail ) {
			answer += " && (TARGET." ATTR_VM_AVAIL_NUM " > 0)";
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
		answer +=
			" && ((CkptArch =?= UNDEFINED) || (CkptArch == TARGET.Arch))"
			" && ((CkptOpSys =?= UNDEFINED) || (CkptOpSys == TARGET.OpSys))";
	}

	if( !checks_disk ) {
		ExprTree * expr = job->Lookup(ATTR_REQUEST_DISK);
		if (expr) {
			double disk = 0;
			if ( ! ExprTreeIsLiteralNumber(expr, disk) || (disk > 0.0)) {
				answer += " && (TARGET.Disk >= " ATTR_REQUEST_DISK ")";
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
			if (job->Lookup(ATTR_REQUEST_DISK)) {
				answer += " && (TARGET.Disk >= " ATTR_REQUEST_DISK ")";
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
		ExprTree * expr = job->Lookup(ATTR_REQUEST_MEMORY);
		if (expr) {
			double mem = 0;
			if ( ! ExprTreeIsLiteralNumber(expr, mem) || (mem > 1.0)) {
				answer += " && (TARGET.Memory >= " ATTR_REQUEST_MEMORY ")";
			}
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
	}

	if ( JobUniverse != CONDOR_UNIVERSE_GRID ) {
		if ( ! checks_cpus) {
			ExprTree * expr = job->Lookup(ATTR_REQUEST_CPUS);
			if (expr) {
				double cpus = 0;
				if ( ! ExprTreeIsLiteralNumber(expr, cpus) || (cpus > 1.0)) {
					answer += " && (TARGET.Cpus >= " ATTR_REQUEST_CPUS ")";
				}
			}
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
			formatstr(clause, " && regexp(Request%s, TARGET.%s)", rname, rname);
		else
			formatstr(clause, " && (TARGET.%s >= Request%s)", rname, rname);
		answer += clause;
	}
	hash_iter_delete(&it);

	if( !checks_tdp && job->Lookup(ATTR_TOOL_DAEMON_CMD)) {
		answer += " && TARGET." ATTR_HAS_TDP;
	}

	bool encrypt_it = false;
	if( !checks_encrypt_exec_dir &&
		job->LookupBool(ATTR_ENCRYPT_EXECUTE_DIRECTORY, encrypt_it) &&
		encrypt_it) {
		answer += " && TARGET." ATTR_HAS_ENCRYPT_EXECUTE_DIRECTORY;
	}

	if( JobUniverse == CONDOR_UNIVERSE_MPI ) {
		if( ! checks_mpi ) {
			answer += " && TARGET." ATTR_HAS_MPI;
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
		const char * domain_check = "(TARGET." ATTR_FILE_SYSTEM_DOMAIN " == MY." ATTR_FILE_SYSTEM_DOMAIN ")";
		const char * xfer_check = "TARGET." ATTR_HAS_FILE_TRANSFER;
		if (!checks_per_file_encryption &&
				(job->Lookup(ATTR_ENCRYPT_INPUT_FILES) || job->Lookup(ATTR_ENCRYPT_OUTPUT_FILES) ||
				job->Lookup(ATTR_DONT_ENCRYPT_INPUT_FILES) || job->Lookup(ATTR_DONT_ENCRYPT_OUTPUT_FILES))
			) {
			xfer_check = "TARGET." ATTR_HAS_FILE_TRANSFER " && TARGET." ATTR_HAS_PER_FILE_ENCRYPTION;
		}

		ShouldTransferFiles_t should_transfer = STF_IF_NEEDED;
		std::string should;
		if (job->LookupString(ATTR_SHOULD_TRANSFER_FILES, should)) {
			should_transfer = getShouldTransferFilesNum(should.c_str());
		}
		if ( should_transfer == STF_NO) {
				// no file transfer used.  if there's nothing about
				// the FileSystemDomain yet, tack on a clause for
				// that. 
			if( ! checks_fsdomain ) {
				answer += " && " ;
				answer += domain_check;
			}
		} else if ( ! checks_file_transfer) {

			const char * join_op = " && (";
			const char * close_op = ")";
			if ( should_transfer == STF_IF_NEEDED && ! checks_fsdomain ) {
				answer += join_op;
				answer += domain_check;
				join_op = " || (";
				close_op = "))";
			}

			answer += join_op;
			answer += xfer_check;

			if (( ! checks_file_transfer_plugin_methods)) {
				classad::References methods;
				// check input
				auto_free_ptr file_list(submit_param(SUBMIT_KEY_TransferInputFiles, SUBMIT_KEY_TransferInputFilesAlt));
				if (file_list) {
					StringList files(file_list.ptr(), ",");
					for (const char * file = files.first(); file; file = files.next()) {
						if (IsUrl(file)){ methods.insert(getURLType(file)); }
					}
				}

				// check output (only a single file this time)
				file_list.set(submit_param(SUBMIT_KEY_OutputDestination, ATTR_OUTPUT_DESTINATION));
				if (file_list) {
					if (IsUrl(file_list)){
						methods.insert(getURLType(file_list));
					}
				}

				for (auto it = methods.begin(); it != methods.end(); ++it) {
					answer += " && stringListIMember(\"";
					answer += *it;
					answer += "\",TARGET.HasFileTransferPluginMethods)";
				}
			}

			// close of the file transfer requirements
			answer += close_op;
		}
	}

		//
		// Job Deferral
		// If the flag was set true by SetJobDeferral() then we will
		// add the requirement in for this feature
		//
	if (NeedsJobDeferral()) {
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

	JobRequirements = answer;

	AssignJobExpr (ATTR_REQUIREMENTS, JobRequirements.c_str());
	RETURN_IF_ABORT();

	return 0;
}

// this function should be called after SetVMParams() and SetRequirements
int SubmitHash::AppendVMRequirements(MyString &vmanswer, bool VMCheckpoint, bool VMNetworking, const MyString &VMNetworkType, bool VMHardwareVT, bool vm_need_fsdomain)
{
	RETURN_IF_ABORT();
	MyString buffer;
	if ( JobUniverse != CONDOR_UNIVERSE_VM ) {
		return 0;
	}

	ClassAd req_ad;
	classad::References job_refs;      // job attrs referenced by requirements
	classad::References machine_refs;  // machine attrs referenced by requirements

		// Insert dummy values for attributes of the job to which we
		// want to detect references.  Otherwise, unqualified references
		// get classified as external references.
	req_ad.Assign(ATTR_CKPT_ARCH,"");
	req_ad.Assign(ATTR_VM_CKPT_MAC,"");

	GetExprReferences(vmanswer.Value(),req_ad,&job_refs,&machine_refs);

	// check file system domain
	if( vm_need_fsdomain ) {
		// some files don't use file transfer.
		// so we need the same file system domain
		bool checks_fsdomain = false;
		checks_fsdomain = machine_refs.count( ATTR_FILE_SYSTEM_DOMAIN );

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
			AssignJobString(ATTR_FILE_SYSTEM_DOMAIN, my_fsdomain.c_str());
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
	checks_vmmemory = machine_refs.count(ATTR_VM_MEMORY);
	if( !checks_vmmemory ) {
		vmanswer += " && (TARGET.";
		vmanswer += ATTR_VM_MEMORY;
		vmanswer += " >= MY.";
		vmanswer += ATTR_JOB_VM_MEMORY;
		vmanswer += ")";
	}

	if( VMHardwareVT ) {
		bool checks_hardware_vt = false;
		checks_hardware_vt = machine_refs.count(ATTR_VM_HARDWARE_VT);

		if( !checks_hardware_vt ) {
			// add hardware vt to requirements
			vmanswer += " && (TARGET.";
			vmanswer += ATTR_VM_HARDWARE_VT;
			vmanswer += ")";
		}
	}

	if( VMNetworking ) {
		bool checks_vmnetworking = false;
		checks_vmnetworking = machine_refs.count(ATTR_VM_NETWORKING);

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
		checks_ckpt_arch = job_refs.count( ATTR_CKPT_ARCH );
		checks_vm_ckpt_mac = job_refs.count( ATTR_VM_CKPT_MAC );

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

	return 0;
}

int SubmitHash::SetConcurrencyLimits()
{
	RETURN_IF_ABORT();
	MyString tmp = submit_param_mystring(SUBMIT_KEY_ConcurrencyLimits, NULL);
	MyString tmp2 = submit_param_mystring(SUBMIT_KEY_ConcurrencyLimitsExpr, NULL);

	if (!tmp.IsEmpty()) {
		if (!tmp2.IsEmpty()) {
			push_error( stderr, SUBMIT_KEY_ConcurrencyLimits " and " SUBMIT_KEY_ConcurrencyLimitsExpr " can't be used together\n" );
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
			AssignJobString(ATTR_CONCURRENCY_LIMITS, str);
			free(str);
		}
	} else if (!tmp2.IsEmpty()) {
		AssignJobExpr(ATTR_CONCURRENCY_LIMITS, tmp2.Value() );
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
		push_error(stderr, "Invalid " SUBMIT_KEY_AcctGroup ": %s\n", group);
		ABORT_AND_RETURN( 1 );
	}
	if ( !IsValidSubmitterName(group_user.c_str()) ) {
		push_error(stderr, "Invalid " SUBMIT_KEY_AcctGroupUser ": %s\n", group_user.c_str());
		ABORT_AND_RETURN( 1 );
	}

	// set attributes AcctGroup, AcctGroupUser and AccountingGroup on the job ad:

	// store the AcctGroupUser (currently unused)
	AssignJobString(ATTR_ACCT_GROUP_USER, group_user.c_str());

	if (group) {
		// store the group name in the (currently unused) AcctGroup attribute
		AssignJobString(ATTR_ACCT_GROUP, group);

		// store the AccountingGroup attribute as <group>.<user>
		MyString submitter;
		submitter.formatstr("%s.%s", group, group_user.c_str());
		AssignJobString(ATTR_ACCOUNTING_GROUP, submitter.c_str());
	} else {
		// If no group, this is accounting group is really a user alias, just set AccountingGroup to be the user name
		AssignJobString(ATTR_ACCOUNTING_GROUP, group_user.c_str());
	}


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

	AssignJobString(ATTR_TRANSFER_INPUT_FILES, tmp_ptr);
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
	AssignJobString(ATTR_JOB_VM_TYPE, VMType.Value());
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
			AssignJobString(ATTR_JOB_VM_NETWORKING_TYPE,  VMNetworkType.Value());
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
		push_error(stderr, SUBMIT_KEY_VM_Memory " cannot be found.\nPlease specify " SUBMIT_KEY_VM_Memory
			" for vm universe in your submit description file.\n");
		ABORT_AND_RETURN(1);
	}

	int64_t vm_mem;
	parse_int64_bytes(tmp_ptr, vm_mem, 1024*1024);
	if( vm_mem <= 0 ) {
		push_error(stderr, SUBMIT_KEY_VM_Memory " is incorrectly specified\n"
				"For example, for vm memroy of 128 Megabytes,\n"
				"you need to use 128 in your submit description file.\n");
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
	AssignJobVal(ATTR_JOB_VM_VCPUS, VMVCPUS);

	/*
	 * Set the MAC address for this VM.
	 */
	tmp_ptr = submit_param(SUBMIT_KEY_VM_MACAddr, ATTR_JOB_VM_MACADDR);
	if(tmp_ptr)
	{
		AssignJobString(ATTR_JOB_VM_MACADDR, tmp_ptr);
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
	bool vm_no_output_vm = submit_param_bool(SUBMIT_KEY_VM_NO_OUTPUT_VM, NULL, false);
	if( vm_no_output_vm ) {
		AssignJobVal(VMPARAM_NO_OUTPUT_VM, true);
	}

	if( (strcasecmp(VMType.Value(), CONDOR_VM_UNIVERSE_XEN) == MATCH) ||
		(strcasecmp(VMType.Value(), CONDOR_VM_UNIVERSE_KVM) == MATCH) ) {
		bool real_xen_kernel_file = false;
		bool need_xen_root_device = false;

		if ( strcasecmp(VMType.Value(), CONDOR_VM_UNIVERSE_XEN) == MATCH )
		{
			// xen_kernel is a required parameter
			char *xen_kernel = NULL;
			xen_kernel = submit_param(SUBMIT_KEY_VM_XEN_KERNEL);
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
					AssignJobVal(ATTR_JOB_VM_HARDWARE_VT, true);
				}else {
					// real kernel file
					real_xen_kernel_file = true;
					need_xen_root_device = true;
				}
				AssignJobString(VMPARAM_XEN_KERNEL, xen_kernel);
				free(xen_kernel);
			}

			
			// xen_initrd is an optional parameter
			char *xen_initrd = NULL;
			xen_initrd = submit_param(SUBMIT_KEY_VM_XEN_INITRD);
			if( xen_initrd ) {
				if( !real_xen_kernel_file ) {
					push_error(stderr, "To use xen_initrd, "
							"xen_kernel should be a real kernel file.\n");
					ABORT_AND_RETURN(1);
				}
				AssignJobString(VMPARAM_XEN_INITRD, xen_initrd);
				free(xen_initrd);
			}

			if( need_xen_root_device ) {
				char *xen_root = NULL;
				xen_root = submit_param(SUBMIT_KEY_VM_XEN_ROOT);
				if( !xen_root ) {
					push_error(stderr, "'%s' cannot be found.\n"
							"Please specify '%s' for the xen virtual machine "
							"in your submit description file.\n", "xen_root", 
							"xen_root");
					ABORT_AND_RETURN(1);
				}else {
					AssignJobString(VMPARAM_XEN_ROOT, xen_root);
					free(xen_root);
				}
			}
		}// xen only params

		// <x>_disk is a required parameter
		char *disk = submit_param(SUBMIT_KEY_VM_DISK);

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
			AssignJobString( VMPARAM_VM_DISK, disk );
			free(disk);
		}

		if ( strcasecmp(VMType.Value(), CONDOR_VM_UNIVERSE_XEN) == MATCH )
		{
			// xen_kernel_params is a optional parameter
			char *xen_kernel_params = NULL;
			xen_kernel_params = submit_param(SUBMIT_KEY_VM_XEN_KERNEL_PARAMS);
			if( xen_kernel_params ) {
				MyString fixedvalue = delete_quotation_marks(xen_kernel_params);
				AssignJobString( VMPARAM_XEN_KERNEL_PARAMS, xen_kernel_params );
				free(xen_kernel_params);
			}
		}

	}else if( strcasecmp(VMType.Value(), CONDOR_VM_UNIVERSE_VMWARE) == MATCH ) {
		bool knob_exists = false;
		bool vmware_should_transfer_files = submit_param_bool(SUBMIT_KEY_VM_VMWARE_SHOULD_TRANSFER_FILES, NULL, false, &knob_exists);
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
		bool vmware_snapshot_disk = submit_param_bool(SUBMIT_KEY_VM_VMWARE_SNAPSHOT_DISK, NULL, false);
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
		vmware_dir = submit_param(SUBMIT_KEY_VM_VMWARE_DIR);
		if ( vmware_dir ) {
			MyString f_dirname = delete_quotation_marks(vmware_dir);
			free(vmware_dir);

			f_dirname = full_path(f_dirname.Value(), false);
			check_and_universalize_path(f_dirname);

			AssignJobString(VMPARAM_VMWARE_DIR, f_dirname.Value());

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
			AssignJobString(VMPARAM_VMWARE_VMX_FILE,condor_basename(vmx_files.next()));
		}

		tmp_ptr = vmdk_files.print_to_string();
		if ( tmp_ptr ) {
			AssignJobString(VMPARAM_VMWARE_VMDK_FILES, tmp_ptr);
			free( tmp_ptr );
		}
	}

	// Now all VM parameters are set
	// So we need to add necessary VM attributes to Requirements
	MyString vmanswer;
	if (AppendVMRequirements(vmanswer, VMCheckpoint, VMNetworking, VMNetworkType, VMHardwareVT, vm_need_fsdomain) != 0) {
		return abort_code;
	}

	// TJ: this could be cleaner if vmanswer didn't always start with &&
	if ( ! vmanswer.empty()) {
		std::string tmp;
		tmp.reserve(JobRequirements.size() + vmanswer.size() + 10);
		tmp = "(";
		tmp += JobRequirements;
		tmp += ")";
		tmp += vmanswer;
		JobRequirements = tmp.c_str();
		AssignJobExpr(ATTR_REQUIREMENTS, JobRequirements.c_str());
		RETURN_IF_ABORT();
	}

	return 0;
}

int SubmitHash::process_input_file_list(StringList * input_list, long long & accumulate_size_kb)
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
		return count;
	}
	return 0;
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
	MyString tmp;
	bool in_files_specified = false;
	bool out_files_specified = false;
	StringList input_file_list(NULL, ",");
	StringList output_file_list(NULL,",");
	ShouldTransferFiles_t should_transfer = STF_IF_NEEDED;
	MyString output_remaps;

	macro_value = submit_param( SUBMIT_KEY_TransferInputFiles, SUBMIT_KEY_TransferInputFilesAlt ) ;
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

	if (process_input_file_list(&input_file_list, TransferInputSizeKb) > 0) {
		in_files_specified = true;
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

	macro_value = submit_param( SUBMIT_KEY_TransferOutputFiles, SUBMIT_KEY_TransferOutputFilesAlt );
	if( macro_value ) 
	{
		// as a special case transferoutputfiles="" will produce an empty list of output files, not a syntax error
		// PRAGMA_REMIND("replace this special case with code that correctly parses any input wrapped in double quotes")
		if (macro_value[0] == '"' && macro_value[1] == '"' && macro_value[2] == 0) {
			output_file_list.clearAll();
			out_files_specified = true;
		} else {
			output_file_list.initializeFromString(macro_value);
			for (const char * file = output_file_list.first(); file != NULL; file = output_file_list.next()) {
				out_files_specified = true;
				tmp = file;
				if (check_and_universalize_path(tmp) != 0)
				{
					// we universalized the path, so update the string list
					output_file_list.deleteCurrent();
					output_file_list.insert(tmp.Value());
				}
			}
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
	bool default_should = false;
	bool default_when;
	FileTransferOutput_t when_output;
	MyString err_msg;
	
	// check to see if the user specified should_transfer_files.
	// if they didn't check to see if the admin did. 
	auto_free_ptr should_param(submit_param(ATTR_SHOULD_TRANSFER_FILES, SUBMIT_KEY_ShouldTransferFiles));
	if ( ! should_param) {
		should_param.set(param("SUBMIT_DEFAULT_SHOULD_TRANSFER_FILES"));
		if (should_param) {
			if (getShouldTransferFilesNum(should_param) < 0) {
				// if config default for should transfer files is invalid, just ignore it.
				// otherwise all submits would generate an error/warning which the users cannot fix.
				should_param.clear();
			} else {
				// admin specified should_transfer_files counts as a default should
				default_should = true;
			}
		}
	}

	should = should_param;
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
	}

	if (should_transfer == STF_NO &&
		(in_files_specified || out_files_specified)) { // (F)
		err_msg = "\nERROR: you specified files you want Condor to transfer via \"";
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

	when = submit_param(ATTR_WHEN_TO_TRANSFER_OUTPUT, SUBMIT_KEY_WhenToTransferOutput);
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

	// actually shove the file transfer 'when' and 'should' into the job ad
	//
	if( should_transfer != STF_NO ) {
		if( ! when_output ) {
			push_error(stderr, "InsertFileTransAttrs() called we might transfer "
					   "files but when_output hasn't been set" );
			abort_code = 1;
			return abort_code;
		}
	}

	//
	// END FILE TRANSFER VALIDATION
	//

	AssignJobString(ATTR_SHOULD_TRANSFER_FILES, getShouldTransferFilesString( should_transfer ));
	if (should_transfer != STF_NO) {
		AssignJobString(ATTR_WHEN_TO_TRANSFER_OUTPUT, getFileTransferOutputString( when_output ));
	}

	// if should transfer files is not YES, then we need to make sure that
	// our filesystem domain is published in the job ad because it will be needed for matchmaking
	if (should_transfer != STF_YES) {
		auto_free_ptr fs_domain(param("FILESYSTEM_DOMAIN"));
		if (fs_domain) {
			AssignJobString(ATTR_FILE_SYSTEM_DOMAIN, fs_domain);
		}
	}

		/*
		  If we're dealing w/ TDP and we might be transfering files,
		  we want to make sure the tool binary and input file (if
		  any) are included in the transfer_input_files list.
		*/
	if( should_transfer!=STF_NO && (tdp_cmd || tdp_input) ) {
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
			AssignJobString(ATTR_TRANSFER_INPUT_FILES, file_list_tdp.Value() );
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
			for (const char * file = files.first(); file != NULL; file = files.next()) {
				tmp = file;
				input_file_list.append(tmp.Value());
			}
			free(macro_value);
		}

		if (process_input_file_list(&input_file_list, TransferInputSizeKb) > 0) {
			in_files_specified = true;
		}

		AssignJobString( ATTR_JOB_CMD, "java");

		AssignJobVal(ATTR_TRANSFER_EXECUTABLE, false);
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
		bool StreamStdout = false;
		bool StreamStderr = false;

		job->LookupString(ATTR_JOB_OUTPUT,output);
		job->LookupString(ATTR_JOB_ERROR,error);
		job->LookupBool(ATTR_STREAM_OUTPUT, StreamStdout);
		job->LookupBool(ATTR_STREAM_ERROR, StreamStderr);

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
			AssignJobString(ATTR_JOB_OUTPUT, working_name);

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
			AssignJobString(ATTR_JOB_ERROR, working_name);

			if(!output_remaps.IsEmpty()) output_remaps += ";";
			output_remaps.formatstr_cat("%s=%s",working_name,error.EscapeChars(";=\\",'\\').Value());
		}
	}

	// if we might be using file transfer, insert in input/output
	// exprs  
	if( should_transfer != STF_NO ) {
		if (in_files_specified) {
			auto_free_ptr input_files(input_file_list.print_to_string());
			AssignJobString (ATTR_TRANSFER_INPUT_FILES, input_files);
		}
#ifdef HAVE_HTTP_PUBLIC_FILES
		char *public_input_files = 
			submit_param(SUBMIT_KEY_PublicInputFiles, ATTR_PUBLIC_INPUT_FILES);
		if (public_input_files) {
			StringList pub_inp_file_list(NULL, ",");
			pub_inp_file_list.initializeFromString(public_input_files);
			// Process file list, but output string is for ATTR_TRANSFER_INPUT_FILES,
			// so it's not used here.
			process_input_file_list(&pub_inp_file_list, TransferInputSizeKb);
			if (pub_inp_file_list.isEmpty() == false) {
				char *inp_file_str = pub_inp_file_list.print_to_string();
				if (inp_file_str) {
					AssignJobString(ATTR_PUBLIC_INPUT_FILES, inp_file_str);
					free(inp_file_str);
				}
			}
			free(public_input_files);
		} 
#endif

		if ( out_files_specified ) {
			if (output_file_list.isEmpty()) {
				// transferoutputfiles="" will produce an empty list of output files
				// (this is so you can prevent the output directory from being transferred back, but still allowing for input transfer)
				AssignJobString (ATTR_TRANSFER_OUTPUT_FILES, "");
			} else {
				auto_free_ptr output_files(output_file_list.print_to_string());
				AssignJobString (ATTR_TRANSFER_OUTPUT_FILES, output_files);
			}
		}
	}

	if( should_transfer == STF_NO && 
		JobUniverse != CONDOR_UNIVERSE_GRID &&
		JobUniverse != CONDOR_UNIVERSE_JAVA &&
		JobUniverse != CONDOR_UNIVERSE_VM )
	{
		char *transfer_exe;
		transfer_exe = submit_param(SUBMIT_KEY_TransferExecutable, ATTR_TRANSFER_EXECUTABLE);
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
		AssignJobString(ATTR_TRANSFER_OUTPUT_REMAPS, output_remaps.Value());
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
		AssignJobExpr(ATTR_MAX_TRANSFER_INPUT_MB,MaxTransferInputExpr);
		free( MaxTransferInputExpr );
	}
	if( MaxTransferOutputExpr ) {
		AssignJobExpr(ATTR_MAX_TRANSFER_OUTPUT_MB,MaxTransferOutputExpr);
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

	if ( ! ad) {
		this->clusterAd = NULL;
		return 0;
	}

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
	ComputeIWD();
	return 0;
}

int SubmitHash::init_base_ad(time_t submit_time_in, const char * owner)
{
	MyString buffer;
	ASSERT(owner);
	submit_owner = owner;

	delete job; job = NULL;
	delete procAd; procAd = NULL;
	baseJob.Clear();
	base_job_is_cluster_ad = false;

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

// after calling make_job_ad for the Procid==0 ad, pass the returned job ad to this function
// to fold the job attributes into the base ad, thereby creating an internal clusterad (owned by the SubmitHash)
// The passed in job ad will be striped down to a proc0 ad and chained to the internal clusterad
// it is an error to pass any ad other than the most recent ad returned by make_job_ad()
// After calling this method, subsequent calls to make_job_ad() will produce a job ad that
// is chained to the cluster ad
// This function does nothing if the SubmitHash is using a foreign clusterad (i.e. you called set_cluster_ad())
//
bool SubmitHash::fold_job_into_base_ad(ClassAd * jobad)
{
	// its only valid to call this function if not using a foreign clusterad
	// and if the job passed in is the same as the job we just returned from make_job_ad()
	if (clusterAd || ! jobad) {
		return false;
	}

	jobad->ChainToAd(NULL); // make sure that there is not currently a chained parent
	int procid = -1;
	if ( ! jobad->LookupInteger(ATTR_PROC_ID, procid) || procid < 0) {
		return false;
	}

	// grab the status from the jobad, this attribute is also one we want to force into the procad
	// although unlike the procid, it's ok if it is also in the cluster ad
	int status = IDLE;
	bool has_status = jobad->LookupInteger(ATTR_JOB_STATUS, status);

	// move all of the attributes from the job to the parent.
	baseJob.Update(*jobad);

	// put the proc id (and possibly status) back into the (now empty) jobad
	jobad->Clear();
	jobad->Assign(ATTR_PROC_ID, procid);
	if (has_status) jobad->Assign(ATTR_JOB_STATUS, status);

	// make sure that the base job has no procid assigment.
	baseJob.Delete(ATTR_PROC_ID);
	base_job_is_cluster_ad = true;

	// chain the job to the base clusterad
	jobad->ChainToAd(&baseJob);
	return true;
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

	if (clusterAd) {
		procAd = new ClassAd();
		procAd->ChainToAd(clusterAd);
	} else if ((jid.proc > 0) && base_job_is_cluster_ad) {
		procAd = new ClassAd();
		procAd->ChainToAd(&baseJob);
	} else {
		procAd = new ClassAd(baseJob);
	}
	job = new DeltaClassAd(*procAd);

	// really a command, needs to happen before any calls to check_open
	JobDisableFileChecks = submit_param_bool(SUBMIT_CMD_skip_filechecks, NULL, false);
	//PRAGMA_REMIND("TODO: several bits of grid code are ignoring JobDisableFileChecks and bypassing FnCheckFile, check to see if that is kosher.")

#if !defined(WIN32)
	SetRootDir();	// must be called very early
	if (!clusterAd) { // if no clusterAd, we also want to check for access
		if (check_root_dir_access()) { return NULL; }
	}
#endif
	SetIWD();		// must be called very early

	SetExecutable();

	SetDescription();
	SetMachineCount();

	// Note: Due to the unchecked use of global variables everywhere in
	// condor_submit, the order that we call the below Set* functions
	// is very important!
	SetJobStatus();
	SetPriority();
	SetNiceUser();
	SetMaxJobRetirementTime(); // must be after SetUniverse
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

	SetUserLog();
	SetCoreSize();
#if !defined(WIN32)
	SetKillSig();
#endif
	SetRank();
	SetStdin();
	SetStdout();
	SetStderr();
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
	} else if (procAd) {
		if (procAd->GetChainedParentAd()) {
			#if 0 // the delta ad does this so we don't need to
			// remove duplicate attributes between procad and chained parent
			procAd->PruneChildAd();
			#endif
			// we need to make sure that that job status is in the proc ad
			// because the job counters by status in the schedd depends on this.
			if ( ! procAd->LookupIgnoreChain(ATTR_JOB_STATUS)) {
				ClassAd::CopyAttribute(ATTR_JOB_STATUS, *procAd, ATTR_JOB_STATUS, *procAd->GetChainedParentAd());
			}
		} else if ( ! clusterAd && ! base_job_is_cluster_ad) {
			// promote the procad to a clusterad
			fold_job_into_base_ad(procAd);
		}
	}
	return procAd;
}

void SubmitHash::insert_source(const char * filename, MACRO_SOURCE & source)
{
	::insert_source(filename, SubmitMacroSet, source);
}

// Check to see if this is a queue statement, if it is, return a pointer to the queue arguments.
// 
const char * SubmitHash::is_queue_statement(const char * line)
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


// destructively split the item, inserting \0 to terminate and trim and returning a vector of pointers to start of each value
int SubmitForeachArgs::split_item(char* item, std::vector<const char*> & values)
{
	values.clear();
	values.reserve(vars.number());
	if ( ! item) return 0;

	const char* token_seps = ", \t";
	const char* token_ws = " \t";

	char * var = vars.first();
	char * data = item;

	// skip leading separators and whitespace
	while (*data == ' ' || *data == '\t') ++data;
	values.push_back(data);

	// check for the use of US as a field separator
	// if we find one, then use that instead of the default token separator
	// In this case US is the only field separator, but we still want to trim
	// whitespace both before and after the values.  we do this because the submit hash
	// essentially assumes that all values have been pre-trimmed of whitespace
	// also, just be helpful, we also trim trailing \r\n from the data, although
	// for most use cases of this function, the caller will have already trimmed those.
	char * pus = strchr(data, '\x1F');
	if (pus) {
		for (;;) {
			*pus = 0;
			// trim token separator and also trailing whitespace
			char * endp = pus-1;
			while (endp >= data && (*endp == ' ' || *endp == '\t')) *endp-- = 0;
			if ( ! var) break;

			// advance to the next field and skip leading whitespace
			data = pus+1;
			while (*data == ' ' || *data == '\t') ++data;
			pus = strchr(data, '\x1F');
			var = vars.next();
			if (var) {
				values.push_back(data);
			}
			if ( ! pus) {
				// last field, check for trailing whitespace and \r\n 
				pus = data + strlen(data);
				if (pus > data && pus[-1] == '\n') --pus;
				if (pus > data && pus[-1] == '\r') --pus;
				if (pus == data) {
					// we ran out of fields!
					// we ran out of fields! use terminating null for all of the remaining fields
					while ((var = vars.next())) { values.push_back(data); }
				}
			}
		}
		return (int)values.size();
	}

	// if we get to here US was not the field separator, so use token_seps instead.

	// if there is more than a single loop variable, then assign them as well
	// we do this by destructively null terminating the item for each var
	// the last var gets all of the remaining item text (if any)
	while ((var = vars.next())) {
		// scan for next token separator
		while (*data && ! strchr(token_seps, *data)) ++data;
		// null terminate the previous token and advance to the start of the next token.
		if (*data) {
			*data++ = 0;
			// skip leading separators and whitespace
			while (*data && strchr(token_ws, *data)) ++data;
			values.push_back(data);
		}
	}

	return (int)values.size();
}

// destructively split the item, inserting \0 to terminate and trim
// populates a map with a key->value pair for each value. and returns the number of values
int SubmitForeachArgs::split_item(char* item, NOCASE_STRING_MAP & values)
{
	values.clear();
	if ( ! item) return 0;

	std::vector<const char*> splits;
	split_item(item, splits);

	int ix = 0;
	for (const char * key = vars.first(); key != NULL; key = vars.next()) {
		values[key] = splits[ix++];
	}
	return (int)values.size();
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
	SubmitForeachArgs & o,                 // IN,OUT: options & items from parsing the queue args
	bool allow_stdin,                      // IN: allow items to be read from stdin.
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
			if ( ! allow_stdin) {
				errmsg = "QUEUE FROM - (read from stdin) is not allowed in this context";
				return -1;
			}
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
	char * queue_args = const_cast<char*>(SubmitHash::is_queue_statement(line));
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
	MacroStreamYourFile ms(fp, source);
	return parse_up_to_q_line(ms, errmsg, qline);
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

enum FixupKeyId {
	idKeyNone=0,
	idKeyExecutable,
	idKeyInitialDir,
};

// struct for a table mapping attribute names to a flag indicating that the attribute
// must only be in cluster ad or only in proc ad, or can be either.
//
typedef struct digest_fixup_key {
	const char * key;
	FixupKeyId   id; //
	// a LessThan operator suitable for inserting into a sorted map or set
	bool operator<(const struct digest_fixup_key& rhs) const {
		return strcasecmp(this->key, rhs.key) < 0;
	}
} DIGEST_FIXUP_KEY;

// table if submit keywords that require special processing when building a submit digest
// NOTE: this table MUST be sorted by case-insensitive value of the first field.
static const DIGEST_FIXUP_KEY aDigestFixupAttrs[] = {
	                                            // these should end  up sorted case-insenstively
	{ ATTR_JOB_CMD,             idKeyExecutable }, // "Cmd"
	{ SUBMIT_KEY_Executable,    idKeyExecutable }, // "executable"
	{ SUBMIT_KEY_InitialDirAlt, idKeyInitialDir }, // "initial_dir" <- special case legacy hack (sigh) note '_' sorts before 'd' OR 'D'
	{ SUBMIT_KEY_InitialDir,    idKeyInitialDir }, // "initialdir"
	{ ATTR_JOB_IWD,             idKeyInitialDir }, // "Iwd"
	{ SUBMIT_KEY_JobIwd,        idKeyInitialDir }, // "job_iwd"     <- special case legacy hack (sigh)
};

// while building a submit digest, fixup right hand side for certain key=rhs pairs
// for now this is mostly used to promote some paths to fully qualified paths.
void SubmitHash::fixup_rhs_for_digest(const char * key, std::string & rhs)
{
	const DIGEST_FIXUP_KEY* found = NULL;
	found = BinaryLookup<DIGEST_FIXUP_KEY>(aDigestFixupAttrs, COUNTOF(aDigestFixupAttrs), key, strcasecmp);
	if ( ! found)
		return;

	// the Executable and InitialDir should be expanded to a fully qualified path here.
	if (found->id == idKeyExecutable || found->id == idKeyInitialDir) {
		if (rhs.empty()) return;
		const char * path = rhs.c_str();
		if (strstr(path, "$$(")) return; // don't fixup if there is a pending $$() expansion.
		if (IsUrl(path)) return; // don't fixup URL paths
		// Convert to a full path it not already a full path
		rhs = full_path(path, false);
	}
}

const char* SubmitHash::make_digest(std::string & out, int cluster_id, StringList & vars, int options)
{
	int flags = HASHITER_NO_DEFAULTS;
	out.reserve(SubmitMacroSet.size * 80); // make a guess at how much space we need.

	std::string rhs;

	// when we selectively expand the submit hash, we want to skip over some knobs
	// because their values can change as we materialize jobs.
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

	// some knobs should never appear in the digest (i'm looking at you getenv)
	// we build of set of those knobs here
	classad::References omit_knobs;
	if (options == 0) { // options == 0 is the default behavior, perhaps turn this into a set of flags in the future?
		omit_knobs.insert(SUBMIT_CMD_GetEnvironment);
		omit_knobs.insert(SUBMIT_CMD_GetEnvironmentAlt);
		omit_knobs.insert(SUBMIT_CMD_AllowStartupScript);
		omit_knobs.insert(SUBMIT_CMD_AllowStartupScriptAlt);
	}

	HASHITER it = hash_iter_begin(SubmitMacroSet, flags);
	for ( ; ! hash_iter_done(it); hash_iter_next(it)) {
		const char * key = hash_iter_key(it);

		// ignore keys that are in the 'omit' set. They should never be copied into the digest.
		if (omit_knobs.find(key) != omit_knobs.end()) continue;

		if (key && key[0] == '$') continue; // dont dump meta params.
		const char * val = hash_iter_value(it);
		out += key;
		out += "=";
		if (val) {
			rhs = val;
			selective_expand_macro(rhs, skip_knobs, SubmitMacroSet, mctx);
			fixup_rhs_for_digest(key, rhs);
			out += rhs;
		}
		out += "\n";
	}
	hash_iter_delete(&it);
	return out.c_str();
}


