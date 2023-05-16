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
