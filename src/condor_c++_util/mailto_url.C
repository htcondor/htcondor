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
#include "condor_config.h"
#include "url_condor.h"

static
int condor_open_mailto_url( const char *name, int flags )
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
	return open_url(filter_url, flags);
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
