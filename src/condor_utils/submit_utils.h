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
**	Names of possible CONDOR variables.
*/
#define SUBMIT_KEY_Cluster "Cluster"
#define SUBMIT_KEY_Process "Process"
#define SUBMIT_KEY_BatchName "batch_name"
#define SUBMIT_KEY_Hold "hold"
#define SUBMIT_KEY_Priority "priority"
#define SUBMIT_KEY_Notification "notification"
#define SUBMIT_KEY_WantRemoteIO "want_remote_io"
#define SUBMIT_KEY_Executable "executable"
#define SUBMIT_KEY_Description "description"
#define SUBMIT_KEY_Arguments1 "arguments"
#define SUBMIT_KEY_Arguments2 "arguments2"
#define SUBMIT_KEY_AllowArgumentsV1 "allow_arguments_v1"
#define SUBMIT_KEY_GetEnvironment "getenv"
#define SUBMIT_KEY_AllowStartupScript "allow_startup_script"
#define SUBMIT_KEY_AllowEnvironmentV1 "allow_environment_v1"
#define SUBMIT_KEY_Environment1 "environment"
#define SUBMIT_KEY_Environment2 "environment2"
#define SUBMIT_KEY_Input "input"
#define SUBMIT_KEY_Output "output"
#define SUBMIT_KEY_Error "error"
#if !defined(WIN32)
#define SUBMIT_KEY_RootDir "rootdir"
#endif
#define SUBMIT_KEY_InitialDir "initialdir"
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
#define SUBMIT_KEY_RequestPrefix "request_"

#define SUBMIT_KEY_Universe "universe"
#define SUBMIT_KEY_MachineCount "machine_count"
#define SUBMIT_KEY_NotifyUser "notify_user"
#define SUBMIT_KEY_EmailAttributes "email_attributes"
#define SUBMIT_KEY_ExitRequirements "exit_requirements"
#define SUBMIT_KEY_UserLogFile "log"
#define SUBMIT_KEY_UseLogUseXML "log_xml"
#define SUBMIT_KEY_DagmanLogFile "dagman_log"
#define SUBMIT_KEY_CoreSize "coresize"
#define SUBMIT_KEY_NiceUser "nice_user"

#define SUBMIT_KEY_GridResource "grid_resource"
#define SUBMIT_KEY_X509UserProxy "x509userproxy"
#define SUBMIT_KEY_UseX509UserProxy "use_x509userproxy"
#define SUBMIT_KEY_DelegateJobGSICredentialsLifetime "delegate_job_gsi_credentials_lifetime"
#define SUBMIT_KEY_GridShell "gridshell"
#define SUBMIT_KEY_GlobusRSL "globus_rsl"
#define SUBMIT_KEY_NordugridRSL "nordugrid_rsl"
#define SUBMIT_KEY_RendezvousDir "rendezvousdir"
#define SUBMIT_KEY_KeystoreFile "keystore_file"
#define SUBMIT_KEY_KeystoreAlias "keystore_alias"
#define SUBMIT_KEY_KeystorePassphraseFile "keystore_passphrase_file"
#define SUBMIT_KEY_CreamAttributes "cream_attributes"
#define SUBMIT_KEY_BatchQueue "batch_queue"

#define SUBMIT_KEY_SendCredential "send_credential"

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

#define SUBMIT_KEY_ToolDaemonCmd "tool_daemon_cmd"
#define SUBMIT_KEY_ToolDaemonArgs "tool_daemon_args" // for backward compatibility
#define SUBMIT_KEY_ToolDaemonArguments1 "tool_daemon_arguments"
#define SUBMIT_KEY_ToolDaemonArguments2 "tool_daemon_arguments2"
#define SUBMIT_KEY_ToolDaemonInput "tool_daemon_input"
#define SUBMIT_KEY_ToolDaemonError "tool_daemon_error"
#define SUBMIT_KEY_ToolDaemonOutput "tool_daemon_output"
#define SUBMIT_KEY_SuspendJobAtExec "suspend_job_at_exec"

#define SUBMIT_KEY_TransferInputFiles "transfer_input_files"
#define SUBMIT_KEY_TransferOutputFiles "transfer_output_files"
#define SUBMIT_KEY_TransferOutputRemaps "transfer_output_remaps"
#define SUBMIT_KEY_TransferExecutable "transfer_executable"
#define SUBMIT_KEY_TransferInput "transfer_input"
#define SUBMIT_KEY_TransferOutput "transfer_output"
#define SUBMIT_KEY_TransferError "transfer_error"
#define SUBMIT_KEY_MaxTransferInputMB "max_transfer_input_mb"
#define SUBMIT_KEY_MaxTransferOutputMB "max_transfer_output_mb"

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
#define SUBMIT_KEY_OnExitHoldCheck "on_exit_hold"
#define SUBMIT_KEY_OnExitHoldReason "on_exit_hold_reason"
#define SUBMIT_KEY_OnExitHoldSubCode "on_exit_hold_subcode"
#define SUBMIT_KEY_OnExitRemoveCheck "on_exit_remove"
#define SUBMIT_KEY_Noop "noop_job"
#define SUBMIT_KEY_NoopExitSignal "noop_job_exit_signal"
#define SUBMIT_KEY_NoopExitCode "noop_job_exit_code"

#define SUBMIT_KEY_GlobusResubmit "globus_resubmit"
#define SUBMIT_KEY_GlobusRematch "globus_rematch"

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

//
// VM universe Parameters
//
#define SUBMIT_KEY_VM_VNC "vm_vnc"
#define SUBMIT_KEY_VM_Type "vm_type"
#define SUBMIT_KEY_VM_Memory "vm_memory"
#define SUBMIT_KEY_VM_VCPUS "vm_vcpus"
#define SUBMIT_KEY_VM_MACAddr "vm_macaddr"
#define SUBMIT_KEY_VM_Checkpoint "vm_checkpoint"
#define SUBMIT_KEY_VM_Networking "vm_networking"
#define SUBMIT_KEY_VM_Networking_Type "vm_networking_type"

//
// EC2 Query Parameters
//
#define SUBMIT_KEY_EC2AccessKeyId "ec2_access_key_id"
#define SUBMIT_KEY_EC2SecretAccessKey "ec2_secret_access_key"
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
#define SUBMIT_KEY_EC2SpotPrice "ec2_spot_price"
#define SUBMIT_KEY_EC2BlockDeviceMapping "ec2_block_device_mapping"
#define SUBMIT_KEY_EC2ParamNames "ec2_parameter_names"
#define SUBMIT_KEY_EC2ParamPrefix "ec2_parameter_"
#define SUBMIT_KEY_EC2IamProfileArn "ec2_iam_profile_arn"
#define SUBMIT_KEY_EC2IamProfileName "ec2_iam_profile_name"

#define SUBMIT_KEY_BoincAuthenticatorFile "boinc_authenticator_file"

//
// GCE Parameters
//
#define SUBMIT_KEY_GceImage "gce_image"
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

#define SUBMIT_KEY_NextJobStartDelay "next_job_start_delay"
#define SUBMIT_KEY_WantGracefulRemoval "want_graceful_removal"
#define SUBMIT_KEY_JobMaxVacateTime "job_max_vacate_time"

#define SUBMIT_KEY_REMOTE_PREFIX "Remote_"

#if !defined(WIN32)
#define SUBMIT_KEY_KillSig "kill_sig"
#define SUBMIT_KEY_RmKillSig "remove_kill_sig"
#define SUBMIT_KEY_HoldKillSig "hold_kill_sig"
#define SUBMIT_KEY_KillSigTimeout "kill_sig_timeout"
#endif

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
	char *set(char* str);
	void clear() { flags = start = end = step = 0; }
	bool  initialized() { return flags & 1; }

	// convert ix based on slice start & step, returns true if translated ix is within slice start and length.
	// input ix is assumed to be 0 based and increasing.
	bool translate(int & ix, int len);

	// check to see if ix is selected for by the slice. negative iteration is ignored 
	bool selected(int ix, int len);

	// returns number of selected items for a list of the given length, result is never negative
	int length_for(int len);

	int to_string(char * buf, int cch);

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

class SubmitForeachArgs {
public:
	SubmitForeachArgs() : foreach_mode(foreach_not), queue_num(1) {}
	void clear() {
		foreach_mode = foreach_not;
		queue_num = 1;
		vars.clearAll();
		items.clearAll();
		slice.clear();
		items_filename.clear();
	}

	int  parse_queue_args(char* pqargs); // destructively parse queue line.
	int  item_len();           // returns number of selected items, the items member must have been populated, or the mode must be foreach_not
	                           // the return does not take queue_num into account.

	int        foreach_mode;   // the mode of operation for foreach, one of the foreach_xxx enum values
	int        queue_num;      // the count of processes to queue for each item
	StringList vars;           // loop variable names
	StringList items;          // list of items to iterate over
	qslice     slice;          // may be initialized to slice if "[]" is parsed.
	MyString   items_filename; // file to read list of items from, if it is "<" list should be read from submit file until )
};


// used to indicate the role of a file when invoking the check_file callback
enum _submit_file_role {
	SFR_STDIN,
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

	void init();
	void clear(); // clear, but do not deallocate
	void setScheddVersion(const char * version) { ScheddVersion = version; }
	void setMyProxyPassword(const char * pass) { MyProxyPassword = pass; }
	bool setDisableFileChecks(bool value) { bool old = DisableFileChecks; DisableFileChecks = value; return old; }
	bool setFakeFileCreationChecks(bool value) { bool old = FakeFileCreationChecks; FakeFileCreationChecks = value; return old; }

	char * submit_param( const char* name, const char* alt_name );
	char * submit_param( const char* name ); // call param with NULL as the alt
	bool submit_param_exists(const char* name, const char * alt_name, std::string & value);
	bool submit_param_long_exists(const char* name, const char * alt_name, long long & value, bool int_range=false);
	int submit_param_int(const char* name, const char * alt_name, int def_value);
	int submit_param_bool(const char* name, const char * alt_name, bool def_value, bool * pexists=NULL);
	MyString submit_param_mystring( const char * name, const char * alt_name );
	char * expand_macro(const char* value) { return ::expand_macro(value, SubmitMacroSet, mctx); }
	const char * lookup(const char* name) { return lookup_macro(name, SubmitMacroSet, mctx); }

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

	// establishes default attibutes that are independent of submit file
	// call once before parsing the submit file and/or calling make_job_ad.
	int init_cluster_ad(time_t _submit_time, const char * owner); // returns 0 on success

	// establish default attributes using a foreign ad rather than by calling init_cluster_ad above
	int set_cluster_ad(ClassAd * ad);

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

	int InsertJobExpr (const char *expr, const char * source_label=NULL);
	int InsertJobExpr (const MyString &expr);
	int InsertJobExprInt(const char * name, int val);
	int InsertJobExprString(const char * name, const char * val);
	bool AssignJobVal(const char * attr, bool val);
	bool AssignJobVal(const char * attr, double val);
	bool AssignJobVal(const char * attr, long long val);
	bool AssignJobVal(const char * attr, int val) { return AssignJobVal(attr, (long long)val); }
	bool AssignJobVal(const char * attr, long val) { return AssignJobVal(attr, (long long)val); }
	//bool AssignJobVal(const char * attr, time_t val)  { return AssignJobVal(attr, (long long)val); }

	MACRO_ITEM* lookup_exact(const char * name) { return find_macro_item(name, NULL, SubmitMacroSet); }
	CondorError* error_stack() const { return SubmitMacroSet.errors; }

	void optimize() { if (SubmitMacroSet.sorted < SubmitMacroSet.size) optimize_macros(SubmitMacroSet); }
	void dump(FILE* out, int flags);
	const char* to_string(std::string & buf, int flags);
	const char* make_digest(std::string & buf, int cluster_id, StringList & vars, int options);
	void setup_macro_defaults(); // setup live defaults table

	MACRO_SET& macros() { return SubmitMacroSet; }
	int getUniverse()  { return JobUniverse; }
	int getClusterId() { return jid.cluster; }
	int getProcId()    { return jid.proc; }
	const char * getScheddVersion() { return ScheddVersion.Value(); }
	const char * getIWD();
	const char * full_path(const char *name, bool use_iwd=true);
	int check_and_universalize_path(MyString &path);

protected:
	MACRO_SET SubmitMacroSet;
	MACRO_EVAL_CONTEXT mctx;
	ClassAd baseJob; // defaults for job attributes, set by init_cluster_ad
	ClassAd * clusterAd; // use instead of baseJob if non-null. THIS POINTER IS NOT OWNED BY THE submit_utils class. It points back to the JobQueueJob that 
	ClassAd * procAd;
	DeltaClassAd * job; // this wraps the procAd or baseJob and tracks changes to the underlying ad.
	JOB_ID_KEY jid; // id of the current job being built
	time_t     submit_time;
	MyString   submit_owner; // owner specified to init_cluster_ad

	int abort_code; // if this is non-zero, all of the SetXXX functions will just quit
	const char * abort_macro_name; // if there is an abort_code and these are non-null, then the abort was because of this macro
	const char * abort_raw_macro_val;

	// options set externally (by command line arguments?)
	bool DisableFileChecks; // file checks disabled by config, not submit file
	bool FakeFileCreationChecks; // don't attempt to create/truncate files just check for write access
	bool IsInteractiveJob;
	bool IsRemoteJob;
	int (*FnCheckFile)(void*pv, SubmitHash * sub, _submit_file_role role, const char * name, int flags);
	void *CheckFileArg;

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
	ShouldTransferFiles_t should_transfer;
	int  JobUniverse;
	bool JobIwdInitialized;
	bool IsNiceUser;
	bool IsDockerJob;
	bool JobDisableFileChecks;	 // file checks disabled by submit file.
	bool NeedsJobDeferral;
	bool NeedsPerFileEncryption;
	bool HasEncryptExecuteDir;
	bool HasTDP;
	bool UserLogSpecified;
	bool StreamStdout;
	bool StreamStderr;
	bool RequestMemoryIsZero;
	bool RequestDiskIsZero;
	bool RequestCpusIsZeroOrOne;
	bool already_warned_requirements_disk;
	bool already_warned_requirements_mem;
	bool already_warned_job_lease_too_small;
	bool already_warned_notification_never;
	long long ExecutableSizeKb; // size of cmd or sizeof VM memory backing
	long long TransferInputSizeKb;
	auto_free_ptr tdp_cmd;
	auto_free_ptr tdp_input;
	auto_free_ptr RunAsOwnerCredD;
	MyString JobRequirements;
	MyString JobIwd;
	#if !defined(WIN32)
	MyString JobRootdir;
	#endif
	MyString JobGridType;  // set from "GridResource" for globus or grid universe jobs.
	MyString VMType;
	MyString TempPathname; // temporary path used by full_path
	MyString ScheddVersion; // target version of schedd, influences how jobad is filled in.
	MyString MyProxyPassword; // set by command line or by submit file. command line wins
	classad::References stringReqRes; // names of request_xxx submit variables that are string valued
	classad::References forcedSubmitAttrs; // + and MY. attribute names from SUBMIT_ATTRS/EXPRS


	// worker functions that build up the job from the hashtable
	// they pass data between themselves by setting class variables
	// so the must be called in a specific order.
	int SetDescription();
	int SetJobStatus();
	int SetJobLease();
	int SetSimpleJobExprs();
	int SetRemoteAttrs();
	int SetJobMachineAttrs();
	int SetExecutable();
	int SetUniverse();
	int SetMachineCount();
	int SetImageSize();
	int SetRequestResources();
	int SetStdFile( int which_file );
	int SetPriority();
	int SetNotification();
	int SetWantRemoteIO();
	int SetNotifyUser ();
	int SetEmailAttributes();
	int SetCronTab();
	int SetRemoteInitialDir();
	int SetExitRequirements();
	int SetOutputDestination();
	int SetWantGracefulRemoval();
	int SetJobMaxVacateTime();
	int SetArguments();
	int SetGridParams();
	int SetGSICredentials();
	int SetJobDeferral();
	int SetJobRetries();
	int SetEnvironment();
	#if !defined(WIN32)
	int ComputeRootDir(bool check_access=true);
	int SetRootDir(bool check_access=true);
	#endif
	int SetRequirements();
	bool check_requirements( char const *orig, MyString &answer );
	int SetTransferFiles();
	int InsertFileTransAttrs( FileTransferOutput_t when_output );
	int SetPerFileEncryption();
	int SetEncryptExecuteDir();
	int SetTDP();
	int SetRunAsOwner();
	int SetLoadProfile();
	int SetRank();
	int ComputeIWD(bool check_access=true);
	int SetIWD(bool check_access=true);
	int SetUserLog();
	int SetUserLogXML();
	int SetCoreSize();
	int SetFileOptions();
	int SetFetchFiles();
	int SetCompressFiles();
	int SetAppendFiles();
	int SetLocalFiles();
	#if !defined(WIN32)
	int SetKillSig();
	char *findKillSigName(const char* submit_name, const char* attr_name);
	#endif

	int SetPeriodicHoldCheck();
	int SetPeriodicRemoveCheck();
	int SetNoopJob();
	int SetLeaveInQueue();
	int SetDAGNodeName();
	int SetMatchListLen();
	int SetDAGManJobId();
	int SetLogNotes();
	int SetUserNotes();
	int SetStackSize();
	int SetJarFiles();
	int SetJavaVMArgs();
	int SetParallelStartupScripts(); //JDB
	int SetMaxJobRetirementTime();
	int SetConcurrencyLimits();
	int SetAccountingGroup();
	int SetVMParams();
	int SetVMRequirements(bool VMCheckpoint, bool VMNetworking, MyString &VMNetworkType, bool VMHardwareVT, bool vm_need_fsdomain);
	int FixupTransferInputFiles();
	int SetForcedAttributes();	// set +Attrib (MY.Attrib) hashtable keys directly into the job ad.  this should be called last.


	// private helper functions
	void push_error(FILE * fh, const char* format, ... ) CHECK_PRINTF_FORMAT(3,4);
	void push_warning(FILE * fh, const char* format, ... ) CHECK_PRINTF_FORMAT(3,4);
private:

	int64_t calc_image_size_kb( const char *name);

	void process_input_file_list(StringList * input_list, MyString *input_files, bool * files_specified, long long & accumulate_size_kb);
	int non_negative_int_fail(const char * Name, char * Value);
	void transfer_vm_file(const char *filename, long long & accumulate_size_kb);
};

const char * init_submit_default_macros();
#ifdef WIN32
void publishWindowsOSVersionInfo(ClassAd & ad);
#endif

#ifndef EXPAND_GLOBS_WARN_EMPTY
// functions in submit_glob.cpp
#define EXPAND_GLOBS_WARN_EMPTY (1<<0)
#define EXPAND_GLOBS_FAIL_EMPTY (1<<1)
#define EXPAND_GLOBS_ALLOW_DUPS (1<<2)
#define EXPAND_GLOBS_WARN_DUPS  (1<<3)
#define EXPAND_GLOBS_TO_DIRS    (1<<4) // when you want dirs only
#define EXPAND_GLOBS_TO_FILES   (1<<5) // when you want files only

int submit_expand_globs(StringList &items, int options, std::string & errmsg);
#endif // EXPAND_GLOBS

const	int			SCHEDD_INTERVAL_DEFAULT = 300;
const	int			JOB_DEFERRAL_PREP_TIME_DEFAULT = 300; // seconds
const	int			JOB_DEFERRAL_WINDOW_DEFAULT = 0; // seconds

#endif // _SUBMIT_UTILS_H

