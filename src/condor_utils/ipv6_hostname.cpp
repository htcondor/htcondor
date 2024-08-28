#include "condor_common.h"
#include <set>
#include "condor_sockaddr.h"
#include "condor_netdb.h"
#include "condor_config.h"
#include "condor_sockfunc.h"
#include "ipv6_hostname.h"
#include "ipv6_addrinfo.h"
#include "my_hostname.h"

static condor_sockaddr local_ipaddr;
static condor_sockaddr local_ipv4addr;
static condor_sockaddr local_ipv6addr;
static std::string local_hostname;
static std::string local_fqdn;
static bool hostname_initialized = false;

static bool nodns_enabled()
{
	return param_boolean("NO_DNS", false);
}

bool init_local_hostname_impl()
{
	bool local_hostname_initialized = false;
	if (param(local_hostname, "NETWORK_HOSTNAME")) {
		local_hostname_initialized = true;
		dprintf(D_HOSTNAME, "NETWORK_HOSTNAME says we are %s\n", local_hostname.c_str());
	}

	if( ! local_hostname_initialized ) {
		// [TODO:IPV6] condor_gethostname is not IPv6 safe. Reimplement it.
		char hostname[MAXHOSTNAMELEN];
		int ret = condor_gethostname(hostname, sizeof(hostname));
		if (ret) {
			dprintf(D_ALWAYS, "condor_gethostname() failed. Cannot initialize "
					"local hostname, ip address, FQDN.\n");
			return false;
		}
		local_hostname = hostname;
	}

	std::string test_hostname = local_hostname;

	bool local_ipaddr_initialized = false;

	std::string network_interface;
	if (param(network_interface, "NETWORK_INTERFACE")) {
		if(local_ipaddr_initialized == false &&
			local_ipaddr.from_ip_string(network_interface)) {
			local_ipaddr_initialized = true;
			if(local_ipaddr.is_ipv4()) { 
				local_ipv4addr = local_ipaddr;
			}
			if(local_ipaddr.is_ipv6()) { 
				local_ipv6addr = local_ipaddr;
			}
		}
	}

	if( ! local_ipaddr_initialized ) {
		std::string ipv4, ipv6, ipbest;
		if( network_interface_to_ip("NETWORK_INTERFACE", network_interface.c_str(), ipv4, ipv6, ipbest)) {
			ASSERT(local_ipaddr.from_ip_string(ipbest));
			// If this fails, network_interface_to_ip returns something invalid.
			local_ipaddr_initialized = true;
		} else {
			dprintf(D_ALWAYS, "Unable to identify IP address from interfaces.  None match NETWORK_INTERFACE=%s. Problems are likely.\n", network_interface.c_str());
		}
		if((!ipv4.empty()) && local_ipv4addr.from_ip_string(ipv4)) {
			ASSERT(local_ipv4addr.is_ipv4());
		}
		if((!ipv6.empty()) && local_ipv6addr.from_ip_string(ipv6)) {
			ASSERT(local_ipv6addr.is_ipv6());
		}
	}

	if (nodns_enabled()) {
			// condor_gethostname() returns a hostname with
			// DEFAULT_DOMAIN_NAME. Thus, it is always fqdn
		local_fqdn = local_hostname;
		if (!local_ipaddr_initialized) {
			local_ipaddr = convert_fake_hostname_to_ipaddr(local_hostname);
			if (local_ipaddr != condor_sockaddr::null) {
				local_ipaddr_initialized = true;
			}
		}
	} else if (!local_hostname_initialized) {
		addrinfo* info = nullptr;
		const int MAX_TRIES = 20;
		const int SLEEP_DUR = 3;
		bool gai_success = false;
		for(int try_count = 1; true; try_count++) {
			int ret = ipv6_getaddrinfo(test_hostname.c_str(), NULL, info);
			if(ret == 0) { gai_success = true; break; }
			if(ret != EAI_AGAIN ) {
				dprintf(D_ALWAYS, "init_local_hostname_impl: ipv6_getaddrinfo() could not look up '%s': %s (%d).  Error is not recoverable; giving up.  Problems are likely.\n", test_hostname.c_str(), gai_strerror(ret), ret );
				gai_success = false;
				break;
			}

			dprintf(D_ALWAYS, "init_local_hostname_impl: ipv6_getaddrinfo() returned EAI_AGAIN for '%s'.  Will try again after sleeping %d seconds (try %d of %d).\n", test_hostname.c_str(), SLEEP_DUR, try_count + 1, MAX_TRIES );
			if(try_count == MAX_TRIES) {
				dprintf(D_ALWAYS, "init_local_hostname_impl: ipv6_getaddrinfo() never succeeded. Giving up. Problems are likely\n");
				break;
			}
			sleep(SLEEP_DUR);
		}

		if(gai_success) {
			if (info->ai_canonname) {
				local_hostname = info->ai_canonname;
			}
			freeaddrinfo(info);
		}

	}

	size_t dotpos = local_hostname.find('.');
	if (dotpos != std::string::npos) { // consider it as a FQDN
		local_fqdn = local_hostname;
		local_hostname.resize(dotpos);
	} else {
		local_fqdn = local_hostname;
		std::string default_domain;
		if (param(default_domain, "DEFAULT_DOMAIN_NAME")) {
			if (default_domain[0] != '.')
				local_fqdn += ".";
			local_fqdn += default_domain;
		}
	}
	dprintf(D_HOSTNAME, "hostname: %s\n", local_fqdn.c_str());

	return true;
}

void reset_local_hostname() {
	if( ! init_local_hostname_impl() ) {
		dprintf( D_ALWAYS, "Something went wrong identifying my hostname and IP address.\n" );
		hostname_initialized = false;
	} else {
		dprintf( D_HOSTNAME, "I am: hostname: %s, fully qualified doman name: %s, IP: %s, IPv4: %s, IPv6: %s\n", local_hostname.c_str(), local_fqdn.c_str(), local_ipaddr.to_ip_string().c_str(), local_ipv4addr.to_ip_string().c_str(), local_ipv6addr.to_ip_string().c_str() );
		hostname_initialized = true;
	}
}

void init_local_hostname() {
	if( hostname_initialized ) { return; }
	reset_local_hostname();
}

condor_sockaddr get_local_ipaddr(condor_protocol proto)
{
	init_local_hostname();
	if ((proto == CP_IPV4) && local_ipv4addr.is_ipv4()) { return local_ipv4addr; }
	if ((proto == CP_IPV6) && local_ipv6addr.is_ipv6()) { return local_ipv6addr; }
	return local_ipaddr;
}

std::string get_local_hostname()
{
	init_local_hostname();
	return local_hostname;
}

std::string get_local_fqdn()
{
	init_local_hostname();
	return local_fqdn;
}

std::string get_fqdn_from_hostname(const std::string& hostname) {
	if (hostname.find('.') != std::string::npos)
		return hostname;

	std::string ret;

	if (!nodns_enabled()) {
		addrinfo* info = nullptr;
		int res  = ipv6_getaddrinfo(hostname.c_str(), NULL, info);
		if (res) {
			dprintf(D_HOSTNAME, "ipv6_getaddrinfo() could not look up %s: %s (%d)\n", 
				hostname.c_str(), gai_strerror(res), res);
			return ret;
		}

		if (info && info->ai_canonname != nullptr) {
			if (strchr(info->ai_canonname, '.')) {
				ret = info->ai_canonname;
				freeaddrinfo(info);
				return ret;
			}
		}
		freeaddrinfo(info);
	}

	std::string default_domain;
	if (param(default_domain, "DEFAULT_DOMAIN_NAME")) {
		ret = hostname;
		if (ret[ret.length() - 1] != '.')
			ret += ".";
		ret += default_domain;
	}
	return ret;
}

int get_fqdn_and_ip_from_hostname(const std::string & hostname,
		std::string & fqdn, condor_sockaddr& addr) {

	std::string ret;
	std::vector<condor_sockaddr> addr_list;

	addr_list = resolve_hostname(hostname, &ret);

	// if FQDN is still unresolved, try to use given hostname
	if (ret.empty()) {
		std::string default_domain;
		// if the hostname contains dot, hostname is assumed to be full hostname
		// else try DEFAULT_DOMAIN_NAME
		if (hostname.find('.') != std::string::npos) {
			ret = hostname;
		} else if (param(default_domain, "DEFAULT_DOMAIN_NAME")) {
			ret = hostname + "." + default_domain;
		}
	}

	if (!ret.empty() && !addr_list.empty()) {
		fqdn = ret;
		addr = addr_list[0];
		return 1;
	}
	return 0;
}

std::string get_hostname(const condor_sockaddr& addr) {
	std::string ret;
	if (nodns_enabled())
		return convert_ipaddr_to_fake_hostname(addr);

	condor_sockaddr targ_addr;

	// if given address is 0.0.0.0 or equivalent,
	// it changes to local IP address.
	if (addr.is_addr_any())
		targ_addr = get_local_ipaddr(addr.get_protocol());
	else
		targ_addr = addr;

	int e;
	char hostname[NI_MAXHOST];

	// if given address is link-local IPv6 address, it will have %NICname
	// at the end of string
	// we would like to avoid it
	if (targ_addr.is_ipv6())
		targ_addr.set_scope_id(0);

	e = condor_getnameinfo(targ_addr, hostname, sizeof(hostname), NULL, 0, NI_NAMEREQD );
	if (e)
		return ret;

	ret = hostname;
	return ret;
}

// will this work for ipv6?
// 1) maybe... even probably.
// 2) i don't care

bool verify_name_has_ip(std::string name, condor_sockaddr addr){
	std::vector<condor_sockaddr> addrs;

	addrs = resolve_hostname(name);
	if (IsDebugVerbose(D_SECURITY)) {
		// print the list of addrs that we are comparing against
		std::string ips_str; ips_str.reserve(addrs.size()*40);
		for(unsigned int i = 0; i < addrs.size(); i++) {
			ips_str += "\n\t";
			ips_str += addrs[i].to_ip_string();
		}
		dprintf(D_SECURITY|D_VERBOSE, "IPVERIFY: checking %s against %s addrs are:%s\n",
				name.c_str(), addr.to_ip_string().c_str(), ips_str.c_str());
	}
	for(unsigned int i = 0; i < addrs.size(); i++) {
		// compare std::string
		// addr.to_ip_string
		if(addrs[i].to_ip_string() == addr.to_ip_string()) {
			dprintf(D_SECURITY, "IPVERIFY: for %s matched %s to %s\n", name.c_str(), addrs[i].to_ip_string().c_str(), addr.to_ip_string().c_str());
			return true;
		}
	}
	return false;
}

std::vector<std::string> get_hostname_with_alias(const condor_sockaddr& addr)
{
	std::vector<std::string> prelim_ret;
	std::vector<std::string> actual_ret;

	std::string hostname = get_hostname(addr);
	if (hostname.empty())
		return prelim_ret;

	// we now start to construct a list (prelim_ret) of the hostname and all
	// the aliases.  first the name itself.
	prelim_ret.push_back(hostname);

	if (nodns_enabled())
		// don't need to verify this... the string is actually an IP here
		return prelim_ret; // no need to call further DNS functions.

	// now, add the aliases

	hostent* ent;
		//int aftype = addr.get_aftype();
		//ent = gethostbyname2(hostname.c_str(), addr.get_aftype());

		// really should call gethostbyname2() however most platforms do not
		// support. (Solaris, HP-UX, IRIX)
		// complete DNS aliases can be only obtained by gethostbyname.
		// however, what happens if we call it in IPv6-only system?
		// can we get DNS aliases for the hostname that only contains
		// IPv6 addresses?
	ent = gethostbyname(hostname.c_str());

	if (ent) {
		char** alias = ent->h_aliases;
		for (; *alias; ++alias) {
			prelim_ret.push_back(std::string(*alias));
		}
	}

	// WARNING! there is a reason this is implimented as two separate loops,
	// so please don't try to combine them.
	//
	// calling verify_name_has_ip() will potentially overwrite static data that
	// is referred to by ent->h_aliases (man 3 gethostbyname, see notes).  so
	// first, we push the name and then all aliases into the std::string vector
	// prelim_ret, and then verify them in the following loop.

	for (unsigned int i = 0; i < prelim_ret.size(); i++) {
		if(verify_name_has_ip(prelim_ret[i], addr)) {
			actual_ret.push_back(prelim_ret[i]);
		} else {
			dprintf(D_ALWAYS, "WARNING: forward resolution of %s doesn't match %s!\n",
					prelim_ret[i].c_str(), addr.to_ip_string().c_str());
		}
	}

	return actual_ret;
}

// look up FQDN for hostname and aliases.
// if not, it adds up DEFAULT_DOMAIN_NAME
std::string get_full_hostname(const condor_sockaddr& addr)
{
		// this function will go smooth even with NODNS.
	std::string ret;
	std::vector<std::string> hostnames = get_hostname_with_alias(addr);
	if (hostnames.empty()) return ret;
	std::vector<std::string>::iterator iter;
	for (iter = hostnames.begin(); iter != hostnames.end(); ++iter) {
		std::string& str = *iter;
		if (str.find('.') != std::string::npos) {
			return str;
		}
	}

	std::string default_domain;
	if (param(default_domain, "DEFAULT_DOMAIN_NAME")) {
			// first element is the hostname got by gethostname()
		ret = *hostnames.begin();
		if (default_domain[0] != '.')
			ret += ".";
		ret += default_domain;
	}
	return ret;
}

std::vector<condor_sockaddr> resolve_hostname(const std::string& hostname, std::string* canonical)
{
	std::vector<condor_sockaddr> ret;
	if (nodns_enabled()) {
		condor_sockaddr addr = convert_fake_hostname_to_ipaddr(hostname);
		if (addr != condor_sockaddr::null) {
			ret.push_back(addr);
			if (canonical) *canonical = hostname;
		}
		return ret;
	}
	return resolve_hostname_raw(hostname, canonical);
}

std::vector<condor_sockaddr> resolve_hostname_raw(const std::string& hostname, std::string* canonical) {
	//
	// For now, we can just document that you need the Punycoded DSN name.
	// If somebody complains about that, we can revisit this.  (If we
	// assume/require UTF-8 (int the config file), we can still readily
	// distinguish Sinfuls, since ':' and '?' won't be used by the UTF-8
	// IDNs.)
	//
	std::vector<condor_sockaddr> ret;
	for( size_t i = 0; i < hostname.length(); ++i ) {
		if( isalnum( hostname[i] ) || hostname[i] == '-' ) { continue; }
		if( hostname[i] == '.' && i + 1 < hostname.length()
			&& hostname[i+1] != '.' ) { continue; }

		dprintf( D_HOSTNAME, "resolve_hostname_raw(): argument '%s' is not a valid DNS name, returning no addresses.\n", hostname.c_str() );
		return ret;
	}

	addrinfo* info = nullptr;
	int res  = ipv6_getaddrinfo(hostname.c_str(), NULL, info);
	if (res) {
		dprintf(D_HOSTNAME, "ipv6_getaddrinfo() could not look up %s: %s (%d)\n", 
			hostname.c_str(), gai_strerror(res), res);
		return ret;
	}

	if (canonical && info->ai_canonname) {
		*canonical = info->ai_canonname;
	}

	addrinfo* next = info;
	do {
		// Ignore non-IP address families
		if (next->ai_family == AF_INET || next->ai_family == AF_INET6) {
			ret.emplace_back(next->ai_addr);
		}
	} while ( (next = next->ai_next) );

	// Sort IPv6 link-local addresses to the end of the list.
	// Optionally sort IPv4 addresses to the top of the list.
	bool prefer_ipv4 = false;
	bool have_preference = param_boolean("IGNORE_DNS_PROTOCOL_PREFERENCE", true);
	if (have_preference) {
		prefer_ipv4 = param_boolean("PREFER_OUTBOUND_IPV4", true);
	}
	auto ip_sort = [=](const condor_sockaddr& a, const condor_sockaddr& b) {
		if ((a.is_ipv4() || !a.is_link_local()) && b.is_ipv6() && b.is_link_local()) {
			return false;
		}
		if (have_preference && a.is_ipv4() != b.is_ipv4()) {
			return prefer_ipv4 == a.is_ipv4();
		}
		return false;
	};
	std::sort(ret.begin(), ret.end(), ip_sort);

	freeaddrinfo(info);
	return ret;
}

std::string convert_ipaddr_to_fake_hostname(const condor_sockaddr& addr)
{
	std::string ret;
	std::string default_domain;
	if (!param(default_domain, "DEFAULT_DOMAIN_NAME")) {
		dprintf(D_ALWAYS,
				"NO_DNS: DEFAULT_DOMAIN_NAME must be defined in your "
				"top-level config file\n");
		return ret;
	}

	ret = addr.to_ip_string();
	for (size_t i = 0; i < ret.length(); ++i) {
		if (ret[i] == '.' || ret[i] == ':')
			ret[i] = '-';
	}
	ret += ".";
	ret += default_domain;

	// Hostnames can't begin with -, as per RFC 1123
	// ipv6 zero-compression could cause this, esp. for the loopback addr
	if (ret[0] == '-') {
		ret = "0" + ret;
	}

	return ret;
}

// Upon failure, return condor_sockaddr::null
condor_sockaddr convert_fake_hostname_to_ipaddr(const std::string& fullname)
{
	std::string hostname;
	std::string default_domain;
	bool truncated = false;
	if (param(default_domain, "DEFAULT_DOMAIN_NAME")) {
		std::string dotted_domain = ".";
		dotted_domain += default_domain;
		size_t pos = fullname.find(dotted_domain);
		if (pos != std::string::npos) {
			truncated = true;
			hostname = fullname.substr(0, pos);
		}
	}
	if (!truncated)
		hostname = fullname;

	// detects if hostname is IPv6
	//
	// hostname is NODNS coded address
	//
	// for example,
	// it could be 127-0-0-1 (127.0.0.1) as IPv4 address
	// it could be fe80-3577--1234 ( fe80:3577::1234) as IPv6 address
	//
	// it is IPv6 address
	// 1) if there are 7 '-'
	// 2) if there are '--' which means compaction of zeroes in IPv6 adress

	char target_char;
	bool ipv6 = false;
	if (hostname.find("--") != std::string::npos)
		ipv6 = true;
	else {
		int dash_count = 0;
		for (size_t i = 0; i < hostname.length(); ++i)
			if (hostname[i] == '-')
				++dash_count;

		if (dash_count == 7)
			ipv6 = true;
	}

	if (ipv6)
		target_char = ':';
	else
		target_char ='.';
		// converts hostname to IP address string
	for (size_t i = 0; i < hostname.length(); ++i) {
		if (hostname[i] == '-')
			hostname[i] = target_char;
	}

	condor_sockaddr ret;
	if (ret.from_ip_string(hostname)) {
		return ret;
	} else {
		return condor_sockaddr::null;
	}
}
