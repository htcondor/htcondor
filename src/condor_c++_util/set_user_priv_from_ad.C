#include "condor_common.h"
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
