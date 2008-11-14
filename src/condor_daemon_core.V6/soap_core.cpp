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
#include "condor_timer_manager.h"
#include "condor_daemon_core.h"
#include "condor_config.h"
#include "reli_sock.h"
#include "condor_io.h"
#include "condor_debug.h"
#include "condor_socket_types.h"
#include "subsystem_info.h"
#include "httpget.h"
#include "directory.h"
#include "stdsoap2.h"
#include "soap_core.h"

extern int soap_serve(struct soap *);

extern DaemonCore *daemonCore;

struct soap ssl_soap;

#define OR(p,q) ((p) ? (p) : (q))

extern SOAP_NMAC struct Namespace namespaces[];

void
init_soap(struct soap *soap)
{
	MyString subsys = MyString(mySubSystem->getName() );

		// KEEP-ALIVE should be turned OFF, not ON.
	soap_init(soap);
	soap_set_namespaces(soap, namespaces);
		//soap_init2(&soap, SOAP_IO_KEEPALIVE, SOAP_IO_KEEPALIVE);

	soap->send_timeout = 20;
	soap->recv_timeout = 20;

		// Register a plugin to handle HTTP GET messages.
		// See httpget.c.
		// Note: valgrind complains about a memory leak inside
		// here, but Matt and Todd say it's not a problem because
		// this is only called at initialization.  wenger 2006-06-22.
	soap_register_plugin_arg(soap,http_get,(void*)http_get_handler);
		// soap.accept_flags = SO_NOSIGPIPE;
		// soap.accept_flags = MSG_NOSIGNAL;

#ifdef COMPILE_SOAP_SSL
	bool enable_soap_ssl = param_boolean("ENABLE_SOAP_SSL", false);
	bool subsys_enable_soap_ssl =
		param_boolean((subsys + "_ENABLE_SOAP_SSL").GetCStr(), false);
	if (subsys_enable_soap_ssl ||
		(enable_soap_ssl &&
		 (!(NULL != param((subsys + "_ENABLE_SOAP_SSL").GetCStr())) ||
		  subsys_enable_soap_ssl))) {
		int ssl_port =
			param_integer((subsys + "_SOAP_SSL_PORT").GetCStr(), 0);

 		if (ssl_port >= 0) {
			dprintf(D_FULLDEBUG,
					"Setting up SOAP SSL socket on port (0 = dynamic): %d\n",
					ssl_port);

			char *server_keyfile;
			if (!(server_keyfile =
				  param((subsys + "_SOAP_SSL_SERVER_KEYFILE").GetCStr())) &&
				!(server_keyfile = param("SOAP_SSL_SERVER_KEYFILE"))) {
				EXCEPT("DaemonCore: Must define [SUBSYS_]SOAP_SSL_SERVER_KEYFILE "
					   "with [SUBSYS_]ENABLE_SOAP_SSL");
			}

				/* gSOAP doesn't use the server_keyfile if no password
				   is provided. If the keyfile is not encrypted with a
				   password then any password will work, even
				   "96hoursofmattslife"! Wonder how long it took me to
				   figure this bug out? The symptom/error I was
				   getting, was "no shared cipher" from SSL. -Matt 3/3/5
				 */
			bool freePassword = true;
			char *server_keyfile_password;
			if (!(server_keyfile_password =
				  param((subsys + "_SOAP_SSL_SERVER_KEYFILE_PASSWORD").GetCStr())) &&
				!(server_keyfile_password =
				  param("SOAP_SSL_SERVER_KEYFILE_PASSWORD")))
			if (NULL == server_keyfile_password) {
				server_keyfile_password = "96hoursofmattslife";
				freePassword = false;
			}

			char *ca_file;
			if (!(ca_file = param((subsys + "_SOAP_SSL_CA_FILE").GetCStr()))) {
				ca_file = param("SOAP_SSL_CA_FILE");
			}

			char *ca_path;
			if (!(ca_path = param((subsys + "_SOAP_SSL_CA_DIR").GetCStr()))) {
				ca_path = param("SOAP_SSL_CA_DIR");
			}

			if (NULL == ca_file &&
				NULL == ca_path) {
				EXCEPT("DaemonCore: Must specify [SUBSYS_]SOAP_SSL_CA_FILE "
					   "or [SUBSYS_]SOAP_SSL_CA_DIR with "
					   "[SUBSYS_]ENABLE_SOAP_SSL");
			}

			char *dh_file;
			if (!(dh_file = param((subsys + "_SOAP_SSL_DH_FILE").GetCStr()))) {
				dh_file = param("SOAP_SSL_DH_FILE");
			}

			dprintf(D_FULLDEBUG,
					"SOAP SSL CONFIG: PORT %d; "
					"SERVER_KEYFILE=%s; "
					"CA_FILE=%s\n"
					"CA_DIR=%s\n",
					ssl_port,
					server_keyfile,
					(ca_file ? ca_file : "(NULL)"),
					(ca_path ? ca_path : "(NULL)"));

			soap_init(&ssl_soap);
			soap_set_namespaces(&ssl_soap, namespaces);

			soap_register_plugin_arg(&ssl_soap,http_get,(void*)http_get_handler);

			ssl_soap.send_timeout = 20;
			ssl_soap.recv_timeout = 20;
			ssl_soap.accept_timeout = 200;	// years of careful reasearch!
				//ssl_soap.accept_flags = SO_NOSIGPIPE;
				//ssl_soap.accept_flags = MSG_NOSIGNAL;

			if (soap_ssl_server_context(&ssl_soap,
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
				soap_print_fault(&ssl_soap, stderr);
				EXCEPT("DaemonCore: Failed to initialize SOAP SSL "
					   "server context");
			}

				// NOTE: If clients use cached sessions gSOAP seems to fail
			SSL_CTX_set_session_cache_mode(ssl_soap.ctx, SSL_SESS_CACHE_OFF);

			int sock_fd;
			if (0 > (sock_fd = soap_bind(&ssl_soap, my_ip_string(),
										 ssl_port,
										 100))) {
				const char **details = soap_faultdetail(&ssl_soap);
				dprintf(D_ALWAYS,
						"Failed to setup SOAP SSL socket on port %d: %s\n",
						ssl_port,
						(details && *details) ? *details : "No details.");
			}

			struct sockaddr_in sockaddr;
			SOCKET_LENGTH_TYPE namelen = sizeof(struct sockaddr_in);
			if (getsockname(sock_fd,
							(struct sockaddr *) &sockaddr,
							&namelen)) {
				dprintf(D_ALWAYS, "Failed to get name of SOAP SSL socket\n");
			} else if (sizeof(struct sockaddr_in) == namelen) {
				dprintf(D_ALWAYS,
						"Setup SOAP SSL on port %d\n",
						ntohs(sockaddr.sin_port));
			} else {
				dprintf(D_ALWAYS,
						"Failed to get name of SOAP SSL socket, "
						"unknown sockaddr returned\n");
			}

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
			if (server_keyfile_password && freePassword) {
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
					"DaemonCore: [SUBSYS_]ENABLE_SOAP_SSL set, "
					"but no valid <SUBSYS>_SOAP_SSL_PORT.\n");

		}
	}
#endif // COMPILE_SOAP_SSL
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
#ifdef COMPILE_SOAP_SSL
    struct soap *current_soap;

	ssl_soap.accept_timeout = 10; // 10 seconds alloted for accept()
	if (-1 == soap_accept(&ssl_soap)) {
		const char **details = soap_faultdetail(&ssl_soap);
		dprintf(D_ALWAYS,
				"DaemonCore: Failed to accept SOAP connection: %s\n",
				(details && *details) ? *details : "No details.");

		return KEEP_STREAM;
	}

	struct sockaddr_in sockaddr;
	memset(&sockaddr, 0, sizeof(struct sockaddr_in));
	sockaddr.sin_addr.s_addr = htonl(ssl_soap.ip);
	sockaddr.sin_port = htons(ssl_soap.port);

	current_soap = soap_copy(&ssl_soap);
	ASSERT(current_soap);

	if (current_soap->recv_timeout > 0) {
		stream->timeout(ssl_soap.recv_timeout);
	} else {
		stream->timeout(20);
	}
	((Sock*)stream)->set_os_buffers(SOAP_BUFLEN, false); // set read buf size
	((Sock*)stream)->set_os_buffers(SOAP_BUFLEN, true); // set write buf size

	if (soap_ssl_accept(current_soap)) {
		const char **details = soap_faultdetail(current_soap);
		dprintf(D_ALWAYS,
				"SOAP SSL connection attempt from %s failed: %s\n",
				sin_to_string(&sockaddr),
				(details && *details) ? *details : "No details.");

		soap_done(current_soap);
		free(current_soap);

		return KEEP_STREAM;
	}

	dprintf(D_ALWAYS,
			"SOAP SSL connection attempt from %s succeeded\n",
			sin_to_string(&sockaddr));

	if (X509_V_OK != SSL_get_verify_result(current_soap->ssl)) {
		dprintf(D_ALWAYS,
				"SOAP SSL connection attempt from %s failed "
				"because the client's certificate was not "
				"verified.\n",
				sin_to_string(&sockaddr));

		soap_done(current_soap);
		free(current_soap);

		return KEEP_STREAM;
	}

	X509 *peer_cert;
	X509_NAME *peer_subject;
	peer_cert = SSL_get_peer_certificate(current_soap->ssl);
	if (NULL == peer_cert) {
		dprintf(D_ALWAYS,
				"SOAP SSL connection attempt from %s failed "
				"because the client did not send a certificate.\n",
				sin_to_string(&sockaddr));

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
				sin_to_string(&sockaddr));

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
				sin_to_string(&sockaddr));

		soap_done(current_soap);
		free(current_soap);

		if (peer_cert) {
			X509_free(peer_cert); peer_cert = NULL;
		}

		return KEEP_STREAM;
	}
	dprintf(D_FULLDEBUG,
			"SOAP SSL connection from %s, X509 subject: %s\n",
			sin_to_string(&sockaddr),
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
			canonical_user.GetCStr());

	if (!daemonCore->Verify("SOAP SSL",SOAP_PERM, &sockaddr, canonical_user.GetCStr())) {

		soap_done(current_soap);
		free(current_soap);

		return KEEP_STREAM;
	}

	current_soap->user = soap_strdup(current_soap, canonical_user.GetCStr());
	soap_serve(current_soap);	// process RPC request
	soap_destroy(current_soap);	// clean up class instances
	soap_end(current_soap);	// clean up everything and close socket
	soap_done(current_soap);

	if (current_soap) {
		free(current_soap);
		current_soap = NULL;
	}

	dprintf(D_FULLDEBUG, "SOAP SSL connection completed\n");

	return KEEP_STREAM;
#else // No COMPILE_SOAP_SSL
	ASSERT("SOAP SSL SUPPORT NOT AVAILABLE");

	return -1;
#endif
}


////////////////////////////////////////////////////////////////////////////////
//  Web server support
////////////////////////////////////////////////////////////////////////////////

int check_authentication(struct soap *soap)
{ if (!soap->userid
   || !soap->passwd
   // || strcmp(soap->userid, AUTH_USERID)
   // || strcmp(soap->passwd, AUTH_PASSWD))
   )
    return 401; /* HTTP not authorized error */
  return SOAP_OK;
}


/******************************************************************************\
 *
 *	Copy static page
 *
\******************************************************************************/

int http_copy_file(struct soap *soap, const char *name, const char *type)
{ FILE *fd;
  size_t r;

  char bbb[64000];

  char * web_root_dir = param("WEB_ROOT_DIR");
  if (!web_root_dir) {
	    return 404; /* HTTP not found */
  }
  char * full_name = dircat(web_root_dir,name);
  fd = safe_fopen_wrapper(full_name, "rb"); /* open file to copy */
  delete [] full_name;
  free(web_root_dir);
  if (!fd) {
    return SOAP_EOF; /* return HTTP error? */
  }
  soap->http_content = type;
  if (soap_begin_send(soap)
   || soap_response(soap, SOAP_FILE)) /* OK HTTP response header */
    return soap->error;

  for (;;)
  //{ r = fread(soap->tmpbuf, 1, sizeof(soap->tmpbuf), fd);
  { r = fread(bbb, 1, sizeof(bbb), fd);
    if (!r)
      break;
    //if (soap_send_raw(soap, soap->tmpbuf, r))
	if (soap_send_raw(soap, bbb, r))
    { if (soap_end_send(soap)) { /* flush failed, already returning error */ }
      fclose(fd);
      return soap->error;
    }
  }
  fclose(fd);
  if (soap_end_send(soap))
    return soap->error;
  return SOAP_OK;
}


/******************************************************************************\
 *
 *	HTTP GET handler for plugin
 *
\******************************************************************************/

int http_get_handler(struct soap *soap)
{ /* HTTP response choices: */
  soap_omode(soap, SOAP_IO_STORE);  /* you have to buffer entire content when returning HTML pages to determine content length */
  //soap_set_omode(soap, SOAP_IO_CHUNK); /* ... or use chunked HTTP content (faster) */
#if 0
  if (soap->zlib_out == SOAP_ZLIB_GZIP) /* client accepts gzip */
    soap_set_omode(soap, SOAP_ENC_ZLIB); /* so we can compress content (gzip) */
  soap->z_level = 9; /* best compression */
#endif
  /* Use soap->path (from request URL) to determine request: */
  dprintf(D_ALWAYS, "HTTP Request: %s\n", soap->endpoint);
  /* Note: soap->path starts with '/' */
  if (strchr(soap->path + 1, '/') || strchr(soap->path + 1, '\\'))	/* we don't like snooping in dirs */
    return 403; /* HTTP forbidden */
  if (!soap_tag_cmp(soap->path, "*.html"))
    return http_copy_file(soap, soap->path + 1, "text/html");
  if (!soap_tag_cmp(soap->path, "*.xml")
   || !soap_tag_cmp(soap->path, "*.xsd")
   || !soap_tag_cmp(soap->path, "*.wsdl"))
    return http_copy_file(soap, soap->path + 1, "text/xml");
  if (!soap_tag_cmp(soap->path, "*.jpg"))
    return http_copy_file(soap, soap->path + 1, "image/jpeg");
  if (!soap_tag_cmp(soap->path, "*.gif"))
    return http_copy_file(soap, soap->path + 1, "image/gif");
  if (!soap_tag_cmp(soap->path, "*.ico"))
    return http_copy_file(soap, soap->path + 1, "image/ico");
  // if (!strncmp(soap->path, "/calc?", 6))
  //  return calc(soap);
  /* Check requestor's authentication: */
  //if (check_authentication(soap))
  //  return 401;
  /* Return Web server status */
  //if (!strcmp(soap->path, "/"))
  //   return info(soap);
  return 404; /* HTTP not found */
}
