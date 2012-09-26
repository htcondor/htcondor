/*
 * Copyright 2009-2012 Red Hat, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// condor includes
#include "condor_common.h"
#include "condor_config.h"
#include "condor_classad.h"
#include "condor_attributes.h"
#include "condor_daemon_core.h"
#include "condor_version.h"
#include "reli_sock.h"
#include "my_hostname.h"

// local includes
#include "EndpointPublisher.h"

using namespace std;
using namespace aviary::locator;

EndpointPublisher::EndpointPublisher(const string& service_name, const string& major_type,
	const string& minor_type)
{
	m_name = service_name;
	m_major_type = major_type;
	m_minor_type = minor_type;
	m_port = -1;
	m_update_interval = 60;
	m_update_timer = -1;
}

EndpointPublisher::~EndpointPublisher()
{
}

bool 
EndpointPublisher::init(const std::string& uri_suffix, bool for_ssl)
{
	dprintf(D_FULLDEBUG,"EndpointPublisher::init\n");
	string scheme;
	string port;

	if (for_ssl) {
		scheme = "https://";
	} else {
		scheme = "http://";
	}

	// grab an ephemeral port
	ReliSock probe_sock;
	if (-1 == probe_sock.bind(true)) {
		dprintf(D_ALWAYS,"EndpointPublisher is unable to obtain ANY ephemeral port from configured range! " \
			"Check configured values of LOWPORT,HIGHPORT.\n");
		return false;
	}
	m_port = probe_sock.get_port();
	formatstr(port,":%d/",m_port);
	m_location = scheme + my_full_hostname() + port + uri_suffix;

	// populate the publish ad
	m_ad = ClassAd();
	m_ad.SetMyTypeName(GENERIC_ADTYPE);
	m_ad.SetTargetTypeName(ENDPOINT);
	m_ad.Assign(ATTR_NAME,m_name);
	m_ad.Assign(ENDPOINT_URI,m_location);
	m_ad.Assign(MAJOR_TYPE,m_major_type);
	if (!m_minor_type.empty()) {
		m_ad.Assign(MINOR_TYPE,m_minor_type);
	}
	daemonCore->publish(&m_ad);

	return true;
}

void 
EndpointPublisher::start(int update_interval)
{
   if (m_update_interval != update_interval)
   {
      m_update_interval = update_interval;

      if (m_update_timer >= 0)
      {
         daemonCore->Cancel_Timer(m_update_timer);
         m_update_timer = -1;
      }

      dprintf(D_FULLDEBUG, "Updating collector every %d seconds\n", m_update_interval);
      m_update_timer = daemonCore->Register_Timer(0, m_update_interval,
                                                        (TimerHandlercpp)&EndpointPublisher::publish,
                                                        "EndpointPublisher::publish",
														this
 												);
   }

   dprintf(D_FULLDEBUG, "EndpointPublisher emitting: '%s'\n", m_location.c_str());
}

void
EndpointPublisher::stop()
{
	invalidate();
	if (0 <= m_update_timer) {
      daemonCore->Cancel_Timer(m_update_timer);
      m_update_timer = -1;
	}
}

void
EndpointPublisher::publish()
{
	// send our custom classad to location plugin
	// in the collector
	daemonCore->sendUpdates(UPDATE_AD_GENERIC, &m_ad);
}

void
EndpointPublisher::invalidate()
{
   ClassAd invalidate_ad;
   string line;

   invalidate_ad.SetMyTypeName(QUERY_ADTYPE);
   invalidate_ad.SetTargetTypeName(ENDPOINT);
   invalidate_ad.Assign(ENDPOINT_URI,m_location.c_str());
   invalidate_ad.Assign(ATTR_NAME,m_name.c_str());
   formatstr(line,"%s == \"%s\"", ATTR_NAME, m_name.c_str()); 
   invalidate_ad.AssignExpr(ATTR_REQUIREMENTS, line.c_str());

	dprintf(D_FULLDEBUG, "EndpointPublisher sending INVALIDATE_ADS_GENERIC: '%s'\n", m_location.c_str());
	// notify the collector that we are out of here
	daemonCore->sendUpdates(INVALIDATE_ADS_GENERIC, &invalidate_ad,NULL,true);
}

int
EndpointPublisher::getPort()
{
	return m_port;
}

