#ifndef   _CONDOR_CLASSAD_STRING_UTILS_H
#define   _CONDOR_CLASSAD_STRING_UTILS_H

// #include <optional>
// #include <vector>
// #include <string>
// #include "classad/classad.h"

// Converts `attribute` in `ad` into a `vector` of `string`s.  If `attribute`
// is missing or not a list, the `optional` return is invalid; otherwise, it
// is valid.  List entries which are not string literals are silently ignored,
// which means the returned vector may be empty.
std::optional< std::vector<std::string> >
LookupClassAdListOfStrings(
    const classad::ClassAd & ad,
    const std::string & attribute
);

// Converts `list` into a ClassAd list of ClassAd strings and assigns it to
// the `attribute` in the `ad`.  Returns true iff the assignment succeeded.
bool
AssignClassAdListOfStrings(
    classad::ClassAd & ad,
    const std::string & attribute,
    const std::ranges::view auto & list
);

#endif /* _CONDOR_CLASSAD_STRING_UTILS_H */
