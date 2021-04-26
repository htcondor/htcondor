
// Note - python_bindings_common.h must be included before condor_common to avoid
// re-definition warnings.
#include "python_bindings_common.h"

#include "condor_common.h"
#include "condor_config.h"
#include "condor_version.h"
#include "condor_attributes.h"
#include "daemon.h"
#include "param_info.h"
#include "old_boost.h"
#include "classad_wrapper.h"
#include "htcondor.h"

using namespace boost::python;

void
do_start_command(int cmd, ReliSock &rsock, const ClassAdWrapper &ad)
{
    std::string addr_str;
    if (!ad.EvaluateAttrString(ATTR_MY_ADDRESS, addr_str))
    {
        THROW_EX(HTCondorValueError, "Address not available in location ClassAd.");
    }
    ClassAd ad_copy; ad_copy.CopyFrom(ad);
    Daemon target(&ad_copy, DT_GENERIC, NULL);

    const char *addr;
    bool connect_error = true;
    do
    {
        addr = target.addr();

        if (rsock.connect(addr, 0))
        {
            connect_error = false;
            break;
        }
    }
    while (target.nextValidCm());

    if (connect_error)
    {
        THROW_EX(HTCondorIOError, "Failed to connect to daemon");
    }
    target.startCommand(cmd, &rsock, 30);
}


void
set_remote_param(const ClassAdWrapper &ad, std::string param, std::string value)
{
    if (!is_valid_param_name(param.c_str())) {THROW_EX(HTCondorValueError, "Invalid parameter name.");}

    ReliSock rsock;
    do_start_command(DC_CONFIG_RUNTIME, rsock, ad);

    rsock.encode();
    if (!rsock.code(param)) {THROW_EX(HTCondorIOError, "Can't send param name.");}
    std::stringstream ss;
    ss << param << " = " << value;
    if (!rsock.put(ss.str())) {THROW_EX(HTCondorIOError, "Can't send parameter value.");}
    if (!rsock.end_of_message()) {THROW_EX(HTCondorIOError, "Can't send EOM for param set.");}

    int rval = 0;
    rsock.decode();
    if (!rsock.code(rval)) {THROW_EX(HTCondorIOError, "Can't get parameter set response.");}
    if (!rsock.end_of_message()) {THROW_EX(HTCondorIOError, "Can't get EOM for parameter set.");}
    if (rval < 0) {THROW_EX(HTCondorReplyError, "Failed to set remote daemon parameter.");}
}


std::string
get_remote_param(const ClassAdWrapper &ad, std::string param)
{
    ReliSock rsock;
    do_start_command(CONFIG_VAL, rsock, ad);

    rsock.encode();
    if (!rsock.code(param))
    {
        THROW_EX(HTCondorIOError, "Can't send requested param name.");
    }
    if (!rsock.end_of_message())
    {
        THROW_EX(HTCondorIOError, "Can't send EOM for param name.");
    }

    std::string val;
    rsock.decode();
    if (!rsock.code(val))
    {
        THROW_EX(HTCondorIOError, "Can't receive reply from daemon for param value.");
    }
    if (!rsock.end_of_message())
    {
        THROW_EX(HTCondorIOError, "Can't receive EOM from daemon for param value.");
    }

    return val;
}


static boost::python::object
get_remote_names(const ClassAdWrapper &ad)
{
    boost::python::object retval = boost::python::list();

    ReliSock rsock;
    do_start_command(DC_CONFIG_VAL, rsock, ad);

    rsock.encode();

    std::string names = "?names";
    if (!rsock.put(names))
    {
        THROW_EX(HTCondorIOError, "Failed to send request for parameter names.");
    }
    if (!rsock.end_of_message())
    {
        THROW_EX(HTCondorIOError, "Failed to send EOM for parameter names.");
    }

    rsock.decode();
    std::string val;
    if (!rsock.code(val))
    {
        THROW_EX(HTCondorIOError, "Cannot receive reply for parameter names.");
    }
    if (val == "Not defined")
    {
        if (!rsock.end_of_message())
        {
            THROW_EX(HTCondorIOError, "Unable to receive EOM from remote daemon (unsupported version).");
        }
        if (get_remote_param(ad, "MASTER") == "Not defined")
        {
            THROW_EX(HTCondorReplyError, "Not authorized to query remote daemon.");
        }
        else {THROW_EX(HTCondorReplyError, "Remote daemon is an unsupported version; 8.1.2 or later is required.");}
    }
    if (val[0] == '!')
    {
        rsock.end_of_message();
        THROW_EX(HTCondorReplyError, "Remote daemon failed to get parameter name list");
    }
    if (!val.empty())
    {
        retval.attr("append")(val);
    }
    while (!rsock.peek_end_of_message())
    {
        if (!rsock.code(val))
        {
            THROW_EX(HTCondorIOError, "Failed to read parameter name.");
        }
        retval.attr("append")(val);
    }
    if (!rsock.end_of_message())
    {
        THROW_EX(HTCondorIOError, "Failed to receive final EOM for parameter names");
    }
    return retval;
}


struct RemoteParam
{
    RemoteParam(const ClassAdWrapper &ad)
    {
        m_ad.CopyFrom(ad);
        refresh();
    }


    void delitem(const std::string &attr)
    {
        if (!contains(attr)) {THROW_EX(KeyError, attr.c_str());}

        set_remote_param(m_ad, attr, "");
    }


    bool contains(const std::string &attr)
    {
        cache_attrs();
        if (!m_attrs.attr("__contains__")(attr)) {return false;}
        std::string value = cache_lookup(attr);
        return value != "Not defined";
    }


    object getitem(const std::string &attr)
    {
        return getitem_impl(attr, object(), true);
    }


    object get(const std::string &attr, object default_val)
    {
        return getitem_impl(attr, default_val, false);
    }


    object getitem_impl(const std::string &attr, object default_val, bool throw_exception)
    {
        if (!contains(attr))
        {
            if (throw_exception) {THROW_EX(KeyError, attr.c_str());}
            else {return default_val;}
        }
        boost::python::object result(cache_lookup(attr));
        return result;
    }


    void setitem(const std::string &attr, const std::string &val)
    {
        m_lookup[attr] = val;
        m_attrs.attr("add")(attr);
        set_remote_param(m_ad, attr, val);
    }


    boost::python::object setdefault(const std::string &attr, const std::string &defaultval)
    {
        if (!contains(attr))
        {
            setitem(attr, defaultval);
            boost::python::object result(defaultval);
            return result;
        }
        boost::python::object result(cache_lookup(attr));
        return result;
    }


    void update(boost::python::object source)
    {
        // See if we have a dictionary-like object.
        if (py_hasattr(source, "items"))
        {
            return this->update(source.attr("items")());
        }
        if (!py_hasattr(source, "__iter__")) { THROW_EX(HTCondorTypeError, "Must provide a dictionary-like object to update()"); }

        object iter = source.attr("__iter__")();
        while (true) {
            PyObject *pyobj = PyIter_Next(iter.ptr());
            if (!pyobj) break;
            if (PyErr_Occurred()) {
                throw_error_already_set();
            }

            object obj = object(handle<>(pyobj));

            tuple tup = extract<tuple>(obj);
            std::string attr = extract<std::string>(tup[0]);
            std::string value = extract<std::string>(tup[1]);
            setitem(attr, value);
        }
    }


    boost::python::list keys()
    {
        boost::python::list result;
        cache_attrs();
        result.attr("extend")(m_attrs);
        return result;
    }


    object iter()
    {
        return keys().attr("__iter__")();
    }


    boost::python::list items()
    {
        boost::python::list result;
        cache_attrs();
        boost::python::object iter_obj = m_attrs.attr("__iter__")();
        while (true)
        {
            boost::python::object next_obj;
            try
            {
                PyObject *next_obj_ptr = iter_obj.ptr()->ob_type->tp_iternext(iter_obj.ptr());
                if (next_obj_ptr == NULL) {THROW_EX(StopIteration, "All remote variables processed.");}
                next_obj = boost::python::object(boost::python::handle<>(next_obj_ptr));
                if (PyErr_Occurred()) throw boost::python::error_already_set();
            }
            catch (const boost::python::error_already_set&)
            {
                if (PyErr_ExceptionMatches(PyExc_StopIteration))
                {
                    PyErr_Clear();
                    break;
                }
                boost::python::throw_error_already_set();
            }
            std::string attr = boost::python::extract<std::string>(next_obj);
            std::string value = cache_lookup(attr);
            result.append(boost::python::make_tuple(attr, value));
        }
        return result;
    }


    void refresh()
    {
        m_attrs = py_import("__main__").attr("__builtins__").attr("set")();
        m_lookup = boost::python::dict();
        m_queried_attrs = false;
    }


    size_t len()
    {
        cache_attrs();
        return py_len(m_attrs);
    }


    void cache_attrs()
    {
        if (m_queried_attrs) {return;}
        m_attrs.attr("update")(get_remote_names(m_ad));
        m_queried_attrs = true;
    }


    std::string cache_lookup(const std::string &attr)
    {
        if (m_lookup.attr("__contains__")(attr))
        {
            return boost::python::extract<std::string>(m_lookup[attr]);
        }
        std::string result = get_remote_param(m_ad, attr);
        m_lookup[attr] = result;
        return result;
    }


private:
    ClassAdWrapper m_ad;
    boost::python::object m_attrs;
    boost::python::dict m_lookup;
    bool m_queried_attrs;
};

struct Param
{
    bool contains(const std::string &attr)
    {
        std::string result;
        return param(result, attr.c_str());
    }


    static object param_to_py(const char *attr, const MACRO_META *pmeta, const char *raw_string)
    {
        param_info_t_type_t ty = param_default_type_by_id(pmeta->param_id);
        object pyresult;
        switch (ty)
        {
        case PARAM_TYPE_STRING:
        {
            std::string result;
            if (!param(result, attr)) {
                THROW_EX(HTCondorValueError, ("Unable to convert value for param " + std::string(attr) + " to string (raw value " + raw_string + ")").c_str());
            }
            pyresult = object(result);
            break;
        }
        case PARAM_TYPE_INT:
        {
            int result = param_integer(attr);
            pyresult = object(result);
            break;
        }
        case PARAM_TYPE_BOOL:
        {
            bool result = param_boolean(attr, false);
            pyresult = object(result);
            break;
        }
        case PARAM_TYPE_DOUBLE:
        {
            double result = param_double(attr);
            pyresult = object(result);
            break;
        }
        case PARAM_TYPE_LONG:
        {
            long long result = param_integer(attr);
            pyresult = object(result);
            break;
        }
        }
        return pyresult;
    }


    object getitem_impl(const std::string &attr, object default_val, bool throw_exception)
    {
        std::string name_used;
        const char *pdef_value;
        const MACRO_META *pmeta;
        const char * result_str = param_get_info(attr.c_str(), NULL, NULL, name_used, &pdef_value, &pmeta);
        if (!result_str)
        {
            if (throw_exception)
            {
                THROW_EX(KeyError, attr.c_str());
            }
            else
            {
                return default_val;
            }
        }
        try
        {
            return param_to_py(attr.c_str(), pmeta, result_str);
        }
        catch (error_already_set&)
        {
            PyErr_Clear();
            return object(result_str);
        }
    }


    object getitem(const std::string &attr)
    {
        return getitem_impl(attr, object(), true);
    }


    object get(const std::string &attr, object default_val)
    {
        return getitem_impl(attr, default_val, false);
    }


    void setitem(const std::string &attr, const std::string &val)
    {
        param_insert(attr.c_str(), val.c_str());
    }


    std::string setdefault(const std::string &attr, const std::string &def)
    {
        std::string result;
        if (!param(result, attr.c_str()))
        {
           param_insert(attr.c_str(), def.c_str());
           return def;
        }
        return result;
    }


    static bool keys_processor(void* user, HASHITER& it)
    {
        if (PyErr_Occurred()) { return true; }

        list &results = *static_cast<list*>(user);
        const char * name = hash_iter_key(it);
        const char * value = hash_iter_value(it);
        if (!name || !value) { return true; }
        try
        {
            results.append(name);
        }
        catch (error_already_set&)
        {
            // Suppress the C++ exception.  The HTCondor code is not thread safe.
        }
        return true;
    }


    list keys()
    {
        list results;
        void *results_ptr = static_cast<void*>(&results);
        foreach_param(0, &keys_processor, results_ptr);
        if (PyErr_Occurred())
        {
            throw_error_already_set();
        }
        return results;
    }


    object iter()
    {
        return keys().attr("__iter__")();
    }


    static bool len_processor(void* user, HASHITER& it)
    {
        if (PyErr_Occurred()) { return true; }

        unsigned long &result = *static_cast<unsigned long*>(user);
        const char * name = hash_iter_key(it);
        const char * value = hash_iter_value(it);
        if (!name || !value) { return true; }
        result++;
        return true;
    }


    unsigned long len()
    {
        unsigned long result = 0;
        void *result_ptr = static_cast<void*>(&result);
        foreach_param(0, &len_processor, result_ptr);
        if (PyErr_Occurred())
        {
            throw_error_already_set();
        }
        return result;
    }


    static bool items_processor(void* user, HASHITER& it)
    {
        if (PyErr_Occurred()) return true;

        list &results = *static_cast<list*>(user);
        const char * name = hash_iter_key(it);
        const char * value = hash_iter_value(it);
        if (!name || !value) { return true; }
        try
        {
            MACRO_META *pmeta = hash_iter_meta(it);
            object pyvalue;
            try
            {
                pyvalue = param_to_py(name, pmeta, value);
            }
            catch (error_already_set&)
            {
                PyErr_Clear();
                pyvalue = object(value);
            }
            results.append(make_tuple<std::string, object>(name, pyvalue));
        }
        catch (error_already_set&)
        {
            // Suppress the python-to-C++ exception.  The HTCondor code is not thread safe.
            // This will set PyErr_Occurred so eventually the python exception fires.
        }
        return true;
    }


    list items()
    {
        list results;
        void *results_ptr = static_cast<void*>(&results);
        foreach_param(0, &items_processor, results_ptr);
        if (PyErr_Occurred())
        {
            throw_error_already_set();
        }
        return results;
    }

    void update(boost::python::object source)
    {
        // See if we have a dictionary-like object.
        if (py_hasattr(source, "items"))
        {
            return this->update(source.attr("items")());
        }
        if (!py_hasattr(source, "__iter__")) { THROW_EX(HTCondorTypeError, "Must provide a dictionary-like object to update()"); }

        object iter = source.attr("__iter__")();
        while (true) {
            PyObject *pyobj = PyIter_Next(iter.ptr());
            if (!pyobj) break;
            if (PyErr_Occurred()) {
                throw_error_already_set();
            }

            object obj = object(handle<>(pyobj));

            tuple tup = extract<tuple>(obj);
            std::string attr = extract<std::string>(tup[0]);
            std::string value = extract<std::string>(tup[1]);
            setitem(attr, value);
        }
    }

};

std::string CondorVersionWrapper() { return CondorVersion(); }

std::string CondorPlatformWrapper() { return CondorPlatform(); }

//BOOST_PYTHON_FUNCTION_OVERLOADS(config_overloads, config, 0, 3);
void configWrapper() {
#ifdef WIN32
	// On Windows, there are two sets of environment variables: Win32 and C runtime.
	// When a python program sets environment variables, they only
	// go into the Win32 set.  The HTCondor config routines only examine the
	// C runtime set.  What a mess, there is only so much we can do until
	// this is "fixed" in Python - see https://bugs.python.org/issue16633
	// In the meantime we attempt to workaround this by scanning the Win32
	// environment for variables on interest to HTCondor's config(), and
	// copying them into the C runtime environment.
	char *env_str = GetEnvironmentStrings();
	if (env_str) {
        const char *ptr = env_str;
        while ( *ptr != '\0' ) {
            if (strncasecmp("CONDOR_CONFIG=",ptr,14)==0 ||
                strncasecmp("_CONDOR_",ptr,8)==0)
            {
                _putenv(ptr);
            }
            ptr += strlen(ptr) + 1;
        }
		FreeEnvironmentStrings(env_str);
		env_str = NULL;
	}
#endif
	config();
}

void export_config()
{
    dprintf_make_thread_safe(); // make sure that any dprintf's we do are thread safe on Linux (they always are on Windows)
    config_ex(CONFIG_OPT_NO_EXIT | CONFIG_OPT_WANT_META);
    param_insert("ENABLE_CLASSAD_CACHING", "false");
    classad::ClassAdSetExpressionCaching(false);

    def("version", CondorVersionWrapper,
        R"C0ND0R(
        Returns the version of HTCondor this module is linked against.
        )C0ND0R");
    def("platform", CondorPlatformWrapper,
        R"C0ND0R(
        Returns the platform of HTCondor this module is running on.
        )C0ND0R");
    def("reload_config", configWrapper,
        R"C0ND0R(
        Reload the HTCondor configuration from disk.
        )C0ND0R");
    class_<Param>("_Param",
            R"C0ND0R(
            A dictionary-like object for the local HTCondor configuration; the keys and
            values of this object are the keys and values of the HTCondor configuration.

            The  ``get``, ``setdefault``, ``update``, ``keys``, ``items``, and ``values``
            methods of this class have the same semantics as a Python dictionary.

            Writing to a ``_Param`` object will update the in-memory HTCondor configuration.
            )C0ND0R",
            init<>(args("self")))
        .def("__getitem__", &Param::getitem)
        .def("__setitem__", &Param::setitem)
        .def("__contains__", &Param::contains)
        .def("setdefault", &Param::setdefault)
        .def("get", &Param::get, (arg("self"), arg("key"), arg("default")=object()))
        .def("keys", &Param::keys)
        .def("__iter__", &Param::iter)
        .def("__len__", &Param::len)
        .def("items", &Param::items)
        .def("update", &Param::update)
        ;
    object param = object(Param());
    param.attr("__doc__") =
        R"C0ND0R(
        Provides dictionary-like access the HTCondor configuration.

        An instance of :class:`_Param`.  Upon importing the :mod:`htcondor` module, the
        HTCondor configuration files are parsed and populate this dictionary-like object.
        )C0ND0R";
    scope().attr("param") = param;

    class_<RemoteParam>("RemoteParam",
                R"C0ND0R(
                The :class:`RemoteParam` class provides a dictionary-like interface to the configuration of an HTCondor daemon.
                The  ``get``, ``setdefault``, ``update``, ``keys``, ``items``, and ``values``
                methods of this class have the same semantics as a Python dictionary.
                )C0ND0R",
            boost::python::init<const ClassAdWrapper &>(
                R"C0ND0R(
                :param ad: An ad containing the location of the remote daemon.
                :type ad: :class:`~classad.ClassAd`
                )C0ND0R",
                args("self", "ad")))
        .def("__getitem__", &RemoteParam::getitem)
        .def("__setitem__", &RemoteParam::setitem)
        .def("__contains__", &RemoteParam::contains)
        .def("setdefault", &RemoteParam::setdefault)
        .def("get", &RemoteParam::get, (arg("self"), arg("key"), arg("default")=boost::python::object()))
        .def("keys", &RemoteParam::keys)
        .def("__iter__", &RemoteParam::iter)
        .def("__len__", &RemoteParam::len)
        .def("__delitem__", &RemoteParam::delitem)
        .def("items", &RemoteParam::items)
        .def("update", &RemoteParam::update)
        .def("refresh", &RemoteParam::refresh,
            R"C0ND0R(
            Rebuilds the dictionary based on the current configuration of the daemon.
            )C0ND0R",
            args("self"))
        ;
}
