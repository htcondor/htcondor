#include <boost/python.hpp>
#include <classad/source.h>

#include "old_boost.h"
#include "classad_wrapper.h"
#include "exprtree_wrapper.h"
#include "classad_expr_return_policy.h"

using namespace boost::python;


std::string ClassadLibraryVersion()
{
    std::string val;
    classad::ClassAdLibraryVersion(val);
    return val;
}


ClassAdWrapper *parseString(const std::string &str)
{
    classad::ClassAdParser parser;
    classad::ClassAd *result = parser.ParseClassAd(str);
    if (!result)
    {
        PyErr_SetString(PyExc_SyntaxError, "Unable to parse string into a ClassAd.");
        boost::python::throw_error_already_set();
    }
    ClassAdWrapper * wrapper_result = new ClassAdWrapper();
    wrapper_result->CopyFrom(*result);
    delete result;
    return wrapper_result;
}


ClassAdWrapper *parseFile(FILE *stream)
{
    classad::ClassAdParser parser;
    classad::ClassAd *result = parser.ParseClassAd(stream);
    if (!result)
    {
        PyErr_SetString(PyExc_SyntaxError, "Unable to parse input stream into a ClassAd.");
        boost::python::throw_error_already_set();
    }
    ClassAdWrapper * wrapper_result = new ClassAdWrapper();
    wrapper_result->CopyFrom(*result);
    delete result;
    return wrapper_result;
}

ClassAdWrapper *parseOld(object input)
{
    ClassAdWrapper * wrapper = new ClassAdWrapper();
    object input_list;
    extract<std::string> input_extract(input);
    if (input_extract.check())
    {
        input_list = input.attr("splitlines")();
    }
    else
    {
        input_list = input.attr("readlines")();
    }
    unsigned input_len = py_len(input_list);
    for (unsigned idx=0; idx<input_len; idx++)
    {
        object line = input_list[idx].attr("strip")();
        if (line.attr("startswith")("#"))
        {
            continue;
        }
        std::string line_str = extract<std::string>(line);
        if (!wrapper->Insert(line_str))
        {
            PyErr_SetString(PyExc_SyntaxError, line_str.c_str());
            throw_error_already_set();
        }
    }
    return wrapper;
}

void *convert_to_FILEptr(PyObject* obj) {
    return PyFile_Check(obj) ? PyFile_AsFile(obj) : 0;
}

struct classad_from_python_dict {

    static void* convertible(PyObject* obj_ptr)
    {
        return PyMapping_Check(obj_ptr) ? obj_ptr : 0;
    }

    static void construct(PyObject* obj,  boost::python::converter::rvalue_from_python_stage1_data* data)
    {
        void* storage = ((boost::python::converter::rvalue_from_python_storage<ClassAdWrapper>*)data)->storage.bytes;
        new (storage) ClassAdWrapper;
        boost::python::handle<> handle(obj);
        boost::python::object boost_obj(handle);
        static_cast<ClassAdWrapper*>(storage)->update(boost_obj);
        data->convertible = storage;
    }
};

BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(setdefault_overloads, setdefault, 1, 2);
BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(get_overloads, get, 1, 2);

BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(init_overloads, init, 0, 1);

BOOST_PYTHON_MODULE(classad)
{
    using namespace boost::python;

    def("version", ClassadLibraryVersion, "Return the version of the linked ClassAd library.");

    def("parse", parseString, return_value_policy<manage_new_object>());
    def("parse", parseFile, return_value_policy<manage_new_object>(),
        "Parse input into a ClassAd.\n"
        ":param input: A string or a file pointer.\n"
        ":return: A ClassAd object.");
    def("parseOld", parseOld, return_value_policy<manage_new_object>(),
        "Parse old ClassAd format input into a ClassAd.\n"
        ":param input: A string or a file pointer.\n"
        ":return: A ClassAd object.");

    class_<ClassAdWrapper, boost::noncopyable>("ClassAd", "A classified advertisement.")
        .def(init<std::string>())
        .def(init<boost::python::dict>())
        .def("__delitem__", &ClassAdWrapper::Delete)
        .def("__getitem__", &ClassAdWrapper::LookupWrap, condor::classad_expr_return_policy<>())
        .def("eval", &ClassAdWrapper::EvaluateAttrObject, "Evaluate the ClassAd attribute to a python object.")
        .def("__setitem__", &ClassAdWrapper::InsertAttrObject)
        .def("__str__", &ClassAdWrapper::toString)
        .def("__repr__", &ClassAdWrapper::toRepr)
        // I see no way to use the SetParentScope interface safely.
        // Delay exposing it to python until we absolutely have to!
        //.def("setParentScope", &ClassAdWrapper::SetParentScope)
        .def("__iter__", boost::python::range(&ClassAdWrapper::beginKeys, &ClassAdWrapper::endKeys))
        .def("keys", boost::python::range(&ClassAdWrapper::beginKeys, &ClassAdWrapper::endKeys))
        .def("values", boost::python::range(&ClassAdWrapper::beginValues, &ClassAdWrapper::endValues))
        .def("items", boost::python::range(&ClassAdWrapper::beginItems, &ClassAdWrapper::endItems))
        .def("__len__", &ClassAdWrapper::size)
        .def("lookup", &ClassAdWrapper::LookupExpr, condor::classad_expr_return_policy<>(), "Lookup an attribute and return a ClassAd expression.  This method will not attempt to evaluate it to a python object.")
        .def("printOld", &ClassAdWrapper::toOldString, "Represent this ClassAd as a string in the \"old ClassAd\" format.")
        .def("get", &ClassAdWrapper::get, get_overloads("Retrieve a value from the ClassAd"))
        .def("setdefault", &ClassAdWrapper::setdefault, setdefault_overloads("Set a default value for a ClassAd"))
        .def("update", &ClassAdWrapper::update, "Copy the contents of a given ClassAd into the current object")
        ;

    class_<ExprTreeHolder>("ExprTree", "An expression in the ClassAd language", init<std::string>())
        .def("__str__", &ExprTreeHolder::toString)
        .def("__repr__", &ExprTreeHolder::toRepr)
        .def("__getitem__", &ExprTreeHolder::getItem, condor::classad_expr_return_policy<>())
        .def("eval", &ExprTreeHolder::Evaluate)
        ;

    register_ptr_to_python< boost::shared_ptr<ClassAdWrapper> >();

    boost::python::enum_<classad::Value::ValueType>("Value")
        .value("Error", classad::Value::ERROR_VALUE)
        .value("Undefined", classad::Value::UNDEFINED_VALUE)
        ;

    boost::python::converter::registry::insert(convert_to_FILEptr,
        boost::python::type_id<FILE>());

    boost::python::converter::registry::push_back(
        &classad_from_python_dict::convertible,
        &classad_from_python_dict::construct,
        boost::python::type_id<ClassAdWrapper>());

}
