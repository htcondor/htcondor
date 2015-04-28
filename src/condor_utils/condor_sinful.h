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
	HTCondor has three layers of address information.  At the bottom layer,
	a condor_sockaddr represents a data structure that can be passed into the
	kernel and identifies an particular actual endpoint.  It identifies a
	protocol and a protocol-specific address.  For instance, the protocol
	IPv4 splits its address field into "addresses" and "ports".

	The middle layer is an individual source route, a specific sequence of
	connections to make and data to send down those connections.  At present,
	the most complicated individual source route we can implement or
	represent is the following: a shared port daemon in front of a collector,
	a CCB callback, and a shared port daemon in front of the callback's
	return address.  Note that this only requires the client (source) to
	know a single condor_sockaddr: that of the first shared port daemon.

	We don't presently represent individual source routes with their own
	class; instead, we represent them as degenerate cases of the upper
	layer of addressing.

	The upper layer of addressing is a list of all a daemon's source routes
	and the network name associated with each route's condor_sockaddr.  This
	class manages these, highest-level addresses.

	This class used to be a string-manipulation class.  It serialized a
	route's condor_sockaddr with its ip-and-port string, and appended a
	URL-style parameter list which encoded the routing parameters.  Some
	of these (e.g., CCBID, PrivAddr) included one or more serialized
	condor_sockaddrs.

	We refer to strings of that style as "original Sinful strings."  They
	should never be generated, but for backwards compatibility, we must
	correctly parse them.  Strings of that style which do not include any IPv6
	addresses, and which have only the following parameters, are referred
	to as "v0 Sinful strings".  We generate these strings only for backwards-
	compability, and only as part of the ClassAd we return when asked for
	our ClassAd representation.

	(Valid v0 parameters are, exclusively: "PrivAddr", "PrivNet", "noUDP",
	"sock", "alias", and "CCBID".  All other parameters will be ignored
	whe generated a v0 string, including the "addrs" parameter set by
	addAddrToAddrs(), or any custom attributes from setParam().)

	For internal usage, we support generating "v1 Sinful strings."  This
	support is born deprecated; whensoever possible, you should simply
	pass the corresponding Sinful object, instead.  A "v1 Sinful string"
	is the canonical serialization of the corresponding ClassAd (as
	returned by getClassad()).

	The v1 ClassAd is a list of ClassAds, each an individual source route,
	each of which must have the following attributes:
		* "a", the address literal (a string)
		* "p", the protocol (a string)
		* "port", the port number (an integer)
		* "n", the network name
	and which may have the following attributes:
		* "alias", a hostname alias for GSI security
		* "CCB", the CCB ID
		* "CCBSharedPort", the shared port ID of the broker
		* "noUDP", a flag (absent if unset) that prevents UDP
		* "sp", the shared port ID.
	Note that the a, p, port tuple is a serialization of a condor_sockaddr.
*/

#include <string>
#include <map>
#include <vector>

#include "condor_sockaddr.h"

enum sinful_version { SV_INVALID_MIN, SV_ORIGINAL, SV_0, SV_1, SV_INVALID_MAX };

class Sinful {
 public:

	// Reliably discerns among properly-formed Sinful strings of various
	// different versions.  Falls back on SV_ORIGINAL behavior if the
	// string is not a properly-formed Sinful.  Deprecated, since you
	// should always know.  (FIXME: Add a function with less-awful semantics
	// and support for DNS to handle figuring out what values from the
	// command-line and/or config-time actually mean.)
	Sinful( char const * sinful = NULL );

	// Results in an invalid Sinful if the string doesn't match the version.
	Sinful( sinful_version v, const std::string & sinful );

	// Necessary for inheriting command sockets.  You shouldn't call them.
	static char const * spacelessEncode( char const * in );
	static char const * spacelessDecode( char const * in );

	bool valid() const { return m_valid; }

	char const * getPrettyString() const;

	char const * getV1String() const;
	// ... getClassAd() const;

	// returns the host portion of the sinful string
	char const *getHost() const;

	// returns the port portion of the sinful string
	char const *getPort() const;

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


	// You must delete the return if it's not NULL.
	std::vector< condor_sockaddr > * getAddrs() const;
	void addAddrToAddrs( const condor_sockaddr & sa );
	void clearAddrs();
	bool hasAddrs() const;

	// generic param interface

	// returns the value of the named parameter (may be NULL)
	char const *getParam(char const *key) const;
	// if value is NULL, deletes the parameter; o.w. sets it
	void setParam(char const *key,char const *value);
	// returns number of params
	int numParams() const;

	// returns true if addr points to this address
	bool addressPointsToMe( Sinful const &addr ) const;

 private:
	bool m_valid;

	std::string m_prettyString;
	void regeneratePrettyString();

	// If the unversioned constructor finds something that isn't obviously
	// anything else, it'll check to see if it came from the user.
	bool handleHostAndPort( char const * userInput );

	// A limited form of original Sinful string.
	static std::string privateAddressString( condor_sockaddr sa, const std::string & sharedPortID );

	// We parse original Sinful strings, but never generate them,
	// so we have no need to cache their results.
	void parseOriginalSinful( const std::string & originalSinful );

	// We generate v0 strings, but we never parse them, because a v0
	// string is only generated by code the understands v1 strings, and
	// will be using those instead.  Only earlier versions of HTCondor
	// will ever parse a v0 string, and then, by treating it as an
	// original Sinful.
	std::string m_v0String;
	void regenerateV0String();

	std::string m_v1String;
	void regenerateV1String();
	void parseV1String();

	void regenerateStrings();

	std::string m_host;
	std::string m_port;
	std::string m_alias;
	std::map<std::string,std::string> m_params;

	std::vector< condor_sockaddr > addrs;
};

#endif
