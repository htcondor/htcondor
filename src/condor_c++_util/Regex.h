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
#ifndef REGEX_INCLUDE
#define REGEX_INCLUDE

#include "condor_common.h"
#include "MyString.h"
#include "extArray.h"
#include "pcre.h"


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
		//const Regex & operator = (const MyString & pattern);
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
	bool compile(const MyString & pattern,
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

private:

	pcre * re;
	int options;

	static pcre * clone_re(pcre * re);
};


#endif

