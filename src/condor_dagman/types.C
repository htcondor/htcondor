#include "types.h"
#include "condor_string.h"

//---------------------------------------------------------------------------
template<class TYPE> int compare(TYPE a, TYPE b) {
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
