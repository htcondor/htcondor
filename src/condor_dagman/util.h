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

#endif /* #ifndef _UTIL_H_ */
