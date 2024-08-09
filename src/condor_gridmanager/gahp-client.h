/***************************************************************
 *
 * Copyright (C) 1990-2021, Condor Team, Computer Sciences Department,
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

#include "globus_utils.h"
#include "proxymanager.h"
#include "condor_arglist.h"
#include <map>
#include <deque>
#include <list>
#include <vector>
#include <string>
#include <utility>

#define NULLSTRING "NULL"

struct GahpProxyInfo
{
	Proxy *proxy;
	time_t cached_expiration;
	int num_references;
};

#define GAHP_SUCCESS 0

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

class GenericGahpClient;

class GahpServer : public Service {
 public:
	static GahpServer *FindOrCreateGahpServer(const char *id,
											  const char *path,
											  const ArgList *args = NULL);
	static std::map <std::string, GahpServer *> GahpServersById;

	GahpServer(const char *id, const char *path, const ArgList *args = NULL);
	~GahpServer();

	bool Startup(bool force=false);
	bool Initialize(Proxy * proxy);
	bool UpdateToken(const std::string &token_file);
	bool CreateSecuritySession();

	void DeleteMe(int timerID);

	static const int m_buffer_size;
	char *m_buffer;
	int m_buffer_pos;
	int m_buffer_end;
	int buffered_read( int fd, void *buf, int count );
	int buffered_peek() const;

	int m_deleteMeTid;

	bool m_in_results;

	static int m_reaperid;

	static int Reaper(int pid,int status);

	class GahpStatistics {
	public:
		GahpStatistics();

		void Publish( ClassAd & ad ) const;
		void Unpublish( ClassAd & ad ) const;

		static void Tick(int tid);

		static int RecentWindowMax;
		static int RecentWindowQuantum;
		static int Tick_tid;

		stats_entry_recent<int> CommandsIssued;
		stats_entry_recent<int> CommandsTimedOut;
		stats_entry_abs<int> CommandsInFlight;
		stats_entry_abs<int> CommandsQueued;
		stats_entry_recent<Probe> CommandRuntime;

		StatisticsPool Pool;
	};

	GahpStatistics m_stats;

	void read_argv(Gahp_Args &g_args);
	void read_argv(Gahp_Args *g_args) { read_argv(*g_args); }
	void write_line(const char *command, const char *debug_cmd = NULL) const;
	void write_line(const char *command,int req,const char *args) const;
	int pipe_ready(int pipe_end);
	int err_pipe_ready(int pipe_end);

	void AddGahpClient();
	void RemoveGahpClient();

	void ProxyCallback();
	void doProxyCheck(int timerID);
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
	unsigned int getPollInterval() const;

	/** Immediately poll the Gahp Server for results.  Normally,
		this method is invoked automatically either by a timer set
		via setPollInterval or by a Gahp Server async result
		notification.
		@see setPollInterval
	*/
	void poll(int timerID);

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
	bool command_update_token_from_file(const std::string &token);
	bool command_commands();
	bool command_async_mode_on();
	bool command_response_prefix(const char *prefix);
	bool command_condor_version();

	int new_reqid();

	int next_reqid;
	bool rotated_reqids;

	unsigned int m_reference_count;
	std::map<int,GenericGahpClient*> requestTable;
	std::deque<int> waitingHighPrio;
	std::deque<int> waitingMediumPrio;
	std::deque<int> waitingLowPrio;

	int m_gahp_pid;
	int m_gahp_readfd;
	int m_gahp_writefd;
	int m_gahp_errorfd;
	int m_gahp_real_readfd;
	int m_gahp_real_errorfd;
	std::string m_gahp_error_buffer;
	std::list<std::string> m_gahp_error_list;
	bool m_gahp_startup_failed;
	bool m_setCondorInherit;
	char m_gahp_version[150];
	std::string m_gahp_condor_version;
	std::vector<std::string> m_commands_supported;
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

	std::string m_sec_session_id;

	GahpProxyInfo *master_proxy;
	int proxy_check_tid;
	bool is_initialized;
	bool can_cache_proxies;
	std::map<std::string, GahpProxyInfo*> ProxiesByFilename;
}; // end of class GahpServer

class GenericGahpClient : public Service {
	friend class GahpServer;

	public:
		GenericGahpClient(	const char * id,
							const char * path = NULL,
							const ArgList * args = NULL );
		virtual ~GenericGahpClient();

		bool Startup(bool force=false);
		bool Initialize( Proxy * proxy );
		bool UpdateToken(const std::string &token_file);
		bool CreateSecuritySession();

		void SetCondorInherit(bool set_inherit) { server->m_setCondorInherit = set_inherit; }

		void purgePendingRequests() { clear_pending(); }
		bool pendingRequestIssued() { return pending_submitted_to_gahp || pending_result; }

		enum mode {
				/** */ normal,
				/** */ results_only,
				/** */ blocking
		};

		void setMode( mode m ) { m_mode = m; }
		mode getMode() { return m_mode; }

		void setTimeout( int t ) { m_timeout = t; }
		unsigned int getTimeout() const { return m_timeout; }

		void setNotificationTimerId( int tid ) { user_timerid = tid; }
		int getNotificationTimerId() const { return user_timerid; }

		void PublishStats( ClassAd * ad );

		void setNormalProxy( Proxy * proxy );
		void setDelegProxy( Proxy * proxy );
		Proxy * getMasterProxy();

		bool isStarted() { return server->m_gahp_pid != -1 && !server->m_gahp_startup_failed; }
		bool isInitialized() { return server->is_initialized; }

		std::vector<std::string>& getCommands() { return server->m_commands_supported; }

	    void setErrorString( const std::string & newErrorString );
		const char * getErrorString();

		const char * getGahpStderr();
		const char * getVersion();
		const char * getCondorVersion();

		int getNextReqId() { return server->next_reqid; }

		enum PrioLevel {
			low_prio,
			medium_prio,
			high_prio
		};

		// The caller must delete 'result' if it isn't NULL.
		int callGahpFunction(
			const char * command,
			const std::vector< YourString > & arguments,
			Gahp_Args * & result,
			PrioLevel priority = medium_prio );

	protected:
		void clear_pending();
		bool is_pending( const char *command, const char *buf );
		void now_pending (const char *command,const char *buf,
						 GahpProxyInfo *proxy = NULL,
						 PrioLevel prio_level = medium_prio );
		Gahp_Args * get_pending_result( const char *, const char * );
		bool check_pending_timeout( const char *, const char * );
		int reset_user_timer( int tid );
		void reset_user_timer_alarm(int timerID);

		unsigned int m_timeout;
		mode m_mode;
		char * pending_command;
		char * pending_args;
		int pending_reqid;
		Gahp_Args * pending_result;
		time_t pending_timeout;
		int pending_timeout_tid;
		time_t pending_submitted_to_gahp;
		int user_timerid;
		GahpProxyInfo * normal_proxy;
		GahpProxyInfo * deleg_proxy;
		GahpProxyInfo * pending_proxy;
		std::string error_string;

		GahpServer * server;
};


class GahpClient : public GenericGahpClient {
	// Hopefully not necessary.
	// friend class GahpServer;

	public:
		
		/** @name Instantiation. 
		 */
		//@{
	
			/// Constructor
		GahpClient(const char *id,
				   const char *path=NULL,
				   const ArgList *args=NULL);
			/// Destructor
		~GahpClient();
		
		//@}

		int getSshForwardPort() { return server->m_ssh_forward_port; }

		//-----------------------------------------------------------
		
		/**@name Scheduler-specific Methods
		 * These methods are for gahp commands specific to individual
		 * remote schedulers.
		 */
		//@{

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
								 const char *proxy_file, time_t proxy_expiration);

		int
		condor_job_update_lease(const char *schedd_name,
		                        const std::vector<PROC_ID> &jobs,
		                        const std::vector<int> &expirations,
		                        std::vector<PROC_ID> &updated);

		int
		blah_ping(const std::string& lrms);

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
		blah_download_proxy(const char *sandbox_id, const ClassAd *job_ad);

		int
		blah_destroy_sandbox(const char *sandbox_id, const ClassAd *job_ad);

		bool
		blah_get_sandbox_path(const char *sandbox_id, std::string &sandbox_path);

		int
		arc_ping(const std::string &service_url);

		int
		arc_job_new(const std::string &service_url,
		            const std::string &rsl,
		            bool has_proxy,
		            std::string &job_id,
		            std::string &job_status);

		int
		arc_job_status(const std::string &service_url,
		               const std::string &job_id,
		               std::string &status);

		int
		arc_job_status_all(const std::string &service_url,
		                   const std::string &states,
		                   std::vector<std::string> &job_ids,
		                   std::vector<std::string> &job_states);

		int
		arc_job_info(const std::string &service_url,
		             const std::string &job_id,
		             std::string &results);

		int
		arc_job_stage_in(const std::string &service_url,
		                 const std::string &job_id,
		                 const std::vector<std::string> &files);

		int
		arc_job_stage_out(const std::string &service_url,
		                  const std::string &job_id,
		                  const std::vector<std::string> &src_files,
		                  const std::vector<std::string> &dest_files);

		int
		arc_job_kill(const std::string &service_url,
		             const std::string &job_id);

		int
		arc_job_clean(const std::string &service_url,
		              const std::string &job_id);

		int
		arc_delegation_new(const std::string &service_url,
		                   const std::string &proxy_file,
		                   std::string &deleg_id);

		int
		arc_delegation_renew(const std::string &service_url,
		                     const std::string &deleg_id,
		                     const std::string &proxy_file);

		int gce_ping( const std::string &service_url,
					  const std::string &auth_file,
					  const std::string &account,
					  const std::string &project,
					  const std::string &zone );

		int gce_instance_insert( const std::string &service_url,
								 const std::string &auth_file,
								 const std::string &account,
								 const std::string &project,
								 const std::string &zone,
								 const std::string &instance_name,
								 const std::string &machine_type,
								 const std::string &image,
								 const std::string &metadata,
								 const std::string &metadata_file,
								 bool preemptible,
								 const std::string &json_file,
								 const std::vector< std::pair< std::string, std::string > > & labels,
								 std::string &instance_id );

		int gce_instance_delete( std::string service_url,
								 const std::string &auth_file,
								 const std::string &account,
								 const std::string &project,
								 const std::string &zone,
								 const std::string &instance_name );

		int gce_instance_list( const std::string &service_url,
							   const std::string &auth_file,
							   const std::string &account,
							   const std::string &project,
							   const std::string &zone,
							   std::vector<std::string> &instance_ids,
							   std::vector<std::string> &instance_names,
							   std::vector<std::string> &statuses,
							   std::vector<std::string> &status_msgs );

		int azure_ping( const std::string &auth_file,
		                const std::string &subscription );

		int azure_vm_create( const std::string &auth_file,
		                     const std::string &subscription,
		                     const std::vector<std::string> &vm_params, std::string &vm_id,
		                     std::string &ip_address );

		int azure_vm_delete( const std::string &auth_file,
		                     const std::string &subscription,
		                     const std::string &vm_name );

		int azure_vm_list( const std::string &auth_file,
		                   const std::string &subscription,
		                   std::vector<std::string> &vm_names,
		                   std::vector<std::string> &vm_statuses );

	private:

};	// end of class GahpClient

class EC2GahpClient : public GahpClient {
	public:

		EC2GahpClient(	const char * id, const char * path, const ArgList * args );
		~EC2GahpClient();

		int describe_stacks(	const std::string & service_url,
								const std::string & publickeyfile,
								const std::string & privatekeyfile,

								const std::string & stackName,

								std::string & stackStatus,
								std::map< std::string, std::string > & outputs,
								std::string & errorCode );

		int create_stack(	const std::string & service_url,
							const std::string & publickeyfile,
							const std::string & privatekeyfile,

							const std::string & stackName,
							const std::string & templateURL,
							const std::string & capability,
							const std::map< std::string, std::string > & parameters,

							std::string & stackID,
							std::string & errorCode );

		int get_function(	const std::string & service_url,
							const std::string & publickeyfile,
							const std::string & privatekeyfile,

							const std::string & functionARN,

							std::string & functionHash,
							std::string & errorCode );

		int call_function(	const std::string & service_url,
							const std::string & publickeyfile,
							const std::string & privatekeyfile,

							const std::string & functionARN,
							const std::string & argumentBlob,

							std::string & returnBlob,
							std::string & errorCode );

		int put_targets(	const std::string & service_url,
							const std::string & publickeyfile,
							const std::string & privatekeyfile,

							const std::string & ruleName,
							const std::string & id,
							const std::string & arn,
							const std::string & input,

							std::string & errorCode );

		int remove_targets(	const std::string & service_url,
							const std::string & publickeyfile,
							const std::string & privatekeyfile,

							const std::string & ruleName,
							const std::string & id,

							std::string & errorCode );

		int put_rule(		const std::string & service_url,
							const std::string & publickeyfile,
							const std::string & privatekeyfile,

							const std::string & ruleName,
							const std::string & scheduleExpression,
							const std::string & state,

							std::string & ruleARN,
							std::string & error_code );

		int delete_rule(	const std::string & service_url,
							const std::string & publickeyfile,
							const std::string & privatekeyfile,

							const std::string & ruleName,

							std::string & error_code );

		int s3_upload(	const std::string & service_url,
						const std::string & publickeyfile,
						const std::string & privatekeyfile,

						const std::string & bucketName,
						const std::string & fileName,
						const std::string & path,

						std::string & error_code );

		int bulk_start(	const std::string & service_url,
						const std::string & publickeyfile,
						const std::string & privatekeyfile,

						const std::string & client_token,
						const std::string & spot_price,
						const std::string & target_capacity,
						const std::string & iam_fleet_role,
						const std::string & allocation_strategy,
						const std::string & valid_until,

						const std::vector< std::string > & launch_configurations,

						std::string & bulkRequestID,
						std::string & error_code );

		int bulk_stop(	const std::string & service_url,
						const std::string & publickeyfile,
						const std::string & privatekeyfile,

						const std::string & bulkRequestID,

						std::string & error_code );

		int bulk_query(	const std::string & service_url,
						const std::string & publickeyfile,
						const std::string & privatekeyfile,

						std::vector<std::string> & returnStatus,
						std::string & error_code );

		int ec2_vm_start( const std::string & service_url,
						  const std::string & publickeyfile,
						  const std::string & privatekeyfile,
						  const std::string & ami_id,
						  const std::string & keypair,
						  const std::string & user_data,
						  const std::string & user_data_file,
						  const std::string & instance_type,
						  const std::string & availability_zone,
						  const std::string & vpc_subnet,
						  const std::string & vpc_ip,
						  const std::string & client_token,
						  const std::string & block_device_mapping,
						  const std::string & iam_profile_arn,
						  const std::string & iam_profile_name,
						  unsigned int maxCount,
						  const std::vector<std::string> & groupnames,
						  const std::vector<std::string> & groupids,
						  const std::vector<std::string> & parametersAndValues,
						  std::vector< std::string > & instance_ids,
						  std::string & error_code );

		int ec2_vm_stop( const std::string & service_url,
						 const std::string & publickeyfile,
						 const std::string & privatekeyfile,
						 const std::string & instance_id,
						 std::string & error_code );

		int ec2_vm_stop( const std::string & service_url,
						 const std::string & publickeyfile,
						 const std::string & privatekeyfile,
						 const std::vector< std::string > & instance_ids,
						 std::string & error_code );

		int ec2_vm_status_all( const std::string & service_url,
							   const std::string & publickeyfile,
							   const std::string & privatekeyfile,
							   std::vector<std::string> & returnStatus,
							   std::string & error_code );

		int ec2_vm_status_all( const std::string & service_url,
							   const std::string & publickeyfile,
							   const std::string & privatekeyfile,
							   const std::string & filterName,
							   const std::string & filterValue,
							   std::vector<std::string> & returnStatus,
							   std::string & error_code );

		int ec2_gahp_statistics( std::vector<std::string> & returnStatistics );

		int ec2_vm_server_type( const std::string & service_url,
								const std::string & publickeyfile,
								const std::string & privatekeyfile,
								std::string & server_type,
								std::string & error_code );

		int ec2_vm_create_keypair( const std::string & service_url,
								   const std::string & publickeyfile,
								   const std::string & privatekeyfile,
								   const std::string & keyname,
								   const std::string & outputfile,
								   std::string & error_code );

		int ec2_vm_destroy_keypair( const std::string & service_url,
									const std::string & publickeyfile,
									const std::string & privatekeyfile,
									const std::string & keyname,
									std::string & error_code );

        /**
         * Used to associate an elastic ip with a running instance
         */
        int ec2_associate_address( const std::string & service_url,
                                   const std::string & publickeyfile,
                                   const std::string & privatekeyfile,
                                   const std::string & instance_id,
                                   const std::string & elastic_ip,
                                   std::vector<std::string> & returnStatus,
                                   std::string & error_code );

		// Used to associate a tag with an resource, like a running instance
        int ec2_create_tags( const std::string & service_url,
							 const std::string & publickeyfile,
							 const std::string & privatekeyfile,
							 const std::string & instance_id,
							 const std::vector<std::string> & tags,
							 std::vector<std::string> & returnStatus,
							 std::string & error_code );

		/**
		 * Used to attach to an EBS volume(s).
		 */
		int ec2_attach_volume( const std::string & service_url,
                               const std::string & publickeyfile,
                               const std::string & privatekeyfile,
                               const std::string & volume_id,
							   const std::string & instance_id,
                               const std::string & device_id,
                               std::vector<std::string> & returnStatus,
                               std::string & error_code );

        // Is there a particular reason these aren't const references?
        int ec2_spot_start( const std::string & service_url,
                            const std::string & publickeyfile,
                            const std::string & privatekeyfile,
                            const std::string & ami_id,
                            const std::string & spot_price,
                            const std::string & keypair,
                            const std::string & user_data,
                            const std::string & user_data_file,
                            const std::string & instance_type,
                            const std::string & availability_zone,
                            const std::string & vpc_subnet,
                            const std::string & vpc_ip,
                            const std::string & client_token,
                            const std::string & iam_profile_arn,
                            const std::string & iam_profile_name,
                            const std::vector<std::string> & groupnames,
                            const std::vector<std::string> & groupids,
                            std::string & request_id,
                            std::string & error_code );

        int ec2_spot_stop( const std::string & service_url,
                           const std::string & publickeyfile,
                           const std::string & privatekeyfile,
                           const std::string & request_id,
                           std::string & error_code );

        int ec2_spot_status_all( const std::string & service_url,
                                 const std::string & publickeyfile,
                                 const std::string & privatekeyfile,
                                 std::vector<std::string> & returnStatus,
                                 std::string & error_code );
};

// Utility functions used all over the GAHP client code.
const char * escapeGahpString( const char * input );
const char * escapeGahpString( const std::string & input );

#endif /* ifndef CONDOR_GAHP_CLIENT_H */
