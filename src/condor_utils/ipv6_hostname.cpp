#include "condor_common.h"
#include <set>
#include "MyString.h"
#include "condor_sockaddr.h"
#include "condor_netdb.h"
#include "condor_config.h"
#include "condor_sockfunc.h"
#include "ipv6_hostname.h"
#include "ipv6_addrinfo.h"
#include "my_hostname.h"

static condor_sockaddr local_ipaddr;
static MyString local_hostname;
static MyString local_fqdn;
static bool hostname_initialized = false;

static bool nodns_enabled()
{
	return param_boolean("NO_DNS", false);
}

void init_local_hostname()
{
		// [m.]
		// initializing local hostname, ip address, fqdn was
		// super complex.
		//
		// implementation was scattered over condor_netdb and
		// my_hostname, get_full_hostname.
		//
		// above them has duplicated code in many ways.
		// so I aggregated all of them into here.

		// Temporarily use the old functions to determine the local
		// hostname and IP address. The code below needs several
		// fixes. See gittrac #2216 for details.
	condor_sockaddr addr( *my_sin_addr() );
	local_ipaddr = addr;;
	local_hostname = my_hostname();
	local_fqdn = my_full_hostname();
#if 0
	bool ipaddr_inited = false;
	char hostname[MAXHOSTNAMELEN];
	int ret;

		// [TODO:IPV6] condor_gethostname is not IPv6 safe.
		// reimplement it.
	ret = condor_gethostname(hostname, sizeof(hostname));
	if (ret) {
		dprintf(D_HOSTNAME, "condor_gethostname() failed. Cannot initialize "
				"local hostname, ip address, FQDN.\n");
		return;
	}

		// if NETWORK_INTERFACE is defined, we use that as a local ip addr.
	MyString network_interface;
	if (param(network_interface, "NETWORK_INTERFACE")) {
		if (local_ipaddr.from_ip_string(network_interface))
			ipaddr_inited = true;
	}

		// now initialize hostname and fqdn
	if (nodns_enabled()) { // if nodns is enabled, we can cut some slack.
			// condor_gethostname() returns a hostname with
			// DEFAULT_DOMAIN_NAME. Thus, it is always fqdn
		local_hostname = hostname;
		local_fqdn = hostname;
		if (!ipaddr_inited) {
			local_ipaddr = convert_hostname_to_ipaddr(local_hostname);
		}
		return;
	}

	addrinfo_iterator ai;
	ret = ipv6_getaddrinfo(hostname, NULL, ai);
	if (ret) {
			// write some error message
		dprintf(D_HOSTNAME, "hostname %s cannot be resolved by getaddrinfo\n",
				hostname);
		return;
	}
	
	bool got_fqdn = false;
	while (addrinfo* info = ai.next()) {
		const char* name = info->ai_canonname;
		if (!name)
			continue;
		const char* dotpos = strchr(name, '.');
		condor_sockaddr addr(info->ai_addr);

		if (addr.is_loopback() || addr.is_private_network())
			continue;

		if (dotpos) {
				// consider it as a FQDN
			local_fqdn = name;
			local_hostname = local_fqdn.Substr(0, dotpos-name-1);
			if (!ipaddr_inited)
				local_ipaddr = addr;
			got_fqdn = true;
			break;
		}
		else {
			local_hostname = name;
			if (!ipaddr_inited)
				local_ipaddr = addr;
		}
	}

	dprintf(D_HOSTNAME, "Identifying myself as: Short: %s, Long: %s, IP: %s\n", local_hostname.Value(), local_fqdn.Value(), local_ipaddr.to_ip_string().Value());
	if (!got_fqdn) {
		local_fqdn = local_hostname;
		MyString default_domain;
		if (param(default_domain, "DEFAULT_DOMAIN_NAME")) {
			if (default_domain[0] != '.')
				local_fqdn += ".";
			local_fqdn += default_domain;
		}
	}
#endif
	hostname_initialized = true;
}

condor_sockaddr get_local_ipaddr()
{
	if (!hostname_initialized)
		init_local_hostname();
	return local_ipaddr;
}

MyString get_local_hostname()
{
	if (!hostname_initialized)
		init_local_hostname();
	return local_hostname;
}

MyString get_local_fqdn()
{
	if (!hostname_initialized)
		init_local_hostname();
	return local_fqdn;
}

MyString get_hostname(const condor_sockaddr& addr)
{
	MyString ret;
	if (nodns_enabled())
		return convert_ipaddr_to_hostname(addr);

	condor_sockaddr targ_addr;

	// just like sin_to_string(), if given address is 0.0.0.0 or equivalent,
	// it changes to local IP address.
	if (addr.is_addr_any())
		targ_addr = get_local_ipaddr();
	else
		targ_addr = addr;

	int e;
	char hostname[NI_MAXHOST];

	e = condor_getnameinfo(targ_addr, hostname, sizeof(hostname), NULL, 0, 0);
	if (e)
		return ret;

	ret = hostname;
	return ret;
}

std::vector<MyString> get_hostname_with_alias(const condor_sockaddr& addr)
{
	std::vector<MyString> ret;
	MyString hostname = get_hostname(addr);
	if (hostname.IsEmpty())
		return ret;
	ret.push_back(hostname);

	if (nodns_enabled())
		return ret; // no need to call further DNS functions.

	hostent* ent;
		//int aftype = addr.get_aftype();
		//ent = gethostbyname2(hostname.Value(), addr.get_aftype());

		// really should call gethostbyname2() however most platforms do not
		// support. (Solaris, HP-UX, IRIX)
		// complete DNS aliases can be only obtained by gethostbyname.
		// however, what happens if we call it in IPv6-only system?
		// can we get DNS aliases for the hostname that only contains
		// IPv6 addresses?
	ent = gethostbyname(hostname.Value());

	if (!ent)
		return ret;

	char** alias = ent->h_aliases;
	for (; *alias; ++alias) {
		ret.push_back(MyString(*alias));
	}
	return ret;
}

// look up FQDN for hostname and aliases.
// if not, it adds up DEFAULT_DOMAIN_NAME
MyString get_full_hostname(const condor_sockaddr& addr)
{
		// this function will go smooth even with NODNS.
	MyString ret;
	std::vector<MyString> hostnames = get_hostname_with_alias(addr);
	if (hostnames.empty()) return ret;
	std::vector<MyString>::iterator iter;
	for (iter = hostnames.begin(); iter != hostnames.end(); ++iter) {
		MyString& str = *iter;
		if (str.FindChar('.') != -1) {
			return str;
		}
	}

	MyString default_domain;
	if (param(default_domain, "DEFAULT_DOMAIN_NAME")) {
			// first element is the hostname got by gethostname()
		ret = *hostnames.begin();
		if (default_domain[0] != '.')
			ret += ".";
		ret += default_domain;
	}
	return ret;
}

std::vector<condor_sockaddr> resolve_hostname(const char* hostname)
{
	MyString host(hostname);
	return resolve_hostname(host);
}


std::vector<condor_sockaddr> resolve_hostname(const MyString& hostname)
{
	std::vector<condor_sockaddr> ret;
	if (nodns_enabled()) {
		condor_sockaddr addr = convert_hostname_to_ipaddr(hostname);
		if (addr == condor_sockaddr::null)
			return ret;
		ret.push_back(addr);
		return ret;
	}

	addrinfo_iterator ai;
	bool res  = ipv6_getaddrinfo(hostname.Value(), NULL, ai);
	if (res) {
		return ret;
	}
	
		// To eliminate duplicate address, here we use std::set
	std::set<condor_sockaddr> s;
	while (addrinfo* info = ai.next()) {
		s.insert(condor_sockaddr(info->ai_addr));
	}

	ret.insert(ret.begin(), s.begin(), s.end());
	return ret;
}

MyString convert_ipaddr_to_hostname(const condor_sockaddr& addr)
{
	MyString ret;
	MyString default_domain;
	if (!param(default_domain, "DEFAULT_DOMAIN_NAME")) {
		dprintf(D_HOSTNAME,
				"NO_DNS: DEFAULT_DOMAIN_NAME must be defined in your "
				"top-level config file\n");
		return ret;
	}

	ret = addr.to_ip_string();
	for (int i = 0; i < ret.Length(); ++i) {
		if (ret[i] == '.' || ret[i] == ':')
			ret.setChar(i, '-');
	}
	ret += ".";
	ret += default_domain;

	return ret;
}

condor_sockaddr convert_hostname_to_ipaddr(const MyString& fullname)
{
	MyString hostname;
	MyString default_domain;
	bool truncated = false;
	if (param(default_domain, "DEFAULT_DOMAIN_NAME")) {
		MyString dotted_domain = ".";
		dotted_domain += default_domain;
		int pos = fullname.find(dotted_domain.Value());
		if (pos != -1) {
			truncated = true;
			hostname = fullname.Substr(0, pos - 1);
		}
	}
	if (!truncated)
		hostname = fullname;

	char target_char = '.';
		// [TODO] Implement a way to detect IPv6 address
		//if (hostname.Length() > 15) { // assume we have IPv6 address
		//target_char = ':';
		//}

		// converts hostname to IP address string
	for (int i = 0; i < hostname.Length(); ++i) {
		if (hostname[i] == '-')
			hostname.setChar(i, target_char);
	}

	condor_sockaddr ret;
	ret.from_ip_string(hostname);
	return ret;
}
