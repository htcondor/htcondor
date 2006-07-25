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

 

#include "condor_common.h"
#include "condor_config.h"
#include "url_condor.h"

static
int condor_open_mailto_url( const char *name, int flags, 
									   size_t n_bytes )
{
	char	*mail_prog = 0;
	char	filter_url[1000];

	if( flags != O_WRONLY ) {
		return -1;
	} 

	mail_prog = param("MAIL");
	if (mail_prog == 0) {
		return -1;
	}

	sprintf(filter_url, "filter:%s %s", mail_prog, name);
	free(mail_prog);
	return open_url(filter_url, flags, n_bytes);
}


void
init_mailto()
{
	static URLProtocol	*MAILTO_URL = 0;

	if (MAILTO_URL == 0) {
	    MAILTO_URL = new URLProtocol("mailto", "MailToUrl", 
					 condor_open_mailto_url);
	}
}
