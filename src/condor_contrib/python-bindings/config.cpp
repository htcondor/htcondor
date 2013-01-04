
#include "condor_common.h"
#include "condor_config.h"
#include "condor_version.h"

#include <boost/python.hpp>

using namespace boost::python;

struct Param
{
    std::string getitem(const std::string &attr)
    {
        std::string result;
        if (!param(result, attr.c_str()))
        {
            PyErr_SetString(PyExc_KeyError, attr.c_str());
            throw_error_already_set();
        }
        return result;
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
};

std::string CondorVersionWrapper() { return CondorVersion(); }

std::string CondorPlatformWrapper() { return CondorPlatform(); }

BOOST_PYTHON_FUNCTION_OVERLOADS(config_overloads, config, 0, 3);

void export_config()
{
    config();
    def("version", CondorVersionWrapper, "Returns the version of HTCondor this module is linked against.");
    def("platform", CondorPlatformWrapper, "Returns the platform of HTCondor this module is running on.");
    def("reload_config", config, config_overloads("Reload the HTCondor configuration from disk."));
    class_<Param>("_Param")
        .def("__getitem__", &Param::getitem)
        .def("__setitem__", &Param::setitem)
        .def("setdefault", &Param::setdefault)
        ;
    object param = object(Param());
    param.attr("__doc__") = "A dictionary-like object containing the HTCondor configuration.";
    scope().attr("param") = param;
}
