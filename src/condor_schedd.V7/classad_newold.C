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
