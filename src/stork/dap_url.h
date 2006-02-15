/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
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

