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

#ifndef	_DAP_URL_H
#define	_DAP_URL_H

// The HTTP standard does not define a max URL length.  However,
// implementations are free to set this.  For sanity, here's ours:
#define MAX_URL	2048

class  Url {
	public:
		Url(const char* s);			// constructor
		~Url(void);					// destructor
		int parsed(void);			// parse success or fail method
		char *url(void);			// copied from URL
		char *urlNoPassword(void);	// parsed from URL
		char *scheme(void);			// parsed from URL
		char *user(void);			// parsed from URL
		char *password(void);		// parsed from URL
		char *host(void);			// parsed from URL
		char *port(void);			// parsed from URL
		char *path(void);			// parsed from URL
		char *dirname(void);		// parsed from URL
		char *basename(void);		// parsed from URL
#ifdef STORK_SRB
		int parsedSrb(void);		// parse SRB fields success or fail method
		char *srbUser(void);		// parsed from URL
		char *srbMdasDomain(void);	// parsed from URL
		char *srbZone(void);		// parsed from URL
		void parseSrb(void);		// parsed from URL
#endif // STORK_SRB

	private:
		char _url[MAX_URL];			// copy of original URL
		char *_urlNoPassword;
		size_t _passwordOffset;
		char *_scheme;
		char *_user;
		char *_password;
		char *_host;
		char *_port;
		char *_pathField;
		char *_path;
		char *_dirname;
		char *_basename;
		int parseSuccess;			// parse success or fail flag
#define PARSE_FAIL		0
#define PARSE_SUCCESS	1

#ifdef STORK_SRB
		char *_srbUserField;
		char *_srbUser;
		char *_srbMdasDomain;
		char *_srbZone;
		int parseSrbSuccess;			// parse success or fail flag
#endif // STORK_SRB

		// methods
		void init(void);			// init data members
		void parse(void);			// actual parse engine
		int  parsePath(void);		// parse path component
		void screenPassword(void);	// make a safe copy of URL without password

}; // class Url

#endif // _DAP_URL_H

