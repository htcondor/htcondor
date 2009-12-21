/***************************************************************
*
* Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
* University of Wisconsin-Madison, WI.
* 
* Licensed under the Apache License, Version 2.0 (the "License"); you
* may not use this file except in compliance with the License.  You may
* obtain a copy of the License at
* 
*    http://www.apache.org/licenses/LICENSE-2.0
* 
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
***************************************************************/

#include "condor_common.h"

/* ANSI-C code produced by gperf version 3.0.3 */
/* Command-line: gperf  */
/* Computed positions: -k'1-2,$' */

#if !((' ' == 32) && ('!' == 33) && ('"' == 34) && ('#' == 35) \
      && ('%' == 37) && ('&' == 38) && ('\'' == 39) && ('(' == 40) \
      && (')' == 41) && ('*' == 42) && ('+' == 43) && (',' == 44) \
      && ('-' == 45) && ('.' == 46) && ('/' == 47) && ('0' == 48) \
      && ('1' == 49) && ('2' == 50) && ('3' == 51) && ('4' == 52) \
      && ('5' == 53) && ('6' == 54) && ('7' == 55) && ('8' == 56) \
      && ('9' == 57) && (':' == 58) && (';' == 59) && ('<' == 60) \
      && ('=' == 61) && ('>' == 62) && ('?' == 63) && ('A' == 65) \
      && ('B' == 66) && ('C' == 67) && ('D' == 68) && ('E' == 69) \
      && ('F' == 70) && ('G' == 71) && ('H' == 72) && ('I' == 73) \
      && ('J' == 74) && ('K' == 75) && ('L' == 76) && ('M' == 77) \
      && ('N' == 78) && ('O' == 79) && ('P' == 80) && ('Q' == 81) \
      && ('R' == 82) && ('S' == 83) && ('T' == 84) && ('U' == 85) \
      && ('V' == 86) && ('W' == 87) && ('X' == 88) && ('Y' == 89) \
      && ('Z' == 90) && ('[' == 91) && ('\\' == 92) && (']' == 93) \
      && ('^' == 94) && ('_' == 95) && ('a' == 97) && ('b' == 98) \
      && ('c' == 99) && ('d' == 100) && ('e' == 101) && ('f' == 102) \
      && ('g' == 103) && ('h' == 104) && ('i' == 105) && ('j' == 106) \
      && ('k' == 107) && ('l' == 108) && ('m' == 109) && ('n' == 110) \
      && ('o' == 111) && ('p' == 112) && ('q' == 113) && ('r' == 114) \
      && ('s' == 115) && ('t' == 116) && ('u' == 117) && ('v' == 118) \
      && ('w' == 119) && ('x' == 120) && ('y' == 121) && ('z' == 122) \
      && ('{' == 123) && ('|' == 124) && ('}' == 125) && ('~' == 126))
/* The character set is not based on ISO-646.  */
#error "gperf generated tables don't work with this execution character set. Please report a bug to <bug-gnu-gperf@gnu.org>."
#endif


#include <string.h>
#include <stdlib.h>

struct mimetype { 
	char *ext; 
	const char * pmimetype; 
};

struct mimetype *mime_lookup (const char *, unsigned int);

extern "C" {
const char *type_for_ext(const char *ext) {
      struct mimetype *tuple;
      if ((tuple = mime_lookup(ext, strlen(ext))) != NULL) {
      	 return tuple->pmimetype;
      } 
      
      return NULL;
}
}

#define TOTAL_KEYWORDS 84
#define MIN_WORD_LENGTH 1
#define MAX_WORD_LENGTH 7
#define MIN_HASH_VALUE 1
#define MAX_HASH_VALUE 258
/* maximum key range = 258, duplicates = 0 */

#ifndef GPERF_DOWNCASE
#define GPERF_DOWNCASE 1
static unsigned char gperf_downcase[256] =
  {
      0,   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,
     15,  16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,  29,
     30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  40,  41,  42,  43,  44,
     45,  46,  47,  48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,
     60,  61,  62,  63,  64,  97,  98,  99, 100, 101, 102, 103, 104, 105, 106,
    107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121,
    122,  91,  92,  93,  94,  95,  96,  97,  98,  99, 100, 101, 102, 103, 104,
    105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119,
    120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134,
    135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149,
    150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164,
    165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179,
    180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194,
    195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209,
    210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224,
    225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239,
    240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254,
    255
  };
#endif

#ifndef GPERF_CASE_STRCMP
#define GPERF_CASE_STRCMP 1
static int
gperf_case_strcmp (register const char *s1, register const char *s2)
{
  for (;;)
    {
      unsigned char c1 = gperf_downcase[(unsigned char)*s1++];
      unsigned char c2 = gperf_downcase[(unsigned char)*s2++];
      if (c1 != 0 && c1 == c2)
        continue;
      return (int)c1 - (int)c2;
    }
}
#endif

#ifdef __GNUC__
__inline
#else
#ifdef __cplusplus
inline
#endif
#endif
static unsigned int
mime_hash (register const char *str, register unsigned int len)
{
  static unsigned short asso_values[] =
    {
      259, 259, 259, 259, 259, 259, 259, 259, 259, 259,
      259, 259, 259, 259, 259, 259, 259, 259, 259, 259,
      259, 259, 259, 259, 259, 259, 259, 259, 259, 259,
      259, 259, 259, 259, 259, 259, 259, 259, 259, 259,
      259, 259, 259, 259, 259, 259, 259, 259,  55, 259,
       12,   7, 259, 259, 259, 259, 259,  55, 259, 259,
      259, 259, 259, 259, 259,  85,   2,  65, 115,  10,
       75,  35,   0,  10,  80,  95,   0,  25,  30,  70,
       20,   5,   0,  15,   5,   0,  10, 125,   0, 259,
        7, 259, 259, 259, 259, 259, 259,  85,   2,  65,
      115,  10,  75,  35,   0,  10,  80,  95,   0,  25,
       30,  70,  20,   5,   0,  15,   5,   0,  10, 125,
        0, 259,   7, 259, 259, 259, 259, 259, 259, 259,
      259, 259, 259, 259, 259, 259, 259, 259, 259, 259,
      259, 259, 259, 259, 259, 259, 259, 259, 259, 259,
      259, 259, 259, 259, 259, 259, 259, 259, 259, 259,
      259, 259, 259, 259, 259, 259, 259, 259, 259, 259,
      259, 259, 259, 259, 259, 259, 259, 259, 259, 259,
      259, 259, 259, 259, 259, 259, 259, 259, 259, 259,
      259, 259, 259, 259, 259, 259, 259, 259, 259, 259,
      259, 259, 259, 259, 259, 259, 259, 259, 259, 259,
      259, 259, 259, 259, 259, 259, 259, 259, 259, 259,
      259, 259, 259, 259, 259, 259, 259, 259, 259, 259,
      259, 259, 259, 259, 259, 259, 259, 259, 259, 259,
      259, 259, 259, 259, 259, 259, 259, 259, 259, 259,
      259, 259, 259, 259, 259, 259
    };
  register int hval = len;

  switch (hval)
    {
      default:
        hval += asso_values[(unsigned char)str[1]];
      /*FALLTHROUGH*/
      case 1:
        hval += asso_values[(unsigned char)str[0]];
        break;
    }
  return hval + asso_values[(unsigned char)str[len - 1]];
}

#ifdef __GNUC__
__inline
#ifdef __GNUC_STDC_INLINE__
__attribute__ ((__gnu_inline__))
#endif
#endif
struct mimetype *
mime_lookup (register const char *str, register unsigned int len)
{
  static struct mimetype wordlist[] =
    {
      {"",""},
      {"h","text/plain"},
      {"hh","text/plain"},
      {"hxx","text/plain"},
      {"",""},
      {"xhtml","application/xhtml+xml"},
      {"",""},
      {"tr","application/x-troff"},
      {"xht","application/xhtml+xml"},
      {"html","text/html"},
      {"lzh","application/octet-stream"},
      {"t","application/x-troff"},
      {"el","text/plain"},
      {"txt","text/plain"},
      {"vrml","model/vrml"},
      {"",""}, {"",""},
      {"qt","video/quicktime"},
      {"xsl","text/xml"},
      {"",""}, {"",""}, {"",""}, {"",""},
      {"exe","application/octet-stream"},
      {"text","text/plain"},
      {"",""}, {"",""}, {"",""},
      {"xml","text/xml"},
      {"texi","application/x-texinfo"},
      {"",""}, {"",""}, {"",""},
      {"htm","text/html"},
      {"",""}, {"",""}, {"",""}, {"",""},
      {"eml","message/rfc822"},
      {"mesh","model/mesh"},
      {"",""}, {"",""}, {"",""},
      {"msh","model/mesh"},
      {"smil","application/smil"},
      {"bin","application/octet-stream"},
      {"",""}, {"",""},
      {"eps","application/postscript"},
      {"midi","audio/midi"},
      {"tgz","application/x-gzip"},
      {"gz","application/x-gzip"},
      {"ps","application/postscript"},
      {"smi","application/smil"},
      {"sgml","text/sgml"},
      {"mp3","audio/mpeg"},
      {"",""}, {"",""},
      {"mpe","video/mpeg"},
      {"",""},
      {"mp2","audio/mpeg"},
      {"",""}, {"",""},
      {"igs","model/iges"},
      {"iges","model/iges"},
      {"",""}, {"",""}, {"",""},
      {"cxx","text/plain"},
      {"",""}, {"",""}, {"",""},
      {"pm","text/plain"},
      {"img","application/octet-stream"},
      {"",""}, {"",""}, {"",""}, {"",""},
      {"sgm","text/sgml"},
      {"",""}, {"",""}, {"",""}, {"",""},
      {"mpg","video/mpeg"},
      {"mpeg","video/mpeg"},
      {"class","application/octet-stream"},
      {"",""},
      {"au","audio/basic"},
      {"lha","application/octet-stream"},
      {"",""}, {"",""}, {"",""},
      {"texinfo","application/x-texinfo"},
      {"tif","image/tiff"},
      {"tiff","image/tiff"},
      {"",""}, {"",""}, {"",""},
      {"iso","application/octet-stream"},
      {"silo","model/mesh"},
      {"",""}, {"",""}, {"",""},
      {"asx","video/x-ms-asf"},
      {"",""}, {"",""}, {"",""},
      {"ai","application/postscript"},
      {"mov","video/quicktime"},
      {"",""}, {"",""}, {"",""}, {"",""},
      {"jpe","image/jpeg"},
      {"mail","message/rfc822"},
      {"",""}, {"",""}, {"",""},
      {"dll","application/octet-stream"},
      {"",""}, {"",""}, {"",""}, {"",""},
      {"gif","image/gif"},
      {"",""}, {"",""}, {"",""}, {"",""},
      {"wrl","model/vrml"},
      {"",""}, {"",""},
      {"c","text/plain"},
      {"",""},
      {"xsd","text/xml"},
      {"mpga","audio/mpeg"},
      {"",""}, {"",""}, {"",""},
      {"jpg","image/jpg"},
      {"jpeg","image/jpeg"},
      {"",""}, {"",""}, {"",""},
      {"man","application/x-troff-man"},
      {"wsdl","text/xml"},
      {"",""}, {"",""}, {"",""},
      {"ico","image/ico"},
      {"roff","application/x-troff"},
      {"",""}, {"",""}, {"",""},
      {"mid","audio/midi"},
      {"",""}, {"",""}, {"",""},
      {"so","application/octet-stream"},
      {"dms","application/octet-stream"},
      {"",""}, {"",""}, {"",""},
      {"nc","application/x-netcdf"},
      {"snd","audio/basic"},
      {"aifc","audio/x-aiff"},
      {"",""}, {"",""}, {"",""},
      {"asc","text/plain"},
      {"",""}, {"",""}, {"",""}, {"",""},
      {"aif","audio/x-aiff"},
      {"aiff","audio/x-aiff"},
      {"",""}, {"",""}, {"",""},
      {"asf","video/x-ms-asf"},
      {"",""}, {"",""}, {"",""}, {"",""},
      {"kar","audio/midi"},
      {"",""}, {"",""}, {"",""}, {"",""},
      {"f90","text/plain"},
      {"",""}, {"",""}, {"",""}, {"",""}, {"",""}, {"",""},
      {"",""}, {"",""},
      {"cc","text/plain"},
      {"",""},
      {"djvu","image/vnd.djvu"},
      {"",""}, {"",""}, {"",""}, {"",""}, {"",""}, {"",""},
      {"",""}, {"",""},
      {"djv","image/vnd.djvu"},
      {"",""}, {"",""}, {"",""}, {"",""}, {"",""}, {"",""},
      {"",""}, {"",""}, {"",""}, {"",""}, {"",""}, {"",""},
      {"",""}, {"",""}, {"",""}, {"",""}, {"",""}, {"",""},
      {"",""}, {"",""}, {"",""}, {"",""}, {"",""}, {"",""},
      {"",""},
      {"jfif","image/jpeg"},
      {"",""}, {"",""}, {"",""}, {"",""}, {"",""}, {"",""},
      {"",""}, {"",""}, {"",""}, {"",""}, {"",""}, {"",""},
      {"",""}, {"",""}, {"",""}, {"",""}, {"",""}, {"",""},
      {"",""}, {"",""}, {"",""}, {"",""}, {"",""},
      {"cdf","application/x-netcdf"}
    };

  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      register int key = mime_hash (str, len);

      if (key <= MAX_HASH_VALUE && key >= 0)
        {
          register const char *s = wordlist[key].ext;

          if ((((unsigned char)*str ^ (unsigned char)*s) & ~32) == 0 && !gperf_case_strcmp (str, s))
            return &wordlist[key];
        }
    }
  return 0;
}
