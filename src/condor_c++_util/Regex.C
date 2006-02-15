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
#include "Regex.h"


template class ExtArray<MyString>;


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
			free(re); re = NULL;
		}
		re = clone_re(copy.re);
	}

	return *this;
}


Regex::~Regex()
{
	if (re) {
		free(re); re = NULL;
	}
}


bool
Regex::compile(const MyString & pattern,
			   const char ** errptr,
			   int * erroffset,
			   int options)
{
	re = pcre_compile(pattern.GetCStr(), options, errptr, erroffset, NULL);

	return (NULL != re);
}


bool
Regex::match(const MyString & string,
			 ExtArray<MyString> * groups)
{
	if (!re) {
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
					   string.GetCStr(),
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

	return rc > 0;
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
