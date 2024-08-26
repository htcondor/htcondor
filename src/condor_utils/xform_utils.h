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

#if !defined(_XFORM_UTILS_H)
#define _XFORM_UTILS_H

#include "submit_utils.h"  // for class MacroStreamCharSource

class MacroStreamXFormSource;
class XFormHash;

// in-place transform of a single ad 
// using the transform rules in xfm,
// variables live in the hashtable mset.
// returns < 0 on failure, errmsg will have details
//
#define XFORM_UTILS_LOG_ERRORS      0x0001  // Log errors to stderr or dprintf
#define XFORM_UTILS_LOG_STEPS       0x0002  // Log errors to stdout or dprintf
#define XFORM_UTILS_LOG_TO_DPRINTF  0xFF00  // Dprintf category, if 0 log to stdout/stderr
int TransformClassAd (
	ClassAd * input_ad,           // the ad to be transformed
	MacroStreamXFormSource & xfm, // the set of transform rules
	XFormHash & mset,             // the hashtable used as temporary storage
	std::string & errmsg,
	unsigned int  flags=0);  // One or move of XFORM_UTILS_* flags

/*
Basic workflow for classad transformation is 
 
init the macro set that the transform will use for temporary variables

  XFormHash mset;
  mset.init();

Load a transform source
  MacroStreamXFormSource xfm;
  xfm.load(FILE*, source)
or
  xfm.open(std::vector<std::string> & statements, source) (or XFormLoadFromClassadJobRouterRoute() which calls this)
 
Save the hashtable state (so we can revert changes before each time we transform)
  chkpt = mset.save_state();

Transform a set of ads
  while (ads to transform) {
     // clean hashtable.
     mset.rewind_to_state(chkpt, false);
 
     // apply transforms
     TransformClassAd(ad, xfm, mset, errmsg, 0);
     TransformClassAd(ad, xfm2, mset, errmsg, 0);
  }

*/


// declare enough of the condor_params structure definitions so that we can define hashtable defaults
namespace condor_params {
	typedef struct string_value { const char * psz; int flags; } string_value;
	struct key_value_pair { const char * key; const string_value * def; };
}


// The MacroStreamXFormSource holds a set of ClassAd transform commands
// 
class MacroStreamXFormSource : public MacroStreamCharSource
{
public:
	MacroStreamXFormSource(const char *nam=NULL);
	virtual ~MacroStreamXFormSource();
	// returns:
	//   < 0 = error, outmsg is error message
	//   > 0 = number of statements in MacroStreamCharSource, outmsg is TRANSFORM statement or empty
	// 
	int load(FILE* fp, MACRO_SOURCE & source, std::string & errmsg);
	int open(const char * statements, int & offset, std::string & errmsg);
private:
	int open(std::vector<std::string> & statements, const MACRO_SOURCE & source, std::string & errmsg);
public:

	//bool open(const char * src_string, const MACRO_SOURCE & source); // this is the base class method...

	bool matches(ClassAd * candidate_ad);

	const char * getName() { return name.c_str(); }
	const char * setName(const char * nam) { name = nam; return getName(); }
	int getUniverse() const { return universe; }
	int setUniverse(int uni) { universe=uni; return universe; }
	int setUniverse(const char * uni);

	classad::ExprTree* getRequirements() { return requirements.Expr(); }
	classad::ExprTree* setRequirements(const char * require, int & err);
	const char * getRequirementsStr() { return requirements.c_str(); }

	// these are used only when the source has an iterating TRANSFORM command
	bool has_pending_fp() { return fp_iter != NULL; }
	bool close_when_done(bool close) { close_fp_when_done = close; return has_pending_fp(); }
	int  parse_iterate_args(char * pargs, int expand_options, XFormHash &mset, std::string & errmsg);
	int will_iterate(XFormHash &mset, std::string & errmsg) {
		if (iterate_init_state <= 1) return iterate_init_state;
		return init_iterator(mset, errmsg);
	}
	bool iterate_init_pending() const { return iterate_init_state > 1; }
	bool first_iteration(XFormHash &mset); // returns true if next_iteration should be called
	bool next_iteration(XFormHash &mset);  // returns true if there was a next
	void clear_iteration(XFormHash &mset); // clean up iteration variables in the hashtable
	void reset(XFormHash & mset); // reset the iterator variables

	MACRO_EVAL_CONTEXT_EX& context() { return ctx; }
	const char * getFormattedText(std::string & buf, const char *prefix="", bool include_comments=false); // return full rule, incl NAME and REQs
	const char * getText() { return file_string.ptr(); } // return active text of the transform, \n delimited.
	const char * getIterateArgs() { return iterate_args.ptr(); }
protected:
	std::string name;
	ConstraintHolder requirements;
	int universe; 
	MACRO_SET_CHECKPOINT_HDR * checkpoint;
	MACRO_EVAL_CONTEXT_EX ctx;

	// these are used when the transform iterates
	FILE* fp_iter; // when load stops at a TRANSFORM line, this holds the fp until parse_iterate_args is called
	int   fp_lineno;
	int   step;
	int   row;
	int   proc;
	bool  close_fp_when_done;
	int   iterate_init_state;
	SubmitForeachArgs oa;
	auto_free_ptr iterate_args; // copy of the arguments from the ITERATE line, set by load()
	auto_free_ptr curr_item; // so we can destructively edit the current item from the items list

	int set_iter_item(XFormHash &mset, const char* item);
	int init_iterator(XFormHash &mset, std::string & errmsg);
	//int report_empty_items(XFormHash& mset, std::string errmsg);
};


// The XFormHash class is used as scratch space and temporary variable storage
// for Classad transforms.
//
class XFormHash {
public:
	enum Flavor {
		Iterating=0, // enable Process/Step/Row iteration for LocalMacroSet (used by condor_transform_ads)
		Basic,       // use a mininimal set of defaults for LocalMacroSet (basically IsWindows/IsLinux, used by JobRouter)
		ParamTable,  // use config defaults as defaults for LocalMacroSet (WARNING! ignores config files!)
	};

	XFormHash(Flavor _flavor=Basic);
	~XFormHash();

	void init();
	void clear(); // clear, but do not deallocate
	void set_flavor(Flavor _flavor); // clear and set new flavor

	char * local_param(const char* name, const char* alt_name, MACRO_EVAL_CONTEXT & ctx);
	char * local_param(const char* name, MACRO_EVAL_CONTEXT & ctx) { return local_param(name, NULL, ctx); }
	char * expand_macro(const char* value, MACRO_EVAL_CONTEXT & ctx) { return ::expand_macro(value, LocalMacroSet, ctx); }
	const char * lookup(const char* name, MACRO_EVAL_CONTEXT & ctx) { return lookup_macro(name, LocalMacroSet, ctx); }

	// type converting local params
	bool local_param_string(const char * name, std::string & value, MACRO_EVAL_CONTEXT & ctx); // returns true if string exists
	bool local_param_unquoted_string(const char * name, std::string & value, MACRO_EVAL_CONTEXT & ctx); // returns true if string exists
	bool local_param_bool(const char * name, bool def_value, MACRO_EVAL_CONTEXT & ctx, bool * pfExist=NULL);
	int local_param_int(const char * name, int def_value, MACRO_EVAL_CONTEXT & ctx, bool * pfExist=NULL);
	double local_param_double(const char * name, double def_value, MACRO_EVAL_CONTEXT & ctx, bool * pfExist=NULL);

	void insert_source(const char * filename, MACRO_SOURCE & source);
	void set_RulesFile(const char * filename, MACRO_SOURCE & source);
	void set_local_param(const char* name, const char* value, MACRO_EVAL_CONTEXT & ctx);
	void set_arg_variable(const char* name, const char* value, MACRO_EVAL_CONTEXT & ctx);
	void set_local_param_used(const char* name);
	void setup_macro_defaults(); // setup live defaults table

	// stuff value into the submit's hashtable and mark 'name' as a used param
	// this function is intended for use during queue iteration to stuff changing values like $(Cluster) and $(Process)
	// Because of this the function does NOT make a copy of value, it's up to the caller to
	// make sure that value is not changed for the lifetime of possible macro substitution.
	void set_live_variable(const char* name, const char* live_value, MACRO_EVAL_CONTEXT & ctx);
	void set_iterate_step(int step, int proc);
	void set_iterate_row(int row, bool iterating);
	void set_factory_vars(int isCluster, bool lateMat);
	void clear_live_variables() const;

	const char * get_RulesFilename();
	MACRO_ITEM* lookup_exact(const char * name) { return find_macro_item(name, NULL, LocalMacroSet); }
	CondorError* error_stack() const {return LocalMacroSet.errors;}
	void warn_unused(FILE* out, const char *app);

	void dump(FILE* out, int flags);
	void push_error(FILE * fh, const char* format, ... ) const CHECK_PRINTF_FORMAT(3,4);
	void push_warning(FILE * fh, const char* format, ... ) const CHECK_PRINTF_FORMAT(3,4);

	MACRO_SET& macros() { return LocalMacroSet; }
	MACRO_SET_CHECKPOINT_HDR * save_state();
	bool rewind_to_state(MACRO_SET_CHECKPOINT_HDR * state, bool and_delete);

protected:
	MACRO_SET LocalMacroSet;
	Flavor    flavor;

	// automatic 'live' submit variables. these pointers are set to point into the macro set allocation
	// pool. so the will be automatically freed. They are also set into the macro_set.defaults tables
	// so that whatever is set here will be found by $(Key) macro expansion.
	char* LiveProcessString;
	char* LiveRowString;
	char* LiveStepString;
	condor_params::string_value * LiveRulesFileMacroDef;
	condor_params::string_value * LiveIteratingMacroDef;

	// private helper functions
private:
	// disallow assignment and copy construction
	XFormHash(const XFormHash & that);
	XFormHash& operator=(const XFormHash & that);
};

// do one time global initialization for the XFormHash class
// (in future maybe re-init after reconfig)
const char * init_xform_default_macros();

// validate the transform commands in xfm, and populate the mset with variable declarations
bool ValidateXForm (
	MacroStreamXFormSource & xfm,  // the set of transform rules
	XFormHash & mset,              // the hashtable used as temporary storage
	int * step_count,              // if non-null returns the number of transform commands
	std::string & errmsg);          // holds parse errors on failure

// load a MacroStreamXFormSource from a jobrouter classad style route.
// returns < 0 on failure, >= 0 on success (positive values are number of statements in the route)
int XFormLoadFromClassadJobRouterRoute (
	MacroStreamXFormSource & xform,
	const std::string & routing_string,
	int & offset,
	const classad::ClassAd & base_route_ad,
	int options);

// load a MacroStreamXFormSource statements  from a jobrouter classad style route
// use this function rather than the one above if you want to ammend the
// statements before initializing the xform with them.
// reads a new-classad style ad from routing_string starting at offset, and updates offset
// returns 0 if the routing_string does not parse, 1 if it does
int ConvertClassadJobRouterRouteToXForm (
	std::vector<std::string> &statements,
	std::string & name, // name from config on input, overwritten with name from route ad if it has one
	const std::string & routing_string,
	int & offset,
	const classad::ClassAd & base_route_ad,
	int options);

// pass these options to XFormLoadFromJobRouterRoute to have it fixup some
// some brokenness in the default JobRouter route.
#define XForm_ConvertJobRouter_Remove_InputRSL 0x00001
#define XForm_ConvertJobRouter_Fix_EvalSet     0x00002

#endif // _XFORM_UTILS_H

