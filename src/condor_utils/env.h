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

#ifndef _ENV_H
#define _ENV_H

/**********************************************************************

The Env object maintains a collection of environment settings, e.g. for
a process that we are about to exec.  Environment values may be fed
into the Env object in several formats.

Example:

Env envobj;
// Add env settings from the environment
envobj.Import();
// Add env settings from job ClassAd:
envobj.MergeFrom(ad);
// Add env settings in raw V2 syntax.
envobj.MergeFromV2Raw("env1=val1 env2=val2 ...");
// Add env settings in raw V1 syntax.
envobj.MergeFromV1Raw("env1=val1;env2=val2;...", ';');
// Add env settings in input V1or2 syntax (in this example V2 input syntax).
envobj.MergeFromV1RaworV2Quoted("\"env1=val1 env2=val2 ...\"");
// Add a single environment value.
envobj.SetEnv("env1=val1");
// Add a single environment value that is already broken into key and value.
envobj.SetEnv("env1","val1");

The V2 environment syntax is preferable over V1, because it is opsys
independent and it supports quoting of special characters.  The V2
syntax is a sequence of whitespace delimited tokens processed by
split_args().  Each token is then split on the first '=' sign into
key=value pairs.

There is a "raw" format and a quoted format.  The quoted format is
designed to differentiate in a backward compatible way between input
strings that are in the old V1 syntax and the new V2 syntax.  The
quoted format simply means that V2 strings should be enclosed in
double-quotes and any literal double-quotes should be backwacked.

Example V2Raw syntax:
           env1='val one' 'env2=''val2''' env3="val3"
  yields {"env1" = "val one"}, {"env2" = "'val2'"}, {"env3" = "\"val3\""}

Example V2Quoted syntax yielding same as above:
           "env1='val one' 'env2=''val2''' env3=""val3"""

***********************************************************************/


#include "MyString.h"
#include "condor_arglist.h"
#include "condor_classad.h"
#include "condor_ver_info.h"
#include "setenv.h"
template <class Key, class Value> class HashTable;

#if defined(WIN32)
#include <algorithm>
#include <set>
#include <string>
// helper class needed for case-insensitive string comparison on Windows
// see comment above the m_sorted_varnames member declaration below for
// more info
//
struct toupper_string_less {
	static bool
	toupper_char_less(char c1, char c2)
	{
		return (toupper(static_cast<unsigned char>(c1)) <
		        toupper(static_cast<unsigned char>(c2)));
	}
	bool
	operator()(const std::string& s1, const std::string& s2) const
	{
		return std::lexicographical_compare(s1.begin(),
		                                    s1.end(),
		                                    s2.begin(),
		                                    s2.end(),
		                                    toupper_char_less);
	}
};
#endif

class Env final {
 public:
	Env( void );
	virtual ~Env( void );

		// Returns the number of environment entries.
	int Count( void ) const;

		// Remove all environment entries.
	void Clear( void );

		// Add (or overwrite) environment entries from an input
		// string.  If the string begins with a double-quote, it will
		// be treated as V2Quoted; otherwise it will be read as V1Raw.
	bool MergeFromV1RawOrV2Quoted( const char *delimitedString, std::string & error_msg );

		// Add (or overwrite) environment entries from an input string.
		// If the string is not in V2Quoted format, this function
		// returns false and generates an error message.
	bool MergeFromV2Quoted( const char *delimitedString, std::string & error_msg );

		// Add (or overwrite) environment entries from an input string.
		// This should only be called for strings in raw V2 format.
	bool MergeFromV2Raw( const char *delimitedString, std::string* error_msg );

		// Add (or overwrite) environment entries from an input string.
		// This should only be called for strings in raw V1 format.
	bool MergeFromV1Raw( const char *delimitedString, char delim, std::string* error_msg );
	bool MergeFromV1AutoDelim( const char *delimitedString, std::string & error_msg, char delim=0 );

		// Add (or overwrite) environment entries from a NULL-terminated
		// array of key=value strings.  This function returns false
		// if there are any entries that lack a variable name or
		// an assignment, but it skips over them and imports all
		// the valid entries anyway.  If you want that behavior
		// and do not consider it a failure, use Import() instead.
	bool MergeFrom( char const * const *stringArray );

		// Add (or overwrite) environment entries from a NULL-delimited
		// character string
	bool MergeFrom( char const * );

		// Add (or overwrite) environment entries from another
		// environment object.
	void MergeFrom( Env const &env );

		// Add (or overwrite) environment entries from a ClassAd.
	bool MergeFrom( const ClassAd *ad, std::string & error_msg );

		// Add (or overwrite) a key=value environment entry.
	bool SetEnvWithErrorMessage( const char *nameValueExpr, std::string* error_msg );

		// Add (or overwrite) a key=value environment entry.
		// Returns false if the input is not a valid var=value.
		// ASSERTS if it runs out of memory.
	bool SetEnv( const char *nameValueExpr ) {
		return SetEnvWithErrorMessage(nameValueExpr, NULL);
	}

		// Add (or overwrite) specified environment variable.
		// Returns false if not a valid var=value (i.e. if empty var).
		// ASSERTS if it runs out of memory.
	bool SetEnv( const char *var, const char *val );

		// Add (or overwrite) specified environment variable.
		// Returns false if not a valid var=value (i.e. if empty var).
		// ASSERTS if it runs out of memory.
	bool SetEnv( const MyString &, const MyString & );

		// Add (or overwrite) specified environment variable.
		// Returns false if not a valid var=value (i.e. if empty var).
		// ASSERTS if it runs out of memory.
	bool SetEnv( const std::string &, const std::string & );

		// Removes an environment variable; returns true if the variable
		// was previously in the environment.
	bool DeleteEnv( const std::string & );

		// write this class into the V2 environment attribute of the job
	bool InsertEnvIntoClassAd(ClassAd & ad) const;
		// Write this class into the V1 enviroment attribute of the job
		// If not all of the enviroment can be expressed in that format, an error is returned and the job is unchanged
	bool InsertEnvV1IntoClassAd(ClassAd & ad, std::string & error_msg, char delim=0) const;
		// write this class into job as either V1 or V2 environment, using V1 if the job already has V1 only
		// and the contents of this class can be expressed as V1, otherwise use V2
		// The warn message string will be set when the job formerly had  V1 enviroment but the insert was forced to switch to V2
		// a non-empty warn message does not indicate failure, it indicates that the V2 attribute was used
	bool InsertEnvIntoClassAd(ClassAd & ad, std::string & warn_msg) const;

		// Returns true if specified condor version requires V1 env syntax. (6.7.15?)
	static bool CondorVersionRequiresV1(CondorVersionInfo const &condor_version);

		// Modern style: space delimited (and quoted as necessary).
	void getDelimitedStringV2Raw(std::string & result) const;

	 // old-style ; or | delimited
	bool getDelimitedStringV1Raw(MyString *result,std::string * error_msg=nullptr,char delim='\0') const;

		// Returns V2Quoted string (i.e. enclosed in double quotes).
	void getDelimitedStringV2Quoted(std::string& result) const;

		// Get a string describing the environment in this Env object.
	void getDelimitedStringForDisplay(std::string & result) const;

#if defined(WIN32)
		// Caller should delete the string.
	char *getWindowsEnvironmentString() const;
#endif

		// Returns a null-terminated array of strings.
		// Caller should delete it (e.g. with deleteStringArray()).
	char **getStringArray() const;

		// Walk the environment, calling walk_func for each entry until walk_func returns false
	void Walk(bool (*walk_func)(void* pv, const MyString &var, MyString &val), void* pv);
	void Walk(bool (*walk_func)(void* pv, const MyString &var, const MyString &val), void* pv) const;

    void Walk(bool (*walk_func)(void* pv, const std::string & var, const std::string & val), void* pv) const;

	bool GetEnv(MyString const &var,MyString &val) const;
	bool GetEnv(const std::string &var, std::string &val) const;

		// Returns true if string is safe to insert in old-style
		// ; or | delimited string.
	static bool IsSafeEnvV1Value(char const *str,char delim='\0');

		// Returns true if string is safe to insert in new-style
		// environment string.
	static bool IsSafeEnvV2Value(char const *str);

		// Return the appropriate environment delimiter for this opsys.
	static char GetEnvV1Delimiter(char const *opsys=NULL);
		// Return the environment delimiter from this ad or for the current platform if the ad does not specify
	static char GetEnvV1Delimiter(const ClassAd& ad);

		// Returns true if string is V2Quoted format.  In other words,
		// this checks that the string begins with a double-quote.
	static bool IsV2QuotedString(char const *str);

		// Convert a V2Quoted string to a V2Raw string.
		// (IsV2QuotedString() must be true or this will EXCEPT.)
	static bool V2QuotedToV2Raw(char const *v1_quoted,MyString *v2_raw,MyString *error_msg=nullptr);

	bool InputWasV1() const {return input_was_v1;}

		// Import environment from process.
		// Unlike MergeFrom(environ), it is not considered
		// an error if there are entries in the environment
		// that do not contain an assignment; they are
		// silently ignored.  The only possible failure
		// in this function is if it runs out of memory.
		// It will ASSERT() in that case.
	
	template <class Filter>
	void Import(Filter filter) {
		char **my_environ = GetEnviron();
		for (int i=0; my_environ[i]; i++) {
			const char	*p = my_environ[i];

			int			j;
			MyString	varname = "";
			MyString	value = "";
			for (j=0;  ( p[j] != '\0' ) && ( p[j] != '=' );  j++) {
				varname += p[j];
			}
			if ( p[j] == '\0' ) {
				// ignore entries in the environment that do not
				// contain an assignment
				continue;
			}
			if ( varname.empty() ) {
				// ignore entries in the environment that contain
				// an empty variable name
				continue;
			}
			ASSERT( p[j] == '=' );
			value = p+j+1;

			// Allow the application to filter the import
			if (filter( varname, value)) {
				bool ret = SetEnv( varname, value );
				ASSERT( ret ); // should never fail
			}
		}
	}

	static bool everything(MyString &, MyString &) {
		return true;
	}

	void Import() {
		return Import(everything);
	}
#if defined(WIN32)
	// on Windows, environment variable names must be treated as case
	// insensitive. however, we can't just make the Env object's
	// hash table case-insensitive on Windows , since Env is also
	// used on the submit side and we want to support cross-
	// submission. our solution is to leave the hash table case
	// sensitive, but when we are on Windows and an environment
	// string is pulled out with the intent of passing
	// it to CreateProcess (done using the getWindowsEnvironmentString
	// method), we make sure that there are no duplicate variable names.
	// if there are multiple variables in the hash table that differ only
	// in case, the last one to be inserted "wins"
	//
	// the m_sorted_varnames set is used on Windows to enable
	// getWindowsEnvironmentString to provide this behavior
	//
	std::set<std::string, toupper_string_less> m_sorted_varnames;
#endif
 protected:
	HashTable<MyString, MyString> *_envTable;
	bool input_was_v1;


	static bool ReadFromDelimitedString( char const *&input, char *output, char delim );

	static void WriteToDelimitedString(char const *input,MyString &output);

	static void AddErrorMessage(char const *msg,std::string &error_buffer) {
		if ( ! error_buffer.empty()) { error_buffer += "\n"; }
		error_buffer += msg;
	}
};

#endif	// _ENV_H
