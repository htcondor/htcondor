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
#include "condor_regex.h"
#include <vector>


Regex::Regex()
{
	this->options = 0;
	this->re = NULL;
}


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
			pcre2_code_free(re); re = NULL;
		}
		re = clone_re(copy.re);
	}

	return *this;
}


Regex::~Regex()
{
	if (re) {
		pcre2_code_free(re); re = NULL;
	}
}


bool
Regex::compile(const char * pattern,
			   int * errcode,
			   int * erroffset,
			   uint32_t options_param)
{
	PCRE2_SPTR pattern_pcre2str = reinterpret_cast<const unsigned char *>(pattern);
	PCRE2_SIZE erroffset_pcre2 = 0;
	re = pcre2_compile(pattern_pcre2str, PCRE2_ZERO_TERMINATED, options_param, errcode, &erroffset_pcre2, NULL);

	if(erroffset) { *erroffset = static_cast<int>(erroffset_pcre2); }

	return (NULL != re);
}

bool
Regex::compile(const std::string & pattern,
			   int * errcode,
			   int * erroffset,
			   uint32_t options_param)
{
	return compile(pattern.c_str(), errcode, erroffset, options_param);
}


// Make sure this is an exact copy of match() before the final purge.
bool
Regex::match(const std::string & string, std::vector<std::string> * groups) {
	if ( ! this->isInitialized() ) {
		return false;
	}

	pcre2_match_data * matchdata = pcre2_match_data_create_from_pattern(re, NULL);
	PCRE2_SPTR string_pcre2str = reinterpret_cast<const unsigned char *>(string.c_str());

	int rc = pcre2_match(re, string_pcre2str, static_cast<PCRE2_SIZE>(string.length()), 0, options, matchdata, NULL);

	PCRE2_SIZE * ovector = pcre2_get_ovector_pointer(matchdata);
	if (NULL != groups) {
		groups->clear();
		for (int i = 0; i < rc; i++) {
			if (ovector[i * 2] == PCRE2_UNSET) {
				groups->emplace_back("");
			} else {
				groups->emplace_back(
						string.substr(static_cast<int>(ovector[i * 2]), 
						static_cast<int>(ovector[i * 2 + 1] - ovector[i * 2])));
			}
		}
	}

	pcre2_match_data_free(matchdata);
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


size_t
Regex::mem_used()
{
	if ( ! re) return 0;

	uint32_t size;
	pcre2_pattern_info(re, PCRE2_INFO_SIZE, &size);
	return static_cast<size_t>(size);
}


pcre2_code *
Regex::clone_re(pcre2_code * re)
{
	if (!re) {
		return NULL;
	}

    pcre2_code * newre = pcre2_code_copy(re);
	// Not using pcre2_code_copy_with_tables because custom tables are not used in the codebase
	pcre2_jit_compile(re, PCRE2_JIT_COMPLETE);

	return newre;
}
