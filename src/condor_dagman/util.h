/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-2001 CONDOR Team, Computer Sciences Department, 
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

#ifndef _UTIL_H_
#define _UTIL_H_

#include "condor_system.h"   /* for <stdio.h> */

#if 0   /* These function are never used! */
int StringHash(char *str, int numBuckets);
int StringCompare(char *s1, char *s2);
int BinaryHash(void *buffer, int nbytes, int numBuckets);
void StringDup(char *&dst, char *src);
/// Create a new string containing the ascii representation of an integer
char *IntToString (int i);
#endif

const int UTIL_MAX_LINE_LENGTH = 1024;

/** Read a \n-terminated line from fp into line, replacing \n by \0. Do
    not read more than maxc-1 characters. Do not write leading
    whitespace into the line buffer. Return the number of characters
    read (excluding \n). Returns -1 on EOF.
*/ 
extern "C" int util_getline (FILE *fp, char *line, int max);

/** Execute a command, printing verbose messages and failure warnings.
    @param cmd The command or script to execute
    @return The return status of the command
*/
extern "C" int util_popen (const char * cmd);

#endif /* #ifndef _UTIL_H_ */
