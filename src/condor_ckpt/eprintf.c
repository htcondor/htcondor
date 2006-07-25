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

/* We compile condor libraries with GNU gcc/g++.  However, we want user to be
 * able to link these libraries with cc, f77, etc., perhaps on systems where GNU
 * is not installed.  
 *
 * The GNU standard C libraries (libgcc, et all) sometimes include functions 
 * which are required and yet non-standard.  These functions cause problems for
 * folks trying to link in the Condor libraries with non-GNU compilers.
 *
 * __eprintf() is one of those functions, so we include an __eprintf here. 
 */
#include <stdio.h>

/* This is used by the `assert' macro.  */
void
__eprintf (string, expression, line, filename)
  const char *string;
  const char *expression;
  int line;
  const char *filename;
{
   fprintf (stderr, string, expression, line, filename);
   fflush (stderr);
   abort ();
}
