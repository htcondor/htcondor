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
#include <algorithm>
#include "compat_classad.h"

#include "condor_classad.h"
#include "classad_oldnew.h"
#include "condor_attributes.h"
#include "classad/xmlSink.h"
#include "condor_config.h"
#include "condor_regex.h"
#include "classad/classadCache.h"
#include "env.h"
#include "condor_arglist.h"
#define CLASSAD_USER_MAP_RETURNS_STRINGLIST 1

#include <unordered_set>

#if defined(UNIX)
#include <dlfcn.h>
#endif

// Utility to clarify a couple of evaluations
static inline bool
IsStringEnd(const char *str, unsigned off)
{
	return (  (str[off] == '\0') || (str[off] == '\n') || (str[off] == '\r')  );
}

static std::vector<std::string> ClassAdUserLibs;

static void registerClassadFunctions();
static void classad_debug_dprintf(const char *s);

namespace {

typedef std::unordered_set<std::string, classad::ClassadAttrNameHash, classad::CaseIgnEqStr> classad_hashmap;
classad_hashmap ClassAdPrivateAttrs = { ATTR_CAPABILITY,
		ATTR_CHILD_CLAIM_IDS, ATTR_CLAIM_ID, ATTR_CLAIM_ID_LIST,
		ATTR_CLAIM_IDS, ATTR_TRANSFER_KEY };

}

static bool ClassAd_initConfig = false;
static bool ClassAd_strictEvaluation = false;

void ClassAdReconfig()
{
	//ClassAdPrivateAttrs.rehash(11);
	ClassAd_strictEvaluation = param_boolean( "STRICT_CLASSAD_EVALUATION", false );
	classad::SetOldClassAdSemantics( !ClassAd_strictEvaluation );

	classad::ClassAdSetExpressionCaching( param_boolean( "ENABLE_CLASSAD_CACHING", false ) );

	char *new_libs = param( "CLASSAD_USER_LIBS" );
	if ( new_libs ) {
		for (const auto& new_lib: StringTokenIterator(new_libs)) {
			if ( !contains(ClassAdUserLibs, new_lib) ) {
				if ( classad::FunctionCall::RegisterSharedLibraryFunctions(new_lib.c_str()) ) {
					ClassAdUserLibs.emplace_back(new_lib);
				} else {
					dprintf( D_ALWAYS, "Failed to load ClassAd user library %s: %s\n",
							 new_lib.c_str(), classad::CondorErrMsg.c_str() );
				}
			}
		}
		free(new_libs);
	}

	reconfig_user_maps();

	char *user_python_char = param("CLASSAD_USER_PYTHON_MODULES");
	if (user_python_char)
	{
		std::string user_python(user_python_char);
		free(user_python_char); user_python_char = NULL;
		char *loc_char = param("CLASSAD_USER_PYTHON_LIB");
		if (loc_char && !contains(ClassAdUserLibs, loc_char))
		{
			std::string loc(loc_char);
			if (classad::FunctionCall::RegisterSharedLibraryFunctions(loc.c_str()))
			{
				ClassAdUserLibs.emplace_back(loc);
#if defined(UNIX)
				void *dl_hdl = dlopen(loc.c_str(), RTLD_LAZY);
				if (dl_hdl) // Not warning on failure as the RegisterSharedLibraryFunctions should have done that.
				{
					void (*registerfn)(void) = (void (*)(void))dlsym(dl_hdl, "Register");
					if (registerfn) {registerfn();}
					dlclose(dl_hdl);
				}
#endif
			}
			else
			{
				dprintf(D_ALWAYS, "Failed to load ClassAd user python library %s: %s\n",
					loc.c_str(), classad::CondorErrMsg.c_str());
			}
		}
		if (loc_char) {free(loc_char);}
	}
	if (!ClassAd_initConfig)
	{
		registerClassadFunctions();
		classad::ExprTree::set_user_debug_function(classad_debug_dprintf);
		ClassAd_initConfig = true;
	}
}

classad::References SplitAttrNames(const std::string& str)
{
	classad::References names;
	for (const auto& name : StringTokenIterator(str)) {
		names.emplace(name);
	}
	return names;
}

classad::References SplitAttrNames(const char* str)
{
	classad::References names;
	for (const auto& name : StringTokenIterator(str)) {
		names.emplace(name);
	}
	return names;
}

std::string JoinAttrNames(const classad::References &names, const char* delim)
{
	std::string str;
	for (const auto& name : names) {
		if (!str.empty()) {
			str += delim;
		}
		str += name;
	}
	return str;
}

static classad::MatchClassAd the_match_ad;
static bool the_match_ad_in_use = false;
classad::MatchClassAd *getTheMatchAd( classad::ClassAd *source,
                                      classad::ClassAd *target,
                                      const std::string &source_alias,
                                      const std::string &target_alias )
{
	ASSERT( !the_match_ad_in_use );
	the_match_ad_in_use = true;

	the_match_ad.ReplaceLeftAd( source );
	the_match_ad.ReplaceRightAd( target );

	the_match_ad.SetLeftAlias( source_alias );
	the_match_ad.SetRightAlias( target_alias );

	return &the_match_ad;
}

void releaseTheMatchAd()
{
	ASSERT( the_match_ad_in_use );

	the_match_ad.RemoveLeftAd();
	the_match_ad.RemoveRightAd();

	the_match_ad_in_use = false;
}


static
bool stringListSize_func( const char * /*name*/,
						  const classad::ArgumentList &arg_list,
						  classad::EvalState &state, classad::Value &result )
{
	classad::Value arg0, arg1;
	std::string list_str;
	std::string delim_str = ", ";

	// Must have one or two arguments
	if ( arg_list.size() < 1 || arg_list.size() > 2 ) {
		result.SetErrorValue();
		return( true );
	}

	// Evaluate both arguments
	if( !arg_list[0]->Evaluate( state, arg0 ) ||
		( arg_list.size() == 2 && !arg_list[1]->Evaluate( state, arg1 ) ) ) {
		result.SetErrorValue();
		return false;
	}

	// If either argument isn't a string, then the result is
	// an error.
	if( !arg0.IsStringValue( list_str) ||
		( arg_list.size() == 2 && !arg1.IsStringValue( delim_str ) ) ) {
		result.SetErrorValue();
		return true;
	}

	StringTokenIterator sti(list_str, delim_str.c_str());
	size_t cnt = std::distance(sti.begin(), sti.end());

	result.SetIntegerValue(cnt);

	return true;
}

static
double sum_func( double item, double accumulator )
{
	return item + accumulator;
}

static
double min_func( double item, double accumulator )
{
	return item < accumulator ? item : accumulator;
}

static
double max_func( double item, double accumulator )
{
	return item > accumulator ? item : accumulator;
}

static
bool stringListSummarize_func( const char *name,
							   const classad::ArgumentList &arg_list,
							   classad::EvalState &state, classad::Value &result )
{
	classad::Value arg0, arg1;
	std::string list_str;
	std::string delim_str = ", ";
	bool is_avg = false;
	double (* func)( double, double ) = NULL;
	double accumulator;
	bool is_real = false;
	bool empty_allowed = false;

	// Must have one or two arguments
	if ( arg_list.size() < 1 || arg_list.size() > 2 ) {
		result.SetErrorValue();
		return( true );
	}

	// Evaluate both arguments
	if( !arg_list[0]->Evaluate( state, arg0 ) ||
		( arg_list.size() == 2 && !arg_list[1]->Evaluate( state, arg1 ) ) ) {
		result.SetErrorValue();
		return false;
	}

	// If either argument isn't a string, then the result is
	// an error.
	if( !arg0.IsStringValue( list_str) ||
		( arg_list.size() == 2 && !arg1.IsStringValue( delim_str ) ) ) {
		result.SetErrorValue();
		return true;
	}

	if ( strcasecmp( name, "stringlistsum" ) == 0 ) {
		func = sum_func;
		accumulator = 0.0;
		empty_allowed = true;
	} else if ( strcasecmp( name, "stringlistavg" ) == 0 ) {
		func = sum_func;
		accumulator = 0.0;
		empty_allowed = true;
		is_avg = true;
	} else if ( strcasecmp( name, "stringlistmin" ) == 0 ) {
		func = min_func;
		accumulator = FLT_MAX;
	} else if ( strcasecmp( name, "stringlistmax" ) == 0 ) {
		func = max_func;
		accumulator = FLT_MIN;
	} else {
		result.SetErrorValue();
		return false;
	}

	int cnt = 0;
	for (auto& entry : StringTokenIterator(list_str, delim_str.c_str())) {
		cnt++;
		double temp;
		int r = sscanf(entry.c_str(), "%lf", &temp);
		if (r != 1) {
			result.SetErrorValue();
			return true;
		}
		if (strspn(entry.c_str(), "+-0123456789") != entry.size()) {
			is_real = true;
		}
		accumulator = func( temp, accumulator );
	}

	if ( cnt == 0 ) {
		if ( empty_allowed ) {
			result.SetRealValue( 0.0 );
		} else {
			result.SetUndefinedValue();
		}
		return true;
	}

	if ( is_avg ) {
		accumulator /= cnt;
	}

	if ( is_real ) {
		result.SetRealValue( accumulator );
	} else {
		result.SetIntegerValue( (long long)accumulator );
	}

	return true;
}

// build a set or case-insensitive set from a string list
template <class T>
inline bool insert_items_from_string_tokens(T & items, const std::string & str, const char * delims) {
	if (str.empty()) return false;
	StringTokenIterator it(str, delims);
	const std::string * pstr;
	std::string item;
	while ((pstr = it.next_string_trim())) { 
		if (pstr->empty()) continue;
		items.insert(*pstr);
	}
	return true;
}

// implements stringListMember, stringListIMember, stringListSubsetMatch, and stringListISubsetMatch
// all functions return bool or undefined
// the stringListI functions are case-insensitive
// Member functions return true when the first arg is a member of the second arg when the second arg is parsed as a stringlist
// SubsetMatch functions return true when the first arg is parsed as a string list and each item
//    is a member of the second arg when the second arg is parsed as a string list
// If there is a third argument, it will be used as the delimiter set for stringlist parsing
static
bool stringListMember_func( const char *name,
							const classad::ArgumentList &arg_list,
							classad::EvalState &state, classad::Value &result )
{
	classad::Value arg0, arg1, arg2;
	std::string item_str;
	std::string list_str;
	std::string delim_str = ", \t\r\n";

	// Must have two or three arguments
	if ( arg_list.size() < 2 || arg_list.size() > 3 ) {
		result.SetErrorValue();
		return( true );
	}

	// Evaluate both arguments
	if( !arg_list[0]->Evaluate( state, arg0 ) ||
		!arg_list[1]->Evaluate( state, arg1 ) ||
		( arg_list.size() == 3 && !arg_list[2]->Evaluate( state, arg2 ) ) ) {
		result.SetErrorValue();
		return false;
	}

	// If any argument isn't a string or undefined, then the result is
	// an error.
	if( ( !arg0.IsUndefinedValue() && !arg0.IsStringValue(item_str)) ||
		( !arg1.IsUndefinedValue() && !arg1.IsStringValue(list_str)) ||
		( arg_list.size() == 3 && (!arg2.IsUndefinedValue() && !arg2.IsStringValue(delim_str)) ) ) {
		result.SetErrorValue();
		return true;
	}

	// if both args are undefined, there result is undefined
	// otherwise, undefined values behave like empty strings
	if (arg0.IsUndefinedValue() && arg1.IsUndefinedValue()) {
		result.SetUndefinedValue();
		return true;
	}

	// decode function name into case-sensitive and Member vs SubsetMatch
	const char * op = name + sizeof("stringlist")-1;
	bool case_sensitive = true;
	if (toupper(*op) == 'I') {
		case_sensitive = false;
		++op;
	}
	bool member = toupper(*op) == 'M';
	bool subset_match = toupper(*op) == 'S';

	int rc = 0;
	if (member) {
		std::vector<std::string> sl = split(list_str, delim_str.c_str());
		if (case_sensitive) {
			rc = contains(sl, item_str.c_str());
		} else {
			rc = contains_anycase(sl, item_str.c_str());
		}
	} else if (subset_match) { // SubsetMatch

		// the empty string is always a subset
		if (item_str.empty()) {
			result.SetBooleanValue(true);
			return true;
		}

		if (case_sensitive) {
			std::set<std::string> listset;
			insert_items_from_string_tokens(listset, list_str, delim_str.c_str());
			StringTokenIterator items(item_str, delim_str.c_str());
			const std::string * item;
			while ((item = items.next_string_trim())) {
				if (item->empty()) continue;
				rc = listset.count(*item);
				if ( ! rc) break;
			}
		} else {
			classad::References listset;
			insert_items_from_string_tokens(listset, list_str, delim_str.c_str());
			StringTokenIterator items(item_str, delim_str.c_str());
			const std::string * item;
			while ((item = items.next_string_trim())) {
				if (item->empty()) continue;
				rc = listset.count(*item);
				if ( ! rc) break;
			}
		}
	} else {
		// this should be unreachable
		result.SetErrorValue();
		return true;
	}
	result.SetBooleanValue( rc ? true : false );

	return true;
}

static uint32_t regexp_str_to_options( const char *option_str )
{
	uint32_t options = 0;
	while (*option_str) {
		switch (*option_str) {
			case 'i':
			case 'I':
				options |= Regex::caseless;
				break;
			case 'm':
			case 'M':
				options |= Regex::multiline;
				break;
			case 's':
			case 'S':
				options |= Regex::dotall;
				break;
			case 'x':
			case 'X':
				options |= Regex::extended;
				break;
			default:
				// Ignore for forward compatibility 
				break;
		}
		option_str++;
	}
	return options;
}

static
bool stringListRegexpMember_func( const char * /*name*/,
								  const classad::ArgumentList &arg_list,
								  classad::EvalState &state,
								  classad::Value &result )
{
	classad::Value arg0, arg1, arg2, arg3;
	std::string pattern_str;
	std::string list_str;
	std::string delim_str = ", ";
	std::string options_str;

	// Must have two or three arguments
	if ( arg_list.size() < 2 || arg_list.size() > 4 ) {
		result.SetErrorValue();
		return( true );
	}

	// Evaluate both arguments
	if( !arg_list[0]->Evaluate( state, arg0 ) ||
		!arg_list[1]->Evaluate( state, arg1 ) ||
		( arg_list.size() >= 3 && !arg_list[2]->Evaluate( state, arg2 ) ) ||
		( arg_list.size() == 4 && !arg_list[3]->Evaluate( state, arg3 ) ) ) {
		result.SetErrorValue();
		return false;
	}

	// If any argument isn't a string, then the result is
	// an error.
	if( !arg0.IsStringValue( pattern_str) ||
		!arg1.IsStringValue( list_str) ||
		( arg_list.size() >= 3 && !arg2.IsStringValue( delim_str ) ) ||
		( arg_list.size() == 4 && !arg3.IsStringValue( options_str ) ) ) {
		result.SetErrorValue();
		return true;
	}

	Regex r;
	int errcode;
	int errpos = 0;
	bool valid;
	uint32_t options = regexp_str_to_options(options_str.c_str());

	/* can the pattern be compiled */
	valid = r.compile(pattern_str.c_str(), &errcode, &errpos, options);
	if (!valid) {
		result.SetErrorValue();
		return true;
	}

	result.SetBooleanValue( false );

	bool empty = true;
	for (auto& entry : StringTokenIterator(list_str, delim_str.c_str())) {
		empty = false;
		if (r.match(entry.c_str())) {
			result.SetBooleanValue( true );
		}
	}

	if (empty) {
		result.SetUndefinedValue();
	}

	return true;
}

// map an input string using the given mapping set, this works using the
// same mapping function that the security layer uses to map security principals
// usage: 
//           // the 2 argument form returns a list of groups
//        userMap("user_to_groups", "user") -> {"group1","group2"}
// 
//           // the 3 argument form returns a single string. it will return the preferred group
//           // if the user is in that group. otherwise it will return the first group
//        userMap("user_to_groups", "user", "group2") -> "group2"
//        userMap("user_to_groups", "user", "group3") -> "group1"

//           // the 4 argument form return a single group. it will return the preferred group
//           // if the user is in that group. otherwise it will return the first group.
//           // If the user is not in any groups, it will return the 4th argument
//        userMap("user_to_groups", "user", "group2", "defgroup") -> "group2"
//        userMap("user_to_groups", "user", "group3", "defgroup") -> "group1"

static
bool userMap_func( const char * /*name*/,
	const classad::ArgumentList & arg_list,
	classad::EvalState & state,
	classad::Value & result)
{
	classad::Value mapVal, userVal, prefVal;

	size_t cargs = arg_list.size();
	if (cargs < 2 || cargs > 4) {
		result.SetErrorValue();
		return true;
	}
	if ( ! arg_list[0]->Evaluate(state, mapVal) ||
		 ! arg_list[1]->Evaluate(state, userVal) ||
		 (cargs >= 3 && !arg_list[2]->Evaluate(state, prefVal)) ||
		 (cargs >= 4 && !arg_list[3]->Evaluate(state, result))
		) {
		result.SetErrorValue();
		return false; // propagate evaluation failure.
	}

	std::string mapName, userName;
	if ( ! mapVal.IsStringValue(mapName) || ! userVal.IsStringValue(userName)) {
		if (mapVal.IsErrorValue() || userVal.IsErrorValue()) {
			result.SetErrorValue();
		} else if (cargs < 4) {
			result.SetUndefinedValue();
		}
		return true;
	}

	std::string output;
	if (user_map_do_mapping(mapName.c_str(), userName.c_str(), output)) {
		StringTokenIterator items(output, ",");

		if (cargs == 2) {
			// 2 arg form, return a list.
		#ifdef CLASSAD_USER_MAP_RETURNS_STRINGLIST
			result.SetStringValue(output.c_str());
		#else
			classad_shared_ptr<classad::ExprList> lst( new classad::ExprList() );
			ASSERT(lst);
			for (const char * str = items.first(); str != NULL; str = items.next()) {
				classad::Value val; val.SetStringValue(str);
				lst->push_back(classad::Literal::MakeLiteral(val));
			}
			result.SetListValue(lst);
		#endif
		} else {
			// 3 or 4 arg form, return as a string a either the preferred item, or the first item
			// preferred item match is case-insensitive.  If the list is empty return undefined
			std::string pref;
			const char * selected_item = NULL;
			if (prefVal.IsStringValue(pref)) {
				for (const char* item = items.first(); item; item = items.next()) {
					if (strcasecmp(item, pref.c_str()) == 0) {
						selected_item = item;
						break;
					}
				}
			}
			if ( ! selected_item) { selected_item = items.first(); }
			if (selected_item) {
				result.SetStringValue(selected_item);
			} else if (cargs < 4) {
				// if result has not been primed with the default value (i.e. this is the 3 arg form)
				// set the result to undefined now.  we end up here when the items list is empty
				result.SetUndefinedValue();
			}
		}
	} else if (cargs < 4) {
		// 2 or 3 arg form, and no mapping, return undefined.
		result.SetUndefinedValue();
	}

	return true;
}


// split user@domain or slot@machine, result is always a list of 2 strings
// if there is no @ in the input, then for user@domain the domain is empty
// for slot@machine the slot is empty
static
bool splitAt_func( const char * name,
	const classad::ArgumentList &arg_list,
	classad::EvalState &state, 
	classad::Value &result )
{
	classad::Value arg0;

	// Must have one argument
	if ( arg_list.size() != 1) {
		result.SetErrorValue();
		return true;
	}

	// Evaluate the argument
	if( !arg_list[0]->Evaluate( state, arg0 )) {
		result.SetErrorValue();
		return false;
	}

	// If either argument isn't a string, then the result is
	// an error.
	std::string str;
	if( !arg0.IsStringValue(str) ) {
		result.SetErrorValue();
		return true;
	}

	classad::Value first;
	classad::Value second;

	size_t ix = str.find_first_of('@');
	if (ix >= str.size()) {
		if (0 == strcasecmp(name, "splitslotname")) {
			first.SetStringValue("");
			second.SetStringValue(str);
		} else {
			first.SetStringValue(str);
			second.SetStringValue("");
		}
	} else {
		first.SetStringValue(str.substr(0, ix));
		second.SetStringValue(str.substr(ix+1));
	}

	classad_shared_ptr<classad::ExprList> lst( new classad::ExprList() );
	ASSERT(lst);
	lst->push_back(classad::Literal::MakeLiteral(first));
	lst->push_back(classad::Literal::MakeLiteral(second));

	result.SetListValue(lst);

	return true;
}

// split using arbitrary separator characters
static
bool splitArb_func( const char * /*name*/,
	const classad::ArgumentList &arg_list,
	classad::EvalState &state, 
	classad::Value &result )
{
	classad::Value arg0;

	// Must have one or 2 arguments
	if ( arg_list.size() != 1 && arg_list.size() != 2) {
		result.SetErrorValue();
		return true;
	}

	// Evaluate the first argument
	if( !arg_list[0]->Evaluate( state, arg0 )) {
		result.SetErrorValue();
		return false;
	}

	// if we have 2 arguments, the second argument is a set of
	// separator characters, the default set of separators is , and whitespace
	std::string seps = ", \t";
	classad::Value arg1;
	if (arg_list.size() >= 2 && ! arg_list[1]->Evaluate( state, arg1 )) {
		result.SetErrorValue();
		return false;
	}

	// If either argument isn't a string, then the result is
	// an error.
	std::string str;
	if( !arg0.IsStringValue(str) ) {
		result.SetErrorValue();
		return true;
	}
	if (arg_list.size() >= 2 && ! arg1.IsStringValue(seps)) {
		result.SetErrorValue();
		return true;
	}

	classad_shared_ptr<classad::ExprList> lst( new classad::ExprList() );
	ASSERT(lst);

	// walk the input string, splitting at each instance of a separator
	// character and discarding empty strings.  Thus leading and trailing
	// separator characters are ignored, and runs of multiple separator
	// characters have special treatment, multiple whitespace seps are
	// treated as a single sep, as are single separators mixed with whitespace.
	// but runs of a single separator are handled individually, thus
	// "foo, bar" is the same as "foo ,bar" and "foo,bar".  But not the same as
	// "foo,,bar", which produces a list of 3 items rather than 2.
	size_t ixLast = 0;
	classad::Value val;
	if (seps.length() > 0) {
		size_t ix = str.find_first_of(seps, ixLast);
		int      ch = -1;
		while (ix < str.length()) {
			if (ix - ixLast > 0) {
				val.SetStringValue(str.substr(ixLast, ix - ixLast));
				lst->push_back(classad::Literal::MakeLiteral(val));
			} else if (!isspace(ch) && ch == str[ix]) {
				val.SetStringValue("");
				lst->push_back(classad::Literal::MakeLiteral(val));
			}
			if (!isspace(str[ix])) ch = str[ix];
			ixLast = ix+1;
			ix = str.find_first_of(seps, ixLast);
		}
	}
	if (str.length() > ixLast) {
		val.SetStringValue(str.substr(ixLast));
		lst->push_back(classad::Literal::MakeLiteral(val));
	}

	result.SetListValue(lst);

	return true;
}


static void
problemExpression(const std::string &msg, classad::ExprTree *problem, classad::Value &result)
{
	result.SetErrorValue();
	classad::ClassAdUnParser unp;
	classad::CondorErrMsg = msg + "  Problem expression: ";
	unp.Unparse(classad::CondorErrMsg, problem);
}


static bool
ArgsToList( const char * name,
	const classad::ArgumentList &arguments,
	classad::EvalState &state,
	classad::Value &result )
{
	if ((arguments.size() != 1) && (arguments.size() != 2))
	{
		result.SetErrorValue();
		classad::CondorErrMsg = std::string("Invalid number of arguments passed to ") +  name + "; one string argument expected.";
		return true;
	}
	int vers = 2;
	if (arguments.size() == 2)
	{
		classad::Value val;
		if (!arguments[1]->Evaluate(state, val))
		{
			problemExpression("Unable to evaluate second argument.", arguments[1], result);
			return false;
		}
		if (!val.IsIntegerValue(vers))
		{
			problemExpression("Unable to evaluate second argument to integer.", arguments[1], result);
			return true;
		}
		if ((vers != 1) && (vers != 2))
		{
			std::string s;
			formatstr(s, "Valid values for version are 1 or 2.  Passed expression evaluates to %d.", vers);
			problemExpression(s, arguments[1], result);
			return true;
		}
	}
	classad::Value val;
	if (!arguments[0]->Evaluate(state, val))
	{
		problemExpression("Unable to evaluate first argument.", arguments[0], result);
		return false;
	}
	std::string args;
	if (!val.IsStringValue(args))
	{
		problemExpression("Unable to evaluate first argument to string.", arguments[0], result);
		return true;
	}
	ArgList arg_list;
	std::string error_msg;
	if ((vers == 1) && !arg_list.AppendArgsV1Raw(args.c_str(), error_msg))
	{
		std::string s;
		s = std::string("Error when parsing argument to arg V1: ") + error_msg;
		problemExpression(s, arguments[0], result);
		return true;
	}
	else if ((vers == 2) && !arg_list.AppendArgsV2Raw(args.c_str(), error_msg))
	{
		std::string s;
		s = std::string("Error when parsing argument to arg V2: ") + error_msg;
		problemExpression(s, arguments[0], result);
		return true;
	}
	std::vector<classad::ExprTree*> list_exprs;

	for (size_t idx=0; idx<arg_list.Count(); idx++)
	{
		classad::Value string_val;
		string_val.SetStringValue(arg_list.GetArg(idx));
		classad::ExprTree *expr = classad::Literal::MakeLiteral(string_val);
		if (!expr)
		{
			for (std::vector<classad::ExprTree*>::iterator it = list_exprs.begin(); it != list_exprs.end(); it++)
			{
				if (*it) {delete *it; *it=NULL;}
			}
			classad::CondorErrMsg = "Unable to create string expression.";
			result.SetErrorValue();
			return false;
		}
		list_exprs.push_back(expr);
	}
	classad_shared_ptr<classad::ExprList> result_list(classad::ExprList::MakeExprList(list_exprs));
	if (!result_list.get())
	{
		for (std::vector<classad::ExprTree*>::iterator it = list_exprs.begin(); it != list_exprs.end(); it++)
		{
			if (*it) {delete *it; *it=NULL;}
		}
		classad::CondorErrMsg = "Unable to create expression list.";
		result.SetErrorValue();
		return false;
	}
	result.SetListValue(result_list);
	return true;
}


static bool
ListToArgs(const char * name,
	const classad::ArgumentList &arguments,
	classad::EvalState &state,
	classad::Value &result)
{
	if ((arguments.size() != 1) && (arguments.size() != 2))
	{
		result.SetErrorValue();
		classad::CondorErrMsg = std::string("Invalid number of arguments passed to ") + name + "; one list argument expected.";
		return true;
	}
	int vers = 2;
	if (arguments.size() == 2)
	{
		classad::Value val;
		if (!arguments[1]->Evaluate(state, val))
		{
			problemExpression("Unable to evaluate second argument.", arguments[1], result);
			return false;
		}
		if (!val.IsIntegerValue(vers))
		{
			problemExpression("Unable to evaluate second argument to integer.", arguments[1], result);
			return true;
		}
		if ((vers != 1) && (vers != 2))
		{
			std::string s;
			formatstr(s, "Valid values for version are 1 or 2.  Passed expression evaluates to %d.", vers);
			problemExpression(s, arguments[1], result);
			return true;
		}
	}
	classad::Value val;
	if (!arguments[0]->Evaluate(state, val))
	{
		problemExpression("Unable to evaluate first argument.", arguments[0], result);
		return false;
	}
	classad_shared_ptr<classad::ExprList> args;
	if (!val.IsSListValue(args))
	{
		problemExpression("Unable to evaluate first argument to list.", arguments[0], result);
		return true;
	}
	ArgList arg_list;
	size_t idx=0;
	for (auto it=args->begin(); it!=args->end(); it++, idx++)
	{
		classad::Value value;
		if (!(*it)->Evaluate(state, value))
		{
			std::string s;
			formatstr(s, "Unable to evaluate list entry %zu.", idx);
			problemExpression(s, *it, result);
			return false;
		}
		std::string tmp_str;
		if (!value.IsStringValue(tmp_str))
		{
			std::string s;
			formatstr(s, "Entry %zu did not evaluate to a string.", idx);
			problemExpression(s, *it, result);
			return true;
		}
		arg_list.AppendArg(tmp_str.c_str());
	}
	std::string error_msg, result_mystr;
	if ((vers == 1) && !arg_list.GetArgsStringV1Raw(result_mystr, error_msg))
	{
		std::string s = std::string("Error when parsing argument to arg V1: ") + error_msg;
		problemExpression(s, arguments[0], result);
		return true;
	}
	else if ((vers == 2) && !arg_list.GetArgsStringV2Raw(result_mystr))
	{
		std::string s = std::string("Error when parsing argument to arg V2: ") +  error_msg;
		problemExpression(s, arguments[0], result);
		return true;
	}
	result.SetStringValue(result_mystr);
	return true;
}


static bool
EnvironmentV1ToV2(const char * name,
	const classad::ArgumentList &arguments,
	classad::EvalState &state,
	classad::Value &result)
{
	if (arguments.size() != 1)
	{
		result.SetErrorValue();
		classad::CondorErrMsg =
		   	std::string("Invalid number of arguments passed to ") +  name + "; one string argument expected.";
		return true;
	}
	classad::Value val;
	if (!arguments[0]->Evaluate(state, val))
	{
		problemExpression("Unable to evaluate first argument.", arguments[0], result);
		return false;
	}
		// By returning undefined if the input is undefined, the caller doesn't need an
		// ifThenElse statement to see if the V1 environment is used in the first place.
		// For example, we can do this:
		//   mergeEnvironment(envV1toV2(Env), Environment, "FOO=BAR");
		// without worrying about whether Env or Environment is already defined.
	if (val.IsUndefinedValue())
	{
		result.SetUndefinedValue();
		return true;
	}
	std::string args;
	if (!val.IsStringValue(args))
	{
		problemExpression("Unable to evaluate first argument to string.", arguments[0], result);
		return true;
	}
	Env env;
	std::string error_msg;
	if (!env.MergeFromV1AutoDelim(args.c_str(), error_msg))
	{
		error_msg.insert(0, "Error when parsing argument to environment V1: ");
		problemExpression(error_msg, arguments[0], result);
		return true;
	}
	std::string result_mystr;
	env.getDelimitedStringV2Raw(result_mystr);
	result.SetStringValue(result_mystr);
	return true;
}


static bool
MergeEnvironment(const char * /*name*/,
	const classad::ArgumentList &arguments,
	classad::EvalState &state,
	classad::Value &result)
{
	Env env;
	size_t idx = 0;
	for (auto it=arguments.begin(); it!=arguments.end(); it++, idx++)
	{
		classad::Value val;
		if (!(*it)->Evaluate(state, val))
		{
			std::string s;
			formatstr(s, "Unable to evaluate argument %zu.", idx);
			problemExpression(s, *it, result);
			return false;
		}
			// Skip over undefined values; this makes it more natural
			// to merge environments for a condor classad where the
			// user may or may not have specified an environment string.
		if (val.IsUndefinedValue())
		{
			continue;
		}
		std::string env_str;
		if (!val.IsStringValue(env_str))
		{
			std::string s;
			formatstr(s, "Unable to evaluate argument %zu.", idx);
			problemExpression(s, *it, result);
			return true;
		}
		if (!env.MergeFromV2Raw(env_str.c_str(), nullptr))
		{
			std::string s;
			formatstr(s, "Argument %zu cannot be parsed as environment string.", idx);
			problemExpression(s, *it, result);
			return true;
		}
	}
	std::string result_mystr;
	env.getDelimitedStringV2Raw(result_mystr);
	result.SetStringValue(result_mystr);
	return true;
}


static bool
return_home_result(const std::string &default_home,
	const std::string &error_msg,
	classad::Value    &result,
	bool isError)
{
	if (default_home.size())
	{
		result.SetStringValue(default_home);
	}
	else
	{
		if (isError) {result.SetErrorValue();} else {result.SetUndefinedValue();}
		classad::CondorErrMsg = error_msg;
	}
	return true;
}

/**
 * Implementation of the userHome function.
 *
 * This returns the home directory of a given username as configured on the system.
 * The home directory is determined using the getpwdnam call.
 *
 * Arguments:
 *   - name.  The username to lookup.
 *   - default (optional).  The default value to return if no user home exists.
 *
 * Returns:
 *   - ERROR on Windows platforms.
 *   - ERROR if invoked with the incorrect number of arguments.
 *   - otherwise, `default` if default is given AND the value of `default` evaluates to a non-empty string AND:
 *     - the name cannot be evaluated to a string/UNDEFINED, OR
 *     - the user does not exist on the system, OR
 *     - the user does not have a home directory.
 *   - otherwise, ERROR if the first argument cannot be evaluated or does not evaluate to a string or
 *     UNDEFINED value (and default does not evaluate to a non-empty string).
 *   - otherwise, UNDEFINED if the first argument evaluates to UNDEFINED
 *   - otherwise, a string specifying the user's home directory.
 */

static bool
userHome_func(const char *                 name,
	const classad::ArgumentList &arguments,
	classad::EvalState          &state,
	classad::Value              &result)
{
	if ((arguments.size() != 1) && (arguments.size() != 2))
	{
		result.SetErrorValue();
		std::string s;
		formatstr(s, "Invalid number of arguments passed to %s ; %zu given, 1 required and 1 optional.", name, arguments.size());
		classad::CondorErrMsg = s;
		return false;
	}

#ifdef WIN32
	result.SetErrorValue();
	classad::CondorErrMsg = "UserHome is not available on Windows.";
	return true;
#endif

	std::string default_home;
	classad::Value default_home_value;
	if (arguments.size() != 2 || !arguments[1]->Evaluate(state, default_home_value) || !default_home_value.IsStringValue(default_home))
	{
		default_home = "";
	}

	std::string owner_string;
	classad::Value owner_value;
	if (!arguments[0]->Evaluate(state, owner_value))
	{
		
	}
	if (owner_value.IsUndefinedValue() && !default_home.size())
	{
		result.SetUndefinedValue();
		return true;
	}
	if (!owner_value.IsStringValue(owner_string))
	{
		std::string s = std::string("Could not evaluate the first argument of ") + name + " to string.  Expression: ";
		classad::ClassAdUnParser unp; unp.Unparse(s, arguments[0]);
		s += '.';
		return return_home_result(default_home, s, result, true);
	}

	errno = 0;
#ifndef WIN32
		// The UserHome function has potential side-effects (such as hitting a LDAP server)
		// that might have a noticable impact on the system.  Disable unless explicitly requested.
		// (really, this only is useful in the very limited HTCondor-CE context.)
	if (!param_boolean("CLASSAD_ENABLE_USER_HOME", false ))
	{
		return return_home_result(default_home, "UserHome is currently disabled; to enable set CLASSAD_ENABLE_USER_HOME=true in the HTCondor config.",
			result, false);
	}
	struct passwd *info = getpwnam(owner_string.c_str());
	if (!info)
	{
		std::string s = std::string("Unable to find home directory for user ") + owner_string;
		if (errno) {
			s += std::string(": ") + strerror(errno) +  "(errno=" + std::to_string(errno) + ')';
		} else {
			s += ": No such user.";
		}
		return return_home_result(default_home, s, result, false);
	}

	if (!info->pw_dir)
	{
		std::string s = std::string("User ") + owner_string + " has no home directory";
		return return_home_result(default_home, s, result, false);
	}
	std::string home_string = info->pw_dir;
	result.SetStringValue(home_string);
#endif

	return true;
}


bool
is_in_tree( const classad::ClassAd * member,
            const classad::ClassAd * leaf ) {
	if( member == leaf ) { return true; }
	if( leaf == NULL ) { return false; }

	// We can't just walk the parent scope pointers, because the parent
	// scope pointer for the LEFT and RIGHT ads is temporarily replaced
	// by the corresponding context ad when the match ad is created.

	const classad::ClassAd * chainedParent = leaf->GetChainedParentAd();
	if( chainedParent != NULL && is_in_tree(member, chainedParent) ) { return true; }

	const classad::ClassAd * parentScope = leaf->GetParentScope();
	if( parentScope != NULL && is_in_tree(member, parentScope) ) { return true; }

	return false;
}

classad::Value
evaluateInContext( classad::ExprTree * expr,
				   classad::EvalState & state,
				   classad::ExprTree * nested_ad_reference ) {
	classad::Value rv;

	classad::Value cav;
	if(! nested_ad_reference->Evaluate(state, cav)) {
		//dprintf( D_FULLDEBUG, "evaluateInContext(): failed to evaluate listed element\n" );
		rv.SetErrorValue();
		return rv;
	}

	classad::ClassAd * nested_ad = NULL;
	if(! cav.IsClassAdValue(nested_ad)) {
		//dprintf( D_FULLDEBUG, "evaluateInContext(): listed element is not a ClassAd\n" );
		if (cav.IsUndefinedValue()) {
			rv.SetUndefinedValue();
		} else {
			rv.SetErrorValue();
		}
		return rv;
	}

	// AttributeReference::FindExpr() doesn't recursively check the parent
	// scope's alternate scope, so set it by hand.  This allows attribute
	// references to work correctly (if confusingly) if this function is
	// called (as it is intended to be) during a match.
	//
	// If the target ad is a chained ad, as it is in the startd, then the
	// nested ad's parent won't (usually) have an alternate scope pointer
	// set.  Instead, we need to figure out if the nested ad's parent is
	// on the RIGHT or LEFT side, and use the corresponding alternate scope
	// pointer.  (We don't check for the nested ad itself because the
	// nested ad isn't a chained parent or parent scope of RIGHT or LEFT.)
	//
	// The confusing part is that MY and TARGET will be reversed during the
	// evaluation of attr if, as intended, the nested ad's parent is not the
	// parent of the expression containing function call.  (We intend for
	// the former to be the slot ad and the latter the job ad.)
	classad::ClassAd * originalAlternateScope = nested_ad->alternateScope;

	// This is awful, but I don't want to change the ClassAd API to fix it.
	classad::MatchClassAd * matchAd =
		const_cast<classad::MatchClassAd *>(dynamic_cast<const classad::MatchClassAd *>(state.rootAd));
	if( matchAd != NULL ) {
		const classad::ClassAd * left = matchAd->GetLeftAd();
		const classad::ClassAd * right = matchAd->GetRightAd();

		if( is_in_tree( nested_ad->GetParentScope(), left ) ) {
			nested_ad->alternateScope = left->alternateScope;
		} else if( is_in_tree( nested_ad->GetParentScope(), right ) ) {
			nested_ad->alternateScope = right->alternateScope;
		} else {
			//dprintf( D_FULLDEBUG, "evaluateInContext(): nested ad not in LEFT or RIGHT\n" );
			rv.SetErrorValue();
		}
	}

	classad::EvalState temporary_state;
	temporary_state.SetScopes(nested_ad);
	if(! expr->Evaluate(temporary_state, rv)) {
		//dprintf( D_FULLDEBUG, "evaluateInContext(): failed to evaluate expr in context\n" );
		rv.SetErrorValue();
	}

	nested_ad->alternateScope = originalAlternateScope;
	return rv;
}

static
bool evalInEachContext_func( const char * name,
							  const classad::ArgumentList &arg_list,
							  classad::EvalState &state,
							  classad::Value &result ) {
	//
	// evalInEachContext( expr, nested_ad_list ) -> list
	// countMatches( expr, nested_ad_list ) -> int
	//
	// Evaluate expr in the context of the current ad and each ad in
	// nested_ad_list, individually.  Returns the index in nested_ad_list
	// of the first ad for which expr evaluaes to true.
	//

	bool is_evalInEach = 0 == strcasecmp(name, "evalineachcontext");

	if( arg_list.size() != 2 ) {
		//dprintf( D_FULLDEBUG, "evalEachInContext(): wrong number of arguments\n" );
		result.SetErrorValue();
		return true;
	}

	classad::ExprTree * expr = arg_list[0];
	classad::ExprTree * nal = arg_list[1];

	// If expr is an attribute reference, look it up before proceeding so
	// that the lookup happens in the context of the match.  Likewise, if
	// nal isn't a literal list, evaluate in the context of the match
	// before proceeding.  Just document expr as either an attribute
	// reference or literal expression; or maybe just the former?  (Makes
	// it only marginally hard to use but simplifies things?)
	if( expr->GetKind() == ExprTree::NodeKind::ATTRREF_NODE ) {
		classad::AttributeReference * ref =
			dynamic_cast<classad::AttributeReference *>(expr);
		if( ref == NULL ) {
			// This should probably be an EXCEPT().
			//dprintf( D_FULLDEBUG, "evalEachInContext(): FIXME\n" );
			result.SetErrorValue();
			return true;
		}

		// Lookup the first argument and get the expression.  If the lookup succeeds we will
		// evaluate the expression against each ad in the list.
		// If the lookup fails we will treat the attribute reference as the expression to evaluate
		// thus we can use this function to evaluate an expression in a list of ads,
		// or to project a single attribute from those ads
		//     evalInEachContext(MY.RequireGPUs, AvailableGPUs) -> { true, false, ... }
		//     evalInEachContext(Capability, AvailableGPUs) -> { eval(GPUs_tag1.Capability), eval(GPUs_tag1.Capability), ... }
		// Or more specifically
		//     sum(evalInEachContext(MY.RequireGPUs, AvailableGPUs)) -> <number of available GPUs that match the RequireGPUs expression>
		//     max(evalInEachContext(Capability, AvailableGPUs)) -> <max capability of all available GPUs>
		//
		classad::ExprTree * attr = NULL;
		// This is a bug in the ClassAd API: this function returns values
		// out of an anonymous enum in ExprTree, but that enum is protected.
		// if(classad::AttributeReference::Deref( *ref, state, attr ) != classad::ExprTree::EVAL_OK) {
		if(classad::AttributeReference::Deref( *ref, state, attr ) == 1 ) {
			expr = attr;
		}
	}

	if( nal->GetKind() != ExprTree::NodeKind::EXPR_LIST_NODE ) {
		classad::Value cav;
		nal->Evaluate(state, cav);
		classad::ExprList * list = NULL;
		if(cav.IsListValue(list)) {
			nal = list;
		} else if (cav.IsUndefinedValue()) {
			if (is_evalInEach) {
				// evalInEach will propagate UNDEFINED values
				result.SetUndefinedValue();
			} else {
				// countMatches will not propagate UNDEFINED and will instead return 0 matches
				result.SetIntegerValue(0);
			}
			return true;
		}
	}

	classad::ExprList * nested_ad_list = dynamic_cast<classad::ExprList *>(nal);
	if( nested_ad_list == NULL ) {
		//dprintf( D_FULLDEBUG, "evalEachInContext(): failed to convert second argument\n" );
		result.SetErrorValue();
		return true;
	}

	if (is_evalInEach) {

		// for evalInEachContext we will return a list of evaluation results
		classad_shared_ptr<classad::ExprList> lst(new classad::ExprList());
		ASSERT(lst);

		for (auto i = nested_ad_list->begin(); i != nested_ad_list->end(); ++i) {
			auto nested_ad = *i;

			classad::Value cav = evaluateInContext(expr, state, nested_ad);
			if (cav.IsListValue()) {
				classad::ExprList * elv = nullptr;
				cav.IsListValue(elv);
				lst->push_back(elv->Copy());
			} else if (cav.IsClassAdValue()) {
				classad::ClassAd * cad = nullptr;
				cav.IsClassAdValue(cad);
				lst->push_back(cad->Copy());
			} else {
				lst->push_back(classad::Literal::MakeLiteral(cav));
			}
		}

		result.SetListValue(lst);
	} else {
		// for countMatches - return an integer count of results that evaluate to true
		// This is useful for matching a job to a heterogenous array of custom resources like GPUs.
		// the slot ad will have (actual names are longer)
		//    AvailableGPUs = { GPUs_tag1, GPUs_tag2 }
		//    GPUS_tag1 = [ Capability = 4.0; Name = "GRX"; ]
		//    GPUS_tag2 = [ Capability = 5.5; Name = "Tesla"; ]
		// The job will indicate which GPU it wants
		//    RequestGPUs = 1
		//    RequireGPUs = Capability > 4
		// And the job's requirements will use this function to insure that the job matches the slot
		//    Requirements = ... && countMatches(RequireGPUs, AvailableGPUs) >= RequestGPUs
		int num_matches = 0;
		for( auto i = nested_ad_list->begin(); i != nested_ad_list->end(); ++i ) {
			auto nested_ad = *i;

			classad::Value r = evaluateInContext( expr, state, nested_ad );
			// not that we do *not* propagate undefined or error here because we want to use this function in matchmaking
			bool matched = false;
			if( r.IsBooleanValueEquiv(matched) && matched ) {
				++num_matches;
			}
		}

		result.SetIntegerValue(num_matches);
	}

	return true;
}



static
void registerClassadFunctions()
{
	std::string name;
	name = "envV1ToV2";
	classad::FunctionCall::RegisterFunction(name, EnvironmentV1ToV2);

	name = "mergeEnvironment";
	classad::FunctionCall::RegisterFunction(name, MergeEnvironment);

	name = "listToArgs";
	classad::FunctionCall::RegisterFunction(name, ListToArgs);

	name = "argsToList";
	classad::FunctionCall::RegisterFunction(name, ArgsToList);

	name = "stringListSize";
	classad::FunctionCall::RegisterFunction( name,
											 stringListSize_func );
	name = "stringListSum";
	classad::FunctionCall::RegisterFunction( name,
											 stringListSummarize_func );
	name = "stringListAvg";
	classad::FunctionCall::RegisterFunction( name,
											 stringListSummarize_func );
	name = "stringListMin";
	classad::FunctionCall::RegisterFunction( name,
											 stringListSummarize_func );
	name = "stringListMax";
	classad::FunctionCall::RegisterFunction( name,
											 stringListSummarize_func );
	name = "stringListMember";
	classad::FunctionCall::RegisterFunction( name,
											 stringListMember_func );
	name = "stringListIMember";
	classad::FunctionCall::RegisterFunction( name,
											 stringListMember_func );
	name = "stringListSubsetMatch";
	classad::FunctionCall::RegisterFunction( name,
											 stringListMember_func );
	name = "stringListISubsetMatch";
	classad::FunctionCall::RegisterFunction( name,
											 stringListMember_func );
	name = "stringList_regexpMember";
	classad::FunctionCall::RegisterFunction( name,
											 stringListRegexpMember_func );

	name = "userHome";
	classad::FunctionCall::RegisterFunction(name, userHome_func);

	name = "userMap";
	classad::FunctionCall::RegisterFunction(name, userMap_func);

	// user@domain, slot@machine & sinful string crackers.
	name = "splitusername";
	classad::FunctionCall::RegisterFunction( name, splitAt_func );
	name = "splitslotname";
	classad::FunctionCall::RegisterFunction( name, splitAt_func );
	name = "split";
	classad::FunctionCall::RegisterFunction( name, splitArb_func );
	//name = "splitsinful";
	//classad::FunctionCall::RegisterFunction( name, splitSinful_func );

    name = "evalInEachContext";
    classad::FunctionCall::RegisterFunction( name, evalInEachContext_func);
    name = "countMatches";
    classad::FunctionCall::RegisterFunction( name, evalInEachContext_func);
}

void
classad_debug_dprintf(const char *s) {
	dprintf(D_FULLDEBUG, "%s", s);
}

CondorClassAdFileParseHelper::~CondorClassAdFileParseHelper()
{
	switch (parse_type) {
		case Parse_xml: {
			classad::ClassAdXMLParser * parser = (classad::ClassAdXMLParser *)new_parser;
			delete parser;
			new_parser = NULL;
		} break;
		case Parse_json: {
			classad::ClassAdJsonParser * parser = (classad::ClassAdJsonParser *)new_parser;
			delete parser;
			new_parser = NULL;
		} break;
		case Parse_new: {
			classad::ClassAdParser * parser = (classad::ClassAdParser *)new_parser;
			delete parser;
			new_parser = NULL;
		} break;
		default: break;
	}
	ASSERT( ! new_parser);
}


bool CondorClassAdFileParseHelper::line_is_ad_delimitor(const std::string & line)
{
	if (blank_line_is_ad_delimitor) {
		const char * p = line.c_str();
		while (*p && isspace(*p)) ++p;
		return ( ! *p );
	}
	bool is_delim = starts_with(line, ad_delimitor);
	if (is_delim) { delim_line = line; }
	else { delim_line.clear(); }
	return is_delim;
}

// this method is called before each line is parsed.
// return 0 to skip (is_comment), 1 to parse line, 2 for end-of-classad, -1 for abort
int CondorClassAdFileParseHelper::PreParse(std::string & line, classad::ClassAd & /*ad*/, FILE* /*file*/)
{
	// if this line matches the ad delimitor, tell the parser to stop parsing
	if (line_is_ad_delimitor(line))
		return 2; //end-of-classad

	// check for blank lines or lines whose first character is #
	// tell the parse to skip those lines, otherwise tell the parser to
	// parse the line.
	for (size_t ix = 0; ix < line.size(); ++ix) {
		if (line[ix] == '#')
			return 0; // skip this line, but don't stop parsing.
		if (line[ix] != ' ' && line[ix] != '\t')
			return 1; // parse this line.
	}
	return 0; // skip this line, but don't stop parsing.
}

// this method is called when the parser encounters an error
// return 0 to skip and continue, 1 to re-parse line, 2 to quit parsing with success, -1 to abort parsing.
int CondorClassAdFileParseHelper::OnParseError(std::string & line, classad::ClassAd & /*ad*/, FILE* file)
{
	if (parse_type >= Parse_xml && parse_type < Parse_auto) {
		// here line is actually errmsg.
		//PRAGMA_REMIND("report parse errors for new parsers?")
		return -1;
	}

		// print out where we barfed to the log file
	dprintf(D_ALWAYS,"failed to create classad; bad expr = '%s'\n",
			line.c_str());

	// read until delimitor or EOF; whichever comes first
	line = "NotADelim=1";
	while ( ! line_is_ad_delimitor(line)) {
		if (feof(file))
			break;
		if ( ! readLine(line, file, false))
			break;
		chomp(line);
	}
	return -1; // abort
}


int CondorClassAdFileParseHelper::NewParser(classad::ClassAd & ad, FILE* file, bool & detected_long, std::string & errmsg)
{
	detected_long = false;
	if (parse_type < Parse_xml || parse_type > Parse_auto) {
		// return 0 to indicate this is a -long form (line oriented) parse type
		return 0;
	}

	int rval = 1;
	switch(parse_type) {
		case Parse_xml: {
			classad::ClassAdXMLParser * parser = (classad::ClassAdXMLParser *)new_parser;
			if ( ! parser) {
				parser = new classad::ClassAdXMLParser();
				new_parser = (void*)parser;
			}
			ASSERT(parser);
			bool fok = parser->ParseClassAd(file, ad);
			if (fok) {
				rval = ad.size();
			} else if (feof(file)) {
				rval = -99;
			} else {
				rval = -1;
			}
		} break;

		case Parse_json: {
			classad::ClassAdJsonParser * parser = (classad::ClassAdJsonParser *)new_parser;
			if ( ! parser) {
				parser = new classad::ClassAdJsonParser();
				new_parser = (void*)parser;
			}
			ASSERT(parser);
			bool fok = parser->ParseClassAd(file, ad, false);
			if ( ! fok) {
				bool keep_going = false;
				classad::Lexer::TokenType tt = parser->getLastTokenType();
				if (inside_list) {
					if (tt == classad::Lexer::LEX_COMMA) { keep_going = true; }
					else if (tt == classad::Lexer::LEX_CLOSE_BOX) { keep_going = true; inside_list = false; }
				} else {
					if (tt == classad::Lexer::LEX_OPEN_BOX) { keep_going = true; inside_list = true; }
				}
				if (keep_going) {
					fok = parser->ParseClassAd(file, ad, false); 
				}
			}
			if (fok) {
				rval = ad.size();
			} else if (feof(file)) {
				rval = -99;
			} else {
				rval = -1;
			}
		} break;

		case Parse_new: {
			classad::ClassAdParser * parser = (classad::ClassAdParser *)new_parser;
			if ( ! parser) {
				parser = new classad::ClassAdParser();
				new_parser = (void*)parser;
			}
			ASSERT(parser);
			bool fok = parser->ParseClassAd(file, ad);
			if ( ! fok) {
				bool keep_going = false;
				classad::Lexer::TokenType tt = parser->getLastTokenType();
				if (inside_list) {
					if (tt == classad::Lexer::LEX_COMMA) { keep_going = true; }
					else if (tt == classad::Lexer::LEX_CLOSE_BRACE) { keep_going = true; inside_list = false; }
				} else {
					if (tt == classad::Lexer::LEX_OPEN_BRACE) { keep_going = true; inside_list = true; }
				}
				if (keep_going) {
					fok = parser->ParseClassAd(file, ad, false); 
				}
			}
			if (fok) {
				rval = ad.size();
			} else if (feof(file)) {
				rval = -99;
			} else {
				rval = -1;
			}
		} break;

		case Parse_auto: { // parse line oriented until we figure out the parse type
			// get a line from the file
			std::string buffer;
			for (;;) {
				if ( ! readLine(buffer, file, false)) {
					return feof(file) ? -99 : -1;
				}
				chomp(buffer);

				int ee = PreParse(buffer, ad, file);
				if (ee == 1) {
					// pre-parser says parse it. can we use it to figure out what type we are?
					// if we are still scanning to decide what the parse type is, we should be able to figure it out now...
					if (buffer == "<?xml version=\"1.0\"?>") {
						parse_type = Parse_xml;
						return NewParser(ad, file, detected_long, errmsg); // skip this line, but don't stop parsing, from now on
					} else if (buffer == "[" || buffer == "{") {
						char ch = buffer[0];
						// could be json or new classads, read character to figure out which.
						int ch2 = fgetc(file);
						if (ch == '{' && ch2 == '[') {
							inside_list = true;
							ungetc('[', file);
							parse_type = Parse_new;
							return NewParser(ad, file, detected_long, errmsg);
						} else if (ch == '[' && ch2 == '{') {
							inside_list = true;
							ungetc('{', file);
							parse_type = Parse_json;
							return NewParser(ad, file, detected_long, errmsg);
						} else {
							buffer = ""; buffer[0] = ch;
							readLine(buffer, file, true);
							chomp(buffer);
						}
					}
					// this doesn't look like a new classad prolog, so just parse it
					// using the line oriented parser.
					parse_type = Parse_long;
					errmsg = buffer;
					detected_long = true;
					return 0;
				}
			}
			
		} break;

		default: rval = -1; break;
	}

	return rval;
}


// returns number of attributes added to the ad
int
InsertFromFile(FILE* file, classad::ClassAd &ad, /*out*/ bool& is_eof, /*out*/ int& error, ClassAdFileParseHelper* phelp /*=NULL*/)
{
	int ee = 1;
	int cAttrs = 0;
	std::string buffer;

	if (phelp) {
		// new classad style parsers do all of the work in the NewParser callback
		// they will return non-zero to indicate that they are new classad style parsers.
		bool detected_long = false;
		cAttrs = phelp->NewParser(ad, file, detected_long, buffer);
		if (cAttrs > 0) {
			error = 0;
			is_eof = false;
			return cAttrs;
		} else if (cAttrs < 0) {
			if (cAttrs == -99) {
				error = 0;
				is_eof = true;
				return 0;
			}
			is_eof = feof(file);
			error = cAttrs;
			return phelp->OnParseError(buffer, ad, file);
		}
		// got a 0 from NewParser, fall down into the old (-long) style parser
		if (detected_long && ! buffer.empty()) {
			// buffer has the first line that we want to parse, jump into the
			// middle of the line-oriented parse loop.
			goto parse_line;
		}
	}

	while( 1 ) {

			// get a line from the file
		if ( ! readLine(buffer, file, false)) {
			is_eof = feof(file);
			error = is_eof ? 0 : errno;
			return cAttrs;
		}
		chomp(buffer);

		// if there is a helper, give the helper first crack at the line
		// otherwise set ee to decide what to do with this line.
		ee = 0;
		if (phelp) {
			ee = phelp->PreParse(buffer, ad, file);
		} else {
			// default is to skip blank lines and comment lines
			for (size_t ix = 0; ix < buffer.size(); ++ix) {
				if (buffer[ix] == '#' || buffer[ix] == '\n') {
					ee = 0; // skip blank line or comment
					break;
				}
				// ignore leading whitespace.
				if (buffer[ix] == ' ' || buffer[ix] == '\t') {
					continue;
				}
				ee = 1; // 1 is parse
				break;
			}
		}
		if (ee == 0) // comment or blank line, skip it and read the next
			continue;
		if (ee != 1) { // 1 is parse, <0, is abort, >1 is end_of_ad
			error = (ee < 0) ? ee : 0;
			is_eof = feof(file);
			return cAttrs;
		}

parse_line:
		// Insert the string into the classad
		if (InsertLongFormAttrValue(ad, buffer.c_str(), true) !=  0) {
			++cAttrs;
		} else {
			ee = -1;
			if (phelp) {
				ee = phelp->OnParseError(buffer, ad, file);
				if (1 == ee) {
					// buffer has (presumably) been modified, re-try parsing.
					// but only retry once.
					if (InsertLongFormAttrValue(ad, buffer.c_str(), true) != 0) {
						++cAttrs;
					} else {
						ee = phelp->OnParseError(buffer, ad, file);
						if (1 == ee) ee = -1;  // treat another attempt to reparse as a failure.
					}
				}
			}

			// the value of ee tells us whether to quit or keep going or
			// < 0 is abort
			//   0 is skip line - keep going
			//   1 is line was parsed, keep going
			// > 1 is end-of-ad, quit the loop
			if (ee < 0 || ee > 1) {
				error = ee > 1 ? 0 : ee;
				is_eof = feof(file);
				return cAttrs;
			}
		}
	}
}

int
InsertFromFile(FILE *file, classad::ClassAd &ad, const std::string &delim, int& is_eof, int& error, int &empty)
{
	CondorClassAdFileParseHelper helper(delim);
	bool eof_bool = false;
	int c_attrs = InsertFromFile(file, ad, eof_bool, error, &helper);
	is_eof = eof_bool;
	empty = c_attrs <= 0;
	return c_attrs;
}

bool CondorClassAdFileIterator::begin(
	FILE* fh,
	bool close_when_done,
	CondorClassAdFileParseHelper::ParseType type)
{
	parse_help = new CondorClassAdFileParseHelper("\n", type);
	free_parse_help = true;
	file = fh;
	close_file_at_eof = close_when_done;
	error = 0;
	at_eof = false;
	return true;
}

bool CondorClassAdFileIterator::begin(
	FILE* fh,
	bool close_when_done,
	CondorClassAdFileParseHelper & helper)
{
	parse_help = &helper;
	free_parse_help = false;
	file = fh;
	close_file_at_eof = close_when_done;
	error = 0;
	at_eof = false;
	return true;
}

int CondorClassAdFileIterator::next(ClassAd & classad, bool merge /*=false*/)
{
	if ( ! merge) classad.Clear();
	if (at_eof) return 0;
	if ( ! file) { error = -1; return -1; }

	int cAttrs = InsertFromFile(file, classad, at_eof, error, parse_help);
	if (cAttrs > 0) return cAttrs;
	if (at_eof) {
		if (file && close_file_at_eof) { fclose(file); file = NULL; }
		return 0;
	}
	if (error < 0)
		return error;
	return 0;
}

ClassAd * CondorClassAdFileIterator::next(classad::ExprTree * constraint)
{
	if (at_eof) return NULL;

	for (;;) {
		ClassAd * ad = new ClassAd();
		int cAttrs = this->next(*ad, true);
		bool include_classad = cAttrs > 0 && error >= 0;
		if (include_classad && constraint) {
			classad::Value val;
			if (ad->EvaluateExpr(constraint,val)) {
				if ( ! val.IsBooleanValueEquiv(include_classad)) {
					include_classad = false;
				}
			}
		}
		if (include_classad) {
			return ad;
		}
		delete ad;
		ad = NULL;

		if (at_eof || error < 0) break;
	}
	return NULL;
}

CondorClassAdFileParseHelper::ParseType CondorClassAdFileIterator::getParseType()
{
	if (parse_help) return parse_help->getParseType();
	return (CondorClassAdFileParseHelper::ParseType)-1;
}

CondorClassAdFileParseHelper::ParseType CondorClassAdListWriter::setFormat(CondorClassAdFileParseHelper::ParseType typ)
{
	if ( ! wrote_header && ! cNonEmptyOutputAds) out_format = typ;
	return out_format;
}

CondorClassAdFileParseHelper::ParseType CondorClassAdListWriter::autoSetFormat(CondorClassAdFileParseHelper & parse_help)
{
	if (out_format != CondorClassAdFileParseHelper::Parse_auto) return out_format;
	return setFormat(parse_help.getParseType());
}

// append a classad into the given output buffer, writing list header or separator as needed.
// return:
//    < 0 failure,
//    0   nothing written
//    1   non-empty ad appended
int CondorClassAdListWriter::appendAd(const ClassAd & ad, std::string & output, const classad::References * includelist, bool hash_order)
{
	if (ad.size() == 0) return 0;
	size_t cchBegin = output.size();

	classad::References attrs;
	classad::References *print_order = NULL;
	if ( ! hash_order || includelist) {
		sGetAdAttrs(attrs, ad, true, includelist);
		print_order = &attrs;
	}

	// if we havn't picked a format yet, pick long.
	if (out_format < ClassAdFileParseType::Parse_long || out_format > ClassAdFileParseType::Parse_new) {
		out_format = ClassAdFileParseType::Parse_long;
	}

	switch (out_format) {
	default:
	case ClassAdFileParseType::Parse_long: {
			if (print_order) {
				sPrintAdAttrs(output, ad, *print_order);
			} else {
				sPrintAd(output, ad);
			}
			if (output.size() > cchBegin) { output += "\n"; }
		} break;

	case ClassAdFileParseType::Parse_json: {
			classad::ClassAdJsonUnParser  unparser;
			output += cNonEmptyOutputAds ? ",\n" : "[\n";
			if (print_order) {
				unparser.Unparse(output, &ad, *print_order);
			} else {
				unparser.Unparse(output, &ad);
			}
			if (output.size() > cchBegin+2) {
				needs_footer = wrote_header = true;
				output += "\n";
			} else {
				output.erase(cchBegin);
			}
		} break;

	case ClassAdFileParseType::Parse_new: {
			classad::ClassAdUnParser  unparser;
			output += cNonEmptyOutputAds ? ",\n" : "{\n";
			if (print_order) {
				unparser.Unparse(output, &ad, *print_order);
			} else {
				unparser.Unparse(output, &ad);
			}
			if (output.size() > cchBegin+2) {
				needs_footer = wrote_header = true;
				output += "\n";
			} else {
				output.erase(cchBegin);
			}
		} break;

	case ClassAdFileParseType::Parse_xml: {
			classad::ClassAdXMLUnParser  unparser;
			unparser.SetCompactSpacing(false);
			size_t cchTmp = cchBegin;
			if (0 == cNonEmptyOutputAds) {
				AddClassAdXMLFileHeader(output);
				cchTmp = output.size();
			}
			if (print_order) {
				unparser.Unparse(output, &ad, *print_order);
			} else {
				unparser.Unparse(output, &ad);
			}
			if (output.size() > cchTmp) {
				needs_footer = wrote_header = true;
				//no extra newlines for xml
				// output += "\n";
			} else {
				output.erase(cchBegin);
			}
		} break;
	}

	if (output.size() > cchBegin) {
		++cNonEmptyOutputAds;
		return 1;
	}
	return 0;
}

// append a classad list footer into the given output buffer if needed
// return:
//    < 0 failure,
//    0   nothing written
//    1   non-empty ad appended
int CondorClassAdListWriter::appendFooter(std::string & buf, bool xml_always_write_header_footer)
{
	int rval = 0;
	switch (out_format) {
	case ClassAdFileParseType::Parse_xml:
		if ( ! wrote_header) {
			if (xml_always_write_header_footer) {
				// for XML output, we ALWAYS write the header and footer, even if there were no ads.
				AddClassAdXMLFileHeader(buf);
			} else {
				break;
			}
		}
		AddClassAdXMLFileFooter(buf);
		rval = 1;
		break;
	case ClassAdFileParseType::Parse_new:
		if (cNonEmptyOutputAds) { buf += "}\n"; rval = 1; }
		break;
	case ClassAdFileParseType::Parse_json:
		if (cNonEmptyOutputAds) { buf += "]\n"; rval = 1; }
	default:
		// nothing to do.
		break;
	}
	needs_footer = false;
	return rval;
}

// write a classad into the given output stream attributes are sorted unless hash_order is true
// return:
//    < 0 failure,
//    0   nothing written
//    1   non-empty ad written
static const int cchReserveForPrintingAds = 16384;
int CondorClassAdListWriter::writeAd(const ClassAd & ad, FILE * out, const classad::References * includelist, bool hash_order)
{
	buffer.clear();
	if ( ! cNonEmptyOutputAds) buffer.reserve(cchReserveForPrintingAds);

	int rval = appendAd(ad, buffer, includelist, hash_order);
	if (rval < 0) return rval;

	if ( ! buffer.empty()) {
		fputs(buffer.c_str(), out);
	}
	return rval;
}

// write a classad list footer into the given output stream if needed
// return:
//    < 0 failure,
//    0   nothing written
//    1   non-empty ad written
int CondorClassAdListWriter::writeFooter(FILE* out, bool xml_always_write_header_footer)
{
	buffer.clear();
	appendFooter(buffer, xml_always_write_header_footer);
	if ( ! buffer.empty()) {
		int rval = fputs(buffer.c_str(), out);
		return (rval < 0) ? rval : 1;
	}
	return 0;
}


bool
ClassAdAttributeIsPrivateV1( const std::string &name )
{
	return ClassAdPrivateAttrs.find(name) != ClassAdPrivateAttrs.end();
}

bool
ClassAdAttributeIsPrivateV2( const std::string &name )
{
	return strncasecmp(name.c_str(), "_condor_priv", 12) == 0;
}

bool
ClassAdAttributeIsPrivateAny( const std::string &name ) {return ClassAdAttributeIsPrivateV2(name) || ClassAdAttributeIsPrivateV1(name);}

int
EvalAttr( const char *name, classad::ClassAd *my, classad::ClassAd *target, classad::Value & value)
{
	int rc = 0;

	if( target == my || target == NULL ) {
		if( my->EvaluateAttr( name, value ) ) {
			rc = 1;
		}
		return rc;
	}

	getTheMatchAd( my, target );
	if( my->Lookup( name ) ) {
		if( my->EvaluateAttr( name, value ) ) {
			rc = 1;
		}
	} else if( target->Lookup( name ) ) {
		if( target->EvaluateAttr( name, value ) ) {
			rc = 1;
		}
	}
	releaseTheMatchAd();
	return rc;
}

int
EvalString(const char *name, classad::ClassAd *my, classad::ClassAd *target, std::string & value)
{
	int rc = 0;

	if( target == my || target == NULL ) {
		if( my->EvaluateAttrString( name, value ) ) {
			rc = 1;
		}
	}
	else
	{
	  getTheMatchAd( my, target );
	  if( my->Lookup( name ) ) {
		  if( my->EvaluateAttrString( name, value ) ) {
			  rc = 1;
		  }
	  } else if( target->Lookup( name ) ) {
		  if( target->EvaluateAttrString( name, value ) ) {
			  rc = 1;
		  }
	  }
	  releaseTheMatchAd();
	}

	return rc;
}

int
EvalInteger (const char *name, classad::ClassAd *my, classad::ClassAd *target, long long &value)
{
	int rc = 0;

	if( target == my || target == NULL ) {
		if( my->EvaluateAttrNumber( name, value ) ) {
			rc = 1;
		}
	}
	else 
	{
	  getTheMatchAd( my, target );
	  if( my->Lookup( name ) ) {
		  if( my->EvaluateAttrNumber( name, value ) ) {
			  rc = 1;
		  }
	  } else if( target->Lookup( name ) ) {
		  if( target->EvaluateAttrNumber( name, value ) ) {
			  rc = 1;
		  }
	  }
	  releaseTheMatchAd();
	}

	return rc;
}

int EvalInteger (const char *name, classad::ClassAd *my, classad::ClassAd *target, int& value) {
	long long ival = 0;
	int result = EvalInteger(name, my, target, ival);
	if ( result ) {
		value = (int)ival;
	}
	return result;
}

int EvalInteger (const char *name, classad::ClassAd *my, classad::ClassAd *target, long & value) {
	long long ival = 0;
	int result = EvalInteger(name, my, target, ival);
	if ( result ) {
		value = (long)ival;
	}
	return result;
}

int
EvalFloat (const char *name, classad::ClassAd *my, classad::ClassAd *target, double &value)
{
	int rc = 0;

	if( target == my || target == NULL ) {
		if( my->EvaluateAttrNumber( name, value ) ) {
			rc = 1;
		}
		return rc;
	}

	getTheMatchAd( my, target );
	if( my->Lookup( name ) ) {
		if( my->EvaluateAttrNumber( name, value ) ) {
			rc = 1;
		}
	} else if( target->Lookup( name ) ) {
		if( target->EvaluateAttrNumber( name, value ) ) {
			rc = 1;
		}
	}
	releaseTheMatchAd();
	return rc;
}

int EvalFloat (const char *name, classad::ClassAd *my, classad::ClassAd *target, float &value) {
	double dval = 0.0;
	int result = EvalFloat(name, my, target, dval);
	if ( result ) {
		value = dval;
	}
	return result;
}

int
EvalBool (const char *name, classad::ClassAd *my, classad::ClassAd *target, bool &value)
{
	int rc = 0;

	if( target == my || target == NULL ) {
		if( my->EvaluateAttrBoolEquiv( name, value ) ) {
			rc = 1;
		}
		return rc;
	}

	getTheMatchAd( my, target );
	if( my->Lookup( name ) ) {
		if( my->EvaluateAttrBoolEquiv( name, value ) ) {
			rc = 1;
		}
	} else if( target->Lookup( name ) ) {
		if( target->EvaluateAttrBoolEquiv( name, value ) ) {
			rc = 1;
		}
	}

	releaseTheMatchAd();
	return rc;
}

bool
initAdFromString( char const *str, classad::ClassAd &ad )
{
	bool succeeded = true;

	// First, clear our ad so we start with a fresh ClassAd
	ad.Clear();

	char *exprbuf = new char[strlen(str)+1];
	ASSERT( exprbuf );

	while( *str ) {
		while( isspace(*str) ) {
			str++;
		}

		size_t len = strcspn(str,"\n");
		strncpy(exprbuf,str,len);
		exprbuf[len] = '\0';

		if( str[len] == '\n' ) {
			len++;
		}
		str += len;

		if (!InsertLongFormAttrValue(ad, exprbuf, true)) {
			dprintf(D_ALWAYS,"Failed to parse ClassAd expression: '%s'\n",
					exprbuf);
			succeeded = false;
			break;
		}
	}

	delete [] exprbuf;
	return succeeded;
}


		// output functions
bool
fPrintAd( FILE *file, const classad::ClassAd &ad, bool exclude_private, const classad::References *attr_include_list, const classad::References *excludeAttrs )
{
	std::string buffer;

	if( exclude_private ) {
		sPrintAd( buffer, ad, attr_include_list, excludeAttrs);
	} else {
		sPrintAdWithSecrets( buffer, ad, attr_include_list, excludeAttrs);
	}

	if (fputs(buffer.c_str(), file) < 0 ) {
		return false;
	} else {
		return true;
	}
}

void
dPrintAd( int level, const classad::ClassAd &ad, bool exclude_private )
{
	if ( IsDebugCatAndVerbosity( level ) ) {
		std::string buffer;

		if( exclude_private ) {
			sPrintAd( buffer, ad );
		} else {
			sPrintAdWithSecrets( buffer, ad );
		}

		dprintf( level|D_NOHEADER, "%s", buffer.c_str() );
	}
}

int sortByFirst(const std::pair<std::string, ExprTree *> & lhs,
				const std::pair<std::string, ExprTree *> & rhs) {
	return lhs.first < rhs.first;
}

int
_sPrintAd( std::string &output, const classad::ClassAd &ad, bool exclude_private, const classad::References *attr_include_list, const classad::References *excludeAttrs /* = nullptr */)
{
	classad::ClassAd::const_iterator itr;

	classad::ClassAdUnParser unp;
	unp.SetOldClassAd( true, true );
	const classad::ClassAd *parent = ad.GetChainedParentAd();

	// Not sure why we need to sort the output. Do we?
	std::vector<std::pair<std::string, ExprTree *>> attributes;

	attributes.reserve(ad.size() + ( parent ? parent->size() : 0));
	if ( parent ) {
		for ( itr = parent->begin(); itr != parent->end(); itr++ ) {
			if ( attr_include_list && !attr_include_list->contains(itr->first) ) {
				continue; // not in include-list
			}

			if (excludeAttrs && (excludeAttrs->find(itr->first) != excludeAttrs->end())) {
				continue;
			}

			if ( ad.LookupIgnoreChain(itr->first) ) {
				continue; // attribute exists in child ad; we will print it below
			}
			if ( !exclude_private ||
				 !ClassAdAttributeIsPrivateAny( itr->first ) ) {
				attributes.emplace_back(itr->first,itr->second);
			}
		}
	}

	for ( itr = ad.begin(); itr != ad.end(); itr++ ) {
		if ( attr_include_list && !attr_include_list->contains(itr->first) ) {
			continue; // not in include-list
		}

		if (excludeAttrs && (excludeAttrs->find(itr->first) != excludeAttrs->end())) {
			continue;
		}

		if ( !exclude_private ||
			 !ClassAdAttributeIsPrivateAny( itr->first ) ) {
			attributes.emplace_back(itr->first,itr->second);
		}
	}

	std::sort(attributes.begin(), attributes.end(), sortByFirst);

	for( const auto &i : attributes) {
		output += i.first;
		output += " = ";
		unp.Unparse( output, i.second );
		output += '\n';
	}

	return true;
}

bool
sPrintAd( std::string &output, const classad::ClassAd &ad, const classad::References *attr_include_list, const classad::References *excludeAttrs )
{
	return _sPrintAd( output, ad, true, attr_include_list, excludeAttrs );
}

bool
sPrintAdWithSecrets( std::string &output, const classad::ClassAd &ad, const classad::References *attr_include_list, const classad::References *excludeAttrs )
{
	return _sPrintAd( output, ad, false, attr_include_list, excludeAttrs );
}

/** Get a sorted list of attributes that are in the given ad, and also match the given includelist (if any)
	and privacy criteria.
	@param attrs the set of attrs to insert into. This is set is NOT cleared first.
	@return true
*/
bool
sGetAdAttrs( classad::References &attrs, const classad::ClassAd &ad, bool exclude_private, const classad::References *attr_include_list, bool ignore_parent )
{
	classad::ClassAd::const_iterator itr;

	for ( itr = ad.begin(); itr != ad.end(); itr++ ) {
		if ( attr_include_list && !attr_include_list->contains(itr->first)) {
			continue; // not in white-list
		}
		if ( !exclude_private ||
			 !ClassAdAttributeIsPrivateAny( itr->first ) ) {
			attrs.insert(itr->first);
		}
	}

	const classad::ClassAd *parent = ad.GetChainedParentAd();
	if ( parent && ! ignore_parent) {
		for ( itr = parent->begin(); itr != parent->end(); itr++ ) {
			if ( attrs.find(itr->first) != attrs.end() ) {
				continue; // we already inserted this into the attrs list.
			}
			if ( attr_include_list && !attr_include_list->contains(itr->first) ) {
				continue; // not in white-list
			}
			if ( !exclude_private ||
				 !ClassAdAttributeIsPrivateAny( itr->first ) ) {
				attrs.insert(itr->first);
			}
		}
	}

	return true;
}

/** Format the given attributes from the ClassAd as an old ClassAd into the given string
	@param output The std::string to write into
	@return true
*/
bool
sPrintAdAttrs( std::string &output, const classad::ClassAd &ad, const classad::References &attrs, const char * indent /*=NULL*/ )
{
	classad::ClassAdUnParser unp;
	unp.SetOldClassAd( true, true );

	classad::References::const_iterator it;
	for (it = attrs.begin(); it != attrs.end(); ++it) {
		const ExprTree * tree = ad.Lookup(*it); // use Lookup rather than find in case we have a parent ad.
		if (tree) {
			if (indent) output += indent;
			output += *it;
			output += " = ";
			unp.Unparse( output, tree );
			output += "\n";
		}
	}

	return true;
}

/** Format the ClassAd as an old ClassAd into the std::string, and return the c_str() of the result
	This version if the classad function prints the attributes in sorted order and allows for an optional
	indent character to be printed at the start of each line. A terminating \n is guaranteed, even if the
	incoming ad is empty. This makes it ideal for use with dprintf() like this
	     if (IsDebugLevel(D_JOB)) { dprintf(D_JOB, "Job ad:\n%s", formatAd(buf, ad, "\t")); }
	@param output The std::string to write into
	@return std::string.c_str()
*/
const char * formatAd(std::string & buffer, const classad::ClassAd &ad, const char * indent /*= "\t"*/, const classad::References *attr_include_list /*= NULL*/, bool exclude_private /*= false*/)
{
	classad::References attrs;
	sGetAdAttrs(attrs, ad, exclude_private, attr_include_list);
	sPrintAdAttrs(buffer, ad, attrs, indent);
	if (buffer.empty() || (buffer[buffer.size()-1] != '\n')) { buffer += "\n"; }
	return buffer.c_str();
}


// Taken from the old classad's function. Got rid of incorrect documentation. 
////////////////////////////////////////////////////////////////////////////////// Print an expression with a certain name into a buffer. 
// The returned buffer will be allocated with malloc(), and it needs
// to be free-ed with free() by the user.
////////////////////////////////////////////////////////////////////////////////
char*
sPrintExpr(const classad::ClassAd &ad, const char* name)
{
	char *buffer = NULL;
	size_t buffersize = 0;
	classad::ClassAdUnParser unp;
	std::string parsedString;
	classad::ExprTree* expr;

	unp.SetOldClassAd( true, true );

    expr = ad.Lookup(name);

    if(!expr)
    {
        return NULL;
    }

    unp.Unparse(parsedString, expr);

    buffersize = strlen(name) + parsedString.length() +
                    3 +     // " = "
                    1;      // null termination
    buffer = (char*) malloc(buffersize);
    ASSERT( buffer != NULL );

    snprintf(buffer, buffersize, "%s = %s", name, parsedString.c_str() );
    buffer[buffersize - 1] = '\0';

    return buffer;
}

// ClassAd methods

		// Type operations
void
SetMyTypeName( classad::ClassAd &ad, const char *myType )
{
	if( myType ) {
		ad.InsertAttr( ATTR_MY_TYPE, std::string( myType ) );
	}

	return;
}

const char*
GetMyTypeName( const classad::ClassAd &ad )
{
	static std::string myTypeStr;
	if( !ad.EvaluateAttrString( ATTR_MY_TYPE, myTypeStr ) ) {
		return "";
	}
	return myTypeStr.c_str( );
}

// Determine if a value is valid to be written to the log. The value
// is a RHS of an expression. According to LogSetAttribute::WriteBody,
// the only invalid character is a '\n'.
bool
IsValidAttrValue(const char *value)
{
    //NULL value is not invalid, may translate to UNDEFINED
    if(!value)
    {
        return true;
    }

    //According to the old classad's docs, LogSetAttribute::WriteBody
    // says that the only invalid character for a value is '\n'.
    // But then it also includes '\r', so whatever.
    while (*value) {
        if(((*value) == '\n') ||
           ((*value) == '\r')) {
            return false;
        }
        value++;
    }

    return true;
}

//	Decides if a string is a valid attribute name, the LHS
//  of an expression.  As per the manual, valid names:
//
//  Attribute names are sequences of alphabetic characters, digits and 
//  underscores, and may not begin with a digit

bool
IsValidAttrName(const char *name) {
		// NULL pointer certainly false
	if (!name) {
		return false;
	}

		// Must start with alpha or _
	if (!isalpha(*name) && *name != '_') {
		return false;
	}

	name++;

		// subsequent letters must be alphanum or _
	while (*name) {
		if (!isalnum(*name) && *name != '_') {
			return false;
		}
		name++;
	}

	return true;
}

void
CopyAttribute(const std::string &target_attr, classad::ClassAd &target_ad, const std::string &source_attr, const classad::ClassAd &source_ad)
{
	classad::ExprTree *e = source_ad.Lookup( source_attr );
	if ( e ) {
		e = e->Copy();
		target_ad.Insert( target_attr, e );
	} else {
		target_ad.Delete( target_attr );
	}
}

void
CopyAttribute(const std::string &target_attr, classad::ClassAd &target_ad, const classad::ClassAd &source_ad)
{
	CopyAttribute(target_attr, target_ad, target_attr, source_ad);
}

void
CopyAttribute(const std::string &target_attr, classad::ClassAd &target_ad, const std::string &source_attr)
{
	CopyAttribute(target_attr, target_ad, source_attr, target_ad);
}

//////////////XML functions///////////

bool
fPrintAdAsXML(FILE *fp, const classad::ClassAd &ad, const classad::References *attr_include_list)
{
    if(!fp)
    {
        return false;
    }

    std::string out;
    sPrintAdAsXML(out,ad,attr_include_list);
    fprintf(fp, "%s", out.c_str());
    return true;
}

bool
sPrintAdAsXML(std::string &output, const classad::ClassAd &ad, const classad::References *attr_include_list)
{
	classad::ClassAdXMLUnParser unparser;
	std::string xml;

	unparser.SetCompactSpacing(false);
	if ( attr_include_list ) {
		unparser.Unparse( xml, &ad, *attr_include_list );
	} else {
		unparser.Unparse( xml, &ad );
	}
	output += xml;
	return true;
}
///////////// end XML functions /////////

bool
fPrintAdAsJson(FILE *fp, const classad::ClassAd &ad, const classad::References *attr_include_list, bool oneline)
{
    if(!fp)
    {
        return false;
    }

    std::string out;
    sPrintAdAsJson(out,ad,attr_include_list,oneline);
    fprintf(fp, "%s", out.c_str());
    return true;
}

bool
sPrintAdAsJson(std::string &output, const classad::ClassAd &ad, const classad::References *attr_include_list, bool oneline)
{
	classad::ClassAdJsonUnParser unparser(oneline);

	if ( attr_include_list ) {
		unparser.Unparse( output, &ad, *attr_include_list );
	} else {
		unparser.Unparse( output, &ad );
	}
	return true;
}

char const *
QuoteAdStringValue(char const *val, std::string &buf)
{
    if(val == NULL) {
        return NULL;
    }

    buf.clear();

    classad::Value tmpValue;
    classad::ClassAdUnParser unparse;

	unparse.SetOldClassAd( true, true );

    tmpValue.SetStringValue(val);
    unparse.Unparse(buf, tmpValue);

    return buf.c_str();
}

void
ChainCollapse(classad::ClassAd &ad)
{
    classad::ExprTree *tmpExprTree;

	classad::ClassAd *parent = ad.GetChainedParentAd();

    if(!parent)
    {   
        //nothing chained, time to leave
        return;
    }

    ad.Unchain();

    classad::AttrList::iterator itr; 

    for(itr = parent->begin(); itr != parent->end(); itr++)
    {
        // Only move the value from our chained ad into our ad when it 
        // does not already exist. Hence the Lookup(). 
        // This means that the attributes in our classad takes precedence
        // over the ones in the chained class ad.

        if( !ad.Lookup((*itr).first) )
        {
            tmpExprTree = (*itr).second;     

            //deep copy it!
            tmpExprTree = tmpExprTree->Copy(); 
            ASSERT(tmpExprTree); 

            //K, it's clear. Insert it, but don't try to 
            ad.Insert((*itr).first, tmpExprTree);
        }
    }
}


// the freestanding functions 

bool
GetReferences( const char* attr, const classad::ClassAd &ad,
               classad::References *internal_refs,
               classad::References *external_refs )
{
	ExprTree *tree;

	tree = ad.Lookup( attr );
	if ( tree != NULL ) {
		return GetExprReferences( tree, ad, internal_refs, external_refs );
	} else {
		return false;
	}
}

bool
GetExprReferences( const char* expr, const classad::ClassAd &ad,
                   classad::References *internal_refs,
                   classad::References *external_refs )
{
	bool rv = false;
	classad::ClassAdParser par;
	classad::ExprTree *tree = NULL;
	par.SetOldClassAd( true );

	if ( !par.ParseExpression( expr, tree, true ) ) {
		return false;
	}

	rv = GetExprReferences( tree, ad, internal_refs, external_refs );

	delete tree;

	return rv;
}

bool
GetExprReferences( const classad::ExprTree *tree, const classad::ClassAd &ad,
                   classad::References *internal_refs,
                   classad::References *external_refs )
{
	if ( tree == NULL ) {
		return false;
	}

	classad::References ext_refs_set;
	classad::References int_refs_set;
	classad::References::iterator set_itr;

	bool ok = true;
	if( external_refs && !ad.GetExternalReferences(tree, ext_refs_set, true) ) {
		ok = false;
	}
	if( internal_refs && !ad.GetInternalReferences(tree, int_refs_set, true) ) {
		ok = false;
	}
	if( !ok ) {
		dprintf(D_FULLDEBUG,"warning: failed to get all attribute references in ClassAd (perhaps caused by circular reference).\n");
		dPrintAd(D_FULLDEBUG, ad);
		dprintf(D_FULLDEBUG,"End of offending ad.\n");
		return false;
	}

	if ( external_refs ) {
		TrimReferenceNames( ext_refs_set, true );
		external_refs->insert( ext_refs_set.begin(), ext_refs_set.end() );
	}
	if ( internal_refs ) {
		TrimReferenceNames( int_refs_set, false );
		internal_refs->insert( int_refs_set.begin(), int_refs_set.end() );
	}
	return true;
}

void TrimReferenceNames( classad::References &ref_set, bool external )
{
	classad::References new_set;
	classad::References::iterator it;
	for ( it = ref_set.begin(); it != ref_set.end(); it++ ) {
		const char *name = it->c_str();
		if ( external ) {
			if ( strncasecmp( name, "target.", 7 ) == 0 ) {
				name += 7;
			} else if ( strncasecmp( name, "other.", 6 ) == 0 ) {
				name += 6;
			} else if ( strncasecmp( name, ".left.", 6 ) == 0 ) {
				name += 6;
			} else if ( strncasecmp( name, ".right.", 7 ) == 0 ) {
				name += 7;
			} else if ( name[0] == '.' ) {
				name += 1;
			}
		} else {
			if ( name[0] == '.' ) {
				name += 1;
			}
		}
		size_t spn = strcspn( name, ".[" );
		new_set.insert( std::string( name, spn ) );
	}
	ref_set.swap( new_set );
}

const char *ConvertEscapingOldToNew( const char *str )
{
	static std::string new_str;
	new_str = "";
	ConvertEscapingOldToNew( str, new_str );
	return new_str.c_str();
}

void ConvertEscapingOldToNew( const char *str, std::string &buffer )
{
		// String escaping is different between new and old ClassAds.
		// We need to convert the escaping from old to new style before
		// handing the expression to the new ClassAds parser.
	while( *str ) {
		size_t n = strcspn(str,"\\");
		buffer.append(str,n);
		str += n;
		if ( *str == '\\' ) {
			buffer.append( 1, '\\' );
			str++;
			if(  (str[0] != '"') ||
				 ( /*(str[0] == '"') && */ IsStringEnd(str, 1) )   )
			{
				buffer.append( 1, '\\' );
			}
		}
	}
		// remove trailing whitespace
	int ix = (int)buffer.size();
	while (ix > 1) {
		char ch = buffer[ix-1];
		if (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n')
			break;
		--ix;
	}
	buffer.resize(ix);
}

// split a single line of -long-form classad into attr and value part
// removes leading whitespace and whitespace around the =
// set rhs to point to the first non whitespace character after the =
// you can pass this to ConvertEscapingOldToNew
// returns true if there was an = and the attr was non-empty
//
bool SplitLongFormAttrValue(const char * line, std::string &attr, const char* &rhs)
{
	while (isspace(*line)) ++line;

	const char * peq = strchr(line, '=');
	if ( ! peq) return false;

	const char * p = peq;
	while (p > line && ' ' == p[-1]) --p;
	attr.clear();
	attr.append(line, p-line);

	// set rhs to the first non-space character after the =
	p = peq+1;
	while (' ' == *p) ++p;
	rhs = p;

	return !attr.empty();
}

// split a single line of -long form classad into addr and value, then
// parse and insert into the given classad, using the classadCache or not as requested.
//
bool InsertLongFormAttrValue(classad::ClassAd & ad, const char * line, bool use_cache)
{
	std::string attr;
	const char * rhs;
	if ( ! SplitLongFormAttrValue(line, attr, rhs)) {
		return false;
	}

	if (use_cache) {
		return ad.InsertViaCache(attr, rhs);
	}

	classad::ClassAdParser parser;
	parser.SetOldClassAd(true);
	ExprTree *tree = parser.ParseExpression(rhs);
	if ( ! tree) {
		return false;

	}
	return ad.Insert(attr, tree);
}

// end functions
