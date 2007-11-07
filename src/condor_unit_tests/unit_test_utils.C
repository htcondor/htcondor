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
#include "unit_test_utils.h"
#include "emit.h"

class Emitter e;

bool utest_sock_eq_octet( 	struct in_addr* address,
							unsigned char oct1,
							unsigned char oct2,
							unsigned char oct3,
							unsigned char oct4 ) {
	unsigned char* byte = (unsigned char*) address;
	return ( *byte == oct1 && *(byte+1) == oct2 && *(byte+2) == oct3 && *(byte+3) == oct4 );
}

/*	Returns TRUE if the input is true and FALSE if it's false */
const char* tfstr( bool var ) {
	if(var) return "TRUE";
	return "FALSE";
}

/*	Returns TRUE if the input is 1 and FALSE if it's 0 */
const char* tfstr( int var ) {
	if(var == TRUE) return "TRUE";
	return "FALSE";
}

/*	Returns TRUE if the input is 0 and FALSE if it's -1 */
const char* tfnze( int var ) {
	if(var != -1) return "TRUE";
	return "FALSE";
}
