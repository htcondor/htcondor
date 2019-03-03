
#include "python_bindings_common.h"

#include "export_headers.h"
#include "classad_wrapper.h"
#include "exception_utils.h"

BOOST_PYTHON_MODULE(classad)
{
	export_classad();

	// Allows us to THROW_EX(ClassAdParseError...):
	PyExc_ClassAdParseError = PyErr_NewException( "classad.ParseError",
		PyExc_RuntimeError, NULL );
	if(! PyExc_ClassAdParseError) {
		boost::python::throw_error_already_set();
	}

	// Adds the exception to the module, which appears to the convention:
	boost::python::scope().attr( "ParseError" ) =
		boost::python::handle<>( boost::python::borrowed(
			PyExc_ClassAdParseError ) );

	// Allows us to THROW_EX(ClassAdEvaluationError...);
	PyExc_ClassAdEvaluationError = CreateExceptionInModule(
		"classad.EvaluationError", "EvaluationError", PyExc_RuntimeError );
}
