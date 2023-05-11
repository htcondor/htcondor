static PyObject *
_version( PyObject *, PyObject * ) {
	return PyUnicode_FromString( CondorVersion() );
}


static PyObject *
_platform( PyObject *, PyObject * ) {
	return PyUnicode_FromString( CondorPlatform() );
}


static PyObject *
_set_subsystem( PyObject *, PyObject * args ) {
	const char * subsystem_name = NULL;
	PyObject * py_subsystem_type = NULL;

	if(! PyArg_ParseTuple( args, "s|O", & subsystem_name, &py_subsystem_type )) {
		// PyArg_ParseTuple() has already set an exception for us.
		return NULL;
	}


	// If py_subsystem_type is not NULL, set subsystem_type.
	SubsystemType subsystem_type = SUBSYSTEM_TYPE_AUTO;
	if( py_subsystem_type ) {
		// We can't use a C reference to a global here because the
		// `htcondor.SubsystemType` type was defined by Boost.
		PyObject * py_htcondor_module = PyImport_ImportModule( "htcondor" );
		PyObject * py_subsystemtype_class = PyObject_GetAttrString( py_htcondor_module, "SubsystemType" );
		if(! PyObject_IsInstance( py_subsystem_type, py_subsystemtype_class )) {
			// This is technically an API violation; we should raise an
			// instance of HTCondorTypeError, instead.
			PyErr_SetString( PyExc_TypeError, "daemonType must be of type htcondor.SubsystemType" );
			return NULL;
		}

		// SubsystemType is defined to be a long.
		subsystem_type = (SubsystemType)PyLong_AsLong(py_subsystem_type);
		if( PyErr_Occurred() ) {
			return NULL;
		}
	}


	// I have no idea why we assume `false` here.
	set_mySubSystem( subsystem_name, false, subsystem_type );

	// I have no idea why we set `m_trusted` here.
	SubsystemInfo * subsystem = get_mySubSystem();
	if( subsystem->isDaemon() ) { subsystem->setIsTrusted( true ); }

	Py_RETURN_NONE;
}
