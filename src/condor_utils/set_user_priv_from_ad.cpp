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

priv_state set_user_priv_from_ad(ClassAd const &ad)
{
	char *owner = NULL;
	char *domain = NULL;

	if (!ad.LookupString(ATTR_OWNER, &owner)) {
		ClassAd ad_copy;
		ad_copy = ad;
		ad_copy.dPrint(D_ALWAYS);
		EXCEPT("Failed to find %s in job ad.", ATTR_OWNER);
	}

	if (!ad.LookupString(ATTR_NT_DOMAIN, &domain)) {
		domain = strdup("");
	}

	if (!init_user_ids(owner, domain)) {
		EXCEPT("Failed in init_user_ids(%s,%s)",
			   owner ? owner : "(nil)",
			   domain ? domain : "(nil)");
	}

	free(owner);
	free(domain);

	return set_user_priv();
}
