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

#include "MyString.h"

// Global emitter declaration
extern class Emitter e;

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
