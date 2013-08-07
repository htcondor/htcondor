#! /usr/bin/env perl
#
# Copyright 2009-2013 Red Hat, Inc.
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

# Sample script for invoking Aviary submits from perl

# yum install perl-SOAP-Lite
use SOAP::Lite;

my $client = SOAP::Lite     
	-> uri('http://job.aviary.grid.redhat.com')
    # update hostname:port as needed
	-> proxy('http://localhost:39090/services/job/submitJob');

# turn on debugging
SOAP::Lite->import(trace => debug);

# tune submit to LINUX
my @requirements = (
    SOAP::Data->name('type' => 'OS')->type(''),
    SOAP::Data->name('value' => 'LINUX')->type('')
);

# application-specific ClassAd attribute
# could be an override also
my @extra = (
    SOAP::Data->name('name' => 'RECIPE')->type(''),
    SOAP::Data->name('type' => 'STRING')->type(''),
    SOAP::Data->name('value' => 'SECRET_SAUCE')->type(''),
);

# our list of arguments to the submitJob call
my @params = (
    SOAP::Data->name('cmd' => '/bin/sleep')->type(''),
    SOAP::Data->name('args' => '60')->type(''),
    SOAP::Data->name('owner')->value(getlogin || getpwuid($<) || 'condor')->type(''),
    SOAP::Data->name('iwd' => '/tmp')->type(''),
    # dereference the lists using \ so that they nest in the XML correctly
    SOAP::Data->name('requirements' => \@requirements)->type(''),
    SOAP::Data->name('extra' => \@extra)->type('')
);

# apparently SOAP::Lite uses the op as the XML root element
my $result = $client->SubmitJob(@params);

# alternatively you could use this form to override basic attributes 
# such as for more detailed "Requirements"
# my $result = $client->call(SOAP::Data->name('SubmitJob')->attr(
#     {'xmlns'=>"http://job.aviary.grid.redhat.com",
#     'allowOverrides'=>'true'}
#     ) 
#     => @params);

unless ($result->fault) {
    print $result->result(), "\n";
  } else {
    print join ', ', 
      $result->faultcode, 
      $result->faultstring, 
      $result->faultdetail,
      "\n";
  }

