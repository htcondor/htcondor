
#include "condor_common.h"
#include "condor_config.h"
#include "condor_daemon_core.h"
#include "classad/classad.h"
#include "string_list.h"
#include "match_auth_verify.h"

bool
allow_match_auth(std::string const &the_user, classad::ClassAd *my_match_ad)
{
	IpVerify* ipv = daemonCore->getSecMan()->getIpVerify();
	condor_sockaddr addr; addr.set_ipv4(); addr.set_addr_any();
	std::string auth_id;
	std::string sinful = "UNKNOWN";
	if (my_match_ad)
	{
		my_match_ad->EvaluateAttrString(ATTR_AUTHENTICATED_IDENTITY, auth_id);
		if (my_match_ad->EvaluateAttrString(ATTR_MY_ADDRESS, sinful))
		{
			addr.from_sinful(sinful);
		}
	}
	MyString deny_reason, allow_reason;

	// We first check the special "self" token; otherwise, the IpVerify logic
	// will cache the deny entry.
	std::string deny_match;
	param(deny_match, "DENY_MATCH_AUTH");
	StringList sl_deny(deny_match.c_str());
	std::string allow_match;
	param(allow_match, "ALLOW_MATCH_AUTH");
	StringList sl_allow(allow_match.c_str());

	if (!sl_deny.contains_anycase("self") && auth_id.size() && the_user.size() && sl_allow.contains_anycase("self") && (auth_id == the_user))
	{
		dprintf(D_SECURITY, "Allowed match authentication for startd ad using self auth.\n");
		return true;
	}

	if (USER_AUTH_SUCCESS != ipv->Verify(MATCH_AUTH, addr, auth_id.size() ? auth_id.c_str() : NULL, &allow_reason, &deny_reason))
	{
		dprintf(D_ALWAYS, "MATCH_AUTHS: Refused match auth for %s for startd %s due to %s.\n", the_user.c_str(), sinful.c_str(), deny_reason.Value());
		return false;
	}
	dprintf(D_SECURITY, "Allowed match authentication for %s for startd %s due to: %s.\n", the_user.c_str(), sinful.c_str(), allow_reason.Value());
	return true;
}

