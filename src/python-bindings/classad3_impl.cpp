#include "condor_python.h"

#include "classad/source.h"
#include "classad/classad.h"
#include "classad/sink.h"
#include "classad/matchClassad.h"

#include "condor_config.h"
#include "dc_collector.h"


static PyObject *
_external_refs( PyObject * /* self */, PyObject * args ) {
	const char * ad_str = NULL;
	const char * expr_str = NULL;

	// 's': valid new ClassAds don't allow embedded NULs, I hope.
	if(! PyArg_ParseTuple( args, "ss", & ad_str, & expr_str )) {
		// PyArg_ParseTuple() has already set an exception for us.
		return NULL;
	}


	classad::ClassAd * ad = NULL;
	classad::ClassAdParser parser;
	ad = parser.ParseClassAd( ad_str, true );
	if( ad == NULL ) {
		PyErr_SetString( PyExc_AssertionError, "_external_refs() passed invalid ad_str" );
		return NULL;
	}


	classad::ExprTree * expr = parser.ParseExpression( expr_str, true );
	if(expr == nullptr) {
		delete ad;

		PyErr_SetString( PyExc_AssertionError, "_external_refs() passed invalid expr_str" );
		return NULL;
	}


	classad::References externalReferences;
	if(! ad->GetExternalReferences( expr, externalReferences, true )) {
		delete expr;
		delete ad;

		// This was ClassAdValueError in version 1.
		PyErr_SetString( PyExc_ValueError, "Unable to determine external reference." );
		return NULL;
	}


	delete expr;
	delete ad;

    bool first = true;
    std::string er_string;
    for( const auto & r : externalReferences ) {
        if( first ) { first = false; er_string = r; continue; }
        // Luckily, commas aren't valid in ClassAd attribute names.
        formatstr_cat( er_string, ",%s", r.c_str() );
    }

	return PyUnicode_FromString( er_string.c_str() );
}


static PyObject *
_internal_refs( PyObject * /* self */, PyObject * args ) {
	const char * ad_str = NULL;
	const char * expr_str = NULL;

	// 's': valid new ClassAds don't allow embedded NULs, I hope.
	if(! PyArg_ParseTuple( args, "ss", & ad_str, & expr_str )) {
		// PyArg_ParseTuple() has already set an exception for us.
		return NULL;
	}


	classad::ClassAd * ad = NULL;
	classad::ClassAdParser parser;
	ad = parser.ParseClassAd( ad_str, true );
	if( ad == NULL ) {
		PyErr_SetString( PyExc_AssertionError, "_internal_refs() passed invalid ad_str" );
		return NULL;
	}


	classad::ExprTree * expr = parser.ParseExpression( expr_str, true );
	if(expr == nullptr) {
		delete ad;

		PyErr_SetString( PyExc_AssertionError, "_internal_refs() passed invalid expr_str" );
		return NULL;
	}


	classad::References internalReferences;
	if(! ad->GetInternalReferences( expr, internalReferences, true )) {
		delete expr;
		delete ad;

		// This was ClassAdValueError in version 1.
		PyErr_SetString( PyExc_ValueError, "Unable to determine internal reference." );
		return NULL;
	}


	delete expr;
	delete ad;

    bool first = true;
    std::string ir_string;
    for( const auto & r : internalReferences ) {
        if( first ) { first = false; ir_string = r; continue; }
        // Luckily, commas aren't valid in ClassAd attribute names.
        formatstr_cat( ir_string, ",%s", r.c_str() );
    }

	return PyUnicode_FromString( ir_string.c_str() );
}


static PyObject *
_flatten( PyObject * /* self */, PyObject * args ) {
	const char * expr_str = NULL;
	const char * scope_str = NULL;

	// 's': valid new ClassAds don't allow embedded NULs, I hope.
	if(! PyArg_ParseTuple( args, "ss", & expr_str, & scope_str )) {
		// PyArg_ParseTuple() has already set an exception for us.
		return NULL;
	}


	classad::ClassAdParser parser;

	classad::ExprTree * expr = parser.ParseExpression( expr_str, true );
	if(expr == nullptr) {
		PyErr_SetString( PyExc_AssertionError, "_flatten() passed invalid expr_str" );
		return NULL;
	}
	// FIXME: assign expr to a classad_shared_ptr now?

	classad::ClassAd * scope = NULL;
	if( scope_str[0] != '\0' ) {
		scope = parser.ParseClassAd( scope_str, true );
		if( scope == NULL ) {
			delete( expr );  // FIXME?

			PyErr_SetString( PyExc_AssertionError, "_flatten() passed invalid scope_str" );
			return NULL;
		}
	}

	classad::Value v;
	ExprTree * flat = NULL;
	if(! scope->Flatten( expr, v, flat )) {
		delete( expr );
		delete( scope );

		// FIXME: See below.
		PyErr_SetString( PyExc_ValueError, "Expression failed to flatten" );
		return NULL;
	}

	std::string result;
	classad::ClassAdUnParser unparser;
	if( flat != NULL ) {
		unparser.Unparse( result, flat );
	} else {
		unparser.Unparse( result, v );
	}
	return PyUnicode_FromString( result.c_str() );
}


bool
EvaluateLooseExpr( classad::ExprTree * expr,
                   classad::ClassAd * my, classad::ClassAd * target,
                   classad::Value & value ) {
	const classad::ClassAd * originalScope = expr->GetParentScope();
	expr->SetParentScope(my);

	bool rv = false;
	if( target == my || target == NULL ) {
		rv = expr->Evaluate( value );
	} else {
		classad::MatchClassAd matchAd( my, target );
		rv = expr->Evaluate( value );
		matchAd.RemoveLeftAd();
		matchAd.RemoveRightAd();
	}

	expr->SetParentScope(originalScope);
	return rv;
}


static PyObject *
_evaluate( PyObject * /* self */, PyObject * args ) {
	const char * expr_str = NULL;
	const char * scope_str = NULL;
	const char * target_str = NULL;

	// 's': valid new ClassAds don't allow embedded NULs, I hope.
	if(! PyArg_ParseTuple( args, "sss", & expr_str, & scope_str, & target_str )) {
		// PyArg_ParseTuple() has already set an exception for us.
		return NULL;
	}


	classad::ClassAdParser parser;

	classad::ExprTree * expr = parser.ParseExpression( expr_str, true );
	if(expr == nullptr) {
		PyErr_SetString( PyExc_AssertionError, "_evaluate() passed invalid expr_str" );
		return NULL;
	}
	// FIXME: assign expr to a classad_shared_ptr now?

	classad::ClassAd * scope = NULL;
	if( scope_str[0] != '\0' ) {
		scope = parser.ParseClassAd( scope_str, true );
		if( scope == NULL ) {
			delete( expr );  // FIXME?

			PyErr_SetString( PyExc_AssertionError, "_evaluate() passed invalid scope_str" );
			return NULL;
		}
	}

	classad::ClassAd * target = NULL;
	if( target_str[0] != '\0' ) {
		target = parser.ParseClassAd( target_str, true );
		if( target == NULL ) {
			delete( expr ); // FIXME?
			delete( scope ); // FIXME?

			PyErr_SetString( PyExc_AssertionError, "_evaluate() passed invalid target_str" );
			return NULL;
		}
	}


	bool rv = false;
	classad::Value value;
	if( scope != NULL ) {
		rv = EvaluateLooseExpr( expr, scope, target, value );
	} else {
		classad::EvalState state;
		rv = expr->Evaluate( state, value );
	}
	if( PyErr_Occurred() ) {
		delete( expr ); // FIXME?
		delete( scope ); // FIXME?
		delete( target ); // FIXME?

		return NULL;
	}
	if(! rv) {
		delete( expr ); // FIXME?
		delete( scope ); // FIXME?
		delete( target ); // FIXME?

		// This was ClassAdErrorValue in version 1.
		// (Should we do the translation in the Python layer?)
		PyErr_SetString( PyExc_ValueError, "Expression failed to evaluate" );
		return NULL;
	}

	delete( expr ); // FIXME?
	delete( scope ); // FIXME?
	delete( target ); // FIXME?

	// FIXME: if value is classad::Value::ERROR_VALUE, raise ClassAdErrorValue,
	// or depend on the Python layer to do that instead?
	std::string result;
	// Return a JSON-formatted string so we can use Python's built-in
	// JSON converter to produce a Python value.
	classad::ClassAdJsonUnParser unparser;
	unparser.Unparse( result, value );

	return PyUnicode_FromString( result.c_str() );
}


static PyObject *
_classad_quoted( PyObject * /* self */, PyObject * args ) {
	const char * str_to_quote = NULL;
	// 's': ClassAd strings don't allow embedded NULs, I hope.
	if(! PyArg_ParseTuple( args, "s", & str_to_quote )) {
		// PyArg_ParseTuple() has already set an exception for us.
		return NULL;
	}

	auto l = classad::Literal::MakeString( str_to_quote );
	// How does this bypass ExprTree not being constructible?
	classad_shared_ptr<classad::ExprTree> expr( l );

	std::string result;
	classad::ClassAdUnParser unparser;
	unparser.Unparse( result, expr.get() );

	return PyUnicode_FromString( result.c_str() );
}


static PyObject *
_canonicalize_expr_string( PyObject * /* self */, PyObject * args ) {
	const char * expr_string = NULL;
	// 's': Valid ClassAd expressions don't have embedded NULs.
	if(! PyArg_ParseTuple( args, "s", & expr_string )) {
		// PyArg_ParseTuple() has already set an exception for us.
		return NULL;
	}

	classad::ClassAdParser parser;
	classad::ExprTree * expr = parser.ParseExpression( expr_string, true );
	if( expr ) {
		std::string result;
		classad::ClassAdUnParser unparser;
		unparser.Unparse( result, expr );

		return PyUnicode_FromString( result.c_str() );
	}

	PyErr_SetString( PyExc_ValueError, "Invalid ClassAd expresison" );
	return NULL;
}


static PyObject *
_collector_query_hack_for_demo_purposes( PyObject *, PyObject * ) {
	CondorQuery query(STARTD_AD);

	// This uses the config system, which makes us all sad.
	CollectorList * collector_list = CollectorList::create();
	if( collector_list == NULL ) {
		PyErr_SetString( PyExc_RuntimeError, "Failed to create collector list" );
		return NULL;
	}

	ClassAdList reply;
	QueryResult qr = collector_list->query(query, reply);

	if( qr != Q_OK ) {
		delete collector_list; // FIXME
		PyErr_SetString( PyExc_RuntimeError, "Query failed for some reason" );
		return NULL;
	}

	ClassAd * ad = NULL;
	PyObject * result = PyList_New(0);
	for( reply.Open(); (ad = reply.Next()); ) {
		std::string adAsJSON;
		classad::ClassAdJsonUnParser cajup;
		cajup.Unparse( adAsJSON, ad );

		int rv = PyList_Append( result, PyUnicode_FromString( adAsJSON.c_str() ) );
		if( rv == -1 ) {
			// PyList_Append() has already set an exception for us.
			delete collector_list;
			Py_DECREF(result);
			return NULL;
		}
	}


	delete collector_list;
	return result;
}


static PyMethodDef classad3_impl_methods[] = {
	{"_canonicalize_expr_string", & _canonicalize_expr_string, METH_VARARGS, NULL /* no function documentation */},
	{"_classad_quoted", & _classad_quoted, METH_VARARGS, NULL /* no function documentation */},
	{"_evaluate", & _evaluate, METH_VARARGS, NULL /* no function documentation */},
	{"_flatten", & _flatten, METH_VARARGS, NULL /* no function documentation */},
	{"_external_refs", & _external_refs, METH_VARARGS, NULL /* no function documentation */},
	{"_internal_refs", & _internal_refs, METH_VARARGS, NULL /* no function documentation */},
	{"_collector_query_hack_for_demo_purposes", & _collector_query_hack_for_demo_purposes, METH_VARARGS, NULL /* no function documentation */},
	{NULL, NULL, 0, NULL}
};


static struct PyModuleDef classad3_impl_module = {
	.m_base = PyModuleDef_HEAD_INIT,
	.m_name = "classad3_impl",
	.m_doc = NULL, /* no module documentation */
	.m_size = -1, /* this module has global state */
	.m_methods = classad3_impl_methods,

	// In C99, we could just leave these off.
	.m_slots = NULL,
	.m_traverse = NULL,
	.m_clear = NULL,
	.m_free = NULL,
};


PyMODINIT_FUNC
PyInit_classad3_impl(void) {
	// Initialization for HTCondor.  *sigh*
	config();
	dprintf_set_tool_debug( "TOOL", 0 );

	return PyModule_Create(& classad3_impl_module);
}
