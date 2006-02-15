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
//#include "condor_common.h"
//#include <string.h>
//#include <iostream>
//#include <stdarg.h>
//#include "simplelist.h"

#ifndef GAHP_COMMON_H
#define GAHP_COMMON_H

/* This file contains GAHP protocol-related code that's shared between
 * the gridmanager and the c-gahp. Eventually, the parsing and unparsing
 * code should be here, but the way the two daemons read the commands off
 * the wire is too different. For now, we just share the data structure
 * for holding the parsed array of strings.
 */

/* Users of GahpArgs should not manipulate the class data members directly.
 * Changing the object should only be done via the member functions.
 * If argc is 0, then the value of argv is undefined. If argc > 0, then
 * argv[0] through argv[argc-1] point to valid null-terminated strings. If
 * a NULL is passed to add_arg(), it will be ignored. argv may be resized
 * or freed by add_arg() and reset(), so users should not copy the pointer
 * and expect it to be valid after these calls.
 */
class Gahp_Args {
 public:
	Gahp_Args();
	~Gahp_Args();
	void reset();
	void add_arg( char *arg );
	char **argv;
	int argc;
	int argv_size;
};


#endif
