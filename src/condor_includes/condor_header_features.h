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
#ifndef CONDOR_SYS_FEATURES_H
#define CONDOR_SYS_FEATURES_H

#ifdef  __cplusplus
#define BEGIN_C_DECLS   extern "C" {
#define END_C_DECLS     }
#else
#define BEGIN_C_DECLS
#define END_C_DECLS
#endif

#if (defined(WIN32) && defined(_DLL)) 
#define DLL_IMPORT_MAGIC __declspec(dllimport)
#else
#define DLL_IMPORT_MAGIC  /* a no-op on Unix */
#endif

#ifdef __GNUC__
/*
Check printf-style format arguments at compile type.
gcc specific.  Put CHECK_PRINTF_FORMAT(1,2) after a functions'
declaration, before the ";"
a - Argument position of the char * format.
b - Argument position of the "..."
Note that for non-static member functions, you need to add
one to account for the "this" pointer which is passed on
the stack.
*/
#define CHECK_PRINTF_FORMAT(a,b) __attribute__((__format__(__printf__, a, b)))
#else
#define CHECK_PRINTF_FORMAT(a,b)
#endif

#endif /* CONDOR_SYS_FEATURES_H */
