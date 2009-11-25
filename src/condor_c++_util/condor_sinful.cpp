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
#include "condor_sinful.h"
#include "condor_debug.h"
#include "internet.h"
#include "condor_attributes.h"

static bool
urlDecode(char const *str,size_t max,std::string &result)
{
	size_t consumed = 0;
	while(*str) {
		size_t len = strcspn(str,"%");
		if( len + consumed > max ) {
			len = max - consumed;
		}
		result.append(str,0,len);
		str += len;
		consumed += len;
		if( consumed == max ) {
			break;
		}
		if( *str == '%' ) {
			char ch = 0;
			int i;
			str++;
			consumed++;
			for(i=0;i<2;i++,str++,consumed++) {
				ch = ch<<4;
				if( *str >= '0' && *str <= '9' ) {
					ch |= *str - '0';
				}
				else if( *str >= 'a' && *str <= 'f' ) {
					ch |= *str - 'a' + 10;
				}
				else if( *str >= 'A' && *str <= 'F' ) {
					ch |= *str - 'A' + 10;
				}
				else {
					return false;
				}
			}
			result += ch;
		}
	}
	return true;
}

static inline bool
needsUrlEncodeEscape(char ch)
{
	// The following is more conservative than it needs to be.
	// At the very least, we need to escape "&;> ,"
	if( isalnum(ch) || ch == '.' || ch == '_' || ch == '-' || ch == ':' || ch == '#' ) {
		return false;
	}
	return true;
}

static void
urlEncode(char const *str,std::string &result)
{
	while(*str) {
		size_t len = 0;
		while( str[len] && !needsUrlEncodeEscape(str[len]) ) {
			len++;
		}

		result.append(str,0,len);
		str += len;
		if( *str ) {
			char code[4];
			sprintf(code,"%%%02x",*str);
			result += code;
			str++;
		}
	}
}

template <typename map_type>
static bool parseUrlEncodedParams(char const *str,map_type &params)
{
	// Parse a string in format key1=value1&key2=value2&...
	// where keys and values are url-encoded using %XX escapes.
	// For delimiting key=value pairs, either '&' or ';' may be used.
	// If the same key appears multiple times, only the last one is
	// stored in the map.

	ASSERT( str );

	while( *str ) {
		while( *str == ';' || *str == '&' ) {
			str++;
		}
		if( !*str ) {
			break;
		}

		std::pair<std::string,std::string> keyval;
		size_t len = strcspn(str,"=&;");

		if( !len ) {
			return false;
		}
		if( !urlDecode(str,len,keyval.first) ) {
			return false;
		}

		str += len;

		if( *str == '=' ) {
			str++;

			len = strcspn(str,"&;");

			if( !urlDecode(str,len,keyval.second) ) {
				return false;
			}

			str += len;
		}

		// insert_result is a pair with an iterator pointing to an
		// existing conflicting member and a bool indicating success
		std::pair<typename map_type::iterator,bool> insert_result =
			params.insert(keyval);

		if( !insert_result.second ) {
			// key already in params
			ASSERT( insert_result.first->first == keyval.first );
			insert_result.first->second = keyval.second;
		}
	}
	return true;
}

template <typename map_type>
static std::string urlEncodeParams(map_type const &params)
{
	std::string result;

	typename map_type::const_iterator it;
	for(it=params.begin(); it!=params.end(); it++) {
		if( result.size() ) {
			result += "&";
		}
		urlEncode(it->first.c_str(),result);
		result += "=";
		urlEncode(it->second.c_str(),result);
	}

	return result;
}

Sinful::Sinful(char const *sinful)
{
	if( !sinful ) { // default constructor
		m_valid = true;
	}
	else {
		char *host=NULL;
		char *port=NULL;
		char *params=NULL;

		if( *sinful != '<' ) {
			m_sinful = "<";
			m_sinful += sinful;
			m_sinful += ">";
		}
		else {
			m_sinful = sinful;
		}

		m_valid = split_sin(m_sinful.c_str(),&host,&port,&params);

		if( m_valid ) {
			if( host ) {
				m_host = host;
			}
			if( port ) {
				m_port = port;
			}
			if( params ) {
				if( !parseUrlEncodedParams(params,m_params) ) {
					m_valid = false;
				}
			}
		}
		free( host );
		free( port );
		free( params );
	}
}

char const *
Sinful::getParam(char const *key) const
{
	std::map<std::string,std::string>::const_iterator it = m_params.find(key);
	if( it == m_params.end() ) {
		return NULL;
	}
	return it->second.c_str();
}

void
Sinful::setParam(char const *key,char const *value)
{
	if( !value ) {
		m_params.erase(key);
	}
	else {
		m_params[key] = value;
	}
	regenerateSinful();
}

void
Sinful::clearParams()
{
	m_params.clear();
	regenerateSinful();
}

int
Sinful::numParams() const
{
	return m_params.size();
}

void
Sinful::setCCBContact(char const *contact)
{
	setParam(ATTR_CCBID,contact);
}

char const *
Sinful::getCCBContact() const
{
	return getParam(ATTR_CCBID);
}

void
Sinful::setSharedPortID(char const *contact)
{
	setParam(ATTR_SOCK,contact);
}

char const *
Sinful::getSharedPortID() const
{
	return getParam(ATTR_SOCK);
}

void
Sinful::setPrivateAddr(char const *addr)
{
	setParam("PrivAddr",addr);
}

char const *
Sinful::getPrivateAddr() const
{
	return getParam("PrivAddr");
}

void
Sinful::setPrivateNetworkName(char const *addr)
{
	setParam("PrivNet",addr);
}

char const *
Sinful::getPrivateNetworkName() const
{
	return getParam("PrivNet");
}

void
Sinful::setHost(char const *host)
{
	m_host = host;
	regenerateSinful();
}
void
Sinful::setPort(char const *port)
{
	m_port = port;
	regenerateSinful();
}
void
Sinful::setPort(int port)
{
	m_port = port;
	regenerateSinful();
}

void
Sinful::regenerateSinful()
{
	// generate "<host:port?params>"

	m_sinful = "<";
	m_sinful += m_host;
	if( m_port.size() ) {
		m_sinful += ":";
		m_sinful += m_port;
	}
	if( m_params.size() ) {
		m_sinful += "?";
		m_sinful += urlEncodeParams(m_params);
	}
	m_sinful += ">";
}

bool
Sinful::addressPointsToMe( Sinful &addr )
{
	if( getHost() && addr.getHost() && !strcmp(getHost(),addr.getHost()) &&
		getPort() && addr.getPort() && !strcmp(getPort(),addr.getPort()) )
	{
		char const *spid = getSharedPortID();
		char const *addr_spid = addr.getSharedPortID();
		if( spid == NULL && addr_spid == NULL ||
			spid && addr_spid && !strcmp(spid,addr_spid) )
		{
			return true;
		}
	}
	if( getPrivateAddr() ) {
		Sinful private_addr( getPrivateAddr() );
		return private_addr.addressPointsToMe( addr );
	}
	return false;
}

int
Sinful::getPortNum()
{
	if( !getPort() ) {
		return -1;
	}
	return atoi( getPort() );
}
