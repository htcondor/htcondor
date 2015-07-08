
#include "condor_common.h"
#include "compat_classad.h"

#include "export_headers.h"

void
enable_classad_extensions()
{
    compat_classad::ClassAd::Reconfig();
    enable_deprecation_warnings();
}

