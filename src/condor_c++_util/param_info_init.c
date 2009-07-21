param_info_insert("UPDATE_COLLECTORS_WITH_TCP", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Update Collectors With Tcp", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "daemon_client,dc_collector");
param_info_insert("HOST", "", "input_host", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Host", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "unit_tests,FTEST_host_in_domain");
param_info_insert("MAX_TRANSFER_LIFETIME", "", "", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Max Transfer Lifetime", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "had,BaseReplicaTransferer");
param_info_insert("ENABLE_WEB_SERVER", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Enable Web Server", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "daemon_core,daemon_core");
param_info_insert("IGNORE_NFS_LOCK_ERRORS", "", "false", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Ignore Nfs Lock Errors", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  ",");
param_info_insert("RECONNECT_BACKOFF_CEILING", "", "300", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Reconnect Backoff Ceiling", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "shadow,baseshadow");
param_info_insert("NEGOTIATOR_DISCOUNT_SUSPENDED_RESOURCES", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Negotiator Discount Suspended Resources", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "accountant,Accountant");
param_info_insert("SEC_CLAIMTOBE_USER", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Sec Claimtobe User", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "io,condor_auth_claim");
param_info_insert("XEN_VT_VIF", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Xen Vt Vif", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "vm-gahp,xen_type");
param_info_insert("WEB_ROOT_DIR", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Web Root Dir", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "daemon_core,soap_core");
param_info_insert("LOG_ON_NFS_IS_ERROR", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Log On Nfs Is Error", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "submit,submit");
param_info_insert("VMP_HOST_MACHINE", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Vmp Host Machine", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "startd,startd_main");
param_info_insert("APPEND_REQ_STANDARD", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Append Req Standard", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "submit,submit");
param_info_insert("CLASSAD_CACHE_LIFETIME", "", "300", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Classad Cache Lifetime", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "classad,classad_lookup");
param_info_insert("DAGMAN_ABORT_ON_SCARY_SUBMIT", "", "true", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Dagman Abort On Scary Submit", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "dagman,dagman_main");
param_info_insert("UNICORE_GAHP", "", "$(SBIN)/unicore_gahp", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Unicore Gahp", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "gridmanager,unicorejob");
param_info_insert("CONDORC_ATTRS_TO_COPY", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Condorc Attrs To Copy", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "gridmanager,condorjob");
param_info_insert("SubmitEventNotes", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Submiteventnotes", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "submit,submit");
param_info_insert("GRIDMAP", "", "$(GSI_DAEMON_DIRECTORY)/grid-mapfile", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Gridmap file", "",
				  "",
				  "ccb,ccb");
param_info_insert("LOCAL_CONFIG_FILE", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Require Local Config File", "The location of the local config file(s) to use, comma or space delimited",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "c++_util,condor_config");
param_info_insert("EXCEPT_ON_ERROR", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Except On Error", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "daemon_core,daemon_core");
param_info_insert("CLASSAD_LIB_PATH", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Classad Lib Path", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "classad,ast");
param_info_insert("STARTD_CRON_AUTOPUBLISH", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Startd Cron Autopublish", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "startd,startd_cronmgr");
param_info_insert("NEGOTIATOR_INFORM_STARTD", "", "true", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Negotiator Inform Startd", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "negotiator,matchmaker");
param_info_insert("XEN_IMAGE_IO_TYPE", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Xen Image Io Type", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "vm-gahp,xen_type");
param_info_insert("ENABLE_STDOUT_TESTING", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Enable Stdout Testing", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "daemon_core,daemon_core");
param_info_insert("LOCK", "", "$(LOG)", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Lock", "manual for details on this.",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "startd,Starter");
param_info_insert("CONFIG_SERVER_PORT", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Config Server Port", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "master,master");
param_info_insert("GSI_DAEMON_DIRECTORY", "", "/etc/grid-security/", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "GSI Daemon Directory", "",
				  "",
				  "");
param_info_insert("MASTER_COLLECTOR_BACKOFF_CONSTANT", "", "1", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Master Collector Backoff Constant", "",
				  "",
				  "");
param_info_insert("STARTD_JOB_HOOK_KEYWORD", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Startd Job Hook Keyword", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "startd,Resource");
param_info_insert("MASTER_HA_LIST", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Master Ha List", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "master,master");
param_info_insert("QUILL_DBSIZE_LIMIT", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Quill Dbsize Limit", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "dbmsd,ManagedDatabase");
param_info_insert("QUEUE_ALL_USERS_TRUSTED", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Queue All Users Trusted", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "schedd,qmgmt");
param_info_insert("GRIDMANAGER_GLOBUS_COMMIT_TIMEOUT", "", "", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Gridmanager Globus Commit Timeout", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "gridmanager,globusjob");
param_info_insert("AMAZON_GAHP_WORKER_MIN_NUM", "", "1", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Amazon Gahp Worker Min Num", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "gridmanager,amazonjob");
param_info_insert("QUILL_IS_REMOTELY_QUERYABLE", "", "true", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Quill Is Remotely Queryable", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "quill,jobqueuedbmanager");
param_info_insert("USER_JOB_WRAPPER", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "User Job Wrapper", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "starter,starter_common");
param_info_insert("JOB_START_DELAY", "", "", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Job Start Delay", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "schedd,schedd");
param_info_insert("DAGMAN_CONDOR_SUBMIT_EXE", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Dagman Condor Submit Exe", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "dagman,dagman_main");
param_info_insert("JOB_INHERITS_STARTER_ENVIRONMENT", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Job Inherits Starter Environment", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "starter,os_proc");
param_info_insert("APPEND_REQUIREMENTS", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Append Requirements", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "submit,submit");
param_info_insert("STARTD_CRON_NAME", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Startd Cron Name", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "startd,startd_cronmgr");
param_info_insert("REAL_TIME_JOB_SUSPEND_UPDATES", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Real Time Job Suspend Updates", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "q,queue");
param_info_insert("TCP_COLLECTOR_HOST", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Tcp Collector Host", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "daemon_client,dc_collector");
param_info_insert("GLEXEC", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Glexec", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "startd,glexec_starter");
param_info_insert("VM_NETWORKING_DEFAULT_TYPE", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Vm Networking Default Type", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "vm-gahp,vmgahp_config");
param_info_insert("FS_LOCAL_DIR", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Fs Local Dir", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "io,condor_auth_fs");
param_info_insert("vm_no_output_vm", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Vm No Output Vm", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "submit,submit");
param_info_insert("GAHP_ARGS", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Gahp Args", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "gridmanager,gahp-client");
param_info_insert("XEN_VIF_PARAMETER", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Xen Vif Parameter", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "vm-gahp,xen_type");
param_info_insert("COLLECTOR_ENVIRONMENT", "", "", "0.0.0", "",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Collector Environment", "",
				  "",
				  "");
param_info_insert("PERIODIC_MEMORY_SYNC", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Periodic Memory Sync", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "shadow,shadow");
param_info_insert("SEC_TCP_SESSION_TIMEOUT", "", "20", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Sec Tcp Session Timeout", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "io,condor_secman");
param_info_insert("MAX_CLAIM_ALIVES_MISSED", "", "6", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Max Claim Alives Missed", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "startd,startd_main");
param_info_insert("MIRROR_JOB_LEASE_INTERVAL", "", "1800", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Mirror Job Lease Interval", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "gridmanager,mirrorjob");
param_info_insert("PROCD_ADDRESS", "", "$(LOCK)/procd_pipe", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Procd Address", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "starter,user_proc");
param_info_insert("DBMSD", "", "$(SBIN)/condor_dbmsd", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Dbmsd", "Where is the DBMSd binary installed and what arguments should be passed\?",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "master,master");
param_info_insert("GLEXEC_USER_DIR", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Glexec User Dir", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "startd,command");
param_info_insert("OBITUARY_LOG_LENGTH", "", "20", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Obituary Log Length", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "master,master");
param_info_insert("DEFAULT_IO_BUFFER_BLOCK_SIZE", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Default Io Buffer Block Size", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "submit,submit");
param_info_insert("ENABLE_SOAP", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Enable Soap", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "daemon_core,daemon_core");
param_info_insert("NEGOTIATOR_POST_JOB_RANK", "", "$(UWCS_NEGOTIATOR_POST_JOB_RANK)", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Negotiator Post Job Rank", "(e.g. if a job is in the pre-empting stage for too long)",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "negotiator,matchmaker");
param_info_insert("STARTD_FACTORY_SCRIPT_BACK_PARTITION", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Startd Factory Script Back Partition", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "startd_factory,startd_factory");
param_info_insert("xen_initrd", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Xen Initrd", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "submit,submit");
param_info_insert("SYSAPI_GET_LOADAVG", "", "1", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Sysapi Get Loadavg", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "sysapi,reconfig");
param_info_insert("GSI_DAEMON_KEY", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "GSI Daemon Key", "",
				  "",
				  "");
param_info_insert("QUILL_ENABLED", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Quill Enabled", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "q,queue");
param_info_insert("PROCD_DEBUG", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Procd Debug", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "c++_util,proc_family_proxy");
param_info_insert("FS_ALLOW_UNSAFE", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Fs Allow Unsafe", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "io,condor_auth_fs");
param_info_insert("USE_AFS", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Use Afs", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "shadow,pseudo_ops");
param_info_insert("STARTD_VM_EXPRS", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Startd Vm Exprs", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "startd,startd_main");
param_info_insert("VMWARE_PERL", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Vmware Perl", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "vm-gahp,vmware_type");
param_info_insert("FS_REMOTE_DIR", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Fs Remote Dir", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "io,condor_auth_fs");
param_info_insert("SETTABLE_ATTRS_NEGOTIATOR", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Settable Attribute Negotiator", "",
				  "",
				  "");
param_info_insert("CERTIFICATE_MAPFILE", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Certificate Mapfile", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "daemon_core,daemon_core");
param_info_insert("CCB_POLLING_INTERVAL", "", "20", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "CCB Polling Interval", "Try to run this often",
				  "",
				  "ccb,ccb");
param_info_insert("MASTER_COLLECTOR_CONTROLLER", "", "", "0.0.0", "",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Master Collector Controller", "",
				  "",
				  "");
param_info_insert("DAGMAN_OLD_RESCUE", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Dagman Old Rescue", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "dagman,condor_submit_dag");
param_info_insert("CLASSAD_CACHE_SIZE", "", "127", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Classad Cache Size", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "classad,classad_lookup");
param_info_insert("EVENTD_ADMIN_MEGABITS_SEC", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Eventd Admin Megabits Sec", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "eventd,admin_event");
param_info_insert("QUILL_USE_SQL_LOG", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Quill Use Sql Log", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "daemon_core,daemon_core_main");
param_info_insert("PVMD", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Pvmd", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "submit,submit");
param_info_insert("EVENTD_MIN_RESCHEDULE_INTERVAL", "", "60", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Eventd Min Reschedule Interval", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "eventd,scheduled_event");
param_info_insert("STORK_MODULE_DIR", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Stork Module Dir", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "stork,dap_server");
param_info_insert("WINDOWS_FIREWALL_FAILURE_RETRY", "", "60", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Windows Firewall Failure Retry", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "c++_util,windows_firewall");
param_info_insert("START_DAEMONS", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Start Daemons", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "master,master");
param_info_insert("QUILL_RUN_HISTORY_DURATION", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Quill Run History Duration", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "dbmsd,ManagedDatabase");
param_info_insert("LIBEXEC", "", "$(RELEASE_DIR)/libexec", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Libexec", "Pathnames",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "stork,dap_server");
param_info_insert("UPDATE_INTERVAL", "", "300", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Update Interval", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "eventd,scheduled_event");
param_info_insert("MAX_JOBS_RUNNING", "", "200", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Max Jobs Running", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "schedd,schedd");
param_info_insert("TRUNC_STARTER_LOG_ON_OPEN", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Truncate Starter Log on Open", "true if the StarterLog should be truncated when it is opened, false otherwise",
				  "http://www.cs.wisc.edu/condor/manual/v7.3/3_3Configuration.html",
				  "ccb,ccb");
param_info_insert("APPEND_RANK_STANDARD", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Append Rank Standard", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "submit,submit");
param_info_insert("RESERVE_AFS_CACHE", "", "", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Reserve Afs Cache", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "sysapi,reconfig");
param_info_insert("PRIVATE_NETWORK_INTERFACE", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Private Network Interface", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "daemon_core,daemon_core");
param_info_insert("QUILL_DB_IP_ADDR", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Quill Db Ip Addr", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "q,queue");
param_info_insert("MAX_PID_COLLISION_RETRY", "", "9", "0.0.0", "0,",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Max Pid Collision Retry", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "daemon_core,daemon_core");
param_info_insert("MASTER_BACKOFF_FACTOR", "", "2.0", "0.0.0", "",
                  STATE_DEFAULT, TYPE_DOUBLE, 0, 1, CUSTOMIZATION_SELDOM,
				  "Master Backoff Factor", "",
				  "",
				  "");
param_info_insert("NEGOTIATOR_TIMEOUT", "", "30", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Negotiator Timeout", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "negotiator,matchmaker");
param_info_insert("GAHP_USE_XML_CLASSADS", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Gahp Use Xml Classads", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "gridmanager,gahp-client");
param_info_insert("SHUTDOWN_GRACEFUL_TIMEOUT", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Shutdown Graceful Timeout", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "daemon_core,daemon_core_main");
param_info_insert("VM_TYPE", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Vm Type", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "vm-gahp,vmgahp_config");
param_info_insert("NEGOTIATOR_MATCH_EXPRS", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Negotiator Match Exprs", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "negotiator,matchmaker");
param_info_insert("HAD_STAND_ALONE_DEBUG", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Had Stand Alone Debug", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "had,StateMachine");
param_info_insert("GRIDMANAGER_MAX_JOBMANAGERS_PER_RESOURCE", "", "10", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Gridmanager Max Jobmanagers Per Resource", "many jobmanagers run causes severe load on the headnode.",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "gridmanager,globusresource");
param_info_insert("APPEND_RANK", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Append Rank", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "submit,submit");
param_info_insert("POOL_HISTORY_MAX_STORAGE", "", "10000000", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Pool History Max Storage", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "collector,view_server");
param_info_insert("VM_KILLING_TIMEOUT", "", "60", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Vm Killing Timeout", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "startd,Starter");
param_info_insert("COLLECTOR_DEBUG_WAIT", "", "false", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Collector Debug Wait", "True if the collector should wait for a debugger to attach before doing anything",
				  "",
				  "ccb,ccb");
param_info_insert("MAX_TRACKING_GID", "", "", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Max Tracking Gid", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "c++_util,proc_family_proxy");
param_info_insert("STARTD_COMPUTE_AVAIL_STATS", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Startd Compute Avail Stats", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "startd,startd_main");
param_info_insert("STRING", "", "input", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "String", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "unit_tests,FTEST_string_to_port");
param_info_insert("DAGMAN_MUNGE_NODE_NAMES", "", "true", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Dagman Munge Node Names", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "dagman,dagman_main");
param_info_insert("SEC_PASSWORD_DOMAIN", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Sec Password Domain", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "tools,store_cred_main");
param_info_insert("LOCAL_CONFIG_DIR", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Local Config Dir", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "c++_util,condor_config");
param_info_insert("CKPT_SERVER_REPLICATION_LEVEL", "", "", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Ckpt Server Replication Level", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "ckpt_server,server2");
param_info_insert("DEFAULT_IO_BUFFER_SIZE", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Default Io Buffer Size", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "submit,submit");
param_info_insert("GROUP_NAMES", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Group Names", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "accountant,Accountant");
param_info_insert("USE_PROCD", "", "", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Use Procd", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "c++_util,proc_family_interface");
param_info_insert("X_RUNS_HERE", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "X Runs Here", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "master,daemon");
param_info_insert("SEC_ENABLE_MATCH_PASSWORD_AUTHENTICATION", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Sec Enable Match Password Authentication", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "shadow,remoteresource");
param_info_insert("START_LOCAL_UNIVERSE", "", "True", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Start Local Universe", "When should a local universe job be allowed to start\?",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "schedd,schedd");
param_info_insert("SCHEDD_SEND_VACATE_VIA_TCP", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Schedd Send Vacate Via Tcp", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "schedd,schedd");
param_info_insert("SETTABLE_ATTRS_WRITE", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Settable Attribute Write", "",
				  "",
				  "");
param_info_insert("NEGOTIATOR_USE_NONBLOCKING_STARTD_CONTACT", "", "true", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Negotiator Use Nonblocking Startd Contact", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "negotiator,matchmaker");
param_info_insert("LOGS_USE_TIMESTAMP", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Logs Use Timestamp", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  ",");
param_info_insert("CKPT_SERVER_MAX_STORE_PROCESSES", "", "", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Ckpt Server Max Store Processes", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "ckpt_server,server2");
param_info_insert("VM_HARDWARE_VT", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Vm Hardware Vt", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "vm-gahp,vmgahp_config");
param_info_insert("STARTD_MAX_AVAIL_PERIOD_SAMPLES", "", "", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Startd Max Avail Period Samples", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "startd,AvailStats");
param_info_insert("VM_RECHECK_INTERVAL", "", "600", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Vm Recheck Interval", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "startd,vmuniverse_mgr");
param_info_insert("STARTER_DEBUG_WAIT", "", "false", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Starter Debug Wait", "True if the starter should wait for a debugger to attach before doing anything",
				  "",
				  "ccb,ccb");
param_info_insert("ALLOW_VM_CRUFT", "", "true", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Allow Vm Cruft", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "starter,starter_common");
param_info_insert("COLLECTOR_UPDATE_INTERVAL", "", "900", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Collector Update Interval", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "collector,collector");
param_info_insert("QUILL_HISTORY_CLEANING_INTERVAL", "", "24", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Quill History Cleaning Interval", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "quill,jobqueuedbmanager");
param_info_insert("MASTER_LOCK", "", "$(LOCK)/MasterLock", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Lock file for locking the master log file", "",
				  "",
				  "ccb,ccb");
param_info_insert("DATABASE_PURGE_INTERVAL", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Database Purge Interval", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "dbmsd,DBMSManager");
param_info_insert("GRIDMANAGER_GAHPCLIENT_DEBUG", "", "true", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Gridmanager Gahpclient Debug", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "gridmanager,gahp-client");
param_info_insert("OPSYS", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Opsys", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "starter,starter");
param_info_insert("PREEN", "", "$(SBIN)/condor_preen", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Preen", "want preen to run at all, just comment out this setting.",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "master,master");
param_info_insert("JAVA_EXTRA_ARGUMENTS", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Java Extra Arguments", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "gridmanager,unicore_gahp_wrapper");
param_info_insert("SHADOW_MAX_JOB_CLEANUP_RETRIES", "", "5", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Shadow Max Job Cleanup Retries", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "shadow,baseshadow");
param_info_insert("INVALID_LOG_FILES", "", "core", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Invalid Log Files", "What files should condor_preen remove from the log directory\?",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "tools,preen");
param_info_insert("BIND_ALL_INTERFACES", "", "true", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Bind All Interfaces", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "starter,user_proc");
param_info_insert("NEGOTIATOR_SIMPLE_MATCHING", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Negotiator Simple Matching", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "negotiator,matchmaker");
param_info_insert("DEDICATED_SCHEDULER_USE_FIFO", "", "true", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Dedicated Scheduler Use Fifo", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "schedd,dedicated_scheduler");
param_info_insert("QUILL_USE_TEMP_TABLE", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Quill Use Temp Table", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "tt,ttmanager");
param_info_insert("GSI_DAEMON_TRUSTED_CA_DIR", "", "$(GSI_DAEMON_DIRECTORY)/certificates", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "GSI Daemon Trusted Certificate Authority Directory", "the directory that contains the public keys and signing policies of trusted certificate authorities",
				  "",
				  "ccb,ccb");
param_info_insert("NEGOTIATOR_PRE_JOB_RANK", "", "$(UWCS_NEGOTIATOR_PRE_JOB_RANK)", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Negotiator Pre Job Rank", "(e.g. if a job is in the pre-empting stage for too long)",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "negotiator,matchmaker");
param_info_insert("PER_JOB_HISTORY_DIR", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Per Job History Dir", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "schedd,schedd");
param_info_insert("CCB_POLLING_TIMESLICE", "", "0.05", "0.0.0", "",
                  STATE_DEFAULT, TYPE_DOUBLE, 0, 1, CUSTOMIZATION_SELDOM,
				  "CCB Polling Timeslice", "What\'s the maximum amount of time the CCB should be run\?",
				  "",
				  "ccb,ccb");
param_info_insert("JAVA_CLASSPATH_DEFAULT", "", "$(LIB) $(LIB)/scimark2lib.jar .", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Java Classpath Default", "needs them.",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "c++_util,java_config");
param_info_insert("VM_GAHP_REQ_TIMEOUT", "", "", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Vm Gahp Req Timeout", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "starter,vm_proc");
param_info_insert("MASTER_BACKOFF_CEILING", "", "3600", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Master Backoff Ceiling", "",
				  "",
				  "");
param_info_insert("vm_should_transfer_cdrom_files", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Vm Should Transfer Cdrom Files", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "submit,submit");
param_info_insert("STARTD_FACTORY_SCRIPT_AVAILABLE_PARTITIONS", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Startd Factory Script Available Partitions", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "startd_factory,startd_factory");
param_info_insert("MASTER_INSTANCE_LOCK", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Instance Lock File", "",
				  "",
				  "");
param_info_insert("SHADOW_LIST", "", "SHADOW, SHADOW_STANDARD", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Shadow List", "Where are the various shadow binaries installed\?",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "schedd,shadow_mgr");
param_info_insert("EVENTD_SHUTDOWN_TIME", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Eventd Shutdown Time", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "eventd,admin_event");
param_info_insert("KEYBOARD_CPUS", "", "1", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Keyboard Cpus", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "startd,startd_main");
param_info_insert("MASTER_LOG", "", "$(LOG)/MasterLog", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Master Log", "Log files",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "master,master");
param_info_insert("MPI_CONDOR_RSH_PATH", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Mpi Condor Rsh Path", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "starter,mpi_master_proc");
param_info_insert("SCHEDD_BACKUP_SPOOL", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Schedd Backup Spool", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "schedd,schedd_main");
param_info_insert("XEN_DEFAULT_INITRD", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Xen Default Initrd", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "vm-gahp,xen_type");
param_info_insert("CONDOR_GAHP_WORKER", "", "$(SBIN)/condor_c-gahp_worker_thread", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Condor Gahp Worker", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "c-gahp,io_loop");
param_info_insert("STARTD_JOB_EXPRS", "", "ImageSize, ExecutableSize, JobUniverse, NiceUser", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Startd Job Exprs", "by commenting out this setting (there is no default).",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "startd,startd_main");
param_info_insert("MASTER_DAEMON_AD_FILE", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Daemon Ad File", "",
				  "",
				  "");
param_info_insert("COLLECTOR_STATS_SWEEP", "", "", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Collector Stats Sweep", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "collector,collector");
param_info_insert("MAX_SLOT_TYPES", "", "10", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Max Slot Types", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "startd,ResMgr");
param_info_insert("STARTER_JOB_ENVIRONMENT", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Starter Job Environment", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "starter,os_proc");
param_info_insert("SUBMIT_SEND_RESCHEDULE", "", "true", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Submit Send Reschedule", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "submit,submit");
param_info_insert("SCHED_UNIV_RENICE_INCREMENT", "", "", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Sched Univ Renice Increment", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "schedd,schedd");
param_info_insert("MAX_DISCARDED_RUN_TIME", "", "3600", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Max Discarded Run Time", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "shadow,shadow");
param_info_insert("xen_cdrom_device", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Xen Cdrom Device", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "submit,submit");
param_info_insert("SETTABLE_ATTRS_ADVERTISE_STARTD", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Settable Attribute Advertise Startd", "",
				  "",
				  "");
param_info_insert("DAGMAN_DELETE_OLD_LOGS", "", "", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Dagman Delete Old Logs", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "dagman,dagman_main");
param_info_insert("SHADOW", "", "$(SBIN)/condor_shadow", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Shadow", "Where are the various shadow binaries installed\?",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "schedd,schedd");
param_info_insert("VMWARE_NETWORKING_TYPE", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Vmware Networking Type", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "vm-gahp,vmware_type");
param_info_insert("VM_STATUS_INTERVAL", "", "", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Vm Status Interval", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "starter,vm_proc");
param_info_insert("DEAD_COLLECTOR_MAX_AVOIDANCE_TIME", "", "3600", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Dead Collector Max Avoidance Time", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "daemon_client,dc_collector");
param_info_insert("RUNTIME_CONFIG_ADMIN", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Runtime Config Admin", "",
				  "",
				  "c++_util,condor_config");
param_info_insert("NONBLOCKING_COLLECTOR_UPDATE", "", "true", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Nonblocking Collector Update", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "daemon_client,dc_collector");
param_info_insert("DEFAULT_RANK_STANDARD", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Default Rank Standard", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "submit,submit");
param_info_insert("ENABLE_BACKFILL", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Enable Backfill", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "startd,ResMgr");
param_info_insert("SOAP_SSL_SERVER_KEYFILE", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Soap Ssl Server Keyfile", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  ",");
param_info_insert("SHADOW_LAZY_QUEUE_UPDATE", "", "true", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Shadow Lazy Queue Update", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "shadow,baseshadow");
param_info_insert("reuse_condor_run_account", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Reuse Condor Run Account", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "c++_util,dynuser");
param_info_insert("QUILL_POLLING_PERIOD", "", "10", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Quill Polling Period", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "quill,jobqueuedbmanager");
param_info_insert("PRIVSEP_SWITCHBOARD", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Privsep Switchboard", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "privsep,privsep_client.UNIX");
param_info_insert("CREDD_CACHE_LOCALLY", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Credd Cache Locally", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "c++_util,uids");
param_info_insert("MIRROR_GAHP", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Mirror Gahp", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "gridmanager,mirrorjob");
param_info_insert("STARTER_UPLOAD_TIMEOUT", "", "300", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Starter Upload Timeout", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "starter,jic_shadow");
param_info_insert("XEN_BOOTLOADER", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Xen Bootloader", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "vm-gahp,xen_type");
param_info_insert("NET_REMAP_ROUTE", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Net Remap Route", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "c++_util,condor_config");
param_info_insert("SOAP_SSL_DH_FILE", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Soap Ssl Dh File", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "daemon_core,soap_core");
param_info_insert("WARN_ON_UNUSED_SUBMIT_FILE_MACROS", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Warn On Unused Submit File Macros", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "submit,submit");
param_info_insert("TOOLS_PROVIDE_OLD_MESSAGES", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Tools Provide Old Messages", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "rm,rm");
param_info_insert("DEFAULT_PRIO_FACTOR", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Default Prio Factor", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "accountant,Accountant");
param_info_insert("QUILL_JOB_HISTORY_DURATION", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Quill Job History Duration", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "dbmsd,ManagedDatabase");
param_info_insert("APPEND_RANK_VANILLA", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Append Rank Vanilla", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "submit,submit");
param_info_insert("vmware_dir", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Vmware Dir", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "submit,submit");
param_info_insert("DAGMAN_ABORT_DUPLICATES", "", "true", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Dagman Abort Duplicates", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "dagman,dagman_main");
param_info_insert("MASTER_CHECK_NEW_EXEC_INTERVAL", "", "300", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Master Check New Exec Interval", "",
				  "",
				  "");
param_info_insert("STARTD_RESOURCE_PREFIX", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Startd Resource Prefix", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "startd,Resource");
param_info_insert("TCP_UPDATE_COLLECTORS", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Tcp Update Collectors", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "daemon_client,dc_collector");
param_info_insert("LIB", "", "$(RELEASE_DIR)/lib", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Lib", "Pathnames",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "tools,version");
param_info_insert("QUERY_TIMEOUT", "", "60", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Query Timeout", "",
				  "",
				  "");
param_info_insert("CCB_SWEEP_INTERVAL", "", "1200", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "CCB Sweep Interval", "",
				  "",
				  "ccb,ccb");
param_info_insert("AMAZON_GAHP_LOG", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Amazon Gahp Log", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "gridmanager,amazonjob");
param_info_insert("EVENTD_SHUTDOWN_CONSTRAINT", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Eventd Shutdown Constraint", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "eventd,admin_event");
param_info_insert("CONSOLE_DEVICES", "", "mouse, console", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Console Devices", "\"/dev/\" portion of the pathname.",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "sysapi,reconfig");
param_info_insert("DAGMAN_DEBUG_CACHE_ENABLE", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Dagman Debug Cache Enable", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "dagman,dagman_main");
param_info_insert("GSI_DAEMON_NAME", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Gsi Daemon Name", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "io,condor_auth_x509");
param_info_insert("GRIDMANAGER_MAX_PENDING_SUBMITS_PER_RESOURCE", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Gridmanager Max Pending Submits Per Resource", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "gridmanager,baseresource");
param_info_insert("CKPT_SERVER_SOCKET_BUFSIZE", "", "", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Ckpt Server Socket Bufsize", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "ckpt_server,server2");
param_info_insert("MASTER_NAME", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Master Name", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "master,master");
param_info_insert("DAGMAN_RETRY_NODE_FIRST", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Dagman Retry Node First", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "dagman,dagman_main");
param_info_insert("STARTER_JOB_HOOK_KEYWORD", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Starter Job Hook Keyword", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "starter,StarterHookMgr");
param_info_insert("MATCH_TIMEOUT", "", "120", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Match Timeout", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "startd,startd_main");
param_info_insert("SETTABLE_ATTRS_ADVERTISE_SCHEDD", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Settable Attribute Advertise Schedd", "",
				  "",
				  "");
param_info_insert("STARTD_VM_ATTRS", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Startd Vm Attrs", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "startd,startd_main");
param_info_insert("CKPT_PROBE", "", "$(LIBEXEC)/condor_ckpt_probe", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Ckpt Probe", "executable will be here: $(LIBEXEC)/condor_ckpt_probe",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  ",");
param_info_insert("FULL_HOSTNAME", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Full Hostname", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "gridmanager,nordugridjob");
param_info_insert("CKPT_SERVER_MAX_PROCESSES", "", "", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Ckpt Server Max Processes", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "ckpt_server,server2");
param_info_insert("GROUP_AUTOREGROUP", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Group Autoregroup", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "negotiator,matchmaker");
param_info_insert("IP", "", "input", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Ip", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "unit_tests,FTEST_string_to_ip");
param_info_insert("GRID_MONITOR", "", "$(SBIN)/grid_monitor.sh", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Grid Monitor", "Where is the GridManager binary installed\?",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "gridmanager,globusresource");
param_info_insert("TRUNC_MASTER_LOG_ON_OPEN", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Truncate Master Log on Open", "true if the MasterLog should be truncated when it is opened, false otherwise",
				  "http://www.cs.wisc.edu/condor/manual/v7.3/3_3Configuration.html",
				  "ccb,ccb");
param_info_insert("POLLING_INTERVAL", "", "5", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Polling Interval", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "startd,startd_main");
param_info_insert("DAGMAN_SUBMIT_DEPTH_FIRST", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Dagman Submit Depth First", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "dagman,dagman_main");
param_info_insert("MASTER_TIMEOUT_MULTIPLIER", "", "", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Master Timeout Multiplier", "",
				  "",
				  "");
param_info_insert("CHECKPOINT_PLATFORM", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Checkpoint Platform", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "sysapi,reconfig");
param_info_insert("GRID_MONITOR_HEARTBEAT_TIMEOUT", "", "300", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Grid Monitor Heartbeat Timeout", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "gridmanager,globusresource");
param_info_insert("SHADOW_ALLOW_UNSAFE_REMOTE_EXEC", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Shadow Allow Unsafe Remote Exec", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "shadow,pseudo_ops");
param_info_insert("GRIDMANAGER_JM_EXIT_LIMIT", "", "30", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Gridmanager Jm Exit Limit", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "gridmanager,globusjob");
param_info_insert("DEBUG_TIME_FORMAT", "", "%m/%d %H:%M:%S", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Debug Log Time Format", "Time Format for the various debug logs",
				  "",
				  "ccb,ccb");
param_info_insert("EVENTD_INTERVAL", "", "900", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Eventd Interval", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "eventd,eventd");
param_info_insert("FLOCK_COLLECTOR_HOSTS", "", "$(FLOCK_TO)", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Flock Collector Hosts", "the FLOCK_NEGOTIATOR_HOSTS list.",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "schedd,schedd");
param_info_insert("NEGOTIATOR_DEBUG_WAIT", "", "false", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Negotiator Debug Wait", "True if the negotiator should wait for a debugger to attach before doing anything",
				  "",
				  "ccb,ccb");
param_info_insert("ENABLE_USERLOG_LOCKING", "", "true", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Enable Userlog Locking", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "c++_util,read_user_log");
param_info_insert("GSI_DAEMON_CERT", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "GSI Daemon Cert", "",
				  "",
				  "");
param_info_insert("vmware_should_transfer_files", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Vmware Should Transfer Files", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "submit,submit");
param_info_insert("NOT_RESPONDING_TIMEOUT", "", "", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Not Responding Timeout", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "daemon_core,daemon_core");
param_info_insert("CKPT_SERVER_CLASSAD_FILE", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Ckpt Server Classad File", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "ckpt_server,server2");
param_info_insert("TOUCH_LOG_INTERVAL", "", "60", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Touch Log Interval", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "daemon_core,daemon_core_main");
param_info_insert("IS_VALID_CHECKPOINT_PLATFORM", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Is Valid Checkpoint Platform", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "startd,Reqexp");
param_info_insert("ARCH", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Arch", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "starter,starter");
param_info_insert("MAX_ACCOUNTANT_DATABASE_SIZE", "", "1000000", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Max Accountant Database Size", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "accountant,Accountant");
param_info_insert("DBMSMANAGER_NAME", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Dbmsmanager Name", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "dbmsd,DBMSManager");
param_info_insert("XAUTHORITY_USERS", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Xauthority Users", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "kbdd,XInterface");
param_info_insert("SETTABLE_ATTRS_SOAP", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Settable Attribute Soap", "",
				  "",
				  "");
param_info_insert("AMAZON_GAHP_WORKER_MAX_NUM", "", "5", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Amazon Gahp Worker Max Num", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "gridmanager,amazonjob");
param_info_insert("JAVA_BENCHMARK_TIME", "", "2", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Java Benchmark Time", "If this time is zero or undefined, no Java benchmarks will be run.",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "starter,java_detect");
param_info_insert("MANAGE_BANDWIDTH", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Manage Bandwidth", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "ckpt_server,server2");
param_info_insert("CREDD_HOST", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Credd Host", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "credd,win_credd");
param_info_insert("SSHD_ARGS", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Sshd Args", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "c++_util,sshd_wrapper");
param_info_insert("PLUGIN_DIR", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Plugin Dir", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "c++_util,LoadPlugins");
param_info_insert("NEGOTIATOR_SOCKET_BUFSIZE", "", "", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Negotiator Socket Bufsize", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "daemon_core,daemon_core");
param_info_insert("STARTER_LIST", "", "STARTER, STARTER_STANDARD", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Starter List", "Where are the various condor_starter binaries installed\?",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "startd,starter_mgr");
param_info_insert("CKPT_SERVER_CLEAN_INTERVAL", "", "", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Ckpt Server Clean Interval", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "ckpt_server,server2");
param_info_insert("MIN_TRACKING_GID", "", "", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Min Tracking Gid", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "c++_util,proc_family_proxy");
param_info_insert("USE_CKPT_SERVER", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Use Ckpt Server", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "shadow,baseshadow");
param_info_insert("PERSISTENT_CONFIG_DIR", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Persistent Config Dir", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "c++_util,condor_config");
param_info_insert("BIN", "", "$(RELEASE_DIR)/bin", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Bin", "Pathnames",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "daemon_core,daemon_core_main");
param_info_insert("DAGMAN_STORK_SUBMIT_EXE", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Dagman Stork Submit Exe", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "dagman,dagman_main");
param_info_insert("CKPT_SERVER_HOST", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Ckpt Server Host", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "starter,starter");
param_info_insert("KILLING_TIMEOUT", "", "30", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Killing Timeout", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "startd,startd_main");
param_info_insert("EXECUTE_LOGIN_IS_DEDICATED", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Execute Login Is Dedicated", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "starter,tool_daemon_proc");
param_info_insert("SHADOW_RENICE_INCREMENT", "", "", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Shadow Renice Increment", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "schedd,schedd");
param_info_insert("COLLECTOR_TCP_SOCKET_BUFSIZE", "", "128*1024", "0.0.0", "1024,",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Soap SSL Port", "",
				  "",
				  "");
param_info_insert("STARTER_LOG", "", "$(LOG)/StarterLog", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Starter Log", "Log files",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "starter,starter_common");
param_info_insert("NEGOTIATOR_MATCHLIST_CACHING", "", "true", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Negotiator Matchlist Caching", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "negotiator,matchmaker");
param_info_insert("XEN_CONTROLLER", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Xen Controller", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "vm-gahp,xen_type");
param_info_insert("POOL_HISTORY_DIR", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Pool History Dir", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "collector,view_server");
param_info_insert("SETTABLE_ATTRS_DEFAULT", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Settable Attribute Default", "",
				  "",
				  "");
param_info_insert("CREAM_GAHP", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Cream Gahp", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "gridmanager,creamjob");
param_info_insert("SHADOW_DEBUG_WAIT", "", "false", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Shadow Debug Wait", "True if the shadow should wait for a debugger to attach before doing anything",
				  "",
				  "ccb,ccb");
param_info_insert("SOFT_UID_DOMAIN", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Soft Uid Domain", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "starter,starter_common");
param_info_insert("CKPT_SERVER_HOSTS", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Ckpt Server Hosts", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "ckpt_server,server2");
param_info_insert("NET_REMAP_INAGENT", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Net Remap Inagent", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "c++_util,condor_config");
param_info_insert("CONCURRENCY_LIMIT_DEFAULT", "", "2308032", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Concurrency Limit Default", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "accountant,Accountant");
param_info_insert("KEEP_OUTPUT_SANDBOX", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Keep Output Sandbox", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "schedd,schedd");
param_info_insert("AMAZON_GAHP", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Amazon Gahp", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "gridmanager,amazonresource");
param_info_insert("EVENTD_ROUTING_INFO", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Eventd Routing Info", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "eventd,scheduled_event");
param_info_insert("SCHEDD", "", "$(SBIN)/condor_schedd", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Schedd", "Where are the binaries for these daemons\?",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "master,master");
param_info_insert("COUNT_HYPERTHREAD_CPUS", "", "1", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Count Hyperthread Cpus", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "sysapi,test");
param_info_insert("SOAP_SSL_PORT", "", "", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Soap SSL Port", "",
				  "",
				  "");
param_info_insert("SETTABLE_ATTRS_OWNER", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Settable Attribute Owner", "",
				  "",
				  "");
param_info_insert("EVENT_LOG_USE_XML", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Event Log Use Xml", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "c++_util,user_log");
param_info_insert("MAX_JOB_QUEUE_LOG_ROTATIONS", "", "", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Max Job Queue Log Rotations", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "schedd,schedd");
param_info_insert("MASTER_BACKOFF_CONSTANT", "", "9", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Master Backoff Constant", "",
				  "",
				  "");
param_info_insert("MAX_JOBS_SUBMITTED", "", "", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Max Jobs Submitted", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "schedd,schedd");
param_info_insert("PREEN_INTERVAL", "", "86400", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Preen Interval", "",
				  "",
				  "");
param_info_insert("PRIORITY_HALFLIFE", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Priority Halflife", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "accountant,Accountant");
param_info_insert("DATABASE_REINDEX_INTERVAL", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Database Reindex Interval", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "dbmsd,DBMSManager");
param_info_insert("STARTD", "", "$(SBIN)/condor_startd", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Startd", "Where are the binaries for these daemons\?",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "master,master");
param_info_insert("ALLOW_USERS", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Allow Users", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "vm-gahp,vmgahp_config");
param_info_insert("START", "", "$(UWCS_START)", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Start", "When is this machine willing to start a job\?",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "startd,Reqexp");
param_info_insert("GRIDFTP_SERVER", "", "$(LIBEXEC)/globus-gridftp-server", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Gridftp Server", "GRIDFTP_URL_BASE = gsiftp://$(FULL_HOSTNAME)",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "gridmanager,gridftpmanager");
param_info_insert("SETTABLE_ATTRS_DAEMON", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Settable Attribute Daemon", "",
				  "",
				  "");
param_info_insert("vm_cdrom_files", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Vm Cdrom Files", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "submit,submit");
param_info_insert("TRUST_UID_DOMAIN", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Trust Uid Domain", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "starter,starter");
param_info_insert("DEFAULT_RANK_VANILLA", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Default Rank Vanilla", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "submit,submit");
param_info_insert("STARTER_LOCAL", "", "$(SBIN)/condor_starter", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Starter Local", "Where are the various condor_starter binaries installed\?",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "schedd,schedd");
param_info_insert("PROCD", "", "$(SBIN)/condor_procd", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Procd", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "c++_util,proc_family_proxy");
param_info_insert("STARTD_CONTACT_TIMEOUT", "", "45", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Startd Contact Timeout", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "schedd,schedd");
param_info_insert("NO_DNS", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "No Dns", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  ",");
param_info_insert("JAVA_CLASSPATH_SEPARATOR", "", ":", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Java Classpath Separator", "one path element from another:",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "c++_util,java_config");
param_info_insert("JAVA_CLASSPATH_ARGUMENT", "", "-classpath", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Java Classpath Argument", "used to introduce a new classpath:",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "c++_util,java_config");
param_info_insert("MASTER_COLLECTOR_BACKOFF_FACTOR", "", "1", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Master Collector Backoff Factor", "",
				  "",
				  "");
param_info_insert("HA_LOCK_URL", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Ha Lock Url", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "master,daemon");
param_info_insert("REMOTE_PRIO_FACTOR", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Remote Prio Factor", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "accountant,Accountant");
param_info_insert("MAX_EVENT_LOG", "", "1000000", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Max Event Log", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "c++_util,user_log");
param_info_insert("MAX_VIRTUAL_MACHINE_TYPES", "", "10", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Max Virtual Machine Types", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "startd,ResMgr");
param_info_insert("NETWORK_INTERFACE", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Network Interface", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  ",");
param_info_insert("SPOOL", "", "$(LOCAL_DIR)/spool", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Spool", "Pathnames",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "accountant,Accountant");
param_info_insert("QUILL_MANAGE_VACUUM", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Quill Manage Vacuum", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "quill,jobqueuedbmanager");
param_info_insert("HAD_CONNECTION_TIMEOUT", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Had Connection Timeout", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "had,StateMachine");
param_info_insert("NETWORK_MAX_PENDING_CONNECTS", "", "", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Network Max Pending Connects", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "daemon_core,daemon_core");
param_info_insert("USE_PROCESS_GROUPS", "", "true", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Use Process Groups", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "daemon_core,daemon_core");
param_info_insert("CCB_SERVER_READ_BUFFER", "", "2048", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "CCB Server Read Buffer", "",
				  "",
				  "ccb,ccb");
param_info_insert("DROP_CORE_ON_RECONFIG", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Drop Core On Reconfig", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "daemon_core,daemon_core_main");
param_info_insert("DAGMAN_RETRY_SUBMIT_FIRST", "", "true", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Dagman Retry Submit First", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "dagman,dagman_main");
param_info_insert("DAGMAN_PROHIBIT_MULTI_JOBS", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Dagman Prohibit Multi Jobs", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "dagman,dagman_main");
param_info_insert("XEN_DEVICE_TYPE_FOR_VT", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Xen Device Type For Vt", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "vm-gahp,xen_type");
param_info_insert("APPEND_REQ_VANILLA", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Append Req Vanilla", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "submit,submit");
param_info_insert("REPLICATION_LIST", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Replication List", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "had,StateMachine");
param_info_insert("USE_VISIBLE_DESKTOP", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Use Visible Desktop", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "daemon_core,daemon_core");
param_info_insert("GRIDMANAGER", "", "$(SBIN)/condor_gridmanager", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Gridmanager", "Where is the GridManager binary installed\?",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "schedd,grid_universe");
param_info_insert("VM_NETWORKING", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Vm Networking", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "vm-gahp,vmgahp_config");
param_info_insert("QUILL_DB_QUERY_PASSWORD", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Quill Db Query Password", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "q,queue");
param_info_insert("JOB_ROUTER_NAME", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Job Router Name", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "schedd,JobRouter");
param_info_insert("QUILL_DB_TYPE", "", "PGSQL", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Quill Db Type", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "c++_util,jobqueuesnapshot");
param_info_insert("MASTER_FLAG", "", "", "0.0.0", "",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Master Flag", "",
				  "",
				  "");
param_info_insert("UID_DOMAIN", "", "$(FULL_HOSTNAME)", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Uid Domain", "to specify that each machine has its own UID space.",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "starter,starter");
param_info_insert("BIOTECH", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Biotech", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "schedd,qmgmt");
param_info_insert("GRIDFTP_SERVER_WRAPPER", "", "$(LIBEXEC)/gridftp_wrapper.sh", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Gridftp Server Wrapper", "GRIDFTP_URL_BASE = gsiftp://$(FULL_HOSTNAME)",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "gridmanager,gridftpmanager");
param_info_insert("NEGOTIATOR_STATE_FILE", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Negotiator State File", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "had,AbstractReplicatorStateMachine");
param_info_insert("SMTP_SERVER", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Smtp Server", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  ",");
param_info_insert("TEMP_DIR", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Temp Dir", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "c++_util,directory");
param_info_insert("EVENT_LIST", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Event List", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "eventd,eventd");
param_info_insert("SETTABLE_ATTRS_ADVERTISE_MASTER", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Settable Attribute Advertise Master", "",
				  "",
				  "");
param_info_insert("VM_GAHP_SERVER", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Vm Gahp Server", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "vm-gahp,vmgahp");
param_info_insert("MAX_XML_LOG", "", "", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Max Xml Log", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "c++_util,file_xml");
param_info_insert("QUILL_NAME", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Quill Name", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "q,queue");
param_info_insert("HAD_UPDATE_INTERVAL", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Had Update Interval", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "had,StateMachine");
param_info_insert("GLITE_LOCATION", "", "$(LIB)/glite", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Glite Location", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "gridmanager,gahp-client");
param_info_insert("JOB_PROXY_OVERRIDE_FILE", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Job Proxy Override File", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "gridmanager,proxymanager");
param_info_insert("ENCRYPT_EXECUTE_DIRECTORY", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Encrypt Execute Directory", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "starter,starter");
param_info_insert("VM_GAHP_CONFIG", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Vm Gahp Config", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "startd,vmuniverse_mgr");
param_info_insert("NEGOTIATE_ALL_JOBS_IN_CLUSTER", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Negotiate All Jobs In Cluster", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "schedd,schedd");
param_info_insert("DEFAULT_DOMAIN_NAME", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Default Domain Name", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  ",");
param_info_insert("NEGOTIATOR_MAX_TIME_PER_SUBMITTER", "", "31536000", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Negotiator Max Time Per Submitter", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "negotiator,matchmaker");
param_info_insert("DAGMAN_IGNORE_DUPLICATE_JOB_EXECUTION", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Dagman Ignore Duplicate Job Execution", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "dagman,dagman_main");
param_info_insert("EVENTD_MAX_PREPARATION", "", "", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Eventd Max Preparation", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "eventd,eventd");
param_info_insert("USER_MAPFILE", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "User Mapfile", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "daemon_core,daemon_core");
param_info_insert("GRIDMANAGER_PER_JOB", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Gridmanager Per Job", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "schedd,schedd");
param_info_insert("GRID_MONITOR_RETRY_DURATION", "", "900", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Grid Monitor Retry Duration", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "gridmanager,globusresource");
param_info_insert("SETTABLE_ATTRS_CLIENT", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Settable Attribute Client", "",
				  "",
				  "");
param_info_insert("DAGMAN_CONDOR_RM_EXE", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Dagman Condor Rm Exe", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "dagman,dagman_main");
param_info_insert("SEC_CLAIMTOBE_INCLUDE_DOMAIN", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Sec Claimtobe Include Domain", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "io,condor_auth_claim");
param_info_insert("CM_IP_ADDR", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Cm Ip Addr", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "daemon_client,daemon");
param_info_insert("SCHEDD_PREEMPTION_RANK", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Schedd Preemption Rank", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "schedd,dedicated_scheduler");
param_info_insert("POOL_HISTORY_SAMPLING_INTERVAL", "", "60", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Pool History Sampling Interval", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "collector,view_server");
param_info_insert("GRIDMANAGER_GAHP_CALL_TIMEOUT_CONDOR", "", "", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Gridmanager Gahp Call Timeout Condor", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "gridmanager,condorjob");
param_info_insert("TRUNC_SHADOW_LOG_ON_OPEN", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Truncate Shadow Log on Open", "true if the ShadowLog should be truncated when it is opened, false otherwise",
				  "http://www.cs.wisc.edu/condor/manual/v7.3/3_3Configuration.html",
				  "ccb,ccb");
param_info_insert("SSHD", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Sshd", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "c++_util,sshd_wrapper");
param_info_insert("HAD_USE_PRIMARY", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Had Use Primary", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "had,StateMachine");
param_info_insert("HAD_USE_REPLICATION", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Had Use Replication", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "had,StateMachine");
param_info_insert("ENABLE_GRID_MONITOR", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Enable Grid Monitor", "than is normally possible.",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "gridmanager,globusjob");
param_info_insert("START_SCHEDULER_UNIVERSE", "", "True", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Start Scheduler Universe", "When should a scheduler universe job be allowed to start\?",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "schedd,schedd");
param_info_insert("SETTABLE_ATTRS_CONFIG", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Settable Attribute Config", "",
				  "",
				  "");
param_info_insert("ALWAYS_CLOSE_USERLOG", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Always Close Userlog", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "c++_util,read_user_log");
param_info_insert("DNS_CACHE_REFRESH", "", "28800", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "DNS Cache Refresh", "",
				  "",
				  "");
param_info_insert("VM_VERSION", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Vm Version", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "vm-gahp,vmgahp_config");
param_info_insert("VM_MAX_NUMBER", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Vm Max Number", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "startd,vmuniverse_mgr");
param_info_insert("JAVA_MAXHEAP_ARGUMENT", "", "-Xmx", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Java Maxheap Argument", "typically 64 MB.  The default (-Xmx) works with the Sun JVM.",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "c++_util,java_config");
param_info_insert("TRUNC_SCHED_LOG_ON_OPEN", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Truncate Sched Log on Open", "true if the SchedLog should be truncated when it is opened, false otherwise",
				  "http://www.cs.wisc.edu/condor/manual/v7.3/3_3Configuration.html",
				  "ccb,ccb");
param_info_insert("DAGMAN_STORK_RM_EXE", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Dagman Stork Rm Exe", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "dagman,dagman_main");
param_info_insert("JOB_ROUTER_SOURCE_JOB_CONSTRAINT", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Job Router Source Job Constraint", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "schedd,JobRouter");
param_info_insert("SSH_KEYGEN_ARGS", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Ssh Keygen Args", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "c++_util,sshd_wrapper");
param_info_insert("GRIDMANAGER_CONNECT_FAILURE_RETRY_COUNT", "", "3", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Gridmanager Connect Failure Retry Count", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "gridmanager,mirrorjob");
param_info_insert("CONDOR_SUPPORT_EMAIL", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Condor Support Email", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  ",");
param_info_insert("PREEN_ARGS", "", "-m -r", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Preen Args", "one from this setting.",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "master,master");
param_info_insert("SEC_INVALIDATE_SESSIONS_VIA_TCP", "", "true", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Sec Invalidate Sessions Via Tcp", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "daemon_core,daemon_core");
param_info_insert("GLEXEC_JOB", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Glexec Job", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "daemon_core,daemon_core");
param_info_insert("STARTD_SENDS_ALIVES", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Startd Sends Alives", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "startd,command");
param_info_insert("CRED_MIN_TIME_LEFT", "", "120", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Cred Min Time Left", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  ",");
param_info_insert("AMAZON_GAHP_DEBUG", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Amazon Gahp Debug", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "gridmanager,amazonjob");
param_info_insert("TMP_DIR", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Tmp Dir", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "c++_util,directory");
param_info_insert("PREEN_ADMIN", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Preen Admin", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "tools,preen");
param_info_insert("XEN_SCRIPT", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Xen Script", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "vm-gahp,xen_type");
param_info_insert("NORDUGRID_GAHP", "", "$(SBIN)/nordugrid_gahp", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Nordugrid Gahp", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "gridmanager,nordugridjob");
param_info_insert("PID_SNAPSHOT_INTERVAL", "", "15", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Pid Snapshot Interval", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "daemon_core,HookClientMgr");
param_info_insert("COLLECTOR_NAME", "", "An adorable little test pool", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Collector Name", "the UW-Madison Computer Science Condor Pool is ``UW-Madison CS\'\'.",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "collector,collector");
param_info_insert("xen_kernel", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Xen Kernel", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "submit,submit");
param_info_insert("COMPRESS_PERIODIC_CKPT", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Compress Periodic Ckpt", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "shadow,shadow");
param_info_insert("ALL_DEBUG", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "All Debug", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  ",");
param_info_insert("xen_kernel_params", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Xen Kernel Params", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "submit,submit");
param_info_insert("GSI_DAEMON_PROXY", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "GSI Daemon Proxy", "",
				  "",
				  "");
param_info_insert("ALWAYS_VM_UNIV_USE_NOBODY", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Always Vm Univ Use Nobody", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "starter,jic_shadow");
param_info_insert("GROUP_DYNAMIC_MACH_CONSTRAINT", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Group Dynamic Mach Constraint", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "negotiator,matchmaker");
param_info_insert("GRIDFTP_URL_BASE", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Gridftp Url Base", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "gridmanager,gridftpmanager");
param_info_insert("GRIDSHELL", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Gridshell", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "gridmanager,globusjob");
param_info_insert("QUILL_HISTORY_DURATION", "", "180", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Quill History Duration", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "quill,jobqueuedbmanager");
param_info_insert("PREEMPTION_RANK", "", "$(UWCS_PREEMPTION_RANK)", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Preemption Rank", "(e.g. if a job is in the pre-empting stage for too long)",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "negotiator,matchmaker");
param_info_insert("XEN_BRIDGE_SCRIPT", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Xen Bridge Script", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "vm-gahp,xen_type");
param_info_insert("SOAP_SSL_CA_FILE", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Soap Ssl Ca File", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "daemon_core,soap_core");
param_info_insert("CCB_RECONNECT_FILE", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "CCB Reconnect File", "",
				  "",
				  "ccb,ccb");
param_info_insert("FILESYSTEM_DOMAIN", "", "$(FULL_HOSTNAME)", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Filesystem Domain", "to specify that each machine has its own file system.",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "starter,starter");
param_info_insert("ENABLE_HISTORY_ROTATION", "", "true", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Enable History Rotation", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "schedd,schedd");
param_info_insert("STARTD_AD_REEVAL_EXPR", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Startd Ad Reeval Expr", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "negotiator,matchmaker");
param_info_insert("HISTORY", "", "$(SPOOL)/history", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "History", "will be created.",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "tools,preen");
param_info_insert("WANT_UDP_COMMAND_SOCKET", "", "true", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Want Udp Command Socket", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "daemon_core,daemon_core");
param_info_insert("GRIDMANAGER_CONTACT_SCHEDD_DELAY", "", "5", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Gridmanager Contact Schedd Delay", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "gridmanager,gridmanager");
param_info_insert("MAIL", "", "/usr/bin/mail", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Mail", "means you want to specify a subject:",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "MAIL,");
param_info_insert("MASTER_GCB_RECONNECT_TIMEOUT", "", "300", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Master Gcb Reconnect Timeout", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "master,master");
param_info_insert("VM_NETWORKING_MAC_PREFIX", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Vm Networking Mac Prefix", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "vm-gahp,vmgahp_config");
param_info_insert("NET_REMAP_ENABLE", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Net Remap Enable", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  ",");
param_info_insert("SCHEDD_MIN_INTERVAL", "", "5", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Schedd Min Interval", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "schedd,schedd");
param_info_insert("WINDOWS_SOFTKILL", "", "$(SBIN)/condor_softkill", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Windows Softkill", "is not enabled) to help when sending soft kills.",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "daemon_core,daemon_core");
param_info_insert("SHADOW_LOCK", "", "$(LOCK)/ShadowLock", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Lock file for locking the shadow log file", "",
				  "",
				  "ccb,ccb");
param_info_insert("JAVA", "", "/usr/bin/java", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Java", "empty or incorrect.",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "c++_util,java_config");
param_info_insert("STARTD_CLAIM_ID_FILE", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Startd Claim Id File", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "c++_util,misc_utils");
param_info_insert("DAGMAN_INSERT_SUB_FILE", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Dagman Insert Sub File", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "dagman,condor_submit_dag");
param_info_insert("STARTER_LOCAL_LOGGING", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Starter Local Logging", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "starter,starter");
param_info_insert("EVENT_LOG_JOB_AD_INFORMATION_ATTRS", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Event Log Job Ad Information Attrs", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "c++_util,user_log");
param_info_insert("ACCOUNTANT_LOCAL_DOMAIN", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Accountant Local Domain", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "accountant,Accountant");
param_info_insert("VM_STATUS_MAX_ERROR", "", "", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Vm Status Max Error", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "starter,vm_proc");
param_info_insert("ADD_WINDOWS_FIREWALL_EXCEPTION", "", "true", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Add Windows Firewall Exception", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "master,master");
param_info_insert("KEEP_POOL_HISTORY", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Keep Pool History", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "collector,view_server");
param_info_insert("BACKFILL_SYSTEM", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Backfill System", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "startd,ResMgr");
param_info_insert("PROCD_LOG", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Procd Log", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "c++_util,proc_family_proxy");
param_info_insert("TRUNC_NEGOTIATOR_LOG_ON_OPEN", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Truncate Negotiator Log on Open", "true if the NegotiatorLog should be truncated when it is opened, false otherwise",
				  "http://www.cs.wisc.edu/condor/manual/v7.3/3_3Configuration.html",
				  "ccb,ccb");
param_info_insert("COLLECTOR_HOST", "", "$(CONDOR_HOST)", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Collector Host", "COLLECTOR_HOST = $(CONDOR_HOST):1234",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "gridmanager,amazonjob");
param_info_insert("OUT_LOWPORT", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Out Lowport", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  ",");
param_info_insert("START_MASTER", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Start Master", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "master,master");
param_info_insert("GLEXEC_STARTER", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Glexec Starter", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "shadow,remoteresource");
param_info_insert("SOAP_SSL_SERVER_KEYFILE_PASSWORD", "", "96hoursofmattslife", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Soap Ssl Server Keyfile Password", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "daemon_core,soap_core");
param_info_insert("STARTER_DEBUG", "", "D_NODATE", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Starter Debug", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "starter,starter_common");
param_info_insert("LOCAL_UNIVERSE_JOB_CLEANUP_RETRY_DELAY", "", "30", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Local Universe Job Cleanup Retry Delay", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "starter,jic_local_schedd");
param_info_insert("DAGMAN_ALLOW_EVENTS", "", "", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Dagman Allow Events", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "dagman,dagman_main");
param_info_insert("USE_CLONE_TO_CREATE_PROCESSES", "", "true", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Use Clone To Create Processes", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "daemon_core,daemon_core");
param_info_insert("SLOW_CKPT_SPEED", "", "", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Slow Ckpt Speed", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "shadow,shadow");
param_info_insert("DAGMAN_LOG_ON_NFS_IS_ERROR", "", "true", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Dagman Log On Nfs Is Error", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "dagman,dagman_multi_dag");
param_info_insert("EVENTD_CAPACITY_INFO", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Eventd Capacity Info", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "eventd,scheduled_event");
param_info_insert("MASTER_NEW_BINARY_DELAY", "", "120", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Master New Binary Delay", "",
				  "",
				  "");
param_info_insert("FILE_LOCK_VIA_MUTEX", "", "true", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "File Lock Via Mutex", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  ",");
param_info_insert("FLAVOR", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Flavor", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "submit,submit");
param_info_insert("GRIDMANAGER_ARGS", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Gridmanager Args", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "schedd,grid_universe");
param_info_insert("MASTER_DEBUG_WAIT", "", "false", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Master Debug Wait", "True if the master should wait for a debugger to attach before doing anything",
				  "",
				  "ccb,ccb");
param_info_insert("GRIDMANAGER_MAX_SUBMITTED_JOBS_PER_RESOURCE", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Gridmanager Max Submitted Jobs Per Resource", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "gridmanager,baseresource");
param_info_insert("ENCRYPT_SECRETS", "", "true", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Encrypt Secrets", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "io,cedar_no_ckpt");
param_info_insert("GRIDMANAGER_GAHPCLIENT_DEBUG_SIZE", "", "", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Gridmanager Gahpclient Debug Size", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "gridmanager,gahp-client");
param_info_insert("SANDBOX_TRANSFER_METHOD", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Sandbox Transfer Method", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "submit,submit");
param_info_insert("SEC_SESSION_DURATION_SLOP", "", "20", "0.0.0", "0,",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Sec Session Duration Slop", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "daemon_core,daemon_core");
param_info_insert("SEC_DEFAULT_SESSION_DURATION", "", "3600", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Sec Default Session Duration", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "daemon_core,daemon_core_main");
param_info_insert("HAD_LIST", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Had List", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "had,StateMachine");
param_info_insert("JOB_RENICE_INCREMENT", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Job Renice Increment", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "starter,os_proc");
param_info_insert("NETWORK_HORIZON", "", "300", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Network Horizon", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "ckpt_server,server2");
param_info_insert("SEC_DEBUG_PRINT_KEYS", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Sec Debug Print Keys", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "daemon_core,daemon_core");
param_info_insert("QUILL_DB_USER", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Quill Db User", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "tt,jobqueuedbmanager");
param_info_insert("RESERVED_SWAP", "", "5", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Reserved Swap", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "shadow,baseshadow");
param_info_insert("VALID_SPOOL_FILES", "", "job_queue.log, job_queue.log.tmp, history, \\", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Valid Spool Files", "What files should condor_preen leave in the spool directory\?",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "tools,preen");
param_info_insert("DISABLE_AUTHENTICATION_IP_CHECK", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Disable Authentication Ip Check", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "daemon_core,daemon_core");
param_info_insert("MASTER_RECOVER_FACTOR", "", "300", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Master Recover Ceiling", "",
				  "",
				  "");
param_info_insert("VM_UNIV_NOBODY_USER", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Vm Univ Nobody User", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "starter,jic_shadow");
param_info_insert("DAGMAN_CONFIG_FILE", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Dagman Config File", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "dagman,dagman_main");
param_info_insert("XEN_DEFAULT_KERNEL", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Xen Default Kernel", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "vm-gahp,xen_type");
param_info_insert("DISCONNECTED_KEYBOARD_IDLE_BOOST", "", "1200", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Disconnected Keyboard Idle Boost", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "startd,startd_main");
param_info_insert("SCP", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Scp", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "c++_util,sshd_wrapper");
param_info_insert("VM_MAX_MEMORY", "", "", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Vm Max Memory", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "vm-gahp,vmgahp_config");
param_info_insert("AMAZON_HTTP_PROXY", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Amazon Http Proxy", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "gridmanager,gahp-client");
param_info_insert("XEN_ALLOW_HARDWARE_VT_SUSPEND", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Xen Allow Hardware Vt Suspend", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "vm-gahp,xen_type");
param_info_insert("APPEND_REQ_VM", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Append Req Vm", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "submit,submit");
param_info_insert("NEGOTIATOR_INTERVAL", "", "300", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Negotiator Interval", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "negotiator,matchmaker");
param_info_insert("CONDOR_GAHP", "", "$(SBIN)/condor_c-gahp", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Condor Gahp", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "gridmanager,condorresource");
param_info_insert("MASTER_COLLECTOR_RECOVER_FACTOR", "", "1", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Master Collector Recover Factor", "",
				  "",
				  "");
param_info_insert("MAX_JOB_MIRROR_UPDATE_LAG", "", "600", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Max Job Mirror Update Lag", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "schedd,JobRouter");
param_info_insert("PERIODIC_EXPR_INTERVAL", "", "300", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Periodic Expr Interval", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "gridmanager,basejob");
param_info_insert("STARTD_SLOT_EXPRS", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Startd Slot Exprs", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "startd,startd_main");
param_info_insert("COLLECTOR_FLAG", "", "", "0.0.0", "",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Collector Flag", "",
				  "",
				  "");
param_info_insert("GRIDMANAGER_MAX_PENDING_SUBMITS", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Gridmanager Max Pending Submits", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "gridmanager,baseresource");
param_info_insert("NEGOTIATOR_MAX_TIME_PER_PIESPIN", "", "31536000", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Negotiator Max Time Per Piespin", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "negotiator,matchmaker");
param_info_insert("STARTD_HAS_BAD_UTMP", "", "", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Startd Has Bad Utmp", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "sysapi,reconfig");
param_info_insert("NEGOTIATOR_IGNORE_USER_PRIORITIES", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Negotiator Ignore User Priorities", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "negotiator,matchmaker");
param_info_insert("SUBMIT_EXPRS", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Submit Exprs", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "submit,submit");
param_info_insert("RESTART_PROCD_ON_ERROR", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Restart Procd On Error", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "c++_util,proc_family_proxy");
param_info_insert("USE_NFS", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Use Nfs", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "shadow,pseudo_ops");
param_info_insert("CCB_POLLING_MAX_INTERVAL", "", "600", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "CCB Polling Maximum Interval", "Run at least this often",
				  "",
				  "ccb,ccb");
param_info_insert("xen_transfer_files", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Xen Transfer Files", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "submit,submit");
param_info_insert("PUBLISH_OBITUARIES", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Publish Obituaries", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "master,master");
param_info_insert("vmware_snapshot_disk", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Vmware Snapshot Disk", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "submit,submit");
param_info_insert("EVENTD_SHUTDOWN_SLOW_START_INTERVAL", "", "", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Eventd Shutdown Slow Start Interval", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "eventd,scheduled_event");
param_info_insert("QUILL_RESOURCE_HISTORY_DURATION", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Quill Resource History Duration", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "dbmsd,ManagedDatabase");
param_info_insert("CCB_ADDRESS", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "CCB Address", "",
				  "",
				  "");
param_info_insert("TCP_FORWARDING_HOST", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Tcp Forwarding Host", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "daemon_core,daemon_core");
param_info_insert("SCHEDD_NAME", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Schedd Name", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "quill,jobqueuedbmanager");
param_info_insert("QUILL_MAINTAIN_SOFT_STATE", "", "true", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Quill Maintain Soft State", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "tt,ttmanager");
param_info_insert("VM_NETWORKING_TYPE", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Vm Networking Type", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "vm-gahp,vmgahp_config");
param_info_insert("ENABLE_PERSISTENT_CONFIG", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Enable Persistent Config", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "c++_util,condor_config");
param_info_insert("MAX_SHADOW_EXCEPTIONS", "", "5", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Max Shadow Exceptions", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "schedd,schedd");
param_info_insert("SHADOW_SIZE_ESTIMATE", "", "125", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Shadow Size Estimate", "Specified in kilobytes.",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "schedd,schedd");
param_info_insert("CONDOR_CREDENTIAL_DIR", "", "/tmp", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Condor Credential Directory", "",
				  "",
				  "");
param_info_insert("STORK_ENABLE_DEPRECATED_USERLOG", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Stork Enable Deprecated Userlog", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "stork,dap_logger");
param_info_insert("LOCAL_DIR", "", "$(RELEASE_DIR)/hosts/$(HOSTNAME)", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Local Dir", "LOCAL_DIR\t\t= $(TILDE)",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "collector,view_server");
param_info_insert("GRIDMANAGER_MAX_PENDING_REQUESTS", "", "50", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Gridmanager Max Pending Requests", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "gridmanager,gahp-client");
param_info_insert("SCHEDD_CRON_NAME", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Schedd Cron Name", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "schedd,schedd_cronmgr");
param_info_insert("SHADOW_JOB_CLEANUP_RETRY_DELAY", "", "30", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Shadow Job Cleanup Retry Delay", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "shadow,baseshadow");
param_info_insert("SHUTDOWN_FAST_TIMEOUT", "", "300", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Shutdown Fast Timeout", "",
				  "",
				  "");
param_info_insert("KERBEROS_MAP_FILE", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Kerberos Map File", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "io,condor_auth_kerberos");
param_info_insert("USE_SCRIPT_TO_CREATE_CONFIG", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Use Script To Create Config", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "vm-gahp,vm_type");
param_info_insert("GRIDMANAGER_CONNECT_FAILURE_RETRY_INTERVAL", "", "5", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Gridmanager Connect Failure Retry Interval", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "gridmanager,globusjob");
param_info_insert("NEGOTIATOR_CONSIDER_PREEMPTION", "", "true", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Negotiator Consider Preemption", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "negotiator,matchmaker");
param_info_insert("DAEMON_LIST", "", "MASTER, STARTD, SCHEDD", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Daemon List", "Daemons you want the master to keep running for you:",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "master,master");
param_info_insert("GRIDMANAGER_MAX_WS_DESTROYS_PER_RESOURCE", "", "", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Gridmanager Max Ws Destroys Per Resource", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "gridmanager,gt4resource");
param_info_insert("STARTD_FACTORY_SCRIPT_BOOT_PARTITION", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Startd Factory Script Boot Partition", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "startd_factory,startd_factory");
param_info_insert("FLOCK_HOSTS", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Flock Hosts", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "schedd,schedd");
param_info_insert("COMPRESS_VACATE_CKPT", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Compress Vacate Ckpt", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "shadow,shadow");
param_info_insert("NET_REMAP_SERVICE", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Net Remap Service", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "c++_util,condor_config");
param_info_insert("CLAIM_RECYCLING_CONSIDER_LIMITS", "", "true", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Claim Recycling Consider Limits", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "schedd,qmgmt");
param_info_insert("SEC_PASSWORD_FILE", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Sec Password File", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "c++_util,store_cred");
param_info_insert("ENABLE_RUNTIME_CONFIG", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Enable Runtime Config", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "c++_util,condor_config");
param_info_insert("REQUIRE_LOCAL_CONFIG_FILE", "", "true", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Require Local Config File", "If not specificed, the default is True",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "c++_util,condor_config");
param_info_insert("POLLING_PERIOD", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Polling Period", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "schedd,Scheduler");
param_info_insert("CKPT_SERVER_CHECK_PARENT_INTERVAL", "", "120", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Ckpt Server Check Parent Interval", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "ckpt_server,server2");
param_info_insert("CCB_SERVER_WRITE_BUFFER", "", "2048", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "CCB Server Write Buffer", "",
				  "",
				  "ccb,ccb");
param_info_insert("STARTER_CHOOSES_CKPT_SERVER", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Starter Chooses Ckpt Server", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "shadow,shadow");
param_info_insert("STORK_TMP_CRED_DIR", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Stork Tmp Cred Dir", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "stork,dap_server");
param_info_insert("VM_SOFT_SUSPEND", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Vm Soft Suspend", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "starter,vm_proc");
param_info_insert("CREATE_CORE_FILES", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Create Core Files", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "daemon_core,daemon_core_main");
param_info_insert("JOB_ROUTER_POLLING_PERIOD", "", "10", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Job Router Polling Period", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "schedd,JobRouter");
param_info_insert("COLLECTOR_REPEAT_STARTD_ADS", "", "", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Collector Repeat Startd Ads", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "collector,collector_engine");
param_info_insert("SOAP_SSL_CA_DIR", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Soap Ssl Ca Dir", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "daemon_core,soap_core");
param_info_insert("DEFAULT_RANK", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Default Rank", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "submit,submit");
param_info_insert("STARTD_FACTORY_SCRIPT_DESTROY_PARTITION", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Startd Factory Script Destroy Partition", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "startd_factory,startd_factory");
param_info_insert("PREEMPTION_REQUIREMENTS", "", "$(UWCS_PREEMPTION_REQUIREMENTS)", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Preemption Requirements", "(e.g. if a job is in the pre-empting stage for too long)",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "q,queue");
param_info_insert("VMWARE_SCRIPT", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Vmware Script", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "vm-gahp,vmware_type");
param_info_insert("EVENT_LOG", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Event Log", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "c++_util,user_log");
param_info_insert("STARTD_SLOT_ATTRS", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Startd Slot Attrs", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "startd,startd_main");
param_info_insert("DC_DAEMON_LIST", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Dc Daemon List", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "master,master");
param_info_insert("GLOBAL_JOB_ID_WITH_TIME", "", "true", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Global Job Id With Time", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "schedd,qmgmt");
param_info_insert("RunBenchmarks", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Runbenchmarks", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "startd,startd_main");
param_info_insert("TRUNC_MATCH_LOG_ON_OPEN", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Truncate Match Log on Open", "true if the MatchLog should be truncated when it is opened, false otherwise",
				  "http://www.cs.wisc.edu/condor/manual/v7.3/3_3Configuration.html",
				  "ccb,ccb");
param_info_insert("MASTER_UPDATE_INTERVAL", "", "300", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Master Update Interval", "",
				  "",
				  "");
param_info_insert("mpi_master_ip", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Mpi Master Ip", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "schedd,dedicated_scheduler");
param_info_insert("STARTD_FACTORY_SCRIPT_GENERATE_PARTITION", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Startd Factory Script Generate Partition", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "startd_factory,startd_factory");
param_info_insert("PRIVSEP_ENABLED", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Privsep Enabled", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "privsep,privsep_client.WINDOWS");
param_info_insert("DAGMAN_STARTUP_CYCLE_DETECT", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Dagman Startup Cycle Detect", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "dagman,dagman_main");
param_info_insert("FAKE_CREATE_THREAD", "", "true", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Fake Create Thread", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "daemon_core,daemon_core");
param_info_insert("QUILL_SHOULD_REINDEX", "", "true", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Quill Should Reindex", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "quill,jobqueuedbmanager");
param_info_insert("EXECUTE", "", "$(LOCAL_DIR)/execute", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Execute", "Pathnames",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "starter,starter");
param_info_insert("DOMAIN", "", "input_domain", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Domain", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "unit_tests,FTEST_host_in_domain");
param_info_insert("SBIN", "", "$(RELEASE_DIR)/sbin", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Sbin", "Pathnames",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "startd,starter_mgr");
param_info_insert("PRIVATE_NETWORK_NAME", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Private Network Name", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "daemon_core,daemon_core");
param_info_insert("MASTER_COLLECTOR_BACKOFF_CEILING", "", "1", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Master Collector Backoff Ceiling", "",
				  "",
				  "");
param_info_insert("CONDOR_GATEKEEPER", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Condor Gatekeeper", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "io,credential");
param_info_insert("FLOCK_NEGOTIATOR_HOSTS", "", "$(FLOCK_TO)", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Flock Negotiator Hosts", "the FLOCK_NEGOTIATOR_HOSTS list.",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "schedd,schedd");
param_info_insert("CRED_SUPER_USERS", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Cred Super Users", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "credd,credd");
param_info_insert("DAGMAN_RESET_RETRIES_UPON_RESCUE", "", "true", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Dagman Reset Retries Upon Rescue", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "dagman,dag");
param_info_insert("IN_LOWPORT", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "In Lowport", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  ",");
param_info_insert("STARTD_NAME", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Startd Name", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "startd,startd_main");
param_info_insert("STARTD_FACTORY_SCRIPT_SHUTDOWN_PARTITION", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Startd Factory Script Shutdown Partition", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "startd_factory,startd_factory");
param_info_insert("job_lease_duration", "", "ATTR_JOB_LEASE_DURATION", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Job Lease Duration", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "submit,submit");
param_info_insert("PROCD_MAX_SNAPSHOT_INTERVAL", "", "60", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Procd Max Snapshot Interval", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "c++_util,proc_family_proxy");
param_info_insert("CKPT_SERVER_MAX_RESTORE_PROCESSES", "", "", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Ckpt Server Max Restore Processes", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "ckpt_server,server2");
param_info_insert("NICE_USER_PRIO_FACTOR", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Nice User Prio Factor", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "accountant,Accountant");
param_info_insert("GAHP", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Gahp", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "gridmanager,gahp-client");
param_info_insert("PASSWD_CACHE_REFRESH", "", "300", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Passwd Cache Refresh", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "c++_util,passwd_cache");
param_info_insert("TRUNC_COLLECTOR_LOG_ON_OPEN", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Truncate Collector Log on Open", "true if the CollectorLog should be truncated when it is opened, false otherwise",
				  "http://www.cs.wisc.edu/condor/manual/v7.3/3_3Configuration.html",
				  "ccb,ccb");
param_info_insert("TRANSFERD", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Transferd", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "schedd,tdman");
param_info_insert("HA_POLL_PERIOD", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Ha Poll Period", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "master,daemon");
param_info_insert("SUBMIT_SKIP_FILECHECKS", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Submit Skip Filechecks", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "submit,submit");
param_info_insert("STARTD_NOCLAIM_SHUTDOWN", "", "", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Startd Noclaim Shutdown", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "startd,startd_main");
param_info_insert("USE_GID_PROCESS_TRACKING", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Use Gid Process Tracking", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "c++_util,proc_family_proxy");
param_info_insert("STARTD_SHOULD_WRITE_CLAIM_ID_FILE", "", "true", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Startd Should Write Claim Id File", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "startd,claim");
param_info_insert("VM_GAHP_SEND_ALL_CLASSAD", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Vm Gahp Send All Classad", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "starter,vm_gahp_server");
param_info_insert("ENABLE_SOAP_SSL", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Enable Soap Ssl", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "daemon_core,soap_core");
param_info_insert("STARTD_FACTORY_SCRIPT_QUERY_WORK_LOADS", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Startd Factory Script Query Work Loads", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "startd_factory,startd_factory");
param_info_insert("EVENTD_SIMULATE_SHUTDOWNS", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Eventd Simulate Shutdowns", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "eventd,scheduled_event");
param_info_insert("SETTABLE_ATTRS_READ", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Settable Attribute Read", "",
				  "",
				  "");
param_info_insert("CKPT_SERVER_DIR", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Ckpt Server Dir", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "ckpt_server,server2");
param_info_insert("CONFIG_SERVER_FILE", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Config Server File", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "master,master");
param_info_insert("ALLOW_ADMIN_COMMANDS", "", "true", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Allow Admin Commands", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "master,master");
param_info_insert("HA_LOCK_HOLD_TIME", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Ha Lock Hold Time", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "master,daemon");
param_info_insert("ORACLE_HOME", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Oracle Home", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "gridmanager,oraclejob");
param_info_insert("CONDOR_VIEW_HOST", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Condor View Host", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "collector,collector");
param_info_insert("ACCOUNTANT_HOST", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Accountant Host", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "startd,startd_main");
param_info_insert("QUILL", "", "$(SBIN)/condor_quill", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Quill", "Where is the Quill binary installed and what arguments should be passed\?",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "master,master");
param_info_insert("EVENTD_SHUTDOWN_CLEANUP_INTERVAL", "", "3600", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Eventd Shutdown Cleanup Interval", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "eventd,scheduled_event");
param_info_insert("LOCAL_UNIVERSE_MAX_JOB_CLEANUP_RETRIES", "", "5", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Local Universe Max Job Cleanup Retries", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "starter,jic_local_schedd");
param_info_insert("EMAIL_DOMAIN", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Email Domain", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "c++_util,email");
param_info_insert("ABORT_ON_EXCEPTION", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Abort On Exception", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "c++_util,condor_config");
param_info_insert("SCHED_DEBUG_WAIT", "", "false", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Sched Debug Wait", "True if the schedd should wait for a debugger to attach before doing anything",
				  "",
				  "ccb,ccb");
param_info_insert("DAGMAN_PENDING_REPORT_INTERVAL", "", "", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Dagman Pending Report Interval", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "dagman,dagman_main");
param_info_insert("DEFAULT_UNIVERSE", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Default Universe", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "submit,submit");
param_info_insert("PLUGINS", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Plugins", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "c++_util,LoadPlugins");
param_info_insert("RELEASE_DIR", "", "/tmp/condorinstall0", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Release Dir", "Where have you installed the bin, sbin and lib condor directories\?",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  ",");
param_info_insert("SCHEDD_PREEMPTION_REQUIREMENTS", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Schedd Preemption Requirements", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "schedd,dedicated_scheduler");
param_info_insert("MASTER_ENVIRONMENT", "", "", "0.0.0", "",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Master Environment", "",
				  "",
				  "");
param_info_insert("SSH_KEYGEN", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Ssh Keygen", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "c++_util,sshd_wrapper");
param_info_insert("DELEGATE_JOB_GSI_CREDENTIALS", "", "true", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Delegate Job Gsi Credentials", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "shadow,remoteresource");
param_info_insert("VM_GAHP_LOG", "", "/tmp/VMGAHPLog.$(USERNAME)", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Vm Gahp Log", "VM_GAHP_LOG\t= $(LOG)/VMGahpLogs/VMGahpLog.$(USERNAME)",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "vm-gahp,vmgahp_main");
param_info_insert("STARTER_INITIAL_UPDATE_INTERVAL", "", "8", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Starter Initial Update Interval", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "starter,job_info_communicator");
param_info_insert("WANT_XML_LOG", "", "false", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Want Xml Log", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "tt,ttmanager");
param_info_insert("SINFUL", "", "input", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Sinful", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "unit_tests,FTEST_is_valid_sinful");
param_info_insert("xen_root", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Xen Root", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "submit,submit");
param_info_insert("xen_disk", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Xen Disk", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "submit,submit");
param_info_insert("LOCAL_UNIV_EXECUTE", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Local Univ Execute", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "schedd,schedd");
param_info_insert("UNUSED_CLAIM_TIMEOUT", "", "600", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Unused Claim Timeout", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "schedd,dedicated_scheduler");
param_info_insert("SCHEDD_INTERVAL", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Schedd Interval", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "submit,submit");
param_info_insert("QUILL_MAINTAIN_DB_CONN", "", "true", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Quill Maintain Db Conn", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "tt,ttmanager");
param_info_insert("CONDOR_ADMIN", "", "michaelb@cs.wisc.edu", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Condor Admin", "the email\?",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "collector,collector");
param_info_insert("QUEUE_SUPER_USERS", "", "root, condor", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Queue Super Users", "By default, this only includes root.",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "schedd,qmgmt");
param_info_insert("DAGMAN_AUTO_RESCUE", "", "true", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Dagman Auto Rescue", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "dagman,condor_submit_dag");
param_info_insert("CKPT_SERVER_INTERVAL", "", "", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Ckpt Server Interval", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "ckpt_server,server2");
param_info_insert("LOG", "", "$(LOCAL_DIR)/log", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Log", "Pathnames",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "stork,dap_server");
param_info_insert("MASTER_SQLLOG", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Master SQL Log", "",
				  "",
				  "");
param_info_insert("SETTABLE_ATTRS_ADMINISTRATOR", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Settable Attribute Administrator", "",
				  "",
				  "");
param_info_insert("UPDATE_COLLECTOR_WITH_TCP", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Update Collector With Tcp", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "daemon_client,dc_collector");
param_info_insert("Q_QUERY_TIMEOUT", "", "20", "0.0.0", "",
                  STATE_DEFAULT, TYPE_INT, 0, 1, CUSTOMIZATION_SELDOM,
				  "Q Query Timeout", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "c++_util,condor_q");
param_info_insert("VM_MEMORY", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Vm Memory", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "startd,vmuniverse_mgr");
param_info_insert("DAGMAN_ON_EXIT_REMOVE", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Dagman On Exit Remove", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "dagman,condor_submit_dag");
param_info_insert("SOAP_LEAVE_IN_QUEUE", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Soap Leave In Queue", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "schedd,soap_scheddStub");
param_info_insert("EMAIL_NOTIFICATION_CC", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Email Notification Cc", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "shadow,shadow_common");
param_info_insert("QUILL_DB_NAME", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Quill Db Name", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "q,queue");
param_info_insert("VMP_VM_LIST", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Vmp Vm List", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "startd,startd_main");
param_info_insert("MASTER_WAITS_FOR_GCB_BROKER", "", "true", "0.0.0", "",
                  STATE_DEFAULT, TYPE_BOOL, 0, 1, CUSTOMIZATION_SELDOM,
				  "Master Waits For Gcb Broker", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "master,master");
param_info_insert("VALID_COD_USERS", "", "", "0.0.0", ".*",
                  STATE_DEFAULT, TYPE_STRING, 0, 1, CUSTOMIZATION_SELDOM,
				  "Valid Cod Users", "",
				  "http://cs.wisc.edu/condor/manual/v7.1/3_3Configuration.html#SECTION",
				  "startd,startd_main");
