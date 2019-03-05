
// Note - python_bindings_common.h must be included before condor_common to avoid
// re-definition warnings.
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

static bool
python_invoke (const char *                 name,
               const classad::ArgumentList &arguments,
               classad::EvalState          &state,
               classad::Value              &result);


static classad::ClassAdFunctionMapping functions[] = 
{
    { "python_invoke",  (void *) python_invoke, 0 },
    { "",        NULL,                          0 }
};


extern "C"
{
    classad::ClassAdFunctionMapping *Init(void)
    {
        if (!Py_IsInitialized())
        {
#if PY_MAJOR_VERSION >= 3
            wchar_t pname[] = L"htcondor";
#else
            char pname[] = "htcondor";
#endif
            Py_SetProgramName(pname);
            Py_InitializeEx(0);
        }
        return functions;
    }

    void Register(void)
    {
        std::string modulesStr;
        if (!param(modulesStr, "CLASSAD_USER_PYTHON_MODULES"))
        {
            return;
        }
        StringList modules(modulesStr.c_str());
        modules.rewind();
        const char *tmpMod;
        while ((tmpMod = modules.next()))
        {
            try
            {
                py_import(boost::python::str(tmpMod));
            }
            catch (...)
            {
                // Ignore for now.
            }
        }
    }
}


static bool
problemExpression (const std::string &msg, classad::ExprTree *problem, classad::Value &result)
{
    result.SetErrorValue();
    classad::ClassAdUnParser up;
    std::string problem_str;
    up.Unparse(problem_str, problem);
    std::stringstream ss;
    ss << msg << "  Problem expression: " << problem_str;
    classad::CondorErrMsg = ss.str();
    return false;
}


static std::string
handle_pyerror()
{
    PyObject *exc,*val,*tb;
    boost::python::object formatted_list, formatted;
    PyErr_Fetch(&exc,&val,&tb);
    boost::python::handle<> hexc(exc), hval(boost::python::allow_null(val)), htb(boost::python::allow_null(tb)); 
    boost::python::object traceback(py_import("traceback"));
    boost::python::object format_exception(traceback.attr("format_exception"));
    formatted_list = format_exception(hexc,hval,htb);
    formatted = boost::python::str("\n").join(formatted_list);
    return boost::python::extract<std::string>(formatted);
}


static bool
python_invoke_internal (boost::python::object                        pyFunc,
                        const classad::ArgumentList                 &arguments,
                        classad::EvalState                          &state,
                        classad::Value                              &result)
{
    bool acceptState = checkAcceptsState(pyFunc);

    classad::ArgumentList::const_iterator args = arguments.begin();
    args++; args++;

    boost::python::list pyArgs;
    for (; args != arguments.end(); args++)
    {
        ExprTreeHolder holder(*args, false);
        if (holder.ShouldEvaluate())
        {
            pyArgs.append(holder.Evaluate());
        }
        else
        {
            classad::ExprTree * exprTree = (*args)->Copy();
            ExprTreeHolder myExpr(exprTree, true);
            pyArgs.append(myExpr);
        }
    }

    boost::python::dict pyKw;
    if (acceptState && state.curAd)
    {
        boost::shared_ptr<ClassAdWrapper> wrapper(new ClassAdWrapper());
        wrapper->CopyFrom(*(state.curAd));
        pyKw["state"] = wrapper;
    }

    boost::python::object pyResult = py_import("__main__").attr("__builtins__").attr("apply")(pyFunc, pyArgs, pyKw);
    classad::ExprTree* exprTreeResult = convert_python_to_exprtree(pyResult);
    if (!exprTreeResult || !exprTreeResult->Evaluate(state, result))
    {
        THROW_EX(ClassAdValueError, "Unable to convert python function result to ClassAd value");
    }
    return true;
}


static bool
python_invoke (const char *                 name,
               const classad::ArgumentList &arguments,
               classad::EvalState          &state,
               classad::Value              &result)
{
    if ((arguments.size() < 2))
    {
        std::stringstream ss;
        result.SetErrorValue();
        ss << "Invalid number of arguments passed to " << name << "; " << arguments.size() << " given, at least 2 required.";
        classad::CondorErrMsg = ss.str();
        return false;
    }

    classad::Value val;
    if (!arguments[0]->Evaluate(state, val))
    {
        return problemExpression("Unable to evaluate first argument.", arguments[0], result);
    }
    std::string moduleName;
    if (!val.IsStringValue(moduleName))
    {
        return problemExpression("Unable to evaluate first argument to string.", arguments[0], result);
    }
    std::string modulesStr;
    if (param(modulesStr, "CLASSAD_USER_PYTHON_MODULES"))
    {
        StringList modules(modulesStr.c_str());
        modules.rewind();
        const char *tmpMod;
        bool whitelisted = false;
        while ((tmpMod = modules.next()))
        {
            if (moduleName == tmpMod)
            {
                whitelisted = true;
                break;
            }
        }
        if (!whitelisted)
        {
            classad::CondorErrMsg = "Requested module " + moduleName + " is not in the list of CLASSAD_USER_PYTHON_MODULES.";
            result.SetErrorValue();
            return false;
        }
    }
    else
    {
        classad::CondorErrMsg = "CLASSAD_USER_PYTHON_MODULES config option is not set.";
        result.SetErrorValue();
        return false;
    }

    if (!arguments[1]->Evaluate(state, val))
    {
        return problemExpression("Unable to evaluate second argument.", arguments[1], result);
    }
    std::string functionName;
    if (!val.IsStringValue(functionName))
    {
        return problemExpression("Unable to evaluate second argument to string.", arguments[1], result);
    }
    if (functionName[0] == '_')
    {
        classad::CondorErrMsg = "Function names starting with underscore are forbidden (" + functionName + ").";
        result.SetErrorValue();
        return false;
    }

    try
    {
        boost::python::object module = py_import(boost::python::str(moduleName));
        boost::python::object pyFunc = module.attr(functionName.c_str());
        return python_invoke_internal(pyFunc, arguments, state, result);
    }
    catch (boost::python::error_already_set&)
    {
        result.SetErrorValue();
        if (PyErr_Occurred())
        {
            classad::CondorErrMsg = handle_pyerror(); 
            // Keep around any python exceptions on the off-chance we
            // return to a python context in the future.
            //PyErr_Clear();
        }
        return true;
    }
    catch (...) // If this is being invoked from python, this will *not* clear the python exception
                // However, it does prevent an exception being thrown into the ClassAd code... which is not ready for it!
    {
        classad::CondorErrMsg = "Unknown C++ exception caught.";
        result.SetErrorValue();
        return true;
    }
}

