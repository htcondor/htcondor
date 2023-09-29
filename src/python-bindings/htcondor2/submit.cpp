struct SubmitBlob {
    static MACRO_SOURCE EmptyMacroSrc;

    public:
        SubmitBlob() :
            m_src_pystring(EmptyMacroSrc),
            m_ms_inline("", 0, EmptyMacroSrc),
            m_queue_may_append_to_cluster(false)
        {
            m_hash.init(JSM_PYTHON_BINDINGS);
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
        // Given a `QUEUE [count] [in|from|matching ...]` statement, `count`
        // is an optional integer expression.  This function returns 1
        // if it's missing, a value less than 0 if the parse failed, and
        // the value otherwise.
        int queueStatementCount() const;

        const char * expand( const char * key );

    private:
        SubmitHash m_hash;
        MACRO_SOURCE m_src_pystring;
        MacroStreamMemoryFile m_ms_inline;
        bool m_queue_may_append_to_cluster;

        // We could easily keep these in Python, if that simplifies things.
        std::string m_qargs;
        std::string m_remainder;
};


MACRO_SOURCE SubmitBlob::EmptyMacroSrc = { false, false, 3, -2, -1, -2 };


const char *
SubmitBlob::lookup(const char * key) {
    return m_hash.lookup( key );
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


bool
SubmitBlob::from_lines( const char * lines, std::string & errorMessage ) {
    m_hash.insert_source("<PythonString>", m_src_pystring);
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
        m_remainder.assign(remainder, remainingBytes );
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
    HASHITER iter = hash_iter_begin( m_hash.macros() );
    while(! hash_iter_done(iter)) {
        const char * key = hash_iter_key(iter);
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
