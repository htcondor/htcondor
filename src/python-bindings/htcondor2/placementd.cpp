// A reply from the placementd failed if it carries a non-zero ErrorCode
// or any ErrorString.  Sets the exception and returns true if so.  If
// results is not null, the exception's results attribute is set to it
// (the per-operation result ads of a failed edit).
static bool
_placement_reply_failed(const ClassAd & ad, PyObject * results = nullptr) {
	int error_code = 0;
	std::string error_string;
	bool has_string = ad.LookupString(ATTR_ERROR_STRING, error_string);
	ad.LookupInteger(ATTR_ERROR_CODE, error_code);
	if (error_code == 0 && ! has_string) {
		return false;
	}
	if (! has_string) {
		error_string = "(unknown)";
	}
	std::string msg;
	formatstr(msg, "PlacementD returned error %d, %s\n", error_code, error_string.c_str());
	if (results == nullptr) {
		PyErr_SetString( PyExc_HTCondorException, msg.c_str());
		return true;
	}
	PyObject * exc = PyObject_CallFunction( PyExc_HTCondorException, "s", msg.c_str() );
	if (exc == nullptr) {
		// PyObject_CallFunction() has already set an exception for us.
		return true;
	}
	if (PyObject_SetAttrString(exc, "results", results) != 0) {
		Py_DecRef(exc);
		return true;
	}
	PyErr_SetObject( PyExc_HTCondorException, exc );
	Py_DecRef(exc);
	return true;
}

// Send a command with its request ads and one end of message.  Returns
// the socket, or nullptr with an exception set.
static Sock *
_placement_send(const char * addr, int cmd, const std::vector<const ClassAd *> & request) {
	Daemon placementd(DT_PLACEMENTD, addr);

	Sock* sock = placementd.startCommand(cmd, Stream::reli_sock, 0);
	if (! sock) {
		PyErr_SetString( PyExc_HTCondorException, "Failed to contact PlacementD");
		return nullptr;
	}

	bool sent = true;
	for (const ClassAd * ad: request) {
		sent = sent && putClassAd(sock, *ad);
	}
	if ( !sent || !sock->end_of_message()) {
		PyErr_SetString( PyExc_HTCondorException, "Failed to send request to PlacementD");
		delete sock;
		return nullptr;
	}
	return sock;
}

// Read result ads up to and including the Summary ad, then close the
// socket.  Returns a new list, or nullptr with an exception set.
static PyObject *
_placement_read_results(Sock * sock, const char * fname) {
	std::unique_ptr<Sock> owned(sock);

	PyObject * list = PyList_New(0);
	if (list == nullptr) {
		PyErr_SetString(PyExc_MemoryError, fname);
		return nullptr;
	}

	ClassAd result_ad;
	do {
		result_ad.Clear();
		if ( !getClassAd(sock, result_ad)) {
			PyErr_SetString( PyExc_HTCondorException, "Failed to receive reply from PlacementD\n");
			Py_DecRef(list);
			return nullptr;
		}

		std::string str;
		if (result_ad.LookupString(ATTR_MY_TYPE, str) && str == "Summary") {
			if (_placement_reply_failed(result_ad, list)) {
				Py_DecRef(list);
				return nullptr;
			}
			sock->end_of_message();
			break;
		}

		PyObject * pyClassAd = py_new_classad2_classad(result_ad.Copy());
		auto rv = PyList_Append(list, pyClassAd);
		Py_DecRef(pyClassAd);
		if (rv != 0) {
			// PyList_Append() has already set an exception for us.
			Py_DecRef(list);
			return nullptr;
		}
	} while (true);

	return list;
}

// Send one request ad and read the list of results.
static PyObject *
_placement_list_command(const char * addr, int cmd, const ClassAd & cmd_ad, const char * fname) {
	Sock * sock = _placement_send(addr, cmd, {& cmd_ad});
	if (! sock) {
		return nullptr;
	}
	return _placement_read_results(sock, fname);
}

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

	ClassAd cmd_ad;
	cmd_ad.Assign("UserName", username);
	cmd_ad.Assign("Authorizations", authz);
	if (project) {
		cmd_ad.Assign("Project", project);
	}
	if (requester) {
		cmd_ad.Assign("Requester", requester);
	}

	std::unique_ptr<Sock> sock(_placement_send(addr, PLACEMENT_USER_LOGIN, {& cmd_ad}));
	if (! sock) {
		return nullptr;
	}

	ClassAd result_ad;
	if ( !getClassAd(sock.get(), result_ad) || ! sock->end_of_message()) {
		PyErr_SetString( PyExc_HTCondorException, "Failed to receive reply from PlacementD");
		return nullptr;
	}
	if (_placement_reply_failed(result_ad)) {
		return nullptr;
	}

	return py_new_classad2_classad(result_ad.Copy());
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

	ClassAd cmd_ad;
	if (username) {
		cmd_ad.Assign("UserName", username);
	}
	return _placement_list_command(addr, PLACEMENT_QUERY_USERS, cmd_ad, "_placement_query_users");
}

static PyObject *
_placement_query_tokens(PyObject *, PyObject * args) {
	// _placement_query_tokens(addr, username, valid_only, token_id)

	const char * addr = nullptr;
	const char * username = nullptr;
	int valid_only = false;
	const char * token_id = nullptr;
	if (! PyArg_ParseTuple(args, "szpz", & addr, & username, & valid_only, & token_id)) {
		// PyArg_ParseTuple() has already set an exception for us.
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
	return _placement_list_command(addr, PLACEMENT_QUERY_TOKENS, cmd_ad, "_placement_query_tokens");
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

	ClassAd cmd_ad;
	if (username) {
		cmd_ad.Assign("UserName", username);
	}
	return _placement_list_command(addr, PLACEMENT_QUERY_AUTHORIZATIONS, cmd_ad, "_placement_query_authorizations");
}

static PyObject *
_placement_query(PyObject *, PyObject * args) {
	// _placement_query(addr, target_type, id, name, constraint, projection, limit, entity_id)

	const char * addr = nullptr;
	const char * target_type = nullptr;
	const char * id = nullptr;
	const char * name = nullptr;
	const char * constraint = nullptr;
	const char * projection = nullptr;
	long long limit = -1;
	const char * entity_id = nullptr;
	if (! PyArg_ParseTuple(args, "sszzzzLz", & addr, & target_type, & id, & name, & constraint, & projection, & limit, & entity_id)) {
		// PyArg_ParseTuple() has already set an exception for us.
		return nullptr;
	}

	ClassAd cmd_ad;
	cmd_ad.Assign(ATTR_TARGET_TYPE, target_type);
	if (id) {
		cmd_ad.Assign("Id", id);
	}
	if (name) {
		// The natural key is Identity for an Identity, Name otherwise.
		cmd_ad.Assign(strcasecmp(target_type, "Identity") == 0 ? "Identity" : ATTR_NAME, name);
	}
	if (constraint && constraint[0]) {
		if (! cmd_ad.AssignExpr(ATTR_REQUIREMENTS, constraint)) {
			PyErr_SetString( PyExc_HTCondorException, "Parse error in constraint");
			return nullptr;
		}
	}
	if (projection && projection[0]) {
		cmd_ad.Assign("Projection", projection);
	}
	if (limit >= 0) {
		cmd_ad.Assign("Limit", limit);
	}
	if (entity_id) {
		cmd_ad.Assign("EntityId", entity_id);
	}
	return _placement_list_command(addr, PLACEMENT_QUERY, cmd_ad, "_placement_query");
}

static PyObject *
_placement_edit(PyObject *, PyObject * args) {
	// _placement_edit(addr, operations, dry_run, on_behalf_of, comment)

	const char * addr = nullptr;
	PyObject * py_ops = nullptr;
	int dry_run = false;
	const char * on_behalf_of = nullptr;
	const char * comment = nullptr;
	if (! PyArg_ParseTuple(args, "sOpzz", & addr, & py_ops, & dry_run, & on_behalf_of, & comment)) {
		// PyArg_ParseTuple() has already set an exception for us.
		return nullptr;
	}
	if (! PyList_Check(py_ops)) {
		PyErr_SetString( PyExc_TypeError, "operations must be a list of ClassAds");
		return nullptr;
	}

	ClassAd header_ad;
	header_ad.Assign(ATTR_MY_TYPE, "PlacementEdit");
	header_ad.Assign("NumOperations", (long long)PyList_Size(py_ops));
	header_ad.Assign("DryRun", (bool)dry_run);
	if (on_behalf_of) {
		header_ad.Assign("OnBehalfOf", on_behalf_of);
	}
	if (comment) {
		header_ad.Assign("Comment", comment);
	}

	std::vector<const ClassAd *> request = {& header_ad};
	for (Py_ssize_t i = 0; i < PyList_Size(py_ops); ++i) {
		PyObject * py_classad = PyList_GetItem(py_ops, i);
		int check = py_is_classad2_classad(py_classad);
		if (check == -1) {
			return nullptr;
		}
		if (check == 0) {
			PyErr_SetString( PyExc_TypeError, "operations must be a list of ClassAds");
			return nullptr;
		}
		auto * handle = get_handle_from(py_classad);
		request.push_back((const ClassAd *)handle->t);
	}

	Sock * sock = _placement_send(addr, PLACEMENT_EDIT, request);
	if (! sock) {
		return nullptr;
	}
	return _placement_read_results(sock, "_placement_edit");
}
