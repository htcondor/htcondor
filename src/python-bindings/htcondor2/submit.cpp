struct SubmitBlob {
    static MACRO_SOURCE EmptyMacroSrc;

    public:
        SubmitBlob() :
            m_src_pystring(EmptyMacroSrc),
            m_ms_inline("", 0, EmptyMacroSrc),
            EmptyItemString{'\0'}
        {
            m_hash.init(JSM_PYTHON_BINDINGS);
            // If we want to differentiate between the Python string
            // passed to from_lines() and the Python string passed to
            // set_queue_args(), we can move this line to those functions
            // and unique-ify things appropriately.
            m_hash.insert_source("<PythonString>", m_src_pystring);
        }

        virtual ~SubmitBlob() { }

        void keys( std::string & buffer );
        bool from_lines( const char * lines, std::string & errorMessage );

        // In version 1, we converted leading `+`s to leading `MY.`s on lookup.
        const char * lookup( const char * key );
        void set_submit_param( const char * key, const char * value );
        bool setDisableFileChecks( bool value );
        void setScheddVersion( const char * version );
        int init_base_ad(time_t submitTime, const char * userName );
        ClassAd * make_job_ad( JOB_ID_KEY j,
            int itemIndex, int step,
            bool interactive, bool remote,
            int (* check_file)( void * pv, SubmitHash * sub, _submit_file_role role, const char * name, int flags ),
            void * pv_check_arg
        );
        CondorError * error_stack() const;
        const char * expand( const char * key );


        // Given a `QUEUE [count] [in|from|matching ...]` statement, `count`
        // is an optional integer expression.  This function returns 1
        // if it's missing, a value less than 0 if the parse failed, and
        // the value otherwise.
        int queueStatementCount() const;

        // Given a cluster ID, parses the queue statement and initializes
        // the itemdata variables.  Returns the corresponding SubmitForeachArgs
        // pointer or NULL on a failure.
        SubmitForeachArgs * init_vars( int clusterID );
        void set_vars( StringList & vars, char * item, int itemIndex );
        void cleanup_vars( StringList & vars );

        const std::string & get_queue_args() const;
        bool set_queue_args( const char * queue_args );

    private:
        SubmitHash m_hash;
        MACRO_SOURCE m_src_pystring;
        MacroStreamMemoryFile m_ms_inline;

        // We could easily keep these in Python, if that simplifies things.
        std::string m_qargs;
        std::string m_remainder;

        // See comment in init_vars().
        char ClusterString[20];
        char ProcessString[20];
        char EmptyItemString[1];
};


MACRO_SOURCE SubmitBlob::EmptyMacroSrc = { false, false, 3, -2, -1, -2 };


const char *
SubmitBlob::lookup(const char * key) {
    // return m_hash.lookup( key );
    return m_hash.lookup_no_default( key );
}


void
SubmitBlob::set_submit_param( const char * key, const char * value ) {
    m_hash.set_submit_param( key, value );
}


bool
SubmitBlob::setDisableFileChecks( bool value ) {
    return m_hash.setDisableFileChecks( value );
}


void
SubmitBlob::setScheddVersion( const char * version ) {
    m_hash.setScheddVersion( version );
}


int
SubmitBlob::init_base_ad( time_t submitTime, const char * userName ) {
    return m_hash.init_base_ad( submitTime, userName );
}


const char *
SubmitBlob::expand( const char * key ) {
    return m_hash.submit_param(key);
}


const std::string &
SubmitBlob::get_queue_args() const {
    return m_qargs;
}


bool
SubmitBlob::set_queue_args( const char * qArgs ) {
    std::string miniSubmitFile = "\n queue " + std::string(qArgs);

    std::string errorMessage;
    return from_lines( miniSubmitFile.c_str(), errorMessage );
}


ClassAd *
SubmitBlob::make_job_ad( JOB_ID_KEY jid,
            int itemIndex, int step,
            bool interactive, bool remote,
            int (* check_file)( void * pv, SubmitHash * sub, _submit_file_role role, const char * name, int flags ),
            void * pv_check_arg
) {
    return m_hash.make_job_ad( jid,
        itemIndex, step, interactive, remote, check_file, pv_check_arg
    );
}


int
SubmitBlob::queueStatementCount() const {
    char * expanded_queue_args = m_hash.expand_macro( m_qargs.c_str() );

    SubmitForeachArgs sfa;
    int rval = sfa.parse_queue_args(expanded_queue_args);
    if( rval < 0 ) {
        free(expanded_queue_args);
        return rval;
    }

    free(expanded_queue_args);
    return sfa.queue_num;
}


SubmitForeachArgs *
SubmitBlob::init_vars( int /* clusterID */ ) {
    char * expanded_queue_args = m_hash.expand_macro( m_qargs.c_str() );

    SubmitForeachArgs * sfa = new SubmitForeachArgs();
    int rval = sfa->parse_queue_args(expanded_queue_args);
    free( expanded_queue_args );
    expanded_queue_args = NULL;
    if( rval < 0 ) {
        delete sfa;
        return NULL;
    }

    // Actually finish parsing the queue statement.
    std::string errorMessage;
    rval = m_hash.load_inline_q_foreach_items( m_ms_inline, *sfa, errorMessage );
    if( rval == 1 ) {
        rval = m_hash.load_external_q_foreach_items( *sfa, false, errorMessage );
    }
    if( rval < 0 ) {
        delete sfa;
        return NULL;
    }


    char * var = NULL;
    sfa->vars.rewind();
    while( (var = sfa->vars.next()) ) {
        // Note that this implies that updates to variables created by the
        // queue statement MUST be pointer replacements.
        m_hash.set_live_submit_variable( var, EmptyItemString, false );
    }


    m_hash.optimize();
    return sfa;
}


void
SubmitBlob::set_vars( StringList & vars, char * item, int /* itemIndex */ ) {
    if( vars.isEmpty() ) { return; }

    if( item == NULL ) {
        item = EmptyItemString;
    }

    // This is awful, but it's what condor_submit does.
    vars.rewind();
    char * var = vars.next();
    char * data = item;
    m_hash.set_live_submit_variable( var, data, false );

    // This is for the human-readable form in the submit file.
    const char * separators = ", \t";
    const char * whitespace = " \t";

    while( (var = vars.next()) ) {
        while (*data && ! strchr(separators, *data)) ++data;
        if( data != NULL ) {
            *data++ = 0;
            while (*data && strchr(whitespace, *data)) ++data;
            m_hash.set_live_submit_variable(var, data, false);
        }
    }
}


void
SubmitBlob::cleanup_vars( StringList & vars ) {
    if( vars.isEmpty() ) { return; }

    vars.rewind();
    char * var = NULL;
    while( (var = vars.next()) ) {
        m_hash.set_live_submit_variable( var, NULL, false );
    }
}


bool
SubmitBlob::from_lines( const char * lines, std::string & errorMessage ) {
    MacroStreamMemoryFile msmf(lines, strlen(lines), m_src_pystring);

    char * qLine = NULL;
    int rv = m_hash.parse_up_to_q_line(msmf, errorMessage, &qLine);
    if( rv != 0 ) {
        return false;
    }

    if(! qLine) { return true; }
    const char * qArgs = SubmitHash::is_queue_statement(qLine);
    if(! qArgs) { return true; }

    m_qargs = qArgs;
    size_t remainingBytes;
    const char * remainder = msmf.remainder(remainingBytes);
    if( remainder != NULL && remainingBytes > 0 ) {
        m_remainder.assign(remainder, remainingBytes);
        m_ms_inline.set(m_remainder.c_str(), remainingBytes, 0, m_src_pystring);
    }

    return true;
}


CondorError *
SubmitBlob::error_stack() const {
    return m_hash.error_stack();
}


void
SubmitBlob::keys( std::string & buffer ) {
    size_t bufferSize = 0;
    std::vector<std::string> keys;
    HASHITER iter = hash_iter_begin( m_hash.macros(), HASHITER_NO_DEFAULTS );
    while(! hash_iter_done(iter)) {
        const char * key = hash_iter_key(iter);
        const char * value = lookup(key);

        // We don't normally allow values to be NULL, so if it is, that
        // means this key was inserted to handle item data, and should
        // have been removed (but the submit hash doesn't allow that).
        if( value == NULL ) { continue; }

        keys.push_back( key );
        bufferSize += 1 + strlen( key );
        hash_iter_next(iter);
    }
    hash_iter_delete(& iter);

    size_t pos = 0;
    buffer.clear();
    buffer.resize(bufferSize);
    for( const auto & key : keys ) {
        buffer.replace(pos, key.size(), key);
        buffer[pos + key.size()] = '\0';
        pos += key.size() + 1;
    }
}


static PyObject *
_submit_init( PyObject *, PyObject * args ) {
    PyObject * self = NULL;
    PyObject_Handle * handle = NULL;
    const char * lines = NULL;

    if(! PyArg_ParseTuple( args, "OOz", & self, (PyObject **)& handle, & lines )) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }


    auto sb = new SubmitBlob();
    handle->t = (void *)sb;
    handle->f = [](void *& v) { dprintf( D_ALWAYS, "[SubmitHash]\n" ); delete (SubmitBlob *)v; v = NULL; };

    if( lines == NULL ) {
        Py_RETURN_NONE;
    }

    std::string errorMessage;
    if(! sb->from_lines(lines, errorMessage)) {
        delete sb;
        handle->t = NULL;

        // This was HTCondorValueError in version 1.
        PyErr_SetString(PyExc_ValueError, errorMessage.c_str());
        return NULL;
    }

    Py_RETURN_NONE;
}


static PyObject *
_submit__getitem__( PyObject *, PyObject * args ) {
    PyObject * self = NULL;
    PyObject_Handle * handle = NULL;
    const char * key = NULL;

    if(! PyArg_ParseTuple( args, "OOz", & self, (PyObject **)& handle, & key )) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }

    SubmitBlob * sb = (SubmitBlob *)handle->t;
    const char * value = sb->lookup(key);
    if( value == NULL ) {
        PyErr_SetString(PyExc_KeyError, key);
        return NULL;
    }

    return PyUnicode_FromString(value);
}


static PyObject *
_submit__setitem__( PyObject *, PyObject * args ) {
    PyObject * self = NULL;
    PyObject_Handle * handle = NULL;
    const char * key = NULL;
    const char * value = NULL;

    if(! PyArg_ParseTuple( args, "OOzz", & self, (PyObject **)& handle, & key, & value )) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }

    SubmitBlob * sb = (SubmitBlob *)handle->t;

    sb->set_submit_param(key, value);

    Py_RETURN_NONE;
}


static PyObject *
_submit_keys( PyObject *, PyObject * args ) {
    PyObject * self = NULL;
    PyObject_Handle * handle = NULL;

    if(! PyArg_ParseTuple( args, "OO", & self, (PyObject **)& handle )) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }

    std::string buffer;
    SubmitBlob * sb = (SubmitBlob *)handle->t;
    sb->keys(buffer);

    return PyUnicode_FromStringAndSize( buffer.c_str(), buffer.size() - 1 );
}


static PyObject *
_submit_expand( PyObject *, PyObject * args ) {
    PyObject * self = NULL;
    PyObject_Handle * handle = NULL;
    const char * key = NULL;

    if(! PyArg_ParseTuple( args, "OOz", & self, (PyObject **)& handle, & key )) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }

    SubmitBlob * sb = (SubmitBlob *)handle->t;
    const char * value = sb->expand(key);
    if( value == NULL ) {
        PyErr_SetString(PyExc_KeyError, key);
        return NULL;
    }

    return PyUnicode_FromString(value);
}


static PyObject *
_submit_setqargs( PyObject *, PyObject * args ) {
    PyObject * self = NULL;
    PyObject_Handle * handle = NULL;
    const char * queue_args = NULL;

    if(! PyArg_ParseTuple( args, "OOz", & self, (PyObject **)& handle, & queue_args )) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }

    SubmitBlob * sb = (SubmitBlob *)handle->t;
    if(! sb->set_queue_args( queue_args )) {
        PyErr_SetString(PyExc_ValueError, "invalid queue statement");
        return NULL;
    }


    Py_RETURN_NONE;
}


static PyObject *
_submit_getqargs( PyObject *, PyObject * args ) {
    PyObject * self = NULL;
    PyObject_Handle * handle = NULL;

    if(! PyArg_ParseTuple( args, "OO", & self, (PyObject **)& handle )) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }

    SubmitBlob * sb = (SubmitBlob *)handle->t;
    const std::string & buffer = sb->get_queue_args();

    return PyUnicode_FromString( buffer.c_str() );
}
