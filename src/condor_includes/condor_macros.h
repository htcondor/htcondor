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
#ifndef CONDOR_MACROS_H
#define CONDOR_MACROS_H

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif

#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif


	/* For security purposes, do not allow open() or fopen().
	 * Folks should use the wrapper functions in 
	 * src/condor_c++_util/condor_open.[hC] 
	 */
#ifndef _CONDOR_ALLOW_OPEN
   #include "..\condor_c++_util\condor_open.h"
   #ifdef open
   #  undef open
   #endif
   #ifdef __GNUC__
   #   pragma GCC poison (Calls_to_open_must_use___safe_open_wrapper___instead)
   #endif
   #define open (Calls_to_open_must_use___safe_open_wrapper___instead)   
#endif
#ifndef _CONDOR_ALLOW_FOPEN
   #include "..\condor_c++_util\condor_open.h"
   #ifdef fopen
   #  undef fopen
   #endif
   #ifdef __GNUC__
   #   pragma GCC poison (Calls_to_fopen_must_use___safe_fopen_wrapper___instead)
   #endif
   #define fopen (Calls_to_fopen_must_use___safe_fopen_wrapper___instead)   
#endif 


#endif /* CONDOR_MACROS_H */
