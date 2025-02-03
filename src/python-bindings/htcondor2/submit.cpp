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

        // Makes a copy of the map.
        void setTransferMap(MapFile * map);
        void unsetTransferMap();

        // set extended submit commands from the schedd
        void addExtendedCommands(const ClassAd & cmds) { m_hash.addExtendedCommands(cmds); }
        void clearExtendedCommands(void) { m_hash.clearExtendedCommands(); }

        bool isFactory(long long & maxMaterialize) { return m_hash.want_factory_submit(maxMaterialize); };

        // Given a `QUEUE [count] [in|from|matching ...]` statement, `count`
        // is an optional integer expression.  This function returns 1
        // if it's missing, a value less than 0 if the parse failed, and
        // the value otherwise.
        int queueStatementCount() const;

        SubmitStepFromQArgs make_qargs_iterator(std::string & errmsg) {
            SubmitStepFromQArgs ssi(m_hash);
            int rval = ssi.init(m_qargs.c_str(), errmsg);
            if (0 == rval) {
                ssi.load_items(m_ms_inline, false, errmsg);
                // rewind the inline itemdata source if we used it
                m_ms_inline.rewind_to( 0, 0 );
            }
            return ssi;
        }
        void cleanup_vars( const std::vector<std::string> & vars );

        // cleanup all state related to submitting a job.
        // this invalidates ads returned by make_job_ad() including the ClusterAD
        void cleanup_submit() {
            m_hash.clearExtendedCommands();
            m_hash.detachTransferMap();
            m_hash.reset(); // delete cached job and cluster ad from submit
        }

        const std::string & get_queue_args() const;
        bool set_queue_args( const char * queue_args );

        void make_digest( std::string & buffer, int clusterID, const std::vector<std::string> & vars, int options ) {
            (void) m_hash.make_digest(buffer, clusterID, vars, options);
        }
        bool submit_param_long_exists( const char * name, const char * alt_name, long long & value, bool int_range=false ) const {
            return m_hash.submit_param_long_exists( name, alt_name, value, int_range );
        }

        void setSubmitMethod(int method_value) { m_hash.setSubmitMethod(method_value); }
        int  getSubmitMethod() { return m_hash.getSubmitMethod(); }

        void insert_macro( const char * name, const std::string & value );

        int process_job_credentials( std::string & URL, std::string & error_string ) {
            return ::process_job_credentials( m_hash, 0, URL, error_string );
        }

        const ClassAd * getJOBSET() { return m_hash.getJOBSET(); }

    private:
        SubmitHash m_hash;
        MACRO_SOURCE m_src_pystring;
        MacroStreamMemoryFile m_ms_inline; // this will point to m_remainder when there is inline itemdata
        MapFile m_protected_url_map;

        // We could easily keep these in Python, if that simplifies things.
        std::string m_qargs;
        std::string m_remainder;

        // See comment in init_vars().
        char EmptyItemString[1];
};


MACRO_SOURCE SubmitBlob::EmptyMacroSrc = { false, false, 3, -2, -1, -2 };


void
SubmitBlob::insert_macro( const char * name, const std::string & value ) {
    MACRO_EVAL_CONTEXT ctx;
    ctx.init("SUBMIT");
    ::insert_macro( name, value.c_str(), m_hash.macros(), DetectedMacro, ctx );
}


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
    std::string miniSubmitFile = "\n queue " + std::string(qArgs) + "\n";

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


void
SubmitBlob::cleanup_vars( const std::vector<std::string> & vars ) {
    for (const auto& var: vars) {
        m_hash.set_live_submit_variable( var.c_str(), NULL, false );
    }
}

bool
SubmitBlob::from_lines( const char * lines, std::string & errorMessage ) {
    MacroStreamMemoryFile msmf(lines, strlen(lines), m_src_pystring);

    char * qLine = NULL;
    int rv = m_hash.parse_up_to_q_line(msmf, errorMessage, &qLine);
    if( rv != 0 ) {
        formatstr(errorMessage, "parse_up_to_q_line() failed");
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


void
SubmitBlob::setTransferMap( MapFile * map ) {
    if( map != NULL ) {
        m_protected_url_map = * map;
        m_hash.attachTransferMap(& m_protected_url_map);
    }
}


void
SubmitBlob::unsetTransferMap() {
    m_hash.detachTransferMap();
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
    handle->f = [](void *& v) { dprintf( D_PERF_TRACE, "[SubmitHash]\n" ); delete (SubmitBlob *)v; v = NULL; };

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
    // This can't happen with a fully-constructed object, but PyTest.
    if( sb == NULL ) { Py_RETURN_NONE; }
    sb->keys(buffer);

    if( buffer.size() == 0 ) {
        Py_RETURN_NONE;
    } else {
        return PyUnicode_FromStringAndSize( buffer.c_str(), buffer.size() - 1 );
    }
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
    // This can't happen with a fully-constructed object, but PyTest.
    if( sb == NULL ) { Py_RETURN_NONE; }
    const std::string & buffer = sb->get_queue_args();

    return PyUnicode_FromString( buffer.c_str() );
}


bool
set_dag_options( PyObject * options, DagmanOptions& dag_opts) {
    PyObject * key = NULL;
    PyObject * value = NULL;
    Py_ssize_t cursor = 0;

    // `key` and `value` are borrowed references
    while( PyDict_Next(options, &cursor, &key, &value) ) {
        if(! PyUnicode_Check(key)) {
            PyErr_SetString(PyExc_TypeError, "options keys must be strings");
            return false;
        }

        std::string k;
        if( py_str_to_std_string(key, k) == -1 ) {
            // py_str_to_std_string() has already set an exception for us.
            return false;
        }

        std::string v;
        if( py_str_to_std_string(value, v) == -1 ) {
            // py_str_to_std_string() has already set an exception for us.
            return false;
        }


        v = dag_opts.processOptionArg( k, v );

        SetDagOpt ret = dag_opts.set( k.c_str(), v );

        std::string msg, type;
        switch (ret) {
            case SetDagOpt::SUCCESS:
                break;
            case SetDagOpt::KEY_DNE:
                formatstr( msg, "%s is not a recognized DAGMan option", k.c_str() );
                PyErr_SetString(PyExc_KeyError, msg.c_str());
                return false;
            case SetDagOpt::INVALID_VALUE:
                type = dag_opts.OptValueType( k );
                formatstr( msg, "option %s value needs to be a %s", k.c_str(), type.c_str() );
                PyErr_SetString(PyExc_TypeError, msg.c_str());
                return false;
            case SetDagOpt::NO_KEY:
                PyErr_SetString(PyExc_HTCondorException, "Developer Error: empty key provided to DAGMan options set()");
                return false;
            case SetDagOpt::NO_VALUE:
                formatstr( msg, "empty value provided for DAGMan option %s", k.c_str() );
                PyErr_SetString(PyExc_ValueError, msg.c_str());
                return false;
        }
    }

    return true;
}


namespace shallow = DagmanShallowOptions;

static PyObject *
_submit_from_dag( PyObject *, PyObject * args ) {
    PyObject * options = NULL;
    const char * filename = NULL;

    if(! PyArg_ParseTuple( args, "zO", & filename, & options )) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }


    DagmanOptions dag_opts;
    std::string dag_file(filename);
    dag_opts.addDAGFile(dag_file);

    if( ! set_dag_options( options, dag_opts )) {
        // set_dag_options() has already set an exception for us.
        return NULL;
    }


    DagmanUtils du;
    // Why not a vector?
    std::list<std::string> lines;
    std::string errMsg;
    if(! du.setUpOptions( dag_opts, lines, &errMsg )) {
        PyErr_SetString(PyExc_HTCondorException, errMsg.c_str());
        return NULL;
    }

    // This is almost certainly an indication of a broken design.
    du.usingPythonBindings = true;
    if(! du.ensureOutputFilesExist( dag_opts )) {
        // This was HTCondorIOError in version 1.
        PyErr_SetString(PyExc_HTCondorException, "Unable to write condor_dagman output files");
        return NULL;
    }

    if(! du.writeSubmitFile( dag_opts, lines )) {
        // This was HTCondorIOError in version 1.
        PyErr_SetString(PyExc_HTCondorException, "Unable to write condor_dagman submit file");
        return NULL;
    }

    return PyUnicode_FromString( dag_opts[shallow::str::SubFile].c_str() );
}


static PyObject *
_display_dag_options( PyObject *, PyObject * /*args*/ ) {
    DagmanUtils du;
    du.DisplayDAGManOptions("%35s   | %s\n", DagOptionSrc::PYTHON_BINDINGS, " : ");

    Py_RETURN_NONE;
}


static PyObject *
_submit_set_submit_method( PyObject *, PyObject * args ) {
    // _submit_set_submit_method( self._handle, method_value )
    PyObject_Handle * handle = NULL;
    long method_value = -1;

    if(! PyArg_ParseTuple( args, "Ol", (PyObject **)& handle, & method_value )) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }

    SubmitBlob * sb = (SubmitBlob *)handle->t;
    sb->setSubmitMethod( method_value );

    Py_RETURN_NONE;
}


static PyObject *
_submit_get_submit_method( PyObject *, PyObject * args ) {
    // _submit_get_submit_method( self._handle )
    PyObject_Handle * handle = NULL;

    if(! PyArg_ParseTuple( args, "O", (PyObject **)& handle )) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }

    SubmitBlob * sb = (SubmitBlob *)handle->t;
    long method_value = sb->getSubmitMethod();

    return PyLong_FromLong(method_value);
}


static PyObject *
_submit_itemdata( PyObject *, PyObject * args ) {
    PyObject * self = NULL;
    PyObject_Handle * handle = NULL;

    if(! PyArg_ParseTuple( args, "OO", & self, (PyObject **)& handle )) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }

    SubmitBlob * sb = (SubmitBlob *)handle->t;
    std::string qargs_errmsg;
    SubmitStepFromQArgs ssqa = sb->make_qargs_iterator(qargs_errmsg);

    if( ! qargs_errmsg.empty()) {
        PyErr_SetString( PyExc_ValueError, qargs_errmsg.c_str() );
        return NULL;
    }

    // load foreach args into a variable so
    // I don't have to refactor the old code
    SubmitForeachArgs * itemdata = &ssqa.m_fea;

    PyObject * py_values = Py_None;
    if( itemdata->items.size() != 0 ) {
        std::string values;
        // Canonicalize the separators for unpacking.
        for( const auto & item : itemdata->items ) {
            std::vector< std::string_view > items;
            itemdata->split_item( item, items, itemdata->vars.size() );
            formatstr_cat( values, "%s\n", join(items, "\0x1F").c_str() );
        }
        values.pop_back();
        py_values = PyUnicode_FromString(values.c_str());
    }

    PyObject * py_keys = Py_None;
    if( itemdata->vars.size() != 0 ) {
        std::string keys = join(itemdata->vars, "\n");
        py_keys = PyUnicode_FromString(keys.c_str());
    }

    return Py_BuildValue( "(OO)", py_keys, py_values );
}


static PyObject *
_submit_issue_credentials( PyObject *, PyObject * args ) {
    // _submit_issue_credentials(self.handle_t)

    PyObject_Handle * handle = NULL;

    if(! PyArg_ParseTuple( args, "O", (PyObject **)& handle )) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }

    SubmitBlob * sb = (SubmitBlob *)handle->t;

    std::string URL;
    std::string error_string;
    int rv = sb->process_job_credentials( URL, error_string );

    if(rv != 0) {
        PyErr_SetString( PyExc_HTCondorException, error_string.c_str() );
        return NULL;
    }

    if(! URL.empty()) {
        return PyUnicode_FromString(URL.c_str());
    }

    Py_RETURN_NONE;
}
