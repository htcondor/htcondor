#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Copyright 2009-2012 Red Hat, Inc.
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

# uses Suds - https://fedorahosted.org/suds/
from suds import *
from suds.client import Client
from sys import exit, argv
from optparse import OptionParser
from subprocess import Popen, PIPE
from aviary.util import *
from time import *
import cmd
import logging

# mainly interactive script for querying a HTCondor Collector
# using Aviary SOAP interface

# change these for other default locations and ports
DEFAULTS = {'wsdl':'file:/var/lib/condor/aviary/services/collector/aviary-collector.wsdl',
            'host':'localhost',
            'port':'9000',
            'service':'/services/collector/',
            'verbose': False
            }
plugins = []
logging.basicConfig(level=logging.CRITICAL)

class AviaryClient:

    client = None
    base_url = None

    def __init__(self,wsdl,base_url):
        self.base_url = base_url
        self.client = Client(wsdl)

    def getClient(self,url_suffix):
        if url_suffix:
            url = self.base_url+url_suffix
            self.client.set_options(location=url)
        return self.client

class CollectorCtrl:
    
    aviary = None
    nodetype = None

    def __init__(self,wsdl,url,nodetype):
        self.nodetype = nodetype
        self.aviary = AviaryClient(wsdl,url)

    def execute(self,line):
        result = None
        target_op = "get"+self.nodetype
        list_client = self.aviary.getClient(target_op)
        func = getattr(list_client.service, target_op, None)
        try:
            if callable(func):
                response = func(line.split())
        except Exception, e:
            print e
        if response:
            for r in response:
                print r

class AviaryCollectorTool(cmd.Cmd):
    
    prompt = 'aviary> '
    host = DEFAULTS['host']
    port = DEFAULTS['port']
    verbose = DEFAULTS['verbose']
    base_url = 'http://'+host+':'+port
    bin_file = ''
    import pwd
    
    def set_base_url(self):
        self.base_url = 'http://'+self.host+':'+self.port
    
    def do_host(self, line):
        "view/edit HTTP hostname"
        if line:
            self.host = line
        print 'host is:', self.host
        self.set_base_url()

    def do_port(self, line):
        "view/edit HTTP port"
        if line:
            self.port = line
        print 'port is:', self.port
        self.set_base_url()

    def do_url(self, line):
        "view/edit the base url for connect"
        print 'base url is:', self.base_url+DEFAULTS['service']
        print 'use "host" and "port" to modify HTTP target'

    def do_verbose(self, line):
        "toggle debug diagnostic messages"
        if self.verbose:
            self.verbose = False
            logging.basicConfig(level=logging.CRITICAL)
            logging.getLogger('suds.client').setLevel(logging.CRITICAL)
        else:
            self.verbose = True
            logging.basicConfig(level=logging.INFO)
            logging.getLogger('suds.client').setLevel(logging.DEBUG)
        print 'verbose:', self.verbose

    def execute(self,nodetype,line):
        CollectorCtrl(DEFAULTS['wsdl'],self.base_url+DEFAULTS['service'],nodetype).execute(line)

    def do_collector(self, line):
        "list collector summaries"
        self.execute('Collector',line)

    def do_master(self, line):
        "list master summaries"
        self.execute('Master',line)
        
    def do_negotiator(self, line):
        "list negotiator summaries"
        self.execute('Negotiator',line)
        
    def do_scheduler(self, line):
        "list scheduler summaries"
        self.execute('Scheduler',line)

    def do_slot(self, line):
        "list slot summaries"
        self.execute('Slot',line)

    def do_submitter(self, line):
        "list submitter summaries"
        self.execute('Submitter',line)

    def do_EOF(self, line):
        return True

if __name__ == '__main__':
    if len(argv) > 1:
        AviaryCollectorTool().onecmd(' '.join(argv[1:]))
    else:
        AviaryCollectorTool().cmdloop()
