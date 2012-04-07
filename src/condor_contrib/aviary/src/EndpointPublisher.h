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
#include "condor_classad.h"

// c++ includes
#include <string>
#include <dc_service.h>

namespace aviary {
namespace locator {

const char CUSTOM[] = "Custom";
const char LOCATOR[] = "Locator";
const char ENDPOINT[] = "Endpoint";
const char ENDPOINT_URI[] = "EndpointUri";
const char MAJOR_TYPE[] = "MajorType";
const char MINOR_TYPE[] = "MinorType";

class EndpointPublisher: public Service
{
	public:
		EndpointPublisher(const std::string& service_name, const std::string& major_type, const std::string& minor_type);
		~EndpointPublisher();
		bool init(const std::string& uri_suffix, bool for_ssl = false);
		void start(int);
		void stop();
		void invalidate();
		int getPort();

	protected:
		std::string m_location;
		std::string m_name;
		std::string m_major_type;
		std::string m_minor_type;
		int m_port;
		int m_update_interval;
		int m_update_timer;
		ClassAd m_ad;
		
		void publish();

};

}} /* aviary::locator */

#endif 