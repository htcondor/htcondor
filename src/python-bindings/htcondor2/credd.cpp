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
            // Is param() actually returning an allocated string here?
            dname = param("UID_DOMAIN");
            if( dname != NULL ) {
                formatstr_cat( local, "@%s", dname );
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
    // _credd_do_store_cred(addr, user, password, mode)

    const char * addr = NULL;
    const char * user = NULL;
    const char * password = NULL;
    long mode = 0;
    if(! PyArg_ParseTuple( args, "szzl", & addr, & user, & password, & mode )) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }

    Daemon d(DT_CREDD, addr);
    std::string cooked_user;
    if(! cook_user(user, mode, cooked_user)) {
        // This was HTCondorValueError in version 1.
        PyErr_SetString( PyExc_ValueError, "invalid user argument" );
        return NULL;
    }
    // int result = do_store_cred(cooked_user.c_str(), password, mode, & d);
    ClassAd returnAd;
    int result = do_store_cred( cooked_user.c_str(), mode,
        (const unsigned char *)password, strlen(password),
        returnAd,
        NULL, & d);

    const char * errString = NULL;
    if( _store_cred_failed(result, mode, & errString) ) {
        // This was HTCondorIOError in version 1.
        PyErr_SetString( PyExc_IOError, errString );
        return NULL;
    }

    Py_RETURN_NONE;
}
