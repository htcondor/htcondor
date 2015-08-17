
// Note - python_bindings_common.h must be included first so it can manage macro definition conflicts
// between python and condor.
#include "python_bindings_common.h"
#include <Python.h>

#include "module_lock.h"

#include "classad/classad.h"

using namespace condor;

MODULE_LOCK_MUTEX_TYPE ModuleLock::m_mutex = MODULE_LOCK_MUTEX_INITAILIZER;
#ifdef WIN32
bool ModuleLock::m_mutex_intialized = false;
#endif

#ifdef WIN32
// define and declare a singlton global object so that the constructor will
// call initialize the module lock critical section at load time.
class ModuleLockInitializer {
public:
	ModuleLockInitializer() {
		ModuleLock::initialize();
	}
} g_ModuleLockInitializerSingleton;
#endif


ModuleLock::ModuleLock()
    : m_release_gil(ModuleLock::is_intialized() && !classad::ClassAdGetExpressionCaching()),
      m_owned(false), m_save(0)
{
    acquire();
}

void
ModuleLock::initialize()
{
#ifdef WIN32
    if (ModuleLock::m_mutex_intialized) return;
    #pragma warning(suppress: 28125) // function must be called from within a try/except block... (except that's really optional)
#endif
    MODULE_LOCK_MUTEX_INITIALIZE(&m_mutex);
#ifdef WIN32
    ModuleLock::m_mutex_intialized = true;
#endif
}

void
ModuleLock::acquire()
{
    if (m_release_gil && !m_owned)
    {
        m_save = PyEval_SaveThread();
        MODULE_LOCK_MUTEX_LOCK(&m_mutex);
        m_owned = true;
    }
}

ModuleLock::~ModuleLock()
{
    release();
}

void
ModuleLock::release()
{
    if (m_release_gil && m_owned)
    {
        MODULE_LOCK_MUTEX_UNLOCK(&m_mutex);
        PyEval_RestoreThread(m_save);
        m_owned = false;
    }
}

