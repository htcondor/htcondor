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

#ifndef REGEX_INCLUDE
#define REGEX_INCLUDE

#include "condor_common.h"
#include "MyString.h"
#include "extArray.h"
#ifdef HAVE_PCRE_PCRE_H
#  include "pcre/pcre.h"
#else
#  include "pcre.h"
#endif

//Regex NULLRegex;

class Regex
{

public:

		// Options
	enum {
		anchored =  PCRE_ANCHORED,
		caseless =  PCRE_CASELESS,
		dollarend = PCRE_DOLLAR_ENDONLY,
		dotall = PCRE_DOTALL,
		extended =  PCRE_EXTENDED,
		multiline = PCRE_MULTILINE,
		ungreedy =  PCRE_UNGREEDY
	};

	Regex();
	Regex(const Regex & copy);
	const Regex & operator = (const Regex & copy);
	~Regex();

		/*
		 * Set the pattern. The options are those that can be passed
		 * to pcre_compile, see man pcreapi in
		 * externals/install/pcre-<VERSION>/man for details. The
		 * errstr and errcode provide information as to why the
		 * construction may have failed. True is returned upon
		 * success, false otherwise.
		 */
	bool compile(const char * pattern,
				 const char ** errstr,
				 int * erroffset,
				 int options = 0);
	bool compile(const MyString & pattern,
				 const char ** errstr,
				 int * erroffset,
				 int options = 0);
	bool compile(const std::string & pattern,
				 const char ** errstr,
				 int * erroffset,
				 int options = 0);

		/*
		 * Match the given string against the regular expression,
		 * returning true if there was a match and false
		 * otherwise. nmatches holds the number of matches, if there
		 * were any. The substrings array can be used to capture
		 * substring matches. The nsubstrings argument specifies the
		 * number of slots in the substrings array.
		 */
	bool match(const MyString & string,
			   ExtArray<MyString> * groups = NULL);
	bool match_str(const std::string & string,
			   ExtArray<std::string> * groups = NULL);

		/**
		 * If the object has been initialized properly and
		 * can now match patterns, return true
		 *
		 * @return true if a pattern was compiled successfully, false otherwise
		 **/
	bool isInitialized( );

		/**
		 * returns memory allocated by the pcre object
		 *
		 **/
	size_t mem_used();

private:

	pcre * re;
	int options;

	static pcre * clone_re(pcre * re);
};


#endif

