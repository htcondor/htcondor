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

#include "condor_common.h"
#include "types.h"
#include "condor_string.h"

//---------------------------------------------------------------------------
int compare(int a, int b) {
  if (a == b) return 0;
  return (a > b ? 1 : -1);
}

//---------------------------------------------------------------------------
int CondorID::Compare (const CondorID condorID) const {
  int result = compare (_cluster, condorID._cluster);
  if (result == 0) result = compare (_proc, condorID._proc);
  if (result == 0) result = compare (_subproc, condorID._subproc);
  return result;
}

//-----------------------------------------------------------------------------
string::string (const string & s) {
    _str = strnewp(s._str);
}

//-----------------------------------------------------------------------------
string::string (const char * s) {
    _str = strnewp(s == NULL ? "" : s);
}

//-----------------------------------------------------------------------------
string::string (const char c) {
    _str = new char[2];
    sprintf (_str, "%c", c);
}

//-----------------------------------------------------------------------------
string::string (const int i) {
    _str = new char[21];  // sufficient for 64 bit ints
    sprintf (_str, "%d", i);
}

//-----------------------------------------------------------------------------
const string & string::operator = (const string & s) {
    delete [] _str;
    _str = strnewp(s._str);
    return s;
}

//-----------------------------------------------------------------------------
const char * string::operator = (const char * s) {
    delete [] _str;
    _str = strnewp(s == NULL ? "" : s);
    return s;
}

//-----------------------------------------------------------------------------
string::string (const string & s1, const string & s2) {
    _str = new char [strlen(s1._str) + strlen(s2._str) + 1];
    sprintf (_str, "%s%s", s1._str, s2._str);
}

//-----------------------------------------------------------------------------
ostream & operator << (ostream & out, const string & s) {
    out << s._str;
    return out;
}

//-----------------------------------------------------------------------------
string operator + (const string & s1, const string & s2) {
    return string (s1._str, s2._str);
}

//-----------------------------------------------------------------------------
const string & string::operator += (const string & s) {
    return (*this = *this + s);
}

//-----------------------------------------------------------------------------
bool string::operator == (const string & s) const {
    return strcmp (s._str, _str) == 0;
}

bool string::operator != (const string & s) const {
    return !(*this == s);
}
