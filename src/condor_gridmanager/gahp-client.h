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


#ifndef CONDOR_GAHP_CLIENT_H
#define CONDOR_GAHP_CLIENT_H

#include "condor_common.h"
#include "condor_daemon_core.h"
#include "gahp_common.h"

#include "classad_hashtable.h"
#include "globus_utils.h"
#include "proxymanager.h"
#include "condor_arglist.h"
#include <map>
#include <queue>
#include <string>


struct GahpProxyInfo
{
	Proxy *proxy;
	int cached_expiration;
	int num_references;
};

typedef void (* unicore_gahp_callback_func_t)(const char *update_ad_string);

#define GAHPCLIENT_DEFAULT_SERVER_ID "DEFAULT"
#define GAHPCLIENT_DEFAULT_SERVER_PATH "DEFAULT"

// Additional error values that GAHP calls can return
///
static const int GAHPCLIENT_COMMAND_PENDING = -100;
///
static const int GAHPCLIENT_COMMAND_NOT_SUPPORTED = -101;
///
static const int GAHPCLIENT_COMMAND_NOT_SUBMITTED = -102;
///
static const int GAHPCLIENT_COMMAND_TIMED_OUT = -103;

void GahpReconfig();

class GahpClient;

class GahpServer : public Service {
 public:
	static GahpServer *FindOrCreateGahpServer(const char *id,
											  const char *path,
											  const ArgList *args = NULL);
	static HashTable <HashKey, GahpServer *> GahpServersById;

	GahpServer(const char *id, const char *path, const ArgList *args = NULL);
	~GahpServer();

	bool Startup();
	bool Initialize(Proxy * proxy);

	void DeleteMe();

	static const int m_buffer_size;
	char *m_buffer;
	int m_buffer_pos;
	int m_buffer_end;
	int buffered_read( int fd, void *buf, int count );
	int buffered_peek();

	int m_deleteMeTid;

	bool m_in_results;

	static int m_reaperid;

	static void Reaper(Service *,int pid,int status);

	void read_argv(Gahp_Args &g_args);
	void read_argv(Gahp_Args *g_args) { read_argv(*g_args); }
	void write_line(const char *command);
	void write_line(const char *command,int req,const char *args);
	int pipe_ready(int pipe_end);
	int err_pipe_ready(int pipe_end);

	void AddGahpClient();
	void RemoveGahpClient();

	int ProxyCallback();
	void doProxyCheck();
	GahpProxyInfo *RegisterProxy( Proxy *proxy );
	void UnregisterProxy( Proxy *proxy );

	/** Set interval to automatically poll the Gahp Server for results.
		If the Gahp server supports async result notification, then
		the poll interval defaults to zero (disabled).  Otherwise,
		it will default to 5 seconds.  
	 	@param interval Polling interval in seconds, or zero
		to disable polling all together.  
		@return true on success, false otherwise.
		@see getPollInterval
	*/
	void setPollInterval(unsigned int interval);

	/** Retrieve the interval used to auto poll the Gahp Server 
		for results.  Also used to determine if async notification
		is in effect.
	 	@return Polling interval in seconds, or a zero
		to represent auto polling is disabled (likely if
		the Gahp server supports async notification).
		@see setPollInterval
	*/
	unsigned int getPollInterval();

	/** Immediately poll the Gahp Server for results.  Normally,
		this method is invoked automatically either by a timer set
		via setPollInterval or by a Gahp Server async result
		notification.
		@see setPollInterval
	*/
	void poll();

	void poll_real_soon();


	bool cacheProxyFromFile( GahpProxyInfo *new_proxy );
	bool uncacheProxy( GahpProxyInfo *gahp_proxy );
	bool useCachedProxy( GahpProxyInfo *new_proxy, bool force = false );

	bool command_cache_proxy_from_file( GahpProxyInfo *new_proxy );
	bool command_use_cached_proxy( GahpProxyInfo *new_proxy );

		// Methods for private GAHP commands
	bool command_version();
	bool command_initialize_from_file(const char *proxy_path,
									  const char *command=NULL);
	bool command_commands();
	bool command_async_mode_on();
	bool command_response_prefix(const char *prefix);

	int new_reqid();

	int next_reqid;
	bool rotated_reqids;

	unsigned int m_reference_count;
	HashTable<int,GahpClient*> *requestTable;
	std::queue<int> waitingHighPrio;
	std::queue<int> waitingMediumPrio;
	std::queue<int> waitingLowPrio;

	int m_gahp_pid;
	int m_gahp_readfd;
	int m_gahp_writefd;
	int m_gahp_errorfd;
	std::string m_gahp_error_buffer;
	bool m_gahp_startup_failed;
	char m_gahp_version[150];
	StringList * m_commands_supported;
	bool use_prefix;
	unsigned int m_pollInterval;
	int poll_tid;
	bool poll_pending;

	int max_pending_requests;
	int num_pending_requests;
	GahpProxyInfo *current_proxy;
	bool skip_next_r;
	char *binary_path;
	ArgList binary_args;
	char *my_id;
	int m_ssh_forward_port;

	char *globus_gass_server_url;
	char *globus_gt2_gram_callback_contact;
	void *globus_gt2_gram_user_callback_arg;
	globus_gram_client_callback_func_t globus_gt2_gram_callback_func;
	int globus_gt2_gram_callback_reqid;

	unicore_gahp_callback_func_t unicore_gahp_callback_func;
	int unicore_gahp_callback_reqid;

	GahpProxyInfo *master_proxy;
	int proxy_check_tid;
	bool is_initialized;
	bool can_cache_proxies;
	HashTable<HashKey,GahpProxyInfo*> *ProxiesByFilename;
}; // end of class GahpServer
	
///
class GahpClient : public Service {
	
	friend class GahpServer;
	public:
		
		/** @name Instantiation. 
		 */
		//@{
	
			/// Constructor
		GahpClient(const char *id=GAHPCLIENT_DEFAULT_SERVER_ID,
				   const char *path=GAHPCLIENT_DEFAULT_SERVER_PATH,
				   const ArgList *args=NULL);
			/// Destructor
		~GahpClient();
		
		//@}

		///
		bool Startup();

		///
		bool Initialize(Proxy *proxy);

		///
		void purgePendingRequests() { clear_pending(); }

		/** @name Mode methods.
		 * Methods to set/get the mode.
		 */
		//@{
	
		/// Enum used by setMode() method
		enum mode {
				/** */ normal,
				/** */ results_only,
				/** */ blocking
		};

		/**
		*/
		void setMode( mode m ) { m_mode = m; }

		/**
		*/
		mode getMode() { return m_mode; }
	
		//@}

		/** @name Timeout methods.
		 * Methods to set/get the timeout on pending async commands.
		 */
		//@{
	
		/** Set the timeout.
			@param t timeout in seconds, or zero to disable timeouts.
			@see getTimeout
		*/
		void setTimeout(int t) { m_timeout = t; }

		/** Get the currently timeout value.
			@return timeout in seconds, or zero if no timeout set.
			@see setTimeout
		*/
		unsigned int getTimeout() { return m_timeout; }
	
		//@}

		/** @name Async results methods.
		   Methods to control how to fetch and/or be notified about
		   pending asynchronous results.
		 */
		//@{
		
		/** Reset the specified timer to go off immediately when a
			pending command has completed.
			@param tid The timer id to reset via DaemonCore's Reset_Timer 
			method, or a -1 to disable.
			@see Reset_Timer
			@see getNotificationTimerId
		*/
		void setNotificationTimerId(int tid) { user_timerid = tid; }

		/** Return the timer id previously set with method 
			setNotificationTimerId.
			@param tid The timer id which will be reset via DaemonCore's 
			Reset_Timer method, or a -1 if deactivated.
			@see setNotificationTimerId
			@see Reset_Timer
		*/
		int getNotificationTimerId() { return user_timerid; }

		//@}

		void setNormalProxy( Proxy *proxy );

		void setDelegProxy( Proxy *proxy );

		Proxy *getMasterProxy();

		bool isStarted() { return server->m_gahp_pid != -1 && !server->m_gahp_startup_failed; }
		bool isInitialized() { return server->is_initialized; }

		const char *getErrorString();

		const char *getVersion();

		int getSshForwardPort() { return server->m_ssh_forward_port; }

		//-----------------------------------------------------------
		
		/**@name Globus Methods
		 * These methods have the exact same API as their native Globus
		 * Toolkit counterparts.  
		 */
		//@{

		const char *
			getGlobusGassServerUrl() { return server->globus_gass_server_url; }

		const char *getGt2CallbackContact()
			{ return server->globus_gt2_gram_callback_contact; }

		/// cache it from the gahp
		const char *
		globus_gram_client_error_string(int error_code);

		///
		int 
		globus_gram_client_callback_allow(
			globus_gram_client_callback_func_t callback_func,
			void * user_callback_arg,
			char ** callback_contact);

		///
		int 
		globus_gram_client_job_request(const char * resource_manager_contact,
			const char * description,
			const int limited_deleg,
			const char * callback_contact,
			std::string & job_contact,
			bool is_restart);

		///
		int 
		globus_gram_client_job_cancel(const char * job_contact);

		///
		int
		globus_gram_client_job_status(const char * job_contact,
			int * job_status,
			int * failure_code);

		///
		int
		globus_gram_client_job_signal(const char * job_contact,
			globus_gram_protocol_job_signal_t signal,
			const char * signal_arg,
			int * job_status,
			int * failure_code);

		///
		int
		globus_gram_client_job_callback_register(const char * job_contact,
			const int job_state_mask,
			const char * callback_contact,
			int * job_status,
			int * failure_code);

		///
		int 
		globus_gram_client_ping(const char * resource_manager_contact);

		///
		int
		globus_gram_client_job_refresh_credentials(const char *job_contact,
												   int limited_deleg);

		///
		int
		globus_gass_server_superez_init( char **gass_url, int port );

		///
		int 
		globus_gram_client_get_jobmanager_version(const char * resource_manager_contact);


		int
		condor_job_submit(const char *schedd_name, ClassAd *job_ad,
						  char **job_id);

		int
		condor_job_update_constrained(const char *schedd_name,
									  const char *constraint,
									  ClassAd *update_ad);

		int
		condor_job_status_constrained(const char *schedd_name,
									  const char *constraint,
									  int *num_ads, ClassAd ***ads);

		int
		condor_job_remove(const char *schedd_name, PROC_ID job_id,
						  const char *reason);

		int
		condor_job_update(const char *schedd_name, PROC_ID job_id,
						  ClassAd *update_ad);

		int
		condor_job_hold(const char *schedd_name, PROC_ID job_id,
						const char *reason);

		int
		condor_job_release(const char *schedd_name, PROC_ID job_id,
						   const char *reason);

		int
		condor_job_stage_in(const char *schedd_name, ClassAd *job_ad);

		int
		condor_job_stage_out(const char *schedd_name, PROC_ID job_id);

		int
		condor_job_refresh_proxy(const char *schedd_name, PROC_ID job_id,
								 const char *proxy_file);

		int
		condor_job_update_lease(const char *schedd_name,
								const SimpleList<PROC_ID> &jobs,
								const SimpleList<int> &expirations,
								SimpleList<PROC_ID> &updated );

		int
		blah_job_submit(ClassAd *job_ad, char **job_id);

		int
		blah_job_status(const char *job_id, ClassAd **status_ad);

		int
		blah_job_cancel(const char *job_id);

		int
		blah_job_refresh_proxy(const char *job_id, const char *proxy_file);

		int
		blah_download_sandbox(const char *sandbox_id, const ClassAd *job_ad,
							  std::string &sandbox_path);

		int
		blah_upload_sandbox(const char *sandbox_id, const ClassAd *job_ad);

		int
		blah_destroy_sandbox(const char *sandbox_id, const ClassAd *job_ad);

		int
		nordugrid_submit(const char *hostname, const char *rsl, char *&job_id);

		int
		nordugrid_status(const char *hostname, const char *job_id,
						 char *&status);

		int
		nordugrid_ldap_query(const char *hostname, const char *ldap_base,
							 const char *ldap_filter, const char *ldap_attrs,
							 StringList &results);

		int
		nordugrid_cancel(const char *hostname, const char *job_id);

		int
		nordugrid_stage_in(const char *hostname, const char *job_id,
						   StringList &files);

		int
		nordugrid_stage_out(const char *hostname, const char *job_id,
							StringList &files);

		int
		nordugrid_stage_out2(const char *hostname, const char *job_id,
							 StringList &src_files, StringList &dest_files);

		int
		nordugrid_exit_info(const char *hostname, const char *job_id,
							bool &normal_exit, int &exit_code,
							float &wallclock, float &sys_cpu,
							float &user_cpu );

		int
		nordugrid_ping(const char *hostname);

		int
		gridftp_transfer(const char *src_url, const char *dst_url);

		///
		int 
		unicore_job_create(const char * description,
						   char ** job_contact);

		///
		int
		unicore_job_start(const char *job_contact);

		///
		int 
		unicore_job_destroy(const char * job_contact);

		///
		int
		unicore_job_status(const char * job_contact,
						   char **job_status);

		///
		int
		unicore_job_recover(const char * description);

		int
		unicore_job_callback(unicore_gahp_callback_func_t callback_func);

		int cream_delegate(const char *delg_service, const char *delg_id);
		
		int cream_job_register(const char *service, const char *delg_id, 
							   const char *jdl, const char *lease_id, char **job_id, char **upload_url, char **download_url);
		
		int cream_job_start(const char *service, const char *job_id);
		
		int cream_job_purge(const char *service, const char *job_id);

		int cream_job_cancel(const char *service, const char *job_id);

		int cream_job_suspend(const char *service, const char *job_id);

		int cream_job_resume(const char *service, const char *job_id);

		int cream_job_status(const char *service, const char *job_id, 
							 char **job_status, int *exit_code, char **failure_reason);

		struct CreamJobStatus {
			std::string job_id;
			std::string job_status;
			int exit_code;
			std::string failure_reason;
		};
		typedef std::map<std::string, CreamJobStatus> CreamJobStatusMap;
		int cream_job_status_all(const char *service, CreamJobStatusMap & job_ids);
		
		int cream_proxy_renew(const char *delg_service, const char *delg_id);
		
		int cream_ping(const char * service);
		
		int cream_set_lease(const char *service, const char *lease_id, time_t &lease_expiry);

		int ec2_vm_start( std::string service_url,
						  std::string publickeyfile,
						  std::string privatekeyfile,
						  std::string ami_id,
						  std::string keypair,
						  std::string user_data,
						  std::string user_data_file,
						  std::string instance_type,
						  std::string availability_zone,
						  std::string vpc_subnet,
						  std::string vpc_ip,
						  std::string client_token,
						  StringList & groupnames,
						  char* & instance_id,
						  char* & error_code );

		int ec2_vm_stop( std::string service_url,
						 std::string publickeyfile,
						 std::string privatekeyfile,
						 std::string instance_id,
						 char* & error_code );

		int ec2_vm_status( std::string service_url,
							  std::string publickeyfile,
							  std::string privatekeyfile,
							  std::string instance_id,
							  StringList & returnStatus,
							  char* & error_code );

		int ec2_vm_status_all( std::string service_url,
							   std::string publickeyfile,
							   std::string privatekeyfile,
							   StringList & returnStatus,
							   char* & error_code );

		int ec2_ping( std::string service_url,
					  std::string publickeyfile,
					  std::string privatekeyfile,
					  char* & error_code );

		int ec2_vm_create_keypair( std::string service_url,
								   std::string publickeyfile,
								   std::string privatekeyfile,
								   std::string keyname,
								   std::string outputfile,
								   char* & error_code );

		int ec2_vm_destroy_keypair( std::string service_url,
									std::string publickeyfile,
									std::string privatekeyfile,
									std::string keyname,
									char* & error_code );

		int ec2_vm_vm_keypair_all( std::string service_url,
								   std::string publickeyfile,
								   std::string privatekeyfile,
								   StringList & returnStatus,
								   char* & error_code );

        /**
         * Used to associate an elastic ip with a running instance
         */
        int ec2_associate_address(std::string service_url,
                                  std::string publickeyfile,
                                  std::string privatekeyfile,
                                  std::string instance_id, 
                                  std::string elastic_ip,
                                  StringList & returnStatus,
                                  char* & error_code );

		// Used to associate a tag with an resource, like a running instance
        int ec2_create_tags(std::string service_url,
							std::string publickeyfile,
							std::string privatekeyfile,
							std::string instance_id, 
							StringList & tags,
							StringList & returnStatus,
							char* & error_code );
		
        /**
         * Used to release an elastic ip from an instance
         * leaving around in case we ever need this. 
         * shutdown causes automatic disassociation
        int ec2_disassociate_address( const char * service_url,
                                      const char * publickeyfile,
                                      const char * privatekeyfile,
                                      const char * elastic_ip,
                                      StringList & returnStatus,
                                      char* & error_code ); */

		/**
		 * Used to attach to an ecs volume(s).
		 */
		int ec2_attach_volume(std::string service_url,
                              std::string publickeyfile,
                              std::string privatekeyfile,
                              std::string volume_id,
							  std::string instance_id, 
                              std::string device_id,
                              StringList & returnStatus,
                              char* & error_code );

        // Is there a particular reason these aren't const references?
        int ec2_spot_start( std::string service_url,
                            std::string publickeyfile,
                            std::string privatekeyfile,
                            std::string ami_id,
                            std::string spot_price,
                            std::string keypair,
                            std::string user_data,
                            std::string user_data_file,
                            std::string instance_type,
                            std::string availability_zone,
                            std::string vpc_subnet,
                            std::string vpc_ip,
                            std::string client_token,
                            StringList & groupnames,
                            char * & request_id,
                            char * & error_code
                          );
        int ec2_spot_stop(  std::string service_url,
                            std::string publickeyfile,
                            std::string privatekeyfile,
                            std::string request_id,
                            char * & error_code
                         );
        int ec2_spot_status(    std::string service_url,
                                std::string publickeyfile,
                                std::string privatekeyfile,
                                std::string request_id,
                                StringList & returnStatus,
                                char * & error_code
                           );
        int ec2_spot_status_all(    std::string service_url,
                                    std::string publickeyfile,
                                    std::string privatekeyfile,
                                    StringList & returnStatus,
                                    char * & error_code
                               );

		int
		dcloud_submit( const char *service_url,
					   const char *username,
					   const char *password,
					   const char *image_id,
					   const char *instance_name,
					   const char *realm_id,
					   const char *hwp_id,
					   const char *hwp_memory,
					   const char *hwp_cpu,
					   const char *hwp_storage,
					   const char *keyname,
					   const char *userdata,
					   StringList &attrs );

		int
		dcloud_status_all( const char *service_url,
						   const char *username,
						   const char *password,
						   StringList &instance_ids,
						   StringList &statuses );

		int
		dcloud_action( const char *service_url,
					   const char *username,
					   const char *password,
					   const char *instance_id,
					   const char *action );

		int
		dcloud_info( const char *service_url,
					 const char *username,
					 const char *password,
					 const char *instance_id,
					 StringList &attrs );

		int
		dcloud_find( const char *service_url,
					 const char *username,
					 const char *password,
					 const char *instance_name,
					 char **instance_id );


		int
		dcloud_get_max_name_length( const char *service_url,
									const char *username,
									const char *password,
									int *max_length );

		int
		dcloud_start_auto( const char *service_url,
						   const char *username,
						   const char *password,
						   bool *autostart );

#ifdef CONDOR_GLOBUS_HELPER_WANT_DUROC
	// Not yet ready for prime time...
	globus_duroc_control_barrier_release();
	globus_duroc_control_init();
	globus_duroc_control_job_cancel();
	globus_duroc_control_job_request();
	globus_duroc_control_subjob_states();
	globus_duroc_error_get_gram_client_error();
	globus_duroc_error_string();
#endif /* ifdef CONDOR_GLOBUS_HELPER_WANT_DUROC */

		//@}

    void setErrorString( const std::string & newErrorString );

	private:

	/// Enum used by now_pending for the waiting queues
	enum PrioLevel {
		/** */ low_prio,
		/** */ medium_prio,
		/** */ high_prio
	};

			// Various Private Methods
		void clear_pending();
		bool is_pending(const char *command, const char *buf);
		void now_pending(const char *command,const char *buf,
						 GahpProxyInfo *proxy = NULL,
						 PrioLevel prio_level = medium_prio);
		Gahp_Args* get_pending_result(const char *,const char *);
		bool check_pending_timeout(const char *,const char *);
		int reset_user_timer(int tid);
		void reset_user_timer_alarm();

			// Private Data Members
		unsigned int m_timeout;
		mode m_mode;
		char *pending_command;
		char *pending_args;
		int pending_reqid;
		Gahp_Args* pending_result;
		time_t pending_timeout;
		int pending_timeout_tid;
		bool pending_submitted_to_gahp;
		int user_timerid;
		GahpProxyInfo *normal_proxy;
		GahpProxyInfo *deleg_proxy;
		GahpProxyInfo *pending_proxy;
		std::string error_string;

			// These data members all deal with the GAHP
			// server.  Since there is only one instance of the GAHP
			// server, all the below data members are static.
		GahpServer *server;

};	// end of class GahpClient


#endif /* ifndef CONDOR_GAHP_CLIENT_H */
