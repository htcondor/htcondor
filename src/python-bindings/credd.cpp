
// Note - python_bindings_common.h must be included before condor_common to avoid
// re-definition warnings.
#include "python_bindings_common.h"

# if defined(__APPLE__)
# undef HAVE_SSIZE_T
# include <pyport.h>
# endif

#include "condor_common.h"
#include <boost/python/overloads.hpp>

#include "condor_attributes.h"

#include "htcondor.h"

#include "condor_classad.h"
#include "daemon.h"
#include "old_boost.h"
#include "module_lock.h"
#include "daemon_location.h"
#include "classad_wrapper.h"
#include "store_cred.h"
#include "condor_config.h"
#include "my_username.h"
#include "my_popen.h"
#include "condor_url.h"

struct CredStatus {

	CredStatus(ClassAd & _ad)
		: m_ad(_ad)
	{
	}

	std::string toString() const
	{
		std::string str;
		sPrintAd(str, m_ad);
		return str;
	}

private:
	ClassAd m_ad;
};

struct CredCheck {

	CredCheck(const std::string & _srv, const std::string & _url)
		: m_srv(_srv)
		, m_url(_url)
	{
	}

	std::string toString() const
	{
		std::string str(m_url.empty() ? m_srv : m_url);
		return str;
	}

	bool toBool() const
	{
		bool success = m_url.empty();
		return success;
	}

	boost::python::object get_present() const
	{
		return boost::python::object(toBool());
	}

	boost::python::object get_srv() const
	{
		return boost::python::str(m_srv.c_str());
	}

	boost::python::object get_url() const
	{
		if (IsUrl(m_url.c_str())) {
			return boost::python::str(m_url.c_str());
		}
		return boost::python::object();
	}

	boost::python::object get_err() const
	{
		if (!m_url.empty() && !IsUrl(m_url.c_str())) {
			return boost::python::str(m_url.c_str());
		}
		return boost::python::object();
	}

private:
	std::string m_srv;
	std::string m_url;
};


/*
 * Object representing a connection to a condor_credd.
 * it can also be used to send Credd commands to a condor_master or condor_schedd
 *
 */
struct Credd
{

	Credd() {}

	Credd(boost::python::object loc)
	{
		int rv = construct_for_location(loc, DT_CREDD, m_addr, m_version);
		if (rv < 0) {
			if (rv == -2) { boost::python::throw_error_already_set(); }
			THROW_EX(HTCondorValueError, "Unknown type");
		}
	}

	boost::python::object location() const {
		return make_daemon_location(DT_CREDD, m_addr, m_version);
	}

	// this is the same as the credd's store_cred_failed function,
	// except we change the generic error message to 'communication error'
	bool store_cred_failed(long long ret, int mode, const char ** errstring)
	{
		if (::store_cred_failed(ret,mode,errstring)) {
			if (ret == FAILURE) *errstring = "Communication error";
			return true;
		}
		return false;
	}

	const char * cook_username_arg(const std::string user_in, std::string & fullusername, int mode)
	{
		// in legacy mode, we can't put an empty password on the wire
		// so we lookup the current account name
		bool legacy_mode = mode & STORE_CRED_LEGACY;
		if (user_in.empty()) {
			 if ( ! legacy_mode) {
				fullusername = "";
				return fullusername.c_str();
			}
			auto_free_ptr uname(my_username());
			auto_free_ptr dname(my_domainname());
			if (! dname) {
				dname.set(param("UID_DOMAIN"));
				if (! dname) {
					dname.set(strdup(""));
				}
			}
			fullusername.reserve(strlen(uname) + strlen(dname) + 2);
			fullusername = uname.ptr();
			fullusername += "@";
			fullusername += dname.ptr();
		} else {
			fullusername = user_in;
			// TODO: append UID_DOMAIN here? if username is not fully qualified?
		}

		// name@domain had better be at least 2 characters long
		if (fullusername.size() < 2) {
			return NULL;
		}

		return  fullusername.c_str();
	}

	Daemon * cook_daemon_arg(int mode)
	{
		if (!m_addr.empty()) {
			return new Daemon(DT_CREDD, m_addr.c_str());
		} else if (!(mode & STORE_CRED_LEGACY)) {
			// TODO: perhaps distinguish between PWD, KRB, and OAUTH when deciding which daemon to talk to?
			return new Daemon(DT_CREDD);
		}
		return NULL;
	}

	void add_password(const std::string password, const std::string user_in)
	{
		int result = FAILURE;
		const char * errstr = NULL;
		Daemon * daemon = NULL;
		std::string fullusername;

		int mode = STORE_CRED_LEGACY_PWD | GENERIC_ADD;

		if (password.empty()) {
			THROW_EX(HTCondorValueError, "password may not be empty");
		}

		const char * user = cook_username_arg(user_in, fullusername, mode);
		if (! user) {
			THROW_EX(HTCondorValueError, "invalid user argument");
		}

		daemon = cook_daemon_arg(mode);
		result = do_store_cred(user, password.c_str(), mode, daemon);
		delete daemon; daemon = NULL;

		if (store_cred_failed(result, mode, &errstr)) {
			THROW_EX(HTCondorIOError, errstr);
		}
	}

	void delete_password(const std::string user_in)
	{
		int result = FAILURE;
		const char * errstr = NULL;
		Daemon * daemon = NULL;
		std::string fullusername;

		int mode = STORE_CRED_LEGACY_PWD | GENERIC_DELETE;

		const char * user = cook_username_arg(user_in, fullusername, mode);
		if (! user) {
			THROW_EX(HTCondorValueError, "invalid user argument");
		}

		daemon = cook_daemon_arg(mode);
		result = do_store_cred(user, NULL, mode, daemon);
		delete daemon; daemon = NULL;

		if (result == FAILURE_NOT_FOUND) {
			// no credential stored
			return;
		} else if (store_cred_failed(result, mode, &errstr)) {
			THROW_EX(HTCondorIOError, errstr);
		}
	}

	bool query_password(const std::string user_in)
	{
		int result = FAILURE;
		const char * errstr = NULL;
		Daemon * daemon = NULL;
		std::string fullusername;

		int mode = STORE_CRED_LEGACY_PWD | GENERIC_QUERY;

		const char * user = cook_username_arg(user_in, fullusername, mode);
		if (! user) {
			THROW_EX(HTCondorValueError, "invalid user argument");
		}

		daemon = cook_daemon_arg(mode);
		result = do_store_cred(user, NULL, mode, daemon);
		delete daemon; daemon = NULL;

		if (result == FAILURE_NOT_FOUND) {
			// no credential stored
			return false;
		} else if (store_cred_failed(result, mode, &errstr)) {
			THROW_EX(HTCondorIOError, errstr);
		}
		return (result == SUCCESS);
	}

	void add_cred(int credtype, boost::python::object py_credential, const std::string user_in)
	{
		long long result = FAILURE;
		const char * errstr = NULL;
		Daemon * daemon = NULL;
		ClassAd return_ad;
		std::string fullusername;
		auto_free_ptr cred;
		int credlen = 0;

		int mode = GENERIC_ADD;

		switch (credtype) {
		case STORE_CRED_USER_PWD:
			mode |= credtype;
			break;
		case STORE_CRED_USER_KRB:
			// TODO: make waiting an option?
			mode |= credtype | STORE_CRED_WAIT_FOR_CREDMON;
			break;
		default:
			THROW_EX(HTCondorEnumError, "invalid credtype");
			break;
		}

		// if no credential provided, check to see if there is a credential producer
		if (py_credential.ptr() == Py_None) {
			auto_free_ptr producer(param("SEC_CREDENTIAL_PRODUCER"));
			if (producer) {
				if (strcasecmp(producer,"CREDENTIAL_ALREADY_STORED") == MATCH) {
					THROW_EX(HTCondorIOError, producer.ptr());
				}

				// run the credential producer
				ArgList args;
				args.AppendArg(producer.ptr());
				time_t timeout = 20;
				int exit_status = 0;
				const bool want_stderr = false;
				const bool drop_privs = false;
				MyPopenTimer pgm;
				if (pgm.start_program(args, want_stderr, NULL, drop_privs) < 0) {
					THROW_EX(HTCondorIOError, "could not run credential producer");
				}
				bool exited = pgm.wait_for_exit(timeout, &exit_status);
				pgm.close_program(1); // close fp, wait 1 sec, then SIGKILL (does nothing if program already exited)
				if ( ! exited) {
					//exit_status = pgm.error_code();
					THROW_EX(HTCondorIOError, "credential producer did not exit")
				}
				if (exit_status) {
					THROW_EX(HTCondorIOError, "credential producer non-zero exit code");
				}

				cred.set(pgm.output().Detach());
				credlen = pgm.output_size();
				if ( ! cred || ! credlen) {
					THROW_EX(HTCondorIOError, "credential producer did not produce a credential");
				}
			}
			
		} else {
			// credential is not none. so it must be a binary blob
		#if 0 // PY_MAJOR_VERSION >= 3
			boost::python::stl_input_iterator<unsigned char> begin(py_credential), end;
			std::vector<unsigned char> cred_in(begin, end);
			// copy from the temprary vector into the cred buffer, then zero out the temporary vector
			// TODO: figure out how to copy directy from py_credential into the cred buffer
			if (cred_in.size() > 0) {
				cred.set((char*)malloc(cred_in.size()));
				memcpy(cred.ptr(), &cred_in[0], cred_in.size());
				memset(&cred_in[0], 0, cred_in.size());
			}
		#elif 0
			// this trick came from stackoverflow "How do I pass a pre-populated unsigned char* buffer to a C++ method using boost.python?"
			boost::python::object locals(boost::python::borrowed(PyEval_GetLocals()));
			boost::python::object py_iter = locals["__builtins__"].attr("iter");
			boost::python::stl_input_iterator<unsigned char> begin(py_iter(py_credential)), end;
			std::vector<char> cred_in(begin, end);
			// copy from the temprary vector into the cred buffer, then zero out the temporary vector
			// TODO: figure out how to copy directy from py_credential into the cred buffer
			if (cred_in.size() > 0) {
				cred.set((char*)malloc(cred_in.size()));
				memcpy(cred.ptr(), &cred_in[0], cred_in.size());
				memset(&cred_in[0], 0, cred_in.size());
			}
		#else
			if (!PyObject_CheckReadBuffer(py_credential.ptr())) {
				THROW_EX(HTCondorValueError, "credendial not in a form usable by Credd binding");
			}
			const void * buf = NULL;
			Py_ssize_t buflen = 0;
			if (PyObject_AsReadBuffer(py_credential.ptr(), &buf, &buflen) < 0) {
				THROW_EX(HTCondorValueError, "credendial not in usable format for python bindings");
			}
			if (buflen > 0) {
				cred.set((char*)malloc(buflen));
				memcpy(cred.ptr(), buf, buflen);
				credlen = buflen;
			}
		#endif
		}

		if (!cred.ptr() || ! credlen) {
			THROW_EX(HTCondorValueError, "credential may not be empty");
		}

		const char * user = cook_username_arg(user_in, fullusername, mode);
		if (! user) {
			THROW_EX(HTCondorValueError, "invalid user argument");
		}

		daemon = cook_daemon_arg(mode);
		result = do_store_cred(user, mode, (unsigned char*)cred.ptr(), credlen, return_ad, NULL, daemon);
		delete daemon; daemon = NULL;

		// overwrite the credential memory with zeros
		if (cred.ptr()) {
			memset(cred.ptr(), 0, credlen);
		}

		if (store_cred_failed(result, mode, &errstr)) {
			THROW_EX(HTCondorIOError, errstr);
		}
	}

	void delete_cred(int credtype, const std::string user_in)
	{
		long long result = FAILURE;
		const char * errstr = NULL;
		Daemon * daemon = NULL;
		ClassAd return_ad;
		std::string fullusername;

		int mode = GENERIC_DELETE;

		switch (credtype) {
		case STORE_CRED_USER_PWD:
		case STORE_CRED_USER_KRB:
		case STORE_CRED_USER_OAUTH:
			mode |= credtype;
			break;
		default:
			THROW_EX(HTCondorEnumError, "invalid credtype");
			break;
		}

		const char * user = cook_username_arg(user_in, fullusername, mode);
		if (! user) {
			THROW_EX(HTCondorValueError, "invalid user argument");
		}

		daemon = cook_daemon_arg(mode);
		result = do_store_cred(user, mode, NULL, 0, return_ad, NULL, daemon);
		delete daemon; daemon = NULL;

		if (store_cred_failed(result, mode, &errstr)) {
			THROW_EX(HTCondorIOError, errstr);
		}
	}

	time_t query_cred(int credtype, const std::string user_in)
	{
		long long result = FAILURE;
		const char * errstr = NULL;
		Daemon * daemon = NULL;
		ClassAd return_ad;
		std::string fullusername;

		int mode = GENERIC_QUERY;

		switch (credtype) {
		case STORE_CRED_USER_PWD:
			mode |= credtype;
			break;
		case STORE_CRED_USER_KRB:
		case STORE_CRED_USER_OAUTH:
			mode |= credtype | STORE_CRED_WAIT_FOR_CREDMON;
			break;
		default:
			THROW_EX(HTCondorEnumError, "invalid credtype");
			break;
		}

		const char * user = cook_username_arg(user_in, fullusername, mode);
		if (! user) {
			THROW_EX(HTCondorValueError, "invalid user argument");
		}

		daemon = cook_daemon_arg(mode);
		result = do_store_cred(user, mode, NULL, 0, return_ad, NULL, daemon);
		delete daemon; daemon = NULL;

		if (store_cred_failed(result, mode, &errstr)) {
			THROW_EX(HTCondorIOError, errstr);
		}

		return result;
	}

	bool cook_service_arg(ClassAd & ad, const std::string & service, const std::string & handle)
	{
		if (! service.empty()) {
			// TODO: clean/validate service names here?
			ad.Assign("service", service);
			if (! handle.empty()) {
				ad.Assign("handle", handle);
			}
		} else if (! handle.empty()) {
			// cannot provide a handle without a service
			return false; 
		}
		// it's ok for service and handle to both be empty
		// or for just service to be provided, or both service and handle
		return true;
	}

	void add_service_cred(int credtype, boost::python::object py_credential, const std::string service, const std::string handle, const std::string user_in)
	{
		long long result = FAILURE;
		const char * errstr = NULL;
		Daemon * daemon = NULL;
		ClassAd return_ad;
		ClassAd service_ad;
		std::string fullusername;
		auto_free_ptr cred;
		int credlen = 0;

		int mode = GENERIC_ADD;
		switch (credtype) {
		case STORE_CRED_USER_OAUTH:
			mode |= credtype;
			break;
		default:
			THROW_EX(HTCondorEnumError, "invalid credtype");
			break;
		}

		// if no credential provided, check to see if there is a credential producer
		if (py_credential.ptr() == Py_None) {
			std::string knob("SEC_CREDENTIAL_PRODUCER_OAUTH_"); knob += service;
			auto_free_ptr producer(param(knob.c_str()));
			if (producer) {
				// run the credential producer
				ArgList args;
				args.AppendArg(producer.ptr());
				time_t timeout = 20;
				int exit_status = 0;
				const bool want_stderr = false;
				const bool drop_privs = false;
				MyPopenTimer pgm;
				if (pgm.start_program(args, want_stderr, NULL, drop_privs) < 0) {
					THROW_EX(HTCondorIOError, "could not run credential producer");
				}
				bool exited = pgm.wait_for_exit(timeout, &exit_status);
				pgm.close_program(1); // close fp, wait 1 sec, then SIGKILL (does nothing if program already exited)
				if ( ! exited) {
					//exit_status = pgm.error_code();
					THROW_EX(HTCondorIOError, "credential producer did not exit")
				}
				if (exit_status) {
					THROW_EX(HTCondorIOError, "credential producer non-zero exit code");
				}

				cred.set(pgm.output().Detach());
				credlen = pgm.output_size();
				if ( ! cred || ! credlen) {
					THROW_EX(HTCondorIOError, "credential producer did not produce a credential");
				}
			}
		} else {
			// we can treat cred as bytes
		#if 0 /// PY_MAJOR_VERSION >= 3
			boost::python::stl_input_iterator<unsigned char> begin(py_credential), end;
			std::vector<unsigned char> cred_in(begin, end);
			// copy from the temprary vector into the cred buffer, then zero out the temporary vector
			// TODO: figure out how to copy directy from py_credential into the cred buffer
			if (cred_in.size() > 0) {
				cred.set((char*)malloc(cred_in.size()));
				memcpy(cred.ptr(), &cred_in[0], cred_in.size());
				memset(&cred_in[0], 0, cred_in.size());
			}
		#elif 0
			// this trick came from stackoverflow "How do I pass a pre-populated unsigned char* buffer to a C++ method using boost.python?"
			boost::python::object locals(boost::python::borrowed(PyEval_GetLocals()));
			boost::python::object py_iter = locals["__builtins__"].attr("iter");
			boost::python::stl_input_iterator<unsigned char> begin(py_iter(py_credential)), end;
			std::vector<char> cred_in(begin, end);
			// copy from the temprary vector into the cred buffer, then zero out the temporary vector
			// TODO: figure out how to copy directy from py_credential into the cred buffer
			if (cred_in.size() > 0) {
				cred.set((char*)malloc(cred_in.size()));
				memcpy(cred.ptr(), &cred_in[0], cred_in.size());
				memset(&cred_in[0], 0, cred_in.size());
			}
		#else
			if (!PyObject_CheckReadBuffer(py_credential.ptr())) {
				THROW_EX(HTCondorValueError, "credendial not in a form usable by Credd binding");
			}
			const void * buf = NULL;
			Py_ssize_t buflen = 0;
			if (PyObject_AsReadBuffer(py_credential.ptr(), &buf, &buflen) < 0) {
				THROW_EX(HTCondorValueError, "credendial not in usable format for python bindings");
			}
			if (buflen > 0) {
				cred.set((char*)malloc(buflen));
				memcpy(cred.ptr(), buf, buflen);
				credlen = buflen;
			}
		#endif
		}

		if (!cred.ptr() || ! credlen) {
			THROW_EX(HTCondorValueError, "credential may not be empty");
		}

		if (!cook_service_arg(service_ad, service, handle) || service_ad.size() == 0) {
			THROW_EX(HTCondorValueError, "invalid service arg");
		}

		const char * user = cook_username_arg(user_in, fullusername, mode);
		if (! user) {
			THROW_EX(HTCondorValueError, "invalid user argument");
		}

		daemon = cook_daemon_arg(mode);
		result = do_store_cred(user, mode, (unsigned char*)cred.ptr(), credlen, return_ad, &service_ad, daemon);
		delete daemon; daemon = NULL;

		if (store_cred_failed(result, mode, &errstr)) {
			THROW_EX(HTCondorIOError, errstr);
		}

	}

	void delete_service_cred(int credtype, const std::string service, const std::string handle, const std::string user_in)
	{
		long long result = FAILURE;
		const char * errstr = NULL;
		Daemon * daemon = NULL;
		ClassAd return_ad;
		ClassAd service_ad;
		std::string fullusername;

		int mode = GENERIC_DELETE;
		switch (credtype) {
		case STORE_CRED_USER_OAUTH:
			mode |= credtype;
			break;
		default:
			THROW_EX(HTCondorEnumError, "invalid credtype");
			break;
		}

		if (!cook_service_arg(service_ad, service, handle) || service_ad.size() == 0) {
			THROW_EX(HTCondorValueError, "invalid service arg");
		}

		const char * user = cook_username_arg(user_in, fullusername, mode);
		if (! user) {
			THROW_EX(HTCondorValueError, "invalid user argument");
		}

		daemon = cook_daemon_arg(mode);
		result = do_store_cred(user, mode, NULL, 0, return_ad, &service_ad, daemon);
		delete daemon; daemon = NULL;

		if (store_cred_failed(result, mode, &errstr)) {
			THROW_EX(HTCondorIOError, errstr);
		}
	}

	boost::shared_ptr<CredStatus>
	query_service_cred(int credtype, const std::string service, const std::string handle, const std::string user_in)
	{
		long long result = FAILURE;
		const char * errstr = NULL;
		Daemon * daemon = NULL;
		ClassAd return_ad;
		ClassAd service_ad;
		std::string fullusername;

		int mode = GENERIC_QUERY;
		switch (credtype) {
		case STORE_CRED_USER_OAUTH:
			mode |= credtype;
			break;
		default:
			THROW_EX(HTCondorEnumError, "invalid credtype");
			break;
		}

		if (!cook_service_arg(service_ad, service, handle)) {
			THROW_EX(HTCondorValueError, "invalid service arg");
		}

		const char * user = cook_username_arg(user_in, fullusername, mode);
		if (! user) {
			THROW_EX(HTCondorValueError, "invalid user argument");
		}

		daemon = cook_daemon_arg(mode);
		result = do_store_cred(user, mode, NULL, 0, return_ad, &service_ad, daemon);
		delete daemon; daemon = NULL;

		if (store_cred_failed(result, mode, &errstr)) {
			THROW_EX(HTCondorIOError, errstr);
		}

		boost::shared_ptr<CredStatus> status(new CredStatus(return_ad));
		return status;
	}

	boost::shared_ptr<CredCheck>
	check_service_creds(int credtype, boost::python::list services, const std::string user_in)
	{
		Daemon * daemon = NULL;
		std::string fullusername;
		std::string url;

		int mode = GENERIC_CONFIG;
		switch (credtype) {
		case STORE_CRED_USER_OAUTH:
			mode |= credtype;
			break;
		default:
			THROW_EX(HTCondorEnumError, "invalid credtype");
			break;
		}

		std::string service_name, handle;
		classad::References unique_names;

		// construct a temp vector of pointers for the call
		// we don't need to copy the request ads here or give ownership to the vector
		std::vector<const classad::ClassAd*> requests;
		int num_req = py_len(services);
		requests.reserve(num_req);
		for (int idx = 0; idx < num_req; idx++)
		{
			boost::python::extract<ClassAdWrapper&> wrap(services[idx]);
			if (wrap.check()) {
				const classad::ClassAd & ad(wrap());

				// build the effective service name and add it to the unique_names collection
				// this also does minimal validation of the incoming request
				service_name.clear();
				if ( ! ad.LookupString("Service", service_name)) {
					THROW_EX(HTCondorValueError, "request has no 'Service' attribute");
				}
				if (ad.LookupString("Handle", handle) && ! handle.empty()) {
					service_name += "*";
					service_name += handle;
				}
				unique_names.insert(service_name);

				// add the request to the vector
				requests.push_back(&ad);
			} else {
				THROW_EX(HTCondorValueError, "service must be of type classad.ClassAd");
			}
		}

		const char * user = cook_username_arg(user_in, fullusername, mode);
		if (! user) {
			THROW_EX(HTCondorValueError, "invalid user argument");
		}

		daemon = cook_daemon_arg(mode);
		int rv = do_check_oauth_creds(&requests[0], (int)requests.size(), url, daemon);
		delete daemon; daemon = NULL;
		if (rv < 0) {
			const char * errstr = "internal error";
			switch (rv) {
			case -1: errstr = "invalid services argument"; break;
			case -2: errstr = "could not locate CredD"; break;
			case -3: errstr = "startCommand failed"; break;
			case -4: errstr = "communication failure"; break;
			}
			THROW_EX(HTCondorIOError, errstr);
		}

		std::string name_list;
		for (auto name = unique_names.begin(); name != unique_names.end(); ++name) {
			if (!name_list.empty()) name_list += ",";
			name_list += *name;
		}

		boost::shared_ptr<CredCheck> check(new CredCheck(name_list, url));
		return check;
	}

private:

    std::string m_addr;
    std::string m_version;
};

enum CredTypes {
	CredPassword=STORE_CRED_USER_PWD,
	CredKerberos=STORE_CRED_USER_KRB,
	CredOAuth=STORE_CRED_USER_OAUTH,
};


void
export_credd()
{
	boost::python::enum_<CredTypes>("CredTypes",
		R"C0ND0R(
            The types of credentials that can be managed by a *condor_credd*.

            The values of the enumeration are:

            .. attribute:: Password
            .. attribute:: Kerberos
            .. attribute:: OAuth
            )C0ND0R")
		.value("Password", CredPassword)
		.value("Kerberos", CredKerberos)
		.value("OAuth", CredOAuth)
		;

#if BOOST_VERSION >= 103400
    #define SELFARG boost::python::arg("self"),
#else
    #define SELFARG
#endif

	boost::python::class_<Credd>("Credd",
		R"C0ND0R(
            A class for sending Credential commands to a Credd, Schedd or Master.
            )C0ND0R",
		boost::python::init<boost::python::object>(
			R"C0ND0R(
            :param location_ad: A ClassAd or DaemonLocation describing the Credd, Schedd or Master location.
                If omitted, the local schedd is assumed.
            :type location_ad: :class:`~classad.ClassAd` or :class:`DaemonLocation`
            )C0ND0R",
			(boost::python::arg("self"), boost::python::arg("ad") = boost::python::object())))
		.def(boost::python::init<>(boost::python::args("self")))
        .add_property("location", &Credd::location,
            R"C0ND0R(
            The Credd to query or send commands to
            :rtype: location :class:`DaemonLocation`
            )C0ND0R")

		.def("add_password", &Credd::add_password,
			R"C0ND0R(
            Store the ``password`` in the Credd for the current user (or for the given ``user``).

            :param password: The password.
            :type password: str
            :param user: Which user to store the credential for (defaults to the current user).
            :type user: str
            )C0ND0R",
            (SELFARG
				boost::python::arg("password"),
				boost::python::arg("user") = ""
				)
			)

		.def("delete_password", &Credd::delete_password,
			R"C0ND0R(
            Delete the ``password`` in the Credd for the current user (or for the given ``user``).

            :param user: Which user to store the credential for (defaults to the current user).
            :type user: str
            )C0ND0R",
			(SELFARG
				boost::python::arg("user") = ""
				)
			)

		.def("query_password", &Credd::query_password,
			R"C0ND0R(
            Check to see if the current user (or the given ``user``) has a password stored in the Credd.

            :param user: Which user to store the credential for (defaults to the current user).
            :type user: str
            :return: bool
            )C0ND0R",
			(SELFARG
				boost::python::arg("user") = ""
				)
			)

		// --------------------  per-user creds (kerberos) ------

		.def("add_user_cred", &Credd::add_cred,
			R"C0ND0R(
            Store a ``credential`` in the Credd for the current user (or for the given ``user``).

            :param credtype: The type of credential to store.
            :type credtype: :class:`CredTypes`
            :param credential: The credential to store.
            :type credential: bytes
            :param user: Which user to store the credential for (defaults to the current user).
            :type user: str
            )C0ND0R",
				(SELFARG
				boost::python::arg("credtype"),
				boost::python::arg("credential"),
				boost::python::arg("user")=""
				)
			)

		.def("delete_user_cred", &Credd::delete_cred,
			R"C0ND0R(
            Delete a credential of the given ``credtype`` for the current user (or for the given ``user``).

            :param credtype: The type of credential to delete.
            :type credtype: :class:`CredTypes`
            :param user: Which user to store the credential for (defaults to the current user).
            :type user: str
            )C0ND0R",
			(SELFARG
				boost::python::arg("credtype"),
				boost::python::arg("user")=""
				)
		    )

		.def("query_user_cred", &Credd::query_cred,
			R"C0ND0R(
            Query whether the current user (or the given user) has a credential of the given type stored.

            :param credtype: The type of credential to query for.
            :type credtype: :class:`CredTypes`
            :param user: Which user to store the credential for (defaults to the current user).
            :type user: str
            :return: The time that the user credential was last updated, or ``None`` if there is no credential
            )C0ND0R",
			(SELFARG
				boost::python::arg("credtype"),
				boost::python::arg("user")=""
				)
            )

			// --------------------  oauth creds  ------

        .def("add_user_service_cred", &Credd::add_service_cred,
			R"C0ND0R(
            Store a credential in the Credd for the current user, or for the given user.
            
            To specify multiple credential for the same service (e.g., you want to transfer
            files from two different accounts that are on the same service), 
            give each a unique ``handle``.

            :param credtype: The type of credential to store.
            :type credtype: :class:`CredTypes`
            :param credential: The credential to store.
            :type credential: bytes
            :param service: The service name.
            :type service: str
            :param handle: Optional service handle (defaults to no handle).
            :type handle: str
            :param user: Which user to store the credential for (defaults to the current user).
            :type user: str
            )C0ND0R",
			(SELFARG
				boost::python::arg("credtype"),
				boost::python::arg("credential"),
				boost::python::arg("service"),
				boost::python::arg("handle")="",
				boost::python::arg("user")=""
				)
            )

        .def("delete_user_service_cred", &Credd::delete_service_cred,
			R"C0ND0R(
            Delete a credential of the given ``credtype`` for service ``service`` for the current user (or for the given ``user``).

            :param credtype: The type of credential to delete.
            :type credtype: :class:`CredTypes`
            :param service: The service name.
            :type service: str
            :param handle: Optional service handle (defaults to no handle).
            :type handle: str
            :param user: Which user to store the credential for (defaults to the current user).
            :type user: str
            )C0ND0R",
			(SELFARG
				boost::python::arg("credtype"),
				boost::python::arg("service"),
				boost::python::arg("handle")="",
				boost::python::arg("user")=""
				)
            )

        .def("query_user_service_cred", &Credd::query_service_cred,
			R"C0ND0R(
            Query whether the current user (or the given ``user``) has a credential of the given ``credtype`` stored.

            :param credtype: The type of credential to check storage for.
            :type credtype: :class:`CredTypes`
            :param service: The service name.
            :type service: str
            :param handle: Optional service handle (defaults to no handle).
            :type handle: str
            :param user: Which user to store the credential for (defaults to the current user).
            :type user: str
            :return: :class:`CredStatus`
            )C0ND0R",
			(SELFARG
				boost::python::arg("credtype"),
				boost::python::arg("service"),
				boost::python::arg("handle")="",
				boost::python::arg("user")=""
				)
            )

        .def("check_user_service_creds", &Credd::check_service_creds,
			R"C0ND0R(
            Check to see if the current user (or the given ``user``) has a given set of service credentials, and if
            any credentials are missing, create a temporary URL that can be used to acquire the missing service credentials.

            :param credtype: The type of credentials to check for.
            :type credtype: :class:`CredTypes`
            :param services: The list of services that are needed.
            :type services: List[:class:`classad.ClassAd`]
            :param user: Which user to store the credential for (defaults to the current user).
            :type user: str
            :return: :class:`CredCheck`
            )C0ND0R",
			(SELFARG
                boost::python::arg("credtype"),
                boost::python::arg("services"),
                boost::python::arg("user")=""
			    )
            )

		// end of class Credd
		;

	boost::python::class_<CredCheck>("CredCheck", boost::python::no_init)
		.def("__str__", &CredCheck::toString)
		.def("__bool__", &CredCheck::toBool)
		.def("__nonzero__", &CredCheck::toBool)
		.add_property("present", &CredCheck::get_present,
            R"C0ND0R(
            True if the necessary tokens are present in the CredD, or if there are no necessary tokens
            False if the necessary tokens are not present, If false, either the url member or the error member
            will be non-empty
            :rtype: bool
            )C0ND0R")
		.add_property("url", &CredCheck::get_url,
            R"C0ND0R(
            The URL to visit to acquire the necessary tokens
            :rtype: str
            )C0ND0R")
		.add_property("error", &CredCheck::get_err,
            R"C0ND0R(
            A message from the CredD when the Creds were not present and a URL could not be created to acquire them.
            :rtype: str
            )C0ND0R")
		.add_property("services", &CredCheck::get_srv,
            R"C0ND0R(
            The list of services that were requested as a comma separated list. 
            This will be the same as the job ClassAd attribute `OAuthServicesNeeded`
            :rtype: str
            )C0ND0R")

		;

	boost::python::class_<CredStatus>("CredStatus", boost::python::no_init)
		.def("__str__", &CredStatus::toString)
		;

	boost::python::register_ptr_to_python< boost::shared_ptr<CredCheck>>();
	boost::python::register_ptr_to_python< boost::shared_ptr<CredStatus>>();

}

