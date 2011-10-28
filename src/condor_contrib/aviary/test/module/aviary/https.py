# Copyright 2011 David Norton, Jr.
# Copyright 2011 Joseph Turner
# Copyright 2009-2011 Red Hat, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

import socket
import httplib
import urllib2 as u2
from M2Crypto import httpslib, SSL
from suds.transport.http import HttpTransport

# Provide an exception here that implementation classes
# can use in common to raise exceptions and return messages
class SSLVerificationError(Exception):
    pass

class HTTPSClientAuthHandler(u2.HTTPSHandler):  
    def __init__(self, key, cert):
        """
        @param key: full path for the client's private key file
        @param cert: full path for the client's PEM certificate file
        """
        u2.HTTPSHandler.__init__(self)  
        self.key = key  
        self.cert = cert  

    def https_open(self, req):
        """
        Override https_open() in the HTTPSHandler class.

        The inherited method does not set private key and certificate values on
        the HTTPSConnection object.
        """
        # Rather than pass in a reference to a connection class, we pass in
        # a reference to a function which, for all intents and purposes,
        # will behave as a constructor 
        return self.do_open(self._get_connection, req)

    def _get_connection(self, host, timeout=300):
        """
        @return: an HTTPSConnection object constructed with private key and
        certificate values supplied.
        @rtype: HTTPSConnection
        """
        return httplib.HTTPSConnection(host, key_file=self.key, cert_file=self.cert)  

class HTTPSClientCertTransport(HttpTransport):
    def __init__(self, key, cert, *args, **kwargs):
        """
        @param key: full path for the client's private key file
        @param cert: full path for the client's PEM certificate file
        """
        HttpTransport.__init__(self, *args, **kwargs)
        self.key = key
        self.cert = cert

    def u2open(self, u2request):
        """
        Open an ssl connection with client certificate validation.

        @param u2request: A urllib2 request.
        @type u2request: urllib2.Request.
        @return: The opened file-like urllib2 object.
        @rtype: fp
        """
        tm = self.options.timeout
        url = u2.build_opener(self._get_auth_handler())
        if self.u2ver() < 2.6:
            socket.setdefaulttimeout(tm)
            return url.open(u2request)
        else:
            return url.open(u2request, timeout=tm)

    def _get_auth_handler(self):
        return HTTPSClientAuthHandler(self.key, self.cert)

class HTTPSFullAuthHandler(HTTPSClientAuthHandler):
	"""
	Add server certificate validation to HTTPSClientAuthHandler
	via a different connection type (VerifiedHTTPSConnection).
	"""
	def __init__(self, my_key, my_cert, root_cert, server_verify):
		"""
		@param my_key: full path for the client's private key file
		@param my_cert: full path for the client's PEM certificate file
		@param root_cert: full path for root certificates file used to
		verify server certificates on connection
		@param server_verify: check server host against the 'commonName'
		field in the server certificate
		"""
		HTTPSClientAuthHandler.__init__(self, my_key, my_cert)
		self.root_cert = root_cert
		self.server_verify = server_verify

	def _get_connection(self, host, timeout=300):
		"""
		@return: A connection object derived from httplib types with
		with client and server certificate validation support
		@rtype: VerifiedHTTPSConnection
		"""
		return VerifiedHTTPSConnection(host,
										key_file=self.key,
										cert_file=self.cert,
										root_cert=self.root_cert,
										server_verify=self.server_verify)

class HTTPSFullCertTransport(HTTPSClientCertTransport):
	"""
	Add server certificate validation to HTTPSClientCertTransport
	via a different handler type (HTTPSFullAuthHandler)
	"""
	def __init__(self, key, cert, root_cert, server_verify=True,
					*args, **kwargs):
		"""
		@param key: full path for the client's private key file
		@param cert: full path for the client's PEM certificate file
		@param root_cert: full path for root certificates file used to
		verify server certificates on connection
		@param server_verify: check server host against the 'commonName'
		field in the server certificate
		"""
		HTTPSClientCertTransport.__init__(self, key, cert, *args, **kwargs)
		self.root_cert = root_cert
		self.server_verify = server_verify

	def _get_auth_handler(self):
		return HTTPSFullAuthHandler(self.key, self.cert, self.root_cert,
									self.server_verify)

# wrap the creation of a SSL.Context, etc in a class
class VerifiedHTTPSConnection(httpslib.HTTPSConnection):

    def __init__(self, host, port=None, key_file=None, cert_file=None,
                 root_cert=None, strict=None, timeout=None,
                 server_verify=True):
        """
        All params except those noted below are passed through to
        the M2Crypto.httpslib.HTTPSConnection constructor.  Check docs on that
        class information.  Note, root_cert is passed in the SSL.Context if
        server_verify is True by means of SSL.Context.load_verify_locations.

        @param server_verify: does server certificate verification if True
        @param timeout: timeout value is set on the socket after connection using
                        socket.settimeout() if timeout is not None.
        """
        self.server_verify = server_verify
        self.my_timeout = timeout

        ctx = SSL.Context()
        ctx.load_cert(cert_file, key_file)
        # Leaving the ctx verify mode set to 0 does not seem
        # to turn off all the server certificate checks, not
        # sure why.  Something in M2Crypto.
        if server_verify:
            ctx.load_verify_locations(root_cert)
            mode = SSL.verify_peer | SSL.verify_fail_if_no_peer_cert
            ctx.set_verify(mode, depth=9)
        httpslib.HTTPSConnection.__init__(self, host, port, strict,
                                          key_file=key_file, cert_file=cert_file,
                                          ssl_context=ctx)
    def connect(self):
        try:
            # Best we can do with the timeout parameter is
            # set it on the socket after the connection is
            # created.  There is no hook in M2Crypto to set
            # this prior to the connection.
            httpslib.HTTPSConnection.connect(self)
            if self.my_timeout is not None:
                self.sock.settimeout(self.my_timeout)
        except SSL.Checker.WrongHost, e:
			msg = "Server certificate CN doesn't match domain"
			if self.server_verify:
				raise SSLVerificationError("ERROR: "+msg)
			else:
				print "Warning:",msg
