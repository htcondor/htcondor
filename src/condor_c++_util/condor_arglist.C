/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_arglist.h"
#include "condor_string.h"
#include "condor_attributes.h"

void append_arg(char const *arg,MyString &result) {
	if(result.Length()) {
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
			if(result.Length() && result[result.Length()-1] == '\'') {
				//combine preceeding quoted section with this one,
				//so we do not introduce a repeated quote.
				result.setChar(result.Length()-1,'\0');
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

void join_args(SimpleList<MyString> const &args_list,MyString *result,int start_arg)
{
	SimpleListIterator<MyString> it(args_list);
	ASSERT(result);
	MyString *arg=NULL;
	for(int i=0;it.Next(arg);i++) {
		if(i<start_arg) continue;
		append_arg(arg->Value(),*result);
	}
}

void join_args(char const * const *args_array,MyString *result,int start_arg) {
	ASSERT(result);
	if(!args_array) return;
	for(int i=0;args_array[i];i++) {
		if(i<start_arg) continue;
		append_arg(args_array[i],*result);
	}
}

bool split_args(
  char const *args,
  SimpleList<MyString> *args_list,
  MyString *error_msg)
{
	MyString buf = "";
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
					error_msg->sprintf("Unbalanced quote starting here: %s",quote);
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
				ASSERT(args_list->Append(buf));
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
		args_list->Append(buf);
	}
	return true;
}

static char **
ArgListToArgsArray(SimpleList<MyString> const &args_list)
{
	SimpleListIterator<MyString> it(args_list);
	MyString *arg;
	int i;
	char **args_array = new char *[args_list.Number()+1];
	ASSERT(args_array);
	for(i=0;it.Next(arg);i++) {
		args_array[i] = strnewp(arg->Value());
		ASSERT(args_array[i]);
	}
	args_array[i] = NULL;
	return args_array;
}

bool split_args(char const *args,char ***args_array,MyString *error_msg) {
	SimpleList<MyString> args_list;
	if(!split_args(args,&args_list,error_msg)) {
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
		delete [] array[i];
	}
	delete [] array;
}


int
ArgList::Count() const {
	return args_list.Number();
}
void
ArgList::Clear() {
	args_list.Clear();
}

char const *
ArgList::GetArg(int n) const {
	SimpleListIterator<MyString> it(args_list);
	MyString *arg;
	int i;
	for(i=0;it.Next(arg);i++) {
		if(i == n) return arg->Value();
	}
	return NULL;
}

void
ArgList::AppendArg(char const *arg) {
	ASSERT(arg);
	ASSERT(args_list.Append(arg));
}

void
ArgList::AppendArg(int arg) {
	char buf[100];
	sprintf(buf,"%d",arg);
	AppendArg(buf);
}

void
ArgList::RemoveArg(int pos) {
	MyString *arg;
	ASSERT(pos >= 0 && pos < Count());
	args_list.Rewind();
	for(int i=0;i <= pos;i++) {
		args_list.Next(arg);
	}
	args_list.DeleteCurrent();
}

void
ArgList::InsertArg(char const *arg,int pos) {
	ASSERT(pos >= 0 && pos <= Count());

	int i;
	char **args_array = GetStringArray();

	args_list.Clear();
	for(i=0;args_array[i];i++) {
		if(i == pos) {
			args_list.Append(arg);
		}
		args_list.Append(args_array[i]);
	}
	if(i == pos) {
		args_list.Append(arg);
	}

	deleteStringArray(args_array);
}

char **
ArgList::GetStringArray() const {
	return ArgListToArgsArray(args_list);
}

bool
ArgList::AppendArgsV1RawOrV2Quoted(char const *args,MyString *error_msg)
{
	if(IsV2QuotedString(args)) {
			// This is actually a V2Quoted string (enclosed in double-quotes).
		MyString v2;
		if(!V2QuotedToV2Raw(args,&v2,error_msg)) {
			return false;
		}
		return AppendArgsV2Raw(v2.Value(),error_msg);
	}

		// It is a raw V1 input string, not enclosed in double-quotes.

	return AppendArgsV1Raw(args,error_msg);
}

bool
ArgList::AppendArgsV1WackedOrV2Quoted(char const *args,MyString *error_msg)
{
	if(IsV2QuotedString(args)) {
			// This is actually a V2Quoted string (enclosed in double-quotes).
		MyString v2;
		if(!V2QuotedToV2Raw(args,&v2,error_msg)) {
			return false;
		}
		return AppendArgsV2Raw(v2.Value(),error_msg);
	}

		// It is a V1Wacked string.  Literal double-quotes are
		// backwacked.
	MyString v1;
	if(!V1WackedToV1Raw(args,&v1,error_msg)) {
		return false;
	}

	return AppendArgsV1Raw(v1.Value(),error_msg);
}

bool
ArgList::AppendArgsV2Quoted(char const *args,MyString *error_msg)
{
	if(!IsV2QuotedString(args)) {
		AddErrorMessage("Expecting double-quoted input string (V2 format).",error_msg);
		return false;
	}

	MyString v2;
	if(!V2QuotedToV2Raw(args,&v2,error_msg)) {
		return false;
	}
	return AppendArgsV2Raw(v2.Value(),error_msg);
}

bool
ArgList::AppendArgsV1Raw(char const *args,MyString *error_msg)
{
	MyString buf = "";
	bool parsed_token = false;

	if(!args) return true;

	input_was_v1 = true;

	while(*args) {
		switch(*args) {
		case ' ':
		case '\t':
		case '\n':
		case '\r': {
			args++; // eat whitespace
			if(parsed_token) {
				parsed_token = false;
				ASSERT(args_list.Append(buf));
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
		args_list.Append(buf);
	}
	return true;
}

bool
ArgList::AppendArgsV2Raw(char const *args,MyString *error_msg)
{
	return split_args(args,&args_list,error_msg);
}

// It is not possible for raw V1 argument strings with a leading space
// to be specified in submit files, so we can use this to mark
// V2 strings when we need to pack V1 and V2 through the same
// channel (e.g. shadow-starter communication).
const char RAW_V2_ARGS_MARKER = ' ';

bool
ArgList::AppendArgsV1or2Raw(char const *args,MyString *error_msg)
{
	if(!args) return true;
	if(*args == RAW_V2_ARGS_MARKER) {
		return AppendArgsV2Raw(args+1,error_msg);
	}
	else {
		return AppendArgsV1Raw(args,error_msg);
	}
}

void
ArgList::AppendArgsFromArgList(ArgList const &args)
{
	input_was_v1 = args.input_was_v1;

	SimpleListIterator<MyString> it(args.args_list);
	MyString *arg=NULL;
	while(it.Next(arg)) {
		AppendArg(arg->Value());
	}
}

void
ArgList::GetArgsStringForDisplay(ClassAd const *ad,MyString *result)
{
	char *args1 = NULL;
	char *args2 = NULL;
	ASSERT(result);
	if( ad->LookupString(ATTR_JOB_ARGUMENTS2, &args2) == 1 ) {
		(*result) = args2;
	}
	else if( ad->LookupString(ATTR_JOB_ARGUMENTS1, &args1) == 1 ) {
		(*result) = args1;
	}
	if(args1) {
		free(args1);
	}
	if(args2) {
		free(args2);
	}
}

bool
ArgList::AppendArgsFromClassAd(ClassAd const *ad,MyString *error_msg)
{
	char *args1 = NULL;
	char *args2 = NULL;
	bool success = false;

	if( ad->LookupString(ATTR_JOB_ARGUMENTS2, &args2) == 1 ) {
		success = AppendArgsV2Raw(args2,error_msg);
		input_was_v1 = false;
	}
	else if( ad->LookupString(ATTR_JOB_ARGUMENTS1, &args1) == 1 ) {
		success = AppendArgsV1Raw(args1,error_msg);
		input_was_v1 = true;
	}
	else {
			// this shouldn't be considered an error... maybe the job
			// just doesn't define any args.  condor_submit always
			// adds an empty string, but we should't rely on that.
		success = true;
			// leave input_was_v1 untouched... (at dan's recommendation)
	}

	if(args1) free(args1);
	if(args2) free(args2);

	return success;
}

bool
ArgList::CondorVersionRequiresV1(CondorVersionInfo const &condor_version)
{
		// Is it earlier than 6.7.15?
	return !condor_version.built_since_version(6,7,15);
}

bool
ArgList::InsertArgsIntoClassAd(ClassAd *ad,CondorVersionInfo *condor_version,MyString *error_msg)
{
	bool has_args1 = ad->Lookup(ATTR_JOB_ARGUMENTS1) != NULL;
	bool has_args2 = ad->Lookup(ATTR_JOB_ARGUMENTS2) != NULL;

	bool requires_args1 = false;
	if(condor_version) {
		requires_args1 = CondorVersionRequiresV1(*condor_version);
	}
	else if(input_was_v1) {
		requires_args1 = true;
	}

	if(requires_args1) {
		if(has_args2) {
			ad->Delete(ATTR_JOB_ARGUMENTS2);
		}
	}

	if( (has_args2 || !has_args1) && !requires_args1)
	{
		MyString args2;
		if(!GetArgsStringV2Raw(&args2,error_msg)) return false;
		ad->Assign(ATTR_JOB_ARGUMENTS2,args2.Value());
	}
	if(has_args1 || requires_args1) {
		MyString args1;

		if(GetArgsStringV1Raw(&args1,error_msg)) {
			ad->Assign(ATTR_JOB_ARGUMENTS1,args1.Value());
		}
		else {
			if(has_args2) {
				// We failed to convert to V1 syntax, but we started
				// with V2, so this is a special kind of failure.
				// Rather than failing outright, simply remove
				// all arguments from the ClassAd, which will
				// cause failures in subsequent AppendArgsFromClassAd().
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
				if(error_msg) {
					dprintf(D_FULLDEBUG,"Failed to convert arguments to V1 syntax: %s\n",error_msg->Value());
				}
			}
			else {
				// Failed to convert to V1 syntax, and the ad does not
				// already contain V2 syntax, so we should assume the
				// worst.
				AddErrorMessage("Failed to convert to target arguments syntax.",error_msg);
				return false;
			}
		}
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
ArgList::GetArgsStringV1Raw(MyString *result,MyString *error_msg) const
{
	SimpleListIterator<MyString> it(args_list);
	MyString *arg=NULL;
	ASSERT(result);
	while(it.Next(arg)) {
		if(!IsSafeArgV1Value(arg->Value())) {
			if(error_msg) {
				error_msg->sprintf("Cannot represent '%s' in V1 arguments syntax.",arg->Value());
			}
			return false;
		}
		if(result->Length()) {
			(*result) += " ";
		}
		(*result) += arg->Value();
	}
	return true;
}

bool
ArgList::GetArgsStringV2Quoted(MyString *result,MyString *error_msg)
{
	MyString v2_raw;
	if(!GetArgsStringV2Raw(&v2_raw,error_msg)) {
		return false;
	}
	V2RawToV2Quoted(v2_raw,result);
	return true;
}

bool
ArgList::GetArgsStringV1WackedOrV2Quoted(MyString *result,MyString *error_msg)
{
	MyString v1_raw;
	if(GetArgsStringV1Raw(&v1_raw,NULL)) {
		V1RawToV1Wacked(v1_raw,result);
		return true;
	}
	else {
		return GetArgsStringV2Quoted(result,error_msg);
	}
}

void
ArgList::V2RawToV2Quoted(MyString const &v2_raw,MyString *result)
{
	result->sprintf_cat("\"%s\"",v2_raw.EscapeChars("\"",'\"').Value());
}

void
ArgList::V1RawToV1Wacked(MyString const &v1_raw,MyString *result)
{
	(*result) += v1_raw.EscapeChars("\"",'\\');
}

bool
ArgList::GetArgsStringV2Raw(MyString *result,MyString *error_msg,int start_arg) const
{
	join_args(args_list,result,start_arg);
	return true;
}

void
ArgList::GetArgsStringForDisplay(MyString *result,int start_arg) const
{
	GetArgsStringV2Raw(result,NULL);
}


bool
ArgList::GetArgsStringV1or2Raw(MyString *result,MyString *error_msg) const
{
	ASSERT(result);
	int old_len = result->Length();

	if(GetArgsStringV1Raw(result,NULL)) {
		return true;
	}

	// V1 attempt failed.  Use V2 syntax.

	if(result->Length() > old_len) {
		// Clear any partial output we may have generated above.
		result->setChar(old_len,'\0');
	}

	(*result) += RAW_V2_ARGS_MARKER;
	return GetArgsStringV2Raw(result,error_msg);
}

bool
ArgList::GetArgsStringV1or2Raw(ClassAd const *ad,MyString *result,MyString *error_msg)
{
	if(!AppendArgsFromClassAd(ad,error_msg)) {
		return false;
	}
	return GetArgsStringV1or2Raw(result,error_msg);
}

void
ArgList::AddErrorMessage(char const *msg,MyString *error_buf)
{
	if(!error_buf) return;

	if(error_buf->Length()) {
		// each message is separated by a newline
		(*error_buf) += "\n";
	}
	(*error_buf) += msg;
}

ArgList::ArgList()
{
	input_was_v1 = false;
}

ArgList::~ArgList()
{
}

bool
ArgList::GetArgsStringWin32(MyString *result,int skip_args,MyString *error_msg) const
{
	ASSERT(result);
	SimpleListIterator<MyString> it(args_list);
	MyString *arg = NULL;
	int i;
	for(i=0;it.Next(arg);i++) {
		if(i<skip_args) continue;
		if(result->Length()) (*result) += ' ';
		if(input_was_v1) {
			// In V1 arg syntax, we just pass on whatever the user entered
			// directly to the Windows OS, assuming the user wants the
			// OS to interpret whatever quotes, backslashes, etc., that
			// are there.
			(*result) += (*arg);
		}
		else {
			// In V2 arg syntax, we encode arguments in a way that should
			// be parsed correctly by Windows function CommandLineToArgv().

			char const *argstr = arg->Value();
			if(argstr[strcspn(argstr," \t\"")] == '\0') {
				// No special characters in the argument.
				(*result) += (*arg);
			}
			else {
				// Special characters, so we need to quote it.

				(*result) += '\"';

				while(*argstr) {
					if(*argstr == '\\') {
						int n = 0;
						while(*argstr == '\\') {
							n++;
							(*result) += *(argstr++);
						}
						if(*argstr == '"') {
							// to produce n backslashes followed by a quote,
							// put 2n backslashes followed by a quote.
							while(n--) {
								(*result) += '\\';
							}
							(*result) += *(argstr++);
						}
					}
					else if(*argstr == '"') {
						(*result) += '\\';
						(*result) += *(argstr++);
					}
					else {
						(*result) += *(argstr++);
					}
				}

				(*result) += '\"';
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
ArgList::V2QuotedToV2Raw(char const *v1_input,MyString *v2_raw,MyString *errmsg)
{
	if(!v1_input) return true;
	ASSERT(v2_raw);

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
				(*v2_raw) += *(v1_input++);
			}
			else {
				quote_terminated = v1_input-1;
				break;
			}
		}
		else {
			(*v2_raw) += *(v1_input++);
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
		if(errmsg) {
			MyString msg;
			msg.sprintf(
				"Unexpected characters following double-quote.  "
				"Did you forget to escape the double-quote by repeating it?  "
				"Here is the quote and trailing characters: %s\n",quote_terminated);
			AddErrorMessage(msg.Value(),errmsg);
		}
		return false;
	}
	return true;
}

bool
ArgList::V1WackedToV1Raw(char const *v1_input,MyString *v1_raw,MyString *errmsg)
{
	if(!v1_input) return true;
	ASSERT(v1_raw);
	ASSERT(!IsV2QuotedString(v1_input));

	while(*v1_input) {
		if(*v1_input == '"') {
			if(errmsg) {
				MyString msg;
				msg.sprintf("Found illegal unescaped double-quote: %s",v1_input);
				AddErrorMessage(msg.Value(),errmsg);
			}
			return false;
		}
		else if(v1_input[0] == '\\' && v1_input[1] == '"') {
			// Escaped double-quote.
			v1_input++;
			(*v1_raw) += *(v1_input++);
		}
		else {
			(*v1_raw) += *(v1_input++);
		}
	}
	return true;
}
