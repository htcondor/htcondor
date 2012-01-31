/*
 * Copyright 2012 Red Hat, Inc.
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

#ifndef _AVIARY_ENDPOINT_PUBLISHER_H
#define _AVIARY_ENDPOINT_PUBLISHER_H

// condor includes
#include "condor_common.h"
#include "reli_sock.h"

// c++ includes
#include <string>

namespace aviary {
namespace locator {

class EndpointPublisher
{
	public:
		EndpointPublisher(const std::string& service_name, const std::string& service_type);
		~EndpointPublisher();
		virtual bool init(const std::string& uri_suffix, bool for_ssl = false);
		virtual void publish();
		virtual void invalidate();
		int getPort();

	protected:
		std::string m_location;
		std::string m_name;
		std::string m_type;
		int m_port;

};

}} /* aviary::locator */

#endif 