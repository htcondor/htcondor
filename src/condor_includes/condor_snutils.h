/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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
#ifndef CONDOR_SNUTILS_H
#define CONDOR_SNUTILS_H

/*  not every platform has an snprintf, so if we're one of the unlucky few,
	we need to provide it. Currently that seems only to be SOLARIS251
	epaulson 6/18/2002 */

BEGIN_C_DECLS

#ifdef NEED_SNPRINTF
int snprintf(char *output, size_t buffer_size, const char *format, ...);
#endif

int condor_snprintf(char *output, int buffer_size, const char *format, ...);
int condor_vsnprintf(char *output, int buffer_size, const char *format, 
					 va_list args);
int printf_length(const char *format, ...);
int vprintf_length(const char *format, va_list args);

END_C_DECLS

#endif /* CONDOR_SNUTILS_H */



