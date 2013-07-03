/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

#include "condor_common.h"
#include "internet.h"
#include "condor_daemon_core.h"
#include "condor_config.h"
#include "condor_io.h"
#include "condor_debug.h"
#include "directory.h"
#include "stdsoap2.h"
#include "soap_core.h"
#include "condor_open.h"

#include "mimetypes.h"
#include "condor_sockfunc.h"

#ifdef WIN32
#define realpath(P,B) _fullpath((B),(P),_MAX_PATH)
#endif

namespace condor_soap {

extern int soap_serve(struct soap *);

}

extern DaemonCore *daemonCore;

struct soap *ssl_soap;

#define OR(p,q) ((p) ? (p) : (q))

extern SOAP_NMAC struct Namespace namespaces[];

int handle_soap_ssl_socket(Service *, Stream *stream);

int get_handler(struct soap *soap);

struct soap *
dc_soap_accept(Sock *socket, const struct soap *soap)
{
	struct soap *cursoap = soap_copy(const_cast<struct soap*>(soap));
	ASSERT(cursoap);

		// Mimic a gsoap soap_accept as follows:
		//   1. stash the socket descriptor in the soap object
		//   2. make socket non-blocking by setting a CEDAR timeout.
		//   3. increase size of send and receive buffers
		//   4. set SO_KEEPALIVE [done automatically by CEDAR accept()]
	cursoap->socket = socket->get_file_desc();
	cursoap->peer = socket->peer_addr().to_sin();
	cursoap->recvfd = soap->socket;
	cursoap->sendfd = soap->socket;
	if ( cursoap->recv_timeout > 0 ) {
		socket->timeout(soap->recv_timeout);
	} else {
		socket->timeout(20);
	}
	socket->set_os_buffers(SOAP_BUFLEN,false);	// set read buf size
	socket->set_os_buffers(SOAP_BUFLEN,true);	// set write buf size

	return cursoap;
}

int
dc_soap_serve(struct soap *soap)
{
	return condor_soap::soap_serve(soap);
}

void
dc_soap_free(struct soap *soap)
{
	soap_destroy(soap); // clean up class instances
	soap_end(soap); // clean up everything and close socket
	soap_free(soap);
}

void
dc_soap_init(struct soap *&soap)
{
	if (NULL == soap) {
		soap = soap_new();
	}
	ASSERT(soap);

		// KEEP-ALIVE should be turned OFF, not ON.
	//soap_init(soap);
	soap_set_namespaces(soap, namespaces);
		//soap_init2(&soap, SOAP_IO_KEEPALIVE, SOAP_IO_KEEPALIVE);

	soap->send_timeout = 20;
	soap->recv_timeout = 20;

	soap->fget = get_handler;

#ifdef HAVE_EXT_OPENSSL
	bool enable_soap_ssl = param_boolean("ENABLE_SOAP_SSL", false);
	if (enable_soap_ssl) {
		int ssl_port = param_integer("SOAP_SSL_PORT", 0);

 		if (ssl_port >= 0) {
			dprintf(D_FULLDEBUG,
					"Setting up SOAP SSL socket on port (0 = dynamic): %d\n",
					ssl_port);

			char *server_keyfile = param("SOAP_SSL_SERVER_KEYFILE");
			if (!server_keyfile) {
				EXCEPT("DaemonCore: Must define SOAP_SSL_SERVER_KEYFILE "
					   "with ENABLE_SOAP_SSL");
			}

				/* gSOAP doesn't use the server_keyfile if no password
				   is provided. If the keyfile is not encrypted with a
				   password then any password will work, even
				   "96hoursofmattslife"! Wonder how long it took me to
				   figure this bug out? The symptom/error I was
				   getting, was "no shared cipher" from SSL. -Matt 3/3/5
				 */
			char *server_keyfile_password =
				param("SOAP_SSL_SERVER_KEYFILE_PASSWORD");
			if (!server_keyfile_password) {
				server_keyfile_password = strdup("96hoursofmattslife");
			}

			char *ca_file = param("SOAP_SSL_CA_FILE");
			char *ca_path = param("SOAP_SSL_CA_DIR");

			if (NULL == ca_file && NULL == ca_path) {
				EXCEPT("DaemonCore: Must specify SOAP_SSL_CA_FILE "
					   "or SOAP_SSL_CA_DIR with ENABLE_SOAP_SSL");
			}

			char *dh_file = param("SOAP_SSL_DH_FILE");

			dprintf(D_FULLDEBUG,
					"SOAP SSL CONFIG: PORT %d; "
					"SERVER_KEYFILE=%s; "
					"CA_FILE=%s\n"
					"CA_DIR=%s\n",
					ssl_port,
					server_keyfile,
					(ca_file ? ca_file : "(NULL)"),
					(ca_path ? ca_path : "(NULL)"));

			if( !ssl_soap ) {
				ssl_soap = new struct soap();
			}
			soap_init(ssl_soap);
			soap_set_namespaces(ssl_soap, namespaces);

			ssl_soap->fget = get_handler;

			ssl_soap->send_timeout = 20;
			ssl_soap->recv_timeout = 20;
			ssl_soap->accept_timeout = 200;	// years of careful reasearch!
				//ssl_soap->accept_flags = SO_NOSIGPIPE;
				//ssl_soap->accept_flags = MSG_NOSIGNAL;

			if (soap_ssl_server_context(ssl_soap,
										SOAP_SSL_REQUIRE_SERVER_AUTHENTICATION |
										SOAP_SSL_REQUIRE_CLIENT_AUTHENTICATION,
											/* keyfile: required when server
											   must authenticate to clients
											   (see SSL docs on how to obtain
											   this file) */
										server_keyfile,
											/* password to read the key
											   file */
										server_keyfile_password,
											/* optional cacert file to store
											   trusted certificates */
										ca_file,
											/* optional capath to directory
											   with trusted certificates */
										ca_path,
											/* DH file, if NULL use RSA */
										dh_file,
											/* if randfile!=NULL: use a file
											   with random data to seed
											   randomness */
										NULL,
											/* This is not so optional. Axis
											   fails on it's second call if
											   this is NULL.  Everything works
											   fine with ZSI. It seems that
											   Axis tries to use cached
											   sessions.
											   See docs on:
											   SSL_CTX_set_session_id_context
											*/
											/* optional server identification
											   to enable SSL session cache
											   (must be a unique name) */
										"felix")) {
				soap_print_fault(ssl_soap, stderr);
				EXCEPT("DaemonCore: Failed to initialize SOAP SSL "
					   "server context");
			}

				// NOTE: If clients use cached sessions gSOAP seems to fail
			SSL_CTX_set_session_cache_mode((SSL_CTX*)ssl_soap->ctx, SSL_SESS_CACHE_OFF);

			int sock_fd;
			if (0 > (sock_fd = soap_bind(ssl_soap, my_ip_string(),
										 ssl_port,
										 100))) {
				const char **details = soap_faultdetail(ssl_soap);
				dprintf(D_ALWAYS,
						"Failed to setup SOAP SSL socket on port %d: %s\n",
						ssl_port,
						(details && *details) ? *details : "No details.");
			}

			condor_sockaddr sockaddr;
			if (condor_getsockname(sock_fd, sockaddr)) {
				dprintf(D_ALWAYS, "Failed to get name of SOAP SSL socket\n");
			} else {
				dprintf(D_ALWAYS,
						"Setup SOAP SSL on port %d\n",
						sockaddr.get_port());
			} //else {
				//	dprintf(D_ALWAYS,
				//		"Failed to get name of SOAP SSL socket, "
				//		"unknown sockaddr returned\n");
				//}

			ReliSock *sock = new ReliSock;
			if (!sock) {
				EXCEPT("DaemonCore: Failed to allocate SOAP SSL socket");
			}
			if (!sock->assign(sock_fd)) {
				EXCEPT("DaemonCore: Failed to use bound SOAP SSL socket");
			}
			int index;
			if (-1 == (index =
					   daemonCore->Register_Socket((Stream *) sock,
												   "SOAP SSL socket.",
												   (SocketHandler)
												   &handle_soap_ssl_socket,
												   "Handler for the SOAP SSL "
												   "socket."))) {
				EXCEPT("DaemonCore: Failed to register SOAP SSL socket");
			}
				// This is a horrible hack, see condor_daemon_core.h
				// for justification.
			daemonCore->soap_ssl_sock = index;

			if (server_keyfile) {
				free(server_keyfile);
				server_keyfile = NULL;
			}
			if (server_keyfile_password) {
				free(server_keyfile_password);
				server_keyfile_password = NULL;
			}
			if (ca_file) {
				free(ca_file);
				ca_file = NULL;
			}
			if (ca_path) {
				free(ca_path);
				ca_path = NULL;
			}
			if (dh_file) {
				free(dh_file);
				dh_file = NULL;
			}
		} else {
			dprintf(D_ALWAYS,
					"DaemonCore: ENABLE_SOAP_SSL set, "
					"but no valid SOAP_SSL_PORT.\n");

		}
	}
#endif // HAVE_EXT_OPENSSL
}


/*
 * DANGER(?): Since DaemonCore is single threaded it must wait for
 * this function to return before it can handle other requests. This
 * is a danger for by all registered socket handlers. This function
 * does an accept and SSL_accept. The latter can take as much as 10
 * seconds (constants programmed into stdsoap.C's soap_ssl_accept),
 * especially if someone just ran "nc server port" and provides no
 * data. The former's timeout is defined below as 10 seconds as
 * well. This means that a single SYN packet could cause a 20 second
 * delay. NOTE: It is possible that the accept will never timeout if
 * DaemonCore does not call this handler until a full 3-way handshake
 * has been performed.
 */
int
handle_soap_ssl_socket(Service *, Stream *stream)
{
#ifdef HAVE_EXT_OPENSSL
    struct soap *current_soap;

	ASSERT( ssl_soap );
	ssl_soap->accept_timeout = 10; // 10 seconds alloted for accept()
	if (-1 == soap_accept(ssl_soap)) {
		const char **details = soap_faultdetail(ssl_soap);
		dprintf(D_ALWAYS,
				"DaemonCore: Failed to accept SOAP connection: %s\n",
				(details && *details) ? *details : "No details.");

		return KEEP_STREAM;
	}

	struct sockaddr_in sockaddr;
	memset(&sockaddr, 0, sizeof(struct sockaddr_in));
	sockaddr.sin_addr.s_addr = htonl(ssl_soap->ip);
	sockaddr.sin_port = htons(ssl_soap->port);

	condor_sockaddr addr(&sockaddr);

	current_soap = soap_copy(ssl_soap);
	ASSERT(current_soap);

	if (current_soap->recv_timeout > 0) {
		stream->timeout(ssl_soap->recv_timeout);
	} else {
		stream->timeout(20);
	}
	((Sock*)stream)->set_os_buffers(SOAP_BUFLEN, false); // set read buf size
	((Sock*)stream)->set_os_buffers(SOAP_BUFLEN, true); // set write buf size

	if (soap_ssl_accept(current_soap)) {
		const char **details = soap_faultdetail(current_soap);
		dprintf(D_ALWAYS,
				"SOAP SSL connection attempt from %s failed: %s\n",
				addr.to_sinful().Value(),
				(details && *details) ? *details : "No details.");

		soap_done(current_soap);
		free(current_soap);

		return KEEP_STREAM;
	}

	dprintf(D_ALWAYS,
			"SOAP SSL connection attempt from %s succeeded\n",
			addr.to_sinful().Value());

	if (X509_V_OK != SSL_get_verify_result((SSL*)current_soap->ssl)) {
		dprintf(D_ALWAYS,
				"SOAP SSL connection attempt from %s failed "
				"because the client's certificate was not "
				"verified.\n",
				addr.to_sinful().Value());

		soap_done(current_soap);
		free(current_soap);

		return KEEP_STREAM;
	}

	X509 *peer_cert;
	X509_NAME *peer_subject;
	peer_cert = SSL_get_peer_certificate((SSL*)current_soap->ssl);
	if (NULL == peer_cert) {
		dprintf(D_ALWAYS,
				"SOAP SSL connection attempt from %s failed "
				"because the client did not send a certificate.\n",
				addr.to_sinful().Value());

		soap_done(current_soap);
		free(current_soap);

		return KEEP_STREAM;
	}
	peer_subject = X509_get_subject_name(peer_cert);
	if (NULL == peer_subject) {
		dprintf(D_ALWAYS,
				"SOAP SSL connection attempt from %s failed "
				"because the client's certificate's subject was not "
				"found.\n",
				addr.to_sinful().Value());

		soap_done(current_soap);
		free(current_soap);

		if (peer_cert) {
			X509_free(peer_cert); peer_cert = NULL;
		}

		return KEEP_STREAM;
	}
	char subject[1024];
	subject[0] = subject[1023] = '\0';
	X509_NAME_oneline(peer_subject, subject, 1023);
	if ('\0' == subject[0]) {
		dprintf(D_ALWAYS,
				"SOAP SSL connection attempt from %s failed "
				"because the client's certificate's subject was not "
				"oneline-able.\n",
				addr.to_sinful().Value());

		soap_done(current_soap);
		free(current_soap);

		if (peer_cert) {
			X509_free(peer_cert); peer_cert = NULL;
		}

		return KEEP_STREAM;
	}
	dprintf(D_FULLDEBUG,
			"SOAP SSL connection from %s, X509 subject: %s\n",
			addr.to_sinful().Value(),
			subject);

	MyString canonical_user;
	MyString principal = MyString(subject);
	if (peer_cert) {
		X509_free(peer_cert); peer_cert = NULL;
	}
	if (daemonCore->mapfile->GetCanonicalization("SSL",
												 principal,
												 canonical_user)) {
		dprintf(D_FULLDEBUG,
				"SOAP SSL connection rejected, "
				"no mapping in CERTIFICATE_MAPFILE\n");

		soap_done(current_soap);
		free(current_soap);

		return KEEP_STREAM;
	}

	dprintf(D_FULLDEBUG,
			"SOAP SSL connection subject mapped to '%s'\n",
			canonical_user.Value());

	if (!daemonCore->Verify("SOAP SSL",SOAP_PERM, addr, canonical_user.Value())) {

		soap_done(current_soap);
		free(current_soap);

		return KEEP_STREAM;
	}

	current_soap->user = soap_strdup(current_soap, canonical_user.Value());
	condor_soap::soap_serve(current_soap);	// process RPC request
	soap_destroy(current_soap);	// clean up class instances
	soap_end(current_soap);	// clean up everything and close socket
	soap_done(current_soap);

	if (current_soap) {
		free(current_soap);
		current_soap = NULL;
	}

	dprintf(D_FULLDEBUG, "SOAP SSL connection completed\n");

	return KEEP_STREAM;
#else // No HAVE_EXT_OPENSSL
	ASSERT("SOAP SSL SUPPORT NOT AVAILABLE");

	return -1;
#endif
}

#define FILEEXT_BUF_SIZE 64

static char *file_ext(const char *filename) {
  static const char *err;
  static int erroffset;
  static pcre *pat = NULL;

  if (pat == NULL) {
    pat = pcre_compile(".*\\.(\\w+)$", 0, &err, &erroffset, NULL);
  }

  char ext[FILEEXT_BUF_SIZE];
  int offsets[6];
  int result;
  
  memset(ext, 0, sizeof(char) * FILEEXT_BUF_SIZE);
  
  result = pcre_exec(pat, NULL, filename, strlen(filename), 0, 0, offsets, 6);
  
  if (result != 2 || pcre_copy_substring(filename, offsets, 2, 1, ext, FILEEXT_BUF_SIZE) <= 0) {
    return strdup("");
  }
  
  return strdup(ext);
}

int serve_file(struct soap *soap, const char *name, const char *type) { 
  FILE *fstr;
  size_t r;

  char bbb[4096];
  char buf[PATH_MAX];
  
  char * web_root_dir = param("WEB_ROOT_DIR");
  char * web_root_realpath = NULL;

  if (!web_root_dir) {
    return 404;
  } 
  
  if (realpath(web_root_dir, buf)) {
    web_root_realpath = strdup(buf);
  }
  free(web_root_dir);

  if (!web_root_realpath) {
    return 404;
  } 
  
  char * full_name = dircat(web_root_realpath,name);
  char * full_name_realpath = NULL;
  if (realpath(full_name, buf)) {
    full_name_realpath = strdup(buf);
  }

  delete [] full_name;

  if (full_name_realpath == NULL) {
    free(web_root_realpath);
    return 404;
  }

  /* Ensure that the requested resource is contained
     within the web root and that the requested 
     resource is not a directory.

     The calls to realpath (above) resolve any 
     symbolic links and relative pathname components 
     in web_root_realpath and full_name_realpath.

     The strstr call ensures that the canonicalized 
     web root path appears at the beginning of the 
     canonical path for the requested file.
  */
  if (strstr(full_name_realpath, web_root_realpath) != full_name_realpath
      || IsDirectory(full_name_realpath)) {
    /* NB:  it might be nice to support an option 
       to redirect to an index file or a dynamically-
       generated index if the requested resource 
       is a directory */
    free(full_name_realpath);
    free(web_root_realpath);
    return 403;
  }

  fstr = safe_fopen_wrapper_follow(full_name_realpath, "rb");

  free(full_name_realpath);
  free(web_root_realpath);

  if (!fstr) {
    return 404;
  }

  soap->http_content = type;

  if (soap_begin_send(soap) || soap_response(soap, SOAP_FILE)) {
    fclose(fstr);
    return soap->error;
  }

  while ((r = fread(bbb, 1, sizeof(bbb), fstr))) {
    if (soap_send_raw(soap, bbb, r) != SOAP_OK) {
      if (soap_end_send(soap) != SOAP_OK) {
	fclose(fstr);
	return soap->error;
      }
    }
  }

  /* Did we break out of the above loop because 
     of an error reading from fd? */
  if (ferror(fstr)) {
    fclose(fstr);
    soap->error = SOAP_HTTP_ERROR;
    return 500;
  }

  fclose(fstr);

  if (soap_end_send(soap) != SOAP_OK) {
    return soap->error;
  }

  return SOAP_OK;
}   

int get_handler(struct soap *soap) {
  const char *type;
  char *ext;

  soap_omode(soap, SOAP_IO_STORE);

  dprintf(D_ALWAYS, "HTTP Request: %s\n", soap->endpoint);
  
  ext = file_ext(soap->path);
  
  if ((type = type_for_ext(ext)) == NULL) {
    /* NB:  it might be nice to have a more
       sensible default, e.g., sniffing 
       text vs. non-text files */
    type = "application/octet-stream";
  }
  

  free(ext);

  return serve_file(soap, soap->path + 1, type);
}
