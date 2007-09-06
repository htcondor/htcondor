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
