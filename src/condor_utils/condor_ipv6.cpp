#include "condor_common.h"
#include "condor_ipv6.h"
#include "condor_config.h"

bool _condor_is_ipv6_mode() {
	static bool inited = false;
	static bool is_ipv6 = false;
	if (!inited) {
		is_ipv6 = param_boolean("ENABLE_IPV6", false);
		inited = true;
	}
	return is_ipv6;
}
