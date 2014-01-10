
#include "condor_common.h"

#include <classad/classad.h>
#include <classad/source.h>
#include <classad/sink.h>
#include "condor_debug.h"

#include "lark_attributes.h"
#include "dhcp_address.h"
#include "dhcp_management.h"
#include "network_namespaces.h"

using namespace lark;

bool
DHCPAddressSelection::LeaseHasRemaining(classad::ClassAd &ad, unsigned remaining)
{
	time_t lease_start, lease_lifetime;
	if (ad.EvaluateAttrInt(ATTR_DHCP_LEASE_START, lease_start) && ad.EvaluateAttrInt(ATTR_DHCP_LEASE, lease_lifetime))
	{
		time_t now = time(NULL);
		if (lease_start + lease_lifetime - now > remaining)
		{
			return true;
		}
	}
	return false;
}

int
DHCPAddressSelection::SelectAddresses()
{
	m_ad->InsertAttr(ATTR_INTERNAL_ADDRESS_IPV4, "0.0.0.0");
	m_ad->InsertAttr(ATTR_EXTERNAL_ADDRESS_IPV4, "0.0.0.0");
	return 0;
}

int
DHCPAddressSelection::Setup()
{
	std::string internal_iface;
	int fd;
	if (m_ad->EvaluateAttrString(ATTR_INTERNAL_INTERFACE, internal_iface) && (fd = NetworkNamespaceManager::OpenLarkLock("/dhcp_leases", internal_iface)) >= 0)
	{
		FILE * fp = fdopen(fd, "r");
		classad::ClassAdParser parser;
		classad::ClassAd *ad = parser.ParseClassAd(fp);
		close(fd);
		if (ad && LeaseHasRemaining(*ad, 0))
		{
			dprintf(D_FULLDEBUG, "Using DHCP address from cache.\n");
			m_ad->Update(*ad);
			m_use_address_from_cache = true;
			return 0;
		}
	}

	if (dhcp_query(*m_ad)) {
		return 1;
	}
	return 0;
}

int
DHCPAddressSelection::SetupPostFork()
{
	if (!m_use_address_from_cache && dhcp_commit(*m_ad)) {
		return 1;
	}
	return 0;
}

#define COPY_ATTR(x) { \
	classad::ExprTree *expr = m_ad->Lookup(x); \
	if (expr) \
	{ \
		classad::ExprTree *expr_copy = expr->Copy(); \
		if (expr_copy) \
		{ \
			ad.Insert(x, expr_copy); \
		} \
	} \
}

int
DHCPAddressSelection::Cleanup()
{
	bool wrote_lease = false;
	if (LeaseHasRemaining(*m_ad, 10*60))
	{
		classad::ClassAd ad;
		COPY_ATTR(ATTR_DHCP_SERVER)
		COPY_ATTR(ATTR_DHCP_GATEWAY)
		COPY_ATTR(ATTR_DHCP_LEASE)
		COPY_ATTR(ATTR_DHCP_LEASE_START)
		COPY_ATTR(ATTR_DHCP_TXID)
		COPY_ATTR(ATTR_DHCP_MAC)
		COPY_ATTR(ATTR_GATEWAY)
		COPY_ATTR(ATTR_SUBNET_MASK)
		COPY_ATTR(ATTR_INTERNAL_ADDRESS_IPV4)
		std::string internal_iface;
		int fd;
		if (m_ad->EvaluateAttrString(ATTR_INTERNAL_INTERFACE, internal_iface) && (fd = NetworkNamespaceManager::OpenLarkLock("/dhcp_leases", internal_iface)) >= 0)
		{
			std::string contents;
			classad::PrettyPrint unparser;
			unparser.Unparse(contents, &ad);
			if (full_write(fd, contents.c_str(), contents.size()) != static_cast<int>(contents.size()))
			{
				dprintf(D_ALWAYS, "Error writing out DHCP lease (errno=%d, %s).\n", errno, strerror(errno));
			}
			else
			{
				wrote_lease = true;
			}
			close(fd);
		}
	}
	if (!wrote_lease)
	{
		dprintf(D_FULLDEBUG, "Releasing DHCP address.\n");
		if (dhcp_release(*m_ad)) {
			return 1;
		}
	}
	return 0;
}

