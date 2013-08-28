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

#include "AviaryProvider.h"
#include "EndpointPublisher.h"
#include "Axis2SoapProvider.h"
#include "Axis2SslProvider.h"
#include "AviaryUtils.h"

using namespace std;
using namespace aviary;
using namespace aviary::transport;
using namespace aviary::soap;
using namespace aviary::util;
using namespace aviary::locator;

AviaryProvider* 
AviaryProviderFactory::create(const string& log_file, 
							  const string& service_name, const string& major_type,
							  const string& minor_type,
							  const string& uri_suffix)
{
    AviaryProvider* provider = NULL;
    string repo_path;
    int port;
    string axis_error;
    char *tmp = NULL;
    EndpointPublisher* ep = NULL;
    static int count = 0;

    // config then env for our all-important axis2 repo dir
    if ((tmp = param("WSFCPP_HOME"))) {
        repo_path = tmp;
        free(tmp);
    }
    else if ((tmp = getenv("WSFCPP_HOME"))) {
        repo_path = tmp;
    }
    else {
        dprintf(D_ALWAYS,"No WSFCPP_HOME in config or env\n");
        return NULL;
    }
    
    int level = getLogLevel();
    int read_timeout = param_integer("AXIS2_READ_TIMEOUT",AXIS2_HTTP_DEFAULT_SO_TIMEOUT);
    
    // which flavor of transport
    bool have_ssl = param_boolean("AVIARY_SSL",FALSE);
	if (!have_ssl) {
		port = param_integer("HTTP_PORT",39000);
	}
	else {
		port = param_integer("HTTP_PORT",39443);
	}

	// see if we are using locator to publish our endpoint
	bool use_locator = param_boolean("AVIARY_PUBLISH_LOCATION",FALSE) && minor_type != LOCATOR;
	if (use_locator) {
		ep = new EndpointPublisher(service_name, major_type, minor_type);
		if (!ep->init(uri_suffix,have_ssl)) {
			dprintf(D_ALWAYS,"Aviary location endpoint config failed\n");
			return NULL;
		}
		port = ep->getPort();
	}

    if (!have_ssl) {
        Axis2SoapProvider* http = new Axis2SoapProvider(level,log_file.c_str(),repo_path.c_str());
        if (!http->init(port,read_timeout,axis_error)) {
            dprintf(D_ALWAYS,"Axis2 HTTP configuration failed, check possible conflict on port %d\n",port);
            delete http;
            return NULL;
        }
        dprintf(D_ALWAYS,"UNSECURE Axis2 HTTP listener activated on port %d\n",port);
        provider = http;
    }
#ifdef HAVE_EXT_OPENSSL
    else {
        Axis2SslProvider* https = new Axis2SslProvider(level,log_file.c_str(),repo_path.c_str());
        if (!https->init(port,read_timeout,axis_error)) {
            dprintf(D_ALWAYS,"SSL/TLS requested but configuration failed\n");
            dprintf(D_ALWAYS,"Check SSL config paths and possible conflict on port %d\n",port);
            delete https;
            return NULL;
        }
        dprintf(D_ALWAYS,"Axis2 HTTPS listener activated on port %d\n",port);
        provider = https;
    }
#endif

    // ready to publish our endpoint
    if (ep) {
        // provider owns this now
        provider->setPublisher(ep);
        ep->start(param_integer("AVIARY_PUBLISH_INTERVAL", 10));
    }

    dprintf(D_ALWAYS,"Aviary plugin count is %d\n",count++);

    return provider;
}
