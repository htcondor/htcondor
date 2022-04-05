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

#ifndef CONDOR_ARGLIST_H
#define CONDOR_ARGLIST_H

/*
  This module contains functions for manipulating argument strings,
  such as the argument string specified for a job in a submit file.

  There are two formats for argument strings.  The old backward-compatible
  one is V1.  The new one is V2.  V1 is interpreted differently on
  different platforms and universes.  In some cases, spaces can be
  inserted in argument values (windows and globus), but in other cases
  there is no way to do that.

  The V2 syntax contains tokens (arguments) delimited by whitespace.
  Special characters within the tokens (e.g. whitespace) may be quoted
  with single quotes.  To insert a literal single quote, you must put
  a repeated single quote inside single quotes.  In other words, enter
  two single quotes if you are already inside a quoted section;
  otherwise enter four single quotes if you are not already within
  quotes.  The backslash is a normal character (because it is too
  common in Windows args and environment).  Note that quotes do not
  delimit tokens, so you may begin and end quotes any number of times
  within the same token.

  The above text describes the "raw" argument syntax.  In cases where
  Condor is reading argument strings from the user, the "quoted" syntax
  is generally used.  V2 strings must be enclosed in double quotes and V1
  strings must _not_ begin with double quotes.  This syntax was chosen
  for maximal backward compatibility, because unescaped double-quotes
  were not legal in V1 submit-file syntax.  In V2 strings, literal
  double-quotes may be escaped by repeating them.

  Example raw V2 syntax:
           this 'contains spaces' and 'some ''quoted'' "text"'
       or  this contains' 'spaces and 'some ''quoted'' "text"'
       or 'this' 'contains spaces' and 'some ''quoted'' "text"'
  yields  "this", "contains spaces", "and", "some 'quoted' \"text\""

  Example quoted V2 syntax yielding same as above:
           "this 'contains spaces' and 'some ''quoted'' ""text""'"

Example Usage of ArgList class:

ArgList args;
MyString errmsg;
bool success;

args.AppendArg("one");
args.AppendArg("two and");
success = args.AppendArgsV1RawOrV2Quoted("three",&errmsg);


Example Usage of split_args():

char **argv=NULL;
MyString errmsg;
bool success = split_args("one 'two and' three",&argv,&errmsg);

*/

#include "simplelist.h"
#include "MyString.h"
#include "condor_classad.h"
#include "condor_ver_info.h"


// Parse a string into a list of tokens.
// This expects args in "raw V2" format (no surrounding double-quotes)
bool split_args(char const *args,SimpleList<MyString> *args_list,MyString *error_msg=NULL);

// Parse a string into a NULL-terminated string array.
// This expects args in "raw V2" format (no surrounding double-quotes)
// Caller should delete the array (e.g. deleteStringArray()).
bool split_args(char const *args,char ***args_array,MyString *error_msg=NULL);

// Produce a string from a list of tokens, quoting as necessary.
// This produces args in "raw V2" format (no surrounding double-quotes)
void join_args(SimpleList<MyString> const &args_list,MyString *result,int start_arg=0);
void join_args(SimpleList<MyString> const & args_list, std::string & result, int start_arg=0);

// Produce a string from a NULL-terminated string array, quoting as necessary.
// This produces args in "raw V2" format (no surrounding double-quotes)
void join_args(char const * const *args_array,MyString *result,int start_arg=0);


// Delete a NULL-terminated array of strings (and delete the strings too).
void deleteStringArray( char **string_array );

class ArgList {
 public:
	ArgList();
	~ArgList();

	int Count() const;                      // return number of args
	void Clear();                           // remove all args
	char const *GetArg(int n) const;        //return nth arg

	void AppendArg(char const *arg);
	void AppendArg(MyString arg);
	void AppendArg(const std::string &arg);
	void AppendArg(int arg);
	void InsertArg(char const *arg,int position);
	void RemoveArg(int position);

	void AppendArgsFromArgList(ArgList const &args);

		// Parse args string in plain old V1 syntax.
		// Double-quotes should not be backwacked.
	bool AppendArgsV1Raw(char const *args,MyString *error_msg);
	bool AppendArgsV1Raw(char const *args, std::string & error_msg);

		// Parse args string in V1Raw or V2Quoted syntax.  This is used
		// anywhere except for arguments strings in submit files (that
		// means it applies to config files and environment strings in
		// submit files).  If the string is enclosed in double-quotes,
		// it will be treated as V2, otherwise it will be treated as
		// V1.  Literal double-quotes in V1 strings should _not_ be
		// escaped, unlike V1 submit syntax (aka V1WackedOrV2Quoted).
		// Therefore, in this format, it is not possible to
		// have a V1 string that begins with a double-quote.  This
		// format was chosen for maximal backward compatibility with
		// the old V1 syntax in the places where this function is used.
	bool AppendArgsV1RawOrV2Quoted(char const *args,MyString *error_msg);
	bool AppendArgsV1RawOrV2Quoted(char const *args,std::string & error_msg);

		// Parse args string in V1Wacked or V2Quoted syntax.  This is
		// used for argument strings in submit files.  If the string
		// is enclosed in double-quotes, it will be treated as V2,
		// otherwise it will be treated as V1.  In V1 syntax, any
		// literal double-quotes must be escaped (backwacked), for
		// compatibility with the original V1 submit-file syntax.
	bool AppendArgsV1WackedOrV2Quoted(char const *args,MyString *error_msg);

		// Parse args string in V2 syntax.
		// Double quotes have no special meaning, unlike V2Quoted syntax.
	bool AppendArgsV2Raw(char const *args,MyString *error_msg);
	bool AppendArgsV2Raw(char const *args, std::string & error_msg);

		// Parse args string in V2Quoted syntax.
		// Just like AppendArgsV1BlahOrV2Quoted(), except the input string
		// _must_ be enclosed in double-quotes or an error (and message)
		// will be generated.
	bool AppendArgsV2Quoted(char const *args,MyString *error_msg);
	bool AppendArgsV2Quoted(char const *args,std::string & error_msg);

		// Internal on-the-wire format allowing V2 or V1 syntax in a
		// backward compatible way.  For backward compatibility,
		// this has to be slightly different from the other user-input
		// V1or2 formats.  To produce a string in this format,
		// use GetArgsStringV1or2Raw().
	bool AppendArgsV1or2Raw(char const *args,MyString *error_msg);

	bool AppendArgsFromClassAd(ClassAd const *ad,MyString *error_msg);
	bool InsertArgsIntoClassAd(ClassAd *ad,CondorVersionInfo *condor_version,MyString *error_string) const;

	bool AppendArgsFromClassAd(ClassAd const *ad, std::string & error_msg);
	bool InsertArgsIntoClassAd(ClassAd *ad, CondorVersionInfo *condor_version, std::string & error_string) const;

		// Returns true if specified condor version requires V1 args syntax.
	static bool CondorVersionRequiresV1(CondorVersionInfo const &condor_version);

		// Get arguments from ClassAd for descriptional purposes.
	static void GetArgsStringForDisplay(ClassAd const *ad,MyString *result);
	static void GetArgsStringForDisplay(ClassAd const *ad,std::string &result);

		// Get arguments from this ArgList object for descriptional purposes.
	void GetArgsStringForDisplay(MyString *result,int start_arg=0) const;
	void GetArgsStringForDisplay(std::string & result, int start_arg=0) const;

		// ...
	void GetArgsStringForLogging( MyString * result ) const;
	void GetArgsStringForLogging( std::string & result ) const;

		// Return a NULL-terminated string array of args.
		// Caller should delete array (e.g. with deleteStringArray())
	char **GetStringArray() const;

		// Create a V1 format args string (no quoting or backwacking)
		// false on error (e.g. because an argument contains spaces).
	bool GetArgsStringV1Raw(std::string & result, std::string & error_msg) const;
	bool GetArgsStringV1Raw(MyString *result,MyString *error_msg) const;

		// Create a V2 format args string.  This differs from
		// GetArgsStringV2Quoted in that string is _not_ enclosed in
		// double quotes.
	bool GetArgsStringV2Raw(std::string & result, int start_arg=0) const;
	bool GetArgsStringV2Raw(MyString *result,MyString *error_msg,int start_arg=0) const;

		// Create V2Quoted args string (i.e. enclosed in double-quotes).
	bool GetArgsStringV2Quoted(MyString *result,MyString *error_msg) const;

		// Create V1Wacked args string if possible; o.w. V2Quoted.  In
		// other words, if possible, produce a string that is
		// backward-compatible with older versions of condor_submit.
	bool GetArgsStringV1WackedOrV2Quoted(MyString *result,MyString *error_msg) const;

		// Create a V1 args string if possible.  o.w. V2, with
		// necessary markings to make it correctly recognized as V2
		// by AppendArgsV1or2Raw().
	bool GetArgsStringV1or2Raw(MyString *result,MyString *error_msg) const;
	bool GetArgsStringV1or2Raw(std::string & result) const;

		// From args in ClassAd, create V1 args string if
		// possible. o.w. V2, with necessary markings to make it
		// correctly recognized as V2 by AppendArgsV1or2Raw().
	bool GetArgsStringV1or2Raw(ClassAd const *ad,MyString *result,MyString *error_msg);
	bool GetArgsStringV1or2Raw(ClassAd const *ad, std::string & result, std::string & error_msg);

		// Create an args string for windows CreateProcess().
	bool GetArgsStringWin32(MyString *result,int skip_args) const;

		// Create args string for system()
		// Every argument will be quoted so that it is treated literally
		// and so that it is treated as a single argument even if it contains
		// spaces.
	bool GetArgsStringSystem(MyString *result,int skip_args) const;
	bool GetArgsStringSystem(std::string & result,int skip_args) const;

	bool InputWasV1() const {return input_was_unknown_platform_v1;}

		// Return true if the string is a V2Quoted string.  Such
		// strings begin and end with double-quotes, since that is not
		// valid in plain old V1 submit file syntax.  The contents
		// within the double quotes should be V2 syntax, but that is
		// not validated by this function; neither is the terminal quote.
		// Instead, such input errors are detected in V2QuotedToV2Raw().
	static bool IsV2QuotedString(char const *str);

		// Convert a V2Quoted string to a V2Raw string.
	    // In other words, remove the surrounding double quotes
	    // and remove backwacks in front of inner double-quotes.
		// (IsV2QuotedString() must be true or this will EXCEPT.)
	static bool V2QuotedToV2Raw(char const *v2_quoted,MyString *v2_raw,MyString *errmsg);

		// Convert V1Wacked string to V1Raw string.
		// In other words, remove backwacks in front of double-quotes.
		// (IsV2QuotedString() must be false or this will EXCEPT.)
	static bool V1WackedToV1Raw(char const *v1_wacked,MyString *v1_raw,MyString *errmsg);

		// Convert V2Raw to V2Quoted string.
		// In other words, enclose in double-quotes (and escape as necessary).
	static void V2RawToV2Quoted(MyString const &v2_raw,MyString *result);

		// Convert V1Raw to V1Wacked string.
		// In other words, escape double-quotes with backwacks.
	static void V1RawToV1Wacked(MyString const &v1_raw,MyString *result);

		// Convenience function for appending error messages.
		// Each new message begins on a new line.
	static void AddErrorMessage(char const *msg,MyString *error_buffer);

	enum ArgV1Syntax {
		UNKNOWN_ARGV1_SYNTAX,
		WIN32_ARGV1_SYNTAX,
		UNIX_ARGV1_SYNTAX
	};

		// Sets the platform-specific syntax to use when parsing V1
		// args.  If this is not called, we don't know what platform
		// to assume, so V1 args are preserved in the original form
		// and we are therefore restricted in how the args can be
		// manipulated.
	void SetArgV1Syntax(ArgV1Syntax v1_syntax);

		// Sets the platform-specific syntax to use when parsing V1
		// args to the platform we are running on (i.e. same platform
		// we were compiled under).  If this is not called, then we
		// don't know what platform to assume, so V1 args are
		// preserved in the original form and we are therefore
		// restricted in how the args can be manipulated.
	void SetArgV1SyntaxToCurrentPlatform();

		// Parse V1Raw args in win32 format.
	bool AppendArgsV1Raw_win32(char const *args,MyString *error_msg);

		// Parse V1Raw args in unix format.
	bool AppendArgsV1Raw_unix(char const *args,MyString *error_msg);

 private:
	SimpleList<MyString> args_list;
	bool input_was_unknown_platform_v1; //true if we got arguments in V1 format for unknown platform
	ArgV1Syntax v1_syntax;

	bool IsSafeArgV1Value(char const *str) const;

};

#endif
