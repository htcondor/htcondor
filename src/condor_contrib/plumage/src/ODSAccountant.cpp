/***************************************************************
 *
 * Copyright (C) 2009-2011 Red Hat, Inc.
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

// always first
#include "condor_common.h"
#include "compat_classad.h"
#include "condor_io.h"

// local includes
#include "ODSAccountant.h"
#include "condor_debug.h"
#include "stl_string_utils.h"

using namespace std;
using namespace plumage::etl;

// lots of code cribbed from userprio
// currently the negotiator doesn't publish this data in its ad

ODSAccountant::ODSAccountant():m_negotiator(NULL)
{
}

ODSAccountant::~ODSAccountant()
{
    if (m_negotiator) delete m_negotiator;
}

bool 
ODSAccountant::connect() 
{
    m_negotiator = new Daemon(DT_NEGOTIATOR, NULL, NULL);
    if (!m_negotiator || !m_negotiator->locate()) {
      dprintf(D_ALWAYS, "ODSAccountant: Can't connect negotiator for Accountant ad!\n");
      return false;
    }
    return true;
}

AttrList*
ODSAccountant::fetchAd ()
{
    AttrList* ad = NULL;

    Sock* sock = NULL;
    if( !m_negotiator ||
        !(sock = m_negotiator->startCommand(GET_PRIORITY,Stream::reli_sock, 0) ) ||
        !sock->end_of_message()) {
        dprintf( D_ALWAYS, "ODSAccountant: failed to send GET_PRIORITY command to negotiator!\n" );
    }

    if (sock) {
       // get reply
       sock->decode();
       ad=new AttrList();
       if (!getClassAdNoTypes(sock, *ad) || !sock->end_of_message()) {
          dprintf( D_ALWAYS, "ODSAccountant: failed to get classad from negotiator!\n" );
       }

      sock->close();
      delete sock;
    }

    return ad;

}
