bool
_store_cred_failed(int result, int mode, const char ** errString) {
    if( store_cred_failed(result, mode, errString) ) {
        if( result == FAILURE ) { * errString = "Communication error"; }
        return true;
    }
    return false;
}

bool
cook_user(const char * user, int mode, std::string & cooked_user) {
    std::string local;

    if( user == NULL || user[0] == '\0' ) {
        if(! (mode & STORE_CRED_LEGACY) ) {
            return "";
        }

        char * uname = my_username();
        local = uname;
        free(uname);

        char * dname = my_domainname();
        if(dname != NULL) {
            formatstr_cat( local, "@%s", dname );
            free(dname);
        } else {
            dname = param("UID_DOMAIN");
            if( dname != NULL ) {
                formatstr_cat( local, "@%s", dname );
                free( dname );
            } else {
                local += "@";
            }
        }
    } else {
        local = user;
    }

    // Should almost certainly be 3, but was 2 in version 1.
    if( local.size() < 2 ) { return false; }

    cooked_user = local;
    return true;
}


static PyObject *
_credd_do_store_cred(PyObject *, PyObject * args) {
    // _credd_do_store_cred(addr, user, credential, mode, service, handle)

    const char * addr = NULL;
    const char * user = NULL;
    const char * credential = NULL;
    Py_ssize_t credential_len = 0;
    const char * service = NULL;
    const char * handle = NULL;
    long mode = 0;
    if(! PyArg_ParseTuple( args, "zzz#lzz", & addr, & user, & credential, & credential_len, & mode, & service, & handle )) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }

    std::string cooked_user;
    if(! cook_user(user, mode, cooked_user)) {
        // This was HTCondorValueError in version 1.
        PyErr_SetString( PyExc_ValueError, "invalid user argument" );
        return NULL;
    }
    // This version of do_store_cred() has been deprecated, but was used
    // in version 1 by the [add|delete|query]_password() functions.  If
    // the new version, below, doesn't work, that's a bug in C++ code.
    // However, the dprintf() logs will still differ from version 1.
    //
    // int result = do_store_cred(cooked_user.c_str(), credential, mode, & d);

    ClassAd * serviceAd = NULL;
    if( service != NULL ) {
        serviceAd = new ClassAd();

        serviceAd->Assign("service", service);
        if( handle != NULL ) {
            serviceAd->Assign("handle", handle);
        }
    } else if( handle != NULL ) {
        // This was HTCondorValueError in version 1.
        PyErr_SetString( PyExc_ValueError, "invalid service argument" );
        return NULL;
    }

    Daemon * d = NULL;
    if( addr != NULL ) {
        d = new Daemon(DT_CREDD, addr);
    }

    ClassAd returnAd;
    int result = do_store_cred( cooked_user.c_str(), mode,
        (const unsigned char *)credential, credential_len,
        returnAd, serviceAd, d);
    if( d != NULL ) { delete d; }
    if( serviceAd != NULL ) { delete serviceAd; }

    const int MODE_MASK = 3;
    const int TYPE_MASK = 0x2c;
    bool  query_mode = (mode & MODE_MASK) == GENERIC_QUERY;
    bool delete_mode = (mode & MODE_MASK) == GENERIC_DELETE;
    bool  oauth_mode = (mode & TYPE_MASK) == STORE_CRED_USER_OAUTH;

    if( result == FAILURE_NOT_FOUND && (query_mode || delete_mode) ) {
        if( query_mode && oauth_mode ) {
            // In query mode, it might be very useful to know who
            // you failed to find any credential as.
            //
            // The only reason to restrict this to OAuth2 is that
            // return value for Kerberos was previously defined as
            // a timestamp, rather than a ClassAd.  Since changing
            // this return isn't a wire-protocol change, maybe we
            // can just do it, since the only client we ship will
            // change with this code?
            std::string str;
            sPrintAd(str, returnAd);
            return PyUnicode_FromString(str.c_str());
        }
        Py_RETURN_NONE;
    }

    const char * errString = NULL;
    if( _store_cred_failed(result, mode, & errString) ) {
        // This was HTCondorIOError in version 1.
        PyErr_SetString( PyExc_HTCondorException, errString );
        return NULL;
    }

    if( query_mode && oauth_mode ) {
        std::string str;
        sPrintAd(str, returnAd);
        return PyUnicode_FromString(str.c_str());
    }

    return PyLong_FromLong(result);
}


static PyObject *
_credd_do_check_oauth_creds(PyObject *, PyObject * args) {
    // _credd_do_check_oauth_creds(addr, user, mode, serviceAds)

    const char * addr = NULL;
    const char * user = NULL;
    long mode = 0;
    PyObject * pyServiceAds = NULL;

    if(! PyArg_ParseTuple( args, "zzlO", & addr, & user, & mode, & pyServiceAds )) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }

    std::string cooked_user;
    if(! cook_user(user, mode, cooked_user)) {
        // This was HTCondorValueError in version 1.
        PyErr_SetString( PyExc_ValueError, "invalid user argument" );
        return NULL;
    }

    Daemon * d = NULL;
    if( addr != NULL ) {
        d = new Daemon(DT_CREDD, addr);
    }

    std::vector<const ClassAd *> requests;
    Py_ssize_t size = PyList_Size( pyServiceAds );
    for( int i = 0; i < size; ++i ) {
        // We verified the types of these on the Python side.
        PyObject * py_classad = PyList_GetItem( pyServiceAds, i );
        auto * handle = get_handle_from( py_classad );
        ClassAd * classad = (ClassAd *)handle->t;

        requests.push_back(classad);
    }

    std::string url;
    int rv = do_check_oauth_creds(&requests[0], (int)requests.size(), url, d);
    if( d != NULL ) { delete d; }

    return Py_BuildValue("iz", rv, url.c_str());
}


static PyObject *
_credd_run_credential_producer(PyObject *, PyObject * args) {
    // _credd_run_credential_producer(producer)

    const char * producer = NULL;
    if(! PyArg_ParseTuple( args, "z", & producer )) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }


    // This code is duplicated, badly, in condor_submit.V6/submit.cpp,
    // in process_job_credentials(), which inexplicably has a 64k limit
    // on the size of credentials.
    ArgList producerArgs;
    producerArgs.AppendArg( producer );

    #define NO_STDERR false
    #define KEEP_PRIVS false

    MyPopenTimer child;
    if( child.start_program( producerArgs, NO_STDERR, NULL, KEEP_PRIVS ) < 0 ) {
        // This was HTCondorIOError in version 1.
        PyErr_SetString( PyExc_HTCondorException, "could not run credential producer" );
        return NULL;
    }

    #define CAREFUL_RESEARCH 20
    int exit_status = 0;
    bool exited = child.wait_for_exit( CAREFUL_RESEARCH, & exit_status );
    child.close_program(1);

    if(! exited) {
        // This was HTCondorIOError in version 1.
        PyErr_SetString( PyExc_HTCondorException, "credential producer did not exit" );
        return NULL;
    }

    if( exit_status != 0 ) {
        // This was HTCondorIOError in version 1.
        PyErr_SetString( PyExc_HTCondorException, "credential producer non-zero exit code" );
        return NULL;
    }

    char * credential = child.output().Detach();
    if( credential == NULL || child.output_size() == 0 ) {
        // This was HTCondorIOError in version 1.
        PyErr_SetString( PyExc_HTCondorException, "credential producer did not produce a credential" );
        return NULL;
    }

    PyObject * rv = PyUnicode_FromString( credential );
    free( credential );

    return rv;
}
