
/*
 * This class creates and configures libcgroups-based limits for the
 * starter.
 *
 */

#include "cgroup.linux.h"

#if defined(HAVE_EXT_LIBCGROUP)

class CgroupLimits {

public:
	CgroupLimits(std::string &cgroup);

	int set_memory_limit_bytes(uint64_t memory_bytes, bool soft=true);
	int set_memsw_limit_bytes(uint64_t memory_bytes);
	int set_cpu_shares(uint64_t share);
	int set_blockio_weight(uint64_t weight);

private:
	const std::string m_cgroup_string;
	Cgroup m_cgroup;

};

#endif

