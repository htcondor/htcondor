#include "has_sysadmin_cap.h"

#ifndef _HAS_SYS_CAPABILITY_H_
	// We are on a system that doesn't have capabilities, so assume true,
	// meaning that you need to check your rootly uid.

bool has_sysadmin_cap() {
	return true;
}

#else

#include <sys/capability.h>

	// This is a linux system that has capabilities, so even if you have uid 0,
	// you may not have permissions to do what you think.
bool has_sysadmin_cap() {
		int value = 0;

        cap_t cap = cap_get_proc();
        cap_set_proc(cap);
        cap_flag_value_t value;
        cap_get_flag(cap, CAP_SYS_ADMIN, CAP_EFFECTIVE, &value);
        cap_free(cap);

		return value;
}

#endif
