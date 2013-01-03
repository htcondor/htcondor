#
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

import sys
from optparse import OptionParser
import logging
from suds.client import Client
from aviary.https import *

# utility to build up common options for test scripts
def build_basic_parser(desc, default_url):
	logging.basicConfig(level=logging.CRITICAL)
	key = '/etc/pki/tls/certs/client.key'
	cert = '/etc/pki/tls/certs/client.crt'
	root = '/etc/pki/tls/certs/server.crt'
	parser = OptionParser(description=desc)
	parser.add_option('-v','--verbose', action='store_true', dest='verbose', default=False, help='enable SOAP logging')
	parser.add_option('-u','--url', action='store', dest='url', default=default_url, help='http or https URL')
	parser.add_option('-t','--timeout', action='store', dest='timeout', default=15, help='URL connection timeout <int>')
	parser.add_option('-k','--key', action="store", dest='key', default=key, help='client SSL key file')
	parser.add_option('-c','--cert', action="store", dest='cert', default=cert, help='client SSL certificate file')
	parser.add_option('-r','--root', action="store", dest='root', default=root, help='server SSL certificate file')
	parser.add_option('-s','--verify', action="store_true", dest='verify', default=False, help='enable server certificate verification')
	return parser

def create_suds_client(opts,wsdl,plugin_list):
	try:
		if "https://" in opts.url:
			client = Client(wsdl, plugins=plugin_list, timeout=int(opts.timeout), 
				transport = HTTPSFullCertTransport(opts.key,opts.cert,opts.root,opts.verify,timeout=int(opts.timeout)))
		else:
			client = Client(wsdl, plugins=plugin_list, timeout=int(opts.timeout))
	except ValueError, ve:
		print "option error:",ve
		sys.exit(1)
	# debug info
	if opts.verbose:
		print opts
		logging.basicConfig(level=logging.INFO)
		logging.getLogger('suds.client').setLevel(logging.DEBUG)
		print client
	return client