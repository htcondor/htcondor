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

#include "condor_common.h"   /* for <ctype.h>, <assert.h> */
#include "debug.h"
#include "util.h"

//------------------------------------------------------------------------
int util_getline(FILE *fp, char *line, int max) {
  int c, i = 0;
    
  assert (EOF  == -1);
  assert (fp   != NULL);
  assert (line != NULL);
  assert (max  > 0);      /* Need at least 1 slot for '\0' character */

  for (c = getc(fp) ; c != '\n' && c != EOF ; c = getc(fp)) {
    if ( !(i == 0 && isspace(c)) && (i+1 < max)) line[i++] = c;
  }
  line[i] = '\0';
  return (i==0 && c==EOF) ? EOF : i;
}

//-----------------------------------------------------------------------------
int util_popen (const char * cmd) {
    FILE *fp;
    debug_println (DEBUG_VERBOSE, "Running: %s", cmd);
    fp = popen (cmd, "r");
    int r;
    if (fp == NULL || (r = pclose(fp)) != 0) {
        if (DEBUG_LEVEL(DEBUG_NORMAL)) {
            printf ("WARNING: failure: %s", cmd);
            if (fp != NULL) printf (" returned %d", r);
            putchar('\n');
        }
    }
    return r;
}

//=============================================================================
#if 0
const int MAX_CHARCODE = 127;

//---------------------------------------------------------------------------
char *IntToString(int i) {
  ostrstream os;

  os << i << '\0';
  return os.str();    
}

//---------------------------------------------------------------------------
int StringHash(char *str, int numBuckets) {
  //
  // Stole this hash function from DEVise
  //
  int remainder = 0;
  char *s = str;
  while (*s != '\0') {
    remainder = (remainder * MAX_CHARCODE + *s) % numBuckets;
    s++;
  }
  return remainder;
}

//---------------------------------------------------------------------------
int StringCompare(char *s1, char *s2) {
  return strcmp(s1, s2);
}

//---------------------------------------------------------------------------
void StringDup(char *&dst, char *src) {
  dst = new char [strlen(src) + 1];
  strcpy(dst, src);
}

//---------------------------------------------------------------------------
int BinaryHash(void *buffer, int nbytes, int numBuckets) {
  int remainder = 0;
  unsigned char *s = (unsigned char *) buffer;
  int i;
  for (i = 0; i < nbytes; i++) {
    remainder = (remainder * MAX_CHARCODE + s[i]) % numBuckets;
  }
  return remainder;
}

/* Read a \n-terminated line from fp into line, replacing \n by \0. Do
   not read more than maxc-1 characters. Do not write leading
   whitespace into the line buffer. Return the number of characters
   read (excluding \n). Returns -1 on EOF. */
//---------------------------------------------------------------------------
int getline (FILE *fp, char *line, int max) {
  int i;
  int lim;
  int c;
  bool done;
    
  lim = max - 1;
  i = 0;
  done = false;
    
  //
  // Discard all leading whitespace
  //
  while ((i < lim) && ((c = getc(fp)) != EOF)) {
    if (c == '\n') {
      done = true;
      break;
    } else if (isspace(c)) continue;
    line[i++] = c;
    break;
  }
    
  if (i == 0 && feof(fp)) return -1;

  if (done) {
    line[i] = '\0';
    return i;
  }
    
  //
  // Read to the end of the line
  //
  while ((i < lim) && ((c = getc(fp)) != EOF)) {
    if (c == '\n') break;
    line[i++] = c;
  }
    
  if (i == 0 && feof(fp)) return -1;
  line[i] = '\0';
  return i;
    
}
#endif

