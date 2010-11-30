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

#include <string.h>
#include "dap_url.h"

// Constructor
Url::Url(const char* s)
{
	init();
	if (! s) return;
	if (! strlen(s) ) return;

	// Purely arbitrary URL length limit, currently consistent with Microsoft
	// Internet Explorer limit, as of November 2004.
	if ( strlen(s) > (sizeof(_url) - 1)) return;
	strcpy(_url, s);
	parse();
	screenPassword();
}

// Destructor
Url::~Url(void)
{
	if (_urlNoPassword) delete []_urlNoPassword;
	if (_scheme) delete []_scheme;
	if (_user) delete []_user;
	if (_password) delete []_password;
	if (_host) delete []_host;
	if (_port) delete []_port;
	if (_path) delete []_path;
	if (_pathField) delete []_pathField;
#ifdef STORK_SRB
	if (_srbUserField) delete []_srbUserField;
#endif // STORK_SRB
}

// init member data
void
Url::init(void)
{
	_url[0] = 0;
	_urlNoPassword = NULL;
	_passwordOffset = 0;
	_scheme = NULL;
	_user = NULL;
	_password = NULL;
	_host = NULL;
	_port = NULL;
	_path = NULL;
	_pathField = NULL;
	_dirname = NULL;
	_basename = NULL;
	parseSuccess = PARSE_FAIL;

#ifdef STORK_SRB
	_srbUserField = NULL;
	_srbUser = NULL;
	_srbMdasDomain = NULL;
	_srbZone = NULL;
	parseSrbSuccess = PARSE_FAIL;
#endif // STORK_SRB
}

// Parse success indicator
int
Url::parsed(void)
{
	return parseSuccess;
}

// Parse path
int
Url::parsePath(void)
{
	const char *p = _path;

	if (! p) return PARSE_FAIL;
	if (! strlen(p) ) return PARSE_FAIL;

	if (*p != '/') return PARSE_FAIL;

	// path is "/"
	if (! strcmp(p, "/")) return PARSE_SUCCESS;

	// make a copy of the path field.
	_pathField = new char[ strlen(p) + 1 ];
	strcpy(_pathField, p);

	// Canonicalize end of path without trailing '/'
	//ps = p + strlen(p) - 1;
	// if (*ps == '/') *ps = 0;

	// Parse the path field into a dirname and basename.  This parsing
	// algorithm assigns any string after the last / to the basename, and any
	// string before the last / to the dirname.
	char *pp = _pathField;	// writeable pointer
	_dirname = pp;
	char *lastslash = strrchr(pp, '/');
	if (! lastslash) return PARSE_FAIL;
	if (lastslash == pp) return PARSE_SUCCESS;	// shouldn't happen
	const char *lastch = pp + strlen(pp) - 1;

	if (lastslash == lastch) {
		// Canonicalize: does NOT end with a /
		*lastslash = 0;
		return PARSE_SUCCESS;
	}

	if (lastslash != pp) {
		*lastslash++ = 0;
		_basename = lastslash;
		return PARSE_SUCCESS;
	}
	return PARSE_SUCCESS;
}

// Parse engine
void
Url::parse(void)
{
	const char *p, *delim, *delim2, *end;
	size_t fieldsize;

	// Create a safety net to avoid parsing too far
	end = _url + strlen(_url);

	// walk the URL, a field at a time ...
	p = _url;

	// Handle file scheme as a special case.
    if (strstr(p, "file:/") == p) {
        int nSlashes = 0;
		_scheme = new char[ strlen( "file" ) + 1 ];
        strcpy(_scheme,  "file");
        p += strlen("file:/") - 1;
        if (p >= end) return; // failure
        while (*p == '/') {
            p++;
            nSlashes++;
        }
        if (p >= end) return; // failure
        if (nSlashes==1 || nSlashes==3) {
            p--;	// point at first slash of path
        } else {
            return; // failure
        }
		if (! strcmp(p, "/")) return;	// failure
		_path = new char[ strlen( p ) + 1 ];
        strcpy(_path,  p);
		if (! parsePath() ) return;	//failure
		// arbitrarily define host as localhost
		_host = new char[ strlen( "localhost" ) + 1 ];
        strcpy(_host,  "localhost");

        parseSuccess = PARSE_SUCCESS;
		return;
    }	// file scheme

	/* scheme */
	delim = strstr(p, "://");
	if (! delim) return;	// failure
	fieldsize = delim - p;
	if (! fieldsize) return;	//failure
	_scheme = new char[ fieldsize+1 ];
	strncpy(_scheme, p, fieldsize);
	_scheme[fieldsize] = 0;
	p = delim + strlen( "://");
	if (p >= end) return;	// failure

	// user, password
	delim = strstr(p, "@");
	if (delim) {

		delim2 = strstr(p, ":");
		if (delim2 && delim2 < delim) {
			// password
			fieldsize = delim2 - p;
			if (! fieldsize) return; // failure
			_user = new char[ fieldsize+1 ];
			strncpy(_user, p, fieldsize);
			_user[fieldsize] = 0;
			p = delim2 + strlen( ":");
			if (p >= end) return;	// failure
			fieldsize = delim - delim2 - 1;
			if (! fieldsize) return; // failure
			_password = new char[ fieldsize+1 ];
			strncpy(_password, delim2+1, fieldsize);
			_password[fieldsize] = 0;
			_passwordOffset = delim2 + 1 - _url;
		} else {
			// no password
			fieldsize = delim - p;
			if (! fieldsize) return;	//failure
			_user = new char[ fieldsize+1 ];
			strncpy(_user, p, fieldsize);
			_user[fieldsize] = 0;
		}
        p = delim + strlen("@");
        if (p >= end) return;	// failure
	}
    if (_password && !_user) {
        return;	// failure
    }

	// host, port
	delim = strstr(p, "/");
	if (delim) {

		delim2 = strstr(p, ":");
		if (delim2 && delim2 < delim) {
			// port
			fieldsize = delim2 - p;
			if (! fieldsize) return; // failure
			_host = new char[ fieldsize+1 ];
			strncpy(_host, p, fieldsize);
			_host[fieldsize] = 0;
			p = delim2 + strlen( ":");
			if (p >= end) return;	// failure
			fieldsize = delim - delim2 - 1;
			if (! fieldsize) return; // failure
			_port = new char[ fieldsize+1 ];
			strncpy(_port, delim2+1, fieldsize);
			_port[fieldsize] = 0;
		} else {
			// no port
			fieldsize = delim - p;
			if (! fieldsize) return;	//failure
			_host = new char[ fieldsize+1 ];
			strncpy(_host, p, fieldsize);
			_host[fieldsize] = 0;
		}
        p = delim;	// point at path
        if (p >= end) return;	// failure
	}
    if (_port && !_host) {
        return;	// failure
    }

	fieldsize = strlen(p);
	if (p + fieldsize > end) return;	// failure
	if (! strcmp(p, "/")) {
		// null path
		parseSuccess = PARSE_SUCCESS;
		return;

	}
	_path = new char[ fieldsize+1 ];
	strncpy(_path, p, fieldsize);
	_path[fieldsize] = 0;
	if (! parsePath() ) return;	//failure

	parseSuccess = PARSE_SUCCESS;
	return;
}

// make a safe copy of URL without password, for public display
void
Url::screenPassword(void)
{
	size_t urlLen = strlen(_url);
	if (! urlLen ) return;

	// copy the URL.
	_urlNoPassword = new char[ urlLen + 1 ];
	strcpy(_urlNoPassword, _url);
	
	// if the original URL had no password, we're done
	if (! _password) return;

	// Otherwise '*' out the password.
	size_t passwordLength = strlen(_password);
	char *p = _urlNoPassword + _passwordOffset;
	while (passwordLength > 0) {
		*p = '*';
		p++;
		passwordLength-- ;
	}
}

char *
Url::urlNoPassword(void)
{
	return _urlNoPassword;
}

char *
Url::scheme(void)
{
	return _scheme;
}

char *
Url::user(void)
{
	return _user;
}

char *
Url::password(void)
{
	return _password;
}

char *
Url::host(void)
{
	return _host;
}

char *
Url::port(void)
{
	return _port;
}

char *
Url::path(void)
{
	return _path;
}

char *
Url::dirname(void)
{
	return _dirname;
}

char *
Url::basename(void)
{
	return _basename;
}

#ifdef STORK_SRB
char *
Url::srbUser(void)
{
	return _srbUser;
}

char *
Url::srbMdasDomain(void)
{
	return _srbMdasDomain;
}

char *
Url::srbZone(void)
{
	return _srbZone;
}

// Parse SRB fields success indicator
int
Url::parsedSrb(void)
{
	return parseSrbSuccess;
}

// Parse SRB fields, of the format
// srb:// [username.mdasdomain [.zone] [:password] @] host [:port] [/path]
void
Url::parseSrb(void)
{
	char *p;

	// Must successfully parse URL first
	if (parseSuccess == PARSE_FAIL) return;	// failure

	// Require srb:// scheme
	if (strcmp("srb", _scheme) ) return;		// failure

	// Parse the user field
	if (_user) {

		// make a copy of the user field.
		_srbUserField = new char[ strlen(_user) + 1 ];
		strcpy(_srbUserField, _user);
		p = _srbUserField;

		p = strtok(p, ".");
		if (p) {
			_srbUser = p;
			p = strtok(NULL, ".");
			if (p) {
				_srbMdasDomain = p;
				p = strtok(NULL, ".");
				if (p) {
					_srbZone = p;
				}
			}
		}
	}	// _user

	parseSrbSuccess = PARSE_SUCCESS;
}

#endif // STORK_SRB
