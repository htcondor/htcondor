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
