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
			py_htcondor_module = PyImport_ImportModule( HTCONDOR2_MODULE_NAME );
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


static PyObject *
_enable_debug( PyObject *, PyObject * ) {
	dprintf_make_thread_safe();
	dprintf_set_tool_debug(get_mySubSystem()->getName(), 0);

	Py_RETURN_NONE;
}


static PyObject *
_enable_log( PyObject *, PyObject * ) {
	dprintf_make_thread_safe();
	dprintf_config(get_mySubSystem()->getName());

	Py_RETURN_NONE;
}


static PyObject *
_dprintf_dfulldebug( PyObject *, PyObject * args ) {
	const char * str = NULL;

	if(! PyArg_ParseTuple( args, "s", & str )) {
		// PyArg_ParseTuple() has already set an exception for us.
		return NULL;
	}

	dprintf( D_FULLDEBUG, "%s", str );

	Py_RETURN_NONE;
}


static PyObject *
_py_dprintf( PyObject *, PyObject * args ) {
	long debug_level = 0;
	const char * str = NULL;

	if(! PyArg_ParseTuple( args, "ls", & debug_level, & str )) {
		// PyArg_ParseTuple() has already set an exception for us.
		return NULL;
	}

	dprintf( debug_level, "%s", str );

	Py_RETURN_NONE;
}


static PyObject *
_send_command( PyObject *, PyObject * args ) {
	// _send_command(ad._handle, daemon_type, daemon_command, target)

	daemon_t daemonType = DT_NONE;
	long command = -1;
	const char * target = NULL;
	PyObject_Handle * handle = NULL;

	if(! PyArg_ParseTuple( args, "Ollz", (PyObject **)& handle, & daemonType, & command, & target )) {
		// PyArg_ParseTuple() has already set an exception for us.
		return NULL;
	}

	ClassAd * locationAd = (ClassAd *)handle->t;

	// Not sure we actually need to make this copy, but version 1 did.
	ClassAd copy;
	copy.CopyFrom(* locationAd);

	bool result;
	Daemon d(& copy, daemonType, NULL);
	{
		result = d.locate(Daemon::LOCATE_FOR_ADMIN);
	}
	if(! result) {
		// This was HTCondorLocateError in version 1.
		PyErr_SetString( PyExc_RuntimeError, "Unable to locate daemon." );
		return NULL;
	}

	ReliSock sock;
	CondorError errorStack;
	{
		result = sock.connect( d.addr(), 0, false, & errorStack );
	}
	if(! result) {
		// This was HTCondorIOError in version 1.
		PyErr_SetString( PyExc_IOError, "Unable to connect to the remote daemon." );
		return NULL;
	}

	{
		result = d.startCommand(command, & sock, 0, NULL);
	}
	if(! result) {
		// This was HTCondorIOError in version 1.
		PyErr_SetString( PyExc_IOError, "Failed to start command." );
		return NULL;
	}

	if( target != NULL ) {
		std::string api_failure(target);
		if(! sock.code(api_failure)) {
			// This was HTCondorIOError in version 1.
			PyErr_SetString( PyExc_IOError, "Failed to send target." );
			return NULL;
		}
	}

	// Version 1 only sent the end-of-message if the command had a payload.
	if(! sock.end_of_message()) {
		// This was HTCondorIOError in version 1.
		PyErr_SetString( PyExc_IOError, "Failed to send end-of-message." );
		return NULL;
	}

	sock.close();

	Py_RETURN_NONE;
}


static PyObject *
_send_alive( PyObject *, PyObject * args ) {
	// _send_alive( addr, pid, timeout )

	const char * addr = NULL;
	long pid = -1;
	long timeout = -1;

	if(! PyArg_ParseTuple( args, "sll", & addr, & pid, & timeout )) {
		// PyArg_ParseTuple() has already set an exception for us.
		return NULL;
	}


	auto * daemon = new Daemon( DT_ANY, addr );
	auto * message = new ChildAliveMsg( pid, timeout, 0, 0, true );
	daemon->sendBlockingMsg(message);
	if( message->deliveryStatus() != DCMsg::DELIVERY_SUCCEEDED) {
		// This was HTCondorIOError in version 1.
		PyErr_SetString( PyExc_IOError, "Failed to deliver keepalive message." );
		return NULL;
	}


	Py_RETURN_NONE;
}
