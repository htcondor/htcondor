#include "has_sysadmin_cap.h"

#ifndef LINUX
	// We are on a system that doesn't have capabilities, so assume true,
	// meaning that you need to check your rootly uid.

bool has_sysadmin_cap() {
	return true;
}

#else

#include "condor_common.h"
#include <sys/prctl.h>
#include <linux/capability.h>

// This is a linux system that has capabilities, so even if you have uid 0,
// you may not have permissions to do what you think.
bool has_sysadmin_cap() {

	return prctl(PR_CAPBSET_READ, CAP_SYS_ADMIN);
}

#endif
