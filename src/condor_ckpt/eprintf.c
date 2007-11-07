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
