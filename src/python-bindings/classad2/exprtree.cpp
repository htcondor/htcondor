static PyObject *
_exprtree_init(PyObject *, PyObject * args) {
	// _exprtree_init( self, self._handle )

    PyObject * self = NULL;
    PyObject_Handle * handle = NULL;
    if(! PyArg_ParseTuple( args, "OO", & self, (PyObject **)& handle )) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }

    // We don't know which kind of ExprTree this is going to be yet,
    // so we can't initialize it to anything (ExprTree proper is abstract).
    handle->t = NULL;
    handle->f = [](void *& v) { dprintf( D_ALWAYS, "[ExprTree]\n" ); delete (classad::ExprTree *)v; v = NULL; };
    Py_RETURN_NONE;
}


static PyObject *
_exprtree_eq( PyObject *, PyObject * args ) {
    // _classad_eq( self._handle, other._handle )

    PyObject_Handle * self = NULL;
    PyObject_Handle * other = NULL;
    if(! PyArg_ParseTuple( args, "OO", (PyObject **)& self, (PyObject **)& other)) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }

    if( *(classad::ExprTree *)self->t == *(classad::ExprTree *)other->t ) {
        Py_RETURN_TRUE;
    } else {
        Py_RETURN_FALSE;
    }
}
