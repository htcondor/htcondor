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

	handle->f = [](void *& v) { dprintf( D_ALWAYS, "[CollectorList]\n" ); delete (CollectorList *)v; v = NULL; };
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
			PyObject * pyClassAd = py_new_classad_classad(classAd->Copy());
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
				PyErr_SetString( PyExc_RuntimeError, "Invalid query." );
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

		PyObject * pyClassAd = py_new_classad_classad(locateOnlyAd->Copy());
		// PyObject * list = Py_BuildValue( "[N]", pyClassAd );
		return pyClassAd;
	} else {
		// This was HTCondorLocateError in version 1.
		PyErr_SetString( PyExc_RuntimeError, "Unable to locate local daemon." );
		return NULL;
	}
}


static PyObject *
_collector_advertise( PyObject *, PyObject * ) {
	PyErr_SetString( PyExc_NotImplementedError, "_collector_advertise" );
	return NULL;
}


static PyObject *
_hack( PyObject *, PyObject * ) {
	Py_RETURN_NONE;
}
