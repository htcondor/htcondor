/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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
#ifndef CONDOR_ARGLIST_H
#define CONDOR_ARGLIST_H

/*
  This module contains functions for manipulating argument strings,
  such as the argument string specified for a job in a submit file.

  The preferred argument format is called "V2", because there was an
  older format ("V1") which could not represent spaces within
  arguments.  V2 contains tokens (arguments) delimited by whitespace.
  Special characters within the tokens (e.g. whitespace) may be quoted
  with single quotes.  To insert a literal single quote, enter two
  single quotes if you are already inside a quoted section; otherwise
  enter four single quotes if you are not already within quotes.  The
  backslash is a normal character (because it is too common in Windows
  args and environment).  Note that quotes do not delimit tokens, so
  you may begin and end quotes any number of times within the same
  token.

  The above text describes the "raw" argument syntax.  In cases where
  Condor is reading argument strings from the user, the "input" syntax
  is used.  V2 strings must be enclosed in double quotes and V1
  strings must contain only backwacked double quotes.  This syntax was
  chosen for backward compatibility, because unescaped double-quotes
  were not legal in V1 submit-file syntax.  The double-quoted V2
  syntax string is called an "extended V1" string.

  Example raw V2 syntax:
           this 'contains spaces' and 'some ''quoted'' "text"'
       or  this contains' 'spaces and 'some ''quoted'' "text"'
       or 'this' 'contains spaces' and 'some ''quoted'' "text"'
  yields  "this", "contains spaces", "and", "some 'quoted' \"text\""

  Example input V2 syntax yielding same as above:
           "this 'contains spaces' and 'some ''quoted'' \"text\"'"

*/

#include "simplelist.h"
#include "MyString.h"
#include "condor_classad.h"
#include "condor_ver_info.h"


// Parse a string into a list of tokens.
bool split_args(char const *args,SimpleList<MyString> *args_list,MyString *error_msg=NULL);

// Parse a string into a NULL-terminated string array.
// Caller should delete the array (e.g. deleteStringArray()).
bool split_args(char const *args,char ***args_array,MyString *error_msg=NULL);

// Produce a string from a list of tokens, quoting as necessary.
void join_args(SimpleList<MyString> const &args_list,MyString *result,int start_arg=0);

// Produce a string from a NULL-terminated string array, quoting as necessary.
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
	void AppendArg(int arg);
	void InsertArg(char const *arg,int position);
	void RemoveArg(int position);

	void AppendArgsFromArgList(ArgList const &args);

		// Parse args string in plain old V1 syntax.
	bool AppendArgsV1Raw(char const *args,MyString *error_msg);

		// Parse args string in V1 or V2 input syntax.  If the string
		// is enclosed in double-quotes, it will be treated as V2,
		// otherwise it will be treated as V1.
	bool AppendArgsV1or2Input(char const *args,MyString *error_msg);

		// Parse args string in V2 syntax.
	bool AppendArgsV2Raw(char const *args,MyString *error_msg);

		// Parse args string in V2 user input syntax.
		// Just like AppendArgsV1or2Input(), except the input string
		// must be enclosed in double-quotes or an error (and message)
		// will be generated.
	bool AppendArgsV2Input(char const *args,MyString *error_msg);

		// Internal on-the-wire format allowing V2 or V1 syntax in a
		// backward compatible way.
	bool AppendArgsV1or2Raw(char const *args,MyString *error_msg);

	bool AppendArgsFromClassAd(ClassAd const *ad,MyString *error_msg);
	bool InsertArgsIntoClassAd(ClassAd *ad,CondorVersionInfo *condor_version,MyString *error_string);

		// Get arguments from ClassAd for descriptional purposes.
	static void GetArgsStringForDisplay(ClassAd const *ad,MyString *result);

		// Get arguments from this ArgList object for descriptional purposes.
	void GetArgsStringForDisplay(MyString *result,int start_arg=0) const;

		// Return a NULL-terminated string array of args.
		// Caller should delete array (e.g. with deleteStringArray())
	char **GetStringArray() const;

		// Create a V1 format args string (no quoting)
		// false on error (e.g. because an argument contains spaces).
	bool GetArgsStringV1Raw(MyString *result,MyString *error_msg) const;

		// Create a V2 format args string.
	bool GetArgsStringV2Raw(MyString *result,MyString *error_msg,int start_arg=0) const;

		// Create a V1 args string if possible.  o.w. marked V2.
	bool GetArgsStringV1or2Raw(MyString *result,MyString *error_msg) const;

		// From args in ClassAd, create V1 args string if
		// possible. o.w. marked V2.
	bool GetArgsStringV1or2Raw(ClassAd const *ad,MyString *result,MyString *error_msg);

		// Create V2 input args string (i.e. enclosed in double-quotes).
	bool GetArgsStringV2Input(MyString *result,MyString *error_msg);

		// Create an args string for windows CreateProcess().
	bool GetArgsStringWin32(MyString *result,int skip_args,MyString *error_msg) const;

	bool InputWasV1() {return input_was_v1;}

		// Return true if the input string is a V2 input string.  Such
		// strings begin and end with double-quotes, since that is not
		// valid in plain old V1 submit file syntax.  The contents
		// within the double quotes should be V2 syntax, but that is
		// not validated by this function; neither is the terminal quote.
		// Instead, such input errors are detected in V2InputToV2Raw().
	static bool IsV2InputString(char const *str);

		// Convert a V2 input string to a V2 raw string.
		// (IsV2InputString() must be true or this will EXCEPT.)
	static bool V2InputToV2Raw(char const *v1_input,MyString *v2_raw,MyString *errmsg);

		// Convert V1 input string to V1 raw string.
		// In other words, remove backwacks in front of double-quotes.
		// (IsV2InExtendedV1String() must be false or this will EXCEPT.)
	static bool V1InputToV1Raw(char const *v1_input,MyString *v1_raw,MyString *errmsg);

		// Convert V2 raw to V2 input string.
		// In other words, enclose in double-quotes (and escape as necessary).
	static void V2RawToV2Input(MyString const &v2_raw,MyString *result);

		// Convenience function for appending error messages.
		// Each new message begins on a new line.
	static void AddErrorMessage(char const *msg,MyString *error_buffer);

 private:
	SimpleList<MyString> args_list;
	bool input_was_v1; //true if we got arguments in V1 format

	bool IsSafeArgV1Value(char const *str) const;

};

#endif
