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



// List of attributes used in ClassAds  If you find yourself using anything
// other than ATTR_<blah> to add/lookup expressions/variables *STOP*, add the
// new variable to this file and use the ATTR_<blah> symbolic constant.  --RR

#ifndef __CONDOR_ATTRIBUTES_H__
#define __CONDOR_ATTRIBUTES_H__

extern const char * const  ATTR_ACCOUNTING_GROUP;
extern const char * const  ATTR_ACTION_CONSTRAINT;
extern const char * const  ATTR_ACTION_IDS;
extern const char * const  ATTR_ACTION_RESULT;
extern const char * const  ATTR_ACTION_RESULT_TYPE;
extern const char * const  ATTR_ACTIVITY;
extern const char * const  ATTR_ALLOW_NOTIFICATION_CC;
extern const char * const  ATTR_ALL_REMOTE_HOSTS;
extern const char * const  ATTR_APPEND_FILES;
extern const char * const  ATTR_ARCH;
extern const char * const  ATTR_AVAIL_BANDWIDTH_TO_SUBMIT_MACHINE;
extern const char * const  ATTR_AVAIL_BANDWIDTH_TO_LAST_CKPT_SERVER;
extern const char * const  ATTR_AVAIL_BANDWIDTH_TO_CKPT_SERVER;
extern const char * const  ATTR_AVAIL_SINCE;
extern const char * const  ATTR_AVAIL_TIME;
extern const char * const  ATTR_AVAIL_TIME_ESTIMATE;
extern const char * const  ATTR_BANDWIDTH_TO_SUBMIT_MACHINE;
extern const char * const  ATTR_BANDWIDTH_TO_LAST_CKPT_SERVER;
extern const char * const  ATTR_BANDWIDTH_TO_CKPT_SERVER;
extern const char * const  ATTR_BUFFER_SIZE;
extern const char * const  ATTR_BUFFER_FILES;
extern const char * const  ATTR_BUFFER_BLOCK_SIZE;
extern const char * const  ATTR_BUFFER_BLOCKS_USED;
extern const char * const  ATTR_BUFFER_PREFETCH_SIZE;
extern const char * const  ATTR_BYTES_SENT;
extern const char * const  ATTR_BYTES_RECVD;
extern const char * const  ATTR_CAN_HIBERNATE;
extern const char * const  ATTR_CAPABILITY;
extern const char * const  ATTR_CKPT_SERVER;
extern const char * const  ATTR_COD_CLAIMS;
extern const char * const  ATTR_COMMAND;
extern const char * const  ATTR_COMPRESS_FILES;
extern const char * const  ATTR_REQUESTED_CAPACITY;
extern const char * const  ATTR_CKPT_ARCH;
extern const char * const  ATTR_CKPT_LAST_READ;
extern const char * const  ATTR_CKPT_OPSYS;
extern const char * const  ATTR_CLAIM_ID;
extern const char * const  ATTR_CLAIM_IDS;
extern const char * const  ATTR_PUBLIC_CLAIM_ID;
extern const char * const  ATTR_PUBLIC_CLAIM_IDS;
extern const char * const  ATTR_CLAIM_STATE;
extern const char * const  ATTR_CLAIM_TYPE;
extern const char * const  ATTR_CLIENT_MACHINE;
extern const char * const  ATTR_CLOCK_DAY;
extern const char * const  ATTR_CLOCK_MIN;
extern const char * const  ATTR_CLUSTER_ID;
extern const char * const  ATTR_AUTO_CLUSTER_ID;
extern const char * const  ATTR_AUTO_CLUSTER_ATTRS;
extern const char * const  ATTR_COMPLETION_DATE;
extern const char * const  ATTR_MATCHED_CONCURRENCY_LIMITS;
extern const char * const  ATTR_CONCURRENCY_LIMITS;
extern const char * const  ATTR_PREEMPTING_CONCURRENCY_LIMITS;
#define ATTR_CONDOR_LOAD_AVG			AttrGetName( ATTRE_CONDOR_LOAD_AVG )
#define ATTR_CONDOR_ADMIN				AttrGetName( ATTRE_CONDOR_ADMIN )
extern const char * const  ATTR_CONSOLE_IDLE;
extern const char * const  ATTR_CONTINUE;
extern const char * const  ATTR_CORE_SIZE;
extern const char * const  ATTR_CRON_MINUTES;
extern const char * const  ATTR_CRON_HOURS;
extern const char * const  ATTR_CRON_DAYS_OF_MONTH;
extern const char * const  ATTR_CRON_MONTHS;
extern const char * const  ATTR_CRON_DAYS_OF_WEEK;
extern const char * const  ATTR_CRON_NEXT_RUNTIME;
extern const char * const  ATTR_CRON_CURRENT_TIME_RANGE;
extern const char * const  ATTR_CRON_PREP_TIME;
extern const char * const  ATTR_CRON_WINDOW;
extern const char * const  ATTR_CPU_BUSY;
extern const char * const  ATTR_CPU_BUSY_TIME;
extern const char * const  ATTR_CPU_IS_BUSY;
extern const char * const  ATTR_CPUS;
extern const char * const  ATTR_CURRENT_HOSTS;
extern const char * const  ATTR_CURRENT_JOBS_RUNNING;
extern const char * const  ATTR_CURRENT_RANK;
extern const char * const  ATTR_CURRENT_STATUS_UNKNOWN;
extern const char * const  ATTR_CURRENT_TIME;
extern const char * const  ATTR_DAEMON_START_TIME;
extern const char * const  ATTR_DAEMON_SHUTDOWN;
extern const char * const  ATTR_DAEMON_SHUTDOWN_FAST;
extern const char * const  ATTR_DAG_NODE_NAME;
extern const char * const  ATTR_DAG_NODE_NAME_ALT;
extern const char * const  ATTR_DAGMAN_JOB_ID;
extern const char * const  ATTR_DEFERRAL_OFFSET;
extern const char * const  ATTR_DEFERRAL_PREP_TIME;
extern const char * const  ATTR_DEFERRAL_TIME;
extern const char * const  ATTR_DEFERRAL_WINDOW;
extern const char * const  ATTR_DESTINATION;
extern const char * const  ATTR_DISK;
extern const char * const  ATTR_DISK_USAGE;
extern const char * const  ATTR_EMAIL_ATTRIBUTES;
extern const char * const  ATTR_ENTERED_CURRENT_ACTIVITY;
extern const char * const  ATTR_ENTERED_CURRENT_STATE;
extern const char * const  ATTR_ENTERED_CURRENT_STATUS;
extern const char * const  ATTR_ERROR_STRING;
extern const char * const  ATTR_EXCEPTION_HIERARCHY;
extern const char * const  ATTR_EXCEPTION_NAME;
extern const char * const  ATTR_EXCEPTION_TYPE;
extern const char * const  ATTR_EXECUTABLE_SIZE;
extern const char * const  ATTR_EXIT_REASON;
extern const char * const  ATTR_FETCH_FILES;
extern const char * const  ATTR_FETCH_WORK_DELAY;
extern const char * const  ATTR_FILE_NAME;
extern const char * const  ATTR_FILE_SIZE;
extern const char * const  ATTR_FILE_SYSTEM_DOMAIN;
extern const char * const  ATTR_FILE_REMAPS;
extern const char * const  ATTR_FILE_READ_COUNT;
extern const char * const  ATTR_FILE_READ_BYTES;
extern const char * const  ATTR_FILE_WRITE_COUNT;
extern const char * const  ATTR_FILE_WRITE_BYTES;
extern const char * const  ATTR_FILE_SEEK_COUNT;
extern const char * const  ATTR_FLOCKED_JOBS;
extern const char * const  ATTR_FLAVOR;
extern const char * const  ATTR_FORCE;
extern const char * const  ATTR_GID;
extern const char * const  ATTR_GLOBAL_JOB_ID;
extern const char * const  ATTR_GZIP;
extern const char * const  ATTR_GLOBUS_CONTACT_STRING;
extern const char * const  ATTR_GLOBUS_DELEGATION_URI;
// Deprecated (cruft) -- no longer used
extern const char * const  ATTR_GLOBUS_GRAM_VERSION;
extern const char * const  ATTR_GLOBUS_RESOURCE;
extern const char * const  ATTR_GLOBUS_RESOURCE_UNAVAILABLE_TIME;
extern const char * const  ATTR_JOB_MUST_EXPAND;
extern const char * const  ATTR_GLOBUS_RSL;
extern const char * const  ATTR_GLOBUS_STATUS;
extern const char * const  ATTR_GLOBUS_XML;
extern const char * const  ATTR_X509_USER_PROXY;
extern const char * const  ATTR_X509_USER_PROXY_SUBJECT;
extern const char * const  ATTR_DELEGATED_PROXY_EXPIRATION;
extern const char * const  ATTR_GLOBUS_JOBMANAGER_TYPE;
extern const char * const  ATTR_GLOBUS_SUBMIT_ID;
extern const char * const  ATTR_GRIDFTP_SERVER_JOB;
extern const char * const  ATTR_GRIDFTP_URL_BASE;
extern const char * const  ATTR_REQUESTED_GRIDFTP_URL_BASE;
extern const char * const  ATTR_GRID_RESOURCE;
extern const char * const  ATTR_GRID_RESOURCE_UNAVAILABLE_TIME;
extern const char * const  ATTR_GRID_JOB_ID;
extern const char * const  ATTR_GRID_JOB_STATUS;
// ckireyev myproxy
extern const char * const  ATTR_MYPROXY_SERVER_DN;
extern const char * const  ATTR_MYPROXY_HOST_NAME;
extern const char * const  ATTR_MYPROXY_PASSWORD;
extern const char * const  ATTR_MYPROXY_CRED_NAME;
extern const char * const  ATTR_MYPROXY_REFRESH_THRESHOLD;
extern const char * const  ATTR_MYPROXY_NEW_PROXY_LIFETIME;
// END ckireyev myproxy
extern const char * const  ATTR_HARDWARE_ADDRESS;
extern const char * const  ATTR_HAS_CHECKPOINTING;
extern const char * const  ATTR_HAS_FILE_TRANSFER;
extern const char * const  ATTR_HAS_PER_FILE_ENCRYPTION;
extern const char * const  ATTR_HAS_IO_PROXY;
extern const char * const  ATTR_HAS_JAVA;
extern const char * const  ATTR_HAS_JIC_LOCAL_CONFIG;
extern const char * const  ATTR_HAS_JIC_LOCAL_STDIN;
extern const char * const  ATTR_HAS_JOB_AD;
extern const char * const  ATTR_HAS_JOB_AD_FROM_FILE;
extern const char * const  ATTR_HAS_JOB_DEFERRAL;
extern const char * const  ATTR_HAS_MPI;
extern const char * const  ATTR_HAS_OLD_VANILLA;
extern const char * const  ATTR_HAS_PVM;
extern const char * const  ATTR_HAS_RECONNECT;
extern const char * const  ATTR_HAS_REMOTE_SYSCALLS;
extern const char * const  ATTR_HAS_TDP;
extern const char * const  ATTR_HAS_SOAP_API;
extern const char * const  ATTR_HAS_WIN_RUN_AS_OWNER;
extern const char * const  ATTR_HAS_VM;
extern const char * const  ATTR_HELD_JOBS;
extern const char * const  ATTR_HIBERNATION_LEVEL;
extern const char * const  ATTR_HIBERNATION_STATE;
extern const char * const  ATTR_HIBERNATION_SUPPORTED_STATES;
extern const char * const  ATTR_HIBERNATION_RAW_MASK;
extern const char * const  ATTR_HIBERNATION_METHOD;
extern const char * const  ATTR_UNHIBERNATE;
extern const char * const  ATTR_HOLD_KILL_SIG;
extern const char * const  ATTR_HOOK_KEYWORD;
extern const char * const  ATTR_IDLE_JOBS;
extern const char * const  ATTR_IMAGE_SIZE;
extern const char * const  ATTR_INTERACTIVE;
extern const char * const  ATTR_IS_DAEMON_CORE;
extern const char * const  ATTR_IS_OWNER;
extern const char * const  ATTR_IS_QUEUE_SUPER_USER;
extern const char * const  ATTR_IS_WAKE_SUPPORTED;
extern const char * const  ATTR_WAKE_SUPPORTED_FLAGS;
extern const char * const  ATTR_IS_WAKE_ENABLED;
extern const char * const  ATTR_WAKE_ENABLED_FLAGS;
extern const char * const  ATTR_IS_WAKEABLE;
extern const char * const  ATTR_INACTIVE;
extern const char * const  ATTR_JAR_FILES;
extern const char * const  ATTR_JAVA_MFLOPS;
extern const char * const  ATTR_JAVA_VENDOR;
extern const char * const  ATTR_JAVA_VERSION;
extern const char * const  ATTR_JOB_ACTION;
extern const char * const  ATTR_JOB_ARGUMENTS1;
extern const char * const  ATTR_JOB_ARGUMENTS2;
extern const char * const  ATTR_JOB_CMD;
extern const char * const  ATTR_JOB_CMD_HASH;
extern const char * const  ATTR_JOB_CMD_MD5;
extern const char * const  ATTR_ORIG_JOB_CMD;
extern const char * const  ATTR_JOB_CORE_DUMPED;
extern const char * const  ATTR_JOB_CORE_FILENAME;
extern const char * const  ATTR_JOB_CURRENT_START_DATE;
extern const char * const  ATTR_JOB_DURATION;
extern const char * const  ATTR_JOB_ENVIRONMENT1;
extern const char * const  ATTR_JOB_ENVIRONMENT1_DELIM;
extern const char * const  ATTR_JOB_ENVIRONMENT2;
extern const char * const  ATTR_JOB_ERROR;
extern const char * const  ATTR_JOB_ERROR_ORIG;
extern const char * const  ATTR_JOB_ERROR_SIZE;
extern const char * const  ATTR_JOB_KEYWORD;
extern const char * const  ATTR_JOB_LEASE_DURATION;
extern const char * const  ATTR_JOB_LEASE_EXPIRATION;
extern const char * const  ATTR_JOB_SPOOL_EXECUTABLE;
extern const char * const  ATTR_JOB_EXIT_STATUS;
extern const char * const  ATTR_JOB_EXIT_REQUIREMENTS;
extern const char * const  ATTR_JOB_ID;
extern const char * const  ATTR_JOB_FINISHED_HOOK_DONE;
extern const char * const  ATTR_JOB_INPUT;
extern const char * const  ATTR_JOB_IWD;
extern const char * const  ATTR_JOB_JAVA_VM_ARGS1;
extern const char * const  ATTR_JOB_JAVA_VM_ARGS2;
extern const char * const  ATTR_ORIG_JOB_IWD;
extern const char * const  ATTR_JOB_REMOTE_IWD;
extern const char * const  ATTR_JOB_LOAD_PROFILE;
extern const char * const  ATTR_JOB_RUNAS_OWNER;
extern const char * const  ATTR_JOB_LOAD_USER_PROFILE;
extern const char * const  ATTR_JOB_LOCAL_CPU;
extern const char * const  ATTR_JOB_LOCAL_SYS_CPU;
extern const char * const  ATTR_JOB_LOCAL_USER_CPU;
extern const char * const  ATTR_JOB_MANAGED;
extern const char * const  ATTR_JOB_MANAGED_MANAGER;
extern const char * const  ATTR_JOB_MATCHED;
extern const char * const  ATTR_JOB_NONESSENTIAL;
extern const char * const  ATTR_JOB_NOOP;
extern const char * const  ATTR_JOB_NOOP_EXIT_SIGNAL;
extern const char * const  ATTR_JOB_NOOP_EXIT_CODE;
extern const char * const  ATTR_JOB_NOTIFICATION;
extern const char * const  ATTR_JOB_OUTPUT;
extern const char * const  ATTR_JOB_OUTPUT_ORIG;
extern const char * const  ATTR_JOB_OUTPUT_SIZE;
extern const char * const  ATTR_JOB_PID;
extern const char * const  ATTR_JOB_PRIO;
extern const char * const  ATTR_JOB_COMMITTED_TIME;
extern const char * const  ATTR_JOB_LANGUAGE;
extern const char * const  ATTR_JOB_LAST_START_DATE;
extern const char * const  ATTR_JOB_LEAVE_IN_QUEUE;
extern const char * const  ATTR_JOB_REMOTE_SYS_CPU;
extern const char * const  ATTR_JOB_REMOTE_USER_CPU;
extern const char * const  ATTR_JOB_REMOTE_WALL_CLOCK;
extern const char * const  ATTR_JOB_ROOT_DIR;
extern const char * const  ATTR_JOB_RUN_COUNT;
extern const char * const  ATTR_JOB_SANDBOX_JOBAD;
extern const char * const  ATTR_JOB_SUBMISSION;
extern const char * const  ATTR_JOB_SUBMISSION_ID;
extern const char * const  ATTR_JOB_START;
extern const char * const  ATTR_JOB_START_DATE;
extern const char * const  ATTR_JOB_STATE;
extern const char * const  ATTR_JOB_STATUS;
extern const char * const  ATTR_LAST_JOB_STATUS;
extern const char * const  ATTR_JOB_STATUS_ON_RELEASE;
extern const char * const  ATTR_JOB_UNIVERSE;
extern const char * const  ATTR_JOB_GRID_TYPE;
extern const char * const  ATTR_JOB_WALL_CLOCK_CKPT;
extern const char * const  ATTR_JOB_QUEUE_BIRTHDATE;
extern const char * const  ATTR_JOB_VM_TYPE;
extern const char * const  ATTR_JOB_VM_MEMORY;
extern const char * const  ATTR_JOB_VM_VCPUS;
extern const char * const  ATTR_JOB_VM_MACADDR;
extern const char * const  ATTR_JOB_VM_CHECKPOINT;
extern const char * const  ATTR_JOB_VM_NETWORKING;
extern const char * const  ATTR_JOB_VM_NETWORKING_TYPE;
extern const char * const  ATTR_JOB_VM_HARDWARE_VT;
extern const char * const  ATTR_KEYBOARD_IDLE;
extern const char * const  ATTR_KEYSTORE_FILE;
extern const char * const  ATTR_KEYSTORE_ALIAS;
extern const char * const  ATTR_KEYSTORE_PASSPHRASE_FILE;
extern const char * const  ATTR_KFLOPS;
extern const char * const  ATTR_KILL;
extern const char * const  ATTR_KILL_SIG;
extern const char * const  ATTR_LAST_AVAIL_INTERVAL;
extern const char * const  ATTR_LAST_BENCHMARK;
extern const char * const  ATTR_LAST_CKPT_SERVER;
extern const char * const  ATTR_LAST_CKPT_TIME;
extern const char * const  ATTR_LAST_PUBLIC_CLAIM_ID;
extern const char * const  ATTR_LAST_PUBLIC_CLAIM_IDS;
extern const char * const  ATTR_LAST_CLAIM_STATE;
extern const char * const  ATTR_LAST_FETCH_WORK_SPAWNED;
extern const char * const  ATTR_LAST_FETCH_WORK_COMPLETED;
extern const char * const  ATTR_LAST_VACATE_TIME;
extern const char * const  ATTR_LAST_HEARD_FROM;
extern const char * const  ATTR_LAST_HOLD_REASON;
extern const char * const  ATTR_LAST_HOLD_REASON_CODE;
extern const char * const  ATTR_LAST_HOLD_REASON_SUBCODE;
extern const char * const  ATTR_LAST_JOB_LEASE_RENEWAL;
extern const char * const  ATTR_LAST_JOB_LEASE_RENEWAL_FAILED;
extern const char * const  ATTR_LAST_MATCH_TIME;
extern const char * const  ATTR_MACHINE_LAST_MATCH_TIME;
extern const char * const  ATTR_LAST_MATCH_LIST_PREFIX;
extern const char * const  ATTR_LAST_MATCH_LIST_LENGTH;
extern const char * const  ATTR_LAST_REJ_MATCH_TIME;
extern const char * const  ATTR_LAST_REJ_MATCH_REASON;
extern const char * const  ATTR_LAST_PERIODIC_CHECKPOINT;
extern const char * const  ATTR_LAST_RELEASE_REASON;
extern const char * const  ATTR_LAST_REMOTE_HOST;
extern const char * const  ATTR_LAST_REMOTE_STATUS_UPDATE;
extern const char * const  ATTR_LAST_UPDATE;
extern const char * const  ATTR_LOCAL_CREDD;
extern const char * const  ATTR_LOCAL_FILES;
extern const char * const  ATTR_LOAD_AVG;
extern const char * const  ATTR_MACHINE;
extern const char * const  ATTR_MACHINE_COUNT;
extern const char * const  ATTR_MASTER_IP_ADDR;
extern const char * const  ATTR_MAX_HOSTS;
extern const char * const  ATTR_MAX_JOB_RETIREMENT_TIME;
extern const char * const  ATTR_MAX_JOBS_RUNNING;
extern const char * const  ATTR_MEMORY;
extern const char * const  ATTR_MIN_HOSTS;
extern const char * const  ATTR_MIPS;
extern const char * const  ATTR_MPI_IS_MASTER;
extern const char * const  ATTR_MPI_MASTER_ADDR;
extern const char * const  ATTR_PARALLEL_IS_MASTER;
extern const char * const  ATTR_PARALLEL_MASTER_ADDR;
extern const char * const  ATTR_MY_CURRENT_TIME;
extern const char * const  ATTR_MY_TYPE;
extern const char * const  ATTR_NAME;
extern const char * const  ATTR_NICE_USER;
extern const char * const  ATTR_NEGOTIATOR_REQUIREMENTS;
extern const char * const  ATTR_NEXT_CLUSTER_NUM;
extern const char * const  ATTR_NEXT_FETCH_WORK_DELAY;
extern const char * const  ATTR_NEXT_JOB_START_DELAY;
extern const char * const  ATTR_NODE;
extern const char * const  ATTR_NORDUGRID_RSL;
extern const char * const  ATTR_NOTIFY_USER;
extern const char * const  ATTR_NOTIFY_JOB_SCHEDULER;
extern const char * const  ATTR_NT_DOMAIN;
extern const char * const  ATTR_WINDOWS_VERSION;
extern const char * const  ATTR_WINDOWS_MAJOR_VERSION;
extern const char * const  ATTR_WINDOWS_MINOR_VERSION;
extern const char * const  ATTR_WINDOWS_BUILD_NUMBER;
extern const char * const  ATTR_WINDOWS_SERVICE_PACK_MAJOR;
extern const char * const  ATTR_WINDOWS_SERVICE_PACK_MINOR;
extern const char * const  ATTR_WINDOWS_PRODUCT_TYPE;
extern const char * const  ATTR_NUM_COD_CLAIMS;
extern const char * const  ATTR_NUM_CKPTS;
extern const char * const  ATTR_NUM_CKPTS_RAW;
extern const char * const  ATTR_NUM_GLOBUS_SUBMITS;
extern const char * const  ATTR_NUM_MATCHES;
extern const char * const  ATTR_NUM_HOPS_TO_SUBMIT_MACHINE;
extern const char * const  ATTR_NUM_HOPS_TO_LAST_CKPT_SERVER;
extern const char * const  ATTR_NUM_HOPS_TO_CKPT_SERVER;
extern const char * const  ATTR_NUM_JOB_STARTS;
extern const char * const  ATTR_NUM_JOB_RECONNECTS;
extern const char * const  ATTR_NUM_PIDS;
extern const char * const  ATTR_NUM_RESTARTS;
extern const char * const  ATTR_NUM_SHADOW_EXCEPTIONS;
extern const char * const  ATTR_NUM_SHADOW_STARTS;
extern const char * const  ATTR_NUM_SYSTEM_HOLDS;
extern const char * const  ATTR_NUM_USERS;
extern const char * const  ATTR_OFFLINE;
extern const char * const  ATTR_OPSYS;
extern const char * const  ATTR_ORIG_MAX_HOSTS;
extern const char * const  ATTR_OWNER;
extern const char * const  ATTR_PARALLEL_SCHEDULING_GROUP;
extern const char * const  ATTR_PARALLEL_SCRIPT_SHADOW;
extern const char * const  ATTR_PARALLEL_SCRIPT_STARTER;
extern const char * const  ATTR_PARALLEL_SHUTDOWN_POLICY;
extern const char * const  ATTR_PERIODIC_CHECKPOINT;
#define ATTR_PLATFORM					AttrGetName( ATTRE_PLATFORM )
extern const char * const  ATTR_PREEMPTING_ACCOUNTING_GROUP;
extern const char * const  ATTR_PREEMPTING_RANK;
extern const char * const  ATTR_PREEMPTING_OWNER;
extern const char * const  ATTR_PREEMPTING_USER;
extern const char * const  ATTR_PREFERENCES;
extern const char * const  ATTR_PREV_SEND_ESTIMATE;
extern const char * const  ATTR_PREV_RECV_ESTIMATE;
extern const char * const  ATTR_PRIO;
extern const char * const  ATTR_PROC_ID;
extern const char * const  ATTR_SUB_PROC_ID;
extern const char * const  ATTR_PRIVATE_NETWORK_IP_ADDR;
extern const char * const  ATTR_PRIVATE_NETWORK_NAME;
extern const char * const  ATTR_PUBLIC_NETWORK_IP_ADDR;
extern const char * const  ATTR_Q_DATE;
extern const char * const  ATTR_RANK;
extern const char * const  ATTR_REAL_UID;
extern const char * const  ATTR_RELEASE_REASON;
extern const char * const  ATTR_REMOTE_GROUP_RESOURCES_IN_USE;
extern const char * const  ATTR_REMOTE_GROUP_QUOTA;
extern const char * const  ATTR_REMOTE_HOST;
extern const char * const  ATTR_REMOTE_HOSTS;
extern const char * const  ATTR_REMOTE_JOB_ID;
extern const char * const  ATTR_REMOTE_OWNER;
extern const char * const  ATTR_REMOTE_POOL;
extern const char * const  ATTR_REMOTE_SCHEDD;
extern const char * const  ATTR_REMOTE_SLOT_ID;
extern const char * const  ATTR_REMOTE_SPOOL_DIR;
extern const char * const  ATTR_REMOTE_USER;
extern const char * const  ATTR_REMOTE_USER_PRIO;
extern const char * const  ATTR_REMOTE_USER_RESOURCES_IN_USE;
// Deprecated (cruft) -- use: ATTR_REMOTE_SLOT_ID 
extern const char * const  ATTR_REMOTE_VIRTUAL_MACHINE_ID;
extern const char * const  ATTR_REMOVE_KILL_SIG;
extern const char * const  ATTR_REMOVE_REASON;
extern const char * const  ATTR_REQUEUE_REASON;
extern const char * const  ATTR_REQUIREMENTS;
extern const char * const  ATTR_SLOT_WEIGHT;
extern const char * const  ATTR_RESULT;
extern const char * const  ATTR_RSC_BYTES_SENT;
extern const char * const  ATTR_RSC_BYTES_RECVD;
extern const char * const  ATTR_RUNNING_JOBS;
extern const char * const  ATTR_RUNNING_COD_JOB;
extern const char * const  ATTR_RUN_BENCHMARKS;
extern const char * const  ATTR_SHADOW_IP_ADDR;
extern const char * const  ATTR_MY_ADDRESS;
extern const char * const  ATTR_SCHEDD_SWAP_EXHAUSTED;
extern const char * const  ATTR_SCHEDD_INTERVAL;
extern const char * const  ATTR_SCHEDD_IP_ADDR;
extern const char * const  ATTR_SCHEDD_NAME;
extern const char * const  ATTR_SCHEDULER;
extern const char * const  ATTR_SHADOW_WAIT_FOR_DEBUG;
extern const char * const  ATTR_SLOT_ID;
extern const char * const  ATTR_SLOT_PARTITIONABLE;
extern const char * const  ATTR_SLOT_DYNAMIC;
extern const char * const  ATTR_SOURCE;
extern const char * const  ATTR_STAGE_IN_START;
extern const char * const  ATTR_STAGE_IN_FINISH;
extern const char * const  ATTR_STAGE_OUT_START;
extern const char * const  ATTR_STAGE_OUT_FINISH;
extern const char * const  ATTR_START;
extern const char * const  ATTR_START_LOCAL_UNIVERSE;
extern const char * const  ATTR_START_SCHEDULER_UNIVERSE;
extern const char * const  ATTR_STARTD_IP_ADDR;
extern const char * const  ATTR_STARTD_PRINCIPAL;
extern const char * const  ATTR_STATE;
extern const char * const  ATTR_STARTER_IP_ADDR;
extern const char * const  ATTR_STARTER_ABILITY_LIST;
extern const char * const  ATTR_STARTER_IGNORED_ATTRS;
extern const char * const  ATTR_STARTER_ULOG_FILE;
extern const char * const  ATTR_STARTER_ULOG_USE_XML;
extern const char * const  ATTR_STARTER_WAIT_FOR_DEBUG;
extern const char * const  ATTR_STATUS;
extern const char * const  ATTR_STREAM_INPUT;
extern const char * const  ATTR_STREAM_OUTPUT;
extern const char * const  ATTR_STREAM_ERROR;
extern const char * const  ATTR_SUBMITTER_ID;
extern const char * const  ATTR_SUBMITTOR_PRIO;  // old-style for ATTR_SUBMITTER_USER_PRIO
extern const char * const  ATTR_SUBMITTER_USER_PRIO;
extern const char * const  ATTR_SUBMITTER_USER_RESOURCES_IN_USE;
extern const char * const  ATTR_SUBMITTER_GROUP_RESOURCES_IN_USE;
extern const char * const  ATTR_SUBMITTER_GROUP_QUOTA;
extern const char * const  ATTR_SUBNET;
extern const char * const  ATTR_SUBNET_MASK;
extern const char * const  ATTR_SUSPEND;
extern const char * const  ATTR_SUSPEND_JOB_AT_EXEC;
extern const char * const  ATTR_TARGET_TYPE;
extern const char * const  ATTR_TIME_TO_LIVE;
extern const char * const  ATTR_TOOL_DAEMON_ARGS1;
extern const char * const  ATTR_TOOL_DAEMON_ARGS2;
extern const char * const  ATTR_TOOL_DAEMON_CMD;
extern const char * const  ATTR_TOOL_DAEMON_ERROR;
extern const char * const  ATTR_TOOL_DAEMON_INPUT;
extern const char * const  ATTR_TOOL_DAEMON_OUTPUT;
extern const char * const  ATTR_TOTAL_CLAIM_RUN_TIME;
extern const char * const  ATTR_TOTAL_CLAIM_SUSPEND_TIME;
#define ATTR_TOTAL_CONDOR_LOAD_AVG			AttrGetName( ATTRE_TOTAL_LOAD )
extern const char * const  ATTR_TOTAL_CPUS;
extern const char * const  ATTR_TOTAL_DISK;
extern const char * const  ATTR_TOTAL_FLOCKED_JOBS;
extern const char * const  ATTR_TOTAL_REMOVED_JOBS;
extern const char * const  ATTR_TOTAL_HELD_JOBS;
extern const char * const  ATTR_TOTAL_IDLE_JOBS;
extern const char * const  ATTR_TOTAL_JOB_ADS;
extern const char * const  ATTR_TOTAL_JOB_RUN_TIME;
extern const char * const  ATTR_TOTAL_JOB_SUSPEND_TIME;
extern const char * const  ATTR_TOTAL_LOAD_AVG;
extern const char * const  ATTR_TOTAL_MEMORY;
extern const char * const  ATTR_TOTAL_RUNNING_JOBS;
extern const char * const  ATTR_TOTAL_LOCAL_RUNNING_JOBS;
extern const char * const  ATTR_TOTAL_LOCAL_IDLE_JOBS;
extern const char * const  ATTR_TOTAL_SCHEDULER_RUNNING_JOBS;
extern const char * const  ATTR_TOTAL_SCHEDULER_IDLE_JOBS;
extern const char * const  ATTR_TOTAL_SLOTS;
extern const char * const  ATTR_TOTAL_TIME_IN_CYCLE;
extern const char * const  ATTR_TOTAL_TIME_BACKFILL_BUSY;
extern const char * const  ATTR_TOTAL_TIME_BACKFILL_IDLE;
extern const char * const  ATTR_TOTAL_TIME_BACKFILL_KILLING;
extern const char * const  ATTR_TOTAL_TIME_CLAIMED_BUSY;
extern const char * const  ATTR_TOTAL_TIME_CLAIMED_IDLE;
extern const char * const  ATTR_TOTAL_TIME_CLAIMED_RETIRING;
extern const char * const  ATTR_TOTAL_TIME_CLAIMED_SUSPENDED;
extern const char * const  ATTR_TOTAL_TIME_MATCHED_IDLE;
extern const char * const  ATTR_TOTAL_TIME_OWNER_IDLE;
extern const char * const  ATTR_TOTAL_TIME_PREEMPTING_KILLING;
extern const char * const  ATTR_TOTAL_TIME_PREEMPTING_VACATING;
extern const char * const  ATTR_TOTAL_TIME_UNCLAIMED_BENCHMARKING;
extern const char * const  ATTR_TOTAL_TIME_UNCLAIMED_IDLE;
// Deprecated (cruft) -- use: ATTR_TOTAL_SLOTS;
extern const char * const  ATTR_TOTAL_VIRTUAL_MACHINES;
extern const char * const  ATTR_TOTAL_VIRTUAL_MEMORY;
extern const char * const  ATTR_UID;
extern const char * const  ATTR_UID_DOMAIN;
extern const char * const  ATTR_ULOG_FILE;
extern const char * const  ATTR_ULOG_USE_XML;
extern const char * const  ATTR_UPDATE_INTERVAL;
extern const char * const  ATTR_CLASSAD_LIFETIME;
extern const char * const  ATTR_UPDATE_PRIO;
extern const char * const  ATTR_UPDATE_SEQUENCE_NUMBER;
extern const char * const  ATTR_USE_GRID_SHELL;
extern const char * const  ATTR_USER;
extern const char * const  ATTR_VACATE;
extern const char * const  ATTR_VACATE_TYPE;
extern const char * const  ATTR_VIRTUAL_MEMORY;
extern const char * const  ATTR_VISA_TIMESTAMP;
extern const char * const  ATTR_VISA_DAEMON_TYPE;
extern const char * const  ATTR_VISA_DAEMON_PID;
extern const char * const  ATTR_VISA_HOSTNAME;
extern const char * const  ATTR_VISA_IP;
extern const char * const  ATTR_WOL_PORT;
extern const char * const  ATTR_WANT_CHECKPOINT;
extern const char * const  ATTR_WANT_CLAIMING;
extern const char * const  ATTR_WANT_IO_PROXY;
extern const char * const  ATTR_WANT_MATCH_DIAGNOSTICS;
extern const char * const  ATTR_WANT_PARALLEL_SCHEDULING_GROUPS;
extern const char * const  ATTR_WANT_REMOTE_SYSCALLS;
extern const char * const  ATTR_WANT_REMOTE_IO;
extern const char * const  ATTR_WANT_SCHEDD_COMPLETION_VISA;
extern const char * const  ATTR_WANT_STARTER_EXECUTION_VISA;
extern const char * const  ATTR_WANT_SUBMIT_NET_STATS;
extern const char * const  ATTR_WANT_LAST_CKPT_SERVER_NET_STATS;
extern const char * const  ATTR_WANT_CKPT_SERVER_NET_STATS;
extern const char * const  ATTR_WANT_AD_REVAULATE;
extern const char * const  ATTR_COLLECTOR_IP_ADDR;
extern const char * const  ATTR_NEGOTIATOR_IP_ADDR;
extern const char * const  ATTR_CREDD_IP_ADDR;
extern const char * const  ATTR_NUM_HOSTS_TOTAL;
extern const char * const  ATTR_NUM_HOSTS_CLAIMED;
extern const char * const  ATTR_NUM_HOSTS_UNCLAIMED;
extern const char * const  ATTR_NUM_HOSTS_OWNER;
extern const char * const  ATTR_MAX_RUNNING_JOBS;
#define ATTR_VERSION					AttrGetName( ATTRE_VERSION )
extern const char * const  ATTR_SCHEDD_BIRTHDATE;
extern const char * const  ATTR_SHADOW_VERSION;
// Deprecated (cruft) -- use: ATTR_SLOT_ID
extern const char * const  ATTR_VIRTUAL_MACHINE_ID;
extern const char * const  ATTR_SHOULD_TRANSFER_FILES;
extern const char * const  ATTR_WHEN_TO_TRANSFER_OUTPUT;
extern const char * const  ATTR_TRANSFER_TYPE;
extern const char * const  ATTR_TRANSFER_FILES;
extern const char * const  ATTR_TRANSFER_KEY;
extern const char * const  ATTR_TRANSFER_EXECUTABLE;
extern const char * const  ATTR_TRANSFER_INPUT;
extern const char * const  ATTR_TRANSFER_OUTPUT;
extern const char * const  ATTR_TRANSFER_ERROR;
extern const char * const  ATTR_TRANSFER_INPUT_FILES;
extern const char * const  ATTR_TRANSFER_INTERMEDIATE_FILES;
extern const char * const  ATTR_TRANSFER_OUTPUT_FILES;
extern const char * const  ATTR_TRANSFER_OUTPUT_REMAPS;
extern const char * const  ATTR_ENCRYPT_INPUT_FILES;
extern const char * const  ATTR_ENCRYPT_OUTPUT_FILES;
extern const char * const  ATTR_DONT_ENCRYPT_INPUT_FILES;
extern const char * const  ATTR_DONT_ENCRYPT_OUTPUT_FILES;
extern const char * const  ATTR_TRANSFER_SOCKET;
extern const char * const  ATTR_SERVER_TIME;
extern const char * const  ATTR_SHADOW_BIRTHDATE;
extern const char * const  ATTR_HOLD_REASON;
extern const char * const  ATTR_HOLD_REASON_CODE;
extern const char * const  ATTR_HOLD_REASON_SUBCODE;
extern const char * const  ATTR_HOLD_COPIED_FROM_TARGET_JOB;
extern const char * const  ATTR_WANT_MATCHING;
extern const char * const  ATTR_WANT_RESOURCE_AD;
extern const char * const  ATTR_TOTAL_SUSPENSIONS;
extern const char * const  ATTR_LAST_SUSPENSION_TIME;
extern const char * const  ATTR_CUMULATIVE_SUSPENSION_TIME;

extern const char * const  ATTR_ON_EXIT_BY_SIGNAL;
extern const char * const  ATTR_ON_EXIT_CODE;
extern const char * const  ATTR_ON_EXIT_HOLD_CHECK;
extern const char * const  ATTR_ON_EXIT_REMOVE_CHECK;
extern const char * const  ATTR_ON_EXIT_SIGNAL;
extern const char * const  ATTR_POST_ON_EXIT_BY_SIGNAL;
extern const char * const  ATTR_POST_ON_EXIT_SIGNAL;
extern const char * const  ATTR_POST_ON_EXIT_CODE;
extern const char * const  ATTR_POST_EXIT_REASON;
extern const char * const  ATTR_PERIODIC_HOLD_CHECK;
extern const char * const  ATTR_PERIODIC_RELEASE_CHECK;
extern const char * const  ATTR_PERIODIC_REMOVE_CHECK;
extern const char * const  ATTR_TIMER_REMOVE_CHECK;
extern const char * const  ATTR_TIMER_REMOVE_CHECK_SENT;
extern const char * const  ATTR_GLOBUS_RESUBMIT_CHECK;
extern const char * const  ATTR_REMATCH_CHECK;

extern const char * const  ATTR_SEC_AUTHENTICATION_METHODS_LIST;
extern const char * const  ATTR_SEC_AUTHENTICATION_METHODS;
extern const char * const  ATTR_SEC_CRYPTO_METHODS;
extern const char * const  ATTR_SEC_AUTHENTICATION;
extern const char * const  ATTR_SEC_ENCRYPTION;
extern const char * const  ATTR_SEC_INTEGRITY;
extern const char * const  ATTR_SEC_ENACT;
extern const char * const  ATTR_SEC_RESPOND;
extern const char * const  ATTR_SEC_COMMAND;
extern const char * const  ATTR_SEC_AUTH_COMMAND;
extern const char * const  ATTR_SEC_SID;
extern const char * const  ATTR_SEC_SUBSYSTEM;
extern const char * const  ATTR_SEC_REMOTE_VERSION;
extern const char * const  ATTR_SEC_SERVER_ENDPOINT;
extern const char * const  ATTR_SEC_SERVER_COMMAND_SOCK;
extern const char * const  ATTR_SEC_SERVER_PID;
extern const char * const  ATTR_SEC_PARENT_UNIQUE_ID;
extern const char * const  ATTR_SEC_PACKET_COUNT;
extern const char * const  ATTR_SEC_NEGOTIATION;
extern const char * const  ATTR_SEC_VALID_COMMANDS;
extern const char * const  ATTR_SEC_SESSION_DURATION;
extern const char * const  ATTR_SEC_SESSION_EXPIRES;
extern const char * const  ATTR_SEC_USER;
extern const char * const  ATTR_SEC_MY_REMOTE_USER_NAME;
extern const char * const  ATTR_SEC_NEW_SESSION;
extern const char * const  ATTR_SEC_USE_SESSION;
extern const char * const  ATTR_SEC_COOKIE;
extern const char * const  ATTR_SEC_AUTHENTICATED_USER;
extern const char * const  ATTR_SEC_TRIED_AUTHENTICATION;

extern const char * const  ATTR_MULTIPLE_TASKS_PER_PVMD;

extern const char * const  ATTR_UPDATESTATS_TOTAL;
extern const char * const  ATTR_UPDATESTATS_SEQUENCED;
extern const char * const  ATTR_UPDATESTATS_LOST;
extern const char * const  ATTR_UPDATESTATS_HISTORY;

extern const char * const  ATTR_QUILL_ENABLED;
extern const char * const  ATTR_QUILL_NAME;
extern const char * const  ATTR_QUILL_IS_REMOTELY_QUERYABLE;
extern const char * const  ATTR_QUILL_DB_IP_ADDR;
extern const char * const  ATTR_QUILL_DB_NAME;
extern const char * const  ATTR_QUILL_DB_QUERY_PASSWORD;

extern const char * const  ATTR_QUILL_SQL_TOTAL;
extern const char * const  ATTR_QUILL_SQL_LAST_BATCH;

extern const char * const  ATTR_CHECKPOINT_PLATFORM;
extern const char * const  ATTR_LAST_CHECKPOINT_PLATFORM;
extern const char * const  ATTR_IS_VALID_CHECKPOINT_PLATFORM;

extern const char * const  ATTR_WITHIN_RESOURCE_LIMITS;

extern const char * const  ATTR_HAD_IS_ACTIVE;
extern const char * const  ATTR_HAD_LIST;
extern const char * const  ATTR_HAD_INDEX;
extern const char * const  ATTR_HAD_SELF_ID;
extern const char * const  ATTR_HAD_CONTROLLEE_NAME;
extern const char * const  ATTR_TERMINATION_PENDING;
extern const char * const  ATTR_TERMINATION_EXITREASON;

extern const char * const  ATTR_REPLICATION_LIST;

extern const char * const  ATTR_TREQ_DIRECTION;
extern const char * const  ATTR_TREQ_INVALID_REQUEST;
extern const char * const  ATTR_TREQ_INVALID_REASON;
extern const char * const  ATTR_TREQ_HAS_CONSTRAINT;
extern const char * const  ATTR_TREQ_JOBID_LIST;
extern const char * const  ATTR_TREQ_PEER_VERSION;
extern const char * const  ATTR_TREQ_FTP;
extern const char * const  ATTR_TREQ_TD_SINFUL;
extern const char * const  ATTR_TREQ_TD_ID;
extern const char * const  ATTR_TREQ_CONSTRAINT;
extern const char * const  ATTR_TREQ_JOBID_ALLOW_LIST;
extern const char * const  ATTR_TREQ_JOBID_DENY_LIST;
extern const char * const  ATTR_TREQ_CAPABILITY;
extern const char * const  ATTR_TREQ_WILL_BLOCK;
extern const char * const  ATTR_TREQ_NUM_TRANSFERS;
extern const char * const  ATTR_TREQ_UPDATE_STATUS;
extern const char * const  ATTR_TREQ_UPDATE_REASON;
extern const char * const ATTR_TREQ_SIGNALED;
extern const char * const ATTR_TREQ_SIGNAL;
extern const char * const ATTR_TREQ_EXIT_CODE;
extern const char * const  ATTR_NEGOTIATOR_MATCH_EXPR;

extern const char * const ATTR_VM_TYPE;
extern const char * const ATTR_VM_MEMORY;
extern const char * const ATTR_VM_NETWORKING;
extern const char * const ATTR_VM_NETWORKING_TYPES;
extern const char * const ATTR_VM_HARDWARE_VT;
extern const char * const ATTR_VM_AVAIL_NUM;
extern const char * const ATTR_VM_ALL_GUEST_MACS;
extern const char * const ATTR_VM_ALL_GUEST_IPS;
extern const char * const ATTR_VM_GUEST_MAC;
extern const char * const ATTR_VM_GUEST_IP;
extern const char * const ATTR_VM_GUEST_MEM;
extern const char * const ATTR_VM_CKPT_MAC;
extern const char * const ATTR_VM_CKPT_IP;


//************* Added for Amazon Jobs ***************************//
extern const char * const ATTR_AMAZON_PUBLIC_KEY;
extern const char * const ATTR_AMAZON_PRIVATE_KEY;
extern const char * const ATTR_AMAZON_AMI_ID;
extern const char * const ATTR_AMAZON_KEY_PAIR_FILE;
extern const char * const ATTR_AMAZON_SECURITY_GROUPS;
extern const char * const ATTR_AMAZON_USER_DATA;
extern const char * const ATTR_AMAZON_USER_DATA_FILE;
extern const char * const ATTR_AMAZON_REMOTE_VM_NAME;
extern const char * const ATTR_AMAZON_INSTANCE_TYPE;
//************* End of changes for Amamzon Jobs *****************//


extern const char * const ATTR_REQUEST_CPUS;
extern const char * const ATTR_REQUEST_MEMORY;
extern const char * const ATTR_REQUEST_DISK;

// This is a record of the job exit status from a standard universe job exit
// via waitpid. It is in the job ad to implement the terminate_pending
// feature. It has to be here because of rampant global variable usage in the
// standard universe shadow. It saved a tremendous amount of code to just
// put this value in the job ad.
extern const char * const  ATTR_WAITPID_STATUS;
extern const char * const  ATTR_TERMINATION_REASON;

// Lease Manager
extern const char * const ATTR_LEASE_MANAGER_IP_ADDR;

// Valid settings for ATTR_JOB_MANAGED.
	// Managed by an external process (gridmanager)
extern const char * const  MANAGED_EXTERNAL;
	// Schedd should manage as normal
extern const char * const  MANAGED_SCHEDD;
	// Schedd should manage as normal.  External process doesn't want back.
extern const char * const  MANAGED_DONE;

extern const char * const  COLLECTOR_REQUIREMENTS;
extern const char * const  ATTR_PREV_LAST_HEARD_FROM;

extern const char * const ATTR_TRY_AGAIN;
extern const char * const ATTR_DOWNLOADING;
extern const char * const ATTR_TIMEOUT;
extern const char * const ATTR_CCBID;
extern const char * const ATTR_REQUEST_ID;
extern const char * const ATTR_SESSION_INFO;
extern const char * const ATTR_SSH_PUBLIC_SERVER_KEY;
extern const char * const ATTR_SSH_PRIVATE_CLIENT_KEY;
extern const char * const ATTR_SHELL;
extern const char * const ATTR_RETRY;
extern const char * const ATTR_SSH_KEYGEN_ARGS;

// Enumerate the ones that can't be constant strings..
typedef enum
{
	ATTRE_CONDOR_LOAD_AVG = 0,
	ATTRE_CONDOR_ADMIN,
	ATTRE_PLATFORM,
	ATTRE_TOTAL_LOAD,
	ATTRE_VERSION,
	// ....
} CONDOR_ATTR;

// Prototypes
int AttrInit( void );
const char *AttrGetName( CONDOR_ATTR );


// ------------------------------------------------------
// Stuff private to the environment variable manager
// ------------------------------------------------------
#if defined _CONDOR_ATTR_MAIN

// Flags available
typedef enum
{
	ATTR_FLAG_NONE = 0,			// No special treatment
	ATTR_FLAG_DISTRO,			// Plug in the distribution name
	ATTR_FLAG_DISTRO_UC,		// Plug in the UPPER CASE distribution name
	ATTR_FLAG_DISTRO_CAP,		// Plug in the Capitolized distribution name
} CONDOR_ATTR_FLAGS;

// Data on each env variable
typedef struct
{
	CONDOR_ATTR			sanity;		// Used to sanity check
	const char			*string;	// My format string
	CONDOR_ATTR_FLAGS	flag;		// Flags
	const char			*cached;	// Cached answer
} CONDOR_ATTR_ELEM;

// The actual list of variables indexed by CONDOR_ATTR
static CONDOR_ATTR_ELEM CondorAttrList[] =
{
	{ ATTRE_CONDOR_LOAD_AVG,"%sLoadAvg",		ATTR_FLAG_DISTRO_CAP, 0 },
	{ ATTRE_CONDOR_ADMIN,	"%sAdmin",			ATTR_FLAG_DISTRO_CAP, 0 },
	{ ATTRE_PLATFORM,		"%sPlatform",		ATTR_FLAG_DISTRO_CAP, 0 },
	{ ATTRE_TOTAL_LOAD,		"Total%sLoadAvg",	ATTR_FLAG_DISTRO_CAP, 0 },
	{ ATTRE_VERSION,		"%sVersion",		ATTR_FLAG_DISTRO_CAP, 0 },
	// ....
};
#endif		// _CONDOR_ATTR_MAIN

#endif
