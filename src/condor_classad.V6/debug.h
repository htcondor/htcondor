/*********************************************************************
 *
 * Condor ClassAd library
 * Copyright (C) 1990-2003, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI and Rajesh Raman.
 *
 * This source code is covered by the Condor Public License, which can
 * be found in the accompanying LICENSE file, or online at
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
 *********************************************************************/

#ifndef __CLASSAD_DEBUG_H__
#define __CLASSAD_DEBUG_H__

#include "common.h"

BEGIN_NAMESPACE( classad )

#define CLASSAD_EXCEPT \
    _except_line_number = __LINE__, \
    _except_file_name   = __FILE__, \
    _classad_except

extern int _except_line_number;
extern char *_except_file_name;
void _classad_except(char *format, ...);

END_NAMESPACE

#endif /* __CLASSAD_DEBUG_H */
