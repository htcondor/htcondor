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
#include "reli_sock.h"
#include "my_hostname.h"

// local includes
#include "EndpointPublisher.h"

using namespace std;
using namespace aviary::locator;

EndpointPublisher::EndpointPublisher(const string& service_name, const string& service_type)
{
	m_name = service_name;
	m_type = service_type;
	m_port = -1;
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
	sprintf(port,":%d/",m_port);
	m_location = scheme + my_full_hostname() + port + uri_suffix;

	return true;
}

void
EndpointPublisher::publish()
{
	ClassAd publish_ad;
	// send our custom classad to location plugin
	// in the collector
	dprintf(D_FULLDEBUG, "EndpointPublisher emitting: '%s'\n", m_location.c_str());
	
	publish_ad.SetMyTypeName(GENERIC_ADTYPE);
//	publish_ad.SetTargetTypeName(COLLECTOR_ADTYPE);
	publish_ad.Assign(ATTR_NAME,m_name);
	publish_ad.Assign(ATTR_MY_ADDRESS, my_ip_string());
	publish_ad.Assign("EndpointUri",m_location);
	daemonCore->sendUpdates(UPDATE_AD_GENERIC, &publish_ad);
}

void
EndpointPublisher::invalidate()
{
	// notify the collector that we are out of here
	ClassAd invalidate_ad;

	invalidate_ad.SetMyTypeName(GENERIC_ADTYPE);
//	invalidate_ad.SetTargetTypeName(COLLECTOR_ADTYPE);
	invalidate_ad.Assign(ATTR_NAME, m_name.c_str());
	daemonCore->sendUpdates(INVALIDATE_ADS_GENERIC, &invalidate_ad);
}

int
EndpointPublisher::getPort()
{
	return m_port;
}

