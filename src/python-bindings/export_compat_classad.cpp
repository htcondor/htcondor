
#include "condor_common.h"
#include "compat_classad.h"

void
enable_classad_extensions()
{
        compat_classad::ClassAd::Reconfig();
}

