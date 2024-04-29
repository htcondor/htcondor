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

#ifndef CONDOR_REGEX_INCLUDE
#define CONDOR_REGEX_INCLUDE

#include "condor_common.h"
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#include <string>
#include <vector>

//Regex NULLRegex;

class Regex
{

public:

		// Options
	enum {
		anchored =  PCRE2_ANCHORED,
		caseless =  PCRE2_CASELESS,
		dollarend = PCRE2_DOLLAR_ENDONLY,
		dotall = PCRE2_DOTALL,
		extended =  PCRE2_EXTENDED,
		multiline = PCRE2_MULTILINE,
		ungreedy =  PCRE2_UNGREEDY
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
		 * As of PCRE2, error code is provided by default in lieu of
		 * a string error message, so if error message is needed,
		 * implement Regex::get_error_message(...)
		 */
	bool compile(const char * pattern,
				 int * errcode,
				 int * erroffset,
				 uint32_t options = 0);
	bool compile(const std::string & pattern,
				 int * errcode,
				 int * erroffset,
				 uint32_t options = 0);
	
		/*
		 * Match the given string against the regular expression,
		 * returning true if there was a match and false
		 * otherwise. nmatches holds the number of matches, if there
		 * were any. The substrings array can be used to capture
		 * substring matches. The nsubstrings argument specifies the
		 * number of slots in the substrings array.
		 */
	bool match(const std::string & string,
			   std::vector<std::string> * groups = NULL);

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

	pcre2_code * re;
	uint32_t options;

	static pcre2_code * clone_re(pcre2_code * re);
};


#endif

