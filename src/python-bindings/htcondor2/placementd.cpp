static PyObject *
_placement_user_login(PyObject *, PyObject * args) {
	// _placement_user_login(addr, username, authorizations, project, requester)

	const char * addr = nullptr;
	const char * username = nullptr;
	const char * authz = nullptr;
	const char * project = nullptr;
	const char * requester = nullptr;
	if (! PyArg_ParseTuple(args, "ssszz", & addr, & username, & authz, & project, & requester)) {
		// PyArg_ParseTuple() has already set an exception for us.
		return nullptr;
	}

	Daemon placementd(DT_PLACEMENTD, addr);

	Sock* sock = nullptr;
	if (!(sock = placementd.startCommand(PLACEMENT_USER_LOGIN, Stream::reli_sock, 0))) {
		PyErr_SetString( PyExc_HTCondorException, "Failed to contact PlacementD");
		delete sock;
		return nullptr;
	}

	ClassAd cmd_ad;
	cmd_ad.Assign("UserName", username);
	cmd_ad.Assign("Authorizations", authz);
	if (project) {
		cmd_ad.Assign("Project", project);
	}
	if (requester) {
		cmd_ad.Assign("Requester", requester);
	}

	if ( !putClassAd(sock, cmd_ad) || !sock->end_of_message()) {
		PyErr_SetString( PyExc_HTCondorException, "Failed to send request to PlacementD");
		delete sock;
		return nullptr;
	}

	ClassAd result_ad;
	if ( !getClassAd(sock, result_ad) || ! sock->end_of_message()) {
		PyErr_SetString( PyExc_HTCondorException, "Failed to receive reply from PlacementD");
		delete sock;
		return nullptr;
	}

	int error_code = 0;
	std::string error_string = "(unknown)";
	if (result_ad.LookupString(ATTR_ERROR_STRING, error_string)) {
		result_ad.LookupInteger(ATTR_ERROR_CODE, error_code);
		std::string msg;
		formatstr(msg, "PlacementD returned error %d, %s\n", error_code, error_string.c_str());
		PyErr_SetString( PyExc_HTCondorException, msg.c_str());
		delete sock;
		return nullptr;
	}

	delete sock;

	// We could probably construct this on the heap initially, pass the
	// dereferenced pointer to getClassAd(), and skip this copy.
	return py_new_classad2_classad_via_copy(& result_ad);
}

static PyObject *
_placement_query_users(PyObject *, PyObject * args) {
	// _placement_query_users(addr, username)

	const char * addr = nullptr;
	const char * username = nullptr;
	if (! PyArg_ParseTuple(args, "sz", & addr, & username)) {
		// PyArg_ParseTuple() has already set an exception for us.
		return nullptr;
	}

	Daemon placementd(DT_PLACEMENTD, addr);

	Sock* sock = nullptr;
	if (!(sock = placementd.startCommand(PLACEMENT_QUERY_USERS, Stream::reli_sock, 0))) {
		PyErr_SetString( PyExc_HTCondorException, "Failed to contact PlacementD");
		return nullptr;
	}

	ClassAd cmd_ad;
	if (username) {
		cmd_ad.Assign("UserName", username);
	}

	if ( !putClassAd(sock, cmd_ad) || !sock->end_of_message()) {
		PyErr_SetString( PyExc_HTCondorException, "Failed to send request to PlacementD");
		delete sock;
		return nullptr;
	}


	std::vector<std::unique_ptr<ClassAd>> result_list;
	do {
		auto resultAd = std::make_unique<ClassAd>();
		if(! resultAd) {
			PyErr_SetString(PyExc_MemoryError, "_placement_query_users");
		}

		if(! getClassAd(sock, * resultAd) ) {
			PyErr_SetString( PyExc_HTCondorException, "Failed to receive reply from PlacementD\n");
			delete sock;
			return nullptr;
		}

		std::string str;
		if (resultAd->LookupString("MyType", str) && str == "Summary") {
			int error_code = 0;
			std::string error_string = "(unknown)";
			if (resultAd->LookupString(ATTR_ERROR_STRING, error_string)) {
				resultAd->LookupInteger(ATTR_ERROR_CODE, error_code);
				std::string msg;
				formatstr(msg, "PlacementD returned error %d, %s\n", error_code, error_string.c_str());
				PyErr_SetString( PyExc_HTCondorException, msg.c_str());
				delete sock;
				return nullptr;
			}
			sock->end_of_message();
			break;
		}

		result_list.push_back( std::move(resultAd) );
	} while (true);
	delete sock;

	return py_list_from_vector( result_list, "_placement_query_users()" );
}

static PyObject *
_placement_query_tokens(PyObject *, PyObject * args) {
	// _placement_query_tokens(addr, username, valid_only)

	const char * addr = nullptr;
	const char * username = nullptr;
	int valid_only = false;
	const char * token_id = nullptr;
	if (! PyArg_ParseTuple(args, "szpz", & addr, & username, & valid_only, & token_id)) {
		// PyArg_ParseTuple() has already set an exception for us.
		return nullptr;
	}

	Daemon placementd(DT_PLACEMENTD, addr);

	Sock* sock = nullptr;
	if (!(sock = placementd.startCommand(PLACEMENT_QUERY_TOKENS, Stream::reli_sock, 0))) {
		PyErr_SetString( PyExc_HTCondorException, "Failed to contact PlacementD");
		return nullptr;
	}

	ClassAd cmd_ad;
	if (token_id) {
		cmd_ad.Assign("TokenId", token_id);
	}
	if (username) {
		cmd_ad.Assign("UserName", username);
	}
	cmd_ad.Assign("ValidOnly", (bool)valid_only);

	if ( !putClassAd(sock, cmd_ad) || !sock->end_of_message()) {
		PyErr_SetString( PyExc_HTCondorException, "Failed to send request to PlacementD");
		delete sock;
		return nullptr;
	}


	std::vector<std::unique_ptr<ClassAd>> result_list;
	do {
		auto resultAd = std::make_unique<ClassAd>();
		if(! resultAd ) {
			PyErr_SetString(PyExc_MemoryError, "_placement_query_tokens");
			delete sock;
			return nullptr;
		}

		if(! getClassAd(sock, * resultAd)) {
			PyErr_SetString( PyExc_HTCondorException, "Failed to receive reply from PlacementD\n");
			delete sock;
			return nullptr;
		}

		std::string str;
		if (resultAd->LookupString("MyType", str) && str == "Summary") {
			int error_code = 0;
			std::string error_string = "(unknown)";
			if (resultAd->LookupString(ATTR_ERROR_STRING, error_string)) {
				resultAd->LookupInteger(ATTR_ERROR_CODE, error_code);
				std::string msg;
				formatstr(msg, "PlacementD returned error %d, %s\n", error_code, error_string.c_str());
				PyErr_SetString( PyExc_HTCondorException, msg.c_str());
				delete sock;
				return nullptr;
			}
			sock->end_of_message();
			break;
		}

		result_list.push_back( std::move(resultAd) );
	} while (true);
	delete sock;

	return py_list_from_vector( result_list, "placement_query_tokens" );
}

static PyObject *
_placement_query_authorizations(PyObject *, PyObject * args) {
	// _placement_query_authorizations(addr, username)

	const char * addr = nullptr;
	const char * username = nullptr;
	if (! PyArg_ParseTuple(args, "sz", & addr, & username)) {
		// PyArg_ParseTuple() has already set an exception for us.
		return nullptr;
	}

	Daemon placementd(DT_PLACEMENTD, addr);

	Sock* sock = nullptr;
	if (!(sock = placementd.startCommand(PLACEMENT_QUERY_AUTHORIZATIONS, Stream::reli_sock, 0))) {
		PyErr_SetString( PyExc_HTCondorException, "Failed to contact PlacementD");
		return nullptr;
	}

	ClassAd cmd_ad;
	if (username) {
		cmd_ad.Assign("UserName", username);
	}

	if ( !putClassAd(sock, cmd_ad) || !sock->end_of_message()) {
		PyErr_SetString( PyExc_HTCondorException, "Failed to send request to PlacementD");
		delete sock;
		return nullptr;
	}


	std::vector<std::unique_ptr<ClassAd>> result_list;
	do {
		auto resultAd = std::make_unique<ClassAd>();
		if(! resultAd ) {
			PyErr_SetString(PyExc_MemoryError, "_placement_query_authorizations");
			delete sock;
			return nullptr;
		}

		if(! getClassAd(sock, * resultAd)) {
			PyErr_SetString( PyExc_HTCondorException, "Failed to receive reply from PlacementD\n");
			delete sock;
			return nullptr;
		}

		std::string str;
		if (resultAd->LookupString("MyType", str) && str == "Summary") {
			int error_code = 0;
			std::string error_string = "(unknown)";
			if (resultAd->LookupString(ATTR_ERROR_STRING, error_string)) {
				resultAd->LookupInteger(ATTR_ERROR_CODE, error_code);
				std::string msg;
				formatstr(msg, "PlacementD returned error %d, %s\n", error_code, error_string.c_str());
				PyErr_SetString( PyExc_HTCondorException, msg.c_str());
				delete sock;
				return nullptr;
			}
			sock->end_of_message();
			break;
		}

		result_list.push_back( std::move(resultAd) );
	} while (true);
	delete sock;

	return py_list_from_vector( result_list, "placement_query_authorizations" );
}
