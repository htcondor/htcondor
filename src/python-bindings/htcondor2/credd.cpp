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
    // However, the dprint() logs will still differ from version 1.
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
    bool query_mode = (mode & MODE_MASK) == GENERIC_QUERY;
    bool delete_mode = (mode & MODE_MASK) == GENERIC_DELETE;
    if( result == FAILURE_NOT_FOUND && (query_mode || delete_mode) ) {
        return PyLong_FromLong(result);
    }

    const char * errString = NULL;
    if( _store_cred_failed(result, mode, & errString) ) {
        // This was HTCondorIOError in version 1.
        PyErr_SetString( PyExc_IOError, errString );
        return NULL;
    }

    const int TYPE_MASK = 0x2c;
    if( (mode & MODE_MASK) == GENERIC_QUERY &&
      (mode & TYPE_MASK) == STORE_CRED_USER_OAUTH ) {
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

        ClassAd * copy = new ClassAd();
        copy->CopyFrom(* classad);

        requests.push_back(copy);
    }

    std::string url;
    int rv = do_check_oauth_creds(&requests[0], (int)requests.size(), url, d);
    if( d != NULL ) { delete d; }
    for( int i = 0; i < size; ++i ) { delete requests[i]; }

    return Py_BuildValue("iz", rv, url.c_str());
}
