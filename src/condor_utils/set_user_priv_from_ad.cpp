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
#include "condor_debug.h"
#include "condor_classad.h"
#include "condor_uid.h"
#include "condor_attributes.h"
#include "set_user_priv_from_ad.h"

bool init_user_ids_from_ad( const classad::ClassAd &ad )
{
	const char* owner = nullptr;
	const char* domain = nullptr;
	std::string user;
	std::string buf;
	std::string ntdomain;

	if (ad.EvaluateAttrString(ATTR_OS_USER, user)) {
		owner = name_of_user(user.c_str(), buf);
		domain = domain_of_user(user.c_str(), nullptr);
	} else {
		if (!ad.EvaluateAttrString(ATTR_USER,  user)) {
			dPrintAd(D_ERROR, ad);
			dprintf( D_ERROR, "Failed to find %s or %s in job ad.\n", ATTR_OS_USER, ATTR_USER );
			return false;
		}

		owner = name_of_user(user.c_str(), buf);

		if (ad.EvaluateAttrString(ATTR_NT_DOMAIN, ntdomain)) {
			domain = ntdomain.c_str();
		}
	}

	if (!init_user_ids(owner, domain)) {
		dprintf(D_ERROR, "Failed in init_user_ids(%s,%s)\n",
		        owner?owner:"(null)", domain?domain:"(null)");
		return false;
	}

	return true;
}

priv_state set_user_priv_from_ad(classad::ClassAd const &ad)
{
	if ( !init_user_ids_from_ad( ad ) ) {
		EXCEPT( "Failed to initialize user ids." );
	}

	return set_user_priv();
}
