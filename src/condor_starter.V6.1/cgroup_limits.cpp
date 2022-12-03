
#include "cgroup_limits.h"

#if defined(HAVE_EXT_LIBCGROUP)

#include "condor_uid.h"

const char * mem_hard_limit = "memory.limit_in_bytes";
const char * mem_soft_limit = "memory.soft_limit_in_bytes";

CgroupLimits::CgroupLimits(std::string &cgroup) : m_cgroup_string(cgroup)
{
	TemporaryPrivSentry sentry(PRIV_ROOT);
	CgroupManager::getInstance().create(m_cgroup_string, m_cgroup,
		CgroupManager::MEMORY_CONTROLLER | CgroupManager::CPU_CONTROLLER | CgroupManager::BLOCK_CONTROLLER,
		CgroupManager::NO_CONTROLLERS,
		false, false);
}

int 
CgroupLimits::set_memsw_limit_bytes(long long mem_bytes)
{
	// On systems with swap accounting disabled, libcgroup will log an error when writing to
	// memory.memsw.limit_in_bytes, and then mark the in-memory copy of the cgroup invalid,
	// and all subsequent writes, even to valid properties will fail.  Check to see if we
	// have memsw, and quietly pass if we don't.
	
	if (access("/sys/fs/cgroup/memory/memory.memsw.limit_in_bytes", 0600) != 0) {
		// We don't have it, just quietly give up
		return 0;
	}

	if (!m_cgroup.isValid() || !CgroupManager::getInstance().isMounted(CgroupManager::MEMORY_CONTROLLER)) {
		dprintf(D_ALWAYS, "Unable to set memsw limit because cgroup is invalid.\n");
		return 1;
	}

	int err;
	struct cgroup_controller * mem_controller;
	const char * limit = "memory.memsw.limit_in_bytes";

	dprintf(D_ALWAYS, "Limiting memsw usage to %llu bytes\n", mem_bytes);
	struct cgroup *memcg = &m_cgroup.getCgroup();
	if ((mem_controller = cgroup_get_controller(memcg, MEMORY_CONTROLLER_STR)) == NULL) {
		dprintf(D_ALWAYS,
			"Unable to get cgroup memsw controller for %s.\n",
			m_cgroup_string.c_str());
		return 1;
	} else if ((err = cgroup_set_value_uint64(mem_controller, limit, mem_bytes))) {
		dprintf(D_ALWAYS,
			"Unable to set memsw limit for %s: %u %s\n",
			m_cgroup_string.c_str(), err, cgroup_strerror(err));
		return 1;
	} else {
		TemporaryPrivSentry sentry(PRIV_ROOT);
		if ((err = cgroup_modify_cgroup(memcg))) {
			dprintf(D_ALWAYS,
				"Unable to commit memsw limit for %s "
				": %u %s\n",
				m_cgroup_string.c_str(), err, cgroup_strerror(err));
			return 1;
		}
	}
	return 0;
}

int CgroupLimits::set_memory_limit_bytes(long long mem_bytes, bool soft)
{
	if (!m_cgroup.isValid() || !CgroupManager::getInstance().isMounted(CgroupManager::MEMORY_CONTROLLER)) {
		dprintf(D_ALWAYS, "Unable to set memory limit because cgroup is invalid.\n");
		return 1;
	}


	int err;
	struct cgroup_controller * mem_controller;
	const char * limit = soft ? mem_soft_limit : mem_hard_limit;

	dprintf(D_ALWAYS, "Limiting (%s) memory usage to %lld bytes\n", soft ? "soft" : "hard", mem_bytes);
	struct cgroup *memcg = &m_cgroup.getCgroup();
	if ((mem_controller = cgroup_get_controller(memcg, MEMORY_CONTROLLER_STR)) == NULL) {
		dprintf(D_ALWAYS,
			"Unable to get cgroup memory controller for %s.\n",
			m_cgroup_string.c_str());
		return 1;
	} else if ((err = cgroup_set_value_uint64(mem_controller, limit, mem_bytes))) {
		dprintf(D_ALWAYS,
			"Unable to set memory limit for %s: %u %s\n",
			m_cgroup_string.c_str(), err, cgroup_strerror(err));
		return 1;
	} else {
		TemporaryPrivSentry sentry(PRIV_ROOT);
		if ((err = cgroup_modify_cgroup(memcg))) {
			dprintf(D_ALWAYS,
				"Unable to commit %s to %lld for %s "
				": %u %s\n", limit, mem_bytes,
				m_cgroup_string.c_str(), err, cgroup_strerror(err));
			return 1;
		}
	}
	return 0;
}

int CgroupLimits::set_cpu_shares(uint64_t shares)
{
	if (!m_cgroup.isValid() || !CgroupManager::getInstance().isMounted(CgroupManager::CPU_CONTROLLER)) {
		dprintf(D_ALWAYS, "Unable to set CPU shares because cgroup is invalid.\n");
		return 1;
	}

	int err;
	struct cgroup *cpucg = &m_cgroup.getCgroup();
	struct cgroup_controller *cpu_controller;

	if ((cpu_controller = cgroup_get_controller(cpucg, CPU_CONTROLLER_STR)) == NULL) {
		dprintf(D_ALWAYS,
			"Unable to add cgroup CPU controller for %s.\n",
			m_cgroup_string.c_str());
			return 1;
	} else if ((err = cgroup_set_value_uint64(cpu_controller, "cpu.shares", shares))) {
		dprintf(D_ALWAYS,
			"Unable to set CPU shares for %s: %u %s\n",
			m_cgroup_string.c_str(), err, cgroup_strerror(err));
			return 1;
	} else {
		TemporaryPrivSentry sentry(PRIV_ROOT);
		if ((err = cgroup_modify_cgroup(cpucg))) {
			dprintf(D_ALWAYS,
				"Unable to commit CPU shares for %s"
				": %u %s\n",
				m_cgroup_string.c_str(), err, cgroup_strerror(err));
			return 1;
		}
	}
	return 0;
}

int CgroupLimits::set_blockio_weight(uint64_t weight)
{
	if (!m_cgroup.isValid() || !CgroupManager::getInstance().isMounted(CgroupManager::BLOCK_CONTROLLER)) {
		dprintf(D_ALWAYS, "Unable to set blockio weight because cgroup is invalid.\n");
		return 1;
	}

	int err;
	struct cgroup *blkiocg = &m_cgroup.getCgroup();
	struct cgroup_controller *blkio_controller;
	if ((blkio_controller = cgroup_get_controller(blkiocg, BLOCK_CONTROLLER_STR)) == NULL) {
		dprintf(D_ALWAYS,
			"Unable to get cgroup block IO controller for %s.\n",
			m_cgroup_string.c_str());
			return 1;
	} else if ((err = cgroup_set_value_uint64(blkio_controller, "blkio.weight", weight))) {
		dprintf(D_ALWAYS,
			"Unable to set block IO weight for %s: %u %s\n",
			m_cgroup_string.c_str(), err, cgroup_strerror(err));
		return 1;
	} else {
		TemporaryPrivSentry sentry(PRIV_ROOT);
		if ((err = cgroup_modify_cgroup(blkiocg))) {
			dprintf(D_ALWAYS,
				"Unable to commit block IO weight for %s"
				": %u %s\n",
				m_cgroup_string.c_str(), err, cgroup_strerror(err));
			return 1;
		}
	}
	return 0;
}

#endif

