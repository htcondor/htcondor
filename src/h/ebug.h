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
/*
 *  Author   : Ronald Wong
 *  Date     : Oct 1991 -- Oct 1991
 *
 *  Uses :
 *	This small debug util helps user keep track of how the program executes
 *	and where are YOU in the program.  This is very usefel for locating
 *	bug or the place where core dump occurs.
 *
 *  Available flags:
 *	EBUG < 0 : see `NOTE:'
 *
 *	EBUG = 1 :
 *	    No debug statements for macros `Enter/Exit' and `debug'.
 *
 *	EBUG = 2;
 *	    `Enter', `Exit' and `debug' will print debug mesg.
 *
 *	EBUG = 3;
 *	    Same as `1' PLUS `Enter/Exit' has indentation for defferent calling
 *	    sequence.
 *
 *  NOTE:
 *	If you want the indentation, set `EBUG' to negative at the when you
 *	include this debug header file.
 *
 *	On further including this file, set `EBUG' to the desired flag.
 *
 *  Indentifiers used :
 *	_null_, Z_Z -- don't use these name in the file.
 *	EBUG, __DEBUG__ define's are used.
 */


#ifndef	_EBUG_H_
#define	_EBUG_H_


#if	EBUG < 0
    int  Z_Z;
	char z_z[20][100];
#endif

#define	deb0	_null_
#define	Enter0	_null_
#define	Exit0	_null_

#include <varargs.h>
/*VARARGS*/
static void _null_(va_alist) va_dcl {}

#if	EBUG == 1 || EBUG == -1

#define	debug	_null_
#define	deb	_null_
#define	Enter	_null_
#define	Exit	_null_

#elif	EBUG == 2 || EBUG == -2

#define	Enter(A)	printf("    Enter %s()\n", A)

#define	Exit(A)		printf("    Exit %s()\n", A)
#define	debug		printf
#define	deb		printf

#elif	EBUG == 3 || EBUG == -3

#include <string.h>


#define	Enter(A)	printf("%*sEnter %s()\n",Z_Z+=3,"",strcpy(z_z[Z_Z/3],A))
#define	Exit()		printf("%*sExit %s()\n",(Z_Z-=3)+3,"",z_z[Z_Z/3+1])
#define	Return()	printf("%*sReturn %s()\n",(Z_Z-=3)+3,"",z_z[Z_Z/3+1]), \
			return(A)
#define	debug		printf
#define	deb		printf("%*s%s: ",Z_Z+3,"",z_z[Z_Z/3]),printf

#if	EBUG == 3
extern int  Z_Z;
extern char z_z[20][100];
#endif

#endif

#endif

