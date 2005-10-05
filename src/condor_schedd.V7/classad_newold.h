#ifndef INCLUDE_CLASSAD_NEWOLD_H
#define INCLUDE_CLASSAD_NEWOLD_H

namespace classad { class ClassAd; }
class ClassAd;

/*
src - New classad to convert.  Will not be modified.

dst - Old classad to that will be modified to receive results.  Should
be empty/newly constructed, otherwise behavior is undefined.

returns: true on success, false on a problem.
*/
bool new_to_old(classad::ClassAd & src, ClassAd & dst);

/*
src - Old classad to convert.  Will not be modified.
dst - New classad to receive results.  Should probably be empty.
returns: true on success, false on a problem.
*/
bool old_to_new(ClassAd & src, classad::ClassAd & dst);

#endif /* INCLUDE_CLASSAD_NEWOLD_H*/
