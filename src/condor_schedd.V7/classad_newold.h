/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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
#ifndef INCLUDE_CLASSAD_NEWOLD_H
#define INCLUDE_CLASSAD_NEWOLD_H

namespace classad { class ClassAd; }
class ClassAd;

/*
src - New classad to convert.  Will not be modified.

dst - Old classad to that will be modified to receive results.  Should
be empty/newly constructed, otherwise behavior is undefined.  Dirty flags
from src will be preserved.

returns: true on success, false on a problem.
*/
bool new_to_old(classad::ClassAd & src, ClassAd & dst);

/*
src - Old classad to convert.  Will not be modified.

dst - New classad to receive results.  Should probably be empty.
Dirty flags may or may not be preserved.  (As of 2005-10-10 they are
not, but it will likely be extended to do so later.)

returns: true on success, false on a problem.

*/
bool old_to_new(ClassAd & src, classad::ClassAd & dst);

#endif /* INCLUDE_CLASSAD_NEWOLD_H*/
