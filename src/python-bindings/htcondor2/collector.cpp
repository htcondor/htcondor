#include "condor_adtypes.h"
#include "daemon_types.h"

// FIXME: This only handles if `pool` is a string (or None).  The documentation
// claims we also handle a list of strings.  The old implementation also let
// the user pass in a `ClassAd` or a `DaemonLocation`.
static PyObject *
_collector_init( PyObject *, PyObject * args ) {
	PyObject * self = NULL;
	PyObject_Handle * handle = NULL;
	const char * pool = NULL;
	if(! PyArg_ParseTuple( args, "OOz", & self, (PyObject **)& handle, & pool )) {
		// PyArg_ParseTuple() has already set an exception for us.
		return NULL;
	}

	handle->f = [](void *& v) { dprintf( D_PERF_TRACE, "[unconstructed CollectorList]\n" ); if(v != NULL){ dprintf(D_ALWAYS, "Error!  Unconstructed Collector has non-NULL handle %p\n", v); } };

	if( pool == NULL || strlen(pool) == 0 ) {
		handle->t = (void *) CollectorList::create();

		if( PyObject_SetAttrString( self, "default", Py_True ) != 0 ) {
			// PyObject_setAttrString() has already set an exception for us.
			return NULL;
		}
	} else {
		handle->t = (void *) CollectorList::create(pool);

		if( PyObject_SetAttrString( self, "default", Py_False ) != 0 ) {
			// PyObject_setAttrString() has already set an exception for us.
			return NULL;
		}
	}


	handle->f = [](void *& v) { dprintf( D_PERF_TRACE, "[CollectorList]\n" ); delete (CollectorList *)v; v = NULL; };
	Py_RETURN_NONE;
}


static PyObject *
_collector_query( PyObject *, PyObject * args ) {
	// _collector_query(_handle, ad_type, constraint, projection, statistics, name)

	PyObject_Handle * handle = NULL;
	AdTypes ad_type = NO_AD;
	const char * constraint = NULL;
	PyObject * projection = NULL;
	const char * statistics = NULL;
	const char * name = NULL;

	if(! PyArg_ParseTuple( args, "OlzOzz", & handle, & ad_type, & constraint, & projection, & statistics, & name )) {
		// PyArg_ParseTuple() has already set an exception for us.
		return NULL;
	}


	CondorQuery query(ad_type);

	if( constraint && strlen(constraint) ) {
		query.addANDConstraint(constraint);
	}

	if( statistics && strlen(statistics) ) {
		query.addExtraAttributeString( "STATISTICS_TO_PUBLISH", statistics );
	}

	if( name && strlen(name) ) {
		query.addExtraAttributeString( "LocationQuery", name );
	}

	if(! PyList_Check(projection)) {
		PyErr_SetString(PyExc_TypeError, "projection must be a list");
		return NULL;
	}

	// In version 1, an empty list was ignored.
	Py_ssize_t size = PyList_Size(projection);
	if( size > 0 ) {
		std::vector<std::string> attributes;
		int rv = py_list_to_vector_of_strings(projection, attributes, "projection");
		if( rv == -1) {
			// py_list_to_vector_of_strings() has already set an exception for us.
			return NULL;
		}

		query.setDesiredAttrs(attributes);
	}

	ClassAdList adList;
	QueryResult result;
	{
		// FIXME: condor::ModuleLock ml;
		auto * collectorList = (CollectorList *)handle->t;
		result = collectorList->query(query, adList, NULL);
	}

	if( result == Q_OK ) {
		PyObject * list = PyList_New(0);
		if( list == NULL ) {
			PyErr_SetString( PyExc_MemoryError, "_collector_query" );
			return NULL;
		}

		ClassAd * classAd = NULL;
		for( adList.Open(); ( classAd = adList.Next() ); ) {
			PyObject * pyClassAd = py_new_classad2_classad(classAd->Copy());
			auto rv = PyList_Append( list, pyClassAd );
			Py_DecRef(pyClassAd);

			if(rv != 0) {
				// PyList_Append() has already set an exception for us.
				return NULL;
			}
		}

		return list;
	} else {
		switch (result) {
			case Q_COMMUNICATION_ERROR:
				// This was HTCondorIOError in version 1.
				PyErr_SetString( PyExc_RuntimeError, "Failed communication with collector." );
				return NULL;

			case Q_INVALID_QUERY:
				// This was HTCondorIOError in version 1.
				PyErr_SetString( PyExc_RuntimeError, "Invalid query." );
				return NULL;

			case Q_NO_COLLECTOR_HOST:
				// This was HTCondorLocateError in version 1.
				PyErr_SetString( PyExc_RuntimeError, "Unable to determine collector host." );
				return NULL;

			// In version 1, we believe these errors were impossible.
			case Q_PARSE_ERROR:
			case Q_MEMORY_ERROR:
			case Q_INVALID_CATEGORY:

			default:
				// This was HTCondorInternalError in version 1.
				PyErr_SetString( PyExc_RuntimeError, "Unknown error from collector query." );
				return NULL;
		}
	}
}


static PyObject *
_collector_locate_local( PyObject *, PyObject * args ) {
	// _collector_locate_local(self, _handle, daemon_type)

	PyObject * self = NULL;
	PyObject_Handle * handle = NULL;
	daemon_t daemon_type = DT_NONE;
	if(! PyArg_ParseTuple( args, "OOl", & self, (PyObject **)& handle, & daemon_type )) {
		// PyArg_ParseTuple() has already set an exception for us.
		return NULL;
	}

	Daemon local( daemon_type, 0, 0 );
	if( local.locate() ) {
		ClassAd * locateOnlyAd = local.locationAd();
		if( locateOnlyAd == NULL ) {
			PyErr_SetString( PyExc_RuntimeError, "Found local daemon but failed to acquire its ad." );
			return NULL;
		}

		PyObject * pyClassAd = py_new_classad2_classad(locateOnlyAd->Copy());
		// PyObject * list = Py_BuildValue( "[N]", pyClassAd );
		return pyClassAd;
	} else {
		// This was HTCondorLocateError in version 1.
		PyErr_SetString( PyExc_RuntimeError, "Unable to locate local daemon." );
		return NULL;
	}
}


static PyObject *
_collector_advertise( PyObject *, PyObject * args ) {
	// _collector_advertise( self._handle, ad_list, command, use_tcp )

	PyObject_Handle * handle = NULL;
	PyObject * py_list_of_ads = NULL;
	const char * command_str = NULL;
	int use_tcp = 1;
	if(! PyArg_ParseTuple( args, "OOzp", (PyObject **)& handle, & py_list_of_ads, & command_str, & use_tcp)) {
		// PyArg_ParseTuple() has already set an exception for us.
		return NULL;
	}

	auto * collectorList = (CollectorList *)handle->t;

	Py_ssize_t length = PyList_Size(py_list_of_ads);
	if( length == 0 ) {
		Py_RETURN_NONE;
	}

	int command = getCollectorCommandNum(command_str);
	if( command == -1 ) {
		std::string exception_text = "invalid command ";
		exception_text += command_str;

		// This was HTCondorEnumError in version 1.
		PyErr_SetString( PyExc_ValueError, exception_text.c_str() );
		return NULL;
	}
	if( command == UPDATE_STARTD_AD_WITH_ACK ) {
		PyErr_SetString( PyExc_NotImplementedError, "startd-with-ack protocol" );
		return NULL;
	}


	ClassAd ad;
	std::unique_ptr<Sock> sock;

    for( auto & collector : collectorList->getList() ) {
		if(! collector->locate()) {
			// This was HTCondorLocateError in version 1.
			PyErr_SetString( PyExc_RuntimeError, "Unable to locate collector." );
			return NULL;
		}

		sock.reset();
		for( Py_ssize_t i = 0; i < length; ++i ) {
			PyObject * py_classad = PyList_GetItem(py_list_of_ads, i);
			// FIXME: We should instead verify these on the Python side.
			int check = py_is_classad2_classad(py_classad);
			if( check == -1 ) {
				return NULL;
			}
			if( check == 0 ) {
				PyErr_SetString( PyExc_TypeError, "ad_list entries must be ClassAds" );
				return NULL;
			}

			// Not sure we actually need to make this copy, but version 1 did.
			auto * handle = get_handle_from(py_classad);
			ClassAd * classad = (ClassAd *)handle->t;
			ad.CopyFrom(* classad);

			int result = 0;
			{
				// FIXME: condor::ModuleLock ml;
				if( use_tcp ) {
					if(! sock.get()) {
						sock.reset(collector->startCommand(command,Stream::reli_sock,20));
					} else {
						sock->encode();
						sock->put(command);
					}
				} else {
					sock.reset(collector->startCommand(command,Stream::safe_sock,20));
				}

				if( sock.get() ) {
					result += putClassAd( sock.get(), ad );
					result += sock->end_of_message();
				}
			}

			if( result != 2 ) {
				// This was HTCondorIOError in version 1.
				PyErr_SetString( PyExc_IOError, "Failed to advertise to collector." );
				return NULL;
			}
		}

		sock->encode();
		sock->put(DC_NOP);
		sock->end_of_message();
	}

	Py_RETURN_NONE;
}
