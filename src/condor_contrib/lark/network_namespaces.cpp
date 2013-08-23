
#include "condor_common.h"
#include "condor_config.h"
#include "condor_arglist.h"
#include "directory.h"

#include <classad/classad.h>
#include <classad/classad_stl.h>

#include "lark_attributes.h"
#include "network_namespaces.h"
#include "network_manipulation.h"
#include "network_adapter.unix.h"
#include "popen_util.h"

using namespace lark;

NetworkNamespaceManager * NetworkNamespaceManager::m_instance;

NetworkNamespaceManager::NetworkNamespaceManager() :
	m_state(UNCREATED), m_network_namespace(""),
	m_internal_pipe(""), m_external_pipe(""),
	m_sock(-1), m_created_pipe(false),
	m_network_configuration(NULL), m_ad()
	{
		dprintf(D_FULLDEBUG, "Initialized a NetworkNamespaceManager plugin.\n");
	}

NetworkNamespaceManager&
NetworkNamespaceManager::GetManager()
{
	static NetworkNamespaceManager instance;
	if (m_instance == NULL) {
		m_instance = &instance;
	}
	return *m_instance;
}

int NetworkNamespaceManager::PrepareNetwork(const std::string &uniq_namespace, const classad::ClassAd & job_ad, classad_shared_ptr<classad::ClassAd> machine_ad) {
	NetworkLock sentry;

	if ((m_state != UNCREATED) && (m_state != CLEANED)) {
		dprintf(D_FULLDEBUG, "Internal bug: NetworkNamespaceManager::PrepareNetwork has already been invoked (state=%d).\n", m_state);
		m_state = FAILED;
		return 1;
	}
	dprintf(D_FULLDEBUG, "Preparing network namespace manager.\n");

	if (!machine_ad.get()) {
		m_state = FAILED;
		return 1;
	}
	m_ad = machine_ad;
	m_job_ad.CopyFrom(job_ad);

	if (uniq_namespace.size() >= IF_NAMESIZE) {
		// I probably can take IF_NAMESIZE bytes, but I'm being cautious here to avoid
		// nul-terminated-buffer issues.
		// -3 because we add the "e_" and "i_" prefixes later to the slot names.
		m_network_namespace = uniq_namespace.substr(0, IF_NAMESIZE-3);
	} else {
		m_network_namespace = uniq_namespace;
	}
	machine_ad->InsertAttr(ATTR_IPTABLE_NAME, m_network_namespace);

	m_internal_pipe = "i_" + m_network_namespace;
	machine_ad->InsertAttr(ATTR_INTERNAL_INTERFACE, m_internal_pipe);
	m_external_pipe = "e_" + m_network_namespace;
	machine_ad->InsertAttr(ATTR_EXTERNAL_INTERFACE, m_external_pipe);

	if ((m_sock = create_socket()) < 0) {
		dprintf(D_ALWAYS, "Unable to create a socket to talk to the kernel for network namespaces.\n");
		m_state = FAILED;
		return 1;
	}

	int rc = 0;

	if ((rc = CreateNetworkPipe())) {
		dprintf(D_ALWAYS, "Unable to create a new set of network pipes; cannot create a namespace.\n");
		m_state = FAILED;
		return rc;
	}

	// Bootstrap network configuration
	m_network_configuration.reset(NetworkConfiguration::GetNetworkConfiguration(machine_ad));
	if (!m_network_configuration.get()) {
		dprintf(D_ALWAYS, "Unable to create network configuration.\n");
		m_state = FAILED;
		return 1;
	}

	if (m_network_configuration->SelectAddresses()) {
		dprintf(D_ALWAYS, "Failed to setup network addresses.\n");
		m_state = FAILED;
		return 1;
	}

	// Create a chain for this starter.  Equivalent to:
	// iptables -N $LarkIptableName
	{
		ArgList args;
		args.AppendArg("iptables");
		args.AppendArg("-N");
		args.AppendArg(m_network_namespace.c_str());
		dprintf(D_FULLDEBUG, "NetworkNamespaceManager iptables chain setup: iptables -N %s\n", m_network_namespace.c_str());
		NAMESPACE_STATE prior_state = m_state;
		m_state = FAILED;
		RUN_ARGS_AND_LOG(NetworkNamespaceManager::CreateNamespace, iptables)
		m_state = prior_state;
	}

	// Configure the network
	if (m_network_configuration->Setup()) {
		dprintf(D_ALWAYS, "Network configuration failed.\n");
		m_state = FAILED;
		return 1;
	}

	// Copy addresses from the machine classad to this class.
	// TODO: Handle IPv6 addresses.
	if (!machine_ad->EvaluateAttrString(ATTR_INTERNAL_ADDRESS_IPV4, m_internal_address_str)) {
		dprintf(D_ALWAYS, "Network configuration did not result in a valid internal IPv4 address.\n");
		m_state = FAILED;
		return 1;
	}
	m_internal_address.from_ip_string(m_internal_address_str.c_str());
	std::string external_address_str;
	if (!machine_ad->EvaluateAttrString(ATTR_EXTERNAL_ADDRESS_IPV4, external_address_str)) {
		dprintf(D_ALWAYS, "Network configuration did not result in a valid external IPv4 address.\n");
		m_state = FAILED;
		return 1;
	}
	m_external_address.from_ip_string(external_address_str.c_str());


	// Configure some services provided the manager itself.
	if (ConfigureNetworkAccounting(*machine_ad)) {
		dprintf(D_ALWAYS, "Network accounting configuration failed.\n");
		m_state = FAILED;
		return 1;
	}

	// Run admin-provided configuration script.
	const char * namespace_script = param("NETWORK_NAMESPACE_CREATE_SCRIPT");
	if (namespace_script) {
		ArgList args;
		std::string external_address = m_external_address.to_ip_string();
		args.AppendArg(namespace_script);
		args.AppendArg(external_address.c_str());
		args.AppendArg(m_internal_address_str.c_str());
		args.AppendArg(m_network_namespace);
		args.AppendArg(m_external_pipe);
		dprintf(D_FULLDEBUG, "NetworkNamespaceManager admin setup script: %s %s %s %s %s\n",
			namespace_script, external_address.c_str(), m_internal_address_str.c_str(), m_network_namespace.c_str(), m_external_pipe.c_str());

		NAMESPACE_STATE prior_state = m_state;
		m_state = FAILED;
		FILE *fp = my_popen(args, "w", TRUE);
		if (fp == NULL) {
			dprintf(D_ALWAYS, "NetworkNamespaceManager::CreateNamespace: my_popen failure on %s: (errno=%d) %s\n", namespace_script, errno, strerror(errno));
			return 1;
		}
		// TODO: log output of this script.  Probably means we can't use popen anymore
		// TODO: Factor out the setup script into a separate method.
		classad::ClassAdUnParser pp;
		std::string job_ad_str;
		pp.Unparse(job_ad_str, &job_ad);
		// Ignore EOF and errors - the script may ignore the stdin and return prior to 
		// us writing the complete ad.
		fputs(job_ad_str.c_str(), fp);
		fputs("\n------\n", fp);
		std::string machine_ad_str;
		pp.Unparse(machine_ad_str, machine_ad.get());
		fputs(machine_ad_str.c_str(), fp);
		fputs("\n", fp);
		int status = my_pclose(fp);
		if (status == -1) {
			dprintf(D_ALWAYS, "NetworkNamespaceManager::CreateNamespace: Error when running %s (errno=%d) %s\n", namespace_script, errno, strerror(errno));
		} else {
			if (WIFEXITED(status)) {
				if ((status = WEXITSTATUS(status))) {
					dprintf(D_ALWAYS, "NetworkNamespaceManager::CreateNamespace: %s exited with status %d\n", namespace_script, status);
					return status;
				} else {
					dprintf(D_FULLDEBUG, "NetworkNamespaceManager::CreateNamespace: %s was successful.\n", namespace_script);
				}
			} else if (WIFSIGNALED(status)) {
				status = WTERMSIG(status);
				dprintf(D_ALWAYS, "NetworkNamespaceManager::CreateNamespace: %s terminated with signal %d\n", namespace_script, status);
			}
		}
		m_state = prior_state;
	} else {
		dprintf(D_FULLDEBUG, "Parameter NETWORK_NAMESPACE_CREATE_SCRIPT is not specified.\n");
	}

	m_state = CREATED;
	return 0;
}

bool NetworkNamespaceManager::FetchMac(const std::string &interface_name, unsigned char *mac)
{
	int fd = OpenLarkLock("/mac_cache", interface_name);
	if (fd == -1)
	{
		return false;
	}
	char mac_char[18]; mac_char[17] = '\0';
	int retval;
	if ((retval = full_read(fd, mac_char, 17)) < 0)
	{
		dprintf(D_ALWAYS, "Unable to read mac from cache (errno=%d, %s).\n", errno, strerror(errno));
		close(fd);
		return false;
	}
	mac_char[retval] = '\0';
	close(fd);
	unsigned char mac_bin_char[8];
	if (!MacAddressHexToBin(mac_char, mac_bin_char))
	{
		dprintf(D_ALWAYS, "Unable to parse the cached mac address.\n");
		return false;
	}
	mac_bin_char[7] = '\0';
	memcpy(mac, mac_bin_char, 8);
	return true;
}

void NetworkNamespaceManager::RecordNewMac(const std::string &interface_name)
{
	int fd;
	struct ifreq iface_req;

	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
	{
		dprintf(D_ALWAYS, "Failed to create socket for retrieving hardware address (errno=%d, %s).\n", errno, strerror(errno));
		return;
	}

	memset(&iface_req, '\0', sizeof(iface_req));
	strncpy(iface_req.ifr_name, interface_name.c_str(), IFNAMSIZ);
	if (-1 == ioctl(fd, SIOCGIFHWADDR, &iface_req))
	{
		dprintf(D_ALWAYS, "Error when retrieving hardware address (errno=%d, %s).\n", errno, strerror(errno));
		close(fd);
		return;
	}
	close(fd);

	char hw_addr_str[18]; hw_addr_str[17] = '\0';
	unsigned char hw_addr[8]; hw_addr[6] = '\0';
	memcpy(hw_addr, iface_req.ifr_hwaddr.sa_data, 6);
	MacAddressBinToHex(hw_addr, hw_addr_str);

	if ((fd = NetworkNamespaceManager::OpenLarkLock("/mac_cache", interface_name)) < 0)
	{
		dprintf(D_ALWAYS, "Error opening lock file $(LOCK)/lark/mac_cache/%s. (errno=%d, %s)\n", interface_name.c_str(), errno, strerror(errno));
	}
	if (full_write(fd, hw_addr_str, strlen(hw_addr_str)) < 0)
	{
		dprintf(D_ALWAYS, "Error writing MAC %s to lock file $(LOCK)/lark/mac_cache/%s. (errno=%d, %s)\n", hw_addr_str, interface_name.c_str(), errno, strerror(errno));
	}
	close(fd);
}

int NetworkNamespaceManager::CreateNetworkPipe() {
	int rc;

	m_internal_pipe = "i_" + m_network_namespace;

	std::vector<unsigned char> e_mac, i_mac; e_mac.reserve(8); i_mac.reserve(8);
	unsigned char *e_mac_char = FetchMac(m_external_pipe, &(e_mac[0])) ? &(e_mac[0]) : NULL;
	unsigned char *i_mac_char = FetchMac(m_internal_pipe, &(i_mac[0])) ? &(i_mac[0]) : NULL;
        if ((rc = create_veth(m_sock, m_external_pipe.c_str(), m_internal_pipe.c_str(), e_mac_char, i_mac_char)))
	{
		if (rc == EEXIST)
		{
			int rc2;
			if ((rc2 = delete_veth(m_sock, m_external_pipe.c_str()))  && (rc2 != ENODEV) && (rc2 != EINVAL)) {
				dprintf(D_ALWAYS, "Failed to delete the veth interface; rc=%d\n", rc);
				m_state = FAILED;
				return rc2;
			}
			rc = create_veth(m_sock, m_external_pipe.c_str(), m_internal_pipe.c_str(), e_mac_char, i_mac_char);
		}
		if (rc)
		{
			dprintf(D_ALWAYS, "Failed to create veth devices %s/%s.\n", m_external_pipe.c_str(), m_internal_pipe.c_str());
			m_state = FAILED;
			return rc;
		}
        }
	m_created_pipe = true;

	if (!e_mac_char)
	{
		RecordNewMac(m_external_pipe);
	}
	if (!i_mac_char)
	{
		RecordNewMac(m_internal_pipe);
	}

	dprintf(D_FULLDEBUG, "Created a pair of veth devices (%s, %s).\n", m_external_pipe.c_str(), m_internal_pipe.c_str());
        
	return 0;
}

int NetworkNamespaceManager::PreFork() {
	NetworkLock sentry;
	if (m_state == UNCREATED || m_state == EXECUTING)
		return 0;

	if ((pipe(m_p2c) < 0) || (pipe(m_c2p) < 0)) {
		dprintf(D_ALWAYS, "NetworkNamespaceManager: pipe() failed with %s (errno=%d).\n", strerror(errno), errno);
		return -1;
	}
	if ((fcntl(m_p2c[0], F_SETFD, FD_CLOEXEC) < 0) || (fcntl(m_c2p[1], F_SETFD, FD_CLOEXEC) < 0)) {
		dprintf(D_ALWAYS, "NetworkNamespaceManager: fcntl() failed with %s (errno=%d).\n", strerror(errno), errno);
		return -1;
	}
	return 0;
}

int NetworkNamespaceManager::PostForkChild() {
	std::string external_gw;

	classad::PrettyPrint pp;
	std::string ad_str;
	pp.Unparse(ad_str, m_ad.get());
	dprintf(D_FULLDEBUG, "Child network classad: \n%s\n", ad_str.c_str());

	if (m_state == UNCREATED || m_state == EXECUTING)
		return 0;

	// Close the end of the pipes that aren't ours
	close(m_p2c[1]);
	close(m_c2p[0]);

	// Call unshare, and tell the parent to proceed.
	// If there's an error, simply return non-zero; the parent will get an EPIPE.
	int rc, rc2;

	rc = ::unshare(CLONE_NEWNET);
	if (rc) {
		dprintf(D_ALWAYS, "Failed to unshare the network namespace\n");
		return errno;
	}

	while (((rc2 = write(m_c2p[1], &rc, sizeof(rc))) < 0) && (errno == EINTR)) {}
	if (rc2 < 0) {
		dprintf(D_ALWAYS, "Error communicating with child: %s (errno=%d).\n", strerror(errno), errno);
		return errno;
	}


	// Wait until the parent indicates it's OK to proceed
	while (((rc2 = read(m_p2c[0], &rc, sizeof(rc))) < 0) && (errno == EINTR)) {}
	if (rc2 == -1) {
		dprintf(D_ALWAYS, "Error reading from parent: %s (errno=%d).\n", strerror(errno), errno);
		return errno;
	}
	if (rc != 0) {
		dprintf(D_ALWAYS, "Got an error from the parent: %d.\n", rc);
		return rc;
	}

	// Perform the child-side of the network setup.
	//

        //
	// Manipulate our network configuration in the child.
	// Notice that we open a new socket to the kernel - this is because the
	// other socket still talks to the original namespace.
	//
	// Note: Because we may be in a shared-memory clone, do NOT modify the heap.
	// This is why we saved the IPv4 address in m_internal_address_str instead of just
	// recreating it from m_internal_address
	int sock;
	dprintf(D_FULLDEBUG, "Child proceeding to configure networking for address %s.\n", m_internal_address_str.c_str());
	if ((sock = create_socket()) < 0) {
		dprintf(D_ALWAYS, "Unable to create socket to talk to kernel for child.\n");
		rc = 1;
		goto failed_socket;
	}
	// TODO: This hardcodes the prefix to 24; however, at least in the DHCP case, we can
	// better determine the correct prefix.
	if (add_address(sock, m_internal_address_str.c_str(), 24, m_internal_pipe.c_str())) {
		dprintf(D_ALWAYS, "Unable to add address %s to %s.\n", m_internal_address_str.c_str(), m_internal_pipe.c_str());
		rc = 2;
		goto finalize_child;
	}
	dprintf(D_FULLDEBUG, "Added address %s to child interface.\n", m_internal_address_str.c_str());
	// This doesn't seem to be necessary if you provide a non-/32 netmask when creating the device.
	/*if (add_local_route(sock, m_internal_address_str.c_str(), m_internal_pipe.c_str(), 24)) {
		dprintf(D_ALWAYS, "Unable to add local route via %s\n", m_internal_address_str.c_str());
		rc = 4;
		goto finalize_child;
	}*/
	if (set_status(sock, m_internal_pipe.c_str(), IFF_UP)) {
		dprintf(D_ALWAYS, "Unable to bring up interface %s.\n", m_internal_pipe.c_str());
		rc = 3;
		goto finalize_child;
	}

	// Gateway setup is done in the network configuration code, as it is substantially
	// different between NAT and bridge configurations.

	m_sock = sock; // This way, the network configuration can reuse our socket.
	if (m_network_configuration->SetupPostForkChild()) {
		dprintf(D_ALWAYS, "Failed to create network configuration.\n");
		rc = 6;
		goto finalize_child;
	}

	if (set_status(sock, m_internal_pipe.c_str(), IFF_UP)) {
		dprintf(D_ALWAYS, "Unable to bring up interface %s post-config.\n", m_internal_pipe.c_str());
		rc = 7;
		goto finalize_child;
	}

	m_state = EXECUTING;

	// Note we don't write anything to the parent, on success or failure.
	// This is because the parent must wait until the child exits, and the fact
	// m_c2p[1] is marked FD_CLOEXEC will cause an EPIPE to be recieved by the parent,
	// the sign of success.
	// The "normal" Create_Process communication pipe takes care of the error case.

finalize_child:
	close(sock);
failed_socket:
	return rc;

}

int NetworkNamespaceManager::PostForkParent(pid_t pid) {
	NetworkLock sentry;
	if (m_state == UNCREATED || m_state == EXECUTING)
		return 0;

        // Close the end of the pipes that aren't ours
	close(m_p2c[0]);
	close(m_c2p[1]);

	// Wait for the child to indicate we can proceed (child must call unshare())
	int rc2, rc=0;
	while (((rc2 = read(m_c2p[0], &rc, sizeof(rc))) < 0) && (errno == EINTR)) {}
	if (rc2 == -1) {
		if (errno == EPIPE) {
			// No log: the normal error-fighting mechanisms will log something.
			return 1;
		} else  {
			dprintf(D_ALWAYS, "Error reading from child fd %d: %s (errno=%d).\n", m_c2p[0], strerror(errno), errno);
			return errno;
		}
	}
	if (rc != 0) {
		dprintf(D_ALWAYS, "Got an error from the child: %d.\n", rc);
		return rc;
	}

	if (m_state != CREATED) {
		dprintf(D_ALWAYS, "NetworkNamespaceManager in incorrect state %d to send device to internal namespace\n", m_state);
		m_state = FAILED;
		return 1;
	}
	
	if ((rc = set_netns(m_sock, m_internal_pipe.c_str(), pid))) {
		dprintf(D_ALWAYS, "Failed to send %s to network namespace %d.\n", m_internal_pipe.c_str(), pid);
		m_state = FAILED;
		return 1;
	}

	m_state = PASSED;

	// Advance automatically, in case we didn't use clone.
	m_state = EXECUTING;

	// Inform the child it can advance.
	if (rc == 0) {
		while (((rc2 = write(m_p2c[1], &rc, sizeof(rc))) < 0) && (errno == EINTR)) {}
		if (rc2 < 0) {
			dprintf(D_ALWAYS, "Error communicating with child: %s (errno=%d).\n", strerror(errno), errno);
			rc = rc2;
		}
	}

	if ((rc = m_network_configuration->SetupPostForkParent())) {
		dprintf(D_ALWAYS, "Failed to configure network post-fork in parent %d.\n", rc);
		m_state = FAILED;
		return 1;
	}

	// Wait until the child's exec or error.
	if (rc == 0) {
		while (((rc2 = read(m_c2p[0], &rc, sizeof(rc))) < 0) && (errno == EINTR)) {}
		if (rc2 > 0) {
			dprintf(D_ALWAYS, "Got error code from child: %d\n", rc);
		} else if ((rc2 == -1) && (errno) && (errno != EPIPE)) {
			dprintf(D_ALWAYS, "Error reading from child: %s (errno=%d).\n", strerror(errno), errno);
			rc = errno;
		}
	}

	return rc;

}

int NetworkNamespaceManager::PerformJobAccounting(classad::ClassAd *classad, const std::string & jobphase) {
	NetworkLock sentry;
	if (m_state == UNCREATED)
		return 0;

	int rc = 0;
	if (m_state == EXECUTING) {
        std::string prev_jobphase_attr("PreviousJobPhase");
        std::string prev_jobphase;
        std::string cur_jobphase_attr("CurrentJobPhase");
        std::string cur_jobphase(jobphase);
        if(! m_statistics.EvaluateAttrString(prev_jobphase_attr, prev_jobphase)) {
            prev_jobphase = "stage_in";
            m_statistics.InsertAttr(prev_jobphase_attr, prev_jobphase);
        }
        m_statistics.InsertAttr(cur_jobphase_attr, cur_jobphase);
		dprintf(D_FULLDEBUG, "Polling netfilter for network statistics on chain %s.\n", m_network_namespace.c_str());
		rc = perform_accounting(m_network_namespace.c_str(), JobAccountingCallback, (void *)&m_statistics);
        struct timeval tv;
        gettimeofday(&tv, NULL);
        double timestamp = (tv.tv_sec)*1000 + (tv.tv_usec)/1000;
        std::string attr_name("PreviousTimestamp");
        m_statistics.InsertAttr(attr_name, timestamp);
        m_statistics.InsertAttr(prev_jobphase_attr, cur_jobphase);

        if(classad) {
            classad->Update(m_statistics);
        }
        
	}
	return rc;
}

int NetworkNamespaceManager::JobAccountingCallback(const unsigned char * rule_name, long long bytes, void * callback_data) {
	classad::ClassAd &classad = *(classad::ClassAd*)callback_data;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    double current_timestamp = (tv.tv_sec)*1000 + (tv.tv_usec)/1000;
    double previous_timestamp;
    double time_interval;
    std::string prev_timestamp_attr("PreviousTimestamp");
    if(! classad.EvaluateAttrReal(prev_timestamp_attr, previous_timestamp)) {
        int initial_update_interval = param_integer("STARTER_INITIAL_UPDATE_INTERVAL", 8);
        time_interval = double(initial_update_interval * 1000);
    } else {
        // Sometimes, the job accounting is called one right after another, the accounting data
        // does not change in that case, thus there is no need to do bandwidth usage calcaulation.
        // We fix it by check the timestamp difference, if it is smaller than a set threshold, we
        // ignore the bandwith usage update.
        time_interval = current_timestamp - previous_timestamp;
    }

    std::string prev_jobphase_attr("PreviousJobPhase");
    std::string prev_jobphase;
    std::string cur_jobphase_attr("CurrentJobPhase");
    std::string cur_jobphase;
    classad.EvaluateAttrString(prev_jobphase_attr, prev_jobphase);
    dprintf(D_FULLDEBUG, "Previous job phase has been evaluated to be %s\n", prev_jobphase.c_str());
    classad.EvaluateAttrString(cur_jobphase_attr, cur_jobphase);
    dprintf(D_FULLDEBUG, "Current job phase has been evaluated to be %s\n", cur_jobphase.c_str());
    
	std::string attr_name("Network");
	attr_name.append((const char *)rule_name);
    double prev_num_bytes;
    if(! classad.EvaluateAttrReal(attr_name, prev_num_bytes)){
        prev_num_bytes = 0;
    }
    std::string average_bandwidth("AverageBandwidthUsage");
    double bandwidth_usage = 0;
    average_bandwidth.append((const char *)rule_name);
    
    std::string file_transfer_download_attr("FileTransferDownload");
    file_transfer_download_attr.append((const char *)rule_name);
    if( !strcmp(prev_jobphase.c_str(), "stage_in") && !strcmp(cur_jobphase.c_str(), "execution") ) {
        classad.InsertAttr(file_transfer_download_attr, prev_num_bytes);
        dprintf(D_FULLDEBUG, "%s has been inserted into job classad with value %f\n", file_transfer_download_attr.c_str(), prev_num_bytes);
    }
    double file_transfer_download_size;
    if( classad.EvaluateAttrReal(file_transfer_download_attr, file_transfer_download_size)) {
        dprintf(D_FULLDEBUG, "Before adding file transfer download data, network accounting data is %lld\n", bytes);
        bytes = bytes + file_transfer_download_size;
        dprintf(D_FULLDEBUG, "After adding file transfer download data, network accounting data is %lld\n", bytes);
    }
    // this threshold is 50 milliseconds
    if(time_interval >= 50) {
        bandwidth_usage = (double(bytes)-prev_num_bytes)/time_interval * 1000; /* bandwidth in bytes/second */
    } else {
        classad.EvaluateAttrReal(average_bandwidth, bandwidth_usage);
    }
    classad.InsertAttr(average_bandwidth, bandwidth_usage);
	classad.InsertAttr(attr_name, double(bytes), classad::Value::B_FACTOR);
    dprintf(D_FULLDEBUG, "The time interval between two consecutive job accounting is %f\n", time_interval);
	dprintf(D_FULLDEBUG, "Network accounting: %s = %lld\n", attr_name.c_str(), bytes);
    dprintf(D_FULLDEBUG, "Network average bandwidth usage: %s = %f bytes/sceond\n", average_bandwidth.c_str(), bandwidth_usage);
	//classad.Assign(attr_name, bytes);
	return 0;
}

int NetworkNamespaceManager::Cleanup(const std::string & name) {
	NetworkLock sentry;
	dprintf(D_FULLDEBUG, "Cleaning up the network namespace %s.\n", name.c_str());

	// Try to only clean once.
	if (m_state == CLEANED) {
		dprintf(D_ALWAYS, "Called Cleanup on an already-cleaned NetworkNamespaceManager!\n");
		return 1;
	}

	if (m_state == UNCREATED) {
		// We never created the namespace.  Do nothing.
		return 0;
	}

	// We always advance to the cleaned state -- assume it works.
	m_state = CLEANED;
	if (!m_created_pipe) {
		// Not much to do in this case.
		return 0;
	}

	// Cleanup network configuration
	int rc4 = 0;
	if (m_network_configuration.get()) {
		rc4 = m_network_configuration->Cleanup();
		if (rc4) {
			dprintf(D_ALWAYS, "Unable to cleanup network configuration.\n");
		}
		m_network_configuration.reset(NULL);
	} else {
		dprintf(D_FULLDEBUG, "No network configuration to cleanup.\n");
	}

        // Cleanup firewall
	cleanup_chain(m_network_namespace.c_str());

	int rc2;
	rc2 = RunCleanupScript();

	if (m_sock < 0) {
		dprintf(D_ALWAYS, "Unable to delete device as socket is invalid.\n");
		return 1;
	}

	int rc = 0, rc3;
	// If the job launched successfully, the pipe may get deleted by the OS
	// automatically.  In such a case, the delete_veth will return with
	// either "no such device" or "invalid value".  Not sure why the kernel
	// gives both - maybe some sort of race?
	if ((rc3 = delete_veth(m_sock, m_external_pipe.c_str()))  && (rc3 != ENODEV) && (rc3 != EINVAL)) {
		dprintf(D_ALWAYS, "Failed to delete the veth interface; rc=%d\n", rc);
		rc = rc3;
	}

	// Close the netlink socket.
	close(m_sock);
	m_sock = -1;

	// Prefer the network configuration error code, then the admin-provided script,
	// then the result of attempting to delete the veth device.
	return rc4 ? rc4 : (rc2 ? rc2 : rc);
}

int NetworkNamespaceManager::RunCleanupScript() {
	const char * namespace_script = param("NETWORK_NAMESPACE_DELETE_SCRIPT");

	if (namespace_script) {
		ArgList args;
		args.AppendArg(namespace_script);
		args.AppendArg(m_network_namespace);
		args.AppendArg(m_external_pipe);
		args.AppendArg(m_internal_address_str);
		FILE *fp = my_popen(args, "w", TRUE);
		if (fp == NULL) {
			dprintf(D_ALWAYS, "NetworkNamespaceManager::DeleteNamespace: my_popen failure on %s: (errno=%d) %s\n", namespace_script, errno, strerror(errno));
			return 1;
		}
		// TODO: log output of this script.  Probably means we can't use popen anymore
		// TODO: Factor out the setup script into a separate method.
		classad::ClassAdUnParser pp;
		std::string job_ad_str;
		pp.Unparse(job_ad_str, &m_job_ad);
		// Ignore EOF and errors - the script may ignore the stdin and return prior to 
		// us writing the complete ad.
		fputs(job_ad_str.c_str(), fp);
		fputs("\n------\n", fp);
		std::string machine_ad_str;
		pp.Unparse(machine_ad_str, m_ad.get());
		fputs(machine_ad_str.c_str(), fp);
		fputs("\n", fp);
		int status = my_pclose(fp);
		if (status == -1) {
			dprintf(D_ALWAYS, "NetworkNamespaceManager::DeleteNamespace: Error when running %s (errno=%d) %s\n", namespace_script, errno, strerror(errno));
		} else {
			if (WIFEXITED(status)) {
				if ((status = WEXITSTATUS(status))) {
					dprintf(D_ALWAYS, "NetworkNamespaceManager::DeleteNamespace: %s exited with status %d\n", namespace_script, status);
					return status;
				} else {
					dprintf(D_FULLDEBUG, "NetworkNamespaceManager::DeleteNamespace: %s was successful.\n", namespace_script);
				}
			} else if (WIFSIGNALED(status)) {
				status = WTERMSIG(status);
				dprintf(D_ALWAYS, "NetworkNamespaceManager::DeleteNamespace: %s terminated with signal %d\n", namespace_script, status);
			}
		}
	}

	return 0;
}

int NetworkNamespaceManager::ConfigureNetworkAccounting(const classad::ClassAd &machine_ad)
{

	// See if network accounting is requested; if not, return early.
	bool request_accounting = false;
	//if (!machine_ad.EvaluateAttrBool(ATTR_NETWORK_ACCOUNTING, request_accounting)) {
    if( !machine_ad.EvaluateAttrBoolEquiv(ATTR_NETWORK_ACCOUNTING, request_accounting)) {
		request_accounting = param_boolean(CONFIG_NETWORK_ACCOUNTING, false);
	}
	if (!request_accounting) {
		dprintf(D_FULLDEBUG, "Network accounting not requested.\n");
		return 0;
	}
	dprintf(D_FULLDEBUG, "Configuring network accounting.\n");

	std::string internal_addr;
	if (!m_ad->EvaluateAttrString(ATTR_INTERNAL_ADDRESS_IPV4, internal_addr))
	{
		dprintf(D_ALWAYS, "No IPV4 address found for network accounting.\n");
		return 1;
	}

	// Count outgoing packets.  Equivalent to:
	// iptables -A $JOBID -i $DEV ! -o $DEV -m comment --comment "Outgoing"
	{
	ArgList args;
	args.AppendArg("iptables");
	args.AppendArg("-A");
	args.AppendArg(m_network_namespace);
	args.AppendArg("-s");
	args.AppendArg(internal_addr);
	args.AppendArg("-m");
	args.AppendArg("comment");
	args.AppendArg("--comment");
	args.AppendArg("Outgoing");
	RUN_ARGS_AND_LOG(NetworkNamespaceManager::ConfigureNetworkAccounting, iptables_outgoing)
	}

	// Count incoming packets.  Equivalent to:
	// iptables -A $JOBID ! -i $DEV -o $DEV -m comment --comment "Incoming"
	{
	ArgList args;
	args.AppendArg("iptables");
	args.AppendArg("-A");
	args.AppendArg(m_network_namespace.c_str());
	args.AppendArg("-d");
	args.AppendArg(internal_addr);
	args.AppendArg("-m");
	args.AppendArg("comment");
	args.AppendArg("--comment");
	args.AppendArg("Incoming");
	RUN_ARGS_AND_LOG(NetworkNamespaceManager::ConfigureNetworkAccounting, iptables_incoming)
	}
	return 0;
}

int
NetworkNamespaceManager::OpenLarkLock(const std::string &dirname, const std::string &filename)
{
	std::string lock;
	if (!param(lock, "Lock"))
	{
		dprintf(D_ALWAYS, "LOCK parameter not set.\n");
	}
	std::string lark_lock_dir = lock + "/lark" + DIR_DELIM_STRING + dirname;
	if (!mkdir_and_parents_if_needed(lark_lock_dir.c_str(), S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH, PRIV_ROOT))
	{
		dprintf(D_ALWAYS, "Error creating Lark network lock directory %s. (errno=%d, %s)\n", lark_lock_dir.c_str(), errno, strerror(errno));
		return -1;
	}
	std::string lark_lock = lark_lock_dir + DIR_DELIM_STRING + filename;
	return open(lark_lock.c_str(), O_RDWR|O_CREAT|O_CLOEXEC, S_IWUSR|S_IRUSR);
}

NetworkNamespaceManager::NetworkLock::NetworkLock() : m_fd(-1)
{
	int fd = NetworkNamespaceManager::OpenLarkLock("/", "network_lock");
	if ((fd < 0) && (errno != EEXIST))
	{
		dprintf(D_ALWAYS, "Error opening $(LOCK)/lark/network_lock. (errno=%d, %s)\n", errno, strerror(errno));
	}
	struct flock lock_info;
	lock_info.l_type = F_WRLCK;
	lock_info.l_whence = SEEK_SET;
	lock_info.l_start = 0;
	lock_info.l_len = 0;
	lock_info.l_pid = 0;
	int retval;
	while (((retval = fcntl(fd, F_SETLKW, &lock_info)) == -1) && (errno == EINTR)) {}
	if (retval == -1)
	{
		dprintf(D_ALWAYS, "Error locking $(LOCK)/lark/network_lock. (errno=%d, %s)\n", errno, strerror(errno));
	}
	m_fd = fd;
}

NetworkNamespaceManager::NetworkLock::~NetworkLock()
{
	if (m_fd != -1) close(m_fd);
	m_fd = -1;
}
