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
#include "condor_debug.h"
#include "condor_arglist.h"
#include "condor_string.h"
#include "condor_attributes.h"

void append_arg(char const *arg, std::string &result) {
	if(result.length()) {
		result += " ";
	}
	ASSERT(arg);
	if(!*arg) {
		result += "''"; //empty arg
	}
	while(*arg) {
		switch(*arg) {
		case ' ':
		case '\t':
		case '\n':
		case '\r':
		case '\'':
			if(result.length() && result[result.length()-1] == '\'') {
				//combine preceeding quoted section with this one,
				//so we do not introduce a repeated quote.
				result.erase(result.length()-1);
			}
			else {
				result += '\'';
			}
			if(*arg == '\'') {
				result += '\''; //repeat the quote to escape it
			}
			result += *(arg++);
			result += '\'';
			break;
		default:
			result += *(arg++);
		}
	}
}

void
join_args(std::vector<std::string> const & args_list, std::string & result, size_t start_arg)
{
	size_t i = 0;
	for (auto& arg : args_list) {
		if(i++<start_arg) continue;
		append_arg(arg.c_str(), result);
	}
}

void join_args(char const * const *args_array,std::string& result,size_t start_arg) {
	if(!args_array) return;
	for(size_t i=0;args_array[i];i++) {
		if(i<start_arg) continue;
		append_arg(args_array[i], result);
	}
}

bool split_args(
  char const *args,
  std::vector<std::string>& args_list,
  std::string* error_msg)
{
	std::string buf = "";
	bool parsed_token = false;

	if(!args) return true;

	while(*args) {
		switch(*args) {
		case '\'': {
			char const *quote = args++;
			parsed_token = true;
			while(*args) {
				if(*args == *quote) {
					if(args[1] == *quote) {
						// This is a repeated quote, which we treat as an
						// escape mechanism for quotes.
						buf += *(args++);
						args++;
					}
					else {
						break;
					}
				}
				else {
					buf += *(args++);
				}
			}
			if(!*args) {
				if(error_msg) {
					formatstr(*error_msg, "Unbalanced quote starting here: %s", quote);
				}
				return false;
			}
			args++; //eat the closing quote
			break;
		}
		case ' ':
		case '\t':
		case '\n':
		case '\r': {
			args++; // eat whitespace
			if(parsed_token) {
				parsed_token = false;
				args_list.emplace_back(buf);
				buf = "";
			}
			break;
		}
		default:
			parsed_token = true;
			buf += *(args++);
			break;
		}
	}
	if(parsed_token) {
		args_list.emplace_back(buf);
	}
	return true;
}

static char **
ArgListToArgsArray(std::vector<std::string> const &args_list)
{
	size_t i = 0;
	char **args_array = (char **)malloc((args_list.size()+1)*sizeof(char*));
	ASSERT(args_array);
	for (const auto& arg: args_list) {
		args_array[i] = strdup(arg.c_str());
		ASSERT(args_array[i]);
		i++;
	}
	args_array[i] = NULL;
	return args_array;
}

bool split_args(char const *args, char ***args_array, std::string* error_msg) {
	std::vector<std::string> args_list;
	if(!split_args(args,args_list,error_msg)) {
		*args_array = NULL;
		return false;
	}

	*args_array = ArgListToArgsArray(args_list);
	return *args_array != NULL;
}

void
deleteStringArray(char **array)
{
	if(!array) return;
	for(int i=0;array[i];i++) {
		free(array[i]);
	}
	free(array);
}


size_t
ArgList::Count() const {
	return args_list.size();
}
void
ArgList::Clear() {
	args_list.clear();
	input_was_unknown_platform_v1 = false;
}

char const *
ArgList::GetArg(size_t n) const {
	if (n >= args_list.size()) {
		return nullptr;
	} else {
		return args_list[n].c_str();
	}
}

void
ArgList::AppendArg(const std::string &arg) {
	args_list.emplace_back(arg);
}

void
ArgList::AppendArg(char const *arg) {
	ASSERT(arg);
	args_list.emplace_back(arg);
}

void
ArgList::RemoveArg(size_t pos) {
	if (pos < args_list.size()) {
		args_list.erase(args_list.begin()+pos);
	}
}

void
ArgList::InsertArg(char const *arg, size_t pos) {
	ASSERT(pos <= Count());

	args_list.insert(args_list.begin()+pos, arg);
}

char **
ArgList::GetStringArray() const {
	return ArgListToArgsArray(args_list);
}

bool
ArgList::AppendArgsV1RawOrV2Quoted(char const *args,std::string & error_msg)
{
	if(IsV2QuotedString(args)) {
			// This is actually a V2Quoted string (enclosed in double-quotes).
		std::string v2;
		if(!V2QuotedToV2Raw(args,v2,error_msg)) {
			return false;
		}
		return AppendArgsV2Raw(v2.c_str(),error_msg);
	}

		// It is a raw V1 input string, not enclosed in double-quotes.

	return AppendArgsV1Raw(args,error_msg);
}

bool
ArgList::AppendArgsV1WackedOrV2Quoted(char const *args, std::string& error_msg)
{
	if(IsV2QuotedString(args)) {
			// This is actually a V2Quoted string (enclosed in double-quotes).
		std::string v2;
		if(!V2QuotedToV2Raw(args,v2,error_msg)) {
			return false;
		}
		return AppendArgsV2Raw(v2.c_str(),error_msg);
	}

		// It is a V1Wacked string.  Literal double-quotes are
		// backwacked.
	std::string v1;
	if(!V1WackedToV1Raw(args,v1,error_msg)) {
		return false;
	}

	return AppendArgsV1Raw(v1.c_str(),error_msg);
}

bool
ArgList::AppendArgsV2Quoted(char const *args, std::string & error_msg)
{
	if(!IsV2QuotedString(args)) {
		AddErrorMessage("Expecting double-quoted input string (V2 format).",error_msg);
		return false;
	}

	std::string v2;
	if(!V2QuotedToV2Raw(args,v2,error_msg)) {
		return false;
	}
	return AppendArgsV2Raw(v2.c_str(),error_msg);
}

bool
ArgList::AppendArgsV1Raw_win32(char const *args,std::string& error_msg)
{
	// Parse an args string in the format expected by the Windows
	// function CommandLineToArgv().

	while(*args) {
		char const *begin_arg = args;
		std::string buf = "";
		while(*args) {
			if(*args == ' ' || *args == '\t' || \
			   *args == '\n' || *args == '\r') {
				break;
			}
			else if(*args != '"') {
				buf += *(args++);
			}
			else {
					// quoted section
				char const *begin_quote = args;
				args++; //begin quote

				while(*args) {
					int backslashes = 0;
					while(*args == '\\') {
						backslashes++;
						args++;
					}
					if(backslashes && *args == '"') {
							//2n backslashes followed by quote
							// --> n backslashes
							//2n+1 backslashes followed by quote
							// --> n backslashes followed by quote
						while(backslashes >= 2) {
							backslashes -= 2;
							buf += '\\';
						}
						if(backslashes) {
							buf += *(args++); //literal quote
						}
						else {
							break; //terminal quote
						}
					}
					else if(backslashes) {
							//literal backslashes
						while(backslashes--) {
							buf += '\\';
						}
					}
					else if(*args == '"') {
						break; //terminal quote
					}
					else {
						buf += *(args++);
					}
				}

				if(*args != '"') {
					std::string msg;
					formatstr(msg, "Unterminated quote in windows argument string starting here: %s",begin_quote);
					AddErrorMessage(msg.c_str(),error_msg);
					return false;
				}
				args++;
			}
		}
		if(args > begin_arg) {
			args_list.emplace_back(buf);
		}
		while(*args == ' ' || *args == '\t' || *args == '\n' || *args == '\r')
		{
			args++;
		}
	}
	return true;
}

bool
ArgList::AppendArgsV1Raw_unix(char const *args,std::string&  /*error_msg*/)
{
	std::string buf = "";
	bool parsed_token = false;

	while(*args) {
		switch(*args) {
		case ' ':
		case '\t':
		case '\n':
		case '\r': {
			args++; // eat whitespace
			if(parsed_token) {
				parsed_token = false;
				args_list.emplace_back(buf);
				buf = "";
			}
			break;
		}
		default:
			parsed_token = true;
			buf += *(args++);
			break;
		}
	}
	if(parsed_token) {
		args_list.emplace_back(buf);
	}
	return true;
}

bool
ArgList::AppendArgsV1Raw(char const *args, std::string &error_msg)
{
	if(!args) return true;

	if(v1_syntax == WIN32_ARGV1_SYNTAX) {
		return AppendArgsV1Raw_win32(args,error_msg);
	}
	else if(v1_syntax == UNIX_ARGV1_SYNTAX) {
		return AppendArgsV1Raw_unix(args,error_msg);
	}
	else if(v1_syntax == UNKNOWN_ARGV1_SYNTAX) {

		// We don't know yet what the final v1_syntax will be, so just
		// slurp in the args by splitting on whitespace (unix
		// style).  Since we are setting the
		// input_was_unknown_platform_v1 flag, any attempt to
		// reconstruct the arguments will apply this same
		// interpretation, so we get back what we started with
		// and will choke on any other arguments that are
		// not expressible in this general V1 syntax (i.e. args
		// containing spaces).  Most argument manipulation happens
		// once we know the final platform (e.g. in the starter),
		// so we should only get here in cases where we don't yet
		// know the final platform (e.g. condor_submit) or haven't
		// bothered to specify it, because we don't add any
		// additional arguments and therefore don't need to worry
		// about how the args are interpreted yet.

		input_was_unknown_platform_v1 = true;
		return AppendArgsV1Raw_unix(args,error_msg);
	}
	else {
		EXCEPT("Unexpected v1_syntax=%d in AppendArgsV1Raw",v1_syntax);
	}
	return false;
}

bool
ArgList::AppendArgsV2Raw(char const *args,std::string &error_msg)
{
	return split_args(args, args_list, &error_msg);
}

void
ArgList::AppendArgsFromArgList(ArgList const &args)
{
	input_was_unknown_platform_v1 = args.input_was_unknown_platform_v1;

	for (const auto& arg: args.args_list) {
		AppendArg(arg);
	}
}

void
ArgList::GetArgsStringForDisplay(ClassAd const *ad, std::string &result)
{
	if( ! ad->LookupString(ATTR_JOB_ARGUMENTS2, result) ) {
		ad->LookupString(ATTR_JOB_ARGUMENTS1, result);
	}
}

bool
ArgList::AppendArgsFromClassAd(ClassAd const * ad, std::string & error_msg)
{
	std::string args1;
	std::string args2;
	bool success = false;

	if( ad->LookupString(ATTR_JOB_ARGUMENTS2, args2) == 1 ) {
		success = AppendArgsV2Raw(args2.c_str(),error_msg);
	}
	else if( ad->LookupString(ATTR_JOB_ARGUMENTS1, args1) == 1 ) {
		success = AppendArgsV1Raw(args1.c_str(),error_msg);
	}
	else {
			// this shouldn't be considered an error... maybe the job
			// just doesn't define any args.  condor_submit always
			// adds an empty string, but we should't rely on that.
		success = true;
	}

	return success;
}

bool
ArgList::CondorVersionRequiresV1(CondorVersionInfo const &condor_version)
{
		// Is it earlier than 6.7.15?
	return !condor_version.built_since_version(6,7,15);
}

bool
ArgList::InsertArgsIntoClassAd( ClassAd * ad, CondorVersionInfo * condor_version, std::string & error_msg) const
{
	bool has_args1 = ad->LookupExpr(ATTR_JOB_ARGUMENTS1) != NULL;
	bool has_args2 = ad->LookupExpr(ATTR_JOB_ARGUMENTS2) != NULL;

	bool requires_v1 = false;
	bool condor_version_requires_v1 = false;
	if(condor_version) {
		requires_v1 = CondorVersionRequiresV1(*condor_version);
		condor_version_requires_v1 = true;
	}
	else if(input_was_unknown_platform_v1) {
		requires_v1 = true;
	}

	if( !requires_v1 )
	{
		std::string args2;
		if(!GetArgsStringV2Raw(args2)) return false;
		ad->Assign(ATTR_JOB_ARGUMENTS2,args2.c_str());
	}
	else if(has_args2) {
		ad->Delete(ATTR_JOB_ARGUMENTS2);
	}

	if(requires_v1) {
		std::string args1;

		if(GetArgsStringV1Raw(args1,error_msg)) {
			ad->Assign(ATTR_JOB_ARGUMENTS1,args1.c_str());
		}
		else {
			if(condor_version_requires_v1 && !input_was_unknown_platform_v1) {
				// We failed to convert to V1 syntax, but otherwise we could
				// have converted to V2 syntax.
				// Rather than failing outright, simply remove
				// all arguments from the ClassAd, which will
				// cause failures in older versions of Condor.
				// We get here, for example, when the schedd is
				// generating the expanded ad to send to an older
				// starter that does not understand V2 syntax.
				// The end result in this case is to fail when the
				// starter tries to read the arguments, and then
				// we go back and get a new match, which hopefully
				// is a machine that understands V2 syntax.  A more
				// direct mechanism would be nice, but this one should
				// at least prevent incorrect behavior.

				ad->Delete(ATTR_JOB_ARGUMENTS1);
				ad->Delete(ATTR_JOB_ARGUMENTS2);
				dprintf(D_FULLDEBUG,"Failed to convert arguments to V1 syntax: %s\n",error_msg.c_str());
			}
			else {
				// Failed to convert to V1 syntax, and the ad does not
				// already contain V2 syntax, so we should assume the
				// worst.
				AddErrorMessage("Failed to convert arguments to V1 syntax.",error_msg);
				return false;
			}
		}
	}
	else if(has_args1) {
		ad->Delete(ATTR_JOB_ARGUMENTS1);
	}
	return true;
}

bool
ArgList::IsSafeArgV1Value(char const *str) const
{
	// This is used to detect whether the given environment value
	// is unexpressable in the old environment syntax (before environment2).

	if(!str) return false;

	const char *specials = " \t\r\n";

	size_t safe_length = strcspn(str,specials);

	// If safe_length goes to the end of the string, we are okay.
	return !str[safe_length];
}

bool
ArgList::GetArgsStringV1Raw(std::string & result, std::string & error_msg) const
{
	for (const auto& arg: args_list) {
		if(!IsSafeArgV1Value(arg.c_str())) {
			formatstr(error_msg,"Cannot represent '%s' in V1 arguments syntax.",arg.c_str());
			return false;
		}
		if(result.length()) {
			result += " ";
		}
		result += arg;
	}
	return true;
}

bool
ArgList::GetArgsStringV2Quoted(std::string& result, std::string& /*error_msg*/) const
{
	std::string v2_raw;
	if(!GetArgsStringV2Raw(v2_raw)) {
		return false;
	}
	V2RawToV2Quoted(v2_raw,result);
	return true;
}

bool
ArgList::GetArgsStringV1WackedOrV2Quoted(std::string& result, std::string& error_msg) const
{
	std::string v1_raw;
	std::string ignore_err;
	if(GetArgsStringV1Raw(v1_raw, ignore_err)) {
		V1RawToV1Wacked(v1_raw,result);
		return true;
	}
	else {
		return GetArgsStringV2Quoted(result,error_msg);
	}
}

void
ArgList::V2RawToV2Quoted(std::string const &v2_raw, std::string& result)
{
	formatstr_cat(result, "\"%s\"", EscapeChars(v2_raw, "\"", '\"').c_str());
}

void
ArgList::V1RawToV1Wacked(std::string const &v1_raw, std::string& result)
{
	result += EscapeChars(v1_raw, "\"", '\\');
}

bool
ArgList::GetArgsStringV2Raw(std::string & result, size_t start_arg) const
{
	join_args(args_list, result, start_arg);
	return true;
}

void
ArgList::GetArgsStringForDisplay(std::string & result, size_t start_arg) const
{
	GetArgsStringV2Raw(result, start_arg);
}


// Separate arguments with a space.  Replace whitespace in each argument
// with their C-style escapes.
void ArgList::GetArgsStringForLogging( std::string & result ) const {

	for (const auto& msArg: args_list) {
		const char * arg = msArg.c_str();

		if( result.length() != 0 ) { result += " "; }
		while( * arg ) {
			switch( * arg ) {
				case ' ':
					result += "\\ ";
					break;
				case '\t':
					result += "\\t";
					break;
				case '\v':
					result += "\\v";
					break;
				case '\n':
					result += "\\n";
					break;
				case '\r':
					result += "\\r";
					break;
				default:
					result += *arg;
					break;
			}
			++arg;
		}
	}
}

ArgList::ArgList()
{
	input_was_unknown_platform_v1 = false;
	v1_syntax = UNKNOWN_ARGV1_SYNTAX;
}

ArgList::~ArgList()
{
}

bool
ArgList::GetArgsStringWin32(std::string& result, size_t skip_args) const
{
	size_t i = 0;
	for (const auto& arg: args_list) {
		if (i++ < skip_args) continue;
		if(result.length()) result += ' ';
		if(input_was_unknown_platform_v1) {
			// In V1 arg syntax, we just pass on whatever the user entered
			// directly to the Windows OS, assuming the user wants the
			// OS to interpret whatever quotes, backslashes, etc., that
			// are there.
			result += arg;
		}
		else {
			// In V2 arg syntax, we encode arguments in a way that should
			// be parsed correctly by Windows function CommandLineToArgv().

			char const *argstr = arg.c_str();
			if(argstr[strcspn(argstr," \t\"")] == '\0') {
				// No special characters in the argument.
				result += arg;
			}
			else {
				// Special characters, so we need to quote it.

				result += '\"';

				while(*argstr) {
					if(*argstr == '\\') {
						int n = 0;
						while(*argstr == '\\') {
							n++;
							result += *(argstr++);
						}
						if(*argstr == '"' || *argstr == '\0') {
							// To produce n backslashes followed by a
							// literal quote, put 2n+1 backslashes
							// followed by a quote.  To produce n
							// backslashes before the terminal quote,
							// put 2n backslashes.
							while(n--) {
								result += '\\';
							}
							if(*argstr == '"') {
								result += '\\';
								result += *(argstr++);
							}
						}
					}
					else if(*argstr == '"') {
						result += '\\';
						result += *(argstr++);
					}
					else {
						result += *(argstr++);
					}
				}

				result += '\"';
			}
		}
	}
	return true;
}

bool
ArgList::IsV2QuotedString(char const *str)
{
	if(!str) return false;

		// allow leading whitespace
	while(isspace(*str)) {
		str++;
	}

	return *str == '"';
}

bool
ArgList::V2QuotedToV2Raw(char const *v1_input, std::string& v2_raw, std::string& errmsg)
{
	if(!v1_input) return true;

		// allow leading whitespace
	while(isspace(*v1_input)) {
		v1_input++;
	}

	ASSERT(IsV2QuotedString(v1_input));
	ASSERT(*v1_input == '"');
	v1_input++;

	const char *quote_terminated = NULL;
	while(*v1_input) {
		if(*v1_input == '"') {
			v1_input++;
			if(*v1_input == '"') {
					// Repeated (i.e. escaped) double-quote.
				v2_raw += *(v1_input++);
			}
			else {
				quote_terminated = v1_input-1;
				break;
			}
		}
		else {
			v2_raw += *(v1_input++);
		}
	}

	if(!quote_terminated) {
		AddErrorMessage("Unterminated double-quote.",errmsg);
		return false;
	}

		// allow trailing whitespace
	while(isspace(*v1_input)) {
		v1_input++;
	}

	if(*v1_input) {
		std::string msg;
		formatstr(msg,
			"Unexpected characters following double-quote.  "
			"Did you forget to escape the double-quote by repeating it?  "
			"Here is the quote and trailing characters: %s\n",quote_terminated);
		AddErrorMessage(msg.c_str(), errmsg);
		return false;
	}
	return true;
}

bool
ArgList::V1WackedToV1Raw(char const *v1_input, std::string& v1_raw, std::string& errmsg)
{
	if(!v1_input) return true;
	ASSERT(!IsV2QuotedString(v1_input));

	while(*v1_input) {
		if(*v1_input == '"') {
			std::string msg;
			formatstr(msg, "Found illegal unescaped double-quote: %s",v1_input);
			AddErrorMessage(msg.c_str(),errmsg);
			return false;
		}
		else if(v1_input[0] == '\\' && v1_input[1] == '"') {
			// Escaped double-quote.
			v1_input++;
			v1_raw += *(v1_input++);
		}
		else {
			v1_raw += *(v1_input++);
		}
	}
	return true;
}

void
ArgList::SetArgV1Syntax(ArgV1Syntax v1_syntax_param)
{
	this->v1_syntax = v1_syntax_param;
}

void
ArgList::SetArgV1SyntaxToCurrentPlatform()
{
#ifdef WIN32
	v1_syntax = WIN32_ARGV1_SYNTAX;
#else
	v1_syntax = UNIX_ARGV1_SYNTAX;
#endif
}

bool
ArgList::GetArgsStringSystem(std::string & result, size_t skip_args) const
{
#ifdef WIN32
	return GetArgsStringWin32(result,skip_args);
#else
	size_t i = 0;
	for (const auto& arg: args_list) {
		if (i++ < skip_args) continue;
		formatstr_cat(result, "%s\"%s\"",
							result.empty() ? "" : " ",
							EscapeChars(arg,"\"\\$`",'\\').c_str());
	}
	return true;
#endif
}
