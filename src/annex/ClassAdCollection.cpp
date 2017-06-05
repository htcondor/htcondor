#include "condor_common.h"
#include "classad_log.h"

// This file demonstrates that the problem isn't being caused by (the code in)
// condor_utils/classad_collection.h, but is intrinsic to the ClassAdLog code
// itself (condor_utils/classad_log.h).

typedef ClassAdLog< HashKey, const char *, ClassAd * > ClassAdCollection;

void function() {
	ClassAdCollection cac( "/tmp/dummy", 0, NULL );
	cac.BeginTransaction();
	{
		cac.AppendLog( new LogDestroyClassAd( "dummy", cac.GetTableEntryMaker() ) );
	}
	cac.CommitTransaction();
}
