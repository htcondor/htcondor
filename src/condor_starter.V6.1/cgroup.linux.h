
/*
 * Utility functions for dealing with libcgroup.
 *
 * This is not meant to replace direct interaction with libcgroup, however
 * it provides some simple initialization and RAII wrappers.
 *
 */

#include "condor_common.h"

#if defined(HAVE_EXT_LIBCGROUP)

#include "condor_debug.h"

#ifdef HAVE_PTHREADS
#include <pthread.h>
static pthread_mutex_t g_cgroups_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

#include "libcgroup.h"

#define MEMORY_CONTROLLER_STR "memory"
#define CPUACCT_CONTROLLER_STR "cpuacct"
#define FREEZE_CONTROLLER_STR "freezer"
#define BLOCK_CONTROLLER_STR "blkio"
#define CPU_CONTROLLER_STR "cpu"

#include <map>


class Cgroup; //Forward decl

class CgroupManager {

public:

	enum ControllerFlags {
		NO_CONTROLLERS = 0,
		MEMORY_CONTROLLER = 1,
		CPUACCT_CONTROLLER = 2,
		FREEZE_CONTROLLER = 4,
		BLOCK_CONTROLLER = 8,
		CPU_CONTROLLER = 16,
		// Each time you add a new controller, increase ALL_CONTROLLERS accordingly
		ALL_CONTROLLERS = 32-1,
	};

	static CgroupManager &getInstance();

	bool isMounted(ControllerFlags cf) const {return cf & m_cgroup_mounts;}

	int create(const std::string& cgroup_string, Cgroup &cgroup,
		ControllerFlags preferred_controllers, ControllerFlags required_controllers,
		bool own=true, bool retrieve=true);
	int destroy(Cgroup&);

private:

	CgroupManager();
	CgroupManager(const CgroupManager&);
	CgroupManager& operator=(const CgroupManager&);

	int initialize();

	int initialize_controller(struct cgroup& cgroup, ControllerFlags controller, const char * controller_str, const bool required, const bool has_cgroup, bool &changed_cgroup) const;

	ControllerFlags m_cgroup_mounts;

	static CgroupManager *m_singleton;

	// Ref-counting
	std::map<std::string, int> cgroup_refs;
	std::map<std::string, bool> cgroup_created;

#ifdef HAVE_PTHREADS
	class MutexGuard {

	public:
		MutexGuard(pthread_mutex_t &mutex) : m_mutex(mutex) { pthread_mutex_lock(&m_mutex); }
		~MutexGuard() {pthread_mutex_unlock(&m_mutex); }

	private:
		pthread_mutex_t &m_mutex;
	};

	static MutexGuard getGuard() { return MutexGuard(g_cgroups_mutex);}
#else
	class MutexGuard {};
	MutexGuard getGuard() { return MutexGuard;};
#endif

};

class Cgroup {

public:
	Cgroup() : m_cgroup(NULL) {}
	~Cgroup();

	// Using the zombie object pattern as exceptions are not available.
	bool isValid() {return m_cgroup != NULL;}
	void destroy();

	struct cgroup& getCgroup() { if (isValid()) {return *m_cgroup;} EXCEPT("Accessing invalid cgroup."); return *m_cgroup;}
	const std::string &getCgroupString() {return m_cgroup_string;};
private:
	std::string m_cgroup_string;
	struct cgroup *m_cgroup;

protected:
	void setCgroupString(const std::string &cgroup_string) {m_cgroup_string = cgroup_string;};
	void setCgroup(struct cgroup &cgroup);

	friend class CgroupManager;
};

CgroupManager::ControllerFlags operator|(CgroupManager::ControllerFlags left, CgroupManager::ControllerFlags right);

CgroupManager::ControllerFlags operator|=(CgroupManager::ControllerFlags &left, const CgroupManager::ControllerFlags right);

#endif

