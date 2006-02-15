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


#include "condor_common.h" 
#include "exit.h"

/*
** These routines are the reverse of the WEXITSTATUS() and friend
** macros. I can't find a POSIX manual that says how it has to be
** laid out, so I'm going with a man page and header file definition
** from a Redhat 9 box. 
*/

/* generate the reverse of WEXITSTATUS */
int 
generate_exit_code(
int input_code
)
{

#if !defined(WIN32)
	return( (input_code & 0x00FF) << 8);
#endif

	/* On Win32, we don't do anything special */
	return input_code;
}

/* Return the reverse of WTERMSIG */
int 
generate_exit_signal(
int input_signal
)
{
#if !defined(WIN32)
	return( (input_signal & 0x007F) );
#endif

	/* On Win32, we don't do anything special */
	return input_signal;
}
