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


#ifndef _CONDOR_SINFUL_H
#define _CONDOR_SINFUL_H

/*
 Sinful is a class for manipulating sinful strings (i.e. addresses in condor).
 The format of a sinful string is:
   <host:port?params>
 where params is a url-encoded list of parameters.

 Example:  <192.168.0.10:1492?param1=value1&param2=value2>
 */


#include <string>
#include <map>

class Sinful {
 public:
	Sinful(char const *sinful=NULL);
	bool valid() const { return m_valid; }

	// returns the full sinful string
	char const *getSinful() const { return m_sinful.c_str(); }

	// returns the host portion of the sinful string
	char const *getHost() { return m_host.c_str(); }

	// returns the port portion of the sinful string
	char const *getPort() { return m_port.c_str(); }

	void setHost(char const *host);
	void setPort(char const *port);
	void setPort(int port);

	// specific params

	// returns the CCB contact info (may be NULL)
	char const *getCCBContact() const;
	void setCCBContact(char const *contact);
	// private network address (may be NULL)
	char const *getPrivateAddr() const;
	void setPrivateAddr(char const *addr);
	// private network name (may be NULL)
	char const *getPrivateNetworkName() const;
	void setPrivateNetworkName(char const *name);
	// id of SharedPortEndpoint (i.e. basename of named socket)
	char const *getSharedPortID() const;
	void setSharedPortID(char const *contact);

	// generic param interface

	// returns the value of the named parameter (may be NULL)
	char const *getParam(char const *key) const;
	// if value is NULL, deletes the parameter; o.w. sets it
	void setParam(char const *key,char const *value);
	// remove all params
	void clearParams();
	// returns number of params
	int numParams() const;

 private:
	std::string m_sinful; // the sinful string "<host:port?params>"
	std::string m_host;
	std::string m_port;
	std::map<std::string,std::string> m_params; // key value pairs from params
	bool m_valid;

	void regenerateSinful();
};

#endif
