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
_disable_debug( PyObject *, PyObject * ) {
	dprintf_make_thread_safe();
	dprintf_deconfig_tool();

	Py_RETURN_NONE;
}


static PyObject *
_enable_log( PyObject *, PyObject * ) {
	dprintf_make_thread_safe();
	dprintf_config_ContinueOnFailure(1);
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
		PyErr_SetString( PyExc_HTCondorException, "Unable to locate daemon." );
		return NULL;
	}

	ReliSock sock;
	CondorError errorStack;
	{
		result = sock.connect( d.addr(), 0, false, & errorStack );
	}
	if(! result) {
		// This was HTCondorIOError in version 1.
		PyErr_SetString( PyExc_HTCondorException, "Unable to connect to the remote daemon." );
		return NULL;
	}

	{
		result = d.startCommand(command, & sock, 0, NULL);
	}
	if(! result) {
		// This was HTCondorIOError in version 1.
		PyErr_SetString( PyExc_HTCondorException, "Failed to start command." );
		return NULL;
	}

	if( target != NULL ) {
		std::string api_failure(target);
		if(! sock.code(api_failure)) {
			// This was HTCondorIOError in version 1.
			PyErr_SetString( PyExc_HTCondorException, "Failed to send target." );
			return NULL;
		}
	}

	// Version 1 only sent the end-of-message if the command had a payload.
	if(! sock.end_of_message()) {
		// This was HTCondorIOError in version 1.
		PyErr_SetString( PyExc_HTCondorException, "Failed to send end-of-message." );
		return NULL;
	}

	sock.close();

	Py_RETURN_NONE;
}


static PyObject *
_ping( PyObject *, PyObject * args ) {
	// _ping(ad._handle, authz)

	const char* addr = nullptr;
	long command = -1;
	const char * authz = nullptr;
	DCpermission authz_int = NOT_A_PERM;

	if(! PyArg_ParseTuple(args, "sz", & addr, & authz )) {
		// PyArg_ParseTuple() has already set an exception for us.
		return nullptr;
	};

	Daemon d(DT_ANY, addr, nullptr);

	// authz is the string form of an authorization level or a CEDAR command.
	// Figure out the appropriate CEDAR command integer.
	if (authz == nullptr) {
		authz = "DC_NOP";
	}
	authz_int = getPermissionFromString(authz);
	if (authz_int != NOT_A_PERM) {
		command = getSampleCommand(authz_int);
	} else {
		command = getCommandNum(authz);
	}
	if (command == -1) {
		// This was HTCondorEnumError in version 1.
		PyErr_SetString(PyExc_HTCondorException, "Unable to determine DaemonCore command value.");
		return nullptr;
	}

	ReliSock sock;
	CondorError errorStack;
	if (!sock.connect(d.addr(), 0, false, &errorStack)) {
		// This was HTCondorIOError in version 1.
		PyErr_SetString(PyExc_HTCondorException, "Unable to connect to the remote daemon.");
		return nullptr;
	}

	if (!d.startSubCommand(DC_SEC_QUERY, command, &sock, 0, nullptr)) {
		// This was HTCondorIOError in version 1.
		PyErr_SetString(PyExc_HTCondorException, "Failed to start command.");
		return NULL;
	}

	sock.decode();
	ClassAd reply_ad;
	if(!getClassAd(&sock, reply_ad) || !sock.end_of_message()) {
		// This was HTCondorIOError in version 1.
		PyErr_SetString(PyExc_HTCondorException, "Failed to send end-of-message.");
		return NULL;
	}

	// Merge the session and Sock policy ads into the ad we got back from the dameon.
	sock.getPolicyAd(reply_ad);
	auto itr = (SecMan::session_cache)->find(sock.getSessionID());
	if (itr == (SecMan::session_cache)->end()) {
		PyErr_SetString(PyExc_HTCondorException, "Failed to find session.");
		return nullptr;
	}
	reply_ad.Update(*(itr->second.policy()));

	sock.close();

	PyObject * pyClassAd = py_new_classad2_classad(reply_ad.Copy());
	return pyClassAd;
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
		PyErr_SetString( PyExc_HTCondorException, "Failed to deliver keepalive message." );
		return NULL;
	}


	Py_RETURN_NONE;
}


static PyObject *
_set_ready_state( PyObject *, PyObject * args ) {
	const char * ready_state = NULL;
	const char * master_sinful = NULL;

	if(! PyArg_ParseTuple( args, "ss", & ready_state, & master_sinful )) {
		// PyArg_ParseTuple() has already set an exception for us.
		return NULL;
	}


    // Cribbed from dc_tool.cpp, of course.
    ClassAd readyAd;
    readyAd.Assign( "DaemonPID", getpid() );
    readyAd.Assign( "DaemonName", get_mySubSystemName() );
    readyAd.Assign( "DaemonState", ready_state );

    classy_counted_ptr<Daemon> d = new Daemon( DT_ANY, master_sinful );
    classy_counted_ptr<ClassAdMsg> m = new ClassAdMsg( DC_SET_READY, readyAd );
    d->sendBlockingMsg(m.get());

    if( m->deliveryStatus() != DCMsg::DELIVERY_SUCCEEDED ) {
        PyErr_SetString( PyExc_HTCondorException, "Failed to deliver ready message." );
        return NULL;
    }

	Py_RETURN_NONE;
}


static PyObject *
_send_generic_payload_command( PyObject *, PyObject * args ) {
	PyObject_Handle * handle = nullptr;
	const char * address = nullptr;
	long command = -1;

	if(! PyArg_ParseTuple( args, "zlO", & address, & command, (PyObject **)& handle )) {
		// PyArg_ParseTuple() has already set an exception for us.
		return nullptr;
	}

	// Tuple[ClassAd, Error Int, Error String]
	PyObject * result_tuple = PyTuple_New(3);

	Daemon d(DT_GENERIC, address);
	const ClassAd * sendPayload = (ClassAd *)handle->t;

	Sock * sock = d.startCommand(command, Stream::reli_sock, 0);
	if (! sock) {
		PyTuple_SetItem(result_tuple, 0, Py_None);
		PyTuple_SetItem(result_tuple, 1, PyLong_FromLong(1));
		PyTuple_SetItem(result_tuple, 2, PyUnicode_FromString("Failed to open socket"));
		return result_tuple;
	}

	if (! putClassAd(sock, *sendPayload) || ! sock->end_of_message()) {
		sock->close();
		PyTuple_SetItem(result_tuple, 0, Py_None);
		PyTuple_SetItem(result_tuple, 1, PyLong_FromLong(2));
		PyTuple_SetItem(result_tuple, 2, PyUnicode_FromString("Failed to send command to daemon"));
		return result_tuple;
	}

	sock->decode();

	ClassAd returnPayload;

	if (! getClassAd(sock, returnPayload) || ! sock->end_of_message()) {
		sock->close();
		PyTuple_SetItem(result_tuple, 0, Py_None);
		PyTuple_SetItem(result_tuple, 1, PyLong_FromLong(3));
		PyTuple_SetItem(result_tuple, 2, PyUnicode_FromString("Failed to get response from daemon"));
		return result_tuple;
	}

	PyTuple_SetItem(result_tuple, 0, py_new_classad2_classad(returnPayload.Copy()));
	PyTuple_SetItem(result_tuple, 1, PyLong_FromLong(0));
	PyTuple_SetItem(result_tuple, 2, Py_None);

	return result_tuple;
}
