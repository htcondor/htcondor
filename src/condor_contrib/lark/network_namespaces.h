
/*
    network_namespaces.h

    There's three essential pieces to network namespaces:
      * The outer-script: this is executed by the starter prior to doing anything else.
        It takes two arguments: a unique job identifier, and a device name.  The script
        should assume the device name is one end of a network pipe; the other end is the
        only network device available to the job.  Routing and IP should be hooked up
        appropriately.  The script should setup an iptables chain with the same name as
        the job identifier.  This chain will be used for accounting.  This script should 
        print an IP address to stdout; this IP address will be used for the internal
        side of the pipe.
      * daemon_core code: After the child forks, but before it execs, the parent will
        assign the internal network pipe to the namespace, and the child will assign
        the pipe the IP address.
      * Job accounting: Given the name of the chain, the current byte count for all
        the associated rules in the chain are inserted into the job's classad and
        sent to the schedd for the next update.
 */

#ifndef __NETWORK_NAMESPACES_H
#define __NETWORK_NAMESPACES_H

#include "condor_common.h"

#include <string>
#include <sys/types.h>
#include <memory>

#include "classad/classad.h"
#include "condor_sockaddr.h"
#include "NetworkPluginManager.h"

#include "network_configuration.h"

#define ATTR_NETWORK_ACCOUNTING "LarkNetworkAccounting"
#define CONFIG_NETWORK_ACCOUNTING "LARK_NETWORK_ACCOUNTING"

namespace lark {

class NetworkNamespaceManager : public NetworkManager {

public:

	int PrepareNetwork(const std::string &uniq_namespace, const classad::ClassAd& job_ad, classad_shared_ptr<classad::ClassAd> machine_ad);

	/*
	 * Functions to invoke for creating the child namespace
	 * These synchronize internally, but the parent and child
	 * must be overlapping.
	 * - pid: the PID of the child process.
	 */
	int PreFork();
	int PostForkParent(pid_t pid);
	int PostForkChild();

	/*
	 * Perform any network accounting for this namespace.
	 * - classad: Reference to a classad to insert network accounting into.
     * - jobphase: string to indicate which phase job is in (stage_in, execution or stage_out)
	 */
	int PerformJobAccounting(classad::ClassAd *classad, const std::string &);

	/*
	 * Cleanup any persistent OS structures created by the manager.
	 */
	int Cleanup(const std::string &);

	/*
	 * Returns the netlink kernel for talking to the kernel.
	 */
	int GetNetlinkSocket() const {return m_sock;};

	/*
	 * Get the current instance of the NetworkNamespaceManager
	 */
	static NetworkNamespaceManager &GetManager();

	static int OpenLarkLock(const std::string &dirname, const std::string &filename);
private:
	NetworkNamespaceManager();

	int ConfigureNetworkAccounting(const classad::ClassAd &machine_ad);

	void RecordNewMac(const std::string &interface_name);
	bool FetchMac(const std::string &interface_name, unsigned char *mac);

	int CreateNetworkPipe();
	int RunCleanupScript();
	static int JobAccountingCallback(const unsigned char * rule_name, long long bytes, void * callback_data);

	enum NAMESPACE_STATE {
		UNCREATED,
		CREATED,
		PASSED,
		EXECUTING,
		FAILED,
		CLEANED
	} m_state;
	std::string m_network_namespace;
	std::string m_internal_pipe;
	std::string m_external_pipe;
	condor_sockaddr m_internal_address, m_external_address;
	std::string m_internal_address_str;
	int m_sock;
	bool m_created_pipe;
	classad::ClassAd m_statistics;

	// Synchronization pipes.
	int m_p2c[2], m_c2p[2];

	std::auto_ptr<NetworkConfiguration> m_network_configuration;

	classad_shared_ptr<classad::ClassAd> m_ad;
	classad::ClassAd m_job_ad;

	// Singleton instance
	static NetworkNamespaceManager *m_instance;

	class NetworkLock {
		public:
			NetworkLock();
			~NetworkLock();
		private:
			int m_fd;
	};
};

}

#endif

