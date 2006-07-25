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

#ifndef NORDUGRIDRESOURCE_H
#define NORDUGRIDRESOURCE_H

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

#include "nordugridjob.h"
#include "baseresource.h"
#include "gahp-client.h"

class NordugridJob;
class NordugridResource;

class NordugridResource : public BaseResource
{
 public:

	NordugridResource( const char *resource_name, const char *proxy_subject );
	~NordugridResource();

	void Reconfig();

	static const char *HashName( const char *resource_name,
								 const char *proxy_subject );
	static NordugridResource *FindOrCreateResource( const char *resource_name,
													const char *proxy_subject );

	char *proxySubject;
	GahpClient *gahp;

 private:
	void DoPing( time_t& ping_delay, bool& ping_complete,
				 bool& ping_succeeded  );
	static HashTable <HashKey, NordugridResource *> ResourcesByName;

};

#endif
