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
#include "condor_common.h"
#include "classad_newold.h"
#include "condor_new_classads.h"
#include "condor_classad.h"
#define WANT_NAMESPACES
#include "classad_distribution.h"

bool new_to_old(classad::ClassAd & src, ClassAd & dst)
{
	// Convert back to new classad string
	classad::ClassAdUnParser unparser;
	std::string s_newadout;
	unparser.Unparse(s_newadout, &src);

	// Convert to old classad
	NewClassAdParser newtoold;
	ClassAd * newad = newtoold.ParseClassAd(s_newadout.c_str());
	if( ! newad ) {
		return false;
	}
	dst = *newad;
	delete newad;

	// Ensure dirtiness is preserved
	dst.ClearAllDirtyFlags();
	for(classad::ClassAd::dirtyIterator it = src.dirtyBegin();
		it != src.dirtyEnd(); ++it) {
		dst.SetDirtyFlag(it->c_str(), true);
	}

	return true;
}


bool old_to_new(ClassAd & src, classad::ClassAd & dst)
{
	// Convert to new classad string
	NewClassAdUnparser oldtonew;
	oldtonew.SetOutputType(true);
	oldtonew.SetOutputTargetType(true);
	MyString s_newad;
	oldtonew.Unparse(&src, s_newad);

	// Convert to new classad
	classad::ClassAdParser parser;
	return parser.ParseClassAd(s_newad.Value(), dst, true);
}
