
#ifndef _UTIL_H_
#define _UTIL_H_

#include <stdio.h>

const int MAX_CHARCODE = 127;

//
// Utility functions for hash tables
//
int StringHash(char *str, int numBuckets);
int StringCompare(char *s1, char *s2);
int IntCompare(int a, int b);
int BinaryHash(void *buffer, int nbytes, int numBuckets);
void StringDup(char *&dst, char *src);

//
// Create a new string containing the ascii representation of an
// integer
//
char *IntToString(int i);

//
// Read a \n-terminated line from fp into line, replacing \n by \0. Do
// not read more than maxc-1 characters. Do not write leading
// whitespace into the line buffer. Return the number of characters
// read (excluding \n). Returns -1 on EOF.
// 
int getline(FILE *fp, char *line, int max);

#endif

