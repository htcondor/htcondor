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
#include <vector>

#include "condor_sockaddr.h"

class Sinful {
 public:
 	//
 	// Version-independent methods.
 	//
	Sinful( char const * sinful = NULL );
	bool valid() const { return m_valid; }
	const std::string & serialize() const;
	std::string logging() const;
	std::string logging( const std::string & ifInvalid ) const;

	//
	// The only capability added by v1 is multiple "primary" addresses.
	// Code for v1 will therefore make use of the v0 functions.
	//

	std::vector< condor_sockaddr > * getV1Addrs() const;
	void addAddrToV1Addrs( const condor_sockaddr & sa );
	void clearV1Addrs();
	bool hasV1Addrs();

	//
	// Methods for v0 (and v1; see above).
	//

	bool hasV0HostSockAddr() const;
	condor_sockaddr getV0HostSockAddr() const;
	std::string getV0CCBEmbedding() const;

	// returns an IPv4-only, v0-formatted string.  Should only be called
	// if you're constructing an ad.  Otherwise, use serialize().
	char const * getV0() const { if( m_sinful.empty() ) return NULL; else return m_sinful.c_str(); }

public:
 	//
 	// All methods below this line are deprecated and should be renamed
 	// (e.g., getV0Host()), so that we can find and replace the old and
 	// broken code that assumes that each daemon has One True Address.
 	//

	// returns the host portion of the sinful string
	char const *getHost() const { if( m_host.empty() ) return NULL; else return m_host.c_str(); }

	// returns the port portion of the sinful string
	char const *getPort() const { if( m_port.empty() ) return NULL; else return m_port.c_str(); }

	// returns -1 if port not set; o.w. port number
	int getPortNum() const;

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
	// hostname alias
	char const *getAlias() const;
	void setAlias(char const *alias);
	// is the noUDP flag set in this address?
	bool noUDP() const;
	void setNoUDP(bool flag);

	// generic param interface

	// returns the value of the named parameter (may be NULL)
	char const *getParam(char const *key) const;
	// if value is NULL, deletes the parameter; o.w. sets it
	void setParam(char const *key,char const *value);
	// remove all params
	void clearParams();
	// returns number of params
	int numParams() const;

	// returns true if addr points to this address
	bool addressPointsToMe( Sinful const &addr ) const;

 private:
	std::string m_sinful; // the sinful string "<host:port?params>"
	std::string m_host;
	std::string m_port;
	std::string m_alias;
	std::map<std::string,std::string> m_params; // key value pairs from params
	bool m_valid;

	std::vector< condor_sockaddr > addrs;

	void regenerateSinful();

	// Necessary to duplicate the behavior of the old getSinful().
	std::string m_serialized;

	int m_version;
};

#endif
