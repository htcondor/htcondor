#include "queue_connection.cpp"

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


    std::vector<std::string> attributes;
    int rv = py_list_to_vector_of_strings(projection, attributes, "projection");
    if( rv == -1 ) {
        // py_list_to_vector_of_strings() has already set an exception for us.
        return NULL;
    }

    CondorError errStack;
    ClassAd * summaryAd = NULL;
    std::vector<ClassAd *> results;
    rv = q.fetchQueueFromHostAndProcess(
        addr, attributes, opts, limit,
        _schedd_query_callback, & results,
        2 /* use fetchQueueFromHostAndProcess2() */, & errStack,
        opts == QueryFetchOpts::fetch_SummaryOnly ? & summaryAd : NULL
    );

    switch( rv ) {
        case Q_OK:
            break;

        case Q_PARSE_ERROR:
        case Q_INVALID_CATEGORY:
            // This was ClassAdParseError in version 1.
            PyErr_SetString(PyExc_HTCondorException, "Parse error in constraint");
            return NULL;

        case Q_UNSUPPORTED_OPTION_ERROR:
            // This was HTCondorIOError in version 1.
            PyErr_SetString(PyExc_HTCondorException, "Query fetch option unsupported by this schedd.");
            return NULL;

        default:
            // This was HTCondorIOError in version 1.
            std::string error = "Failed to fetch ads from schedd, errmsg="
                              + errStack.getFullText();
            PyErr_SetString(PyExc_HTCondorException, error.c_str());
            return NULL;
    }


    PyObject * list = PyList_New(0);
    if( list == NULL ) {
        PyErr_SetString( PyExc_MemoryError, "_schedd_query" );
        return NULL;
    }

    if( opts == QueryFetchOpts::fetch_SummaryOnly ) {
        ASSERT(summaryAd != NULL);
        ASSERT(results.size() == 0);
        results.push_back(summaryAd);
    }

    for( auto & classAd : results ) {
        PyObject * pyClassAd = py_new_classad2_classad(classAd);
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
    // _schedd_act_on_job_ids(addr, job_list, action, reason_string, reason_code)

    const char * addr = NULL;
    const char * job_list = NULL;
    long action = 0;
    const char * reason_string = NULL;
    const char * reason_code = NULL;

    if(! PyArg_ParseTuple( args, "zzlzz", & addr, & job_list, & action, & reason_string, & reason_code )) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }

    std::vector<std::string> ids = split(job_list);

    ClassAd * result = NULL;
    DCSchedd schedd(addr);
    switch( action ) {
        case JA_HOLD_JOBS:
            result = schedd.holdJobs( ids, reason_string, reason_code, NULL, AR_TOTALS );
            break;

        case JA_RELEASE_JOBS:
            result = schedd.releaseJobs( ids, reason_string, NULL, AR_TOTALS );
            break;

        case JA_REMOVE_JOBS:
            result = schedd.removeJobs( ids, reason_string, NULL, AR_TOTALS );
            break;

        case JA_REMOVE_X_JOBS:
            result = schedd.removeXJobs( ids, reason_string, NULL, AR_TOTALS );
            break;

        case JA_VACATE_JOBS:
        case JA_VACATE_FAST_JOBS:
            {
            auto vacate_type = action == JA_VACATE_JOBS ? VACATE_GRACEFUL : VACATE_FAST;
            result = schedd.vacateJobs( ids, vacate_type, NULL, AR_TOTALS);
            } break;

        case JA_SUSPEND_JOBS:
            result = schedd.suspendJobs( ids, reason_string, NULL, AR_TOTALS );
            break;

        case JA_CONTINUE_JOBS:
            result = schedd.continueJobs( ids, reason_string, NULL, AR_TOTALS );
            break;

        default:
            // This was HTCondorEnumError, in version 1.
            PyErr_SetString(PyExc_HTCondorException, "Job action not implemented.");
            return NULL;
    }

    if( result == NULL ) {
        // This was HTCondorReplyError, in version 1.
        PyErr_SetString(PyExc_HTCondorException, "Error when performing action on the schedd.");
        return NULL;
    }


    return py_new_classad2_classad(result->Copy());
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

    if( constraint == NULL || constraint[0] == '\0' ) {
        constraint = "true";
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
            PyErr_SetString(PyExc_HTCondorException, "Job action not implemented.");
            return NULL;
    }

    if( result == NULL ) {
        // This was HTCondorReplyError, in version 1.
        PyErr_SetString(PyExc_HTCondorException, "Error when performing action on the schedd.");
        return NULL;
    }


    return py_new_classad2_classad(result->Copy());
}


static PyObject *
_schedd_edit_job_ids(PyObject *, PyObject * args) {
    // schedd_edit_job_ids(addr, job_list, attr, value, flags)

    const char * addr = NULL;
    const char * job_list = NULL;
    const char * attr = NULL;
    const char * value = NULL;
    long flags = 0;

    if(! PyArg_ParseTuple( args, "zzzzl", & addr, & job_list, & attr, & value, & flags )) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }

    flags = flags | SetAttribute_NoAck;


    QueueConnection qc;
    if(! qc.connect(addr)) {
        // This was HTCondorIOError, in version 1.
        PyErr_SetString(PyExc_HTCondorException, "Failed to connect to schedd.");
        return NULL;
    }

    long matchCount = 0;
	for (auto& id: StringTokenIterator(job_list)) {
        JOB_ID_KEY jobIDKey;
        if(! jobIDKey.set(id)) {
            qc.abort();

            // This was HTCondorValueError, in version 1.
            PyErr_SetString(PyExc_ValueError, "Invalid ID");
            return NULL;
        }

        int rv = SetAttribute(jobIDKey.cluster, jobIDKey.proc, attr, value, flags);
        if( rv == -1 ) {
            qc.abort();

            // This was HTCondorIOError, in version 1.
            PyErr_SetString(PyExc_HTCondorException, "Unable to edit job");
            return NULL;
        }

        matchCount += 1;
    }


    std::string message;
    if(! qc.commit(message)) {
        // This was HTCondorIOError, in version 1.
        PyErr_SetString(PyExc_HTCondorException, ("Unable to commit transaction:" + message).c_str());
        return NULL;
    }

    return PyLong_FromLong(matchCount);
}


static PyObject *
_schedd_edit_job_constraint(PyObject *, PyObject * args) {
    // schedd_edit_job_ids(addr, constraint, attr, value, flags)

    const char * addr = NULL;
    const char * constraint = NULL;
    const char * attr = NULL;
    const char * value = NULL;
    long flags = 0;

    if(! PyArg_ParseTuple( args, "zzzzl", & addr, & constraint, & attr, & value, & flags )) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }

    if( constraint == NULL || constraint[0] == '\0' ) {
        constraint = "true";
    }

    if(! param_boolean("CONDOR_Q_ONLY_MY_JOBS", true)) {
        flags = flags | SetAttribute_OnlyMyJobs;
    }
    flags = flags | SetAttribute_NoAck;


    QueueConnection qc;
    if(! qc.connect(addr)) {
        // This was HTCondorIOError, in version 1.
        PyErr_SetString(PyExc_HTCondorException, "Failed to connect to schedd.");
        return NULL;
    }

    int rval = SetAttributeByConstraint( constraint, attr, value, flags );
    if( rval == -1 ) {
        qc.abort();

        // This was HTCondorIOError, in version 1.
        PyErr_SetString(PyExc_HTCondorException, "Unable to edit jobs matching constraint");
        return NULL;
    }


    std::string message;
    if(! qc.commit(message)) {
        // This was HTCondorIOError, in version 1.
        PyErr_SetString(PyExc_HTCondorException, ("Unable to commit transaction: " + message).c_str());
        return NULL;
    }

    return PyLong_FromLong(rval);
}


static PyObject *
_schedd_reschedule(PyObject *, PyObject * args) {
    // schedd_reschedule(addr)

    const char * addr = NULL;

    if(! PyArg_ParseTuple( args, "z", & addr )) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }


    DCSchedd schedd(addr);
    Stream::stream_type st = schedd.hasUDPCommandPort() ? Stream::safe_sock : Stream::reli_sock;
    if(! schedd.sendCommand(RESCHEDULE, st, 0)) {
        dprintf( D_ALWAYS, "Can't send RESCHEDULE command to schedd.\n" );
    }

    Py_RETURN_NONE;
}


static PyObject *
_schedd_export_job_ids(PyObject *, PyObject * args) {
    // _schedd_export_job_ids(addr, job_list, export_dir, new_spool_dir)

    const char * addr = NULL;
    const char * job_list = NULL;
    const char * export_dir = NULL;
    const char * new_spool_dir = NULL;

    if(! PyArg_ParseTuple( args, "zzzz", & addr, & job_list, & export_dir, & new_spool_dir )) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }

    std::vector<std::string> ids = split(job_list);

    CondorError errorStack;
    DCSchedd schedd(addr);
    ClassAd * result = schedd.exportJobs(
        ids, export_dir, new_spool_dir, & errorStack
    );

    if( errorStack.code() > 0 ) {
        // This was HTCondorIOError in version 1.
        PyErr_SetString( PyExc_HTCondorException, errorStack.getFullText(true).c_str() );
        return NULL;
    }
    if( result == NULL ) {
        // This was HTCondorIOError in version 1.
        PyErr_SetString( PyExc_HTCondorException, "No result ad" );
        return NULL;
    }

    return py_new_classad2_classad(result->Copy());
}


static PyObject *
_schedd_export_job_constraint(PyObject *, PyObject * args) {
    // _schedd_export_job_constraint(addr, constraint, export_dir, new_spool_dir)

    const char * addr = NULL;
    const char * constraint = NULL;
    const char * export_dir = NULL;
    const char * new_spool_dir = NULL;

    if(! PyArg_ParseTuple( args, "zzzz", & addr, & constraint, & export_dir, & new_spool_dir )) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }

    if( constraint == NULL || constraint[0] == '\0' ) {
        constraint = "true";
    }


    CondorError errorStack;
    DCSchedd schedd(addr);
    ClassAd * result = schedd.exportJobs(
        constraint, export_dir, new_spool_dir, & errorStack
    );

    if( errorStack.code() > 0 ) {
        // This was HTCondorIOError in version 1.
        PyErr_SetString( PyExc_HTCondorException, errorStack.getFullText(true).c_str() );
        return NULL;
    }
    if( result == NULL ) {
        // This was HTCondorIOError in version 1.
        PyErr_SetString( PyExc_HTCondorException, "No result ad" );
        return NULL;
    }


    return py_new_classad2_classad(result->Copy());
}


static PyObject *
_schedd_import_exported_job_results(PyObject *, PyObject * args) {
    // schedd_reschedule(addr, import_dir)

    const char * addr = NULL;
    const char * import_dir = NULL;

    if(! PyArg_ParseTuple( args, "zz", & addr, & import_dir )) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }


    DCSchedd schedd(addr);
    CondorError errorStack;
    ClassAd * result = schedd.importExportedJobResults( import_dir, & errorStack );

    // Check for errorStack.code()?

    if( result == NULL ) {
        // This was HTCondorIOError in version 1.
        PyErr_SetString( PyExc_HTCondorException, "No result ad" );
        return NULL;
    }

    // Check for `Error` attribute in result?


    return py_new_classad2_classad(result->Copy());
}


static PyObject *
_schedd_unexport_job_ids(PyObject *, PyObject * args) {
    // _schedd_unexport_job_constraint(addr, job_list)

    const char * addr = NULL;
    const char * job_list = NULL;

    if(! PyArg_ParseTuple( args, "zz", & addr, & job_list )) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }

    std::vector<std::string> ids = split(job_list);

    DCSchedd schedd(addr);
    CondorError errorStack;
    ClassAd * result = schedd.unexportJobs( ids, & errorStack );

    if( errorStack.code() > 0 ) {
        // This was HTCondorIOError in version 1.
        PyErr_SetString( PyExc_HTCondorException, errorStack.getFullText(true).c_str() );
        return NULL;
    }
    if( result == NULL ) {
        // This was HTCondorIOError in version 1.
        PyErr_SetString( PyExc_HTCondorException, "No result ad" );
        return NULL;
    }


    return py_new_classad2_classad(result->Copy());
}


static PyObject *
_schedd_unexport_job_constraint(PyObject *, PyObject * args) {
    // _schedd_unexport_job_constraint(addr, constraint)

    const char * addr = NULL;
    const char * constraint = NULL;

    if(! PyArg_ParseTuple( args, "zz", & addr, & constraint )) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }

    if( constraint == NULL || constraint[0] == '\0' ) {
        constraint = "true";
    }


    DCSchedd schedd(addr);
    CondorError errorStack;
    ClassAd * result = schedd.unexportJobs( constraint, & errorStack );

    if( errorStack.code() > 0 ) {
        // This was HTCondorIOError in version 1.
        PyErr_SetString( PyExc_HTCondorException, errorStack.getFullText(true).c_str() );
        return NULL;
    }
    if( result == NULL ) {
        // This was HTCondorIOError in version 1.
        PyErr_SetString( PyExc_HTCondorException, "No result ad" );
        return NULL;
    }


    return py_new_classad2_classad(result->Copy());
}


static PyObject *
_schedd_submit( PyObject *, PyObject * args ) {
    // _schedd_submit(addr, submit.handle_t, count, spool)

    const char * addr = NULL;
    PyObject_Handle * handle = NULL;
    long count = 0;
    int spool = 0;

    if(! PyArg_ParseTuple( args, "zOlp", & addr, (PyObject **)& handle, & count, & spool )) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }

    SubmitBlob * sb = (SubmitBlob *)handle->t;
    CondorError errstack;
    std::string errmsg;

    SubmitStepFromQArgs ssqa = sb->make_qargs_iterator(errmsg);
    if ( ! errmsg.empty()) {
        errmsg.insert(0, "invalid Queue statement, errmsg=");
        PyErr_SetString( PyExc_ValueError, errmsg.c_str());
        return NULL;
    }

    if( count == 0 ) {
        count = ssqa.m_fea.queue_num;
    } else {
        ssqa.m_fea.queue_num = count;
    }
    if( count < 0 || ssqa.selected_job_count() <= 0) {
        // This was HTCondorValueError in version 1.
        PyErr_SetString( PyExc_ValueError, "invalid Queue statement" );
        return NULL;
    }

    AbstractScheddQ * myq = nullptr;
    // for regular submit use this AbstractScheddQ
    ActualScheddQ scheddQ;
    //for dry-run use this AbstractScheddQ
    //const int sim_starting_cluster = 1;
    //SimScheddQ simQ(sim_starting_cluster);

    DCSchedd schedd(addr);

    // SUBMIT_SKIP_FILECHECKS. this does nothing unless you pass check callback to make_job_ad
    // TODO: make a filecheck callback? or just set this to false?
    sb->setDisableFileChecks( param_boolean_crufty("SUBMIT_SKIP_FILECHECKS", true) ? 1 : 0 );

    // Set the remote schedd version. this may do a locate query to the collector
    if( schedd.version() != NULL ) {
        sb->setScheddVersion( schedd.version() );
    } else {
        sb->setScheddVersion( CondorVersion() );
    }

    // Initialize the new cluster ad.
    if( sb->init_base_ad( time(NULL), schedd.getOwner().c_str() ) != 0 ) {

        std::string error = "Failed to create a cluster ad, errmsg="
            + sb->error_stack()->getFullText();

        // This was HTCondorInternalError in version 1.
        PyErr_SetString( PyExc_HTCondorException, error.c_str() );
        return NULL;
    }


    // --- schedd communication starts here ---
    //

    //if (dry_run) {
    //	FILE * outfile = nullptr; // FILE* to dry-run to
    //	simQ.Connect(outfile, false, false);
    //	myq = &SimQ;
    //} else
    {
        if (scheddQ.Connect(schedd, errstack) == 0) {
            errmsg = "Failed to connect to schedd, errmsg=" + errstack.getFullText();
            PyErr_SetString(PyExc_HTCondorException, errmsg.c_str());
            return NULL;
        }
        myq = &scheddQ;
    }

    // add extended submit commands for this Schedd to the submitHash
    // add the transfer map
    {
        ClassAd extended_submit_commands;
        if (myq->has_extended_submit_commands(extended_submit_commands)) {
            sb->addExtendedCommands(extended_submit_commands);
        }

        // TODO: either kill this or fetch it from the schedd
        sb->setTransferMap(getProtectedURLMap());
    }

    
    long long maxMaterialize = 0;
    bool isFactoryJob = sb->isFactory(maxMaterialize);
    if ( ! isFactoryJob && param_boolean("SUBMIT_FACTORY_JOBS_BY_DEFAULT", false)) {
        int late_ver = 0;
        if (myq->allows_late_materialize() &&
            myq->has_late_materialize(late_ver) && late_ver >= 2 &&
            ssqa.selected_job_count() > 1) {
            isFactoryJob = true;
        }
    }
    if (isFactoryJob) {
        // late mat will need to know the CWD of submit
        std::string cwd;
        condor_getcwd(cwd);
        sb->insert_macro( "FACTORY.Iwd", cwd );
    }

    // Get a new cluster ID.
    int clusterID = myq->get_NewCluster(errstack);
    if( clusterID < 0 ) {

        // This was HTCondorInternalError in version 1.
        errmsg = "Failed to create new cluster." + errstack.getFullText();
        PyErr_SetString( PyExc_HTCondorException, errmsg.c_str() );

        myq->disconnect(false, errstack);
        sb->cleanup_submit();
        return NULL;
    }

    // This ends up being a pointer to a member of (a member of) `sb`.
    // it will be valid from the first call to make_job_ad until cleanup_submit
    ClassAd * clusterAd = NULL;
    // When spooling, we need a copy of each proc ad to pass to the spooling code (sigh)
    std::vector< ClassAd *> * spooledProcAds = NULL;
    if( spool ) {
        spooledProcAds = new std::vector< ClassAd *>();
    }

    bool send_cluster_ad = true;
    bool iter_selected = ! isFactoryJob;
    int numJobs=0, itemIndex=0, step=0;

    JOB_ID_KEY jid(clusterID,0);
    ssqa.begin(jid, ! isFactoryJob);

    // loop while we have procs to submit.
    // For late mat we break out of the loop after the cluster ad is sent
    while (ssqa.next_impl(iter_selected, jid, itemIndex, step, ! isFactoryJob) > 0) {

        if ( ! isFactoryJob) {

            int procID = myq->get_NewProc(clusterID);
            if (procID != jid.proc) {
                formatstr(errmsg, "expected next ProcId to be %d, but Schedd says %d", jid.proc, procID);
                PyErr_SetString( PyExc_HTCondorException, errmsg.c_str() );
                numJobs = -1;
                break;
            }

        } else { // isFactoryJob

            std::string submitDigest;
            sb->make_digest( submitDigest, clusterID, ssqa.vars(), 0 );
            if( submitDigest.empty() ) {
                PyErr_SetString( PyExc_HTCondorException, "Failed to make submit digest (late materialization)" );
                numJobs = -1;
                break;
            }

            if (myq->send_Itemdata(clusterID, ssqa.m_fea, errmsg) < 0) {
                errmsg.insert(0, "Failed to send item data (late materialization), ");
                PyErr_SetString( PyExc_HTCondorException, errmsg.c_str());
                numJobs = -1;
                break;
            }

            if (0 != append_queue_statement(submitDigest, ssqa.m_fea)) {
                PyErr_SetString( PyExc_HTCondorException, "Failed to append queue statement (late materialization)" );
                numJobs = -1;
                break;
            }

            // Compute max materialization.
            numJobs = ssqa.selected_job_count();
            if( maxMaterialize <= 0 ) { maxMaterialize = INT_MAX; }
            maxMaterialize = MIN(maxMaterialize, numJobs);
            maxMaterialize = MAX(maxMaterialize, 1);

            // send the submit digest
            if (myq->set_Factory(clusterID, (int)maxMaterialize, "", submitDigest.c_str()) < 0) {
                PyErr_SetString( PyExc_HTCondorException, "Failed to send submit digest (late materialization)" );
                numJobs = -1;
                break;
            }

            // now we can set the live vars from the ssqa.next() call from way up there ^^
            ssqa.set_live_vars();
        }


        // Generate the job ClassAd
        ClassAd * procAd = sb->make_job_ad(jid, itemIndex, 0, false, spool, NULL, NULL);
        if(! procAd) {
            std::string error = "Failed to create job ad"; 
            if (isFactoryJob) error += " (late materialization)";
            formatstr_cat( error, ", errmsg=%s", sb->error_stack()->getFullText(true).c_str() );
            // This was HTCondorInternalError in version 1.
            PyErr_SetString( PyExc_HTCondorException, error.c_str() );
            numJobs = -1;
            break;
        }

        if (send_cluster_ad) { // we need to send the cluster ad
            clusterAd = procAd->GetChainedParentAd();
            if(! clusterAd) {
                PyErr_SetString( PyExc_HTCondorException, "Failed to get parent ad (late materialization)" );
                numJobs = -1;
                break;
            }

            // If there is also a jobset ad, send it before the cluster ad.
            const ClassAd * jobsetAd = sb->getJOBSET();
            if (jobsetAd) {
                int jobset_version = 0;
                if (myq->has_send_jobset(jobset_version)) {
                    if (myq->send_Jobset(clusterID, jobsetAd) < 0) {
                        PyErr_SetString( PyExc_HTCondorException, "Failed to send Jobset Ad" );
                        numJobs = -1;
                        break;
                    }
                }
            }

            // send the cluster ad
            errmsg = myq->send_JobAttributes(JOB_ID_KEY(clusterID, -1), *clusterAd, SetAttribute_NoAck);
            if ( ! errmsg.empty()) {
                PyErr_SetString( PyExc_HTCondorException, "Failed to send Cluster Ad");
                numJobs = -1;
                break;
            }

            // send the cluster ad only once
            send_cluster_ad = false;
        }

        if (isFactoryJob) {
            // break out of the loop, we are done.
            break;
        }

        // send the proc ad
        errmsg = myq->send_JobAttributes(jid, *procAd, SetAttribute_NoAck);
        if ( ! errmsg.empty()) {
            PyErr_SetString( PyExc_HTCondorException, errmsg.c_str() );
            numJobs = -1;
            break;
        }

        if( spooledProcAds ) {
            spooledProcAds->push_back((ClassAd*)procAd->Copy());
        }
        numJobs += 1;

    } // end submit loop

    sb->cleanup_vars(ssqa.vars());

    // commit transaction and disconnect queue
    bool commit_transaction = numJobs > 0;
    if ( ! myq->disconnect(commit_transaction, errstack)) {
        if (commit_transaction) {
            errmsg = "Unable to commit transaction, " + errstack.getFullText();
            PyErr_SetString(PyExc_HTCondorException, errmsg.c_str());
        }
        if (spooledProcAds) {
            while ( ! spooledProcAds->empty()) { 
                delete spooledProcAds->back();
                spooledProcAds->pop_back();
            }
            delete spooledProcAds;
        }
        sb->cleanup_submit();
        return NULL;
    }

    // commit succeeded, set warnings
    //
    if(! errstack.empty()) {
        // The Python warning infrastructure will by default suppress
        // the second and subsequent warnings, but that's the standard.
        PyErr_WarnEx( PyExc_UserWarning,
            ("Submit succeeded with warning: " + errstack.getFullText(true)).c_str(), 2
        );
    }

    // TODO: make the schedd do this automatically, then remove this call
    //
    Stream::stream_type st = schedd.hasUDPCommandPort() ? Stream::safe_sock : Stream::reli_sock;
    if(! schedd.sendCommand(RESCHEDULE, st, 0)) {
        dprintf( D_ALWAYS, "Can't send RESCHEDULE command to schedd.\n" );
    }


    PyObject * pySpooledProcAds = Py_None;
    if( spooledProcAds ) {
        pySpooledProcAds = py_new_htcondor2_spooled_proc_ad_list( spooledProcAds );
    }

    PyObject * pyClusterAd = py_new_classad2_classad(clusterAd->Copy());
    sb->cleanup_submit();
    return py_new_htcondor2_submit_result( clusterID, 0, numJobs, pyClusterAd, pySpooledProcAds );
}


static PyObject *
_history_query(PyObject *, PyObject * args) {
    // _history_query( addr, constraint, projection, match, since, history_record_source, command )

    const char * addr = NULL;
    const char * constraint = NULL;
    const char * projection = NULL;
    long match = 0;
    const char * since = NULL;
    const char * adFilter = NULL;
    long history_record_source = -1;
    long command = -1;
    if(! PyArg_ParseTuple( args, "zzzlzzll", & addr, & constraint, & projection, & match, & since, & adFilter, & history_record_source, & command )) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }


    ClassAd commandAd;

    if( strlen(constraint) != 0 ) {
        if(! commandAd.AssignExpr(ATTR_REQUIREMENTS, constraint)) {
            // This was ClassAdParseError in version 1.
            PyErr_SetString( PyExc_ValueError, "invalid constraint" );
            return NULL;
        }
    }

    commandAd.Assign(ATTR_NUM_MATCHES, match);

    std::string prefix = "";
    std::string projectionList = "{";
	for (auto& attr : StringTokenIterator(projection)) {
        projectionList += prefix + "\"" + attr + "\"";
        prefix = ", ";
    }
    projectionList += "}";
    commandAd.AssignExpr(ATTR_PROJECTION, projectionList.c_str());

    if( since != NULL && since[0] != '\0' ) {
        commandAd.AssignExpr("Since", since);
    }

    if ( adFilter && adFilter[0] != '\0' ) {
        commandAd.InsertAttr(ATTR_HISTORY_AD_TYPE_FILTER, adFilter);
    }

    switch( history_record_source ) {
        case 0: /* HRS_SCHEDD_JOB_HIST */
            break;
        case 1: /* HRS_STARTD_JOB_HIST */
            commandAd.InsertAttr(ATTR_HISTORY_RECORD_SOURCE, "STARTD");
            break;
        case 2: /* HRS_JOB_EPOCH */
            commandAd.InsertAttr(ATTR_HISTORY_RECORD_SOURCE, "JOB_EPOCH");
            break;
        default:
            // This was HTCondorValueError in version 1.
            PyErr_SetString( PyExc_HTCondorException, "unknown history record source" );
            return NULL;
    }


    daemon_t dt = DT_SCHEDD;
    if( command == GET_HISTORY ) {
        dt = DT_STARTD;
    }
    Daemon d(dt, addr);

    CondorError errorStack;
    Sock * sock = d.startCommand( command, Stream::reli_sock, 0, & errorStack );
    if( sock == NULL ) {
        const char * message = errorStack.message(0);
        if( message == NULL || message[0] == '\0' ) {
            message = "Unable to connect to schedd";
            if( dt == DT_STARTD ) { message = "Unable to connect to startd"; }

            // This was HTCondorIOError in version 1.
            PyErr_SetString( PyExc_HTCondorException, message );
            return NULL;
        }
    }

    if(! putClassAd( sock, commandAd )) {
        // This was HTCondorIOError in version 1.
         PyErr_SetString( PyExc_HTCondorException, "Unable to send history request classad" );
        return NULL;
    }

    if(! sock->end_of_message()) {
        // This was HTCondorIOError in version 1.
        PyErr_SetString( PyExc_HTCondorException, "Unable to send end-of-message" );
        return NULL;
    }

    std::vector<ClassAd> results;

    long long owner = -1;
    while( true ) {
        ClassAd result;

        if(! getClassAd( sock, result )) {
            // This was HTCondorIOError in version 1.
            PyErr_SetString( PyExc_HTCondorException, "Failed to receive history ad." );
            return NULL;
        }

        // Why is our code base so broken?  This unconditionally clears `owner.`
        if( result.EvaluateAttrInt(ATTR_OWNER, owner) && (owner == 0) ) {
            if(! sock->end_of_message()) {
                // This was HTCondorIOError in version 1.
                PyErr_SetString( PyExc_HTCondorException, "Failed to receive end-of-message." );
                return NULL;
            }
            sock->close();

            long long errorCode = -1;
            std::string errorMessage;
            if( result.EvaluateAttrInt(ATTR_ERROR_CODE, errorCode) &&
              errorCode != 0 &&
              result.EvaluateAttrString(ATTR_ERROR_STRING, errorMessage) ) {
                // This was HTCondorIOError in version 1.
                PyErr_SetString( PyExc_HTCondorException, errorMessage.c_str() );
                return NULL;
            }

            long long malformedAds = -1;
            if( result.EvaluateAttrInt("MalformedAds", malformedAds) &&
              malformedAds != 0 ) {
                // This was HTCondorIOError in version 1.
                PyErr_SetString( PyExc_HTCondorException, "Remote side had parse errors in history file" );
                return NULL;
            }

            long long matches = -1;
            if( (! result.EvaluateAttrInt(ATTR_NUM_MATCHES, matches)) ||
              matches != (long long)results.size() ) {
                // This was (incorrectly) HTCondorValueError in version 1.
                PyErr_SetString( PyExc_HTCondorException, "Incorrect number of ads received" );
                return NULL;
            }

            break;
        }

        results.push_back(result);
    }

    PyObject * list = PyList_New(0);
    if( list == NULL ) {
        PyErr_SetString( PyExc_MemoryError, "_history_query" );
        return NULL;
    }

    for( auto & classAd : results ) {
        // We could probably dispense with the copy by allocating the
        // result ads on the heap in the first place, but that would
        // require more attention to clean-up in the preceeding loop.
        PyObject * pyClassAd = py_new_classad2_classad(classAd.Copy());
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
retrieve_job_from( const char * addr, const char * constraint ) {
    DCSchedd schedd(addr);

    CondorError errStack;
    bool result = schedd.receiveJobSandbox(constraint, & errStack);
    if(! result) {
        // This was HTCondorIOError in version 1.
        PyErr_SetString(PyExc_HTCondorException, errStack.getFullText().c_str());
        return NULL;
    }

    Py_RETURN_NONE;
}


static PyObject *
_schedd_retrieve_job_ids(PyObject *, PyObject * args) {
    // _schedd_retrieve_job_ids(addr, job_list)

    const char * addr = NULL;
    const char * job_list = NULL;

    if(! PyArg_ParseTuple( args, "zz", & addr, & job_list )) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }

    std::string constraint;
    formatstr( constraint,
        "stringListIMember( strcat(ClusterID, \".\", ProcID), \"%s\" )",
        job_list
    );

    return retrieve_job_from(addr, constraint.c_str());
}


static PyObject *
_schedd_retrieve_job_constraint(PyObject *, PyObject * args) {
    // _schedd_retrieve_job_constraint(addr, constraint)

    const char * addr = NULL;
    const char * constraint = NULL;

    if(! PyArg_ParseTuple( args, "zz", & addr, & constraint )) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }

    return retrieve_job_from( addr, constraint );
}


static PyObject *
_schedd_spool(PyObject *, PyObject * args) {
    // _schedd_spool(addr, sr._clusterad._handle, sr._spooledJobAds._handle)

    const char * addr = NULL;
    PyObject_Handle * clusterAdHandle = NULL;
    PyObject_Handle * spooledProcAdsHandle = NULL;

    if(! PyArg_ParseTuple( args, "sOO", & addr, & clusterAdHandle, & spooledProcAdsHandle )) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }

    auto * clusterAd = (ClassAd *)clusterAdHandle->t;
    auto * spooledProcAds = (std::vector<ClassAd *> *)spooledProcAdsHandle->t;

    // Chain the proc ads.
    for( auto * ad : * spooledProcAds ) {
        ad->ChainToAd( clusterAd );
    }

    DCSchedd schedd(addr);
    CondorError errorStack;
    bool result = schedd.spoolJobFiles(
        spooledProcAds->size(), &(*spooledProcAds)[0],
        & errorStack
    );

    // Unchain the proc ads.
    for( auto * ad : * spooledProcAds ) {
        ad->Unchain();
    }

    if(! result) {
        // This was HTCondorIOError in version 1.
        PyErr_SetString( PyExc_HTCondorException, errorStack.getFullText(true).c_str() );
        return NULL;
    }

    Py_RETURN_NONE;
}


#include "globus_utils.h"

static PyObject *
_schedd_refresh_gsi_proxy(PyObject *, PyObject * args) {
    // _schedd_refresh_gsi_proxy( addr, cluster, proc, path, lifetime )

    const char * addr = NULL;
    long cluster = 0;
    long proc = 0;
    const char * path = NULL;
    long lifetime = 0;
    if(! PyArg_ParseTuple( args, "sllsl", & addr, & cluster, & proc, & path, & lifetime )) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }


    // This is documented as `-1` only, but for backwards-compatibility,
    // accept any negative number.
    if( lifetime < 0 ) {
        lifetime = param_integer("DELEGATE_JOB_GSI_CREDENTIALS_LIFETIME", 0);
    }


    time_t now = time(NULL);
    DCSchedd schedd(addr);

    bool do_delegation = param_boolean("DELEGATE_JOB_GSI_CREDENTIALS", true);
    if( do_delegation ) {
        CondorError errorStack;
        time_t result_expiration;

        bool result = schedd.delegateGSIcredential(
            cluster, proc, path,
            lifetime ? now+lifetime : 0,
            & result_expiration, & errorStack
        );
        if(! result ) {
            // This was HTCondorIOError in version 1.
            PyErr_SetString( PyExc_HTCondorException, errorStack.getFullText(true).c_str() );
            return NULL;
        }

        return PyLong_FromLong(result_expiration - now);
    } else {
        CondorError errorStack;

        bool result = schedd.updateGSIcredential(
            cluster, proc, path,
            & errorStack
        );
        if(! result ) {
            // This was HTCondorIOError in version 1.
            PyErr_SetString( PyExc_HTCondorException, errorStack.getFullText(true).c_str() );
            return NULL;
        }

        time_t result_expiration = x509_proxy_expiration_time(path);
        if( result_expiration < 0 ) {
            // This was HTCondorValueError in version 1.
            PyErr_SetString( PyExc_HTCondorException, "Unable to determine proxy expiration time" );
            return NULL;
        }

        return PyLong_FromLong(result_expiration - now);
    }
}

static PyObject *
_schedd_get_dag_contact_info(PyObject *, PyObject * args) {
    // schedd_reschedule(addr, import_dir)

    const char* addr;
    long cluster = 0;

    if(! PyArg_ParseTuple( args, "sl", & addr, & cluster )) {
        // PyArg_ParseTuple() has already set an exception for us.
        return NULL;
    }


    DCSchedd schedd(addr);
    CondorError errorStack;
    ClassAd * result = schedd.getDAGManContact( cluster, errorStack );

    if( errorStack.code() > 0 ) {
        PyErr_SetString( PyExc_HTCondorException, errorStack.getFullText(true).c_str() );
        return NULL;
    }

    if( result == NULL ) {
        // This was HTCondorIOError in version 1.
        PyErr_SetString( PyExc_HTCondorException, "No result ad" );
        return NULL;
    }

    return py_new_classad2_classad(result->Copy());
}
