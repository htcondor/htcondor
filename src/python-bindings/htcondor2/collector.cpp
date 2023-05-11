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
	}

	Py_RETURN_NONE;
}


static PyObject *
_collector_query( PyObject *, PyObject * args ) {
    // _collector_query(_handle, ad_type, constraint, projection, statistics, name)

	PyObject_Handle * handle = NULL;
	AdTypes ad_type = NO_AD;
	const char * constraint = NULL;
	PyObject * projection = NULL;
	PyObject * statistics = NULL;
	const char * name = NULL;

	if(! PyArg_ParseTuple( args, "OlzOOz", & handle, & ad_type, & constraint, & projection, & statistics, & name )) {
		// PyArg_ParseTuple() has already set an exception for us.
		return NULL;
	}


	CondorQuery query(ad_type);

	if( constraint && strlen(constraint) ) {
		query.addANDConstraint(constraint);
	}

	/* FIXME: statistics */

	if( name && strlen(name) ) {
		query.addExtraAttribute( "LocationQuery", name );
	}

	/* FIXME: projection */

	ClassAdList adList;
	QueryResult result;
	{
		// FIXME: condor::ModuleLock ml;
		auto * collectorList = (CollectorList *)handle->t;
		result = collectorList->query(query, adList, NULL);
	}

	if( result == Q_OK ) {

ClassAd * classAd = NULL;
for( adList.Open(); ( classAd = adList.Next() ); ) {
	// dPrintAd( D_ALWAYS, * classAd );
	std::string name;
	classAd->LookupString( "Name", name );
	dprintf( D_ALWAYS, "_condor_query(): found '%s'\n", name.c_str() );
}

		// FIXME: return a Python list of Python ClassAds.
		Py_RETURN_NONE;
	} else {
		// FIXME

		PyErr_SetString( PyExc_NotImplementedError, "_collector_query" );
		return NULL;
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


	// FIXME

	PyErr_SetString( PyExc_NotImplementedError, "_collector_locate" );
	return NULL;
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
