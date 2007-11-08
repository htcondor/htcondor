##**************************************************************
##
## Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
## University of Wisconsin-Madison, WI.
## 
## Licensed under the Apache License, Version 2.0 (the "License"); you
## may not use this file except in compliance with the License.  You may
## obtain a copy of the License at
## 
##    http://www.apache.org/licenses/LICENSE-2.0
## 
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
##**************************************************************

{
    printf"sed -e \"s/$(CONDOR_OPENSSL_INCLUDE)/$(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE)/g\" -e \"s/$(CONDOR_OPENSSL_LIB)/$(CONDOR_OPENSSL_LIB) $(CONDOR_POSTGRESQL_LIB)/g\" -e \"s/$(CONDOR_OPENSSL_LIBPATH)/$(CONDOR_OPENSSL_LIBPATH) $(CONDOR_POSTGRESQL_LIBPATH)/g\" %s > backup\\%s\n",$0,$0 
}
