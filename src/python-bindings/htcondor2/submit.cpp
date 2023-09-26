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

        bool from_lines( const char * lines, std::string & errorMessage );

        // In version 1, we converted leading `+`s to leading `MY.`s on lookup.
        const char * lookup(const char * key) { return m_hash.lookup(key); }
        void set_submit_param( const char * key, const char * value ) { m_hash.set_submit_param(key, value); }
        void keys( std::string & buffer );

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


