
// The condor_shadow links to condor_events.o in libcondor_utils
// which depends on libuuid, but we don't want that depedency
// in the shadow for memory reasons.  So, stub out the two
// functions

#ifndef WINDOWS

#include <uuid/uuid.h>

void uuid_unparse(const uuid_t /*uu*/, char * /*out*/) {}
void uuid_generate_random(uuid_t /*out*/) {}

#endif
