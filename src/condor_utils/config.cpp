/***************************************************************
 *
 * Copyright (C) 1990-2011, Condor Team, Computer Sciences Department,
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
#include "condor_debug.h"
#include "pool_allocator.h"
#include "condor_config.h"
#include "param_info.h"
#include "param_info_tables.h"
#include "condor_string.h"
#include "extra_param_info.h"
#include "condor_random_num.h"
#include "condor_uid.h"
#include "my_popen.h"

#if defined(__cplusplus)
extern "C" {
#endif

#define CONFIG_GETLINE_OPT_COMMENT_DOESNT_CONTINUE        1
#define CONFIG_GETLINE_OPT_CONTINUE_MAY_BE_COMMENTED_OUT  2
static char *getline_implementation(FILE * fp, int buffer_size, int options);
extern "C++" void param_default_set_use(const char * name, int use, MACRO_SET & set);

int		ConfigLineNo;

/* WARNING: When we mean alphanumeric in this snippet of code, we really mean 
	characters that are legal in a C indentifier plus period and forward slash.
	It looks like what character set is allowable to be in the default value
	of the $$ expansion hasn't been thought about very well....

	XXX: If you've come here looking to add \ so windows paths may be
	substituted as the default value in a $$ expansion with a default
	value, be very careful. The $$() expansion algorithm is deep in
	the parsing of the RHS of the attr/value pair, and at the writing
	of this comment, it is unknown if \ substitution would happen
	before/during/after the $$ expansion would happen, in which case
	you'd be screwed and have to understand/alter much more code.
	If you perform these code alterings to support \ in this manner,
	then remove this XXX comment.
*/
int
condor_isidchar(int c)
{
	if( ('a' <= c && c <= 'z') ||
		('A' <= c && c <= 'Z') ||
		('0' <= c && c <= '9') ||
		/* See the above comment for this function and about the next line. */
		(strchr("_./", c) != NULL) )
	{
		return 1;
	} 

	return 0;
}

#define ISIDCHAR(c)		( condor_isidchar(c) )

// $$ expressions may also contain a colon
#define ISDDCHAR(c) ( ISIDCHAR(c) || ((c)==':') )

#define ISOP(c)		(((c) == '=') || ((c) == ':'))

// Magic macro to represent a dollar sign, i.e. $(DOLLAR)="$"
#define DOLLAR_ID "DOLLAR"
// The length of the DOLLAR_ID string
// Should probably use constexpr here when we use C++11 in earnest
#define DOLLAR_ID_LEN (6)

int is_valid_param_name(const char *name)
{
		/* Check that "name" is a legal identifier : only
		   alphanumeric characters and _ allowed*/
	while( *name ) {
		if( !ISIDCHAR(*name++) ) {
			return 0;
		}
	}

	return 1;
}

// This is used by daemon_core to help it with DC_CONFIG_PERSIST & DC_CONFIG_RUNTIME
// this code validates the passed in config assignment before it is written into the
// persist or runtime tables.  it also returns a copy of the parameter name extracted
// from the config line.
//
char * is_valid_config_assignment(const char *config)
{
	char *name, *tmp = NULL;

	while (isspace(*config)) ++config;

	bool is_meta = starts_with_ignore_case(config, "use ");
	if (is_meta) {
		config += 4;
		while (isspace(*config)) ++config;
		--config; // leave room for leading $
	}

	if (!(name = strdup(config))) {
		EXCEPT("Out of memory!");
	}

	// if this is a metaknob assigment, we have to check to see if the category and value are valid.
	// and set the config name to be $category.option
	if (is_meta) {
		name[0] = '$'; // mark config name as being a metaknob name.

		bool is_valid = false;
		// name points to the category name, everything after the colon must be a list of options for that category
		tmp = strchr(name, ':');
		if (tmp) {
			// turn the right hand side into a string list
			StringList opts(tmp+1);

			// null terminate and trim trailing whitespace from the category name
			*tmp = 0; 
			while (tmp > name && isspace(tmp[-1])) --tmp;
			*tmp = 0;

			// the proper way to parse the right hand side of a metaknob is by using a stringlist
			// but for remote setting, we really only want to allow a single options on the right hand side.
			opts.rewind();
			char * opt;
			while ((opt = opts.next())) {
				// lookup name,val as a metaknob, a return of -1 means not found
				if ( ! is_valid && param_default_get_source_meta_id(name+1, opt) >= 0) {
					is_valid = true;
					// append the value to the metaknob name.
					*tmp++ = '.';
					strcpy(tmp, opt);
					tmp += strlen(tmp);
					continue;
				}
				// if we get here, either we failed to lookup the option, or we saw more the one option.
				is_valid = false;
				break;
			}
			if (is_valid) return name;
		}
		free(name);
		return NULL; // indicate failure.

	} else { // not a metaknob, just a knob.

		tmp = strchr(name, '=');
		#ifdef WARN_COLON_FOR_PARAM_ASSIGN
		// for remote param set calls, we don't want to allow the : style assignment at all. 
		#else
		char * tmp2 = strchr(name, ':');
		if ( ! tmp || (tmp2 && tmp2 < tmp)) tmp = tmp2;
		#endif

		if (!tmp) {
				// Line is invalid, should be "name = value" (or "name : value" if ! WARN_COLON_FOR_PARAM_ASSIGN)
			free (name);
			return NULL;
		}

			// Trim whitespace from the param name.
		*tmp = ' ';
		while (isspace(*tmp)) {
			*tmp = '\0';
			tmp--;
		}

	}
	return name;
}

bool 
is_piped_command(const char* filename)
{
	bool retVal = false;

	char const *pdest = strchr( filename, '|' );
	if ( pdest != NULL ) {
		// This is not a filename (still not sure it's a valid command though)
		retVal = true;
	}

	return retVal;
}

int Parse_config_string(MACRO_SOURCE & source, int depth, const char * config, MACRO_SET& macro_set, const char * subsys);

int read_meta_config(MACRO_SOURCE & source, int depth, const char *name, const char * rhs, MACRO_SET& macro_set, const char * subsys)
{
#ifdef GUESS_METAKNOB_CATEGORY
	std::string nameguess;
	if ( ! name || ! name[0]) {
		// guess the name by looking for matches on the rhs.
		for (int id = 0; ; ++id) {
			MACRO_DEF_ITEM * pmet = param_meta_source_by_id(id);
			if ( ! pmet) break;
			const char * pcolon = strchr(pmet->key, ':');
			if ( ! pcolon) continue;
			if (MATCH == strcasecmp(rhs, pcolon+1)) {
				nameguess = pmet->key;
				nameguess[pcolon - pmet->key] = 0;
				name = nameguess.c_str();
				break;
			}
		}
	}
#endif

	if ( ! name || ! name[0]) {
		fprintf(stderr,
				"Configuration Error: use needs a keyword before : %s\n", rhs);
		return -1;
	}

	MACRO_TABLE_PAIR* ptable = param_meta_table(name);
	if ( ! ptable)
		return -1;

	StringList items(rhs);
	items.rewind();
	char * item;
	while ((item = items.next()) != NULL) {
		const char * value = param_meta_table_string(ptable, item);
		if ( ! value) {
			fprintf(stderr,
					"Configuration Error: use %s: does not recognise %s\n",
					name, item);
			return -1;
		}
		source.meta_id = param_default_get_source_meta_id(name, item);
		int ret = Parse_config_string(source, depth, value, macro_set, subsys);
		if (ret < 0) {
			const char * msg = "Internal Configuration Error: use %s: %s is invalid\n";
			if (ret == -2) msg = "Configuration Error: use %s: %s nesting too deep\n"; 
			fprintf(stderr, msg, name, item);
			return ret;
		}
	}

	source.meta_id = -1;
	return 0;
}

// Make sure the last character is the '|' char.  For now, that's our only criteria.
bool 
is_valid_command(const char* cmdToExecute)
{
	bool retVal = false;

	int cmdStrLength = strlen(cmdToExecute);
	if ( cmdToExecute[cmdStrLength - 1] == '|' ) {
		retVal = true;
	}

	return retVal;
}

/*
** Special version of expand_macro that only expands 'self' references. i.e. it only
** expands the macro whose name is specified in the self argument.
** Expand parameter references of the form "left$(self)right".  This
** is deceptively simple, but does handle multiple and or nested references.
** We only expand references to to the parameter specified by self. use expand_macro
** to expand all references. 
*/
extern "C" char *
expand_self_macro(const char *value,
			 MACRO_SET& macro_set,
			 const char *self,
			 const char *subsys)
{
	char *tmp = strdup( value );
	char *left, *name, *right;
	const char *tvalue;
	char *rval;

	ASSERT(self != NULL && self[0] != 0);

	// to avoid infinite recursive expansion, we have to look for both "subsys.self" and "self"
	// so we want to set selfless equal to the part of self after the prefix.
	const char *selfless = NULL; // if self=="master.foo" and subsys=="master", then this contains "foo"
	if (subsys) {
		const char * a = subsys;
		const char * b = self;
		while (*a && (tolower(*a) == tolower(*b))) {
			++a; ++b;
		}
		// if a now points to a 0, and b now points to ".", then self contains subsys as a prefix.
		if (0 == a[0] && '.' == b[0] && b[1] != 0) {
			selfless = b+1;
		}
	}

	bool all_done = false;
	while( !all_done ) {		// loop until all done expanding
		all_done = true;

		if (find_config_macro(tmp, &left, &name, &right, self) ||
			(selfless && find_config_macro(tmp, &left, &name, &right, selfless)) ) {
			all_done = false;
		   #ifdef COLON_DEFAULT_FOR_MACRO_EXPAND
			char * pcolon = strchr(name, ':');
			if (pcolon) { *pcolon++ = 0; }
		   #endif
			tvalue = lookup_macro(name, subsys, macro_set, 0);
			if (subsys && ! tvalue)
				tvalue = lookup_macro(name, NULL, macro_set, 0);

				// Note that if 'name' has been explicitly set to nothing,
				// tvalue will _not_ be NULL so we will not call
				// param_default_string().  See gittrack #1302
			if (tvalue == NULL
				&& macro_set.defaults
				&& (macro_set.options & CONFIG_OPT_DEFAULTS_ARE_PARAM_INFO) != 0) {
				tvalue = param_default_string(name, subsys);
			}
		   #ifdef COLON_DEFAULT_FOR_MACRO_EXPAND
			if (pcolon && ( ! tvalue || ! tvalue[0])) {
				tvalue = pcolon;
			}
		   #endif
			if( tvalue == NULL ) {
				tvalue = "";
			}

			rval = (char *)MALLOC( (unsigned)(strlen(left) + strlen(tvalue) +
											  strlen(right) + 1));
			ASSERT( rval != NULL );
			(void)sprintf( rval, "%s%s%s", left, tvalue, right );
			FREE( tmp );
			tmp = rval;
		}
	}

	return( tmp );
}


typedef enum  {
	CIFT_EMPTY=0,
	CIFT_NUMBER,
	CIFT_BOOL,
	CIFT_IDENTIFIER,
	CIFT_MACRO,
	CIFT_VERSION,
	CIFT_IFDEF,
	CIFT_COMPLEX,
} expr_character_t;

// helper function to compare ptr case-insensitively to a known lower case literal
// leading and trailing whitespace is ignored, but the literal must otherwise match exactly.
// if no_trailing_token is true, there must be nothing after the literal but whitespace.
// if no_trailing_token is false, then the next character after the literal must not be
// an identifier token.
static bool matches_literal_ignore_case(const char * ptr, const char * lit, bool no_trailing_token=true)
{
	while (isspace(*ptr)) ++ptr;
	while (*lit) { if ((*ptr++ | 0x20) != *lit++) return false; }
	if (no_trailing_token) {
		while (isspace(*ptr)) ++ptr;
		return !*ptr;
	}
	return !isalnum(*ptr);
}

static expr_character_t Characterize_config_if_expression(const char * expr, bool keyword_check)
{
	const char * p = expr;
	while (isspace(*p)) ++p;
	if ( ! *p) return CIFT_EMPTY;

	const char * begin = p;

	// we don't want a leading - to confuse us into thinking we are seeing a sum
	bool leading_minus = *p == '-';
	if (leading_minus) ++p;

	enum _char_types {
		ct_space = 0x01, // internal whitespace (trailing whitespace doesn't count)
		ct_digit = 0x02, // 0-9
		ct_alpha = 0x04, // a-zA-Z
		ct_ident = 0x08, // _/.
		ct_cmp   = 0x10, // <=>
		ct_sum   = 0x20, // +-
		ct_logic = 0x40, // & |
		ct_group = 0x80, // ()[]{}
		ct_money = 0x100, // $
		ct_colon = 0x200, // :
		ct_other = 0x400, // all other characters
		ct_float = 0x1000, // digits with . or e
		ct_macro = 0x2000, // $(
	};

	int set = 0;
	while (int ch = *p++) {
		if (ch >= '0' && ch <= '9') set |= ct_digit;
		else if (ch == '.') { if (set == ct_digit || (!*p || (*p >= '0' && *p <= '9'))) set |= ct_float; else set |= ct_ident; }
		else if (ch == 'e' || ch == 'E') { if ((set & ~ct_float) == ct_digit) set |= ct_float; else  set |= ct_alpha; }
		else if (ch == '-' || ch == '+') { if (set != (ct_digit|ct_float)) set |= ct_sum; }
		else if (ch >= 'a' && ch <= 'z') set |= ct_alpha;
		else if (ch >= 'A' && ch <= 'Z') set |= ct_alpha;
		else if (ch == '_' || ch == '/') set |= ct_ident;
		else if (ch >= '<' && ch <= '>') set |= ct_cmp;
		else if (ch == '!' && *p == '=') set |= ct_cmp;
		else if (ch == '$') { set |= ct_money; if (*p == '(') set |= ct_macro; }
		else if (isspace(ch)) { if ( *p && ! isspace(*p)) set |= ct_space; } // we only count internal spaces.
		else if (ch == '&' || ch == '|') set |= ct_logic;
		else if (ch >= '{' && ch <= '}') set |= ct_group;
		else if (ch == '(' || ch == ')') set |= ct_group;
		else if (ch == '[' || ch == ']') set |= ct_group;
		else if (ch == ':') set |= ct_colon;
		else set |= ct_other;
	}

	// intentify some simple cases.
	switch (set) {
		case 0:
			return CIFT_EMPTY;

		case ct_digit:
		case ct_digit|ct_float: 
			return CIFT_NUMBER;

		case ct_alpha:
			if (matches_literal_ignore_case(expr, "false") || matches_literal_ignore_case(expr, "true"))
				return CIFT_BOOL;
			if (keyword_check) {
				if (matches_literal_ignore_case(begin, "version"))
					return CIFT_VERSION; // identify bare version to insure a reasonable error message
				if (matches_literal_ignore_case(begin, "defined"))
					return CIFT_IFDEF; // identify bare defined to insure a reasonable error message
			}
		case ct_alpha|ct_digit:
		case ct_alpha|ct_digit|ct_float:
		case ct_alpha|ct_ident:
		case ct_alpha|ct_ident|ct_digit:
		case ct_alpha|ct_ident|ct_digit|ct_float:
			return CIFT_IDENTIFIER;

			// this matches version >= 8.1.2
		case ct_alpha|ct_space|ct_cmp|ct_digit:
		case ct_alpha|ct_space|ct_cmp|ct_digit|ct_float:
			if (keyword_check && matches_literal_ignore_case(begin, "version", false))
				return CIFT_VERSION;
			return CIFT_COMPLEX;

			// this matches defined identifier, defined bool & defined int
		case ct_alpha|ct_space:
		case ct_alpha|ct_space|ct_colon:
		case ct_alpha|ct_space|ct_ident:
		case ct_alpha|ct_space|ct_ident|ct_colon:
		case ct_alpha|ct_space|ct_digit:
		case ct_alpha|ct_space|ct_digit|ct_float:
			if (keyword_check && matches_literal_ignore_case(begin, "defined", false))
				return CIFT_IFDEF; // identify bare defined to insure a reasonable error message
			return CIFT_COMPLEX;
	}

	if ((set & ct_macro) && 0 == (set & ~(ct_money|ct_ident|ct_alpha|ct_digit|ct_macro|ct_colon)))
		return CIFT_MACRO;

	return CIFT_COMPLEX;
}

static bool Evaluate_config_if_bool(const char * expr, expr_character_t ec)
{
	if (ec == CIFT_NUMBER) {
		double dd = atof(expr);
		return dd < 0 || dd > 0;
	} else if (ec == CIFT_BOOL) {
		if (matches_literal_ignore_case(expr, "false")) return false;
		if (matches_literal_ignore_case(expr, "true")) return true;
	}
	return false;
}

static bool is_crufty_bool(const char * expr, bool & result)
{
	// crufty bools look like identifiers to the characterize function
	if (matches_literal_ignore_case(expr, "yes") || matches_literal_ignore_case(expr, "t")) {
		result = true;
		return true;
	}
	if (matches_literal_ignore_case(expr, "no") || matches_literal_ignore_case(expr, "f")) {
		result = false;
		return true;
	}
	return false;
}
// returns true if valid.
//
static bool Evaluate_config_if(const char * expr, bool & result, std::string & err_reason, MACRO_SET & macro_set, const char * subsys)
{
	expr_character_t ec = Characterize_config_if_expression(expr, true);
	if (ec == CIFT_NUMBER || ec == CIFT_BOOL) {
		result = Evaluate_config_if_bool(expr, ec);
		return true;
	}
	// crufty bools look like identifiers to the characterize function
	if ((ec == CIFT_IDENTIFIER) && is_crufty_bool(expr, result)) {
		return true;
	}

	if (ec == CIFT_VERSION) {
		const char * ptr = expr+7; // skip over "version"
		while (isspace(*ptr)) ++ptr;

		// extract the compparison operator and set ptr to the version field
		int op = 0; // -1 is <   0 is =   1 is >
		bool or_equal = false;
		bool negated = (*ptr == '!'); if (negated) ++ptr;
		if (*ptr >= '<' && *ptr <= '>') op = (*ptr++ - '=');
		if (*ptr == '=') or_equal = (*ptr++ == '=');
		while (isspace(*ptr)) ++ptr;
		int ver_diff = -99;

		CondorVersionInfo version;
		if (version.is_valid(ptr)) {
			ver_diff = version.compare_versions(ptr); // returns -1 for <, 0 for =, and 1 for >
		} else {
			// for (possible) future compat with classad syntax. v prefix indicates version literal.
			if (*ptr == 'v' || *ptr == 'V') ++ptr;

			int majv=0, minv=0, subv=0;
			int cfld = sscanf(ptr,"%d.%d.%d",&majv, &minv, &subv);
			if (cfld >= 2 && majv >= 6) {
				// allow subminor version to be omitted.
				if (cfld < 3) { subv = version.getSubMinorVer(); }
				CondorVersionInfo version2(majv, minv, subv);
				ver_diff = version.compare_versions(version2);
			} else {
				// doesn't look like a valid version string.
				err_reason = "the version literal is invalid";
				return false;
			}
		}
		ver_diff *= -1; // swap left and right hand side of the comparison.
		result = (ver_diff == op) || (or_equal && (ver_diff == 0));
		if (negated) result = !result;
		return true;
	}

	if (ec == CIFT_IFDEF) {
		const char * ptr = expr+7; // skip over "defined"
		while (isspace(*ptr)) ++ptr;

		if (!*ptr) {
			result = false; // empty string is same as undef
		} else {
			expr_character_t ec2 = Characterize_config_if_expression(ptr, false);
			// if it's an identifier, do macro lookup.
			if (ec2 == CIFT_IDENTIFIER) {
				const char * name = ptr;
				const char * tvalue = lookup_macro(name, subsys, macro_set);
				if (subsys && ! tvalue)
					tvalue = lookup_macro(name, NULL, macro_set);
				if ( ! tvalue && macro_set.defaults) {
					tvalue = param_default_string(name, subsys);
				}
				if ( ! tvalue && is_crufty_bool(name, result)) {
					tvalue = "true"; // any non empty value will do here.
				}

				// result is false if macro is not defined, or if defined to be ""
				result = (tvalue != NULL && tvalue[0] != 0);
			// if what we are checking for 'defined' is a bool or int, then it's defined.
			} else if (ec2 == CIFT_NUMBER || ec2 == CIFT_BOOL) {
				result = true;
			} else if (starts_with_ignore_case(ptr, "use ")) { // is it check for a metaknob definition?
				ptr += 4; // skip over "use ";
				while (isspace(*ptr)) ++ptr;
				// there are two allowed forms "if defined use <cat>", and "if defined use <cat>:<val>"
				MACRO_TABLE_PAIR * tbl = param_meta_table(ptr);
				result = false;
				if (tbl) {
					const char * pcolon = strchr(ptr, ':');
					if ( ! pcolon || !pcolon[1] || param_meta_table_string(tbl, pcolon+1))
						result = true;
				}
				if (strchr(ptr, ' ') || strchr(ptr, '\t') || strchr(ptr, '\r')) { // catch most common syntax error
					err_reason = "defined use meta argument with internal spaces will never match";
					return false;
				}
			} else {
				err_reason = "defined argument must be param name, boolean, or number";
				return false;
			}
		}
		return true;
	}

#if 1
	// TODO: convert version & defined to booleans, and then evaluate the result as a ClassAd expression
#else // this code sort of works, but isn't necessarily the way we want to go
	// the expression MAY be evaluatable by the classad library, if it is, then great
	int ival;
	ClassAd rad;
	if (rad.AssignExpr("ifcondition", expr) && rad.EvalBool("ifcondition", NULL, ival)) {
		result = (ival != 0);
		return true;
	}
#endif

	if (ec == CIFT_COMPLEX) {
		err_reason = "complex conditionals are not supported";
	} else {
		err_reason = "expression is not a conditional";
	}

	return false;
}

bool Test_config_if_expression(const char * expr, bool & result, std::string & err_reason, MACRO_SET& macro_set, const char * subsys)
{
	bool value = result;
	bool inverted = false;

	// optimize the simple case by not bothering to macro expand if there are no $ in the expression
	char * expanded = NULL; // so we know whether to free the macro expansion
	if (strstr(expr, "$")) {
		expanded = expand_macro(expr, macro_set, true, subsys);
		if ( ! expanded) return false;
		expr = expanded;
		char * ptr = expanded + strlen(expanded);
		while (ptr > expanded && isspace(ptr[-1])) *--ptr = 0;
	}

	while (isspace(*expr)) ++expr;
	if (*expr == '!') {
		inverted = true;
		++expr;
		while (isspace(*expr)) ++expr;
	}

	bool valid;
	if (expanded && !*expr) {
		// if the value expands to empty, then treat that as a boolean false
		// we do this for that if $(foo) is false when foo isn't defined.
		valid = true; value = false;
	} else {
		valid = Evaluate_config_if(expr, value, err_reason, macro_set, subsys);
	}

	if (expanded) free(expanded);
	result = inverted ? !value : value;
	return valid;
}

// a class to help keep track of if/elif/else stack while parsing config
// this implementation has a max stack depth of 63.
//
// theory of operation:
// a long integer will hold a bit map showing the current yes/no state of all ifs
// that surround the current line. begin_if, begin_elif and begin_else, set a bit
// in the state corresponding to the current nesting depth for the current yes/no value.
// Since an if block is enabled only when ALL of the nesting levels are in yes state, we can
// determine the enabled state by checking for a string of all 1 bits from the current level
// down to bit0. Note: Bit0 in the state corresponds to base level, outside of all ifs so bit0
// will always be 1 (assuming that the base state is enabled.)
// Thus if we are inside 3 nested if's, top is 0x8, and the current line is enabled if
// state ends in 0xF, and disabled if it ends in any other hex value.
// to allow for elif without nesting, the estate field contains a set bit if ANY previous if/elif
// body was 1. we use this to determine the state of else and to decided whether an
// elif should be tested or just set to 0
// TODO: fix to detect duplicate else, elsif after else. 
class ConfigIfStack {
public:
	unsigned long long state;   // the current yes/no state of all nested ifs. valid from bit0 to top
	unsigned long long estate;  // 1 bits indicate an if or elif was true for all nested if. valid from bit0 to top
	unsigned long long istate;  // 1 bits that a if has been seen, but no else yet, use to multiple else.
	unsigned long long top;     // mask for the bit in state corresponding to current nesting level. only one bit should be set
	ConfigIfStack() : state(1), estate(0), istate(0), top(1) {}
	bool enabled() { unsigned long long mask = top | (top-1); return (state&mask)==mask; }
	bool inside_if() { return top > 1; }
	bool inside_else() { return top > 1 && !(istate & top); }
	bool begin_if(bool bb) { top <<= 1; istate |= top; if (bb) { state |= top; estate |= top; } else { state &= ~top; estate &= ~top; } return top != 0; }
	bool begin_else() { 
		if (!(istate & top)) return false;
		istate &= ~top;
		if ((estate | state) & top) { state &= ~top; } else { state |= top; }
		return top > 1; 
	}
	bool begin_elif(bool bb) {
		if (!(istate & top)) return false;
		if (estate & top) { // if one of the previous if was true, then this else is false
			state &= ~top;
		} else { // if all of the previous ifs were false, then evaluate bb
			if (bb) { estate |= top; state |= top; } else { state &= ~top; }
		}
		return top > 1;
	}
	bool end_if() { istate &= ~top; top >>= 1; if (!top) { top = state = 1; istate = estate = 0; return false; } return true; }
	bool line_is_if(const char * line, std::string & errmsg, MACRO_SET& macro_set, const char * subsys);
};

bool ConfigIfStack::line_is_if(const char * line, std::string & errmsg, MACRO_SET& macro_set, const char * subsys)
{
	if (starts_with_ignore_case(line,"if") && (isspace(line[2]) || !line[2])) {
		const char * expr = line+2;
		while (isspace(*expr)) ++expr;

		bool bb = this->enabled();
		std::string err_reason;
		if (bb && ! Test_config_if_expression(expr, bb, err_reason, macro_set, subsys)) {
			formatstr(errmsg, "%s is not a valid if condition", expr);
			if (!err_reason.empty()) { errmsg += " because "; errmsg += err_reason; }
		} else if ( ! this->begin_if(bb)) {
			formatstr(errmsg, "if nesting too deep!");
		} else {
			errmsg.clear();
		}
		return true;
	}
	if (starts_with_ignore_case(line, "else") && (isspace(line[4]) || !line[4])) {
		if ( ! this->begin_else()) {
			errmsg = this->inside_else() ? "else is not allowed after else" : "else without matching if";
		} else {
			errmsg.clear();
		}
		return true;
	}
	if (starts_with_ignore_case(line, "elif") && (isspace(line[4]) || !line[4])) {
		const char * expr = line+4;
		while (isspace(*expr)) ++expr;
		std::string err_reason;

		bool bb = !(estate & top) && ((state & (top-1)) == (top-1));	// if an outer if prunes this, don't evaluate the expression.
		if (bb && ! Test_config_if_expression(expr, bb, err_reason, macro_set, subsys)) {
			formatstr(errmsg, "%s is not a valid elif condition", expr);
			if (!err_reason.empty()) { errmsg += " because "; errmsg += err_reason; }
		} else if ( ! this->begin_elif(bb)) {
			errmsg = this->inside_else() ? "elif is not allowed after else" : "elif without matching if";
		} else {
			errmsg.clear();
		}
		return true;
	}
	if (starts_with_ignore_case(line, "endif") && (isspace(line[5]) || !line[5])) {
		if ( ! this->end_if()) {
			errmsg = "endif without matching if";
		} else {
			errmsg.clear();
		}
		return true;
	}
	return false;
}

// parse a string containing one or more statements in config syntax
// and insert the resulting declarations into the given macro set.
// this code is used to parse meta-knob definitions.
//
int Parse_config_string(MACRO_SOURCE & source, int depth, const char * config, MACRO_SET& macro_set, const char * subsys)
{
	source.meta_off = -1;

	ConfigIfStack ifstack;

	StringList lines(config, "\n");
	lines.rewind();
	char * line;
	while ((line = lines.next()) != NULL) {
		++source.meta_off;
		if( line[0] == '#' || blankline(line) )
			continue;

		std::string errmsg;
		if (ifstack.line_is_if(line, errmsg, macro_set, subsys)) {
			if ( ! errmsg.empty()) {
				dprintf(D_CONFIG | D_FAILURE, "Parse_config if error: '%s' line: %s\n", errmsg.c_str(), line);
				return -1;
			} else {
				dprintf(D_CONFIG | D_VERBOSE, "config %lld,%lld,%lld line: %s\n", ifstack.top, ifstack.state, ifstack.estate, line);
			}
			continue;
		}
		if ( ! ifstack.enabled()) {
			dprintf(D_CONFIG | D_VERBOSE, "config if(%lld,%lld,%lld) ignoring: %s\n", ifstack.top, ifstack.state, ifstack.estate, line);
			continue;
		}

		const char * name = line;
		char * ptr = line;
		int op = 0;

		// detect the 'use' keyword
		bool is_meta = starts_with_ignore_case(line, "use ");
		if (is_meta) {
			ptr += 4; while (isspace(*ptr)) ++ptr;
			name = ptr; // name is now the metaknob category name rather than the keyword 'use'
		}

		// parse to the end of the name and null terminate it
		while (*ptr) {
			if (isspace(*ptr) || ISOP(*ptr)) {
				op = *ptr;  // capture the operator
				*ptr++ = 0; // null terminate the name
				break;
			}
			++ptr;
		}
		// parse to the start of the value, it's ok to not have any value
		while (*ptr) {
			if (ISOP(*ptr)) {
				if (ISOP(op)) {
					op = 0; // more than one op is not allowed, so trigger a failure
					break;
				}
				op = *ptr;
			} else if ( ! isspace(*ptr)) {
				break;
			}
			++ptr;
		}

		if ( ! *ptr && ! ISOP(op)) {
			// Here we have determined this line has no operator, or too many
			//PRAGMA_REMIND("tj: should report parse error in meta knobs here.")
			return -1;
		}

		// ptr now points to the first non-space character of the right hand side, it may point to a \0
		const char * rhs = ptr;

		// Expand the knob name - do we allow this??
		/*
		PRAGMA_REMIND("tj: allow macro expansion in knob names in meta_params?")
		char* expanded_name = expand_macro(name, macro_set);
		if (expanded_name == NULL) {
			return -1;
		}
		name = expanded_name;
		*/

		// if this is a metaknob use statement
		if (is_meta) {
			if (depth >= CONFIG_MAX_NESTING_DEPTH) {
				// looks like infinite recursion, give up and return an error instead.
				return -2;
			}
			// for recursive metaknobs, we need to use a temp copy of the source info
			// to avoid loosing the source id/offset info.
			MACRO_SOURCE source2 = source;
			int retval = read_meta_config(source2, depth+1, name, rhs, macro_set, subsys);
			if (retval < 0) {
				return retval;
			}
		} else {

			/* Check that "name" is a legal identifier : only
			   alphanumeric characters and _ allowed*/
			if ( ! is_valid_param_name(name) ) {
				return -1;
			}

			/* expand self references only */
			//PRAGMA_REMIND("TJ: this handles only trivial self-refs, needs rethink.")
			char * value = expand_self_macro(rhs, macro_set, name, subsys);
			if (value == NULL) {
				return -1;
			}

			insert(name, value, macro_set, source);
			FREE(value);
		}

		// FREE(expanded_name);
	}
	source.meta_off = -2;
	return 0;
}

int
Read_config(const char* config_source,
			int depth, // a simple recursion detector
			MACRO_SET& macro_set,
			int expand_flag,
			bool check_runtime_security,
			const char * subsys,
			std::string & config_errmsg)
{
	FILE*	conf_fp = NULL;
	char*	name = NULL;
	char*	value = NULL;
	char*	rhs = NULL;
	char*	ptr = NULL;
	char	op;
	int		retval = 0;
	bool	is_pipe_cmd = false;
	bool	firstRead = true;
	const int gl_opt_old = 0;
	const int gl_opt_new = CONFIG_GETLINE_OPT_COMMENT_DOESNT_CONTINUE | CONFIG_GETLINE_OPT_CONTINUE_MAY_BE_COMMENTED_OUT;
	int gl_opt = (macro_set.options & CONFIG_OPT_OLD_COM_IN_CONT) ? gl_opt_old : gl_opt_new;
	bool gl_opt_smart = (macro_set.options & CONFIG_OPT_SMART_COM_IN_CONT) ? true : false;
	int opt_meta_colon = (macro_set.options & CONFIG_OPT_COLON_IS_META_ONLY) ? 1 : 0;
	ConfigIfStack ifstack;

	if (subsys && ! *subsys) subsys = NULL;

	ConfigLineNo = 0;
	config_errmsg.clear();

	// initialize a MACRO_SOURCE for this file that we will use
	// in subsequent macro insert calls.
	MACRO_SOURCE FileMacro;
	insert_source(config_source, macro_set, FileMacro);

	// check for exceeded nesting depth.
	if (depth >= CONFIG_MAX_NESTING_DEPTH) {
		config_errmsg = "includes nested too deep";
		return -2; // indicate that nesting depth has been exceeded.
	}

	// Determine if the config file name specifies a file to open, or a
	// pipe to suck on. Process each accordingly
	if ( is_piped_command(config_source) ) {
		is_pipe_cmd = true;
		if ( is_valid_command(config_source) ) {
			// try to run the cmd specified before the '|' symbol, and
			// get the configuration from it's output.
			char *cmdToExecute = strdup( config_source );
			cmdToExecute[strlen(cmdToExecute)-1] = '\0';

			ArgList argList;
			MyString args_errors;
			if(!argList.AppendArgsV1RawOrV2Quoted(cmdToExecute, &args_errors)) {
				formatstr(config_errmsg, "Can't append args, %s", args_errors.Value());
				free( cmdToExecute );
				return -1;
			}
			conf_fp = my_popen(argList, "r", FALSE);
			if( conf_fp == NULL ) {
				config_errmsg = "not a valid command";
				free( cmdToExecute );
				return -1;
			}
			free( cmdToExecute );
		} else {
			config_errmsg = "not a valid command, | must be at the end\n";
			return  -1;
		}
	} else {
		is_pipe_cmd = false;
		conf_fp = safe_fopen_wrapper_follow(config_source, "r");
		if( conf_fp == NULL ) {
			config_errmsg = "can't open file";
			return  -1;
		}
	}

	if( check_runtime_security ) {
#ifndef WIN32
			// unfortunately, none of this works on windoze... (yet)
		if ( is_pipe_cmd ) {
			fprintf( stderr, "Configuration Error File <%s>: runtime config "
					 "not allowed to come from a pipe command\n",
					 config_source );
			retval = -1;
			goto cleanup;
		}
		int fd = fileno(conf_fp);
		struct stat statbuf;
		uid_t f_uid;
		int rval = fstat( fd, &statbuf );
		if( rval < 0 ) {
			fprintf( stderr, "Configuration Error File <%s>, fstat() failed: %s (errno: %d)\n",
					 config_source, strerror(errno), errno );
			retval = -1;
			goto cleanup;
		}
		f_uid = statbuf.st_uid;
		if( can_switch_ids() ) {
				// if we can switch, the file *must* be owned by root
			if( f_uid != 0 ) {
				fprintf( stderr, "Configuration Error File <%s>, "
						 "running as root yet runtime config file owned "
						 "by uid %d, not 0!\n", config_source, (int)f_uid );
				retval = -1;
				goto cleanup;
			}
		} else {
				// if we can't switch, at least ensure we own the file
			if( f_uid != get_my_uid() ) {
				fprintf( stderr, "Configuration Error File <%s>, "
						 "running as uid %d yet runtime config file owned "
						 "by uid %d!\n", config_source, (int)get_my_uid(),
						 (int)f_uid );
				retval = -1;
				goto cleanup;
			}
		}
#endif /* ! WIN32 */
	} // if( check_runtime_security )

	while(true) {
		name = getline_implementation(conf_fp, 128, gl_opt);
		// If the file is empty the first time through, warn the user.
		if (name == NULL) {
			if (firstRead) {
				dprintf(D_FULLDEBUG, "WARNING: Config source is empty: %s\n",
						config_source);
			}
			break;
		}
		firstRead = false;
		
		/* Skip over comments */
		if( *name == '#' || blankline(name) ) {
			if (gl_opt_smart) {
				if (MATCH == strcasecmp(name, "#opt:oldcomment")) {
					gl_opt = gl_opt_old;
				} else if (MATCH == strcasecmp(name, "#opt:newcomment")) {
					gl_opt = gl_opt_new;
				} else if (MATCH == strcasecmp(name, "#opt:strict")) {
					opt_meta_colon = 2;
				}
			}
			continue;
		}

		// to allow if/else constructs to be used in files that are parsed by pre 8.1.5 HTCondor
		// we ignore a ':' at the beginning of the line of an if body.
		// before 8.1.5, a line that began with ':' would be effectively ignored*.
		// now we ignore the ':' but not the rest of the line, so a ':' can be used to make lines
		// invisible to the old config parser, but not the current one.  To help avoid regression
		// we only ignore ':' at the start of lines that constitute an if body
		if (*name == ':') {
			if (ifstack.inside_if()
				|| (name[1] == 'i' && name[2] == 'f' && (isspace(name[3]) || !name[3]))) {
				++name;
			}
		}

		// if the line is an if/elif/else/endif handle it here, updating the ifstack as needed.
		std::string errmsg;
		if (ifstack.line_is_if(name, errmsg, macro_set, subsys)) {
			if ( ! errmsg.empty()) {
				dprintf(D_CONFIG | D_FAILURE, "Parse_config if error: '%s' line: %s\n", errmsg.c_str(), name);
				config_errmsg = errmsg;
				retval = -1;
				goto cleanup;
			} else {
				dprintf(D_CONFIG | D_VERBOSE, "config %s:%lld,%lld,%lld line: %s\n", name, ifstack.top, ifstack.state, ifstack.estate, name);
			}
			continue;
		}
		// if the line is inside the body of an if/elif/else that is false, ignore it.
		if ( ! ifstack.enabled()) {
			dprintf(D_CONFIG | D_VERBOSE, "config if(%lld,%lld,%lld) ignoring: %s\n", ifstack.top, ifstack.state, ifstack.estate, name);
			continue;
		}

		op = 0;
		ptr = name;

		// Assumption is that the line starts with a non whitespace
		// character
		// Example :
		// OP_SYS=SunOS
		while( *ptr ) {
			if( isspace(*ptr) || ISOP(*ptr) ) {
			  /* *ptr is now whitespace or '=' or ':' */
			  break;
			} else {
			  ptr++;
			}
		}

		if( !*ptr ) {
				// Here we have determined this line has no operator
			if ( name && name[0] == '[' ) {
				// Treat a line w/o an operator that begins w/ a square bracket
				// as a comment so a config file can look like
				// a Win32 .ini file for MS Installer purposes.		
				continue;
			} else {
				// No operator and no square bracket... bail.
				retval = -1;
				goto cleanup;
			}
		}

		char * pop = ptr; // keep track of where we see the operator
		if( ISOP(*ptr) ) {
			op = *ptr;
			//op is now '=' in the above eg
			*ptr++ = '\0';
			// name is now 'OpSys' in the above eg
		} else {
			*ptr++ = '\0';
			while( *ptr && !ISOP(*ptr) ) {
				ptr++;
			}
			if( !*ptr ) {
				retval = -1;
				goto cleanup;
			}
			pop = ptr;
			op = *ptr++;
		}

		/* Skip to next non-space character */
		while( *ptr && isspace(*ptr) ) {
			ptr++;
		}

		rhs = ptr;
		// rhs is now 'SunOS' in the above eg

		// in order to prevent "use" and "include" from looking like valid config in 8.0 and earlier
		// they can optionally be preceeded with an @ that doesn't change the  meaning, but will
		// generate a syntax error in 8.0 and earlier.
		bool has_at = (*name == '@');
		int is_include = (op == ':' && MATCH == strcasecmp(name + has_at, "include"));
		bool is_meta = (op == ':' && MATCH == strcasecmp(name + has_at, "use"));

		// if the name is 'use' then this is a metaknob, so the actual name
		// is the word after 'use'. so we want to find that word and
		// remove trailing spaces from it.
		if (is_meta) {
			// set name to point to the word after use (if there is one)
			if (name+has_at+4 < pop) {
				name += has_at+4;
				while (isspace(*name) && name < pop) { ++name; }
				char * p = pop-1;
				while (isspace(*p) && p > name) { *p-- = 0; }
			} else {
				name += has_at+3; // this will point at the null terminator of 'use'
			}
		} else if (is_include) {
			// check for keywords after "include" and before the :
			// these keywords will modifity the behavior of include
			if (name+has_at+8 < pop) {
				name += has_at+8;
				while (isspace(*name)) ++name; // skip whitespace
				*pop = 0; // guarantee null term for include keyword.
				char * p = pop-1;
				while (isspace(*p) && p > name) { *p-- = 0; }
				if (*name) {
					if (MATCH == strcasecmp(name, "output")) is_include = 2; // a value of 2 indicates a command rather than a filename.
					else if (MATCH == strcasecmp(name, "command")) is_include = 2; // a value of 2 indicates a command rather than a filename.
					else {
						config_errmsg = "unexpected keyword '";
						config_errmsg += name;
						config_errmsg += "' after include";
						return -1;
					}
				}
			}
			// set name to be the filename or command line.
			name = pop+1;
			while (isspace(*name)) ++name; // skip whitespace before the filename
		} else if (op == ':') {
		#ifdef WARN_COLON_FOR_PARAM_ASSIGN
			if (opt_meta_colon < 2) { op = '='; } // change the op to = so that we don't "goto cleanup" below

			// backward compat hack. the old config file used : syntax for RunBenchmarks,
			// so grandfather this in. tread error as warning, tread warning as ignore.
			if (MATCH == strcasecmp(name, "RunBenchmarks")) { op = '='; if (opt_meta_colon < 2) opt_meta_colon = 0; }

			if (opt_meta_colon) {
				fprintf( stderr, "Configuration %s \"%s\", Line %d: obsolete use of ':' for parameter assignment at %s : %s\n",
						op == ':' ? "Error" : "Warning",
						config_source, ConfigLineNo,
						name, rhs
						);
				if (op == ':') {
					retval = -1;
					goto cleanup;
				}
			}
		#endif // def WARN_COLON_FOR_PARAM_ASSIGN
		}

		/* Expand references to other parameters */
		bool use_default_param_table = (macro_set.options & CONFIG_OPT_DEFAULTS_ARE_PARAM_INFO) != 0;
		name = expand_macro(name, macro_set, use_default_param_table, subsys);
		if( name == NULL ) {
			retval = -1;
			goto cleanup;
		}

		// if this is a metaknob
		if (is_meta) {
			FileMacro.line = ConfigLineNo;
			retval = read_meta_config(FileMacro, depth+1, name, rhs, macro_set, subsys);
			if (retval < 0) {
				fprintf( stderr,
							"Configuration Error \"%s\", Line %d: at use %s:%s\n",
							config_source, ConfigLineNo, name, rhs );
				goto cleanup;
			}
		} else if (is_include) {
			FileMacro.line = ConfigLineNo; // save current line number around the recursive call
			if ((is_include > 1) && !is_piped_command(name)) {
				std::string cmd(name); cmd += " |";
				if (use_default_param_table) local_config_sources.append(cmd.c_str());
				retval = Read_config(cmd.c_str(), depth+1, macro_set, expand_flag, check_runtime_security, subsys, config_errmsg);
			} else {
				if (use_default_param_table) local_config_sources.append(name);
				retval = Read_config(name, depth+1, macro_set, expand_flag, check_runtime_security, subsys, config_errmsg);
			}
			int last_line = ConfigLineNo; // get the exit line from the recursive call
			ConfigLineNo = FileMacro.line; // restore the global line number.

			if (retval < 0) {
				fprintf( stderr,
							"Configuration Error \"%s\", Line %d, Include Depth %d: %s\n",
							name, last_line, depth+1, config_errmsg.c_str());
				config_errmsg.clear();
				goto cleanup;
			}
		} else {

			/* Check that "name" is a legal identifier : only
			   alphanumeric characters and _ allowed*/
			if( !is_valid_param_name(name) ) {
				fprintf( stderr,
						 "Configuration Error \"%s\", Line %d: Illegal Identifier: <%s>\n",
						 config_source, ConfigLineNo, name );
				retval = -1;
				goto cleanup;
			}

			if( expand_flag == EXPAND_IMMEDIATE ) {
				value = expand_macro(rhs, macro_set);
				if( value == NULL ) {
					retval = -1;
					goto cleanup;
				}
			} else  {
				/* expand self references only */
				//PRAGMA_REMIND("TJ: this handles only trivial self-refs, needs rethink.")
				value = expand_self_macro(rhs, macro_set, name, subsys);
				if( value == NULL ) {
					retval = -1;
					goto cleanup;
				}
			}

			if( op == ':' || op == '=' ) {
					/*
					  Yee haw!!! We can treat "expressions" and "macros"
					  the same way: just insert them into the config hash
					  table.  Everything now behaves like macros used to
					  Derek Wright <wright@cs.wisc.edu> 4/11/00
					*/
				FileMacro.line = ConfigLineNo;
				insert(name, value, macro_set, FileMacro);
			} else {
				fprintf( stderr,
					"Configuration Error \"%s\", Line %d: Syntax Error, missing : or =\n",
					config_source, ConfigLineNo );
				retval = -1;
				goto cleanup;
			}
		}

		FREE( name );
		name = NULL;
		FREE( value );
		value = NULL;
	}

	if (ifstack.inside_if()) {
		fprintf(stderr,
				"Configuration Error \"%s\", Line %d: \n",
				config_source, ConfigLineNo );
		config_errmsg = "endif(s) not found before end-of-file";
		retval = -1;
		goto cleanup;
	}

 cleanup:
	if ( conf_fp ) {
		if ( is_pipe_cmd ) {
			int exit_code = my_pclose( conf_fp );
			if ( retval == 0 && exit_code != 0 ) {
				fprintf( stderr, "Configuration Error \"%s\": "
						 "command terminated with exit code %d\n",
						 config_source, exit_code );
				retval = -1;
			}
		} else {
			fclose( conf_fp );
		}
	}
	if(name) {
		FREE( name );
	}
	if(value) {
		FREE( value );
	}
	return retval;
}


/*
** Just compute a hash value for the given string such that
** 0 <= value < size 
*/
#if 0 // this is not currently used.
int
condor_hash( register const char *string, register int size )
{
	register unsigned int		answer;

	answer = 1;

	for( ; *string; string++ ) {
		answer <<= 1;
		answer += (int)*string;
	}
	answer >>= 1;	/* Make sure not negative */
	answer %= size;
	return answer;
}
#endif

// case-insensitive hash, usable when the character set of name
// is restricted to A-Za-Z0-9 and symbols except []{}\|^~
#if 0 // this is not currently used
int
condor_hash_symbol_name(const char *name, int size)
{
	register const char * psz = name;
	unsigned int answer = 1;

	// in order to make this hash interoperate with strlwr/condor_hash
	// _ is the only legal character for symbol name for which |= 0x20
	// is not benign.  the test for _ is needed only to make this hash
	// behave identically to strlwr/condor_hash. 
	for( ; *psz; ++psz) {
		answer <<= 1;
		int ch = (int)*psz;
		if (ch != '_') ch |= 0x20;
		answer += ch;
	}
	answer >>= 1;	/* Make sure not negative */
	answer %= size;
	return answer;
}
#endif

/*
** Insert the parameter name and value into the given hash table.
*/
//PRAGMA_REMIND("TJ: insert bug - self refs to default refs never expanded")

extern "C++" MACRO_ITEM* find_macro_item (const char *name, MACRO_SET& set)
{
	int cElms = set.size;
	MACRO_ITEM* aTable = set.table;

	if (set.sorted < set.size) {
		// use a brute force search on the unsorted parts of the table.
		for (int ii = set.sorted; ii < cElms; ++ii) {
			if (MATCH == strcasecmp(aTable[ii].key, name))
				return &aTable[ii];
		}
		cElms = set.sorted;
	}

	// table is sorted, so we can binary search it.
	if (cElms <= 0)
		return NULL;

	int ixLower = 0;
	int ixUpper = cElms-1;
	for (;;) {
		if (ixLower > ixUpper)
			return NULL; // return null for "not found"

		int ix = (ixLower + ixUpper) / 2;
		int iMatch = strcasecmp(aTable[ix].key, name);
		if (iMatch < 0)
			ixLower = ix+1;
		else if (iMatch > 0)
			ixUpper = ix-1;
		else
			return &aTable[ix];
	}
}

void insert_source(const char * filename, MACRO_SET & set, MACRO_SOURCE & source)
{
	if ( ! set.sources.size()) {
		set.sources.push_back("<Detected>");
		set.sources.push_back("<Default>");
		set.sources.push_back("<Environment>");
		set.sources.push_back("<Over>");
	}
	source.line = 0;
	source.inside = false;
	source.id = (int)set.sources.size();
	set.sources.push_back(set.apool.insert(filename));
}

static bool same_param_value(
	const char * a,
	const char * b,
#ifdef WIN32
	bool is_path)
#else
	bool /*is_path*/) // to get rid of stupid g++ warning
#endif
{
	if ( ! a || ! b)
		return (a == b);

	// exact matches are always matches.
	if (MATCH == strcmp(a, b))
		return true;

	// some special cases tolerate case insensitive matches.
	if (MATCH == strcasecmp(a, b)) {
		if (MATCH == strcasecmp(a, "true") || MATCH == strcasecmp(a, "false"))
			return true;
		#ifdef WIN32
		// paths are case-insensitive on windows.
		if (is_path) { return true; }
		#endif
	}
#ifdef WIN32
	if (is_path) {
		while (*a && *b) {
			if (toupper(*a) != toupper(*b) && ((*a != '/' && *a != '\\') || (*b != '/' && *b != '\\')))
				return false;
			++a, ++b;
		}
		return true;
	}
#endif

	return false;
}

void insert(const char *name, const char *value, MACRO_SET & set, const MACRO_SOURCE & source)
{
	// if already in the macro-set, we need to expand self-references and replace
	MACRO_ITEM * pitem = find_macro_item(name, set);
	if (pitem) {
		char * tvalue = expand_self_macro(value, set, name, NULL);
		if (MATCH != strcmp(tvalue, pitem->raw_value)) {
			pitem->raw_value = set.apool.insert(tvalue);
		}
		if (set.metat) {
			MACRO_META * pmeta = &set.metat[pitem - set.table];
			pmeta->source_id = source.id;
			pmeta->source_line = source.line;
			pmeta->source_meta_id = source.meta_id;
			pmeta->source_meta_off = source.meta_off;
			pmeta->inside = (source.inside != false);
			pmeta->param_table = false;
			// use the name here in case we have a compound name, i.e "master.value"
			int param_id = param_default_get_id(name);
			const char * def_value = param_default_rawval_by_id(param_id);
			pmeta->matches_default = (def_value == pitem->raw_value);
			if ( ! pmeta->matches_default) {
				bool is_path = param_default_ispath_by_id(pmeta->param_id);
				pmeta->matches_default = same_param_value(def_value, pitem->raw_value, is_path);
			}
		}
		if (tvalue) free(tvalue);
		return;
	}

	if (set.size+1 >= set.allocation_size) {
		int cAlloc = set.allocation_size*2;
		if ( ! cAlloc) cAlloc = 32;
		set.allocation_size = cAlloc;
		MACRO_ITEM * ptab = new MACRO_ITEM[cAlloc];
		if (set.table) {
			// transfer existing key/value pairs old allocation to new one.
			if (set.size > 0) {
				memcpy(ptab, set.table, sizeof(set.table[0]) * set.size);
				memset(set.table, 0, sizeof(set.table[0]) * set.size);
			}
			delete [] set.table;
		}
		set.table = ptab;
		if (set.metat != NULL || (set.options & CONFIG_OPT_WANT_META) != 0) {
			MACRO_META * pmet = new MACRO_META[cAlloc];
			if (set.metat) {
				// transfer existing metadata from old allocation to new one.
				if (set.size > 0) {
					memcpy(pmet, set.metat, sizeof(set.metat[0]) * set.size);
					memset(set.metat, 0, sizeof(set.metat[0]) * set.size);
				}
				delete [] set.metat;
			}
			set.metat = pmet;
		}
	}

	int matches_default = false;
	int param_id = param_default_get_id(name);
	const char * def_value = param_default_rawval_by_id(param_id);
	bool is_path = param_default_ispath_by_id(param_id);
	if (same_param_value(def_value, value, is_path)) {
		matches_default = true; // flag value as matching the default.
		if ( ! (set.options & CONFIG_OPT_KEEP_DEFAULTS))
			return; // don't put default-matching values into the macro set
	}

	// for now just append the item.
	// the set after this will no longer be sorted.
	int ixItem = set.size++;

	pitem = &set.table[ixItem];
	const char * def_name = param_default_name_by_id(param_id);
	if (def_name && MATCH == strcmp(name, def_name)) {
		pitem->key = def_name;
	} else {
		pitem->key = set.apool.insert(name);
	}
	if (matches_default) {
		pitem->raw_value = def_value;
	} else {
		pitem->raw_value = set.apool.insert(value);
	}
	if (set.metat) {
		MACRO_META * pmeta = &set.metat[ixItem];
		pmeta->flags = 0; // clear all flags.
		pmeta->matches_default = matches_default;
		pmeta->inside = source.inside;
		pmeta->source_id = source.id;
		pmeta->source_line = source.line;
		pmeta->source_meta_id = source.meta_id;
		pmeta->source_meta_off = source.meta_off;
		pmeta->use_count = 0;
		pmeta->ref_count = 0;
		pmeta->index = ixItem;
		pmeta->param_id = param_id;
	}
}

/*
** Sets the given macro's state to used
*/

int increment_macro_use_count (const char *name, MACRO_SET & set)
{
	MACRO_ITEM* pitem = find_macro_item(name, set);
	if (pitem && set.metat) {
		MACRO_META* pmeta = &set.metat[pitem - set.table];
		return ++(pmeta->use_count);
	}
	return -1;
}

void clear_macro_use_count (const char *name, MACRO_SET & set)
{
	MACRO_ITEM* pitem = find_macro_item(name, set);
	if (pitem && set.metat) {
		MACRO_META* pmeta = &set.metat[pitem - set.table];
		pmeta->ref_count = pmeta->use_count = 0;
	}
}

int get_macro_use_count (const char *name, MACRO_SET & set)
{
	MACRO_ITEM* pitem = find_macro_item(name, set);
	if (pitem && set.metat) {
		MACRO_META* pmeta = &set.metat[pitem - set.table];
		return pmeta->use_count;
	}
	return -1;
}

int get_macro_ref_count (const char *name, MACRO_SET & set)
{
	MACRO_ITEM* pitem = find_macro_item(name, set);
	if (pitem && set.metat) {
		MACRO_META* pmeta = &set.metat[pitem - set.table];
		return pmeta->ref_count;
	}
	return -1;
}

/*
** Read one line and any continuation lines that go with it.  Lines ending
** with <white space><backslash> are continued onto the next line.
** Lines can be of any lengh.  We pass back a pointer to a buffer; do _not_
** free this memory.  It will get freed the next time getline() is called (this
** function used to contain a fixed-size static buffer).
*/
char *
getline( FILE *fp )
{
	return getline_implementation(fp,_POSIX_ARG_MAX, 0);
}

static char *
getline_implementation( FILE *fp, int requested_bufsize, int options )
{
	static char	*buf = NULL;
	static unsigned int buflen = 0;
	char	*end_ptr;	// Pointer to read into next read
	char    *line_ptr;	// Pointer to beginning of current line from input
	int      in_comment = FALSE;
	//int      in_continuation = FALSE;

	if( feof(fp) ) {
			// We're at the end of the file, clean up our buffers and
			// return NULL.  
		if ( buf ) {
			free(buf);
			buf = NULL;
			buflen = 0;
		}
		return NULL;
	}

	if ( buflen < (unsigned int)requested_bufsize ) {
		if ( buf ) free(buf);
		buf = (char *)malloc(requested_bufsize);
		buflen = requested_bufsize;
	}
	ASSERT( buf != NULL );
	buf[0] = '\0';
	end_ptr = buf;
	line_ptr = buf;

	// Loop 'til we're done reading a whole line, including continutations
	for(;;) {
		int		len = buflen - (end_ptr - buf);
		if( len <= 5 ) {
			// we need a larger buffer -- grow buffer by 4kbytes
			char *newbuf = (char *)realloc(buf, 4096 + buflen);
			if ( newbuf ) {
				end_ptr = (end_ptr - buf) + newbuf;
				line_ptr = (line_ptr - buf) + newbuf;
				buf = newbuf;	// note: realloc() freed our old buf if needed
				buflen += 4096;
				len += 4096;
			} else {
				// malloc returned NULL, we're out of memory
				EXCEPT( "Out of memory - config file line too long" );
			}
		}

		if( fgets(end_ptr,len,fp) == NULL ) {
			if( buf[0] == '\0' ) {
				return NULL;
			} else {
				return buf;
			}
		}

		// See if fgets read an entire line, or simply ran out of buffer space
		if ( *end_ptr == '\0' ) {
			continue;
		}

		int cch = strlen(end_ptr);
		if (end_ptr[cch-1] != '\n') {
			// if we made it here, fgets() ran out of buffer space.
			// move our read_ptr pointer forward so we concatenate the
			// rest on after we realloc our buffer above.
			end_ptr += cch;
			continue;	// since we are not finished reading this line
		}

		ConfigLineNo++;
		end_ptr += cch;

			// Instead of calling ltrim() below, we do it inline,
			// taking advantage of end_ptr to avoid overhead.

			// trim whitespace from the end
		while( end_ptr>line_ptr && isspace( end_ptr[-1] ) ) {
			*(--end_ptr) = '\0';
		}	

			// trim whitespace from the beginning of the line
		char	*ptr = line_ptr;
		while( isspace(*ptr) ) {
			ptr++;
		}
		// special interactions between \ and #.
		// if we have a # AFTER a continuation then we may want to treat everthing between the # and \n
		// as if it were whitespace. conversely, if the entire line begins with # we may want to ignore
		// \ at the end of that line.
		in_comment = (*ptr == '#');
		if (in_comment) {
			if (line_ptr == buf) {
				// we are the the start of the whole line.
			} else if (options & CONFIG_GETLINE_OPT_CONTINUE_MAY_BE_COMMENTED_OUT) {
				// pretend this is whitespace to the end of the line
				ptr = end_ptr-1;
				in_comment = false;
			}
		}
		if( ptr != line_ptr ) {
			(void)memmove( line_ptr, ptr, end_ptr-ptr+1 );
			end_ptr = (end_ptr - ptr) + line_ptr;
		}

		if( end_ptr > buf && end_ptr[-1] == '\\' ) {
			/* Ok read the continuation and concatenate it on */
			*(--end_ptr) = '\0';

			// special interactions between \ and #.
			// if we have a \ at the end of a line that begins with #
			// we want to pretend that the \ isn't there and NOT continue
			// we do this on the theory that a comment that has continuation
			// is likely to be an error.
			if ( ! in_comment || ! (options & CONFIG_GETLINE_OPT_COMMENT_DOESNT_CONTINUE)) {
				line_ptr = end_ptr;
				continue;
			}
		}
		return buf;
	}
}

} // end of extern "C"

/* 
** Utility function to get an integer from a string.
** Returns: -1 if input is NULL, -2 if input is invalid, 0 for success
*/
static int
string_to_long( const char *s, long *valuep )
{
	// Verify that we have a valid pointer
	if ( !s ) {
		return -1;
	}

	// Call strtol(), verify that it liked the input
	char	*endp;
	long	value = strtol( s, &endp, 10 );
	if ( (const char *)endp == s ) {
		return -2;
	}

	// Done, get out of here
	*valuep = value;
	return 0;
}

/*
** Expand parameter references of the form "left$(middle)right".  This
** is deceptively simple, but does handle multiple and or nested references.
** Also expand references of the form "left$ENV(middle)right",
** replacing $ENV(middle) with getenv(middle).
** Also expand references of the form "left$RANDOM_CHOICE(middle)right".
*/
extern "C" char *
expand_macro(const char *value,
			 MACRO_SET& macro_set,
			 bool use_default_param_table,
			 const char *subsys,
			 int use)
{
	char *tmp = strdup( value );
	char *left, *name, *right;
	const char *tvalue;
	char *rval;

	bool all_done = false;
	while( !all_done ) {		// loop until all done expanding
		all_done = true;

		if (find_special_config_macro("$ENV",true,tmp, &left, &name, &right)) 
		{
			all_done = false;
			tvalue = getenv(name);
			if( tvalue == NULL ) {
				//EXCEPT("Can't find %s in environment!",name);
				tvalue = "UNDEFINED";		
			}

			rval = (char *)MALLOC( (unsigned)(strlen(left) + strlen(tvalue) + strlen(right) + 1));
			ASSERT(rval);

			(void)sprintf( rval, "%s%s%s", left, tvalue, right );
			FREE( tmp );
			tmp = rval;
		}

		if (find_special_config_macro("$RANDOM_CHOICE",false,tmp, &left, &name, &right))
		{
			all_done = false;
			StringList entries(name,",");
			int num_entries = entries.number();
			tvalue = NULL;
			if ( num_entries > 0 ) {
				int rand_entry = (get_random_int() % num_entries) + 1;
				int i = 0;
				entries.rewind();
				while ( (i < rand_entry) && (tvalue=entries.next()) ) {
					i++;
				}
			}
			if( tvalue == NULL ) {
				EXCEPT("$RANDOM_CHOICE() macro in config file empty!" );
			}

			rval = (char *)MALLOC( (unsigned)(strlen(left) + strlen(tvalue) +
											  strlen(right) + 1));
			(void)sprintf( rval, "%s%s%s", left, tvalue, right );
			FREE( tmp );
			tmp = rval;
		}

		if (find_special_config_macro("$RANDOM_INTEGER",false,tmp, &left, &name, &right))
		{
			all_done = false;
			StringList entries(name, ",");

			entries.rewind();
			const char *tmp2;

			tmp2 = entries.next();
			long	min_value=0;
			if ( string_to_long( tmp2, &min_value ) < 0 ) {
				EXCEPT( "$RANDOM_INTEGER() config macro: invalid min!" );
			}

			tmp2 = entries.next();
			long	max_value=0;
			if ( string_to_long( tmp2, &max_value ) < 0 ) {
				EXCEPT( "$RANDOM_INTEGER() config macro: invalid max!" );
			}

			tmp2 = entries.next();
			long	step = 1;
			if ( string_to_long( tmp2, &step ) < -1 ) {
				EXCEPT( "$RANDOM_INTEGER() config macro: invalid step!" );
			}

			if ( step < 1 ) {
				EXCEPT( "$RANDOM_INTEGER() config macro: invalid step!" );
			}
			if ( min_value > max_value ) {
				EXCEPT( "$RANDOM_INTEGER() config macro: min > max!" );
			}

			// Generate the random value
			long	range = step + max_value - min_value;
			long 	num = range / step;
			long	random_value =
				min_value + (get_random_int() % num) * step;

			// And, convert it to a string
			char	buf[128];
			snprintf( buf, sizeof(buf)-1, "%ld", random_value );
			buf[sizeof(buf)-1] = '\0';
			rval = (char *)MALLOC( (unsigned)(strlen(left) + strlen(buf) +
											  strlen(right) + 1));
			ASSERT( rval != NULL );
			(void)sprintf( rval, "%s%s%s", left, buf, right );
			FREE( tmp );
			tmp = rval;
		}

		if (find_config_macro(tmp, &left, &name, &right, NULL)) {
			all_done = false;
		   #ifdef COLON_DEFAULT_FOR_MACRO_EXPAND
			char * pcolon = strchr(name, ':');
			if (pcolon) { *pcolon++ = 0; }
		   #endif
			tvalue = lookup_macro(name, subsys, macro_set, use);
			if (subsys && ! tvalue)
				tvalue = lookup_macro(name, NULL, macro_set, use);

				// Note that if 'name' has been explicitly set to nothing,
				// tvalue will _not_ be NULL so we will not call
				// param_default_string().  See gittrack #1302
			if (use_default_param_table && tvalue == NULL) {
				tvalue = param_default_string(name, subsys);
				if (use) { param_default_set_use(name, use, macro_set); }
			}
		   #ifdef COLON_DEFAULT_FOR_MACRO_EXPAND
			if (pcolon && ( ! tvalue || ! tvalue[0])) {
				tvalue = pcolon;
			}
		   #endif
			if( tvalue == NULL ) {
				tvalue = "";
			}

			rval = (char *)MALLOC( (unsigned)(strlen(left) + strlen(tvalue) +
											  strlen(right) + 1));
			ASSERT( rval != NULL );
			(void)sprintf( rval, "%s%s%s", left, tvalue, right );
			FREE( tmp );
			tmp = rval;
		}
	}

	// Now, deal with the special $(DOLLAR) macro.
	while( find_config_macro(tmp, &left, &name, &right, DOLLAR_ID) ) {
		rval = (char *)MALLOC( (unsigned)(strlen(left) + 1 +
										  strlen(right) + 1));
		ASSERT( rval != NULL );
		(void)sprintf( rval, "%s$%s", left, right );
		FREE( tmp );
		tmp = rval;
	}

	return( tmp );
}


bool hash_iter_done(HASHITER& it) {
	// the first time this is called, so some setup
	if (it.ix == 0 && it.id == 0) {
		if ( ! it.set.defaults || ! it.set.defaults->table || ! it.set.defaults->size) {
			it.opts |= HASHITER_NO_DEFAULTS;
		} else if ( ! (it.opts & HASHITER_NO_DEFAULTS)) {
			// decide whether the first item is in the defaults table or not.
			const char * pix_key = it.set.table[it.ix].key;
			const char * pid_key = it.set.defaults->table[it.id].key;
			int cmp = strcasecmp(pix_key, pid_key);
			it.is_def = (cmp > 0);
			if ( ! cmp && ! (it.opts & HASHITER_SHOW_DUPS)) {
				++it.id;
			}
		}
	}
	if (it.ix >= it.set.size && ((it.opts & HASHITER_NO_DEFAULTS) != 0 || (it.id >= it.set.defaults->size)))
		return true;
	return false;
}
bool hash_iter_next(HASHITER& it) {
	if (hash_iter_done(it)) return false;
	if (it.is_def) {
		++it.id;
	} else {
		++it.ix;
	}

	if (it.opts & HASHITER_NO_DEFAULTS) {
		it.is_def = false;
		return (it.ix < it.set.size);
	}

	if (it.ix < it.set.size) {
		if (it.id < it.set.defaults->size) {
			const char * pix_key = it.set.table[it.ix].key;
			const char * pid_key = it.set.defaults->table[it.id].key;
			int cmp = strcasecmp(pix_key, pid_key);
			it.is_def = (cmp > 0);
			if ( ! cmp && ! (it.opts & HASHITER_SHOW_DUPS)) {
				++it.id;
			}
		} else {
			it.is_def = false;
		}
		return true;
	}
	it.is_def = (it.id < it.set.defaults->size);
	return it.is_def;
}
const char * hash_iter_key(HASHITER& it) {
	if (hash_iter_done(it)) return NULL;
	if (it.is_def) {
		return it.pdef ? it.pdef->key : it.set.defaults->table[it.id].key;
	}
	return it.set.table[it.ix].key;
}
const char * hash_iter_value(HASHITER& it) {
	if (hash_iter_done(it)) return NULL;
	if (it.is_def) {
		const condor_params::nodef_value * pdef = it.pdef ? it.pdef->def : it.set.defaults->table[it.id].def;
		if ( ! pdef)
			return NULL;
		return pdef->psz;
	}
	return it.set.table[it.ix].raw_value;
}
MACRO_META * hash_iter_meta(HASHITER& it) {
	if (hash_iter_done(it)) return NULL;
	if (it.is_def) {
		static MACRO_META meta;
		memset(&meta, 0, sizeof(meta));
		meta.inside = true;
		meta.param_table = true;
		meta.param_id = it.id;
		meta.index = it.ix;
		meta.source_id = 1;
		meta.source_line = -2;
		if (it.set.defaults && it.set.defaults->metat) {
			meta.ref_count = it.set.defaults->metat[it.id].ref_count;
			meta.use_count = it.set.defaults->metat[it.id].use_count;
		} else {
			meta.ref_count = -1;
			meta.use_count = -1;
		}
		return &meta;
	}
	return it.set.metat ? &it.set.metat[it.ix] : NULL;
}
int hash_iter_used_value(HASHITER& it) {
	if (hash_iter_done(it)) return -1;
	if (it.is_def) {
		if (it.set.defaults && it.set.defaults->metat) {
			return it.set.defaults->metat[it.id].use_count + it.set.defaults->metat[it.id].ref_count;
		}
		return -1;
	}
	if (it.set.metat && (it.ix >= 0 && it.ix < it.set.size))
		return it.set.metat[it.ix].use_count + it.set.metat[it.ix].ref_count;
	return -1;
};

/*
** Same as find_config_macro() below, but finds special references like $ENV().
*/
extern "C" int
find_special_config_macro( const char *prefix, bool only_id_chars, register char *value, 
		register char **leftp, register char **namep, register char **rightp )
{
	char *left, *left_end, *name, *right;
	char *tvalue;
	int prefix_len;

	if ( prefix == NULL ) {
		return( 0 );
	}

	prefix_len = strlen(prefix);
	tvalue = value;
	left = value;

		// Loop until we're done, helped with the magic of goto's
	while (1) {
tryagain:
		if (tvalue) {
			value = const_cast<char *>(strstr(tvalue, prefix) );
		}
		
		if( value == NULL ) {
			return( 0 );
		}

		value += prefix_len;
		if( *value == '(' ) {
			left_end = value - prefix_len;
			name = ++value;
			while( *value && *value != ')' ) {
				char c = *value++;
				if( !ISIDCHAR(c) && only_id_chars ) {
					tvalue = name;
					goto tryagain;
				}
			}

			if( *value == ')' ) {
				right = value;
				break;
			} else {
				tvalue = name;
				goto tryagain;
			}
		} else {
			tvalue = value;
			goto tryagain;
		}
	}

	*left_end = '\0';
	*right++ = '\0';

	*leftp = left;
	*namep = name;
	*rightp = right;

	return( 1 );
}

/* Docs are in /src/condor_includes/condor_config.h */
extern "C" int
find_config_macro( register char *value, register char **leftp, 
		 register char **namep, register char **rightp,
		 const char *self,
		 bool getdollardollar, int search_pos)
{
	char *left, *left_end, *name, *right;
	char *tvalue;
   #ifdef COLON_DEFAULT_FOR_MACRO_EXPAND
	int after_colon = 0; // records the offset of the : from the start of the macro
   #endif

	tvalue = value + search_pos;
	left = value;

	for(;;) {
tryagain:
		if (tvalue) {
			value = const_cast<char *>( strchr(tvalue, '$') );
		}
		
		if( value == NULL ) {
			return( 0 );
		}

			// Do not treat $$(foo) as a macro unless
			// getdollardollar = true.  This is for
			// condor_submit, so it does not try to expand
			// macros when we do "executable = foo.$$(arch)"
			// If getdollardollar is true, than only get
			// macros with two dollar signs, i.e. $$(foo).
		if ( getdollardollar ) {
			if ( *++value != '$' ) {
				// this is not a $$ macro
				tvalue = value;
				goto tryagain;
			}
		} else {
			// here getdollardollar is false, so ignore
			// any $$(foo) macro.
			if ( (*(value + sizeof(char))) == '$' ) {
				value++; // skip current $
				value++; // skip following $
				tvalue = value;
				goto tryagain;
			}
		}

		if( *++value == '(' ) {
			if( getdollardollar && *value != '\0' && value[1] == '[' ) {

				// This is a classad expression to be evaled.  This layer
				// doesn't need any intelligence, just scan for "])" which
				// terminates it.  If we get to end of string without finding
				// the terminating pattern, this $$ match fails, try again.

				char * end_marker = strstr(value, "])");
				if( end_marker == NULL ) {
					tvalue = value;
					goto tryagain;
				}

				left_end = value - 2;
				name = ++value;
				right = end_marker + 1;
				break;

			} else { 

				// This might be a "normal" values $(FOO), $$(FOO) or
				// $$(FOO:Bar) Skim until ")".  We only allow valid characters
				// inside (basically alpha-numeric. $$ adds ":"); a non-valid
				// character means this $$ match fails.  End of string before
				// ")" means this match fails, try again.

				if ( getdollardollar ) {
					left_end = value - 2;
				} else {
					left_end = value - 1;
				}
				name = ++value;
			   #ifdef COLON_DEFAULT_FOR_MACRO_EXPAND
				after_colon = 0;
			   #endif
				while( *value && *value != ')' ) {
					char c = *value++;
				   #ifdef COLON_DEFAULT_FOR_MACRO_EXPAND
					if ( ! after_colon && c == ':') {
						after_colon = (int)(value - name);
					} else if (after_colon) {
						if (c == '(') {
							// skip ahead past the close )
							char * ptr = strchr(value, ')');
							if (ptr) value = ptr+1;
							continue;
						} else if (strchr("$ ,\\", c)) {
							// allow some characters after the : that we don't allow in param names
							continue;
						}
					}
					if( !ISDDCHAR(c) ) {
						tvalue = name;
						goto tryagain;
					}
				   #else
					if( getdollardollar ) {
						if( !ISDDCHAR(c) ) {
							tvalue = name;
							goto tryagain;
						}
					} else {
						if( !ISIDCHAR(c) ) {
							tvalue = name;
							goto tryagain;
						}
					}
				   #endif
				}

				if( *value == ')' ) {
					// We pass (value-name) to strncmp since name contains
					// the entire line starting from the identifier and value
					// points to the end of the identifier.  Thus, the difference
					// between these two pointers gives us the length of the
					// identifier.
					int namelen = value-name;
				   #ifdef COLON_DEFAULT_FOR_MACRO_EXPAND
					if (after_colon) namelen = after_colon-1;
				   #endif
					if( !self || ( strncasecmp( name, self, namelen ) == MATCH && self[namelen] == '\0' ) ) {
							// $(DOLLAR) has special meaning; it is
							// set to "$" and is _not_ recursively
							// expanded.  To implement this, we have
							// find_config_macro() ignore $(DOLLAR) and we then
							// handle it in expand_macro().
							// Note that $$(DOLLARDOLLAR) is handled a little
							// differently.  Instead of skipping over it,
							// we treat it like anything else, and it is up
							// to the caller to advance search_pos, so we
							// don't run into the literal $$ again.
						if ( !self && namelen == DOLLAR_ID_LEN &&
								strncasecmp(name,DOLLAR_ID,namelen) == MATCH ) {
							tvalue = name;
							goto tryagain;
						}
						right = value;
						break;
					} else {
						tvalue = name;
						goto tryagain;
					}
				} else {
					tvalue = name;
					goto tryagain;
				}
			}
		} else {
			tvalue = value;
			goto tryagain;
		}
	}

	*left_end = '\0';
	*right++ = '\0';

	*leftp = left;
	*namep = name;
	*rightp = right;

	return( 1 );
}

/*
** Return the value associated with the named parameter.  Return NULL
** if the given parameter is not defined.
*/
const char * lookup_macro_exact(const char *name, MACRO_SET & set, int use)
{
	MACRO_ITEM* pitem = find_macro_item(name, set);
	if (pitem) {
		if (set.metat) {
			MACRO_META* pmeta = &set.metat[pitem - set.table];
			pmeta->use_count += (use&1);
			pmeta->ref_count += (use>>1)&1;
		}
		return pitem->raw_value;
	}
	return NULL;
}

const char * lookup_macro(const char *name, const char *prefix, MACRO_SET & set, int use)
{
	MyString prefixed_name;
	if (prefix) {
		// snprintf(tmp_name, MAX_PARAM_LEN, "%s.%s", prefix, name);
		prefixed_name.formatstr("%s.%s", prefix, name);
		name = prefixed_name.Value();
	}
	return lookup_macro_exact(name, set, use);
}

