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

/*
	This file contains functions used by the rest of the unit_test suite.
 */

#include "condor_common.h"
#include "MyString.h"
#include "string_list.h"
#include "condor_debug.h"
#include "condor_config.h"

bool utest_sock_eq_octet( struct in_addr* address, unsigned char oct1, unsigned char oct2, unsigned char oct3, unsigned char oct4 );

/*  Prints TRUE or FALSE depending on if the input indicates success or failure */
const char* tfstr(bool);
const char* tfstr(int);

/*  tfstr() for ints where -1 indicates failure and 0 indicates success */
const char* tfnze(int);

/* For MyString, calls vsprintf on the given str */
bool vsprintfHelper(MyString* str, char* format, ...);

/* For MyString, calls vsprintf_cat on the given str */
bool vsprintf_catHelper(MyString* str, char* format, ...);

/* Returns the empty string when the passed string is null */
const char* nicePrint(const char* str);

/* Exactly like strcmp, but treats NULL and "" as equal */
int niceStrCmp(const char* str1, const char* str2);

/* Exactly like free, but only frees when not null */
void niceFree(char* str);

/* Returns  a char** representation of the StringList starting at the string 
   at index start
*/
char** string_compare_helper(StringList* list, int start);

/* Frees a char** */
void free_helper(char** array);

/* Originallly from condor_c++_util/test_old_classads.cpp */
void make_big_string(int length, char **string, char **quoted_string);

/* Originally from condor_c++_util/test_old_classads.cpp*/
compat_classad::ClassAd* get_classad_from_file();

/* Originally from condor_c++_util/test_old_classads.cpp*/
bool floats_close( float one, float two, float diff = .0001);

bool strings_similar(const MyString* str1, const MyString* str2, 
	const char* delims = " ");

/* Checks if the given strings contain the same strings when split into 
	tokens by the given delimiters .
   Note that if the two strings have duplicates, this does not check if they
    have the same number of duplicates. */
bool strings_similar(const char* str1, const char* str2, 
	const char* delims = " ");

/* Converts the given char** into a MyString seperated by the given delims */
MyString* convert_string_array(char** str, int size, char* delim = " ");

/* Converts the given char** into a MyString seperated by the given delims */
MyString* convert_string_array(const char** str, int size, 
	const char* delim = " ");
