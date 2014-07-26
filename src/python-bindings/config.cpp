
// Note - pyconfig.h must be included before condor_common to avoid
// re-definition warnings.
# include <pyconfig.h>

#include "condor_common.h"
#include "condor_config.h"
#include "condor_version.h"
#include "param_info.h"

#include <boost/python.hpp>

#include "old_boost.h"

using namespace boost::python;

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
            if (!param(result, attr)) { THROW_EX(ValueError, ("Unable to convert value for param " + std::string(attr) + " to string (raw value " + raw_string + ")").c_str()); }
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
        MyString name_used;
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
        catch (error_already_set)
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
        catch (error_already_set)
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
            catch (error_already_set)
            {
                PyErr_Clear();
                pyvalue = object(value);
            }
            results.append(make_tuple<std::string, object>(name, pyvalue));
        }
        catch (error_already_set)
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
        if (!py_hasattr(source, "__iter__")) { THROW_EX(ValueError, "Must provide a dictionary-like object to update()"); }

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
void configWrapper() { config(); }

void export_config()
{
    config_ex(CONFIG_OPT_NO_EXIT | CONFIG_OPT_WANT_META);
    def("version", CondorVersionWrapper, "Returns the version of HTCondor this module is linked against.");
    def("platform", CondorPlatformWrapper, "Returns the platform of HTCondor this module is running on.");
    def("reload_config", configWrapper, "Reload the HTCondor configuration from disk.");
    class_<Param>("_Param")
        .def("__getitem__", &Param::getitem)
        .def("__setitem__", &Param::setitem)
        .def("__contains__", &Param::contains)
        .def("setdefault", &Param::setdefault)
        .def("get", &Param::get, "Returns the value associated with the key; if the key is not a defined parameter, returns the default argument.  Default is None.", (arg("self"), arg("key"), arg("default")=object()))
        .def("keys", &Param::keys)
        .def("__iter__", &Param::iter)
        .def("__len__", &Param::len)
        .def("items", &Param::items)
        .def("update", &Param::update)
        ;
    object param = object(Param());
    param.attr("__doc__") = "A dictionary-like object containing the HTCondor configuration.";
    scope().attr("param") = param;
}
