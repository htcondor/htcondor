
#ifndef __CLASSAD_EXPR_RETURN_POLICY_H_
#define __CLASSAD_EXPR_RETURN_POLICY_H_

#include <boost/python/default_call_policies.hpp>
#include <boost/python/object/life_support.hpp>
#include <boost/python/converter/registry.hpp>
#include <boost/python/converter/registrations.hpp>
#include "exprtree_wrapper.h"
#include "classad_wrapper.h"

namespace condor {

template <class BasePolicy_ = boost::python::default_call_policies>
struct classad_expr_return_policy : BasePolicy_
{

    template <class ArgumentPackage>
    static PyObject* postcall(ArgumentPackage const& args_, PyObject* result)
    {

        PyObject* patient = PyTuple_GET_ITEM(args_, 0);
        PyObject* nurse = result;

        if (nurse == 0) return 0;

        result = BasePolicy_::postcall(args_, result);
        if (result == 0)
            return 0;

        boost::python::type_info info = boost::python::type_id<ExprTreeHolder>();
        const boost::python::converter::registration* reg = boost::python::converter::registry::query(info);
        if (!reg) {Py_XDECREF(result); return 0;}

        PyTypeObject* type_obj = reg->get_class_object();
        if (!type_obj) {Py_XDECREF(result); return 0;}

        if (PyObject_TypeCheck(result, type_obj) && (boost::python::objects::make_nurse_and_patient(nurse, patient) == 0))
        {
            Py_XDECREF(result);
            return 0;
        }

        info = boost::python::type_id<ClassAdWrapper>();
        reg = boost::python::converter::registry::query(info);
        if (!reg) {Py_XDECREF(result); return 0;}

        type_obj = reg->get_class_object();
        if (!type_obj) {Py_XDECREF(result); return 0;}

        if (PyObject_TypeCheck(result, type_obj) && (boost::python::objects::make_nurse_and_patient(nurse, patient) == 0))
        {
            Py_XDECREF(result);
            return 0;
        }


        return result;
    }
};


}

#endif
