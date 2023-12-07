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
		static PyObject * py_htcondor_module = NULL;
		if( py_htcondor_module == NULL ) {
			py_htcondor_module = PyImport_ImportModule( "htcondor2" );
		}

		static PyObject * py_subsystemtype_class = NULL;
		if( py_subsystemtype_class == NULL ) {
			py_subsystemtype_class = PyObject_GetAttrString( py_htcondor_module, "SubsystemType" );
		}

		int ii = PyObject_IsInstance( py_subsystem_type, py_subsystemtype_class );
		switch (ii) {
			case 1:
				break;
			case 0:
				// This was HTCondorTypeError in version 1.
				PyErr_SetString( PyExc_TypeError, "subsystem_ype must be of type htcondor.SubsystemType" );
				return NULL;
			case -1:
				// PyObject_IsInstance() has already set an exception for us.
				return NULL;
			default:
				PyErr_SetString( PyExc_AssertionError, "Undocumented return from PyObject_IsInstance()." );
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


static PyObject *
_reload_config( PyObject *, PyObject * ) {
	// FIXME: In version 1, there was some Windows-specific stuff to do here,
	// which should probably be moved into config() itself.
	config();

	Py_RETURN_NONE;
}
