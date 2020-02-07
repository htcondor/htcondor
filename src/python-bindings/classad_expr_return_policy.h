
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

//
// For a given result object, if it is a tuple `t` and `t[1]` potentially
// contains a reference to a parent ClassAd, then have t[1] hold a reference
// to the first argument.
//
// The use case is the iterator returned by the `ClassAdWrapper::items` method;
// the iterator, which holds a reference to the ClassAd, must be kept alive for
// as long as t[1] is alive.  Otherwise, t[1] may be able to reach ClassAd memory
// that has been garbage collected.
//

template <class BasePolicy_ = boost::python::default_call_policies>
struct tuple_classad_value_return_policy : BasePolicy_
{

    template <class ArgumentPackage>
    static PyObject* postcall(ArgumentPackage const& args_, PyObject* result)
    {

            // Grab a pointer to the 'patient', the first argument to this function call (aka, `self`).
        PyObject* patient = PyTuple_GET_ITEM(args_, 0);

        if (result == 0) return 0;

        result = BasePolicy_::postcall(args_, result);
        if (result == 0)
            return 0;

            // Grab a pointer to the 'nurse' that keeps the patient alive; it's assumed that this is
            // result[1] and that result is a tuple.
        if (!PyTuple_Check(result))
            return result;

        PyObject* nurse = PyTuple_GetItem(result, 1);
        if (!nurse)
            return 0;

            // Grab the registered PyTypeObject of an ExprTreeHolder.
        boost::python::type_info info = boost::python::type_id<ExprTreeHolder>();
        const boost::python::converter::registration* reg = boost::python::converter::registry::query(info);
        if (!reg) {Py_XDECREF(result); return 0;}

        PyTypeObject* type_obj = reg->get_class_object();
        if (!type_obj) {Py_XDECREF(result); return 0;}

            // If the nurse is an ExprTreeHolder, tie the lifetime to the patient and return.
        if (PyObject_TypeCheck(nurse, type_obj) && (boost::python::objects::make_nurse_and_patient(nurse, patient) == 0))
        {
            Py_XDECREF(result);
            return 0;
        }

            // Repeat the same procedure with the ClassAdWrapper type.
        info = boost::python::type_id<ClassAdWrapper>();
        reg = boost::python::converter::registry::query(info);
        if (!reg) {Py_XDECREF(result); return 0;}

        type_obj = reg->get_class_object();
        if (!type_obj) {Py_XDECREF(result); return 0;}

        if (PyObject_TypeCheck(nurse, type_obj) && (boost::python::objects::make_nurse_and_patient(nurse, patient) == 0))
        {
            Py_XDECREF(result);
            return 0;
        }

            // All is good, return the original result.
        return result;
    }
};


}

#endif
