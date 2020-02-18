
#include "condor_common.h"
#include "compat_classad.h"

#include "export_headers.h"

void
enable_classad_extensions()
{
    compat_classad::ClassAdReconfig();
    enable_deprecation_warnings();
}

