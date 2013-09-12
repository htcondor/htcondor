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
#include "match_prefix.h"

int
match_prefix(const char *s1, const char *s2)
{
	size_t	s1l = strlen(s1);
	size_t	s2l = strlen(s2);
	size_t min = (s1l < s2l) ? s1l : s2l;

	// return true if the strings match (i.e., strcmp() returns 0)
	if (strncmp(s1, s2, min) == 0)
		return 1;

	return 0;
}

bool
is_arg_prefix(const char * parg, const char * pval, int must_match_length /*= 0*/) 
{
	// no matter what, at least 1 char must match
	// this also protects us from the case where parg is ""
	if (!*pval || (*parg != *pval)) return false;

	// do argument matching based on a minimum prefix. when we run out
	// of characters in parg we must be at a \0 or no match and we
	// must have matched at least must_match_length characters or no match
	int match_length = 0;
	while (*parg == *pval) {
		++match_length;
		++parg; ++pval;
		if (!*pval) break;
	}
	if (*parg) return false;
	if (must_match_length < 0) return (*pval == 0);
	return match_length >= must_match_length;
}

bool
is_arg_colon_prefix(const char * parg, const char * pval, const char ** ppcolon, int must_match_length /*= 0*/) 
{
	if (ppcolon) *ppcolon = NULL;

	// no matter what, at least 1 char must match
	// this also protects us from the case where parg is ""
	if (!*pval || (*parg != *pval)) return false;

	// do argument matching based on a minimum prefix. when we run out
	// of characters in parg we must be at a \0 or no match and we
	// must have matched at least must_match_length characters or no match
	int match_length = 0;
	while (*parg == *pval) {
		++match_length;
		++parg; ++pval;
		if (*parg == ':') {
			if (ppcolon) *ppcolon = parg;
			break;
		}
		if (!*pval) break;
	}
	if (*parg && *parg != ':') return false;
	if (must_match_length < 0) return (*pval == 0);
	return match_length >= must_match_length;
}

bool
is_dash_arg_prefix(const char * parg, const char * pval, int must_match_length /*= 0*/)
{
	if (*parg != '-') return false;
	++parg;
	// if arg begins with --, then we require an exact match for pval.
	if (*parg == '-') { ++parg; must_match_length = -1; }
	return is_arg_prefix(parg, pval, must_match_length);
}

