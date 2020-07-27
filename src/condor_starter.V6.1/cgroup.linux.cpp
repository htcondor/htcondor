
/*
 * Utility library for libcgroup initialization routines.
 * 
 */

#include "cgroup.linux.h"

#if defined(HAVE_EXT_LIBCGROUP)

/*
 * Helper functions for flag manipulation.
 */
CgroupManager::ControllerFlags operator|(CgroupManager::ControllerFlags left, CgroupManager::ControllerFlags right) {
        return static_cast<CgroupManager::ControllerFlags>(static_cast<int>(left) | static_cast<int>(right));
}

CgroupManager::ControllerFlags operator|=(CgroupManager::ControllerFlags &left, const CgroupManager::ControllerFlags right) {
        left = static_cast<CgroupManager::ControllerFlags>(static_cast<int>(left) | static_cast<int>(right));
	return left;
}

CgroupManager *CgroupManager::m_singleton = NULL;

/*
 * Create a CgroupManager.  Note this is private - users of the CgroupManager
 * may create an instance via CgroupManager::getInstance()
 */

CgroupManager::CgroupManager() :
	m_cgroup_mounts(NO_CONTROLLERS)
{
	initialize();
}

CgroupManager& CgroupManager::getInstance()
{
	MutexGuard guard = CgroupManager::getGuard();

	if (m_singleton == NULL) {
		m_singleton = new CgroupManager;
	}
	return *m_singleton;
}

/*
 * Initialize libcgroup and mount the controllers Condor will use (if possible)
 *
 * Returns 0 on success, -1 otherwise.
 */
int CgroupManager::initialize()
{
	// Initialize library and data structures
	dprintf(D_FULLDEBUG, "Initializing cgroup library.\n");
	cgroup_init();
	void *handle=NULL;
	controller_data info;
	int ret = cgroup_get_all_controller_begin(&handle, &info);
	while (ret == 0) {
		//dprintf(D_FULLDEBUG, "Found controller for %s.\n", info.name); // Too noisy
		if (strcmp(info.name, MEMORY_CONTROLLER_STR) == 0) {
			m_cgroup_mounts |= (info.hierarchy != 0) ? MEMORY_CONTROLLER : NO_CONTROLLERS;
		} else if (strcmp(info.name, CPUACCT_CONTROLLER_STR) == 0) {
			m_cgroup_mounts |= (info.hierarchy != 0) ? CPUACCT_CONTROLLER : NO_CONTROLLERS;
		} else if (strcmp(info.name, FREEZE_CONTROLLER_STR) == 0) {
			m_cgroup_mounts |= (info.hierarchy != 0) ? FREEZE_CONTROLLER : NO_CONTROLLERS;
		} else if (strcmp(info.name, BLOCK_CONTROLLER_STR) == 0) {
			m_cgroup_mounts |= (info.hierarchy != 0) ? BLOCK_CONTROLLER : NO_CONTROLLERS;
		} else if (strcmp(info.name, CPU_CONTROLLER_STR) == 0) {
			m_cgroup_mounts |= (info.hierarchy != 0) ? CPU_CONTROLLER : NO_CONTROLLERS;
		}
		ret = cgroup_get_all_controller_next(&handle, &info);
	}
	if (handle) {
		cgroup_get_all_controller_end(&handle);
	}
	if (!isMounted(BLOCK_CONTROLLER)) {
		dprintf(D_ALWAYS, "Cgroup controller for I/O statistics is not available.\n");
	}
	if (!isMounted(FREEZE_CONTROLLER)) {
		dprintf(D_ALWAYS, "Cgroup controller for process management is not available.\n");
	}
	if (!isMounted(CPUACCT_CONTROLLER)) {
		dprintf(D_ALWAYS, "Cgroup controller for CPU accounting is not available.\n");
	}
	if (!isMounted(MEMORY_CONTROLLER)) {
		dprintf(D_ALWAYS, "Cgroup controller for memory accounting is not available.\n");
	}
	if (!isMounted(CPU_CONTROLLER)) {
		dprintf(D_ALWAYS, "Cgroup controller for CPU is not available.\n");
	}
	if (ret != ECGEOF) {
		dprintf(D_ALWAYS, "Error iterating through cgroups mount information: %s\n", cgroup_strerror(ret));
		return -1;
	}

	return 0;
}

/*
 * Initialize a controller for a given cgroup.
 *
 * Not designed for external users - extracted from CgroupManager::create to reduce code duplication.
 */
int CgroupManager::initialize_controller(struct cgroup& cgroup, const ControllerFlags controller,
	const char * controller_str, const bool required, const bool has_cgroup,
	bool & changed_cgroup) const
{
	if (!isMounted(controller)) {
		if (required) {
			dprintf(D_ALWAYS, "Error - cgroup controller %s not mounted, but required.\n",
				controller_str);
			return 1;
		} else {
			dprintf(D_FULLDEBUG, "Warning - cgroup controller %s not mounted (but not required).\n", controller_str);
			return 0;
		}
	}

	if ((has_cgroup == false) || (cgroup_get_controller(&cgroup, controller_str) == NULL)) {
                changed_cgroup = true;
                if (cgroup_add_controller(&cgroup, controller_str) == NULL) {
                        dprintf(required ? D_ALWAYS : D_FULLDEBUG,
                                "Unable to initialize cgroup %s controller.\n",
                                controller_str);
			return required ? 1 : 0;
                }
        }

	return 0;
}

/*
 * Create a new cgroup.
 * Parameters:
 *   - cgroup: reference to a Cgroup object to create/initialize.
 *   - preferred_controllers: Bitset of the controllers we would prefer.
 *   - required_controllers: Bitset of the controllers which are required.
 * Return values:
 *   - 0 on success if the cgroup is pre-existing.
 *   - -1 on error
 * On failure, the state of cgroup is undefined.
 */
int CgroupManager::create(const std::string &cgroup_string, Cgroup &cgroup,
	CgroupManager::ControllerFlags preferred_controllers,
	CgroupManager::ControllerFlags required_controllers,
	bool own, bool retrieve)
{

	MutexGuard guard = getGuard();

	bool created_cgroup = false, changed_cgroup = false;
	struct cgroup *cgroupp = cgroup_new_cgroup(cgroup_string.c_str());
	if (cgroupp == NULL) {
		dprintf(D_ALWAYS, "Unable to construct new cgroup object.\n");
		return -1;
	}

	// Make sure all required controllers are in preferred controllers:
	preferred_controllers |= required_controllers;

	// Try to fill in the struct cgroup from /proc, if it exists.
	bool has_cgroup = retrieve ? true : false;
	if (retrieve && (ECGROUPNOTEXIST == cgroup_get_cgroup(cgroupp))) {
		has_cgroup = false;
	}

	// Work through the various controllers.
	if ((preferred_controllers & CPUACCT_CONTROLLER) &&
		initialize_controller(*cgroupp, CPUACCT_CONTROLLER,
			CPUACCT_CONTROLLER_STR,
			required_controllers & CPUACCT_CONTROLLER,
			has_cgroup, changed_cgroup)) {
		return -1;
	}
	if ((preferred_controllers & MEMORY_CONTROLLER) &&
		initialize_controller(*cgroupp, MEMORY_CONTROLLER,
			MEMORY_CONTROLLER_STR,
			required_controllers & MEMORY_CONTROLLER,
			has_cgroup, changed_cgroup)) {
		return -1;
	}
	if ((preferred_controllers & FREEZE_CONTROLLER) &&
		initialize_controller(*cgroupp, FREEZE_CONTROLLER,
			FREEZE_CONTROLLER_STR,
			required_controllers & FREEZE_CONTROLLER,
			has_cgroup, changed_cgroup)) {
		return -1;
	}
	if ((preferred_controllers & BLOCK_CONTROLLER) &&
		initialize_controller(*cgroupp, BLOCK_CONTROLLER,
			BLOCK_CONTROLLER_STR,
			required_controllers & BLOCK_CONTROLLER,
			has_cgroup, changed_cgroup)) {
		return -1;
	}
	if ((preferred_controllers & CPU_CONTROLLER) &&
		initialize_controller(*cgroupp, CPU_CONTROLLER,
			CPU_CONTROLLER_STR,
			required_controllers & CPU_CONTROLLER,
			has_cgroup, changed_cgroup)) {
		return -1;
	}

	int err;
	if (has_cgroup == false) {
		if ((err = cgroup_create_cgroup(cgroupp, 0))) {
			// Only record at D_ALWAYS if any cgroup mounts are available.
			dprintf(m_cgroup_mounts ? D_ALWAYS : D_FULLDEBUG,
				"Unable to create cgroup %s."
				"  Cgroup functionality will not work: %s\n",
				cgroup_string.c_str(), cgroup_strerror(err));
			return -1;
		} else {
			created_cgroup = true;
		}
	} else if (has_cgroup && changed_cgroup && (err = cgroup_modify_cgroup(cgroupp))) {
		dprintf(D_ALWAYS,
			"Unable to modify cgroup %s."
			"  Some cgroup functionality may not work: %u %s\n",
			cgroup_string.c_str(), err, cgroup_strerror(err));
	}

	// Try to turn on hierarchical memory accounting.
	struct cgroup_controller * mem_controller = cgroup_get_controller(cgroupp, MEMORY_CONTROLLER_STR);
	if (retrieve && isMounted(MEMORY_CONTROLLER) && created_cgroup && (mem_controller != NULL)) {
		if ((err = cgroup_add_value_bool(mem_controller, "memory.use_hierarchy", true))) {
			dprintf(D_ALWAYS,
				"Unable to set hierarchical memory settings for %s: %u %s\n",
				cgroup_string.c_str(), err, cgroup_strerror(err));
		} else {
			if ((err = cgroup_modify_cgroup(cgroupp))) {
				dprintf(D_ALWAYS,
					"Unable to enable hierarchical memory accounting for %s "
					": %u %s\n",
					cgroup_string.c_str(), err, cgroup_strerror(err));
			}
		}
	}

	// Finally, fill in the Cgroup object's state:
	cgroup.setCgroupString(cgroup_string);
	cgroup.setCgroup(*cgroupp);

	// Do our ref-counting
	std::map<std::string, int>::iterator it = cgroup_refs.find(cgroup_string);
	if (it != cgroup_refs.end()) {
		it->second += 1;
	} else {
		cgroup_refs[cgroup_string] = 1;
		cgroup_created[cgroup_string] = own ? created_cgroup : false;
	}

	return 0;
}

/*
 * Delete the cgroup in the OS.
 * Returns 0 on success, -1 on failure;
 */
int
CgroupManager::destroy(Cgroup &cgroup)
{
	MutexGuard guard = getGuard();

	if (!cgroup.isValid()) {
		return 0;
	}
	const std::string &cgroup_string = cgroup.getCgroupString();

	std::map<std::string, int>::iterator it = cgroup_refs.find(cgroup_string);
	if (it == cgroup_refs.end()) {
		EXCEPT("Destroying an unknown cgroup.");
	}
	it->second--;

	// Only delete if this is the last ref and we originally created it.
	if ((it->second == 0) && (cgroup_created[cgroup_string] )) {
		int err;
		// Must re-initialize the cgroup structure before deletion.
		struct cgroup* dcg = cgroup_new_cgroup(cgroup_string.c_str());
		ASSERT (dcg != NULL);
		if ((err = cgroup_get_cgroup(dcg))) {
			dprintf(D_ALWAYS,
				"Unable to read cgroup %s for deletion: %u %s\n",
				cgroup_string.c_str(), err, cgroup_strerror(err));
			return -1;
		}
		else if ((err = cgroup_delete_cgroup(dcg, CGFLAG_DELETE_RECURSIVE | CGFLAG_DELETE_IGNORE_MIGRATION)))
		{
			dprintf(D_ALWAYS,
				"Unable to completely remove cgroup %s for. %u %s\n",
				cgroup_string.c_str(), err, cgroup_strerror(err));
		} else {
			dprintf(D_FULLDEBUG,
				"Deleted cgroup %s.\n",
				cgroup_string.c_str());
		}
		cgroup_free(&dcg);
	}

	return 0;
}

/*
 * Cleanup cgroup.
 * If the cgroup was created by us in the OS, remove it..
 */
Cgroup::~Cgroup()
{
	destroy();
}

void Cgroup::setCgroup(struct cgroup &cgroup)
{ 
	if (m_cgroup) {
		destroy();
	}
	m_cgroup = &cgroup;
}

void Cgroup::destroy()
{
	if (m_cgroup) {
		CgroupManager::getInstance().destroy(*this);
		cgroup_free(&m_cgroup);
		m_cgroup = NULL;
	}
}

#endif

