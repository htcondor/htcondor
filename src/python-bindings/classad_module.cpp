
#include "python_bindings_common.h"

#include "export_headers.h"
#include "classad_wrapper.h"
#include "exception_utils.h"

BOOST_PYTHON_MODULE(classad)
{
	export_classad();

	// Allows us to THROW_EX(ClassAdParseError...);
	PyExc_ClassAdParseError = CreateExceptionInModule(
		"classad.ParseError", "ParseError", PyExc_ValueError );

	// Allows us to THROW_EX(ClassAdEvaluationError...);
	PyExc_ClassAdEvaluationError = CreateExceptionInModule(
		"classad.EvaluationError", "EvaluationError", PyExc_ValueError );
}
