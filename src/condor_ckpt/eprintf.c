
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
