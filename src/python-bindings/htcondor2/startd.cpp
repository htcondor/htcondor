static PyObject *
_startd_drain_jobs(PyObject *, PyObject * args) {
    // _startd_drain_jobs(addr, drain_type, on_completion, check_expr, start_expr, reason)

    const char * addr = NULL;
    long drain_type = -1;
    long on_completion = -1;
    const char * check = NULL;
    const char * start = NULL;
    const char * reason = NULL;
    if(! PyArg_ParseTuple( args, "sllzzz", & addr, & drain_type, & on_completion, & check, & start, & reason )) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }

    DCStartd startd(addr);
    std::string requestID;
    bool r = startd.drainJobs( drain_type, reason, on_completion, check, start, requestID );
    if(! r) {
        // This was HTCondorReplyError in version 1.
        PyErr_SetString( PyExc_HTCondorException, "Startd failed to start draining jobs." );
        return NULL;
    }

    return PyUnicode_FromString( requestID.c_str() );
}

static PyObject *
_startd_cancel_drain_jobs(PyObject *, PyObject * args) {
    // _startd_cancel_drain_jobs(addr, request_id)

    const char * addr = NULL;
    const char * request_id = NULL;
    if(! PyArg_ParseTuple( args, "sz", & addr, & request_id )) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }

    DCStartd startd(addr);
    bool r = startd.cancelDrainJobs( request_id );
    if(! r) {
        // This was HTCondorReplyError in version 1.
        PyErr_SetString( PyExc_HTCondorException, "Startd failed to cancel draining jobs." );
        return NULL;
    }

    Py_RETURN_NONE;
}

