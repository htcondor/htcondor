static PyObject *
_submit_init( PyObject *, PyObject * args ) {
    PyObject * self = NULL;
    PyObject_Handle * handle = NULL;
    const char * lines = NULL;

    if(! PyArg_ParseTuple( args, "OOz", & self, (PyObject **)& handle, lines )) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }

    handle->t = new SubmitHash();
    handle->t.init(JSM_PYTHON_BINDINGS);

    
}
