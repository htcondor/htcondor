/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
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
