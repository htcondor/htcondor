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

#ifndef __CONDOR_NET_STRING_LIST_H
#define __CONDOR_NET_STRING_LIST_H

#include "condor_common.h"
#include "string_list.h"


/*

  This class is derived from StringList, but adds support for
  network-related string manipulations.  This way, we can have
  ClassAds depend on the regular, simple StringList, without pulling
  in dependencies on all our network-related utilities.  This is sort
  of a hack, but seems like a lesser evil, especially since we only
  use this string_withnetwork() method in a single place in the code
  (condor_daemon_core.V6/condor_ipverify.C). 
*/
class NetStringList : public StringList {
public:
	NetStringList(const char *s = NULL, const char *delim = " ," ); 
	const char *  string_withnetwork( const char * );
};

#endif /* __CONDOR_NET_STRING_LIST_H */

