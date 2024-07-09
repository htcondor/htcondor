bool
start_config_command( int cmd, ReliSock & sock, const ClassAd & location ) {
    std::string address;
    if(! location.EvaluateAttrString( ATTR_MY_ADDRESS, address )) {
        // This was HTCondorValueError in version 1.
        PyErr_SetString( PyExc_ValueError, "Address not available in location ClassAd." );
        return false;
    }

    // Not sure we actually need to make this copy, but version 1 did.
    ClassAd copy;
    copy.CopyFrom(location);

    Daemon d( &copy, DT_GENERIC, NULL );

    CondorError errorStack;
    bool result = sock.connect( d.addr(), 0, false, & errorStack );
    if(! result) {
        dprintf( D_NETWORK | D_VERBOSE, "start_config_command(): sock.connect() failed: %s\n", errorStack.getFullText().c_str() );

        // This was HTCondorValueError in version 1.
        PyErr_SetString( PyExc_HTCondorException, "Failed to connect to daemon." );
        return false;
    }

    result = d.startCommand( cmd, & sock, 0, & errorStack );
    if(! result) {
        dprintf( D_NETWORK | D_VERBOSE, "start_config_command(): d.startCommand() failed: %s\n", errorStack.getFullText().c_str() );

        // This was HTCondorIOError in version 1.
        PyErr_SetString( PyExc_HTCondorException, "Failed to start command." );
        return false;
    }

    return true;
}


static PyObject *
_remote_param_keys( PyObject *, PyObject * args ) {
    // _param_keys(self.location._handle)
    PyObject_Handle * handle = NULL;

    if(! PyArg_ParseTuple( args, "O", (PyObject **)& handle )) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }

    auto * location = (ClassAd * )handle->t;

    ReliSock sock;
    bool result = start_config_command( DC_CONFIG_VAL, sock, * location );
    if(! result) {
        // start_config_command() has already set an exception for us.
        return NULL;
    }

    sock.encode();

    std::string payload = "?names";
    if(! sock.put(payload)) {
        // This was HTCondorIOError in version 1.
        PyErr_SetString( PyExc_HTCondorException, "Failed to send request for parameter names." );
        return NULL;
    }
    if(! sock.end_of_message()) {
        // This was HTCondorIOError in version 1.
        PyErr_SetString( PyExc_HTCondorException, "Failed to send EOM for parameter names." );
        return NULL;
    }

    sock.decode();

    std::string reply;
    if(! sock.code(reply)) {
        // This was HTCondorIOError in version 1.
        PyErr_SetString( PyExc_HTCondorException, "Failed to receive reply for parameter names." );
        return NULL;
    }

    if( reply == "Not defined" ) {
        if(! sock.end_of_message()) {
            // This was HTCondorIOError in version 1.
            PyErr_SetString( PyExc_HTCondorException, "Failed to receive EOM from remote daemon (unsupported version)." );
            return NULL;
        }

        // Strictly speaking, this could be a reply from a daemon earlier
        // than version 8.1.2, but let's not add another round-trip to
        // half-ass a check by looking for a known knob name.

        // This was HTCondorReplyError in version 1.
        PyErr_SetString( PyExc_HTCondorException, "Not authorized to query remote daemon." );
        return NULL;
    }
    if( reply[0] == '!' ) {
        sock.end_of_message();

        // This was HTCondorIOError in version 1.
        PyErr_SetString( PyExc_HTCondorException, "Remote daemon failed to get parameter names." );
        return NULL;
    }

    std::vector<std::string> keys;
    if(! reply.empty()) {
        keys.push_back(reply.c_str());
    }

    std::string key;
    while(! sock.peek_end_of_message()) {
        if(! sock.code(key)) {
            // This was HTCondorIOError in version 1.
            PyErr_SetString( PyExc_HTCondorException, "Failed to read parameter name." );
            return NULL;
        }

        keys.push_back(key.c_str());
    }

    if(! sock.end_of_message()) {
        // This was HTCondorIOError in version 1.
        PyErr_SetString( PyExc_HTCondorException, "Failed to receive final EOM for parameter names." );
        return NULL;
    }

    return PyUnicode_FromString(join(keys, ",").c_str());
}


static PyObject *
_remote_param_get( PyObject *, PyObject * args ) {
    // _param_get(self.location._handle, key)
    PyObject_Handle * handle = NULL;
    const char * key = NULL;

    if(! PyArg_ParseTuple( args, "Os", (PyObject **)& handle, & key )) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }

    auto * location = (ClassAd * )handle->t;

    ReliSock sock;
    bool result = start_config_command( CONFIG_VAL, sock, * location );
    if(! result) {
        // start_config_command() has already set an exception for us.
        return NULL;
    }


    sock.encode();

    if(! sock.put(key)) {
        // This was HTCondorIOError in version 1.
        PyErr_SetString( PyExc_HTCondorException, "Can't send requested param name." );
        return NULL;
    }
    if(! sock.end_of_message()) {
        // This was HTCondorIOError in version 1.
        PyErr_SetString( PyExc_HTCondorException, "Can't send EOM for param name." );
        return NULL;
    }


    sock.decode();

    std::string value;
    if(! sock.code(value)) {
        // This was HTCondorIOError in version 1.
        PyErr_SetString( PyExc_HTCondorException, "Failed to receive reply from daemon for param value." );
        return NULL;
    }
    if(! sock.end_of_message()) {
        // This was HTCondorIOError in version 1.
        PyErr_SetString( PyExc_HTCondorException, "Failed to receive EOM from daemon for param value." );
        return NULL;
    }


    return PyUnicode_FromString(value.c_str());
}


static PyObject *
_remote_param_set( PyObject *, PyObject * args ) {
    // _param_get(self.location._handle, key, value)
    PyObject_Handle * handle = NULL;
    const char * key = NULL;
    const char * value = NULL;

    if(! PyArg_ParseTuple( args, "Oss", (PyObject **)& handle, & key, & value )) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }

    auto * location = (ClassAd * )handle->t;

    ReliSock sock;
    bool result = start_config_command( DC_CONFIG_RUNTIME, sock, * location );
    if(! result) {
        // start_config_command() has already set an exception for us.
        return NULL;
    }

    sock.encode();

    if(! sock.put(key)) {
        // This was HTCondorIOError in version 1.
        PyErr_SetString( PyExc_HTCondorException, "Can't send requested param name." );
        return NULL;
    }
    std::string wtaf;
    formatstr( wtaf, "%s = %s", key, value );
    if(! sock.code(wtaf)) {
        // This was HTCondorIOError in version 1.
        PyErr_SetString( PyExc_HTCondorException, "Can't send requested param value." );
        return NULL;
    }
    if(! sock.end_of_message()) {
        // This was HTCondorIOError in version 1.
        PyErr_SetString( PyExc_HTCondorException, "Can't send EOM for param name." );
        return NULL;
    }

    sock.decode();

    int rval = 0;
    if(! sock.code(rval)) {
        // This was HTCondorIOError in version 1.
        PyErr_SetString( PyExc_HTCondorException, "Failed to receive reply from daemon after setting param." );
        return NULL;
    }
    if(! sock.end_of_message()) {
        // This was HTCondorIOError in version 1.
        PyErr_SetString( PyExc_HTCondorException, "Failed to receive EOM from daemon after setting param value." );
        return NULL;
    }
    if( rval < 0 ) {
        // This was HTCondorReplyError in version 1.
        PyErr_SetString( PyExc_HTCondorException, "Failed to set remote daemon parameter." );
        return NULL;
    }

    Py_RETURN_NONE;
}
