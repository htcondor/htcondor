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

#ifndef _MATCH_PREFIX_H_
#define _MATCH_PREFIX_H_

// returns 1 if N characters of s1 and s2 match, where N is
// the length of the shortest string.  note that this will permit
// garbage at the end of argument names, use is_arg_prefix() to prevent this.
// 
int match_prefix (const char *s1, const char *s2);


// return true if parg is at least must_match_length characters long
// and matches pval up to the length of parg. 
// if must_match_length is -1, then parg and pval must match exactly
// if must_match_length is 0, then parg must match at least 1 char of pval.
//
// is_arg_prefix("poolboy","pool") returns false. because all of parg doesn't match pval
// is_arg_prefix("poo","pool") returns true 
// but is_arg_prefix("p","pool",2) returns false because "p" is not at last 2 characters long 
bool is_arg_prefix(const char * parg, const char * pval, int must_match_length = 0);

// same as is_arg_prefix, but treats a : as the end of the argument and returns a pointer to the :
// so is_arg_colon_prefix("pool:boy", "pool", &pc, -1) returns true and pc points to ":boy"
// use this when you want to tack qualifiers onto arguments rather than supplying them as separate arguments.
bool is_arg_colon_prefix(const char * parg, const char * pval, const char ** ppcolon, int must_match_length = 0);

// return true of parg begins with '-' or '--' and is_arg_prefix(parg,pval,mml) is true
// if parg begins with '--' then parg and pval must match exactly (i.e. must_match_length is set to -1)
bool is_dash_arg_prefix(const char * parg, const char * pval, int must_match_length = 0);

// return true if parg with '-' or '--' and is_arg_colon_prefix(parg,pval,ppc,mml) is true
// if parg begins with '--' then parg and pval must match exactly (i.e. must_match_length is set to -1)
bool is_dash_arg_colon_prefix(const char * parg, const char * pval, const char ** ppcolon, int must_match_length = 0);

#endif
