/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

// List of attributes used in ClassAds  If you find yourself using anything
// other than ATTR_<blah> to add/lookup expressions/variables *STOP*, add the
// new variable to this file and use the ATTR_<blah> symbolic constant.  --RR

#ifndef __CONDOR_ATTRIBUTES_H__
#define __CONDOR_ATTRIBUTES_H__

#include "condor_header_features.h"

BEGIN_C_DECLS

extern const char ATTR_ACTIVITY[];
extern const char ATTR_ARCH[];
extern const char ATTR_AVAIL_BANDWIDTH_TO_SUBMIT_MACHINE[];
extern const char ATTR_AVAIL_BANDWIDTH_TO_LAST_CKPT_SERVER[];
extern const char ATTR_AVAIL_BANDWIDTH_TO_CKPT_SERVER [];
extern const char ATTR_BANDWIDTH_TO_SUBMIT_MACHINE[];
extern const char ATTR_BANDWIDTH_TO_LAST_CKPT_SERVER[];
extern const char ATTR_BANDWIDTH_TO_CKPT_SERVER [];
extern const char ATTR_BUFFER_SIZE[];
extern const char ATTR_BUFFER_BLOCK_SIZE[];
extern const char ATTR_BUFFER_BLOCKS_USED[];
extern const char ATTR_BUFFER_PREFETCH_SIZE[];
extern const char ATTR_BYTES_SENT[];
extern const char ATTR_BYTES_RECVD[];
extern const char ATTR_CAPABILITY[];
extern const char ATTR_REQUESTED_CAPACITY[];
extern const char ATTR_CKPT_ARCH[];
extern const char ATTR_CKPT_LAST_READ[];
extern const char ATTR_CKPT_OPSYS[];
extern const char ATTR_CLIENT_MACHINE[];
extern const char ATTR_CLOCK_DAY[];
extern const char ATTR_CLOCK_MIN[];
extern const char ATTR_CLUSTER_ID[];
extern const char ATTR_COMPLETION_DATE[];
extern const char ATTR_CONDOR_LOAD_AVG[];
extern const char ATTR_CONDOR_ADMIN[];
extern const char ATTR_CONSOLE_IDLE[];
extern const char ATTR_CONTINUE[];
extern const char ATTR_CORE_SIZE[];
extern const char ATTR_CPUS[];
extern const char ATTR_CURRENT_HOSTS[];
extern const char ATTR_CURRENT_RANK[];
extern const char ATTR_DESTINATION[];
extern const char ATTR_DISK[];
extern const char ATTR_DISK_USAGE[];
extern const char ATTR_ENTERED_CURRENT_ACTIVITY[];
extern const char ATTR_ENTERED_CURRENT_STATE[];
extern const char ATTR_EXECUTABLE_SIZE[];
extern const char ATTR_FILE_NAME[];
extern const char ATTR_FILE_SIZE[];
extern const char ATTR_FILE_SYSTEM_DOMAIN[];
extern const char ATTR_FILE_REMAPS[];
extern const char ATTR_FILE_READ_COUNT[];
extern const char ATTR_FILE_READ_BYTES[];
extern const char ATTR_FILE_WRITE_COUNT[];
extern const char ATTR_FILE_WRITE_BYTES[];
extern const char ATTR_FILE_SEEK_COUNT[];
extern const char ATTR_FLAVOR[];
extern const char ATTR_FORCE[];
extern const char ATTR_HELD_JOBS[];
extern const char ATTR_IDLE_JOBS[];
extern const char ATTR_IMAGE_SIZE[];
extern const char ATTR_INACTIVE[];
extern const char ATTR_JOB_ARGUMENTS[];
extern const char ATTR_JOB_CMD[];
extern const char ATTR_JOB_CMDEXT[];
extern const char ATTR_JOB_ENVIRONMENT[];
extern const char ATTR_JOB_ERROR[];
extern const char ATTR_JOB_EXIT_STATUS[];
extern const char ATTR_JOB_ID[];
extern const char ATTR_JOB_INPUT[];
extern const char ATTR_JOB_IWD[];
extern const char ATTR_JOB_LOCAL_CPU[];
extern const char ATTR_JOB_LOCAL_SYS_CPU[];
extern const char ATTR_JOB_LOCAL_USER_CPU[];
extern const char ATTR_JOB_NOTIFICATION[];
extern const char ATTR_JOB_OUTPUT[];
extern const char ATTR_JOB_PRIO[];
extern const char ATTR_JOB_COMMITTED_TIME[];
extern const char ATTR_JOB_REMOTE_SYS_CPU[];
extern const char ATTR_JOB_REMOTE_USER_CPU[];
extern const char ATTR_JOB_REMOTE_WALL_CLOCK[];
extern const char ATTR_JOB_ROOT_DIR[];
extern const char ATTR_JOB_START[];
extern const char ATTR_JOB_START_DATE[];
extern const char ATTR_JOB_STATUS[];
extern const char ATTR_JOB_UNIVERSE[];
extern const char ATTR_JOB_WALL_CLOCK_CKPT[];
extern const char ATTR_KEYBOARD_IDLE[];
extern const char ATTR_KFLOPS[];
extern const char ATTR_KILL[];
extern const char ATTR_KILL_SIG[];
extern const char ATTR_LAST_BENCHMARK[];
extern const char ATTR_LAST_CKPT_SERVER[];
extern const char ATTR_LAST_CKPT_TIME[];
extern const char ATTR_LAST_VACATE_TIME[];
extern const char ATTR_LAST_HEARD_FROM[];
extern const char ATTR_LAST_PERIODIC_CHECKPOINT[];
extern const char ATTR_LAST_REMOTE_HOST[];
extern const char ATTR_LAST_UPDATE[];
extern const char ATTR_LOAD_AVG[];
extern const char ATTR_MACHINE[];
extern const char ATTR_MASTER_IP_ADDR[];
extern const char ATTR_MAX_HOSTS[];
extern const char ATTR_MAX_JOBS_RUNNING[];
extern const char ATTR_MEMORY[];
extern const char ATTR_MIN_HOSTS[];
extern const char ATTR_MIPS[];
extern const char ATTR_MPI_IS_MASTER[];
extern const char ATTR_MY_TYPE[];
extern const char ATTR_NAME[];
extern const char ATTR_NICE_USER[];
extern const char ATTR_NEST[];
extern const char ATTR_NEXT_CLUSTER_NUM[];
extern const char ATTR_NOTIFY_USER[];
extern const char ATTR_NT_DOMAIN[];
extern const char ATTR_NUM_CKPTS[];
extern const char ATTR_NUM_HOPS_TO_SUBMIT_MACHINE[];
extern const char ATTR_NUM_HOPS_TO_LAST_CKPT_SERVER[];
extern const char ATTR_NUM_HOPS_TO_CKPT_SERVER[];
extern const char ATTR_NUM_RESTARTS[];
extern const char ATTR_NUM_USERS[];
extern const char ATTR_OPSYS[];
extern const char ATTR_ORIG_MAX_HOSTS[];
extern const char ATTR_OWNER[];
extern const char ATTR_PERIODIC_CHECKPOINT[];
extern const char ATTR_PLATFORM[];
extern const char ATTR_PREFERENCES[];
extern const char ATTR_PREV_SEND_ESTIMATE[];
extern const char ATTR_PREV_RECV_ESTIMATE[];
extern const char ATTR_PRIO[];
extern const char ATTR_PROC_ID[];
extern const char ATTR_Q_DATE[];
extern const char ATTR_RANK[];
extern const char ATTR_REAL_UID[];
extern const char ATTR_REMOTE_HOST[];
extern const char ATTR_REMOTE_POOL[];
extern const char ATTR_REMOTE_USER[];
extern const char ATTR_REMOTE_USER_PRIO[];
extern const char ATTR_REQUIREMENTS[];
extern const char ATTR_RSC_BYTES_SENT[];
extern const char ATTR_RSC_BYTES_RECVD[];
extern const char ATTR_RUNNING_JOBS[];
extern const char ATTR_RUN_BENCHMARKS[];
extern const char ATTR_SHADOW_IP_ADDR[];
extern const char ATTR_MY_ADDRESS[];
extern const char ATTR_SCHEDD_IP_ADDR[];
extern const char ATTR_SCHEDD_NAME[];
extern const char ATTR_SOURCE[];
extern const char ATTR_START[];
extern const char ATTR_STARTD_IP_ADDR[];
extern const char ATTR_STATE[];
extern const char ATTR_STATUS[];
extern const char ATTR_SUBMITTOR_PRIO[];
extern const char ATTR_SUBNET[];
extern const char ATTR_SUSPEND[];
extern const char ATTR_TARGET_TYPE[];
extern const char ATTR_TOTAL_CONDOR_LOAD_AVG[];
extern const char ATTR_TOTAL_CPUS[];
extern const char ATTR_TOTAL_DISK[];
extern const char ATTR_TOTAL_REMOVED_JOBS[];
extern const char ATTR_TOTAL_HELD_JOBS[];
extern const char ATTR_TOTAL_IDLE_JOBS[];
extern const char ATTR_TOTAL_LOAD_AVG[];
extern const char ATTR_TOTAL_MEMORY[];
extern const char ATTR_TOTAL_RUNNING_JOBS[];
extern const char ATTR_TOTAL_VIRTUAL_MACHINES[];
extern const char ATTR_TOTAL_VIRTUAL_MEMORY[];
extern const char ATTR_UID_DOMAIN[];
extern const char ATTR_ULOG_FILE[];
extern const char ATTR_UPDATE_INTERVAL[];
extern const char ATTR_UPDATE_PRIO[];
extern const char ATTR_USE_CKPT_SERVER[];
extern const char ATTR_USER[];
extern const char ATTR_VACATE[];
extern const char ATTR_VACATE_CKPT_SERVER[];
extern const char ATTR_VIRTUAL_MEMORY[];
extern const char ATTR_WANT_CHECKPOINT[];
extern const char ATTR_WANT_REMOTE_SYSCALLS[];
extern const char ATTR_WANT_SUBMIT_NET_STATS[];
extern const char ATTR_WANT_LAST_CKPT_SERVER_NET_STATS[];
extern const char ATTR_WANT_CKPT_SERVER_NET_STATS[];
extern const char ATTR_COLLECTOR_IP_ADDR[];
extern const char ATTR_NUM_HOSTS_TOTAL[];
extern const char ATTR_NUM_HOSTS_CLAIMED[];
extern const char ATTR_NUM_HOSTS_UNCLAIMED[];
extern const char ATTR_NUM_HOSTS_OWNER[];
extern const char ATTR_VERSION[];
extern const char ATTR_VIRTUAL_MACHINE_ID[];
extern const char ATTR_TRANSFER_TYPE[];
extern const char ATTR_TRANSFER_FILES[];
extern const char ATTR_TRANSFER_KEY[];
extern const char ATTR_TRANSFER_INPUT_FILES[];
extern const char ATTR_TRANSFER_OUTPUT_FILES[];
extern const char ATTR_TRANSFER_SOCKET[];
extern const char ATTR_SERVER_TIME[];
extern const char ATTR_SHADOW_BIRTHDATE[];
extern const char ATTR_HOLD_REASON[];
extern const char ATTR_WANT_RESOURCE_AD[];

END_C_DECLS

#endif
