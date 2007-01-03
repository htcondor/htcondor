
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
#if !defined(__CONDOR_FIX_FSTREAM_H) && defined(__cplusplus)
#define __CONDOR_FIX_FSTREAM_H

	/* Save what we redefined open() and fopen() to be, if anything.
 	*/
#ifndef _CONDOR_ALLOW_OPEN
#	define _CONDOR_OPEN_SAVE open
#	undef open
#endif
#ifndef _CONDOR_ALLOW_FOPEN
#	define _CONDOR_FOPEN_SAVE fopen
#	undef fopen
#endif

#include <fstream>
using namespace std;

	/* Restore the open() and fopen() macros back to what they were
	 */
#ifdef _CONDOR_OPEN_SAVE
#	define open _CONDOR_OPEN_SAVE
#endif
#ifdef _CONDOR_FOPEN_SAVE
#	define fopen _CONDOR_FOPEN_SAVE
#endif


#endif
