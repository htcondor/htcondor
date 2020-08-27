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


#include "condor_common.h"
#include "command_strings.h"
#include "condor_perms.h"
#include <map>

#define USE_GENERATED_CMD_TABLES
#ifdef USE_GENERATED_CMD_TABLES
   #include "command_name_tables.h"
#else

const struct Translation DCTranslation[] = {
	{ "QMGMT_READ_CMD", QMGMT_READ_CMD }, 
	{ "QMGMT_WRITE_CMD", QMGMT_WRITE_CMD }, 
	{ "CONTINUE_CLAIM", CONTINUE_CLAIM },
	{ "SUSPEND_CLAIM", SUSPEND_CLAIM },
	{ "DEACTIVATE_CLAIM", DEACTIVATE_CLAIM },
	{ "DEACTIVATE_CLAIM_FORCIBLY", DEACTIVATE_CLAIM_FORCIBLY },
//	{ "LOCAL_STATUS", LOCAL_STATUS },				/* Not used */
//	{ "LOCAL_STATISTICS", LOCAL_STATISTICS },		/* Not used */
	{ "PERMISSION", PERMISSION },
//	{ "SET_DEBUG_FLAGS", SET_DEBUG_FLAGS },			/* Not used */
//	{ "PREEMPT_LOCAL_JOBS", PREEMPT_LOCAL_JOBS },	/* Not used */
//	{ "RM_LOCAL_JOB", RM_LOCAL_JOB },				/* Not used */
//	{ "START_FRGN_JOB", START_FRGN_JOB },			/* Not used */
//	{ "AVAILABILITY", AVAILABILITY },				/* Not used */
//	{ "NUM_FRGN_JOBS", NUM_FRGN_JOBS },				/* Not used */
//	{ "STARTD_INFO", STARTD_INFO },					/* Not used */
//	{ "SCHEDD_INFO", SCHEDD_INFO },					/* Not used */
	{ "NEGOTIATE", NEGOTIATE },
	{ "NEGOTIATE_WITH_SIGATTRS", NEGOTIATE_WITH_SIGATTRS},
	{ "SEND_JOB_INFO", SEND_JOB_INFO },
	{ "SEND_RESOURCE_REQUEST_LIST", SEND_RESOURCE_REQUEST_LIST }
	{ "NO_MORE_JOBS", NO_MORE_JOBS },
	{ "JOB_INFO", JOB_INFO },
//	{ "GIVE_STATUS", GIVE_STATUS },					/* Not used */
	{ "RESCHEDULE", RESCHEDULE },
//	{ "PING", PING },								/* Not used */
//	{ "NEGOTIATOR_INFO", NEGOTIATOR_INFO },			/* Not used */
//	{ "GIVE_STATUS_LINES", GIVE_STATUS_LINES },		/* Not used */
	{ "END_NEGOTIATE", END_NEGOTIATE },
	{ "REJECTED", REJECTED },
	{ "X_EVENT_NOTIFICATION", X_EVENT_NOTIFICATION },
//	{ "RECONFIG", RECONFIG },						/* Not used */
//	{ "GET_HISTORY", GET_HISTORY },					/* Not used */
//	{ "UNLINK_HISTORY_FILE", UNLINK_HISTORY_FILE },		/* Not used */
//	{ "UNLINK_HISTORY_FILE_DONE", UNLINK_HISTORY_FILE_DONE },	/* Not used */
//	{ "DO_NOT_UNLINK_HISTORY_FILE", DO_NOT_UNLINK_HISTORY_FILE },	/* Not used */
//	{ "SEND_ALL_JOBS", SEND_ALL_JOBS },				/* Not used */
//	{ "SEND_ALL_JOBS_PRIO", SEND_ALL_JOBS_PRIO },	/* Not used */
	{ "REQ_NEW_PROC", REQ_NEW_PROC },
	{ "PCKPT_FRGN_JOB", PCKPT_FRGN_JOB },
//	{ "SEND_RUNNING_JOBS", SEND_RUNNING_JOBS },		/* Not used */
//	{ "CHECK_CAPABILITY", CHECK_CAPABILITY },		/* Not used */
//	{ "GIVE_PRIORITY", GIVE_PRIORITY },				/* Not used */
	{ "MATCH_INFO", MATCH_INFO },
	{ "ALIVE", ALIVE },
	{ "REQUEST_CLAIM", REQUEST_CLAIM },
	{ "RELEASE_CLAIM", RELEASE_CLAIM },
	{ "ACTIVATE_CLAIM", ACTIVATE_CLAIM },
//	{ "PRIORITY_INFO", PRIORITY_INFO },				/* Not used */
	{ "PCKPT_ALL_JOBS", PCKPT_ALL_JOBS },
	{ "VACATE_ALL_CLAIMS", VACATE_ALL_CLAIMS },
	{ "GIVE_STATE", GIVE_STATE },
	{ "SET_PRIORITY", SET_PRIORITY },
//	{ "GIVE_CLASSAD", GIVE_CLASSAD },				/* Not used */
	{ "GET_PRIORITY", GET_PRIORITY },
//	{ "GIVE_REQUEST_AD", GIVE_REQUEST_AD },			/* Not used */
	{ "RESTART", RESTART },
	{ "RESTART_PEACEFUL", RESTART_PEACEFUL },
	{ "DAEMONS_OFF", DAEMONS_OFF },
	{ "DAEMONS_ON", DAEMONS_ON },
	{ "MASTER_OFF", MASTER_OFF },
	{ "CONFIG_VAL", CONFIG_VAL },
	{ "RESET_USAGE", RESET_USAGE },
	{ "DELETE_USER", DELETE_USER },
	{ "SET_PRIORITYFACTOR", SET_PRIORITYFACTOR },
	{ "RESET_ALL_USAGE", RESET_ALL_USAGE },
	{ "DAEMONS_OFF_FAST", DAEMONS_OFF_FAST },
	{ "DAEMONS_OFF_PEACEFUL", DAEMONS_OFF_PEACEFUL },
	{ "MASTER_OFF_FAST", MASTER_OFF_FAST },
	{ "GET_RESLIST", GET_RESLIST },
	{ "ATTEMPT_ACCESS", ATTEMPT_ACCESS },
	{ "VACATE_CLAIM", VACATE_CLAIM },
	{ "PCKPT_JOB", PCKPT_JOB },
	{ "DAEMON_OFF", DAEMON_OFF },
	{ "DAEMON_OFF_FAST", DAEMON_OFF_FAST },
	{ "DAEMON_OFF_PEACEFUL", DAEMON_OFF_PEACEFUL },
	{ "DAEMON_ON", DAEMON_ON },
	{ "GIVE_TOTALS_CLASSAD", GIVE_TOTALS_CLASSAD },
	{ "DUMP_STATE", DUMP_STATE },
	{ "PERMISSION_AND_AD", PERMISSION_AND_AD },
//	{ "REQUEST_NETWORK", REQUEST_NETWORK },			/* Not used */
	{ "VACATE_ALL_FAST", VACATE_ALL_FAST },
	{ "VACATE_CLAIM_FAST", VACATE_CLAIM_FAST },
	{ "REJECTED_WITH_REASON", REJECTED_WITH_REASON },
	{ "START_AGENT", START_AGENT },
	{ "ACT_ON_JOBS", ACT_ON_JOBS },
	{ "CA_AUTH_CMD", CA_AUTH_CMD },
	{ "CA_CMD", CA_CMD },
	{ "CA_REQUEST_CLAIM", CA_REQUEST_CLAIM },
	{ "CA_RELEASE_CLAIM", CA_RELEASE_CLAIM },
	{ "CA_ACTIVATE_CLAIM", CA_ACTIVATE_CLAIM },
	{ "CA_DEACTIVATE_CLAIM", CA_DEACTIVATE_CLAIM },
	{ "CA_SUSPEND_CLAIM", CA_SUSPEND_CLAIM },
	{ "CA_RESUME_CLAIM", CA_RESUME_CLAIM },
	{ "CA_RENEW_LEASE_FOR_CLAIM", CA_RENEW_LEASE_FOR_CLAIM },
	{ "CA_LOCATE_STARTER", CA_LOCATE_STARTER },
	{ "CA_RECONNECT_JOB", CA_RECONNECT_JOB },
	{ "DC_SIGSUSPEND", DC_SIGSUSPEND },
	{ "DC_SIGCONTINUE", DC_SIGCONTINUE },
	{ "DC_SIGSOFTKILL", DC_SIGSOFTKILL },
	{ "DC_SIGHARDKILL", DC_SIGHARDKILL },
	{ "DC_SIGPCKPT", DC_SIGPCKPT },
	{ "DC_SIGREMOVE", DC_SIGREMOVE },
	{ "DC_SIGHOLD", DC_SIGHOLD },
	{ "DC_RAISESIGNAL", DC_RAISESIGNAL },
	{ "DC_CONFIG_PERSIST", DC_CONFIG_PERSIST },
	{ "DC_CONFIG_RUNTIME", DC_CONFIG_RUNTIME },
	{ "DC_GET_SESSION_TOKEN", DC_GET_SESSION_TOKEN },
	{ "DC_RECONFIG", DC_RECONFIG },
	{ "DC_RECONFIG_FULL", DC_RECONFIG_FULL },
	{ "DC_OFF_GRACEFUL", DC_OFF_GRACEFUL },
	{ "DC_OFF_FAST", DC_OFF_FAST },
	{ "DC_OFF_PEACEFUL", DC_OFF_PEACEFUL },
	{ "DC_CONFIG_VAL", DC_CONFIG_VAL },
	{ "DC_CHILDALIVE", DC_CHILDALIVE },
	{ "DC_FETCH_LOG", DC_FETCH_LOG },
	{ "DC_INVALIDATE_KEY", DC_INVALIDATE_KEY },
	{ "FILETRANS_UPLOAD", FILETRANS_UPLOAD },
	{ "FILETRANS_DOWNLOAD", FILETRANS_DOWNLOAD },
//	{ "PW_SETPASS", PW_SETPASS },					/* Not used */
//	{ "PW_GETPASS", PW_GETPASS },					/* Not used */
//	{ "PW_CLEARPASS", PW_CLEARPASS },				/* Not used */
	{ "SHADOW_UPDATEINFO", SHADOW_UPDATEINFO },
//	{ "TAKE_MATCH", TAKE_MATCH },					/* Not used */
	{ "MPI_START_COMRADE", MPI_START_COMRADE },
	{ "CHILD_OFF", CHILD_OFF },
	{ "CHILD_OFF_FAST", CHILD_OFF_FAST },
	{ "CHILD_ON", CHILD_ON },
	{ "VM_REGISTER", VM_REGISTER },
	{ "DELEGATE_GSI_CRED_SCHEDD", DELEGATE_GSI_CRED_SCHEDD },
	{ "DELEGATE_GSI_CRED_STARTER", DELEGATE_GSI_CRED_STARTER },
	{ "DELEGATE_GSI_CRED_STARTD", DELEGATE_GSI_CRED_STARTD },
	{ "REQUEST_SANDBOX_LOCATION", REQUEST_SANDBOX_LOCATION },
	/* maybe not right place, but so what for now */
	{ "TRANSFERD_REGISTER", TRANSFERD_REGISTER },
	{ "TRANSFERD_CONTROL_CHANNEL", TRANSFERD_CONTROL_CHANNEL},
	{ "TRANSFERD_WRITE_FILES", TRANSFERD_WRITE_FILES},
	{ "TRANSFERD_READ_FILES", TRANSFERD_READ_FILES},
	{ "VM_UNIV_GAHP_ERROR", VM_UNIV_GAHP_ERROR },
	{ "VM_UNIV_VMPID", VM_UNIV_VMPID },
	{ "VM_UNIV_GUEST_IP", VM_UNIV_GUEST_IP },
	{ "VM_UNIV_GUEST_MAC", VM_UNIV_GUEST_MAC },
	{ "CCB_REGISTER", CCB_REGISTER },
	{ "CCB_REQUEST", CCB_REQUEST },
	{ "CCB_REVERSE_CONNECT", CCB_REVERSE_CONNECT },
	{ "GET_JOB_CONNECT_INFO", GET_JOB_CONNECT_INFO },
	{ "CREATE_JOB_OWNER_SEC_SESSION", CREATE_JOB_OWNER_SEC_SESSION },
	{ "START_SSHD", START_SSHD },
	{ "STARTER_PEEK", STARTER_PEEK },
	{ "SHARED_PORT_CONNECT", SHARED_PORT_CONNECT },
	{ "SHARED_PORT_PASS_SOCK", SHARED_PORT_PASS_SOCK },
	{ "RECYCLE_SHADOW", RECYCLE_SHADOW },
        { "CLEAR_DIRTY_JOB_ATTRS", CLEAR_DIRTY_JOB_ATTRS },
        { "UPDATE_JOBAD", UPDATE_JOBAD },
	{ "DRAIN_JOBS", DRAIN_JOBS },
	{ "CANCEL_DRAIN_JOBS", CANCEL_DRAIN_JOBS },
	{ "DC_AUTHENTICATE", DC_AUTHENTICATE },
	{ "DC_SEC_QUERY", DC_SEC_QUERY },
	{ "DC_NOP", DC_NOP },
	{ "DC_NOP_READ", DC_NOP_READ },
	{ "DC_NOP_WRITE", DC_NOP_WRITE },
	{ "DC_NOP_NEGOTIATOR", DC_NOP_NEGOTIATOR },
	{ "DC_NOP_ADMINISTRATOR", DC_NOP_ADMINISTRATOR },
	{ "DC_NOP_OWNER", DC_NOP_OWNER },
	{ "DC_NOP_CONFIG", DC_NOP_CONFIG },
	{ "DC_NOP_DAEMON", DC_NOP_DAEMON },
	{ "DC_NOP_ADVERTISE_STARTD", DC_NOP_ADVERTISE_STARTD },
	{ "DC_NOP_ADVERTISE_SCHEDD", DC_NOP_ADVERTISE_SCHEDD },
	{ "DC_NOP_ADVERTISE_MASTER", DC_NOP_ADVERTISE_MASTER },
	{ "DC_OFF_FORCE", DC_OFF_FORCE },
	{ "DC_PURGE_LOG", DC_PURGE_LOG },
	{ "DC_SET_FORCE_SHUTDOWN", DC_SET_FORCE_SHUTDOWN },
	{ "DC_SET_PEACEFUL_SHUTDOWN", DC_SET_PEACEFUL_SHUTDOWN },
	{ "DC_TIME_OFFSET", DC_TIME_OFFSET },
	{ "SPOOL_JOB_FILES", SPOOL_JOB_FILES },
	{ "SPOOL_JOB_FILES_WITH_PERMS", SPOOL_JOB_FILES_WITH_PERMS },
	{ "TRANSFER_DATA", TRANSFER_DATA },
	{ "TRANSFER_DATA_WITH_PERMS", TRANSFER_DATA_WITH_PERMS },
	{ "UPDATE_GSI_CRED", UPDATE_GSI_CRED },
	{ "GET_MYPROXY_PASSWORD", GET_MYPROXY_PASSWORD },
	{ "GIVE_MATCHES", GIVE_MATCHES },
	{ "SET_ACCUMUSAGE", SET_ACCUMUSAGE },
	{ "SET_BEGINTIME", SET_BEGINTIME },
	{ "SET_LASTTIME", SET_LASTTIME },
	{ "SET_SHUTDOWN_PROGRAM", SET_SHUTDOWN_PROGRAM },
	{ "GET_PRIORITY_ROLLUP", GET_PRIORITY_ROLLUP },
	{ "STORE_CRED", STORE_CRED },
	{ "STORE_POOL_CRED", STORE_POOL_CRED },
	{ "CREDD_STORE_CRED", CREDD_STORE_CRED },
	{ "CREDD_GET_CRED", CREDD_GET_CRED },
	{ "CREDD_REMOVE_CRED" , CREDD_REMOVE_CRED },
	{ "CREDD_QUERY_CRED", CREDD_QUERY_CRED },
	{ "CREDD_GET_PASSWD", CREDD_GET_PASSWD },
	{ "CREDD_NOP", CREDD_NOP },
	{ "CREDD_CHECK_CREDS", CREDD_CHECK_CREDS },
	{ "HAD_ALIVE_CMD", HAD_ALIVE_CMD },
	{ "HAD_SEND_ID_CMD", HAD_SEND_ID_CMD },
//	{ "HAD_REPL_UPDATE_VERSION", HAD_REPL_UPDATE_VERSION },	/* Not used */
	{ "HAD_BEFORE_PASSIVE_STATE", HAD_BEFORE_PASSIVE_STATE },
	{ "HAD_AFTER_ELECTION_STATE", HAD_AFTER_ELECTION_STATE },
	{ "HAD_AFTER_LEADER_STATE", HAD_AFTER_LEADER_STATE },
	{ "HAD_IN_LEADER_STATE", HAD_IN_LEADER_STATE },
	{ "REPLICATION_TRANSFER_FILE", REPLICATION_TRANSFER_FILE },
	{ "REPLICATION_LEADER_VERSION", REPLICATION_LEADER_VERSION },
	{ "REPLICATION_NEWLY_JOINED_VERSION", REPLICATION_NEWLY_JOINED_VERSION },
	{ "REPLICATION_GIVING_UP_VERSION", REPLICATION_GIVING_UP_VERSION },
	{ "REPLICATION_SOLICIT_VERSION", REPLICATION_SOLICIT_VERSION },
	{ "REPLICATION_SOLICIT_VERSION_REPLY", REPLICATION_SOLICIT_VERSION_REPLY },
	{ "REPLICATION_TRANSFER_FILE_NEW", REPLICATION_TRANSFER_FILE_NEW },
	{ "QUERY_SCHEDD_HISTORY", QUERY_SCHEDD_HISTORY },
	{ "QUERY_JOB_ADS", QUERY_JOB_ADS },
	{ "SWAP_CLAIM_AND_ACTIVATION", SWAP_CLAIM_AND_ACTIVATION },
	{ "FETCH_PROXY_DELEGATION", FETCH_PROXY_DELEGATION },
	{ "", 0 }
};

const struct Translation CollectorTranslation[] = {
	{ "UPDATE_STARTD_AD", UPDATE_STARTD_AD },
    { "UPDATE_STARTD_AD_WITH_ACK", UPDATE_STARTD_AD_WITH_ACK },
	{ "UPDATE_SCHEDD_AD", UPDATE_SCHEDD_AD },
	{ "UPDATE_MASTER_AD", UPDATE_MASTER_AD },
//	{ "UPDATE_GATEWAY_AD", UPDATE_GATEWAY_AD },		/* Not used */
	{ "UPDATE_CKPT_SRVR_AD", UPDATE_CKPT_SRVR_AD },
	{ "UPDATE_NEGOTIATOR_AD", UPDATE_NEGOTIATOR_AD },
	{ "UPDATE_HAD_AD", UPDATE_HAD_AD },
//	{ "UPDATE_XFER_SERVICE_AD", UPDATE_XFER_SERVICE_AD },	/* Not used */
//	{ "UPDATE_LEASE_MANAGER_AD", UPDATE_LEASE_MANAGER_AD },	/* Not used */
	{ "UPDATE_AD_GENERIC", UPDATE_AD_GENERIC },
	{ "QUERY_STARTD_ADS", QUERY_STARTD_ADS },
	{ "QUERY_SCHEDD_ADS", QUERY_SCHEDD_ADS },
	{ "QUERY_MASTER_ADS", QUERY_MASTER_ADS },
//	{ "QUERY_GATEWAY_ADS", QUERY_GATEWAY_ADS },		/* Not used */
	{ "QUERY_CKPT_SRVR_ADS", QUERY_CKPT_SRVR_ADS },
	{ "QUERY_STARTD_PVT_ADS", QUERY_STARTD_PVT_ADS },
	{ "QUERY_NEGOTIATOR_ADS", QUERY_NEGOTIATOR_ADS },
	{ "QUERY_HAD_ADS", QUERY_HAD_ADS },
//	{ "QUERY_XFER_SERVICE_ADS", QUERY_XFER_SERVICE_ADS },	/* Not used */
//	{ "QUERY_LEASE_MANAGER_ADS", QUERY_LEASE_MANAGER_ADS },	/* Not used */
	{ "QUERY_GENERIC_ADS", QUERY_GENERIC_ADS },
	{ "UPDATE_SUBMITTOR_AD", UPDATE_SUBMITTOR_AD },
	{ "QUERY_SUBMITTOR_ADS", QUERY_SUBMITTOR_ADS },
	{ "INVALIDATE_STARTD_ADS", INVALIDATE_STARTD_ADS },
	{ "INVALIDATE_SCHEDD_ADS", INVALIDATE_SCHEDD_ADS },
	{ "INVALIDATE_MASTER_ADS", INVALIDATE_MASTER_ADS },
//	{ "INVALIDATE_GATEWAY_ADS", INVALIDATE_GATEWAY_ADS },	/* Not used */
	{ "INVALIDATE_CKPT_SRVR_ADS", INVALIDATE_CKPT_SRVR_ADS },
	{ "INVALIDATE_SUBMITTOR_ADS", INVALIDATE_SUBMITTOR_ADS },
	{ "INVALIDATE_NEGOTIATOR_ADS", INVALIDATE_NEGOTIATOR_ADS },
	{ "INVALIDATE_HAD_ADS", INVALIDATE_HAD_ADS },
//	{ "INVALIDATE_XFER_SERVICE_ADS", INVALIDATE_XFER_SERVICE_ADS },	/* Not used */
//	{ "INVALIDATE_LEASE_MANAGER_ADS", INVALIDATE_LEASE_MANAGER_ADS },	/* Not used */
	{ "INVALIDATE_ADS_GENERIC", INVALIDATE_ADS_GENERIC },
	{ "UPDATE_COLLECTOR_AD", UPDATE_COLLECTOR_AD },
	{ "QUERY_COLLECTOR_ADS", QUERY_COLLECTOR_ADS },
	{ "INVALIDATE_COLLECTOR_ADS", INVALIDATE_COLLECTOR_ADS },
	{ "QUERY_HIST_STARTD", QUERY_HIST_STARTD },
	{ "QUERY_HIST_STARTD_LIST", QUERY_HIST_STARTD_LIST },
	{ "QUERY_HIST_SUBMITTOR", QUERY_HIST_SUBMITTOR },
	{ "QUERY_HIST_SUBMITTOR_LIST", QUERY_HIST_SUBMITTOR_LIST },
	{ "QUERY_HIST_GROUPS", QUERY_HIST_GROUPS },
	{ "QUERY_HIST_GROUPS_LIST", QUERY_HIST_GROUPS_LIST },
	{ "QUERY_HIST_SUBMITTORGROUPS", QUERY_HIST_SUBMITTORGROUPS },
	{ "QUERY_HIST_SUBMITTORGROUPS_LIST", QUERY_HIST_SUBMITTORGROUPS_LIST },
	{ "QUERY_HIST_CKPTSRVR", QUERY_HIST_CKPTSRVR },
	{ "QUERY_HIST_CKPTSRVR_LIST", QUERY_HIST_CKPTSRVR_LIST },
	{ "UPDATE_LICENSE_AD", UPDATE_LICENSE_AD },
	{ "QUERY_LICENSE_ADS", QUERY_LICENSE_ADS },
	{ "INVALIDATE_LICENSE_ADS", INVALIDATE_LICENSE_ADS },
	{ "UPDATE_STORAGE_AD", UPDATE_STORAGE_AD },
	{ "QUERY_STORAGE_ADS", QUERY_STORAGE_ADS },
	{ "INVALIDATE_STORAGE_ADS", INVALIDATE_STORAGE_ADS },
	{ "QUERY_ANY_ADS", QUERY_ANY_ADS },
	{ "STARTER_HOLD_JOB", STARTER_HOLD_JOB },
	{ "UPDATE_GRID_AD", UPDATE_GRID_AD },
	{ "QUERY_GRID_ADS", QUERY_GRID_ADS },
	{ "INVALIDATE_GRID_ADS", INVALIDATE_GRID_ADS },
	{ "MERGE_STARTD_AD", MERGE_STARTD_AD },
	{ "UPDATE_ACCOUNTING_AD", UPDATE_ACCOUNTING_AD },
	{ "QUERY_ACCOUNTING_ADS", QUERY_ACCOUNTING_ADS },
	{ "INVALIDATE_ACCOUNTING_ADS", INVALIDATE_ACCOUNTING_ADS },
	{ "", 0 }
};

#endif

const struct Translation CAResultTranslation[] = {
	{ "Success", CA_SUCCESS },
	{ "Failure", CA_FAILURE },
	{ "NotAuthenticated", CA_NOT_AUTHENTICATED },
	{ "NotAuthorized", CA_NOT_AUTHORIZED },
	{ "InvalidRequest", CA_INVALID_REQUEST },
	{ "InvalidState", CA_INVALID_STATE },
	{ "InvalidReply", CA_INVALID_REPLY },
	{ "LocateFailed", CA_LOCATE_FAILED },
	{ "ConnectFailed", CA_CONNECT_FAILED },
	{ "CommunicationError", CA_COMMUNICATION_ERROR },
	{ "", 0 }
};

const struct Translation DrainingScheduleTranslation[] = {
	{ "graceful", DRAIN_GRACEFUL },
	{ "quick", DRAIN_QUICK },
	{ "fast", DRAIN_FAST },
	{ "", 0 }
};

#ifdef USE_GENERATED_CMD_TABLES
// binary search of an array of structures containing a member psz
// find the (case insensitive) matching element in the array
// and return a pointer to that element.
template <typename T>
const T * BinaryLookup (const T aTable[], int cElms, int id)
{
	if (cElms <= 0)
		return NULL;

	int ixLower = 0;
	int ixUpper = cElms-1;
	for (;;) {
		if (ixLower > ixUpper)
			return NULL; // return null for "not found"

		int ix = (ixLower + ixUpper) / 2;
		int iMatch = aTable[ix].id - id;
		if (iMatch < 0)
			ixLower = ix+1;
		else if (iMatch > 0)
			ixUpper = ix-1;
		else
			return &aTable[ix];
	}
}

template <typename T>
const T * BinaryLookupFromIndex (const int aIndex[], const T aTable[], int cElms, const char * name, int (*fncmp)(const char *, const char *))
{
	if (cElms <= 0)
		return NULL;

	int ixLower = 0;
	int ixUpper = cElms-1;
	for (;;) {
		if (ixLower > ixUpper)
			return NULL; // return null for "not found"

		int ix = (ixLower + ixUpper) / 2;
		int iMatch = fncmp(aTable[aIndex[ix]].name, name);
		if (iMatch < 0)
			ixLower = ix+1;
		else if (iMatch > 0)
			ixUpper = ix-1;
		else
			return &aTable[aIndex[ix]];
	}
}


const char * getCommandString(int id)
{
	const BTranslation * ptr = BinaryLookup(
			DCTranslation,
			DCTranslation_count,
			id);
	if (ptr) return ptr->name;
	return NULL;
}

int getCommandNum(const char * name)
{
	const BTranslation * ptr = BinaryLookupFromIndex(
			DCTranslationIndexByName,
			DCTranslation,
			DCTranslation_count,
			name,
			strcasecmp);
	if (ptr) return ptr->id;
	return -1;
}

const char * getCollectorCommandString(int id)
{
	// collector commands are at the start of the DC command table, they are the 'unbased' (i.e. 0 based ones)
	const BTranslation * pTable = &DCTranslation[DCTranslation_COLLECTOR_start];
	int cItems = DCTranslation_COLLECTOR_count;
	const BTranslation * ptr = BinaryLookup(pTable, cItems, id);
	if (ptr) return ptr->name;
	return NULL;
}

int getCollectorCommandNum(const char * name)
{
	// we have to do name lookup within the entire name table.
	int cmd = getCommandNum(name);
	// then check to see if we got back a collector command  id
	if (cmd >= DCTranslation[DCTranslation_COLLECTOR_start].id &&
		cmd <= DCTranslation[DCTranslation_COLLECTOR_start + DCTranslation_COLLECTOR_count -1].id)
		return cmd;
	return -1;
}

#else

const char*
getCommandString( int num )
{
	char const *result = getNameFromNum( num, DCTranslation );
	if( !result ) {
		return getCollectorCommandString(num);
	}
	return result;
}


int
getCommandNum( const char* command )
{
	int result = getNumFromName( command, DCTranslation );
	if( -1 == result ) {
		return getCollectorCommandNum(command);
	}
	return result;
}

const char*
getCollectorCommandString( int num )
{
	return getNameFromNum( num, CollectorTranslation );
}

int
getCollectorCommandNum( const char* command )
{
	return getNumFromName( command, CollectorTranslation );
}

#endif

// return the command name for known commmand or "command NNN" for unknown commands
// returned pointer is valid forever and the caller does not free it
const char*
getCommandStringSafe( int num )
{
	char const *result = getCommandString(num);
	if (result)
		return result;

	return getUnknownCommandString(num);
}

// return a string pointer representation of an unknown command id
// that can be cached by the caller for the lifetime of the process.
const char*
getUnknownCommandString(int num)
{
	static std::map<int, const char*> * pcmds = NULL;
	if ( ! pcmds) {
		pcmds = new std::map<int, const char*>();
		if ( ! pcmds) return "malloc-fail!";
	}

	std::map<int, const char*>::iterator it = pcmds->find(num);
	if (it != pcmds->end()) {
		return it->second;
	}

	static const char fmt[] = "command %u";
	char * pstr = (char*)malloc(sizeof(fmt)+8); // max int string is 10 bytes (-2 for %d)
	if ( ! pstr) return "malloc-fail!";
	sprintf(pstr, fmt, num);
	(*pcmds)[num] = pstr;
	return pstr;
}


const char*
getCAResultString( CAResult r )
{
	return getNameFromNum( (int)r, CAResultTranslation );
}

CAResult
getCAResultNum( const char* str )
{
	return (CAResult)getNumFromName( str, CAResultTranslation );
}

int
getDrainingScheduleNum( char const *name )
{
	return getNumFromName( name, DrainingScheduleTranslation );
}

char const *
getDrainingScheduleName( int num )
{
	return getNameFromNum( num, DrainingScheduleTranslation );
}

int getSampleCommand( int authz_level ) {
        switch(authz_level) {
                case ALLOW:
                        return DC_NOP;
                case READ:
                        return DC_NOP_READ;
                case WRITE:
                        return DC_NOP_WRITE;
                case NEGOTIATOR:
                        return DC_NOP_NEGOTIATOR;
                case ADMINISTRATOR:
                        return DC_NOP_ADMINISTRATOR;
                case OWNER:
                        return DC_NOP_OWNER;
                case CONFIG_PERM:
                        return DC_NOP_CONFIG;
                case DAEMON:
                        return DC_NOP_DAEMON;
                case ADVERTISE_STARTD_PERM:
                        return DC_NOP_ADVERTISE_STARTD;
                case ADVERTISE_SCHEDD_PERM:
                        return DC_NOP_ADVERTISE_SCHEDD;
                case ADVERTISE_MASTER_PERM:
                        return DC_NOP_ADVERTISE_MASTER;
        }
        return -1;
}

