#include "util.h"
#include <ctype.h>    /* for isspace() */
#include <assert.h>

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

