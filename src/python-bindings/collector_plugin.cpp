
/**
 * This module extends the condor_collector plugin interface to allow
 * the callouts be python-based.
 */
#include "python_bindings_common.h"

#include "condor_common.h"
#include "condor_config.h"
#include "string_list.h"

#include <sstream>

#include "old_boost.h"

#include "classad/classad.h"
#include "classad/literals.h"
#include "classad/fnCall.h"
#include "classad/sink.h"

#include "classad_wrapper.h"
#include "exprtree_wrapper.h"

#include <sys/types.h>
#if !defined(WIN32)
	#include <pwd.h>
	#include <sys/stat.h>
#else
	// WINDOWS only
	#include "store_cred.h"
#endif

#include "../condor_collector.V6/CollectorPlugin.h"
#include "command_strings.h"

// Code duplicated from the classad_python_user module.
static std::string
handle_pyerror()
{
    try {
        PyObject *exc,*val,*tb;
        boost::python::object formatted_list, formatted;
        PyErr_Fetch(&exc,&val,&tb);
        if ((tb == nullptr) || (val == nullptr) || (exc == nullptr)) {
           return "(Python unable to provide exception)";
        }
        boost::python::handle<> hexc(exc), hval(boost::python::allow_null(val)), htb(boost::python::allow_null(tb));
        boost::python::object traceback(py_import("traceback"));
        boost::python::object format_exception(traceback.attr("format_exception"));
        formatted_list = format_exception(hexc,hval,htb);
        formatted = boost::python::str("\n").join(formatted_list);
        return boost::python::extract<std::string>(formatted);
    } catch (...) {
        return "(Another exception occurred while generating a traceback)";
    }
}


class PythonCollectorPlugin : public CollectorPlugin
{
public:
    virtual void initialize();
    virtual void shutdown();
    virtual void update(int command, const ClassAd &ad);
    virtual void invalidate(int command, const ClassAd &ad);

private:
    std::vector<boost::python::object> m_shutdown_funcs;
    std::vector<boost::python::object> m_update_funcs;
    std::vector<boost::python::object> m_invalidate_funcs;
};


void
PythonCollectorPlugin::initialize()
{
    if (!Py_IsInitialized())
    {
#if PY_MAJOR_VERSION >= 3
        wchar_t pname[] = L"condor_collector";
#else
        char pname[] = "condor_collector";
#endif
        Py_SetProgramName(pname);
        Py_InitializeEx(0);
    }

    std::string modulesStr;
    if (!param(modulesStr, "COLLECTOR_PLUGIN_PYTHON_MODULES"))
    {
        dprintf(D_FULLDEBUG, "No python module specified as a collector plugin.\n");
        return;
    }
    dprintf(D_FULLDEBUG, "Registering python modules %s.\n", modulesStr.c_str());

    try
    {
        boost::python::object module = py_import("classad");
    }
    catch (boost::python::error_already_set &)
    {
        if (PyErr_Occurred())
        {
            dprintf(D_ALWAYS, "Python exception occurred when loading module classad: %s; will not enable collector plugin.\n", handle_pyerror().c_str());
            PyErr_Clear();
        }
        return;
    }

    StringList modules(modulesStr.c_str());
    m_shutdown_funcs.reserve(modules.number());
    m_update_funcs.reserve(modules.number());
    m_invalidate_funcs.reserve(modules.number());
    modules.rewind();
    const char *tmpMod;
    while ((tmpMod = modules.next()))
    {
        try
        {
            boost::python::object module = py_import(boost::python::str(tmpMod));
            if (py_hasattr(module, "shutdown")) {
                m_shutdown_funcs.push_back(module.attr("shutdown"));
            }
            if (py_hasattr(module, "update")) {
                m_update_funcs.push_back(module.attr("update"));
            }
            if (py_hasattr(module, "invalidate")) {
                m_invalidate_funcs.push_back(module.attr("invalidate"));
            }
        }
        catch (boost::python::error_already_set &)
        {
            if (PyErr_Occurred())
            {
                dprintf(D_ALWAYS, "Python exception occurred when loading module %s: %s\n", tmpMod, handle_pyerror().c_str());
                // Assume we'll never go back to python - clear out the python exception.
                PyErr_Clear();
            }
        }
    }
}


void
PythonCollectorPlugin::shutdown()
{
   if (m_shutdown_funcs.empty()) {return;}

    std::vector<boost::python::object>::const_iterator iter = m_shutdown_funcs.begin();
    for (; iter != m_shutdown_funcs.end(); iter++) {
        try
        {
            (*iter)();
        }
        catch (boost::python::error_already_set &)
        {
            if (PyErr_Occurred())
            {
                dprintf(D_ALWAYS, "Python exception occurred when invoking shutdown function: %s\n", handle_pyerror().c_str());
                PyErr_Clear();
            }
        }
    } 
}


void
PythonCollectorPlugin::update(int command, const ClassAd &ad)
{
   if (m_update_funcs.empty()) {return;}

    boost::python::list pyArgs;

    try
    {

        boost::shared_ptr<ClassAdWrapper> adWrapper(new ClassAdWrapper());
        adWrapper->CopyFrom(ad);

        const char *command_str = getCollectorCommandString(command);
        if (!command_str) {command_str = "UNKNOWN";}
        pyArgs.append(command_str);
        pyArgs.append(adWrapper);
    }
    catch (boost::python::error_already_set &)
    {
        if (PyErr_Occurred())
        {
            dprintf(D_ALWAYS, "Python exception occurred when building arguments for update function: %s\n", handle_pyerror().c_str());
            PyErr_Clear();
        }
        return;
    }

    std::vector<boost::python::object>::const_iterator iter = m_update_funcs.begin();
    for (; iter != m_update_funcs.end(); iter++)
    {
        try
        {
            boost::python::object arg1 = pyArgs[0];
            boost::python::object arg2 = pyArgs[1];
            (*iter)(arg1, arg2);
        }
        catch (boost::python::error_already_set &)
        {
            if (PyErr_Occurred())
            {
                dprintf(D_ALWAYS, "Python exception occurred when invoking update function: %s\n", handle_pyerror().c_str());
                PyErr_Clear();
            }
        }
    }
}


void
PythonCollectorPlugin::invalidate(int command, const ClassAd &ad)
{
   if (m_invalidate_funcs.empty()) {return;}

    boost::python::list pyArgs;

    boost::shared_ptr<ClassAdWrapper> adWrapper(new ClassAdWrapper());
    adWrapper->CopyFrom(ad);

    try
    {
        const char *command_str = getCollectorCommandString(command);
        if (!command_str) {command_str = "UNKNOWN";}
        pyArgs.append(command_str);
        pyArgs.append(adWrapper);
    }
    catch (boost::python::error_already_set &)
    {
        if (PyErr_Occurred())
        {
            dprintf(D_ALWAYS, "Python exception occurred when building arguments for invalidate function: %s\n", handle_pyerror().c_str());
            PyErr_Clear();
        }
        return;
    }

    std::vector<boost::python::object>::const_iterator iter = m_invalidate_funcs.begin();
    for (; iter != m_invalidate_funcs.end(); iter++)
    {
        try
        {
            boost::python::object arg1 = pyArgs[0];
            boost::python::object arg2 = pyArgs[1];
            (*iter)(arg1, arg2);
        }
        catch (boost::python::error_already_set &)
        {
            if (PyErr_Occurred())
            {
                dprintf(D_ALWAYS, "Python exception occurred when invoking invalidate function: %s\n", handle_pyerror().c_str());
                PyErr_Clear();
            }
        }
    }
}


// Global instance; parent constructor auto-registers with the PluginManager
// object when loaded.
static PythonCollectorPlugin instance;

