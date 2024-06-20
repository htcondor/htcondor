Sock *
start_negotiator_command( long command, const char * addr ) {
    Daemon negotiator( DT_NEGOTIATOR, addr );
    Sock * s;
    {
        // FIXME: condor::ModuleLock ml;
        s = negotiator.startCommand(command, Stream::reli_sock, 0);
    }
    return s;
}


static PyObject *
_negotiator_command(PyObject *, PyObject * args) {
    // _negotiator_user_command(self, command, user)

    const char * addr = NULL;
    long command = -1;
    if(! PyArg_ParseTuple( args, "sl", & addr, & command )) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }

    Sock * s = start_negotiator_command(command, addr);
    if( s == NULL ) {
        // This was HTCondorIOError in version 1.
        PyErr_SetString( PyExc_HTCondorException, "Unable to connect to the negotiator" );
        return NULL;
    }

    bool rv = false;
    {
        // FIXME: condor::ModuleLock ml;
        rv = !s->end_of_message();
    }
    s->close();

    if( rv ) {
        // This was HTCondorIOError in version 1.
        PyErr_SetString( PyExc_HTCondorException, "Failed to send command to negotiator" );
        return NULL;
    }

    Py_RETURN_NONE;
}


static PyObject *
_negotiator_command_return(PyObject *, PyObject * args) {
    // _negotiator_user_command(self, command, user)

    const char * addr = NULL;
    long command = -1;
    if(! PyArg_ParseTuple( args, "sl", & addr, & command )) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }

    Sock * s = start_negotiator_command(command, addr);
    if( s == NULL ) {
        // This was HTCondorIOError in version 1.
        PyErr_SetString( PyExc_HTCondorException, "Unable to connect to the negotiator" );
        return NULL;
    }

    bool rv = false;
    ClassAd * returnAd = new ClassAd();
    {
        // FIXME: condor::ModuleLock ml;
        rv |= !s->end_of_message();
        s->decode();
        rv |= !getClassAdNoTypes(s, *returnAd);
        rv |= !s->end_of_message();
    }
    s->close();

    if( rv ) {
        // This was HTCondorIOError in version 1.
        PyErr_SetString( PyExc_HTCondorException, "Failed to send command to negotiator" );
        return NULL;
    }

    return py_new_classad2_classad(returnAd);
}


static PyObject *
_negotiator_command_user(PyObject *, PyObject * args) {
    // _negotiator_user_command(self, command, user)

    const char * addr = NULL;
    long command = -1;
    const char * user = NULL;
    if(! PyArg_ParseTuple( args, "sls", & addr, & command, & user )) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }

    Sock * s = start_negotiator_command(command, addr);
    if( s == NULL ) {
        // This was HTCondorIOError in version 1.
        PyErr_SetString( PyExc_HTCondorException, "Unable to connect to the negotiator" );
        return NULL;
    }

    bool rv = false;
    {
        // FIXME: condor::ModuleLock ml;
        rv = !s->put(user) || !s->end_of_message();
    }
    s->close();

    if( rv ) {
        // This was HTCondorIOError in version 1.
        PyErr_SetString( PyExc_HTCondorException, "Failed to send command to negotiator" );
        return NULL;
    }

    Py_RETURN_NONE;
}


static PyObject *
_negotiator_command_user_return(PyObject *, PyObject * args) {
    // _negotiator_user_command(self, command, user)

    const char * addr = NULL;
    long command = -1;
    const char * user = NULL;
    if(! PyArg_ParseTuple( args, "sls", & addr, & command, & user )) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }

    Sock * s = start_negotiator_command(command, addr);
    if( s == NULL ) {
        // This was HTCondorIOError in version 1.
        PyErr_SetString( PyExc_HTCondorException, "Unable to connect to the negotiator" );
        return NULL;
    }

    bool rv = false;
    ClassAd * returnAd = new ClassAd();
    {
        // FIXME: condor::ModuleLock ml;
        rv = !s->put(user) || !s->end_of_message();
        s->decode();
        rv |= !getClassAdNoTypes(s, *returnAd);
        rv |= !s->end_of_message();
    }
    s->close();

    if( rv ) {
        // This was HTCondorIOError in version 1.
        PyErr_SetString( PyExc_HTCondorException, "Failed to send command to negotiator" );
        return NULL;
    }

    return py_new_classad2_classad(returnAd);
}


static PyObject *
_negotiator_command_user_value(PyObject *, PyObject * args) {
    // _negotiator_user_command(self, command, user, value)

    const char * addr = NULL;
    long command = -1;
    const char * user = NULL;
    PyObject * py_val = NULL;
    if(! PyArg_ParseTuple( args, "slsO", & addr, & command, & user, & py_val )) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }

    int is_float = PyFloat_Check(py_val);
    int is_long = PyLong_Check(py_val);
    if(! (is_float || is_long)) {
        PyErr_SetString( PyExc_TypeError, "value must be a float or a long" );
        return NULL;
    }

    Sock * s = start_negotiator_command(command, addr);
    if( s == NULL ) {
        // This was HTCondorIOError in version 1.
        PyErr_SetString( PyExc_HTCondorException, "Unable to connect to the negotiator" );
        return NULL;
    }

    bool rv = false;
    {
        // FIXME: condor::ModuleLock ml;
        if( is_float ) {
            float value = (float)PyFloat_AsDouble(py_val);
            rv = !s->put(user) || !s->put(value) || !s->end_of_message();
        } else {
            long value = PyLong_AsLong(py_val);
            rv = !s->put(user) || !s->put(value) || !s->end_of_message();
        }
    }
    s->close();

    if( rv ) {
        // This was HTCondorIOError in version 1.
        PyErr_SetString( PyExc_HTCondorException, "Failed to send command to negotiator" );
        return NULL;
    }

    Py_RETURN_NONE;
}
