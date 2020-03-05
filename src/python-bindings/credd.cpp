
// Note - python_bindings_common.h must be included before condor_common to avoid
// re-definition warnings.
#include "python_bindings_common.h"

# if defined(__APPLE__)
# undef HAVE_SSIZE_T
# include <pyport.h>
# endif

#include "condor_common.h"
#include <boost/python/overloads.hpp>

//#include "enum_utils.h"
#include "condor_attributes.h"
#include "dc_credd.h"
//#include "globus_utils.h"
//#include "classad/source.h"

#include "old_boost.h"
#include "module_lock.h"
#include "classad_wrapper.h"
#include "dc_credd.h"
#include "store_cred.h"
#include "condor_config.h"
#include "my_username.h"
#include "my_popen.h"


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

	CredCheck(const std::string & _url, const char * _errstr)
		: m_url(_url)
		, m_err(_errstr ? _errstr : "")
	{
	}

	std::string toString() const
	{
		std::string str(m_url);
		if (!m_err.empty()) {
			str += "\nERROR: ";
			str += m_err;
		}
		return str;
	}

private:
	std::string m_url;
	std::string m_err;
};


/*
 * Object representing a connection to a condor_credd.
 * it can also be used to send Credd commands to a condor_master or condor_schedd
 *
 */
struct Credd
{

	Credd() {}

	Credd(boost::python::object ad_obj)
	{
		ClassAdWrapper ad = boost::python::extract<ClassAdWrapper>(ad_obj);
		if (!ad.EvaluateAttrString(ATTR_MY_ADDRESS, m_addr))
		{
			THROW_EX(ValueError, "No contact string in ClassAd");
		}
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

	const char * cook_username_arg(const std::string user_in, std::string & fullusername)
	{
		if (user_in.empty()) {
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
			THROW_EX(ValueError, "password may not be empty");
		}

		const char * user = cook_username_arg(user_in, fullusername);
		if (! user) {
			THROW_EX(ValueError, "invalid user argument");
		}

		daemon = cook_daemon_arg(mode);
		result = do_store_cred(user, password.c_str(), mode, daemon);
		delete daemon; daemon = NULL;

		if (store_cred_failed(result, mode, &errstr)) {
			THROW_EX(RuntimeError, errstr);
		}
	}

	void delete_password(const std::string user_in)
	{
		int result = FAILURE;
		const char * errstr = NULL;
		Daemon * daemon = NULL;
		std::string fullusername;

		int mode = STORE_CRED_LEGACY_PWD | GENERIC_DELETE;

		const char * user = cook_username_arg(user_in, fullusername);
		if (! user) {
			THROW_EX(ValueError, "invalid user argument");
		}

		daemon = cook_daemon_arg(mode);
		result = do_store_cred(user, NULL, mode, daemon);
		delete daemon; daemon = NULL;

		if (result == FAILURE_NOT_FOUND) {
			// no credential stored
			return;
		} else if (store_cred_failed(result, mode, &errstr)) {
			THROW_EX(RuntimeError, errstr);
		}
	}

	bool query_password(const std::string user_in)
	{
		int result = FAILURE;
		const char * errstr = NULL;
		Daemon * daemon = NULL;
		std::string fullusername;

		int mode = STORE_CRED_LEGACY_PWD | GENERIC_QUERY;

		const char * user = cook_username_arg(user_in, fullusername);
		if (! user) {
			THROW_EX(ValueError, "invalid user argument");
		}

		daemon = cook_daemon_arg(mode);
		result = do_store_cred(user, NULL, mode, daemon);
		delete daemon; daemon = NULL;

		if (result == FAILURE_NOT_FOUND) {
			// no credential stored
			return false;
		} else if (store_cred_failed(result, mode, &errstr)) {
			THROW_EX(RuntimeError, errstr);
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
			THROW_EX(ValueError, "invalid credtype");
			break;
		}

		// if no credential provided, check to see if there is a credential producer
		if (py_credential.ptr() == Py_None) {
			auto_free_ptr producer(param("SEC_CREDENTIAL_PRODUCER"));
			if (producer) {
				if (strcasecmp(producer,"CREDENTIAL_ALREADY_STORED") == MATCH) {
					THROW_EX(RuntimeError, producer.ptr());
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
					THROW_EX(RuntimeError, "could not run credential producer");
				}
				bool exited = pgm.wait_for_exit(timeout, &exit_status);
				pgm.close_program(1); // close fp, wait 1 sec, then SIGKILL (does nothing if program already exited)
				if ( ! exited) {
					//exit_status = pgm.error_code();
					THROW_EX(RuntimeError, "credential producer did not exit")
				}
				if (exit_status) {
					THROW_EX(RuntimeError, "credential producer non-zero exit code");
				}

				cred.set(pgm.output().Detach());
				credlen = pgm.output_size();
				if ( ! cred || ! credlen) {
					THROW_EX(RuntimeError, "credential producer did not produce a credential");
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
				THROW_EX(RuntimeError, "credendial not in a form usable by Credd binding");
			}
			const void * buf = NULL;
			Py_ssize_t buflen = 0;
			if (PyObject_AsReadBuffer(py_credential.ptr(), &buf, &buflen) < 0) {
				THROW_EX(RuntimeError, "credendial not in usable format for python bindings");
			}
			if (buflen > 0) {
				cred.set((char*)malloc(buflen));
				memcpy(cred.ptr(), buf, buflen);
				credlen = buflen;
			}
		#endif
		}

		if (!cred.ptr() || ! credlen) {
			THROW_EX(ValueError, "credential may not be empty");
		}

		const char * user = cook_username_arg(user_in, fullusername);
		if (! user) {
			THROW_EX(ValueError, "invalid user argument");
		}

		daemon = cook_daemon_arg(mode);
		result = do_store_cred(user, mode, (unsigned char*)cred.ptr(), credlen, return_ad, NULL, daemon);
		delete daemon; daemon = NULL;

		// overwrite the credential memory with zeros
		if (cred.ptr()) {
			memset(cred.ptr(), 0, credlen);
		}

		if (store_cred_failed(result, mode, &errstr)) {
			THROW_EX(RuntimeError, errstr);
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
			THROW_EX(ValueError, "invalid credtype");
			break;
		}

		const char * user = cook_username_arg(user_in, fullusername);
		if (! user) {
			THROW_EX(ValueError, "invalid user argument");
		}

		daemon = cook_daemon_arg(mode);
		result = do_store_cred(user, mode, NULL, 0, return_ad, NULL, daemon);
		delete daemon; daemon = NULL;

		if (store_cred_failed(result, mode, &errstr)) {
			THROW_EX(RuntimeError, errstr);
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
			THROW_EX(ValueError, "invalid credtype");
			break;
		}

		const char * user = cook_username_arg(user_in, fullusername);
		if (! user) {
			THROW_EX(ValueError, "invalid user argument");
		}

		daemon = cook_daemon_arg(mode);
		result = do_store_cred(user, mode, NULL, 0, return_ad, NULL, daemon);
		delete daemon; daemon = NULL;

		if (store_cred_failed(result, mode, &errstr)) {
			THROW_EX(RuntimeError, errstr);
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
			THROW_EX(ValueError, "invalid credtype");
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
					THROW_EX(RuntimeError, "could not run credential producer");
				}
				bool exited = pgm.wait_for_exit(timeout, &exit_status);
				pgm.close_program(1); // close fp, wait 1 sec, then SIGKILL (does nothing if program already exited)
				if ( ! exited) {
					//exit_status = pgm.error_code();
					THROW_EX(RuntimeError, "credential producer did not exit")
				}
				if (exit_status) {
					THROW_EX(RuntimeError, "credential producer non-zero exit code");
				}

				cred.set(pgm.output().Detach());
				credlen = pgm.output_size();
				if ( ! cred || ! credlen) {
					THROW_EX(RuntimeError, "credential producer did not produce a credential");
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
				THROW_EX(RuntimeError, "credendial not in a form usable by Credd binding");
			}
			const void * buf = NULL;
			Py_ssize_t buflen = 0;
			if (PyObject_AsReadBuffer(py_credential.ptr(), &buf, &buflen) < 0) {
				THROW_EX(RuntimeError, "credendial not in usable format for python bindings");
			}
			if (buflen > 0) {
				cred.set((char*)malloc(buflen));
				memcpy(cred.ptr(), buf, buflen);
				credlen = buflen;
			}
		#endif
		}

		if (!cred.ptr() || ! credlen) {
			THROW_EX(ValueError, "credential may not be empty");
		}

		if (!cook_service_arg(service_ad, service, handle) || service_ad.size() == 0) {
			THROW_EX(ValueError, "invalid service arg");
		}

		const char * user = cook_username_arg(user_in, fullusername);
		if (! user) {
			THROW_EX(ValueError, "invalid user argument");
		}

		daemon = cook_daemon_arg(mode);
		result = do_store_cred(user, mode, (unsigned char*)cred.ptr(), credlen, return_ad, &service_ad, daemon);
		delete daemon; daemon = NULL;

		if (store_cred_failed(result, mode, &errstr)) {
			THROW_EX(RuntimeError, errstr);
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
			THROW_EX(ValueError, "invalid credtype");
			break;
		}

		if (!cook_service_arg(service_ad, service, handle) || service_ad.size() == 0) {
			THROW_EX(ValueError, "invalid service arg");
		}

		const char * user = cook_username_arg(user_in, fullusername);
		if (! user) {
			THROW_EX(ValueError, "invalid user argument");
		}

		daemon = cook_daemon_arg(mode);
		result = do_store_cred(user, mode, NULL, 0, return_ad, &service_ad, daemon);
		delete daemon; daemon = NULL;

		if (store_cred_failed(result, mode, &errstr)) {
			THROW_EX(RuntimeError, errstr);
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
			THROW_EX(ValueError, "invalid credtype");
			break;
		}

		if (!cook_service_arg(service_ad, service, handle)) {
			THROW_EX(ValueError, "invalid service arg");
		}

		const char * user = cook_username_arg(user_in, fullusername);
		if (! user) {
			THROW_EX(ValueError, "invalid user argument");
		}

		daemon = cook_daemon_arg(mode);
		result = do_store_cred(user, mode, NULL, 0, return_ad, &service_ad, daemon);
		delete daemon; daemon = NULL;

		if (store_cred_failed(result, mode, &errstr)) {
			THROW_EX(RuntimeError, errstr);
		}

		boost::shared_ptr<CredStatus> status(new CredStatus(return_ad));
		return status;
	}

	boost::shared_ptr<CredCheck>
	check_service_creds(int credtype, boost::python::object /*services*/, const std::string user_in)
	{
		//long long result = FAILURE;
		const char * errstr = NULL;
		Daemon * daemon = NULL;
		std::string fullusername;
		std::string url("not implemented");

		int mode = GENERIC_CONFIG;
		switch (credtype) {
		case STORE_CRED_USER_OAUTH:
			mode |= credtype;
			break;
		default:
			THROW_EX(ValueError, "invalid credtype");
			break;
		}

		// TODO: cook services arg

		const char * user = cook_username_arg(user_in, fullusername);
		if (! user) {
			THROW_EX(ValueError, "invalid user argument");
		}

		daemon = cook_daemon_arg(mode);
		// TODO: send CREDD_CHECK_CREDS command
		// result = do_store_cred(user, mode, NULL, 0, return_ad, &service_ad, daemon);
		delete daemon; daemon = NULL;

		boost::shared_ptr<CredCheck> check(new CredCheck(url, errstr));
		return check;
	}

private:

    std::string m_addr;
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
            The types of credentials that can be managed by a ``condor_credd``.

            The values of the enumeration are:

            .. attribute:: Password
            .. attribute:: Graceful
            .. attribute:: Quick
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
            A class for sending Credential commands to a Credd, Schedd or Master
            )C0ND0R",
		boost::python::init<boost::python::object>(
			R"C0ND0R(
            :param ad: A ClassAd describing the Credd, Schedd or Master location.
                If omitted, the local schedd is assumed
            :type ad: :class:`~classad.ClassAd`
            )C0ND0R",
			(boost::python::arg("self"), boost::python::arg("ad") = boost::python::object())))
		.def(boost::python::init<>(boost::python::args("self")))

		.def("add_password", &Credd::add_password,
			R"C0ND0R(
            Store password in the Credd for the current user, or for the given user.

            :param password: the password
            :type password: str
            :param user: Optional username, defaults to the current user
            :type user: str
            :return: None
            )C0ND0R",
				(SELFARG
				boost::python::arg("password"),
				boost::python::arg("user") = ""
				)
			)

		.def("delete_password", &Credd::delete_password,
			R"C0ND0R(
            Delete password in the Credd for the current user, or for the given user.

            :param user: Optional username, defaults to the current user
            :type user: str
            :return: None
            )C0ND0R",
			(SELFARG
				boost::python::arg("user") = ""
				)
			)

		.def("query_password", &Credd::query_password,
			R"C0ND0R(
            Check to see if the current user, or the given user has a password stored in the Credd.

            :param user: Optional username, defaults to the current user
            :type user: str
            :return: Bool
            )C0ND0R",
			(SELFARG
				boost::python::arg("user") = ""
				)
			)

		// --------------------  per-user creds (kerberos) ------

		.def("add_user_cred", &Credd::add_cred,
			R"C0ND0R(
            Store a credential in the Credd for the current user, or for the given user.

            :param credtype: the type of credential
            :type credtype: :class:`CredTypes`
            :param credential: the credential
            :type credential: binary data
            :param user: Optional username, defaults to the current user
            :type user: str
            :return: None
            )C0ND0R",
				(SELFARG
				boost::python::arg("credtype"),
				boost::python::arg("credential"),
				boost::python::arg("user")=""
				)
			)

		.def("delete_user_cred", &Credd::delete_cred,
			R"C0ND0R(
            Query wiether the current user, or for the given user has a credential of the given type stored.

            :param credtype: the type of credential
            :type credtype: :class:`CredTypes`
            :param user: Optional username, defaults to the current user
            :type user: str
            :return: None
            )C0ND0R",
			(SELFARG
				boost::python::arg("credtype"),
				boost::python::arg("user")=""
				)
		)

		.def("query_user_cred", &Credd::query_cred,
			R"C0ND0R(
            Query wiether the current user, or for the given user has a credential of the given type stored.

            :param credtype: the type of credential
            :type credtype: :class:`CredTypes`
            :param user: Optional username, defaults to the current user
            :type user: str
            :return: The time that the user credential was last updated, or None if there is no credential
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

            :param credtype: the type of credential
            :type credtype: :class:`CredTypes`
            :param credential: the credential
            :type credential: binary data
            :param service: service name
            :type service: str
            :param handle: optional service handle, defaults to None
            :type handle: str
            :param user: Optional username, defaults to the current user
            :type user: str
            :return: None
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
            Query wiether the current user, or for the given user has a credential of the given type stored.

            :param credtype: the type of credential
            :type credtype: :class:`CredTypes`
            :param service: service name
            :type service: str
            :param handle: optional service handle, defaults to None
            :type handle: str
            :param user: Optional username, defaults to the current user
            :type user: str
            :return: None
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
            Query wiether the current user, or for the given user has a credential of the given type stored.

            :param credtype: the type of credential
            :type credtype: :class:`CredTypes`
            :param service: optional service name, defaults to None
            :type service: str
            :param handle: optional service handle, defaults to None
            :type handle: str
            :param user: Optional username, defaults to the current user
            :type user: str
            :return: CredStatus
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
            Check to see if the current user, or the given user has a given set of service credentials, and if
            any creds a missing, create a temporary URL that can be used to acquire the missing service credentials.

            :param credtype: the type of credential
            :type credtype: :class:`CredTypes`
            :param services: list of services that are needed
            :type services: list
            :param user: Optional username, defaults to the current user
            :type user: str
            :return: CredCheck
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
		;

	boost::python::class_<CredStatus>("CredStatus", boost::python::no_init)
		.def("__str__", &CredStatus::toString)
		;

	boost::python::register_ptr_to_python< boost::shared_ptr<CredCheck>>();
	boost::python::register_ptr_to_python< boost::shared_ptr<CredStatus>>();

}

