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
#include "printf_format.h"

#if defined(__cplusplus)
extern "C" {
#endif

#define CONFIG_GETLINE_OPT_COMMENT_DOESNT_CONTINUE        1
#define CONFIG_GETLINE_OPT_CONTINUE_MAY_BE_COMMENTED_OUT  2
static char *getline_implementation(FILE * fp, int buffer_size, int options, int & line_number);


//int		ConfigLineNo;

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
	// NULL or empty param names are not valid
	if(!name || !name[0]) {
		return 0;
	}

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


int read_meta_config(MACRO_SOURCE & source, int depth, const char *name, const char * rhs, MACRO_SET& macro_set, MACRO_EVAL_CONTEXT & ctx)
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

	// the SUBMIT macro set stores metaknobs directly in it's defaults table.
	if (macro_set.options & CONFIG_OPT_SUBMIT_SYNTAX) {

		StringList items(rhs);
		items.rewind();
		char * item;
		while ((item = items.next()) != NULL) {
			std::string metaname;
			formatstr(metaname, "$%s.%s", name, item);
			const MACRO_DEF_ITEM * p = find_macro_def_item(name, macro_set, ctx.use_mask);
			if ( ! p) {
				fprintf(stderr, "\nERROR: use %s: does not recognise %s\n", name, item);
				return -1;
			}
			int ret = Parse_config_string(source, depth, p->def->psz, macro_set, ctx);
			if (ret < 0) {
				const char * msg = "Internal Submit Error: use %s: %s is invalid\n";
				if (ret == -2) msg = "\nERROR: use %s: %s nesting too deep\n"; 
				fprintf(stderr, msg, name, item);
				return ret;
			}
		}
		return 0;
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
		int ret = Parse_config_string(source, depth, value, macro_set, ctx);
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
		case ct_alpha|ct_space|ct_digit|ct_ident:
		case ct_alpha|ct_space|ct_digit|ct_float:
		case ct_alpha|ct_space|ct_digit|ct_ident|ct_float:
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
static bool Evaluate_config_if(const char * expr, bool & result, std::string & err_reason, MACRO_SET & macro_set, MACRO_EVAL_CONTEXT & ctx)
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
				const char * tvalue = lookup_macro(name, macro_set, ctx);
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

bool Test_config_if_expression(const char * expr, bool & result, std::string & err_reason, MACRO_SET& macro_set, MACRO_EVAL_CONTEXT & ctx)
{
	bool value = result;
	bool inverted = false;

	// optimize the simple case by not bothering to macro expand if there are no $ in the expression
	char * expanded = NULL; // so we know whether to free the macro expansion
	if (strstr(expr, "$")) {
		expanded = expand_macro(expr, macro_set, ctx);
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
		valid = Evaluate_config_if(expr, value, err_reason, macro_set, ctx);
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
	bool line_is_if(const char * line, std::string & errmsg, MACRO_SET& macro_set, MACRO_EVAL_CONTEXT & ctx);
};

bool ConfigIfStack::line_is_if(const char * line, std::string & errmsg, MACRO_SET& macro_set, MACRO_EVAL_CONTEXT & ctx)
{
	if (starts_with_ignore_case(line,"if") && (isspace(line[2]) || !line[2])) {
		const char * expr = line+2;
		while (isspace(*expr)) ++expr;

		bool bb = this->enabled();
		std::string err_reason;
		if (bb && ! Test_config_if_expression(expr, bb, err_reason, macro_set, ctx)) {
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
		if (bb && ! Test_config_if_expression(expr, bb, err_reason, macro_set, ctx)) {
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
int Parse_config_string(MACRO_SOURCE & source, int depth, const char * config, MACRO_SET& macro_set, MACRO_EVAL_CONTEXT & ctx)
{
	const bool is_submit = macro_set.options & CONFIG_OPT_SUBMIT_SYNTAX;
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
		if (ifstack.line_is_if(line, errmsg, macro_set, ctx)) {
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
		char* expanded_name = expand_macro(name, macro_set, ctx);
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
			int retval = read_meta_config(source2, depth+1, name, rhs, macro_set, ctx);
			if (retval < 0) {
				return retval;
			}
		} else if (is_submit && (*name == '+' || *name == '-')) {

			// submit files have special +Attr= and -Attr= syntax that is used to store raw 
			// key/value pairs directly into the job ad. We will put them into the submit
			// macro set with a "MY." prefix on their names.
			//
			std::string plusname = "MY."; plusname += (name+1);
			insert_macro(plusname.c_str(), (*name=='+') ? rhs : "", macro_set, source, ctx);

		} else {

			/* Check that "name" is a legal identifier : only
			   alphanumeric characters and _ allowed*/
			if ( ! is_valid_param_name(name) ) {
				return -1;
			}

			/* expand self references only */
			char * value = expand_self_macro(rhs, name, macro_set, ctx);
			if (value == NULL) {
				return -1;
			}

			insert_macro(name, value, macro_set, source, ctx);
			FREE(value);
		}

		// FREE(expanded_name);
	}
	source.meta_off = -2;
	return 0;
}

int
Parse_macros(
	FILE* conf_fp,
	MACRO_SOURCE& FileSource,
	int depth, // a simple recursion detector
	MACRO_SET& macro_set,
	int options,
	MACRO_EVAL_CONTEXT * pctx,
	std::string& config_errmsg,
	int (*fnSubmit)(void* pv, MACRO_SOURCE& source, MACRO_SET& set, char * line, std::string & errmsg),
	void * pvSubmitData)
{
	char*	name = NULL;
	char*	value = NULL;
	char*	rhs = NULL;
	char*	ptr = NULL;
	char	op, name_end_ch;
	int		retval = 0;
	bool	firstRead = true;
	const int gl_opt_old = 0;
	const int gl_opt_new = CONFIG_GETLINE_OPT_COMMENT_DOESNT_CONTINUE | CONFIG_GETLINE_OPT_CONTINUE_MAY_BE_COMMENTED_OUT;
	int gl_opt = (macro_set.options & CONFIG_OPT_OLD_COM_IN_CONT) ? gl_opt_old : gl_opt_new;
	bool gl_opt_smart = (macro_set.options & CONFIG_OPT_SMART_COM_IN_CONT) ? true : false;
	int opt_meta_colon = (macro_set.options & CONFIG_OPT_COLON_IS_META_ONLY) ? 1 : 0;
	ConfigIfStack ifstack;
	MACRO_EVAL_CONTEXT defctx = { NULL, NULL, false, 2 };
	if ( ! pctx) pctx = &defctx;

	bool is_submit = (fnSubmit != NULL);
	const char * source_file = macro_source_filename(FileSource, macro_set);
	const char * source_type = is_submit ? "Submit file" : "Config source";

	while (true) {
		name = getline_implementation(conf_fp, 128, gl_opt, FileSource.line);
		// If the file is empty the first time through, warn the user.
		if (name == NULL) {
			if (firstRead) {
				dprintf(D_FULLDEBUG, "WARNING: %s is empty: %s\n", source_type, source_file);
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
		if (ifstack.line_is_if(name, errmsg, macro_set, *pctx)) {
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
				// Here we have determined the line has a name but no operator or whitespace after it.
			if (is_submit) {
				// a line with no operator may be a QUEUE statement, so hand it off to the queue callback.
				retval = fnSubmit(pvSubmitData, FileSource, macro_set, name, config_errmsg);
				if (retval != 0)
					goto cleanup;
				continue;
			} else if ( name && name[0] == '[' ) {
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

#if 1 // SUBMIT_USING_CONFIG_PARSER
		char * pop = ptr; // keep track of where we see the operator
		op = *pop;
		char * name_end = ptr; // keep track of where we null-terminate the name, so we can reverse it later
		name_end_ch = *name_end;
		if (*ptr) { *ptr++ = '\0'; }
		// scan for an operator character if we don't have one already.
		if ( ! ISOP(op)) {
			while ( *ptr && ! ISOP(*ptr) ) {
				++ptr;
			}
			pop = ptr;
			op = *ptr;
			if (op) ++ptr;
		}
		// if we still haven't got an operator, then this isn't a valid config line,
		// (it *might* be a valid submit line however.)
		if ( ! ISOP(op) && ! is_submit) {
			retval = -1;
			goto cleanup;
		}
#else
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
			if( !*ptr  && ! is_submit) {
				// Here we have determined the line has no operator at all
				retval = -1;
				goto cleanup;
			}
			pop = ptr;
			op = *ptr++;
		}
#endif

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
		} else if (op == ':' && ! is_submit) {
		#ifdef WARN_COLON_FOR_PARAM_ASSIGN
			if (opt_meta_colon < 2) { op = '='; } // change the op to = so that we don't "goto cleanup" below

			// backward compat hack. the old config file used : syntax for RunBenchmarks,
			// so grandfather this in. tread error as warning, tread warning as ignore.
			if (MATCH == strcasecmp(name, "RunBenchmarks")) { op = '='; if (opt_meta_colon < 2) opt_meta_colon = 0; }

			if (opt_meta_colon) {
				fprintf( stderr, "%s %s \"%s\", Line %d: obsolete use of ':' for parameter assignment at %s : %s\n",
						source_type, op == ':' ? "Error" : "Warning",
						source_file, FileSource.line,
						name, rhs
						);
				if (op == ':') {
					retval = -1;
					goto cleanup;
				}
			}
		#endif // def WARN_COLON_FOR_PARAM_ASSIGN
		}

		// Expand references to other parameters in the macro name.
		// this returns a strdup'd string even if there are no macros to expand.
		// bool use_default_param_table = (macro_set.options & CONFIG_OPT_DEFAULTS_ARE_PARAM_INFO) != 0;
		char * line = name; // in case we need to get back to pre-expanded state (for submit)
		name = expand_macro(name, macro_set, *pctx);
		if( name == NULL ) {
			retval = -1;
			goto cleanup;
		}
		// name is now a copy of the original, so we can put back the character that we
		// overwrote with the null terminator so that line is the original line we read.
		*name_end = name_end_ch;

		// if this is a metaknob
		if (is_meta) {
			//FileSource.line = ConfigLineNo;
			retval = read_meta_config(FileSource, depth+1, name, rhs, macro_set, *pctx);
			if (retval < 0) {
				fprintf( stderr,
							"%s Error \"%s\", Line %d: at use %s:%s\n",
							source_type, source_file, FileSource.line, name, rhs );
				goto cleanup;
			}
		} else if (is_include) {
			MACRO_SOURCE InnerSource;
			FILE* fp = Open_macro_source(InnerSource, name, (is_include > 1), macro_set, config_errmsg);
			if ( ! fp) { retval = -1; }
			else {
				if (depth+1 >= CONFIG_MAX_NESTING_DEPTH) {
					config_errmsg = "includes nested too deep";
					retval = -2; // indicate that nesting depth has been exceeded.
				} else {
					if ( ! is_submit) local_config_sources.append(macro_source_filename(InnerSource, macro_set));
					retval = Parse_macros(fp, InnerSource, depth+1, macro_set, options, pctx, config_errmsg, fnSubmit, pvSubmitData);
				}
			}
			if (retval < 0) {
				fprintf( stderr,
							"%s Error \"%s\", Line %d, Include Depth %d: %s\n",
							source_type, name, InnerSource.line, depth+1, config_errmsg.c_str());
				config_errmsg.clear();
				goto cleanup;
			}
		} else if (is_submit && op == '=' && (*name == '+' || *name == '-')) {

			// submit files have special +Attr= and -Attr= syntax that is used to store raw 
			// key/value pairs directly into the job ad. We will put them into the submit
			// macro set with a "MY." prefix on their names.
			//
			std::string plusname = "MY."; plusname += name+1;
			insert_macro(plusname.c_str(), (*name=='+') ? rhs : "", macro_set, FileSource, *pctx);

		} else if (is_submit && (op != '=' || MATCH == strcasecmp(name, "queue"))) {

			retval = fnSubmit(pvSubmitData, FileSource, macro_set, line, config_errmsg);
			if (retval != 0) // this may or may not be a failure, but we should stop reading the file.
				goto cleanup;

		} else {

			/* Check that "name" is a legal identifier : only
			   alphanumeric characters and _ allowed*/
			if( !is_valid_param_name(name) ) {
				fprintf( stderr,
						 "%s Error \"%s\", Line %d: Illegal Identifier: <%s>\n",
						 source_type, source_file, FileSource.line, (name?name:"(null)") );
				retval = -1;
				goto cleanup;
			}

			if (options & READ_MACROS_EXPAND_IMMEDIATE) {
				value = expand_macro(rhs, macro_set, *pctx);
				if( value == NULL ) {
					retval = -1;
					goto cleanup;
				}
			} else  {
				/* expand self references only */
				value = expand_self_macro(rhs, name, macro_set, *pctx);
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
				insert_macro(name, value, macro_set, FileSource, *pctx);
			} else {
				fprintf( stderr,
					"%s Error \"%s\", Line %d: Syntax Error, missing : or =\n",
					source_type, source_file, FileSource.line );
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
				"%s Error \"%s\", Line %d: \n",
				source_type, source_file, FileSource.line );
		config_errmsg = "endif(s) not found before end-of-file";
		retval = -1;
		goto cleanup;
	}

 cleanup:
	if(name) { FREE( name ); }
	if(value) { FREE( value ); }
	return retval;
}

FILE* Open_macro_source (
	MACRO_SOURCE& macro_source,
	const char* source,
	bool        source_is_command,
	MACRO_SET& macro_set,
	std::string & config_errmsg)
{
	FILE*	fp = NULL;
	std::string cmdbuf; // in case we have to produce a modified command
	const char * cmd = NULL;

	bool is_pipe_cmd = is_piped_command(source);
	if (source_is_command && ! is_pipe_cmd) {
		is_pipe_cmd = true;
		cmd = source; // the input source is actually the command (without trailing |)
		cmdbuf = source; cmdbuf += " |";
		source = cmdbuf.c_str();
	} else if (is_pipe_cmd) {
		cmdbuf = source; // the input source is the command with trailing |
		// remove trailing | and spaces
		for (int ix = (int)cmdbuf.length()-1; ix > 0 && (cmdbuf[ix] == '|' || cmdbuf[ix] == ' '); --ix) {
			cmdbuf[ix] = 0;
		}
		cmd = cmdbuf.c_str();
	}

	// initialize a MACRO_SOURCE for this file
	insert_source(source, macro_set, macro_source);
	macro_source.is_command = is_pipe_cmd;

	// Determine if the config file name specifies a file to open, or a
	// pipe to suck on. Process each accordingly
	if (is_pipe_cmd) {
		if ( is_valid_command(source) ) {
			ArgList argList;
			MyString args_errors;
			if(!argList.AppendArgsV1RawOrV2Quoted(cmd, &args_errors)) {
				formatstr(config_errmsg, "Can't append args, %s", args_errors.Value());
				return NULL;
			}
			fp = my_popen(argList, "r", FALSE);
			if ( ! fp) {
				config_errmsg = "not a valid command";
				return NULL;
			}
		} else {
			config_errmsg = "not a valid command, | must be at the end\n";
			return NULL;
		}
	} else {
		fp = safe_fopen_wrapper_follow(source, "r");
		if ( ! fp) {
			config_errmsg = "can't open file";
			return NULL;
		}
	}

	return fp;
}

int Close_macro_source(FILE* conf_fp, MACRO_SOURCE& source, MACRO_SET& macro_set, int parsing_return_val)
{
	if (conf_fp) {
		if (source.is_command) {
			int exit_code = my_pclose(conf_fp);
			if (0 == parsing_return_val && exit_code != 0) {
				fprintf( stderr, "Configuration Error \"%s\": "
						 "command terminated with exit code %d\n",
						 macro_source_filename(source, macro_set), exit_code );
				return -1;
			}
		} else {
			fclose(conf_fp);
		}
	}
	return parsing_return_val;
}


/*
** lowest level macro lookup function, looks up a name in the given macro set
** but does not look in the macro_set defaults.  if prefix is supplied 
** then prefix.name is looked up.
*/

// compare key to prefix+sep+name in case-insensitive way.
int strjoincasecmp(const char * key, const char * prefix, const char * name, char sep)
{
	const char * k = key;
	const char * p = prefix;
	if (prefix) {
		while (*k && tolower(*k) == tolower(*p)) { ++k; ++p; }
		if ( ! *k) { // ran out of key
			if ( ! *p) return name ? -1 : 0;
			else return -1; // because tolower(*p) > tolower(*k) must be true here..
		} else if (*p) { // still have prefix, but does not match
			return tolower(*p) > tolower(*k) ? -1 : 1;
		} else if (sep) {
			if (*k == sep) ++k;
			else return (int)(unsigned char)sep > (int)(unsigned char)*k ? -1 : 1;
		}
		if ( ! name) return 1;
	}
	return strcasecmp(k, name);
}

extern "C++" MACRO_ITEM* find_macro_item (const char *name, const char * prefix, MACRO_SET& set)
{
	int cElms = set.size;
	MACRO_ITEM* aTable = set.table;

	if (set.sorted < set.size) {
		// use a brute force search on the unsorted parts of the table.
		for (int ii = set.sorted; ii < cElms; ++ii) {
			if (MATCH == strjoincasecmp(aTable[ii].key, prefix, name, '.'))
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
		int iMatch = strjoincasecmp(aTable[ix].key, prefix, name, '.');
		if (iMatch < 0)
			ixLower = ix+1;
		else if (iMatch > 0)
			ixUpper = ix-1;
		else
			return &aTable[ix];
	}
}

// insert a source name into the MACRO_SET's table of names
// and initialize the MACRO_SOURCE.
void insert_source(const char * filename, MACRO_SET & set, MACRO_SOURCE & source)
{
	if ( ! set.sources.size()) {
		set.sources.push_back("<Detected>");
		set.sources.push_back("<Default>");
		set.sources.push_back("<Environment>");
		set.sources.push_back("<Over>");
	}
	source.line = 0;
	source.is_inside = false;
	source.is_command = false;
	source.id = (int)set.sources.size();
	source.meta_id = -1;
	source.meta_off = -2;
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
		// we get here only if *a or *b is 0
		// if both are 0, then this is a matching path, otherwise it isn't
		return (*a)==(*b);
	}
#endif

	return false;
}

void insert_macro(const char *name, const char *value, MACRO_SET & set, const MACRO_SOURCE & source, MACRO_EVAL_CONTEXT & ctx)
{
	// if already in the macro-set, we need to expand self-references and replace
	MACRO_ITEM * pitem = find_macro_item(name, NULL, set);
	if (pitem) {
		char * tvalue = expand_self_macro(value, name, set, ctx);
		if (MATCH != strcmp(tvalue, pitem->raw_value)) {
			pitem->raw_value = set.apool.insert(tvalue);
		}
		if (set.metat) {
			MACRO_META * pmeta = &set.metat[pitem - set.table];
			pmeta->source_id = source.id;
			pmeta->source_line = source.line;
			pmeta->source_meta_id = source.meta_id;
			pmeta->source_meta_off = source.meta_off;
			pmeta->inside = (source.is_inside != false);
			pmeta->param_table = false;
			// use the name here in case we have a compound name, i.e "master.value"
			const char * post_prefix = NULL;
			int param_id = param_default_get_id(name, &post_prefix);
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
	const char * post_prefix = NULL; // if non-null, a pointer to the name after the "subsys." or "localname."
	int param_id = param_default_get_id(name, &post_prefix);
	const char * def_value = param_default_rawval_by_id(param_id);
	bool is_path = param_default_ispath_by_id(param_id);
	if (same_param_value(def_value, value, is_path)) {
		matches_default = true; // flag value as matching the default.
		if ( ! post_prefix && ! (set.options & CONFIG_OPT_KEEP_DEFAULTS))
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
		pmeta->inside = source.is_inside;
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
	MACRO_ITEM* pitem = find_macro_item(name, NULL, set);
	if (pitem && set.metat) {
		MACRO_META* pmeta = &set.metat[pitem - set.table];
		return ++(pmeta->use_count);
	}
	return -1;
}

void clear_macro_use_count (const char *name, MACRO_SET & set)
{
	MACRO_ITEM* pitem = find_macro_item(name, NULL, set);
	if (pitem && set.metat) {
		MACRO_META* pmeta = &set.metat[pitem - set.table];
		pmeta->ref_count = pmeta->use_count = 0;
	}
}

int get_macro_use_count (const char *name, MACRO_SET & set)
{
	MACRO_ITEM* pitem = find_macro_item(name, NULL, set);
	if (pitem && set.metat) {
		MACRO_META* pmeta = &set.metat[pitem - set.table];
		return pmeta->use_count;
	}
	return -1;
}

int get_macro_ref_count (const char *name, MACRO_SET & set)
{
	MACRO_ITEM* pitem = find_macro_item(name, NULL, set);
	if (pitem && set.metat) {
		MACRO_META* pmeta = &set.metat[pitem - set.table];
		return pmeta->ref_count;
	}
	return -1;
}

// These provide external linkage to the getline_implementation function for use by non-config code
extern "C" char * getline_trim( FILE *fp ) {
	int lineno=0;
	const int options = CONFIG_GETLINE_OPT_CONTINUE_MAY_BE_COMMENTED_OUT | CONFIG_GETLINE_OPT_COMMENT_DOESNT_CONTINUE;
	return getline_implementation(fp, _POSIX_ARG_MAX, options, lineno);
}
extern "C++" char * getline_trim( FILE *fp, int & lineno, int mode ) {
	const int default_options = CONFIG_GETLINE_OPT_CONTINUE_MAY_BE_COMMENTED_OUT | CONFIG_GETLINE_OPT_COMMENT_DOESNT_CONTINUE;
	const int simple_options = 0;
	int options = (mode & GETLINE_TRIM_SIMPLE_CONTINUATION) ? simple_options : default_options;
	return getline_implementation(fp,_POSIX_ARG_MAX, options, lineno);
}

/*
** Read one line and any continuation lines that go with it.  Lines ending
** with <white space><backslash> are continued onto the next line.
** Lines can be of any lengh.  We pass back a pointer to a buffer; do _not_
** free this memory.  It will get freed the next time getline() is called (this
** function used to contain a fixed-size static buffer).
*/
static char *
getline_implementation( FILE *fp, int requested_bufsize, int options, int & line_number )
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

		++line_number;
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

// classes derived from this class can be used to 
// control skipping of some instances of a macro based
// on the body text. 
class ConfigMacroBodyCheck {
public:
	virtual bool skip(int func_id, const char * body, int bodylen) = 0;
};

enum {
	MACRO_ID_DOUBLEDOLLAR=-2,
	MACRO_ID_NORMAL=-1,
	SPECIAL_MACRO_ID_NONE=0,
	SPECIAL_MACRO_ID_ENV,
	SPECIAL_MACRO_ID_RANDOM_CHOICE,
	SPECIAL_MACRO_ID_RANDOM_INTEGER,
	SPECIAL_MACRO_ID_CHOICE,
	SPECIAL_MACRO_ID_SUBSTR,
	SPECIAL_MACRO_ID_INT,
	SPECIAL_MACRO_ID_REAL,
	SPECIAL_MACRO_ID_STRING,
	SPECIAL_MACRO_ID_BASENAME,
	SPECIAL_MACRO_ID_DIRNAME,
	SPECIAL_MACRO_ID_FILENAME,
};

enum MACRO_BODY_CHARS {
	MACRO_BODY_ANYTHING=0,   // allow anything other than ) in the body
	MACRO_BODY_IDCHAR_COLON, // allow only identifier characters up to a : and  then a few more characters after
	MACRO_BODY_SCAN_BRACKET, // scan ahead for "])"
};

int next_config_macro (
	int (*check_prefix)(const char *dollar, int length, MACRO_BODY_CHARS & idchar_only),
	ConfigMacroBodyCheck & body,
	char *value, int search_pos,
	char **leftp, char **namep, char **rightp, char** dollar );

static int is_special_config_macro(const char* prefix, int length, MACRO_BODY_CHARS & bodychars)
{
	#define PRE(n) { "$" #n, sizeof(#n), SPECIAL_MACRO_ID_ ## n }
	static const struct {
		const char * name;
		int length;
		int id;
	} pre[] = {
		PRE(ENV),
		PRE(RANDOM_CHOICE),
		PRE(RANDOM_INTEGER),
		PRE(CHOICE),
		PRE(SUBSTR),
		PRE(INT),
		PRE(REAL),
		PRE(STRING),
		PRE(BASENAME), PRE(DIRNAME),
	};
	#undef PRE

	if (length <= 1)
		return 0;

	// tell the caller that we allow any char in the body of this config macro
	// we will set this to true only if $ENV ends up being the matched special macro.
	bodychars = MACRO_BODY_ANYTHING;

	// the special filename macro function has a bunch of names, so check it first.
	if (length >= 1 && prefix[1] == 'F') {
		bool is_fname = true;
		for (int ii = 2; ii < length; ++ii) {
			int ch = prefix[ii] | 0x20; // convert to lowercase for comparision.
			if (ch != 'p' && ch != 'n' && ch != 'x' && ch != 'd' && ch != 'q') {
				is_fname = false;
				break;
			}
		}
		if (is_fname)
			return SPECIAL_MACRO_ID_FILENAME;
	}

	for (int ii = 0; ii < (int)COUNTOF(pre); ++ii) {
		if (length == pre[ii].length && MATCH == strncmp(prefix, pre[ii].name, pre[ii].length)) {
			if (pre[ii].id == SPECIAL_MACRO_ID_ENV) { bodychars = MACRO_BODY_IDCHAR_COLON; }
			return pre[ii].id;
		}
	}

	return 0;
}

static int is_config_macro(const char* prefix, int length, MACRO_BODY_CHARS & bodychars)
{
	// prefix of just "$" is our normal macro expansion
	// prefix of just "$$" is dollardollar macro expansion, we should not match those...
	if (length == 1) {
		bodychars = MACRO_BODY_IDCHAR_COLON;
		return MACRO_ID_NORMAL;
	} else if (length > 1 && prefix[1] == '$') {
		return 0;
	}
	return is_special_config_macro(prefix, length, bodychars);
}


char * strdup_quoted(const char* str, int cch, bool quoted) {
	if (cch < 0) cch = (int)strlen(str);

	// ignore leading and/or trailing quotes when we dup
	if (*str=='"') { ++str; --cch; }
	if (cch > 0 && str[cch-1] == '"') --cch;

	// malloc with room for quotes and a terminating 0
	char * out = (char*)malloc(cch+3);
	ASSERT(out);
	char * p = out;

	// copy, adding quotes or not as requested.
	if (quoted) { *p++ = '"'; }
	memcpy(p, str, cch*sizeof(str[0]));
	if (quoted) { p[cch++] = '"'; }
	p[cch] = 0;

	return out;
}

// use this class with next_config_macro to skip $(DOLLAR)
class NoDollarBody : public ConfigMacroBodyCheck {
public:
	virtual bool skip(int func_id, const char * body, int len) {
		return func_id == MACRO_ID_NORMAL
			&& len == DOLLAR_ID_LEN
			&& MATCH == strncasecmp(body, DOLLAR_ID, DOLLAR_ID_LEN);
	}
};

class DollarOnlyBody : public ConfigMacroBodyCheck {
public:
	virtual bool skip(int func_id, const char * body, int len) {
		return func_id != MACRO_ID_NORMAL
			|| len != DOLLAR_ID_LEN
			|| MATCH != strncasecmp(body, DOLLAR_ID, DOLLAR_ID_LEN);
	}
};

/*
** Expand parameter references of the form "left$(middle)right".  This
** is deceptively simple, but does handle multiple and or nested references.
** Also expand references of the form "left$ENV(middle)right",
** replacing $ENV(middle) with getenv(middle).
** Also expand references of the form "left$RANDOM_CHOICE(middle)right".
*/


const char * lookup_macro_exact_no_default(const char *name, MACRO_SET & set, int use)
{
	MACRO_ITEM* pitem = find_macro_item(name, NULL, set);
	if (pitem) {
		if (set.metat && use) {
			MACRO_META* pmeta = &set.metat[pitem - set.table];
			pmeta->use_count += (use&1);
			pmeta->ref_count += (use>>1)&1;
		}
		return pitem->raw_value;
	}
	return NULL;
}


const char * lookup_macro_exact_no_default(const char *name, const char *prefix, MACRO_SET & set, int use)
{
	MACRO_ITEM* pitem = find_macro_item(name, prefix, set);
	if (pitem) {
		if (set.metat && use) {
			MACRO_META* pmeta = &set.metat[pitem - set.table];
			pmeta->use_count += (use&1);
			pmeta->ref_count += (use>>1)&1;
		}
		return pitem->raw_value;
	}
	return NULL;
}

// lookup macro in all of the usual places.  The lookup order is
//    localname.name in config file
//    localname.name in defaults table
//    subsys.name in config file
//    subsys.name in defaults table
//    name in config file
//    name in defaults table
// return:
//   NULL    if the macro does not exist in any of the tables
//   ""      if the macro exists but was not given a value.
//   string  if the macro was defined. the string pointer is valid until the next reconfig, it should not be freed.
//

const char * lookup_macro(const char * name, MACRO_SET & macro_set, MACRO_EVAL_CONTEXT & ctx)
{
	const char * lval = NULL;
	if (ctx.localname) {
		lval = lookup_macro_exact_no_default(name, ctx.localname, macro_set, ctx.use_mask);
		if (lval) return lval;

		if (macro_set.defaults && ! ctx.without_default) {
			const MACRO_DEF_ITEM * p = find_macro_subsys_def_item(name, ctx.localname, macro_set, ctx.use_mask);
			if (p) return p->def ? p->def->psz : "";
		}
	}
	if (ctx.subsys) {
		lval = lookup_macro_exact_no_default(name, ctx.subsys, macro_set, ctx.use_mask);
		if (lval) return lval;

		if (macro_set.defaults && ! ctx.without_default) {
			const MACRO_DEF_ITEM * p = find_macro_subsys_def_item(name, ctx.subsys, macro_set, ctx.use_mask);
			if (p) return p->def ? p->def->psz : "";
		}
	}

	// lookup 'name' in the primary macro_set.
	// Note that if 'name' has been explicitly set to nothing,
	// lval will _not_ be NULL so we will not call
	// find_macro_def_item().  See gittrack #1302
	lval = lookup_macro_exact_no_default(name, macro_set, ctx.use_mask);
	if (lval) return lval;

	// if not found in the config file, lookup in the param table
	// note that we do ALL of the param lookups last.  This means that
	// if you have subsys.foo in the param table, a simple foo in
	// the config file will take precedence.
	if (macro_set.defaults && ! ctx.without_default) {
		const MACRO_DEF_ITEM * p = find_macro_def_item(name, macro_set, ctx.use_mask);
		if (p && p->def) lval = p->def->psz;
	}
	return lval;
}

// given the body text of a config macro, and the macro id and macro context
// evaluate the body and return a string. the string may be a literal, or
// may point into to the buffer returned in tbuf.  The caller will NOT free
// the return value, but will free tbuf if it is not NULL with the understanding
// that the return value may be freed as a result.
static const char * evaluate_macro_func (
	const char * func, // can be NULL, or ptr to macro function name terminated by \0 or (
	int special_id,    // non-zero macro id, must match func if func is supplied
	char * body, // \0 terminated body of macro - what's inside the (). body may be altered by this function.
	auto_free_ptr & tbuf, // use this to return a malloc to be auto-free'd by the caller
	MACRO_SET& macro_set,
	MACRO_EVAL_CONTEXT & ctx)
{
	const char * tvalue = NULL; // return value from evaluating
	const char * name = body;
	char * buf = NULL;

	switch (special_id) {
		case MACRO_ID_NORMAL:
		{
			char * pcolon = strchr(body, ':');
			if (pcolon) { *pcolon++ = 0; }
#if 1
			tvalue = lookup_macro(name, macro_set, ctx);
#else
			tvalue = lookup_macro(name, subsys, macro_set, use);
			if (subsys && ! tvalue)
				tvalue = lookup_macro(name, NULL, macro_set, use);

				// Note that if 'name' has been explicitly set to nothing,
				// tvalue will _not_ be NULL so we will not call
				// lookup_macro_def().  See gittrack #1302
			if ( ! tvalue) {
				if (use_default_param_table) tvalue = param_default_string(name, subsys);
				else tvalue = lookup_macro_def(name, subsys, macro_set, use);
			}
#endif
			if (pcolon && ( ! tvalue || ! tvalue[0])) {
				tvalue = pcolon;
			}
			if ( ! tvalue) tvalue = "";
		}
		break;

		case SPECIAL_MACRO_ID_ENV:
		{
			tvalue = getenv(name);
			if( tvalue == NULL ) {
				//EXCEPT("Can't find %s in environment!",name);
				tvalue = "UNDEFINED";
			}
		}
		break;

		case SPECIAL_MACRO_ID_RANDOM_CHOICE:
		{
			StringList entries(name,",");
			int num_entries = entries.number();
			tvalue = NULL;
			// the the list we are choosing from has only one entry
			// try and use that entry as a macro name.
			if (num_entries == 1) {
				entries.rewind();
				const char * list_name = entries.next();
				if ( ! list_name) {
					EXCEPT( "$RANDOM_CHOICE() config macro: no list!" );
				}

				const char * lval = lookup_macro(list_name, macro_set, ctx);

				// if the first entry resolved to a macro, clear the entries list and
				// repopulate it from the value of the macro.
				if (lval) {
					entries.clearAll(); list_name = NULL;
					// now re-populate the entries list from lval.
					if (strchr(lval, '$')) {
						char * tmp3 = expand_macro(lval, macro_set, ctx);
						if (tmp3) {
							entries.initializeFromString(tmp3);
							free(tmp3);
						}
					} else {
						entries.initializeFromString(lval);
					}
					num_entries = entries.number();
				} else {
					// if the lookup failed, fall through to use the the list name as the list item,
					// we do this for backward compatibility with the original behavior of RANDOM_CHOICE
				}
			}
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
			tvalue = buf = strdup(tvalue);
		}
		break;

		case SPECIAL_MACRO_ID_RANDOM_INTEGER:
		{
			StringList entries(body, ",");

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
			const int cbuf = 20;
			buf = (char*)malloc(cbuf+1);
			snprintf( buf, cbuf, "%ld", random_value );
			buf[cbuf] = '\0';
			tvalue = buf;
		}
		break;

			// the $CHOICE() macro comes in 2 forms
			// $CHOICE(index,list_name) or $CHOICE(index,item1,item2,...)
			//   index can either be an integer, or the macro name of an integer.
			//   list_name must be the macro name of a comma separated list of items.
		case SPECIAL_MACRO_ID_CHOICE:
		{
			// use the version of stringlist that doesn't eat empty fields or extra delimiters.
			StringList entries(body, ',', true);
			entries.rewind();

			const char * index_name = entries.next();
			if ( ! index_name) {
				EXCEPT( "$CHOICE() config macro: no index!" );
			}
			const char * mval = lookup_macro(index_name, macro_set, ctx);
			if ( ! mval) mval = index_name;

			char * tmp2 = NULL;
			if (strchr(mval, '$')) {
				tmp2 = expand_macro(mval, macro_set, ctx);
				mval = tmp2;
			}

			long long index = -1;
			if ( ! string_is_long_param(mval, index) || index < 0 || index >= INT_MAX) {
				EXCEPT( "$CHOICE() macro: %s is invalid index!", mval );
			}

			tvalue = NULL;
			if (entries.number() == 2) {
				const char * list_name = entries.next();
				if ( ! list_name) {
					EXCEPT( "$CHOICE() config macro: no list!" );
				}

				const char * lval = lookup_macro(list_name, macro_set, ctx);
				if ( ! lval) {
					EXCEPT( "$CHOICE() macro: no list named %s!", list_name);
				}

				// now populate the entries list from lval.
				entries.clearAll(); list_name = index_name = NULL;
				if (strchr(lval, '$')) {
					char * tmp3 = expand_macro(lval, macro_set, ctx);
					if (tmp3) {
						entries.initializeFromString(tmp3);
						free(tmp3);
					}
				} else {
					entries.initializeFromString(lval);
				}
				entries.rewind();
			}

			// scan the list looking for an item with the given index
			for (int ii = 0; ii <= (int)index; ++ii) {
				const char * val = entries.next();
				if (val != NULL && ii == index) {
					tvalue = buf = strdup(val);
					break;
				}
			}

			if ( ! tvalue) {
				EXCEPT( "$CHOICE() config macro: index %d is out of range!", (int)index );
			}

			if (tmp2) {free(tmp2);} tmp2 = NULL;
		}
		break;

			// $SUBSTR(name,length) or $SUBSTR(name,start,length)
			// lookup and macro expand name, then extract a substring.
			// negative length values of length mean 'from the end'
		case SPECIAL_MACRO_ID_SUBSTR:
		{
			char * len_arg = NULL;
			char * start_arg = strchr(body, ',');
			if ( ! start_arg) {
				EXCEPT( "$SUBSTR() macro: no length specified!" );
			}

			*start_arg++ = 0;
			len_arg = strchr(start_arg, ',');
			if (len_arg) *len_arg++ = 0;

			int start_pos = 0;
			if (start_arg) {
				const char * arg = lookup_macro(start_arg, macro_set, ctx);
				if ( ! arg) arg = start_arg;

				char * tmp3 = NULL;
				if (strchr(arg, '$')) {
					tmp3 = expand_macro(arg, macro_set, ctx);
					arg = tmp3;
				}

				long long index = -1;
				if ( ! string_is_long_param(arg, index) || index < INT_MIN || index >= INT_MAX) {
					EXCEPT( "$SUBSTR() macro: %s is invalid start index!", arg );
				}
				start_pos = (int)index;
				if (tmp3) {free(tmp3);} tmp3 = NULL;
			}

			int sub_len = INT_MAX/2;
			if (len_arg) {
				const char * arg = lookup_macro(len_arg, macro_set, ctx);
				if ( ! arg) arg = len_arg;

				char * tmp3 = NULL;
				if (strchr(arg, '$')) {
					tmp3 = expand_macro(arg, macro_set, ctx);
					arg = tmp3;
				}

				long long index = -1;
				if ( ! string_is_long_param(arg, index) || index < INT_MIN || index > INT_MAX) {
					EXCEPT( "$SUBSTR() macro: %s is invalid length !", arg );
				}
				sub_len = (int)index;
				if (tmp3) {free(tmp3);} tmp3 = NULL;
			}

			const char * mval = lookup_macro(name, macro_set, ctx);
			if ( ! mval) {
				tvalue = "";
			} else {
				tvalue = NULL;

				if (strchr(mval, '$')) {
					buf = expand_macro(mval, macro_set, ctx);
				} else {
					buf = strdup(mval);
				}

				int cch = strlen(buf);
				// a negative starting pos means measure from the end
				if (start_pos < 0) { start_pos = cch + start_pos; }
				if (start_pos < 0) { start_pos = 0; }
				else if (start_pos > cch) { start_pos = cch; }

				tvalue = buf + start_pos;
				cch -= start_pos;

				// a negative length means measure from the end
				if (sub_len < 0) { sub_len = cch + sub_len; }
				if (sub_len < 0) { sub_len = 0; }
				else if (sub_len > cch) { sub_len = cch; }

				buf[start_pos + sub_len] = 0;
			}
		}
		break;

			// $INT(name) or $INT(name,fmt) or $REAL(name) $REAL(name,fmt)
			// lookup name, macro expand it if necessary, then evaluate it as a int or double
			//
		case SPECIAL_MACRO_ID_INT:
		case SPECIAL_MACRO_ID_REAL:
		{
			char * fmt = strchr(body, ',');
			if (fmt) {
				*fmt++ = 0;
				const char * tmp_fmt = fmt;
				printf_fmt_info fmt_info;
				if ( ! parsePrintfFormat(&tmp_fmt, &fmt_info)
					|| (fmt_info.type == PFT_STRING || fmt_info.type == PFT_RAW || fmt_info.type == PFT_VALUE)
					|| (fmt_info.type == PFT_FLOAT && (special_id == SPECIAL_MACRO_ID_INT))
					|| (fmt_info.type == PFT_INT && (special_id == SPECIAL_MACRO_ID_REAL))
					) {
					EXCEPT( "%s macro: '%s' is not a valid format specifier!",
						(special_id == SPECIAL_MACRO_ID_INT) ? "$INT()" : "$REAL()", fmt);
				}
			}

			const char * mval = lookup_macro(name, macro_set, ctx);
			if ( ! mval) mval = name;
			tvalue = NULL;

			char * tmp2 = NULL;
			if (strchr(mval, '$')) {
				tmp2 = expand_macro(mval, macro_set, ctx);
				mval = tmp2;
			}

			if (special_id == SPECIAL_MACRO_ID_INT) {
				long long int_val = -1;
				if ( ! string_is_long_param(mval, int_val)) {
					EXCEPT( "$INT() macro: %s does not evaluate to an integer!", mval );
				}

				const int cbuf = 56;
				tvalue = buf = (char*)malloc(cbuf+1);
				snprintf( buf, cbuf, fmt ? fmt : "%lld", int_val );
			} else {
				double dbl_val = -1;
				if ( ! string_is_double_param(mval, dbl_val)) {
					EXCEPT( "$REAL() macro: %s does not evaluate to an real!", mval );
				}

				const int cbuf = 56;
				tvalue = buf = (char*)malloc(cbuf+1);
				snprintf( buf, cbuf, fmt ? fmt : "%.16G", dbl_val );
				if (fmt && ! strchr(buf, '.')) { strcat(buf, ".0"); } // force it to look like a real
			}

			if (tmp2) {free(tmp2);} tmp2 = NULL;
		}
		break;

		case SPECIAL_MACRO_ID_DIRNAME:
		case SPECIAL_MACRO_ID_BASENAME:
		case SPECIAL_MACRO_ID_FILENAME:
		{
			const char * mval = lookup_macro(name, macro_set, ctx);
			tvalue = NULL;

			// filename extraction macros, for expanding only parts of a filename in $(FILE).
			// Any macro function of the form $Fqdpnx(FILE), where q,d,p,n,x are all optional
			// and indicate which parts of the filename to keep.
			// q - quote the result (incoming quotes are stripped if this is not specified)
			// d - parent directory
			// p - full directory path
			// n - file basename without extension
			// x - file extension, including the .

			int parts = 0;
			bool quoted = false;
			if (special_id == SPECIAL_MACRO_ID_BASENAME) {
				parts = 1|2;
			} else if (special_id == SPECIAL_MACRO_ID_DIRNAME) {
				parts = 4;
			} else {
				for (const char*p = func; *p != '('; ++p) {
					switch (*p | 0x20) {
					case 'x': parts |= 0x1; break;
					case 'n': parts |= 0x2; break;
					case 'p': parts |= 0x4; break;
					case 'd': parts |= 0x8; break;
					case 'q': quoted = true; break;
					}
				}
			}

			if (mval) {

				buf = strdup_quoted(mval, -1, quoted);  // copy the macro value with quotes add/removed as requested.
				int ixend = strlen(buf); // this will be the end of what we wish to return
				int ixn = (int)(condor_basename(buf) - buf); // index of start of filename, ==0 if no path sep
				int ixx = (int)(condor_basename_extension_ptr(buf+ixn) - buf); // index of . in extension, ==ixend if no ext
				// if this is a bare filename, we can ignore the p & d flags if n or x is set
				if ( ! ixn) { if (parts & (2|1)) parts &= ~(4|8); }

				// set tvalue to start, and ixend to end of text we want to return.
				switch (parts & 0xF)
				{
				case 1:     tvalue = buf+ixx;  break;
				case 2|1:   tvalue = buf+ixn;  break;
				case 2:     tvalue = buf+ixn;  ixend = ixx; break;
				case 0:
				case 4|2|1: tvalue = buf;      break;
				case 4|1:   tvalue = buf;      break; // TODO: fix to strip out filename part?
				case 4:     tvalue = buf; ixend = ixn; break;
				case 4|2:   tvalue = buf; ixend = ixx; break;
				default:
					// ixn is 0 if no dir.
					if (ixn > 0) {
						char ch = buf[ixn-1]; buf[ixn-1] = 0; // truncate filename saving the old character
						tvalue = condor_basename(buf); // tvalue now points to the start of the first directory
						buf[ixn-1] = ch; // put back the dir/filename separator
						if (2 == (parts&3)) { ixend = ixx; }
						else if (0 == (parts&3)) { ixend = ixn; }
						else if (1 == (parts&3)) { /* TODO: strip out filename part? */ }
					} else {
						// we get here when there is no dir, but only dir should be output
						// so we pick a spot inside the buffer with room to quote
						// and set start==end
						ixend = 1;
						tvalue = buf+ixend;
					}
				break;
				}

				// we may have truncated quotes in the switch statement above
				// but if we did we know that buf has room to put them back.
				if (quoted) {
					int ixv = (int)(tvalue-buf);
					if (buf[ixv] != '"') { ASSERT(ixv > 0); buf[--ixv] = '"'; tvalue = buf+ixv; }
					if (ixend > 1 && buf[ixend-1] == '"') --ixend;
					buf[ixend++] = '"';
				}
				// make sure that the end of tvalue is null terminated.
				buf[ixend] = 0;
			}
			if ( ! tvalue) tvalue = "";
		}
		break;

		default:
			EXCEPT("Unknown special config macro %d!", special_id);
		break;
	}

	tbuf.set(buf);
	return tvalue;
}

char *
expand_macro(const char *value,
			 MACRO_SET& macro_set,
			 MACRO_EVAL_CONTEXT & ctx)
{
	char *tmp = strdup( value );
	char *left, *name, *right, *func;
	const char *tvalue;
	char *rval;

	bool all_done = false;
	while ( ! all_done) { // loop until all done expanding
		all_done = true;

		NoDollarBody no_dollar; // prevents $(DOLLAR) from being matched by next_config_macro
		int special_id = next_config_macro(is_config_macro, no_dollar, tmp, 0, &left, &name, &right, &func);
		if (special_id) {
			all_done = false;

			auto_free_ptr tbuf; // malloc or strdup'd buffer (if needed)
			tvalue = evaluate_macro_func(func, special_id, name, tbuf, macro_set, ctx);

			rval = (char *)MALLOC( (unsigned)(strlen(left) + strlen(tvalue) + strlen(right) + 1));
			ASSERT(rval);

			(void)sprintf( rval, "%s%s%s", left, tvalue, right );
			FREE( tmp );
			tmp = rval;
		}
	}

	// Now, deal with the special $(DOLLAR) macro.
	DollarOnlyBody dollar_only; // matches only $(DOLLAR)
	while( next_config_macro(is_config_macro, dollar_only, tmp, 0, &left, &name, &right, &func) ) {
		rval = (char *)MALLOC( (unsigned)(strlen(left) + 1 +
										  strlen(right) + 1));
		ASSERT( rval != NULL );
		(void)sprintf( rval, "%s$%s", left, right );
		FREE( tmp );
		tmp = rval;
	}

	return tmp;
}


// MACRO self expansion ----
class SelfOnlyBody : public ConfigMacroBodyCheck {
public:
	SelfOnlyBody(const char * _self, int _len) : self(_self), self2(NULL), selflen(_len), selflen2(0) {}
	virtual bool skip(int func_id, const char * name, int namelen) {
		if (func_id != MACRO_ID_NORMAL && func_id != SPECIAL_MACRO_ID_FILENAME)
			return true;
		if (namelen == selflen && MATCH == strncasecmp(name, self, namelen))
			return false;
		if (namelen == selflen2 && MATCH == strncasecmp(name, self2, namelen))
			return false;
		return true;
	}
	void set_self2(const char *_self2, int _len2) { self2 = _self2; selflen2 = _len2; }
protected:
	const char * self;
	const char * self2;
	int selflen;
	int selflen2;
};

/*
** Special version of expand_macro that only expands 'self' references. i.e. it only
** expands the macro whose name is specified in the self argument.
** Expand parameter references of the form "left$(self)right".  This
** is deceptively simple, but does handle multiple and or nested references.
** We only expand references to to the parameter specified by self. use expand_macro
** to expand all references. 
*/
char *
expand_self_macro(const char *value,
				 const char *self,
				 MACRO_SET& macro_set,
				 MACRO_EVAL_CONTEXT & ctx)
{
	char *tmp = strdup( value );
	char *rval;

	ASSERT(self != NULL && self[0] != 0);

	SelfOnlyBody only_self(self, strlen(self));

	// to avoid infinite recursive expansion, we have to look for both "prefix.self" and "self"
	// so we want to set selfless equal to the part of self after the prefix.
	const char * prefix = NULL;
	if (ctx.localname) {
		const char * a = ctx.localname;
		const char * b = self;
		while (*a && (tolower(*a) == tolower(*b))) {
			++a; ++b;
		}
		// if a now points to a 0, and b now points to ".", then self contains subsys as a prefix.
		if (0 == a[0] && '.' == b[0] && b[1] != 0) {
			const char *selfless = b+1;
			only_self.set_self2(selfless, strlen(selfless));
			prefix = ctx.localname;
		}
	}
	if (ctx.subsys && ! prefix) {
		const char * a = ctx.subsys;
		const char * b = self;
		while (*a && (tolower(*a) == tolower(*b))) {
			++a; ++b;
		}
		// if a now points to a 0, and b now points to ".", then self contains subsys as a prefix.
		if (0 == a[0] && '.' == b[0] && b[1] != 0) {
			const char *selfless = b+1;
			only_self.set_self2(selfless, strlen(selfless));
			prefix = ctx.subsys;
		}
	}

	bool all_done = false;
	while( !all_done ) { // loop until all done expanding
		all_done = true;

		char *left, *body, *right, *func;
		const char *tvalue;

		int func_id = next_config_macro(is_config_macro, only_self, tmp, 0, &left, &body, &right, &func);
		if (func_id) {
			all_done = false;

			auto_free_ptr tbuf; // malloc or strdup'd buffer (if needed)
			tvalue = evaluate_macro_func(func, func_id, body, tbuf, macro_set, ctx);

			rval = (char *)MALLOC( (unsigned)(strlen(left) + strlen(tvalue) + strlen(right) + 1));
			ASSERT(rval);

			(void)sprintf( rval, "%s%s%s", left, tvalue, right );
			FREE( tmp );
			tmp = rval;
		}
	}

	return( tmp );
}


// param hashtable iteration

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
	if (it.ix >= it.set.size && ((it.opts & HASHITER_NO_DEFAULTS) != 0 || ! it.set.defaults || (it.id >= it.set.defaults->size)))
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


/** Find next $(MACRO) $func(MACRO), $$(MACRO) or $$([expression])
	search begins at pos and continues to terminating null

  check_prefix callback determins with values between $ and ( are permitted
  and converts them into a prefix id which will be the return value of
  the function.  the check_prefix should return 0 if the characters
  between $ and ( are not a valid function

  body_check is a class. this skip method of this class is is passed a
  pointer to the start of the $() body and the length, it should return
  true if the contents of the body should cause this $() macro to be skipped
  during this pass.  This is used to match only $(self) or $(DOLLAR) during
  certain passes and to ignore $$() and $(DOLLAR) during other passes.

- value - The null-terminated string to scan. WILL BE MODIFIED!

- pos - 0-indexed position in value to start scanning at.

- leftp - OUTPUT. *leftp will be set to value+search_pos.  It
	will be null terminated at the first $ for the next $$(MACRO) found.

- namep - OUTPUT. The name of the MACRO (the bit between the
	parenthesis).  Pointer into value.  Null terminated at the
	closing parenthesis.

- rightp - OUTPUT. Everything to the right of the close paren for the $$(MACRO).
	Pointer into value.

- funcp - OUTPUT. points to the char after the first $ of the $( $func( or $$ macro.
	Pointer into value, it is NOT null terminated.

  returns a non-zero macro prefix (function) id if an appropriate $ or $$ macro is found
  returns zero if not found.
*/

/*
** Same as find_config_macro() below, but finds special references like $ENV() as well
** depending on what check_prefix returns when handed the characters between $ and (
*/
int next_config_macro (
	int (*check_prefix)(const char *dollar, int length, MACRO_BODY_CHARS & bodychars),
	ConfigMacroBodyCheck & body_check,
	char *value, int search_pos,
	char **leftp, char **namep, char **rightp,  char**funcp )
{
	char *left, *left_end, *name, *right;
	char *tvalue;
	int prefix_len;
	int prefix_id = 0;
	int after_colon = 0; // if : default is allowed, keeps track of the offset of the : from the start of the macro.

	if ( ! check_prefix ) return 0;

	tvalue = value + search_pos;
	left = value;

	MACRO_BODY_CHARS bodychars = MACRO_BODY_ANYTHING;

		// Loop until we're done, helped with the magic of goto's
	for (;;) {
tryagain:
		// find the next valid $prefix, set value to point to the $
		// prefix_id to the identifier, and prefix_len to it's length.
		prefix_len = 0;
		if (tvalue) {
			// scan for $anyalphanumtext( or $$( and then check to see 
			// if it's a valid prefix. keep scanning til we find a prefix
			// or get to the end of the input.
			for (;;) {
				value = strchr(tvalue, '$');
				if ( ! value) return 0;
				char * p = value+1;
				if (*p == '$') ++p; // permit $$ as well as $ as part of the prefix

				// scan over alphanumeric characters, then if the next character is (
				// we have a potential prefix - call check_prefix to find out.
				while (*p && (isalnum(*p) || *p == '_')) ++p;
				if (*p == '(') {
					prefix_len = (int)(p - value);
					prefix_id = check_prefix(value, prefix_len, bodychars);
					if (prefix_id != 0)
						break;
				}
				tvalue = p;
			}
		}

		if ( ! value) return 0;


		value += prefix_len;
		if( *value == '(' ) {
			left_end = value - prefix_len;
			name = ++value;
			if (bodychars == MACRO_BODY_ANYTHING) {
				while( *value && *value != ')' ) { ++value; }
			} else if (bodychars == MACRO_BODY_IDCHAR_COLON) {
				after_colon = 0;
				while( *value && *value != ')' ) {
					char c = *value++;
					if (c == ':' && ! after_colon) {
						after_colon = (int)(value - name);
						continue;
					} else if (after_colon) {
						if (c == '(') {
							// skip to the close )
							char * ptr = strchr(value, ')');
							if (ptr) value = ptr+1;
						} else if (strchr("$ ,\\", c)) {
							// allow some characters after the : that we don't allow in param names
							continue;
						}
					}
					if ( ! ISIDCHAR(c)) {
						tvalue = name;
						goto tryagain;
					}
				}
			} else if (bodychars == MACRO_BODY_SCAN_BRACKET) {
				char * end_marker = strstr(value, "])");
				if (end_marker == NULL) {
					tvalue = value;
					goto tryagain;
				}
				value = end_marker + 1;
			}

			if( *value == ')' ) {
				if (body_check.skip(prefix_id, name, (int)(value-name))) {
					tvalue = value; // skip over the whole body
					goto tryagain;
				}
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

	*funcp = left_end+1;
	*leftp = left;
	*namep = name;
	*rightp = right;

	return prefix_id;
}

/** Find next $$(MACRO) or $$([expression]) in value
	search begins at pos and continues to terminating null

- value - The null-terminated string to scan. WILL BE MODIFIED!

- pos - 0-indexed position in value to start scanning at.

- left - OUTPUT. *leftp will be set to value+search_pos.  It
	will be null terminated at the first $ for the next $$(MACRO) found.

- name - OUTPUT. The name of the MACRO (the bit between the
	parenthesis).  Pointer into value.  Null terminated at the
	closing parenthesis.

- right - OUTPUT. Everything to the right of the close paren for the $$(MACRO).
	Pointer into value.

returns non-zero if a $$() is found, zero if not found.
*/

static int is_dollardollar_prefix(const char* prefix, int length, MACRO_BODY_CHARS & bodychars)
{
	// prefix of just "$$" is dollardollar macro expansion. 
	if (length == 2 && prefix[1] == '$') {
		bodychars = (prefix[3] == '[') ? MACRO_BODY_SCAN_BRACKET : MACRO_BODY_IDCHAR_COLON;
		return MACRO_ID_DOUBLEDOLLAR;
	}
	return 0; // not a $$ macro
}

class DollarDollarBody : public ConfigMacroBodyCheck {
public:
	virtual bool skip(int /*func_id*/, const char * /*body*/, int /*len*/) {
		// no special skippable bodies
		return false;
	}
};


int next_dollardollar_macro(char * value, int pos, char** left, char** name, char** right)
{
	char * func;
	DollarDollarBody dollardollar_body; // prevents $(DOLLAR) from being matched by next_config_macro
	int id = next_config_macro(is_dollardollar_prefix, dollardollar_body, value, pos, left, name, right, &func);
	return id ? 1 : 0;
}


template <typename T>
int BinaryLookupIndex (const T aTable[], int cElms, const char * key, int (*fncmp)(const char *, const char *))
{
	if (cElms <= 0)
		return -1;

	int ixLower = 0;
	int ixUpper = cElms-1;
	for (;;) {
		if (ixLower > ixUpper)
			return -1; // -1 for not found

		int ix = (ixLower + ixUpper) / 2;
		int iMatch = fncmp(aTable[ix].key, key);
		if (iMatch < 0)
			ixLower = ix+1;
		else if (iMatch > 0)
			ixUpper = ix-1;
		else
			return ix;
	}
}

static int param_default_get_index(const char * name, MACRO_SET & set)
{
	MACRO_DEFAULTS * defs = set.defaults;
	if ( ! defs || ! defs->table)
		return -1;

	return BinaryLookupIndex<const MACRO_DEF_ITEM>(defs->table, defs->size, name, strcasecmp);
}

void param_default_set_use(const char * name, int use, MACRO_SET & set)
{
	MACRO_DEFAULTS * defs = set.defaults;
	if ( ! defs || ! defs->metat)
		return;
	int ix = param_default_get_index(name, set);
	if (ix >= 0) {
		defs->metat[ix].use_count += (use&1);
		defs->metat[ix].ref_count += (use>>1)&1;
	}
}


const MACRO_DEF_ITEM * find_macro_subsys_def_item(const char * name, const char * subsys, MACRO_SET & set, int use)
{
	MACRO_DEF_ITEM * p = NULL;
	// if subsys was passed, first try to lookup in param subsystem overrides table.
	if (set.defaults && set.defaults->table) {
		MACRO_DEF_ITEM * pSubTab = NULL;
		int cSubTab = param_get_subsys_table(set.defaults->table, subsys, &pSubTab);
		if (cSubTab && pSubTab) {
			int ix = BinaryLookupIndex<const MACRO_DEF_ITEM>(pSubTab, cSubTab, name, strcasecmp);
			if (ix >= 0) {
				p = pSubTab + ix;
				if (use) param_default_set_use(name, use, set); 
			}
		}
	}
	return p;
}

const MACRO_DEF_ITEM * find_macro_def_item(const char * name, MACRO_SET & set, int use)
{
	MACRO_DEF_ITEM * p = NULL;

	// if name has a dot in it, then this MIGHT be a subsys.name lookup
	// so try looking up name in the subsys default table. (the lookup with only compare up to the .)
	// if a subsys table is found, then try lookup up the remaineder of the name in that table
	// note that this type of lookup will always fail in the submit macro set.
	const char * pdot = strchr(name, '.');
	if (pdot) {
		MACRO_DEF_ITEM * pSubTab = NULL;
		const char * subsys = name; // no need to null terminate the subsys prefix
		int cSubTab = param_get_subsys_table(set.defaults->table, subsys, &pSubTab);
		if (cSubTab && pSubTab) {
			int ix = BinaryLookupIndex<const MACRO_DEF_ITEM>(pSubTab, cSubTab, pdot+1, strcasecmp);
			if (ix >= 0) {
				p = pSubTab + ix;
				if (use) param_default_set_use(pdot+1, use, set); 
				return p;
			}
		}
	}

	// if we get here, there was no . in the name, or the subsys lookup failed
	// we expect to always end up here for the submit macro set, but not always for
	// the config macro set.

	// do an exact match lookup of the name in the main defaults table.
	int ix = param_default_get_index(name, set);
	if (ix >= 0) {
		if (use && set.defaults && set.defaults->metat) {
			set.defaults->metat[ix].use_count += (use&1);
			set.defaults->metat[ix].ref_count += (use>>1)&1;
		}
		if (set.defaults && set.defaults->table) {
			p = &set.defaults->table[ix];
		}
	}
	return p;
}

