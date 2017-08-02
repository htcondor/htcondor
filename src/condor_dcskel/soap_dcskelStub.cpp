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
#include "condor_classad.h"
#include "condor_daemon_core.h"

// Things to include for the stubs
#include "condor_version.h"

#include "condor_attributes.h"

#include "soap_dcskelStub.h"
#include "condorDcskel.nsmap"


using namespace soap_dcskel;



namespace condor_soap {

int
soap_serve(struct soap *soap)
{
	return soap_dcskel::soap_serve(soap);
}

}

namespace soap_dcskel {



#include "soap_daemon_core.cpp"
}
