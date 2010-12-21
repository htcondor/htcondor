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
#include "Regex.h"


Regex::Regex()
{
	this->options = 0;
	this->re = NULL;
}


// const Regex &
// Regex::operator = (const MyString & pattern)
// {
// 	this->options = 0;

// 	if (re) {
// 		free(re); re = NULL;
// 	}

// 	const char *errptr;
// 	int erroffset;
// 	return compile(pattern, &errptr, &erroffset) ? *this : NULLRegex;
// }


Regex::Regex(const Regex & copy)
{
	this->options = copy.options;
	re = clone_re(copy.re);
}


const Regex &
Regex::operator = (const Regex & copy)
{
	if (this != &copy) {
		this->options = copy.options;

		if (re) {
			pcre_free(re); re = NULL;
		}
		re = clone_re(copy.re);
	}

	return *this;
}


Regex::~Regex()
{
	if (re) {
		pcre_free(re); re = NULL;
	}
}


bool
Regex::compile(const MyString & pattern,
			   const char ** errptr,
			   int * erroffset,
			   int options_param)
{
	re = pcre_compile(pattern.Value(), options_param, errptr, erroffset, NULL);

	return (NULL != re);
}


bool
Regex::match(const MyString & string,
			 ExtArray<MyString> * groups)
{
	if ( ! this->isInitialized() ) {
		return false;
	}

	int group_count;
	pcre_fullinfo(re, NULL, PCRE_INFO_CAPTURECOUNT, &group_count);
	int oveccount = 3 * (group_count + 1); // +1 for the string itself
	int * ovector = (int *) malloc(oveccount * sizeof(int));
	if (!ovector) {
			// XXX: EXCEPTing sucks
		EXCEPT("No memory to allocate data for re match");
	}

	int rc = pcre_exec(re,
					   NULL,
					   string.Value(),
					   string.Length(),
					   0, // Index in string from which to start matching
					   options,
					   ovector,
					   oveccount);

	if (NULL != groups) {
		for (int i = 0; i < rc; i++) {
			(*groups)[i] = string.Substr(ovector[i * 2], ovector[i * 2 + 1] - 1);
		}
	}

	free(ovector);
	return rc > 0;
}

/**
 * If the object has been initialized properly and 
 * can now match patterns, return true
 * 
 * @return true if a pattern was compiled successfully, false otherwise
 **/
bool
Regex::isInitialized( )
{
	return ( this->re != NULL );
}

pcre *
Regex::clone_re(pcre * re)
{
	if (!re) {
		return NULL;
	}

	size_t size;
	pcre_fullinfo(re, NULL, PCRE_INFO_SIZE, &size);

	pcre * newre = (pcre *) malloc(size * sizeof(char));
	if (!newre) {
			// XXX: EXCEPTing sucks
		EXCEPT("No memory to allocate re clone");
	}

	memcpy(newre, re, size);

	return newre;
}
