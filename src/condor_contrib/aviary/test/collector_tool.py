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
import pwd

# mainly interactive script for querying a HTCondor Collector
# using Aviary SOAP interface

# change these for other default locations and ports
DEFAULTS = {'wsdl':'file:/var/lib/condor/aviary/services/collector/aviary-collector.wsdl',
            'host':'localhost',
            'port':'39000',
            'service':'/services/collector/',
            'verbose': False
            }
logging.basicConfig(level=logging.CRITICAL)
cli_url = None
cli_names = None

# full summaries for everything
# optionally retrieve dynamic slots with partitionable slot summaries
from suds.plugin import MessagePlugin
from suds.sax.attribute import Attribute
class SummaryPlugin(MessagePlugin):
    def marshalled(self, context):
        sj_body = context.envelope.getChild('Body')[0]
        sj_body.attributes.append(Attribute("includeSummaries", "true"))
        sj_body.attributes.append(Attribute("includeDynamic", "true"))
plugin_list = [SummaryPlugin()]

class AviaryClient:

    client = None
    base_url = None

    def __init__(self,wsdl,base_url):
        self.base_url = base_url
        self.client = Client(wsdl,plugins=plugin_list)

    def getClient(self,url_suffix):
        if url_suffix:
            url = self.base_url+url_suffix
            self.client.set_options(location=url)
        return self.client

class CollectorCtrl(cmd.Cmd):
    
    aviary = None
    nodetype = None

    def __init__(self,wsdl,url,nodetype):
        self.nodetype = nodetype
        self.aviary = AviaryClient(wsdl,url)

    def _strip_sinful(self,val):
        stripped = val.replace('<','').replace('>','')
        idx = stripped.rfind(':')
        if idx>1:
            stripped = stripped[:-(len(stripped)-idx)]
        return stripped

    def do_attributes(self,line):
        res_id = None
        response = None
        the_client = None
        op_args = []
        names = []
        if cli_names:
            names = cli_names
        args = line.split()
        the_client = self.aviary.getClient("getAttributes")
        for arg in args:
            res_id = the_client.factory.create("ns1:ResourceID")
            try:
                if "/" not in arg:
                    print "need a 'name/host ip address' from ResourceID to get attributes!"
                    return
                (res_id.name,res_id.address) = arg.split("/")
                res_id.address = self._strip_sinful(res_id.address)
            except (ValueError, AttributeError), e:
                print e
            res_id.resource = the_client.factory.create("ns1:ResourceType")
            res_id.resource = self.nodetype.upper()
            op_args.append({'id':res_id, 'names':names})
        if not res_id:
            print "need at least one ResourceID name/host for attribute lookup"
            return
        try:
            response = the_client.service.getAttributes(op_args)
            for r in response:
                print r.id
                if r.status.code == "OK":
                    for attr in r.ad.attrs:
                        print attr.name,"=",attr.value
                else:
                    print r.status.code,r.status.text
        except Exception, e:
            print e
            return


    def default(self,line):
        response = None
        clean_ids = []
        dirty_ids = line.split()
        for id in dirty_ids:
            if "/" in id:
                clean_ids.append(id.split('/')[0])
            else:
                clean_ids.append(id)
        target_op = "get"+self.nodetype
        the_client = self.aviary.getClient(target_op)
        func = getattr(the_client.service, target_op, None)
        try:
            if callable(func):
                response = func(clean_ids)
            for r in response:
                if r.status.code == "OK":
                    if self.nodetype != "Submitter":
                        print r.id.name+"/"+self._strip_sinful(r.id.address)
                    print str(r)+"\n"
                else:
                    print r.status.code,r.status.text
        except Exception, e:
            print e
            return

    def emptyline(self):
        response = None
        target_op = "get"+self.nodetype
        the_client = self.aviary.getClient(target_op)
        func = getattr(the_client.service, target_op, None)
        try:
            if callable(func):
                response = func(None)
            for r in response:
                if self.nodetype != "Submitter":
                    print r.id.name+"/"+self._strip_sinful(r.id.address)
                else:
                    print r.id.name
        except Exception, e:
            print e
            return

class AviaryCollectorTool(cmd.Cmd):
    
    prompt = 'aviary> '
    host = DEFAULTS['host']
    port = DEFAULTS['port']
    verbose = DEFAULTS['verbose']
    base_url = 'http://'+host+':'+port
    bin_file = ''
    names = []

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

    def do_names(self, line):
        "view/edit attribute names"
        if line:
            self.names = line.split(',')
        cli_names = self.names
        print cli_names

    _AVAILABLE_CMDS = ('attributes')
    def completedefault(self, text, line, begidx, endidx):
        return [i for i in self._AVAILABLE_CMDS if i.startswith(text)]

    def execute(self,nodetype,line):
        CollectorCtrl(DEFAULTS['wsdl'],self.base_url+DEFAULTS['service'],nodetype).onecmd(line)

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

    def names_callback(option, opt, value, parser):
        setattr(parser.values, option.dest, value.split(','))

    if len(argv) > 1:
        parser = build_basic_parser('Retrieve HTCondor collector data remotely via SOAP.','http://localhost:39000/services/collector/')
        parser.add_option('-n','--names', type='string',
                  action='callback', callback=names_callback, help="comma-separated list of attribute names to retrieve")
        (opts,args) =  parser.parse_args()
        tool = AviaryCollectorTool()
        if opts.verbose:
            tool.do_verbose(True)
        if opts.url:
            cli_url = opts.url
        if opts.names:
            cli_names = opts.names
        tool.onecmd(' '.join(args))
    else:
        AviaryCollectorTool().cmdloop()
