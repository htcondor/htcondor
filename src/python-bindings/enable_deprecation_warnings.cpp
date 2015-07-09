
// Note - python_bindings_common.h must be included before condor_common to avoid
// re-definition warnings.
#include "python_bindings_common.h"

#include "condor_common.h"
#include "condor_config.h"

#include <boost/python.hpp>

#include "old_boost.h"

void enable_deprecation_warnings()
{
    bool do_warnings = param_boolean("ENABLE_DEPRECATION_WARNINGS", true);

    boost::python::object warnings_module = py_import(boost::python::str("warnings"));
    boost::python::object exceptions_module = py_import(boost::python::str("exceptions"));
    boost::python::object warning_class = exceptions_module.attr("DeprecationWarning");
    warnings_module.attr("filterwarnings")(do_warnings ? "default" : "ignore", "ClassAd Deprecation:.*", warning_class, ".*");
}

