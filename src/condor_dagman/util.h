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
