#include "has_sysadmin_cap.h"

#ifndef LINUX
	// We are on a system that doesn't have capabilities, so assume true,
	// meaning that you need to check your rootly uid.

bool has_sysadmin_cap() {
	return true;
}

#else

#include "condor_common.h"
#include <linux/capability.h>

extern "C" int capget(void *, void *);

	// This is a linux system that has capabilities, so even if you have uid 0,
	// you may not have permissions to do what you think.
bool has_sysadmin_cap() {

	int result = 0;
	struct __user_cap_header_struct header;
	struct __user_cap_data_struct data[2];
	header.version = _LINUX_CAPABILITY_VERSION_3;
	header.pid = getpid();

	result = capget(&header, &data);

	return result != 0;
}

#endif
