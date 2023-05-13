static PyObject *
_classad_init( PyObject *, PyObject * ) {
    Py_RETURN_NONE;
}


static PyObject *
_classad_to_string( PyObject *, PyObject * args ) {
    // _classad_to_string( self._handle )

    PyObject_Handle * handle = NULL;
    if(! PyArg_ParseTuple( args, "O", (PyObject **)& handle )) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }

    std::string str;
    classad::PrettyPrint pp;
    pp.Unparse( str, (ClassAd *)handle->t );

    return PyUnicode_FromString(str.c_str());
}


static PyObject *
_classad_get_item( PyObject *, PyObject * args ) {
    // _classad_get_item( self._handle, key )

    PyObject_Handle * handle = NULL;
    const char * key = NULL;
    if(! PyArg_ParseTuple( args, "Os", (PyObject **)& handle, & key )) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }

    auto * classAd = (ClassAd *)handle->t;
    classad::ExprTree * expr = classAd->Lookup( key );
    if(! expr) {
        PyErr_SetString( PyExc_KeyError, key );
        return NULL;
    }


    if( should_convert_to_python( expr ) ) {
        classad::Value v;
        if(! expr->Evaluate( v )) {
            // FIXME: ClassAdEvaluationError
            PyErr_SetString( PyExc_RuntimeError, "Failed to evaluate convertible expression" );
            return NULL;
        }

        return convert_value_to_python(v);
    }


    // If we can't convert the ExprTree into a Python object and return that,
    // make a deep copy of it, clear the parent scope, and return the
    // ExprTree Python type with its _handle member set to the deep copy.

    PyErr_SetString( PyExc_NotImplementedError, "_classad_get_item" );
    return NULL;
}
