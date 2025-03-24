/***************************************************************
 *
 * Copyright (C) 1990-2016, Condor Team, Computer Sciences Department,
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

#if !defined(_SUBMIT_UTILS_H)
#define _SUBMIT_UTILS_H

#include "condor_config.h" // for MACRO_SOURCE
#include <dc_schedd.h> // for ShouldTransferFiles_t


/*
**	submit keywords that control submit behavior
*/
#define SUBMIT_CMD_skip_filechecks "skip_filechecks"
#define SUBMIT_CMD_AllowArgumentsV1 "allow_arguments_v1"
#define SUBMIT_CMD_AllowEnvironmentV1 "allow_environment_v1"
#define SUBMIT_CMD_GetEnvironment "getenv"
#define SUBMIT_CMD_GetEnvironmentAlt "get_env"
#define SUBMIT_CMD_SendCredential "send_credential"

/*
**	submit keywords that specify job attributes
*/
#define SUBMIT_KEY_Cluster "Cluster"
#define SUBMIT_KEY_Process "Process"
#define SUBMIT_KEY_BatchName "batch_name"
#define SUBMIT_KEY_BatchId "batch_id"
#define SUBMIT_KEY_Hold "hold"
#define SUBMIT_KEY_Priority "priority"
#define SUBMIT_KEY_Prio "prio"
#define SUBMIT_KEY_Notification "notification"
#define SUBMIT_KEY_Executable "executable"
#define SUBMIT_KEY_INTERACTIVE_Executable "interactive_exectuable"
#define SUBMIT_KEY_Description "description"
#define SUBMIT_KEY_Arguments1 "arguments"
#define SUBMIT_KEY_Arguments2 "arguments2"
#define SUBMIT_KEY_INTERACTIVE_Args "interactive_args"
#define SUBMIT_KEY_Environment "environment"
#define SUBMIT_KEY_Env "env"
#define SUBMIT_KEY_Environment2 "environment2"
#define SUBMIT_KEY_Input "input"
#define SUBMIT_KEY_Shell "shell"
#define SUBMIT_KEY_Stdin "stdin"
#define SUBMIT_KEY_Output "output"
#define SUBMIT_KEY_Stdout "stdout"
#define SUBMIT_KEY_Error "error"
#define SUBMIT_KEY_Stderr "stderr"
#define SUBMIT_KEY_InitialDir "initialdir"
#define SUBMIT_KEY_InitialDirAlt "initial_dir"
#define SUBMIT_KEY_JobIwd "job_iwd"
#define SUBMIT_KEY_RemoteInitialDir "remote_initialdir"
#define SUBMIT_KEY_Requirements "requirements"
#define SUBMIT_KEY_Preferences "preferences"
#define SUBMIT_KEY_Rank "rank"
#define SUBMIT_KEY_ImageSize "image_size"
#define SUBMIT_KEY_DiskUsage "disk_usage"
#define SUBMIT_KEY_MemoryUsage "memory_usage"

#define SUBMIT_KEY_RequestCpus "request_cpus"
#define SUBMIT_KEY_RequestMemory "request_memory"
#define SUBMIT_KEY_RequestDisk "request_disk"
#define SUBMIT_KEY_RequestGpus "request_gpus"
#define SUBMIT_KEY_RequireGpus "require_gpus"
#define SUBMIT_KEY_RequestPrefix "request_"
#define SUBMIT_KEY_RequirePrefix "require_"
// GPU property constraint values
#define SUBMIT_KEY_GpusMinMemory "gpus_minimum_memory"
#define SUBMIT_KEY_GpusMinCapability "gpus_minimum_capability"
#define SUBMIT_KEY_GpusMaxCapability "gpus_maximum_capability"
#define SUBMIT_KEY_GpusMinRuntime "gpus_minimum_runtime"

#define SUBMIT_KEY_Universe "universe"
#define SUBMIT_KEY_MachineCount "machine_count"
#define SUBMIT_KEY_NotifyUser "notify_user"
#define SUBMIT_KEY_EmailAttributes "email_attributes"
#define SUBMIT_KEY_ExitRequirements "exit_requirements"
#define SUBMIT_KEY_UserLogFile "log"
#define SUBMIT_KEY_UserLogUseXML "log_xml"
#define SUBMIT_KEY_ULogExecuteEventAttrs "ulog_execute_attrs"
#define SUBMIT_KEY_DagmanLogFile "dagman_log"
#define SUBMIT_KEY_CoreSize "core_size"
#define SUBMIT_KEY_NiceUser "nice_user"
#define SUBMIT_KEY_KeepClaimIdle "keep_claim_idle"
#define SUBMIT_KEY_JobAdInformationAttrs "job_ad_information_attrs"
#define SUBMIT_KEY_JobLeaseDuration "job_lease_duration"
#define SUBMIT_KEY_JobMachineAttrs "job_machine_attrs"
#define SUBMIT_KEY_JobMachineAttrsHistoryLength "job_machine_attrs_history_length"
#define SUBMIT_KEY_NodeCount "node_count"
#define SUBMIT_KEY_NodeCountAlt "NodeCount"

#define SUBMIT_KEY_GridResource "grid_resource"
#define SUBMIT_KEY_X509UserProxy "x509userproxy"
#define SUBMIT_KEY_UseX509UserProxy "use_x509userproxy"
#define SUBMIT_KEY_UseScitokens "use_scitokens"
#define SUBMIT_KEY_UseScitokensAlt "use_scitoken"
#define SUBMIT_KEY_ScitokensFile "scitokens_file"
#define SUBMIT_KEY_DelegateJobGSICredentialsLifetime "delegate_job_gsi_credentials_lifetime"
#define SUBMIT_KEY_ArcRte "arc_rte"
#define SUBMIT_KEY_ArcApplication "arc_application"
#define SUBMIT_KEY_ArcResources "arc_resources"
#define SUBMIT_KEY_ArcDataStaging "arc_data_staging"
#define SUBMIT_KEY_RendezvousDir "rendezvousdir"
#define SUBMIT_KEY_BatchExtraSubmitArgs "batch_extra_submit_args"
#define SUBMIT_KEY_BatchProject "batch_project"
#define SUBMIT_KEY_BatchQueue "batch_queue"
#define SUBMIT_KEY_BatchRuntime "batch_runtime"

#define SUBMIT_KEY_FileRemaps "file_remaps"
#define SUBMIT_KEY_BufferFiles "buffer_files"
#define SUBMIT_KEY_BufferSize "buffer_size"
#define SUBMIT_KEY_BufferBlockSize "buffer_block_size"

#define SUBMIT_KEY_StackSize "stack_size"
#define SUBMIT_KEY_FetchFiles "fetch_files"
#define SUBMIT_KEY_CompressFiles "compress_files"
#define SUBMIT_KEY_AppendFiles "append_files"
#define SUBMIT_KEY_LocalFiles "local_files"

#define SUBMIT_KEY_EncryptExecuteDir "encrypt_execute_directory"
#define SUBMIT_KEY_StarterDebug "starter_debug"
#define SUBMIT_KEY_StarterLog "starter_log"

#define SUBMIT_KEY_ToolDaemonCmd "tool_daemon_cmd"
#define SUBMIT_KEY_ToolDaemonArgs "tool_daemon_args" // for backward compatibility
#define SUBMIT_KEY_ToolDaemonArguments1 "tool_daemon_arguments"
#define SUBMIT_KEY_ToolDaemonArguments2 "tool_daemon_arguments2"
#define SUBMIT_KEY_ToolDaemonInput "tool_daemon_input"
#define SUBMIT_KEY_ToolDaemonError "tool_daemon_error"
#define SUBMIT_KEY_ToolDaemonOutput "tool_daemon_output"
#define SUBMIT_KEY_SuspendJobAtExec "suspend_job_at_exec"

#define SUBMIT_KEY_WhenToTransferOutput "when_to_transfer_output"
#define SUBMIT_KEY_ShouldTransferFiles "should_transfer_files"
#define SUBMIT_KEY_PreserveRelativePaths "preserve_relative_paths"
#define SUBMIT_KEY_TransferCheckpointFiles "transfer_checkpoint_files"
#define SUBMIT_KEY_TransferContainer "transfer_container"
#define SUBMIT_KEY_TransferInputFiles "transfer_input_files"
#define SUBMIT_KEY_TransferInputFilesAlt "TransferInputFiles"
#define SUBMIT_KEY_TransferOutputFiles "transfer_output_files"
#define SUBMIT_KEY_TransferOutputFilesAlt "TransferOutputFiles"
#define SUBMIT_KEY_TransferOutputRemaps "transfer_output_remaps"
#define SUBMIT_KEY_TransferExecutable "transfer_executable"
#define SUBMIT_KEY_TransferInput "transfer_input"
#define SUBMIT_KEY_TransferOutput "transfer_output"
#define SUBMIT_KEY_TransferError "transfer_error"
#define SUBMIT_KEY_TransferPlugins "transfer_plugins"
#define SUBMIT_KEY_MaxTransferInputMB "max_transfer_input_mb"
#define SUBMIT_KEY_MaxTransferOutputMB "max_transfer_output_mb"
#define SUBMIT_KEY_WantIoProxy "want_io_proxy"

#define SUBMIT_KEY_ManifestDesired "manifest"
#define SUBMIT_KEY_ManifestDir "manifest_dir"

#define SUBMIT_KEY_UseOAuthServices "use_oauth_services"
#define SUBMIT_KEY_UseOAuthServicesAlt "UseOAuthServices"

#define SUBMIT_KEY_JobSet "jobset"

#ifdef HAVE_HTTP_PUBLIC_FILES
    #define SUBMIT_KEY_PublicInputFiles "public_input_files"
#endif

#define SUBMIT_KEY_EncryptInputFiles "encrypt_input_files"
#define SUBMIT_KEY_EncryptOutputFiles "encrypt_output_files"
#define SUBMIT_KEY_DontEncryptInputFiles "dont_encrypt_input_files"
#define SUBMIT_KEY_DontEncryptOutputFiles "dont_encrypt_output_files"

#define SUBMIT_KEY_OutputDestination "output_destination"

#define SUBMIT_KEY_StreamInput "stream_input"
#define SUBMIT_KEY_StreamOutput "stream_output"
#define SUBMIT_KEY_StreamError "stream_error"

#define SUBMIT_KEY_CopyToSpool "copy_to_spool"
#define SUBMIT_KEY_LeaveInQueue "leave_in_queue"

#define SUBMIT_KEY_PeriodicHoldCheck "periodic_hold"
#define SUBMIT_KEY_PeriodicHoldReason "periodic_hold_reason"
#define SUBMIT_KEY_PeriodicHoldSubCode "periodic_hold_subcode"
#define SUBMIT_KEY_PeriodicReleaseCheck "periodic_release"
#define SUBMIT_KEY_PeriodicRemoveCheck "periodic_remove"
#define SUBMIT_KEY_PeriodicVacateCheck "periodic_vacate"
#define SUBMIT_KEY_OnExitHoldCheck "on_exit_hold"
#define SUBMIT_KEY_OnExitHoldReason "on_exit_hold_reason"
#define SUBMIT_KEY_OnExitHoldSubCode "on_exit_hold_subcode"
#define SUBMIT_KEY_OnExitRemoveCheck "on_exit_remove"
#define SUBMIT_KEY_Noop "noop_job"
#define SUBMIT_KEY_NoopExitSignal "noop_job_exit_signal"
#define SUBMIT_KEY_NoopExitCode "noop_job_exit_code"

#define SUBMIT_KEY_LastMatchListLength "match_list_length"

#define SUBMIT_KEY_DAGManJobId "dagman_job_id"
#define SUBMIT_KEY_LogNotesCommand "submit_event_notes"
#define SUBMIT_KEY_UserNotesCommand "submit_event_user_notes"
#define SUBMIT_KEY_JarFiles "jar_files"
#define SUBMIT_KEY_JavaVMArgs "java_vm_args"
#define SUBMIT_KEY_JavaVMArguments1 "java_vm_arguments"
#define SUBMIT_KEY_JavaVMArguments2 "java_vm_arguments2"

#define SUBMIT_KEY_ParallelScriptShadow "parallel_script_shadow"  
#define SUBMIT_KEY_ParallelScriptStarter "parallel_script_starter" 

#define SUBMIT_KEY_MaxJobRetirementTime "max_job_retirement_time"

#define SUBMIT_KEY_JobWantsAds "want_ads"

#define SUBMIT_KEY_SkipIfDataflow "skip_if_dataflow"

//
// Job Deferral Parameters
//
#define SUBMIT_KEY_DeferralTime "deferral_time"
#define SUBMIT_KEY_DeferralWindow "deferral_window"
#define SUBMIT_KEY_DeferralPrepTime "deferral_prep_time"

// Job Retry Parameters
#define SUBMIT_KEY_MaxRetries "max_retries"
#define SUBMIT_KEY_RetryUntil "retry_until"
#define SUBMIT_KEY_SuccessExitCode "success_exit_code"

// Self-Checkpointing Parameters
#define SUBMIT_KEY_CheckpointExitCode "checkpoint_exit_code"
#define SUBMIT_KEY_CheckpointDestination "checkpoint_destination"

// ...
#define SUBMIT_KEY_EraseOutputAndErrorOnRestart "erase_output_and_error_on_restart"

//
// CronTab Parameters
// The index value below should be the # of parameters
//
#define SUBMIT_KEY_CronMinute "cron_minute"
#define SUBMIT_KEY_CronHour "cron_hour"
#define SUBMIT_KEY_CronDayOfMonth "cron_day_of_month"
#define SUBMIT_KEY_CronMonth "cron_month"
#define SUBMIT_KEY_CronDayOfWeek "cron_day_of_week"
#define SUBMIT_KEY_CronWindow "cron_window"
#define SUBMIT_KEY_CronPrepTime "cron_prep_time"

#define SUBMIT_KEY_RunAsOwner "run_as_owner"
#define SUBMIT_KEY_LoadProfile "load_profile"
#define SUBMIT_KEY_PrimaryUnixGroup "primary_unix_group"

// Concurrency Limit parameters
#define SUBMIT_KEY_ConcurrencyLimits "concurrency_limits"
#define SUBMIT_KEY_ConcurrencyLimitsExpr "concurrency_limits_expr"

// Accounting Group parameters
#define SUBMIT_KEY_AcctGroup "accounting_group"
#define SUBMIT_KEY_AcctGroupUser "accounting_group_user"

//
// docker "universe" Parameters
//
#define SUBMIT_KEY_DockerImage "docker_image"
#define SUBMIT_KEY_DockerNetworkType "docker_network_type"
#define SUBMIT_KEY_DockerPullPolicy "docker_pull_policy"
#define SUBMIT_KEY_DockerOverrideEntrypoint "docker_override_entrypoint"
#define SUBMIT_KEY_DockerSendCredentials "docker_send_credentials"

#define SUBMIT_KEY_ContainerImage "container_image"
#define SUBMIT_KEY_ContainerServiceNames "container_service_names"
#define SUBMIT_KEY_ContainerPortSuffix "_container_port"
#define SUBMIT_KEY_ContainerTargetDir "container_target_dir"
#define SUBMIT_KEY_MountUnderScratch "mount_under_scratch"

//
// VM universe Parameters
//
#define SUBMIT_KEY_VM_VNC "vm_vnc"
#define SUBMIT_KEY_VM_Type "vm_type"
#define SUBMIT_KEY_VM_Memory "vm_memory"
#define SUBMIT_KEY_VM_VCPUS "vm_vcpus"
#define SUBMIT_KEY_VM_DISK "vm_disk"
#define SUBMIT_KEY_VM_MACAddr "vm_macaddr"
#define SUBMIT_KEY_VM_Checkpoint "vm_checkpoint"
#define SUBMIT_KEY_VM_Networking "vm_networking"
#define SUBMIT_KEY_VM_Networking_Type "vm_networking_type"
#define SUBMIT_KEY_VM_NO_OUTPUT_VM "vm_no_output_vm"
#define SUBMIT_KEY_VM_XEN_KERNEL "xen_kernel"
#define SUBMIT_KEY_VM_XEN_INITRD "xen_initrd"
#define SUBMIT_KEY_VM_XEN_ROOT   "xen_root"
#define SUBMIT_KEY_VM_XEN_KERNEL_PARAMS "xen_kernel_params"

//
// EC2 Query Parameters
//
#define SUBMIT_KEY_WantNameTag "WantNameTag"
#define SUBMIT_KEY_EC2AccessKeyId "ec2_access_key_id"
#define SUBMIT_KEY_EC2SecretAccessKey "ec2_secret_access_key"
#define SUBMIT_KEY_AWSAccessKeyIdFile "aws_access_key_id_file"
#define SUBMIT_KEY_AWSSecretAccessKeyFile "aws_secret_access_key_file"
#define SUBMIT_KEY_AWSRegion "aws_region"
#define SUBMIT_KEY_S3AccessKeyIdFile "s3_access_key_id_file"
#define SUBMIT_KEY_S3SecretAccessKeyFile "s3_secret_access_key_file"
#define SUBMIT_KEY_GSAccessKeyIdFile "gs_access_key_id_file"
#define SUBMIT_KEY_GSSecretAccessKeyFile "gs_secret_access_key_file"
#define SUBMIT_KEY_EC2AmiID "ec2_ami_id"
#define SUBMIT_KEY_EC2UserData "ec2_user_data"
#define SUBMIT_KEY_EC2UserDataFile "ec2_user_data_file"
#define SUBMIT_KEY_EC2SecurityGroups "ec2_security_groups"
#define SUBMIT_KEY_EC2SecurityIDs "ec2_security_ids"
#define SUBMIT_KEY_EC2KeyPair "ec2_keypair"
#define SUBMIT_KEY_EC2KeyPairFile "ec2_keypair_file"
#define SUBMIT_KEY_EC2KeyPairAlt "ec2_keyp_air"
#define SUBMIT_KEY_EC2KeyPairFileAlt "ec2_key_pair_file"
#define SUBMIT_KEY_EC2InstanceType "ec2_instance_type"
#define SUBMIT_KEY_EC2ElasticIP "ec2_elastic_ip"
#define SUBMIT_KEY_EC2EBSVolumes "ec2_ebs_volumes"
#define SUBMIT_KEY_EC2AvailabilityZone "ec2_availability_zone"
#define SUBMIT_KEY_EC2VpcSubnet "ec2_vpc_subnet"
#define SUBMIT_KEY_EC2VpcIP "ec2_vpc_ip"
#define SUBMIT_KEY_EC2TagNames "ec2_tag_names"
#define SUBMIT_KEY_EC2TagPrefix "ec2_tag_"
#define SUBMIT_KEY_EC2SpotPrice "ec2_spot_price"
#define SUBMIT_KEY_EC2BlockDeviceMapping "ec2_block_device_mapping"
#define SUBMIT_KEY_EC2ParamNames "ec2_parameter_names"
#define SUBMIT_KEY_EC2ParamPrefix "ec2_parameter_"
#define SUBMIT_KEY_EC2IamProfileArn "ec2_iam_profile_arn"
#define SUBMIT_KEY_EC2IamProfileName "ec2_iam_profile_name"

//
// GCE Parameters
//
#define SUBMIT_KEY_GceImage "gce_image"
#define SUBMIT_KEY_GceAccount "gce_account"
#define SUBMIT_KEY_GceAuthFile "gce_auth_file"
#define SUBMIT_KEY_GceMachineType "gce_machine_type"
#define SUBMIT_KEY_GceMetadata "gce_metadata"
#define SUBMIT_KEY_GceMetadataFile "gce_metadata_file"
#define SUBMIT_KEY_GcePreemptible "gce_preemptible"
#define SUBMIT_KEY_GceJsonFile "gce_json_file"

// Azure Parameters
#define SUBMIT_KEY_AzureAuthFile "azure_auth_file"
#define SUBMIT_KEY_AzureLocation "azure_location"
#define SUBMIT_KEY_AzureSize "azure_size"
#define SUBMIT_KEY_AzureImage "azure_image"
#define SUBMIT_KEY_AzureAdminUsername "azure_admin_username"
#define SUBMIT_KEY_AzureAdminKey "azure_admin_key"

// Common cloud parameters
#define SUBMIT_KEY_CloudLabelNames "cloud_label_names"
#define SUBMIT_KEY_CloudLabelPrefix "cloud_label_"

#define SUBMIT_KEY_NextJobStartDelay "next_job_start_delay"
#define SUBMIT_KEY_WantGracefulRemoval "want_graceful_removal"
#define SUBMIT_KEY_JobMaxVacateTime "job_max_vacate_time"
#define SUBMIT_KEY_AllowedJobDuration "allowed_job_duration"
#define SUBMIT_KEY_AllowedExecuteDuration "allowed_execute_duration"

#define SUBMIT_KEY_JobMaterializeLimit "max_materialize"
#define SUBMIT_KEY_JobMaterializeMaxIdle "max_idle"
#define SUBMIT_KEY_JobMaterializeMaxIdleAlt "materialize_max_idle"

#define SUBMIT_KEY_CUDAVersion "cuda_version"

#define SUBMIT_KEY_REMOTE_PREFIX "Remote_"

#define SUBMIT_KEY_KillSig "kill_sig"
#define SUBMIT_KEY_RmKillSig "remove_kill_sig"
#define SUBMIT_KEY_HoldKillSig "hold_kill_sig"
#define SUBMIT_KEY_KillSigTimeout "kill_sig_timeout"

//Temporary function to get a mapfile object point to protected url map
MapFile* getProtectedURLMap();

// class to parse, hold and manage a python style slice: [x:y:z]
// used by the condor_submit queue 'foreach' handling
class qslice {
public:
	qslice() : flags(0), start(0), end(0), step(0) {}
	qslice(int _start) : flags(1|2), start(_start), end(0), step(0) {}
	qslice(int _start, int _end) : flags(1|2|4), start(_start), end(_end), step(0) {}
	~qslice() {}
	// set the slice by parsing a string [x:y:z], where
	// the enclosing [] are required
	// x,y & z are integers, y and z are optional
	const char *set(const char* str);
	char *set(char* str) { return const_cast<char*>(set(const_cast<const char *>(str))); }
	void clear() { flags = start = end = step = 0; }
	bool  initialized() const { return flags & 1; }

	// convert ix based on slice start & step, returns true if translated ix is within slice start and length.
	// input ix is assumed to be 0 based and increasing.
	bool translate(int & ix, int len) const;

	// check to see if ix is selected for by the slice. negative iteration is ignored 
	bool selected(int ix, int len) const;

	// returns number of selected items for a list of the given length, result is never negative
	int length_for(int len) const;

	int to_string(char * buf, int cch) const;

private:
	int flags; // 1==initialized, 2==start set, 4==length set, 8==step set
	int start;
	int end;
	int step;
};

// parse a the arguments for a Queue statement. this will be of the form
//
//    [<num-expr>] [[<var>[,<var2>]] in|from|matching [<slice>][<tokening>] (<items>)]
// 
//  {} indicates optional, <> indicates argument type rather than literal text, | is either or
//
//  <num-expr> is any classad expression that parses to an int it defines the number of
//             procs to queue per item in <items>.  If not present 1 is used.
//  <var>      is a variable name, case insensitive, case preserving, must begin with alpha and contain only alpha, numbers and _
//  in|from|matching  only one of these case-insensitive keywords may occur, these control interpretation of <items>
//  <slice>    is a python style slice controlling the start,end & step through the <items>
//  <tokening> arguments that control tokenizing <items>.
//  <items>    is a list of items to iterate and queue. the () surrounding items are optional, if they exist then
//             items may span multiple lines, in which case the final ) must be on a line by itself.
//
enum {
	foreach_not=0,
	foreach_in,
	foreach_from,
	foreach_matching,
	foreach_matching_files,
	foreach_matching_dirs,
	foreach_matching_any,

	// special case of foreach from, where there is an async reader that we are actually getting items from.
	// in this case the item stringlist will not contain all of the items. this mode is never set just
	// from parsing the submit file, it is set at runtime while iterating.
	foreach_from_async=0x102,
};

struct SubmitTableOpts {
	int  header_row{-1};  // row number of header/schema
	int  skip_rows{0};    // count of rows to skip after the header/schema
	bool ws_sep{true};    // when true, whitespace is also a separator
	bool trim_ws{true};   // when true, leading and trailing whitespace is trimmed from column data
	char sep_char{','};   // when non-zero, use this as the column separator character
	char comment_char{0}; // when non-zero, rows that start with this char as ignored

	// parse pqtable and set the above members based on it
	int assign(const char * ptr, size_t len);
	void clear();
	bool empty() { return header_row == -1 && skip_rows == 0 && ws_sep && trim_ws && sep_char == ','; }
};

constexpr SubmitTableOpts standard_foreach_table_opts{-1,0,true,true,',',0}; // the usual itemdata splitting options from long ago
constexpr SubmitTableOpts table_default_table_opts{0,0,false,true,',',0};    // default splitting options for TABLE keyword
constexpr SubmitTableOpts csv_default_table_opts{-1,0,false,true,',',0};     // csv no-header splitting options for TABLE keyword

class SubmitForeachArgs {
public:
	SubmitForeachArgs() : foreach_mode(foreach_not), queue_num(1), items_idx(0) {}
	void clear() {
		foreach_mode = foreach_not;
		queue_num = 1;
		items_idx = 0;
		vars.clear();
		items.clear();
		slice.clear();
		table_opts.clear();
		items_filename.clear();
	}

	int  parse_queue_args(char* pqargs);  // destructively parse queue line.
	char* set_table_opts(char* pqtable, int &err); // destructively scan table options, called by parse_queue_args
	int  load_schema(std::string & errmsg, bool check_against_existing = false);
	int  item_len() const;           // returns number of selected items, the items member must have been populated, or the mode must be foreach_not
	                           // the return does not take queue_num into account.

	// split a row into columns using the table opts
	// populates a vector of pointers to start of each value and returns the number of values
	// number of returned values should be num_cols
	int  split_item(std::string_view item, std::vector<std::string_view> & values, size_t num_cols);

	int        foreach_mode;   // the mode of operation for foreach, one of the foreach_xxx enum values
	int        queue_num;      // the count of processes to queue for each item
	std::vector<std::string> vars; // loop variable names
	std::vector<std::string> items; // list of items to iterate over
	SubmitTableOpts table_opts;    // options that control csv/table ingestion used with foreach_from
	size_t     items_idx;
	qslice     slice;          // may be initialized to slice if "[]" is parsed.
	std::string   items_filename; // file to read list of items from, if it is "<" list should be read from submit file until )
};


// used to indicate the role of a file when invoking the check_file callback
enum _submit_file_role {
	SFR_GENERIC,
	SFR_STDOUT,
	SFR_STDERR,
	SFR_INPUT,
	SFR_VM_INPUT,
	SFR_EXECUTABLE,
	SFR_PSEUDO_EXECUTABLE, // a 'cmd' file that isn't actually an exe. used for Grid jobs.
	SFR_LOG,
	SFR_OUTPUT,
};


typedef int (*FNSUBMITPARSE)(void* pv, MACRO_SOURCE& source, MACRO_SET& set, char * line, std::string & errmsg);

class DeltaClassAd;

class SubmitHash {
public:
	SubmitHash();
	~SubmitHash();

	void init(int value=-1);
	void clear(); // clear, but do not deallocate
	void setScheddVersion(const char * version) { ScheddVersion = version ? version : ""; }
	bool setDisableFileChecks(bool value) { bool old = DisableFileChecks; DisableFileChecks = value; return old; }
	bool setFakeFileCreationChecks(bool value) { bool old = FakeFileCreationChecks; FakeFileCreationChecks = value; return old; }
	bool addExtendedCommands(const classad::ClassAd & cmds) { return extendedCmds.Update(cmds); }
	void clearExtendedCommands() { extendedCmds.Clear(); }

	char * submit_param( const char* name, const char* alt_name ) const;
	char * submit_param( const char* name ) const; // call param with NULL as the alt
	bool submit_param_exists(const char* name, const char * alt_name, std::string & value) const;
	bool submit_param_long_exists(const char* name, const char * alt_name, long long & value, bool int_range=false) const;
	int submit_param_int(const char* name, const char * alt_name, int def_value) const;
	int submit_param_bool(const char* name, const char * alt_name, bool def_value, bool * pexists=NULL) const;
	std::string submit_param_string( const char * name, const char * alt_name ) const;
	char * expand_macro(const char* value) const { return ::expand_macro(value, const_cast<MACRO_SET&>(SubmitMacroSet), const_cast<MACRO_EVAL_CONTEXT&>(mctx)); }

	const char * lookup(const char* name) const {
		return lookup_macro( name,
		    const_cast<MACRO_SET&>(SubmitMacroSet),
		    const_cast<MACRO_EVAL_CONTEXT&>(mctx)
		);
	}

	const char * lookup_no_default(const char * name) const {
		return lookup_macro_exact_no_default_impl( name,
		    const_cast<MACRO_SET&>(SubmitMacroSet)
		);
	}

	void set_submit_param( const char* name, const char* value);
	void set_submit_param_used( const char* name);
	void set_arg_variable(const char* name, const char * value);
	void insert_source(const char * filename, MACRO_SOURCE & source);
	// like insert_source above but also sets the value that $(SUBMIT_FILE)
	// will expand to. (set into the defaults table, not the submit hash table)
	void insert_submit_filename(const char * filename, MACRO_SOURCE & source);

	// parse a submit file from fp until a queue statement is reached, then stop parsing
	// if a valid queue line is reached, return value will be 0, and qline will point to
	// the line text. the pqline pointer will be owned by getline_implementation
	int  parse_file_up_to_q_line(FILE* fp, MACRO_SOURCE & source, std::string & errmsg, char** qline);

	// parse a macro stream source until a queue statement is reached.
	// if a valid queue line is reached, return value will be 0, and qline will point to
	// the line text. the pqline pointer will be owned by getline_implementation
	int parse_up_to_q_line(MacroStream & ms, std::string & errmsg, char** qline);

	// parse a vector of lines and add them to the macro_set  Used to implement -a (or someday dagman Vars?)
	// can also be used to implement pre key=value lines. if you call before parsing the submit file.
	int parse_append_lines(std::vector<std::string_view> lines,  MACRO_SOURCE & source);

	// look for submit commands in the hash that indicate the a late materialization submit should be used
	// returns 1 if late materialization is requested by the job, 0 if not
	// if return is 1, max_materialize will be set to the requested value or to INT_MAX
	bool want_factory_submit(long long & max_materialize);

	// helper function to split queue arguments if any from the word 'queue'
	// returns NULL if the line does not begin with the word queue
	// otherwise returns a pointer to the first character of the queue arguments
	// suitable for passing to parse_queue_args()
	static const char * is_queue_statement(const char * line);

	// helper function to check if submit file contains DAG file commands
	static bool is_dag_command(const char * line);

	// parse the arguments after the Queue statement and populate a SubmitForeachArgs
	// as much as possible without globbing or reading any files.
	// if queue_args is "", then that is interpreted as Queue 1 just like condor_submit
	int  parse_q_args(
		const char * queue_args,         // IN: arguments after Queue statement before macro expansion
		SubmitForeachArgs & o,           // OUT: options & items from parsing the queue args
		std::string & errmsg);           // OUT: error message if return value is not 0

	// finish populating the items in a SubmitForeachArgs if they are in the submit file itself.
	// returns 0 for success, 1 if the items are external, < 0 for error.
	int  load_inline_q_foreach_items(
		MacroStream & ms,                // IN: submit file and source information, used only if the items are inline.
		SubmitForeachArgs & o,           // OUT: options & items from parsing the queue args
		std::string & errmsg);           // OUT: error message if return value is not 0

	// finish populating the items in a SubmitForeachArgs by reading files and/or globbing.
	int  load_external_q_foreach_items(
		SubmitForeachArgs & o,     // IN,OUT: options & items from parsing the queue args
		bool allow_stdin,          // IN: allow items to be read from stdin.
		std::string & errmsg);     // OUT: error message if return value is not 0

	// parse a submit file from fp using the given parse_q callback for handling queue statements
	int  parse_file(FILE* fp, MACRO_SOURCE & source, std::string & errmsg, FNSUBMITPARSE parse_q, void* parse_pv);
	// parse a submit file from memory buffer using the given parse_q callback for handling queue statements
	int  parse_mem(MacroStreamMemoryFile &fp, std::string & errmsg, FNSUBMITPARSE parse_q, void* parse_pv);
	// parse a submit file from null terminated memory buffer
	int  parse_mem(const char * buf, MACRO_SOURCE & source, std::string & errmsg, FNSUBMITPARSE parse_q, void* parse_pv) {
		MacroStreamMemoryFile ms(buf, -1, source);
		return parse_mem(ms, errmsg, parse_q, parse_pv);
	}

	int  process_q_line(MACRO_SOURCE & source, char* line, std::string & errmsg, FNSUBMITPARSE parse_q, void* parse_pv);

	void warn_unused(FILE* out, const char *app=NULL);
	int check_open( _submit_file_role role, const char *name, int flags );

	// stuff value into the submit's hashtable and mark 'name' as a used param
	// this function is intended for use during queue iteration to stuff changing values like $(Cluster) and $(Process)
	// Because of this the function does NOT make a copy of value, it's up to the caller to
	// make sure that value is not changed for the lifetime of possible macro substitution.
	void set_live_submit_variable(const char* name, const char* live_value, bool force_used=true);

	// force live submit variables to point to the empty string. (because the owner of the live value wants to free it)
	void unset_live_submit_variable(const char * name);

	// establishes default job attibutes that are independent of submit file (i.e. SUBMIT_ATTRS, etc)
	// call once before parsing the submit file and/or calling make_job_ad.
	int init_base_ad(time_t _submit_time, const char * username); // returns 0 on success

	// establish default attributes using a foreign ad rather than by calling init_base_ad above
	// used by late materialization when the 'base' ad is the cluster ad in the job queue.
	// note that this is NOT an ownership transfer of the ad, and the caller is responsible for
	// making sure that the ad is not deleted until after this class is destroyed or set_cluster_ad(NULL)
	// is called.
	// after this method is called, make_job_ad() will return job ads that are chained to the given cluster ad.
	// call this method with NULL to detach the foreign cluster ad from the SubmitHash
	int set_cluster_ad(ClassAd * ad);

	// after calling make_job_ad for the Procid==0 ad, pass the returned job ad to this function
	// to fold the job attributes into the base ad, thereby creating an internal clusterad (owned by the SubmitHash)
	// The passed in job ad will be striped down to a proc0 ad and chained to the internal clusterad
	// it is an error to pass any ad other than the most recent ad returned by make_job_ad()
	// After calling this method, subsequent calls to make_job_ad() will produce a job ad that
	// is chained to the cluster ad
	// This function does nothing if the SubmitHash is using a foreign clusterad (i.e. you called set_cluster_ad())
	bool fold_job_into_base_ad(int cluster_id, ClassAd * job);

	// If we have an initialized cluster ad, return it
	ClassAd * get_cluster_ad() {
		if (clusterAd) return clusterAd;
		else if (base_job_is_cluster_ad) return &baseJob;
		return NULL;
	}

	bool base_job_was_initialized() { return baseJob.size() > 0; }

	// fills out a job ad for the input job_id.
	// while the job ad is created, the check_file callback will be called one for each file
	// that might need to be transferred or checked for access.
	// the returned ClassAd will be valid until the next call to delete_job_ad() or make_job_ad()
	// call once for each job. 
	ClassAd * make_job_ad(JOB_ID_KEY job_id,
		int item_index, int step,
		bool interactive, bool remote,
		int (*check_file)(void*pv, SubmitHash * sub, _submit_file_role role, const char * name, int flags),
		void* pv_check_arg);

	// delete the last job ClassAd returned by make_job_ad (if any)
	void delete_job_ad();

	// forget variables used by make_job_ad that tie this submit hash to a specific submission
	// used by the python bindings since the submithash has longer life than a single transaction/submission
	void reset() {
		delete_job_ad();
		baseJob.Clear();
		jid.cluster = 0; jid.proc = 0;
		clusterAd = NULL;
		base_job_is_cluster_ad = 0;
	}

	int AssignJobExpr (const char *attr, const char * expr, const char * source_label=NULL);
	bool AssignJobString(const char * name, const char * val);
	bool AssignJobVal(const char * attr, bool val);
	bool AssignJobVal(const char * attr, double val);
	bool AssignJobVal(const char * attr, long long val);
	bool AssignJobVal(const char * attr, int val) { return AssignJobVal(attr, (long long)val); }
	bool AssignJobVal(const char * attr, long val) { return AssignJobVal(attr, (long long)val); }
	//bool AssignJobVal(const char * attr, time_t val)  { return AssignJobVal(attr, (long long)val); }

	int AssignJOBSETExpr(const char *attr, const char * expr, const char * source_label=NULL);
	bool AssignJOBSETString(const char * name, const char * sval);
	const ClassAd * getJOBSET() { return jobsetAd; }

	//Set job submit method to enum equal to passed value if value is in range
	void setSubmitMethod(int value) { s_method = value; }
	int getSubmitMethod(){ return s_method; }//Return job submit method value given s_method enum

	MACRO_ITEM* lookup_exact(const char * name) { return find_macro_item(name, NULL, SubmitMacroSet); }
	CondorError* error_stack() const { return SubmitMacroSet.errors; }

	void optimize() { if (SubmitMacroSet.sorted < SubmitMacroSet.size) optimize_macros(SubmitMacroSet); }
	void dump(FILE* out, int flags); // print the hash to the given FILE*
	void dump_templates(FILE* out, const char * category, int flags); // print the templates to the given FILE*
	const char* to_string(std::string & buf, int flags); // print (append) the hash to the supplied buffer
	const char* make_digest(std::string & buf, int cluster_id, const std::vector<std::string> & vars, int options);
	void setup_macro_defaults(); // setup live defaults table
	void setup_submit_time_defaults(time_t stime); // setup defaults table for $(SUBMIT_TIME)

	// check to see if the job needs OAuth services, returns TRUE if it does
	// the list of service handles is returned as a comma separated list  
	// in the formed needed to set the value of the OAuthServicesNeeded job attribute
	// if a request_ads collection is provided, it will be populated with OAuth service ads
	// and ads_error be set to describe any required but missing attributes in the request_ads
	bool NeedsOAuthServices(bool add_local, std::string & services, std::vector<ClassAd> * request_ads=NULL, std::string * ads_error=NULL) const;

	// job needs the countMatches classad function to match
	bool NeedsCountMatchesFunc() const { return HasRequireResAttr; };

	inline const char* get_source_filename(MACRO_SOURCE& src) {
		return macro_source_filename(src, SubmitMacroSet);
	}

	MACRO_SET& macros() { return SubmitMacroSet; }
	int getUniverse() const  { return JobUniverse; }
	int getClusterId() const { return jid.cluster; }
	int getProcId() const    { return jid.proc; }
	time_t getSubmitTime() const { return submit_time; } // aka QDATE, if this is 0, baseJob has never been initialized
	bool getSubmitOnHold(int & code) const { code = SubmitOnHoldCode; return SubmitOnHold; }
	const char * getScheddVersion() { return ScheddVersion.c_str(); }
	const char * getIWD();
	const char * full_path(const char *name, bool use_iwd=true);
	int check_and_universalize_path(std::string &path);

	enum class ContainerImageType {
		DockerRepo,
		SIF,
		SandboxImage,
		Unknown
	};

	// Attach and detach Protected URL transfer map object by pointer
	// Note: SubmitHash does not own this pointer
	void attachTransferMap(MapFile* map) { protectedUrlMap = map; }
	void detachTransferMap() { protectedUrlMap = nullptr; }

protected:
	MACRO_SET SubmitMacroSet;
	MACRO_EVAL_CONTEXT mctx;
	ClassAd baseJob; // defaults for job attributes, set by init_cluster_ad
	ClassAd * clusterAd; // use instead of baseJob if non-null. THIS POINTER IS NOT OWNED BY THE submit_utils class. It points back to the JobQueueJob that 
	ClassAd * procAd;
	ClassAd * jobsetAd;
	DeltaClassAd * job; // this wraps the procAd or baseJob and tracks changes to the underlying ad.
	JOB_ID_KEY jid; // id of the current job being built
	time_t     submit_time;
	std::string   submit_username; // username specified to init_cluster_ad
	ClassAd extendedCmds; // extended submit keywords, from config
	MapFile *protectedUrlMap{nullptr}; // Map file for protected url separation in job ad

	// these are used with the internal ABORT_AND_RETURN() and RETURN_IF_ABORT() methods
	mutable int abort_code; // if this is non-zero, all of the SetXXX functions will just quit
	mutable const char * abort_macro_name; // if there is an abort_code and these are non-null, then the abort was because of this macro
	mutable const char * abort_raw_macro_val;

	// keep track of whether we have turned the baseJob into a cluster ad yet, and what cluster it is
	int base_job_is_cluster_ad;

	// options set externally (by command line arguments?)
	bool DisableFileChecks; // file checks disabled by config, not submit file
	bool FakeFileCreationChecks; // don't attempt to create/truncate files just check for write access
	bool IsInteractiveJob;
	bool IsRemoteJob;
	int (*FnCheckFile)(void*pv, SubmitHash * sub, _submit_file_role role, const char * name, int flags);
	void *CheckFileArg;

	bool CheckProxyFile;

	// automatic 'live' submit variables. these pointers are set to point into the macro set allocation
	// pool. so the will be automatically freed. They are also set into the macro_set.defaults tables
	// so that whatever is set here will be found by $(Key) macro expansion.
	char* LiveNodeString;
	char* LiveClusterString;
	char* LiveProcessString;
	char* LiveRowString;
	char* LiveStepString;

	// options set as we parse the submit file
	// these variables are used to pass values between the various SetXXX functions below
	int  JobUniverse;
	bool JobIwdInitialized;
	bool IsDockerJob;
	bool IsContainerJob;
	bool HasRequireResAttr;
	bool JobDisableFileChecks;	 // file checks disabled by submit file.
	bool SubmitOnHold;
	int  SubmitOnHoldCode;
	int  s_method; //-1 represents undefined job submit method
	bool already_warned_requirements_disk;
	bool already_warned_requirements_mem;
	bool already_warned_job_lease_too_small;
	bool already_warned_notification_never;
	bool already_warned_require_gpus;
	bool UseDefaultResourceParams;
	bool InsertDefaultPolicyExprs;
	auto_free_ptr RunAsOwnerCredD;
	std::string JobIwd;
	std::string JobGridType;  // set from "GridResource" for grid universe jobs.
	std::string VMType;
	std::string TempPathname; // temporary path used by full_path
	std::string ScheddVersion; // target version of schedd, influences how jobad is filled in.
	classad::References stringReqRes; // names of request_xxx submit variables that are string valued
	classad::References forcedSubmitAttrs; // + and MY. attribute names from SUBMIT_ATTRS/EXPRS

	// entries of this struct map the table of the build functions to what job attrs are set and what keywords are referenced.
	struct _build_job_attrs {
		int (SubmitHash::*build)();
		const char * job_attrs;
		const char * submit_keys;
	};
	const struct _build_job_attrs* SaBuild();

	// worker functions that build up the job from the hashtable
	// they pass data between themselves by setting class variables
	// so the must be called in a specific order.

	int SetUniverse();  /* run once */

	int ComputeIWD();
	int SetIWD();		  /* factory:ok */


	int SetExecutable();  /* run once if */ // TODO: split out some functionality
	int SetArguments();  /* run once if */
	int SetGridParams();  /* run once if, grid universe only */
	int SetVMParams();  /* run once if, VM universe only */
	int SetJavaVMArgs();  /* run once if,  */
	int SetParallelParams();  /* run once */

	int SetEnvironment(); /* run once if, special */

	int SetJobStatus();  /* run once if */

	int SetTDP();  /* run once if, maybe split up?*/
	int SetStdin();  /* run once if */
	int SetStdout();  /* run once if */
	int SetStderr();  /* run once if */
	int SetGSICredentials();  /* run once if */

	int SetNotification();  /* factory:ok */
	int SetRank();  /* run once if */
	int SetPeriodicExpressions();  /* factory:ok */
	int SetLeaveInQueue();  /* factory:ok */
	int SetJobRetries();  /* factory:ok */
	int SetKillSig();  /* run once if */
	char *fixupKillSigName(char* sig);

	int SetImageSize(); /* run always */ // TODO: , split out request_disk

	int SetRequestResources(); /* n attrs, prunable by pattern */
	int SetConcurrencyLimits();  /* 2 attrs, prunable */
	int SetAccountingGroup();  /* 3 attrs, prunable */
	int SetOAuth(); /* 1 attr, prunable, factory:ok */

	int SetSimpleJobExprs(); /* run always */
	int SetExtendedJobExprs(); /* run always */
	int SetAutoAttributes(); /* run always */
	int ReportCommonMistakes(); /* run always */

	int SetJobDeferral();  /* run always */

	// For now, just handles port-forwarding.
	int SetContainerSpecial();

	// a LOT of the above functions must happen before SetTransferFiles, which in turn must be before SetRequirements
	int SetTransferFiles();
	int FixupTransferInputFiles();
	int SetRequirements(); // after SetTransferFiles
	int SetResourceRequirements(); // after SetRequestResources

	int SetForcedSubmitAttrs(); // set +Attrib (MY.Attrib) values from SUBMIT_ATTRS directly into the job ad. this should be called second to last
	int SetForcedAttributes();	// set +Attrib (MY.Attrib) hashtable keys directly into the job ad.  this should be called last.

	int ProcessJobsetAttributes();

	// check if the job ad has  Cron attributes set, checked by SetRequirements
	// return value is NULL if false,
	// otherwise it is the name of the attribute that tells us we need job deferral
	const char * NeedsJobDeferral();

	int CheckStdFile(
		_submit_file_role role,
		const char * value, // in: filename to use, may be NULL
		int access,         // in: desired access if checking for file accessiblity
		std::string & file, // out: filename, possibly fixed up.
		bool & transfer_it, // in,out: whether we expect to transfer it or not
		bool & stream_it);  // in,out: whether we expect to stream it or not

	// private helper functions
	int do_simple_commands(const struct SimpleSubmitKeyword * cmdtable);
	int build_oauth_service_ads(classad::References & services, std::vector<ClassAd> & ads, std::string & error) const;
	void fixup_rhs_for_digest(const char * key, std::string & rhs);
	int query_universe(std::string & sub_type, const char * & topping); // figure out universe, but DON'T modify the cached members
	bool key_is_prunable(const char * key); // return true if key can be pruned from submit digest
	void push_error(FILE * fh, const char* format, ... ) const CHECK_PRINTF_FORMAT(3,4);
	void push_warning(FILE * fh, const char* format, ... ) const CHECK_PRINTF_FORMAT(3,4);
private:

	int64_t calc_image_size_kb( const char *name);

	// returns a count of files in the input list
	int process_input_file_list(std::vector<std::string>& input_list, long long * accumulate_size_kb);
	//int non_negative_int_fail(const char * Name, char * Value);
	typedef int (SubmitHash::*FNSETATTRS)(const char * key);
	FNSETATTRS is_special_request_resource(const char * key);

	int SetRequestMem(const char * key);   /* used by SetRequestResources */
	int SetRequestDisk(const char * key);  /* used by SetRequestResources */
	int SetRequestCpus(const char * key);  /* used by SetRequestResources */
	int SetRequestGpus(const char * key);  /* used by SetRequestResources */
	int SetProtectedURLTransferLists();    /* used by FixupTransferInputFiles*/

	void handleAVPairs(const char * s, const char * j,
	  const char * sp, const char * jp,
	  const YourStringNoCase & gt );      /* used by SetGridParams */

	int process_container_input_files(std::vector<std::string> & input_files, long long * accumulate_size_kb); // call after building the input files list to find .vmx and .vmdk files in that list

	ContainerImageType image_type_from_string(std::string image) const;
};

struct SubmitStepFromQArgs {

	SubmitStepFromQArgs(SubmitHash & h)
		: m_hash(h)
		, m_jidInit(0,0)
		, m_nextProcId(0)
		, m_step_size(0)
		, m_done(false)
	{} // this needs to be cheap because Submit.queue() will always invoke it, even if there is no foreach data

	~SubmitStepFromQArgs() {
		// disconnnect the hashtable from our livevars pointers
		unset_live_vars();
	}

	bool has_items() const { return m_fea.items.size() > 0; }
	bool done() const { return m_done; }
	int  step_size() const { return m_step_size; }
	JOB_ID_KEY next_jobid() const { return JOB_ID_KEY(m_jidInit.cluster,m_nextProcId); }
	int  selected_item_count() const { return m_fea.item_len(); }
	int  selected_job_count() const {
		if (m_fea.queue_num < 0) return 0;
		return m_fea.item_len() * m_step_size;
	}

	// clear and re-initialize from new qargs
	int init(const char * qargs, std::string & errmsg)
	{
		int rval = 0;
		m_fea.clear();
		m_step_size = 1;
		m_done = false;
		if (qargs) {
			rval = m_hash.parse_q_args(qargs, m_fea, errmsg);
			m_step_size = (m_fea.queue_num > 1) ? m_fea.queue_num : 1;
		}
		return rval;
	}

	// setup for iteration from the args of a QUEUE statement and (possibly) inline itemdata
	int begin(const JOB_ID_KEY & id, bool set_live)
	{
		m_jidInit = id;
		m_nextProcId = id.proc;
		if (set_live) {
			if (m_fea.vars.empty()) {
				m_hash.set_live_submit_variable("Item", "", true);
			} else {
				for (const auto& key: vars()) {
					m_hash.set_live_submit_variable(key.c_str(), "", false);
				}
			}
		}
		m_step_size = (m_fea.queue_num > 1) ? m_fea.queue_num : 1;
		m_hash.optimize();
		return 0;
	}

	// setup for iteration when there is only the 'count' provided via the python bindings
	void begin(const JOB_ID_KEY & id, int count, bool set_live)
	{
		m_jidInit = id;
		m_nextProcId = id.proc;
		m_fea.clear(); m_fea.queue_num = count;
		m_step_size = (m_fea.queue_num > 1) ? m_fea.queue_num : 1;
		if (set_live) m_hash.set_live_submit_variable("Item", "", true);
		m_hash.optimize();
	}

	int load_items(MacroStream & ms_inline_items, bool allow_stdin, std::string errmsg)
	{
		int rval = m_hash.load_inline_q_foreach_items(ms_inline_items, m_fea, errmsg);
		if (rval == 1) { // items are external
			rval = m_hash.load_external_q_foreach_items(m_fea, allow_stdin, errmsg);
		}
		if (rval == 0) { m_fea.load_schema(errmsg); }
		return rval;
	}

	// returns < 0 on error
	// returns 0 if done iterating
	// returns 2 for first iteration
	// returns 1 for subsequent iterations
	int next_impl(bool selected, JOB_ID_KEY & jid, int & item_index, int & step, bool set_live)
	{
		if (m_done) return 0;

		int iter_index = (m_nextProcId - m_jidInit.proc);

		jid.cluster = m_jidInit.cluster;
		jid.proc = m_nextProcId;
		item_index = iter_index / m_step_size;
		step = iter_index % m_step_size;

		if (selected && ! m_fea.items.empty()) {
			// convert item_index from [0...n] to [slice start... end]
			// we are done iterating when translate returns false
			if ( ! m_fea.slice.translate(item_index, m_fea.items.size())) {
				m_done = true;
				return 0;
			}
		}

		if (0 == step) { // have we started a new row?
			if (select_rowdata(item_index)) {
				if (set_live) set_live_vars();
			} else {
				// if no next row, then we are done iterating, unless it is the FIRST iteration
				// in which case we want to pretend there is a single empty item called "Item"
				if (0 == iter_index) {
					m_hash.set_live_submit_variable("Item", "", true);
				} else {
					m_done = true;
					return 0;
				}
			}
		}

		++m_nextProcId;
		return (0 == iter_index) ? 2 : 1;
	}

	// sets jid, item_index and step for the next job, ignoring the slice (if any)
	int next_raw(JOB_ID_KEY & jid, int & item_index, int & step, bool set_live) {
		return next_impl(false, jid, item_index, step, set_live);
	}
	// sets jid, item_index and step for the next job, taking the slice into account
	int next_selected(JOB_ID_KEY & jid, int & item_index, int & step, bool set_live) {
		return next_impl(true, jid, item_index, step, set_live);
	}

	std::vector<std::string> & vars() { return m_fea.vars; }

	// make the current livevars active in the submit hash 
	void set_live_vars()
	{
		if (vars().empty()) {
			m_hash.set_live_submit_variable("Item", "", true);
			return;
		}

		for (const auto& key: vars()) {
			auto str = m_livevars.find(key);
			if (str != m_livevars.end()) {
				m_hash.set_live_submit_variable(key.c_str(), str->second.c_str(), false);
			} else {
				m_hash.unset_live_submit_variable(key.c_str());
			}
		}
	}

	// disconnect the livevars from the submit hash. (this sets the the submit hash vars to "")
	void unset_live_vars()
	{
		// set the pointers of the 'live' variables to the unset string (i.e. "")
		for (const auto& key: vars()) {
			m_hash.unset_live_submit_variable(key.c_str());
		}
	}

	// load livevars from a row
	// returns 0 if row data does not exist. livevars will remain unchanged
	// returns 1 if livevars was updated, This will invalidate the submit hash references to the current livevars
	int select_rowdata(size_t row_index)
	{
		if (row_index >= m_fea.items.size()) {
			return 0;
		}

		// split the data in the reqired number of fields
		// then store that field data into the m_livevars set

		// NOTE: If a row of item data does not contain a value for each variable (A,B,C -> 1,2) then
		// the remaining variables will not update their values. Meaning they inherit the previous
		// jobs values (or is undefined). If this 'allowed' behavior changes in condor_submit then
		// also change it here.
		// TODO: split_item will currently set empty values in the splits vector if there are not enough columns
		// TODO: add check for empty columnar data??
		auto & item = m_fea.items[row_index];
		std::vector<std::string_view> splits;
		int num_items = m_fea.split_item(item, splits, vars().size());
	
		int ix = 0;
		for (const auto& key: vars()) {
			if (ix >= num_items) { break; }
			m_livevars[key] = splits[ix++];
		}
		return 1;
	}

	// load the next rowdata into livevars
	// but not into the SubmitHash
	int next_rowdata()
	{
		if (m_fea.items_idx >= m_fea.items.size()) {
			return 0;
		}
		return select_rowdata(m_fea.items_idx++);
	}

	// return all of the live value data as a single 'line' using the given item separator and line terminator
	int get_rowdata(std::string & line, const char * sep, const char * eol)
	{
		// so that the separator and line terminators can be \0, we make the size strlen()
		// unless the first character is \0, then the size is 1
		int cchSep = sep ? (sep[0] ? (int)strlen(sep) : 1) : 0;
		int cchEol = eol ? (eol[0] ? (int)strlen(eol) : 1) : 0;
		line.clear();
		for (const auto& key: vars()) {
			if ( ! line.empty() && sep) line.append(sep, cchSep);
			auto str = m_livevars.find(key);
			if (str != m_livevars.end() && ! str->second.empty()) {
				line += str->second;
			}
		}
		if (eol && ! line.empty()) line.append(eol, cchEol);
		return (int)line.size();
	}

	// this is called repeatedly when we are sending rowdata to the schedd
	static int send_row(void* pv, std::string & rowdata) {
		SubmitStepFromQArgs *sii = (SubmitStepFromQArgs*)pv;

		rowdata.clear();
		if (sii->done())
			return 0;

		// Split and write into the string using US (0x1f) a field separator and LF as record terminator
		if ( ! sii->get_rowdata(rowdata, "\x1F", "\n"))
			return 0;

		int rval = sii->next_rowdata();
		if (rval < 0) { return rval; }
		if (rval == 0) { sii->m_done = true; } // so subsequent iterations will return 0
		return 1;
	}


	SubmitHash & m_hash;         // the (externally owned) submit hash we are updating as we iterate
	JOB_ID_KEY m_jidInit;
	SubmitForeachArgs m_fea;
	NOCASE_STRING_MAP m_livevars; // holds live data for active vars
	int  m_nextProcId;
	int  m_step_size;
	bool m_done;
};


const char * init_submit_default_macros();
#ifdef WIN32
void publishWindowsOSVersionInfo(ClassAd & ad);
#endif

// used for utility debug code in condor_submit
const struct SimpleSubmitKeyword * get_submit_keywords();

#ifndef EXPAND_GLOBS_WARN_EMPTY
// functions in submit_glob.cpp
#define EXPAND_GLOBS_WARN_EMPTY (1<<0)
#define EXPAND_GLOBS_FAIL_EMPTY (1<<1)
#define EXPAND_GLOBS_ALLOW_DUPS (1<<2)
#define EXPAND_GLOBS_WARN_DUPS  (1<<3)
#define EXPAND_GLOBS_TO_DIRS    (1<<4) // when you want dirs only
#define EXPAND_GLOBS_TO_FILES   (1<<5) // when you want files only

int submit_expand_globs(std::vector<std::string> &items, int options, std::string & errmsg);
#endif // EXPAND_GLOBS

const	int			SCHEDD_INTERVAL_DEFAULT = 300;
const	int			JOB_DEFERRAL_PREP_TIME_DEFAULT = 300; // seconds
const	int			JOB_DEFERRAL_WINDOW_DEFAULT = 0; // seconds


#define PJC_NOT_DRY_RUN 0

int process_job_credentials(
    // Input parameters.
    SubmitHash & submit_hash,
    int DashDryRun /* should default to 0 */,

    // Output parameters.
    std::string & URL,
    std::string & error_string
);

int append_queue_statement(std::string & submit_digest, SubmitForeachArgs & o);

#endif // _SUBMIT_UTILS_H

