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

 

#ifndef _EXCEPT_H
#define _EXCEPT_H

#if defined(__cplusplus)
extern "C" {
#endif

/*
**	Definition of exception macro
*/
#define EXCEPT \
	_EXCEPT_Line = __LINE__; \
	_EXCEPT_File = __FILE__; \
	_EXCEPT_Errno = errno; \
	_EXCEPT_

/*
**	Important external variables
*/
extern DLL_IMPORT_MAGIC int	errno;

extern int	_EXCEPT_Line;			/* Line number of the exception           */
extern char	*_EXCEPT_File;			/* File name of the exception             */
extern int	_EXCEPT_Errno;			/* Error number from most recent sys call */
extern int (*_EXCEPT_Cleanup)(int,int,char*);	/* Function to call to clean up (or NULL) */
extern void _EXCEPT_( char *fmt, ... );

#if defined(__cplusplus)
}
#endif

#endif /* _EXCEPT_H */
