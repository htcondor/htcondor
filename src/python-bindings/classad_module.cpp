
#include "python_bindings_common.h"

#include "export_headers.h"
#include "classad_wrapper.h"
#include "exception_utils.h"

BOOST_PYTHON_MODULE(classad)
{
	export_classad();

	PyExc_ClassAdException = CreateExceptionInModule(
		"classad.ClassAdException", "ClassAdException", PyExc_Exception );

	PyExc_ClassAdEnumError = CreateExceptionInModule(
		"classad.ClassAdEnumError", "ClassAdEnumError",
		PyExc_ClassAdException, PyExc_TypeError );
	PyExc_ClassAdEvaluationError = CreateExceptionInModule(
		"classad.ClassAdEvaluationError", "ClassAdEvaluationError",
		PyExc_ClassAdException, PyExc_TypeError, PyExc_RuntimeError );
	PyExc_ClassAdInternalError = CreateExceptionInModule(
		"classad.ClassAdInternalError", "ClassAdInternalError",
		PyExc_ClassAdException, PyExc_ValueError, PyExc_RuntimeError );
	PyExc_ClassAdParseError = CreateExceptionInModule(
		"classad.ClassAdParseError", "ClassAdParseError",
		PyExc_ClassAdException, PyExc_ValueError, PyExc_SyntaxError,
		PyExc_RuntimeError );
	PyExc_ClassAdValueError = CreateExceptionInModule(
		"classad.ClassAdValueError", "ClassAdValueError",
		PyExc_ClassAdException, PyExc_ValueError, PyExc_RuntimeError,
		PyExc_TypeError );
	PyExc_ClassAdTypeError = CreateExceptionInModule(
		"classad.ClassAdTypeError", "ClassAdTypeError",
		PyExc_ClassAdException, PyExc_TypeError, PyExc_ValueError );
	PyExc_ClassAdOSError = CreateExceptionInModule(
		"classad.ClassAdOSError", "ClassAdOSError",
		PyExc_ClassAdException, PyExc_OSError, PyExc_RuntimeError );
}
