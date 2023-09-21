static bool
_schedd_query_callback( void * r, ClassAd * ad ) {
    auto * results = static_cast<std::vector<ClassAd *> *>(r);
    results->push_back( ad );

    // Don't delete the pointers.  The `results` local below
    // will do that when it goes out of scope, after we've
    // copied everything into Python.
    return false;
}


static PyObject *
_schedd_query(PyObject *, PyObject * args) {
    // _schedd_query(addr, constraint, projection, limit, opts)

    const char * addr = NULL;
    const char * constraint = NULL;
    PyObject * projection = NULL;
    long limit = 0;
    long opts = 0;
    if(! PyArg_ParseTuple( args, "zzOll", & addr, & constraint, & projection, & limit, & opts )) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }

    if(! PyList_Check(projection)) {
        PyErr_SetString(PyExc_TypeError, "projection must be a list");
        return NULL;
    }

    CondorQ q;
    // Required for backwards compatibility in version 1.
    q.requestServerTime(true);


    if( strlen(constraint) != 0 ) {
        q.addAND(constraint);
    }


    // FIXME: copied from _collector_query(), refactor.
    std::vector<std::string> attributes;
    Py_ssize_t size = PyList_Size(projection);
    for( int i = 0; i < size; ++i ) {
        PyObject * py_attr = PyList_GetItem(projection, i);
        if( py_attr == NULL ) {
            // PyList_GetItem() has already set an exception for us.
            return NULL;
        }

        if(! PyUnicode_Check(py_attr)) {
            PyErr_SetString(PyExc_TypeError, "projection must be a list of strings");
            return NULL;
        }

        std::string attribute;
        if( py_str_to_std_string(py_attr, attribute) != -1 ) {
            attributes.push_back(attribute);
        } else {
            // py_str_to_std_str() has already set an exception for us.
            return NULL;
        }
    }

    // Why _don't_ we have a std::vector<std::string> constructor for these?
    StringList slAttributes;
    for( const auto & attribute : attributes ) {
        slAttributes.append(attribute.c_str());
    }


    CondorError errStack;
    ClassAd * summaryAd = NULL;
    std::vector<ClassAd *> results;
    int rv = q.fetchQueueFromHostAndProcess(
        addr, slAttributes, opts, limit,
        _schedd_query_callback, & results,
        2 /* E_VERY_MAGICAL */, & errStack,
        opts == CondorQ::fetch_SummaryOnly ? & summaryAd : NULL
    );

    switch( rv ) {
        case Q_OK:
            break;

        case Q_PARSE_ERROR:
        case Q_INVALID_CATEGORY:
            // This was ClassAdParseError in version 1.
            PyErr_SetString(PyExc_RuntimeError, "Parse error in constraint");
            return NULL;

        case Q_UNSUPPORTED_OPTION_ERROR:
            // This was HTCondorIOError in version 1.
            PyErr_SetString(PyExc_IOError, "Query fetch option unsupported by this schedd.");
            return NULL;

        default:
            // This was HTCondorIOError in version 1.
            std::string error = "Failed to fetch ads from schedd, errmsg="
                              + errStack.getFullText();
            PyErr_SetString(PyExc_IOError, "");
            return NULL;
    }


    PyObject * list = PyList_New(0);
    if( list == NULL ) {
        PyErr_SetString( PyExc_MemoryError, "_schedd_query" );
        return NULL;
    }

    if( opts == CondorQ::fetch_SummaryOnly ) {
        ASSERT(summaryAd != NULL);
        ASSERT(results.size() == 0);
        results.push_back(summaryAd);
    }

    for( auto & classAd : results ) {
        // We could probably dispense with the copy by clearing the
        // `results` vector after this loop.
        PyObject * pyClassAd = py_new_htcondor2_classad(classAd->Copy());
        auto rv = PyList_Append(list, pyClassAd);
        Py_DecRef(pyClassAd);

        if( rv != 0 ) {
            // PyList_Append() has already set an exception for us.
            return NULL;
        }
    }

    return list;
}


static PyObject *
_schedd_act_on_job_ids(PyObject *, PyObject * args) {
    // _schedd_act_on_job_ids(addr, job_spec, action, reason_string, reason_code)

    const char * addr = NULL;
    const char * job_spec = NULL;
    long action = 0;
    const char * reason_string = NULL;
    const char * reason_code = NULL;

    if(! PyArg_ParseTuple( args, "zzlzz", & addr, & job_spec, & action, & reason_string, & reason_code )) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }

    StringList ids(job_spec);


    ClassAd * result = NULL;
    DCSchedd schedd(addr);
    switch( action ) {
        case JA_HOLD_JOBS:
            result = schedd.holdJobs(& ids, reason_string, reason_code, NULL, AR_TOTALS );
            break;

        case JA_RELEASE_JOBS:
            result = schedd.releaseJobs(& ids, reason_string, NULL, AR_TOTALS );
            break;

        case JA_REMOVE_JOBS:
            result = schedd.removeJobs(& ids, reason_string, NULL, AR_TOTALS );
            break;

        case JA_REMOVE_X_JOBS:
            result = schedd.removeXJobs(& ids, reason_string, NULL, AR_TOTALS );
            break;

        case JA_VACATE_JOBS:
        case JA_VACATE_FAST_JOBS:
            {
            auto vacate_type = action == JA_VACATE_JOBS ? VACATE_GRACEFUL : VACATE_FAST;
            result = schedd.vacateJobs(& ids, vacate_type, NULL, AR_TOTALS);
            } break;

        case JA_SUSPEND_JOBS:
            result = schedd.suspendJobs(& ids, reason_string, NULL, AR_TOTALS );
            break;

        case JA_CONTINUE_JOBS:
            result = schedd.continueJobs(& ids, reason_string, NULL, AR_TOTALS );
            break;

        default:
            // This was HTCondorEnumError, in version 1.
            PyErr_SetString(PyExc_RuntimeError, "Job action not implemented.");
            return NULL;
    }

    if( result == NULL ) {
        // This was HTCondorReplyError, in version 1.
        PyErr_SetString(PyExc_RuntimeError, "Error when performing action on the schedd.");
        return NULL;
    }


    return py_new_htcondor2_classad(result->Copy());
}


static PyObject *
_schedd_act_on_job_constraint(PyObject *, PyObject * args) {
    // _schedd_act_on_job_ids(addr, constraint, action, reason_string, reason_code)

    const char * addr = NULL;
    const char * constraint = NULL;
    long action = 0;
    const char * reason_string = NULL;
    const char * reason_code = NULL;

    if(! PyArg_ParseTuple( args, "zzlzz", & addr, & constraint, & action, & reason_string, & reason_code )) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }


    ClassAd * result = NULL;
    DCSchedd schedd(addr);
    switch( action ) {
        case JA_HOLD_JOBS:
            result = schedd.holdJobs(constraint, reason_string, reason_code, NULL, AR_TOTALS );
            break;

        case JA_RELEASE_JOBS:
            result = schedd.releaseJobs(constraint, reason_string, NULL, AR_TOTALS );
            break;

        case JA_REMOVE_JOBS:
            result = schedd.removeJobs(constraint, reason_string, NULL, AR_TOTALS );
            break;

        case JA_REMOVE_X_JOBS:
            result = schedd.removeXJobs(constraint, reason_string, NULL, AR_TOTALS );
            break;

        case JA_VACATE_JOBS:
        case JA_VACATE_FAST_JOBS:
            {
            auto vacate_type = action == JA_VACATE_JOBS ? VACATE_GRACEFUL : VACATE_FAST;
            result = schedd.vacateJobs(constraint, vacate_type, NULL, AR_TOTALS);
            } break;

        case JA_SUSPEND_JOBS:
            result = schedd.suspendJobs(constraint, reason_string, NULL, AR_TOTALS );
            break;

        case JA_CONTINUE_JOBS:
            result = schedd.continueJobs(constraint, reason_string, NULL, AR_TOTALS );
            break;

        default:
            // This was HTCondorEnumError, in version 1.
            PyErr_SetString(PyExc_RuntimeError, "Job action not implemented.");
            return NULL;
    }

    if( result == NULL ) {
        // This was HTCondorReplyError, in version 1.
        PyErr_SetString(PyExc_RuntimeError, "Error when performing action on the schedd.");
        return NULL;
    }


    return py_new_htcondor2_classad(result->Copy());
}
