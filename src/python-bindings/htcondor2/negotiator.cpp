static PyObject *
_negotiator_init(PyObject *, PyObject * args) {
    // _negotiator_init( self, self._handle )

    PyObject * self = NULL;
    PyObject_Handle * handle = NULL;
    if(! PyArg_ParseTuple( args, "OO", & self, (PyObject **)& handle )) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }

    handle->t = NULL;
    handle->f = [](void *& v) { dprintf( D_ALWAYS, "[negotiator]\n" ); delete (classad::ExprTree *)v; v = NULL; };
    Py_RETURN_NONE;
}
