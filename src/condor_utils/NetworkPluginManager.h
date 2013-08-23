
#ifndef __NETWORK_PLUGIN_MANAGER_H_
#define __NETWORK_PLUGIN_MANAGER_H_

#include <string>
#include <sys/types.h>

#include <classad/classad_stl.h>

#include "condor_uid.h"
#include "PluginManager.h"

// Forward decl
namespace classad {
	class ClassAd;
}

/*
 * The NetworkManager is invoked at various parts of the Starter's lifetime
 * to execute the network policy specified by the job.
 *
 * Long term, the Startd will likely become more active in the NetworkManager,
 * deciding which network policies are available and helping with the cleanup
 * in case of untimely starter death.
 *
 * Due to the way plugins work in Condor, this is basically a singleton.  HOWEVER,
 * implementers of this class SHOULD NOT assume this will always be a singleton
 * in the future.
 */

class NetworkManager {

	public:
		NetworkManager() { PluginManager<NetworkManager>::registerPlugin(this); }

		/* 
		 * Prepare the network configurations for the child.  Called before
		 * the fork(), directly by the starter.  Gives the manager a chance to
		 * start doing configuration changes at a high level - allows for an
		 * early failure too.
		 *
		 * - name: A unique name that the NetworkManager can use to identify this job.
		 * - job_ad: A reference to the job's classad.
		 * - machine_ad: A reference to the machine's classad.
		 *
		 * Note that the machine_ad is non-const and may be modified by the call.
		 *
		 * 0 on success, non-zero on failure.
		 */
		virtual int PrepareNetwork(const std::string & /*name*/,
			const classad::ClassAd & /*job_ad*/,
			classad_shared_ptr<classad::ClassAd> /*machine_ad*/) = 0;

		/*
		 * Called immediately before fork/clone in the parent process.
		 * Allows for last-second system changes.  For example, the NetworkNamespaceManager
		 * uses this as a chance to create the communication pipes for the PostFork*
		 * communication.
		 *
		 * When possible, attempt to do all code which can fail in PrepareNetwork.
		 *
		 * 0 on success, non-zero on failure.
		 */
		virtual int PreFork() = 0;

		/* 
		 * Called immediately after the fork/clone in the parent process.
		 * Combined with PostForkChild, it allows the manager to do parent/child
		 * communication and coordination before the child exec's.
		 *
		 * - pid: the PID of the child process.
		 *
		 * 0 on success, non-zero on failure.
		 */
		virtual int PostForkParent(pid_t /*pid*/) = 0;

		/*
		 * Called immediately after the fork/clone in the child process.
		 * Combined with PostForkParent, it allows the manager to do parent/child
		 * communication before the child exec's.
		 *
		 * 0 on success, non-zero on failure.
		 */
		virtual int PostForkChild() = 0;

		/*
		 * Perform any network accounting for this namespace.  Place results
		 * in a ClassAd to be sent to the shadow.
		 *
		 * Prior to cleanup, this function may be called with a NULL ClassAd.
		 * In such a case, perform accounting and save the state internally -
		 * we will call it with a valid ClassAd later, post invoking Cleanup.
		 *
		 * - classad: Reference to a ClassAd for resulting changes.
         * - jobphase: String to indicate the phase of the job.
		 *
		 * 0 on success, non-zero on failure.
		 */
	        virtual int PerformJobAccounting(classad::ClassAd *,/*classad*/ const std::string & /*jobphase*/) = 0;

		/*
		 * Cleanup any persistent OS structures created by the manager.
		 *
		 * May be called at any time, but only by the parent.  Condor does
		 * "best-effort" to call this only once.
		 *
		 * - name: A unique name to identify the job to cleanup.  If this
		 *   string is empty, clean up all possible OS structures created
		 *   by Condor.
		 *
		 * 0 on success, non-zero on failure.
		 */
		virtual int Cleanup(const std::string & /*name*/) = 0;

};

/*
 * The plugin manager for the NetworkManager classes.
 *
 * Almost 100% of this class is boilerplate as necessitated by how plugins
 * work in Condor.  For each function, refer to the corresponding one in
 * the NetworkManager class for documentation.
 */
class NetworkPluginManager : public PluginManager<NetworkManager> {

	public:
		static int PrepareNetwork(const std::string & uniq_name,
				const classad::ClassAd &job_ad,
				classad_shared_ptr<classad::ClassAd> machine_ad) {
 			NetworkManager *plugin;
			SimpleList<NetworkManager *> plugins = getPlugins();
			plugins.Rewind();
			TemporaryPrivSentry sentry(PRIV_ROOT);
			while (plugins.Next(plugin)) {
				int result;
				if ((result = plugin->PrepareNetwork(uniq_name,
						job_ad, machine_ad))) {
					return result;
				}
			}
			return 0;
		}

		static int PreFork() {
			NetworkManager *plugin;
			SimpleList<NetworkManager *> plugins = getPlugins();
			plugins.Rewind();
			TemporaryPrivSentry sentry(PRIV_ROOT);
			while (plugins.Next(plugin)) {
				int result;
				if ((result = plugin->PreFork())) {
					return result;
				}
			}
			return 0;
		}

		static int PostForkParent(pid_t pid) {
			NetworkManager *plugin;
			SimpleList<NetworkManager *> plugins = getPlugins();
			plugins.Rewind();
			// Note we use the special "no_memory_changes" version of set_priv,
			// due to the unique clone issues.  Makes this non-exception-safe.
			priv_state tmp_priv_state = set_priv_no_memory_changes(PRIV_ROOT);
			TemporaryPrivSentry sentry(PRIV_ROOT);
			int result = 0;
			while (plugins.Next(plugin)) {
				if ((result = plugin->PostForkParent(pid))) {
					break;
				}
			}
			set_priv_no_memory_changes(tmp_priv_state);
			return result;
		}

		static int PostForkChild() {
			NetworkManager *plugin;
			SimpleList<NetworkManager *> plugins = getPlugins();
			plugins.Rewind();
			// See note in PostForkParent about no_memory_changes.
			priv_state tmp_priv_state = set_priv_no_memory_changes(PRIV_ROOT);
			int result = 0;
			while (plugins.Next(plugin)) {
				if ((result = plugin->PostForkChild())) {
					break;
				}
			}
			set_priv_no_memory_changes(tmp_priv_state);
			return result;
		}

		static int PerformJobAccounting(classad::ClassAd *classad, const std::string & jobphase) {
			NetworkManager *plugin;
			SimpleList<NetworkManager *> plugins = getPlugins();
			plugins.Rewind();
			TemporaryPrivSentry sentry(PRIV_ROOT);
			while (plugins.Next(plugin)) {
				int result;
				if ((result = plugin->PerformJobAccounting(classad, jobphase))) {
					return result;
				}
			}
			return 0;
		}

		static int Cleanup(const std::string & name) {
			NetworkManager *plugin;
			SimpleList<NetworkManager *> plugins = getPlugins();
			plugins.Rewind();
			TemporaryPrivSentry sentry(PRIV_ROOT);
			while (plugins.Next(plugin)) {
				int result;
				if ((result = plugin->Cleanup(name))) {
					return result;
				}
			}
			return 0;
		}

		static bool HasPlugins() {
			SimpleList<NetworkManager *> plugins = getPlugins();
			return !plugins.IsEmpty();
		}
};

//template PluginManager<NetworkManager>;
//template SimpleList<NetworkManager *>;

#endif

