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
#include "enum_utils.h"


const struct Translation ClaimTypeTranslation[] = {
	{ "NONE", CLAIM_NONE },
	{ "COD", CLAIM_COD },
	{ "OPPORTUNISTIC", CLAIM_OPPORTUNISTIC },
	{ "FETCH", CLAIM_FETCH },
	{ "NULL", 0 }
};


const struct Translation ClaimStateTranslation[] = {
    { "Unclaimed", CLAIM_UNCLAIMED },
    { "Idle", CLAIM_IDLE },
    { "Running", CLAIM_RUNNING },
    { "Suspended", CLAIM_SUSPENDED },
    { "Vacating", CLAIM_VACATING },
    { "Killing", CLAIM_KILLING },
	{ "NULL", 0 }
};


const struct Translation HookTypeTranslation[] = {
	{ "FETCH_WORK", HOOK_FETCH_WORK },
	{ "REPLY_FETCH", HOOK_REPLY_FETCH },
	{ "EVICT_CLAIM", HOOK_EVICT_CLAIM },
	{ "PREPARE_JOB", HOOK_PREPARE_JOB },
	{ "UPDATE_JOB_INFO", HOOK_UPDATE_JOB_INFO },
	{ "JOB_EXIT", HOOK_JOB_EXIT },
	{ "EVICT_JOB", HOOK_EVICT_JOB },
	{ "TRANSLATE_JOB", HOOK_TRANSLATE_JOB },
	{ "JOB_CLEANUP", HOOK_JOB_CLEANUP },
	{ "JOB_FINALIZE", HOOK_JOB_FINALIZE },
};


const struct Translation VacateTypeTranslation[] = {
	{ "GRACEFUL", VACATE_GRACEFUL },
	{ "FAST", VACATE_FAST },
	{ "NULL", 0 }
};


// duplicates are allowed in the table.  however, the first entry we
// find in the table that matches what we're looking for is used.
const struct Translation ShouldTransferFilesTranslation[] = {
	{ "YES", STF_YES },
	{ "TRUE", STF_YES },
	{ "ALWAYS", STF_YES },
	{ "NO", STF_NO },
	{ "FALSE", STF_NO },
	{ "NEVER", STF_NO },
	{ "IF_NEEDED", STF_IF_NEEDED },
	{ "AUTO", STF_IF_NEEDED },
	{ "NULL", 0 }
};


const struct Translation FileTransferOutputTranslation[] = {
	{ "ON_EXIT_OR_EVICT", FTO_ON_EXIT_OR_EVICT },
	{ "ALWAYS", FTO_ON_EXIT_OR_EVICT },
	{ "ON_EXIT", FTO_ON_EXIT },
	{ "ONEXIT", FTO_ON_EXIT },
	{ "NONE", FTO_NONE },
	{ "NULL", FTO_NONE },
	{ "ON_SUCCESS", FTO_ON_SUCCESS },
	{ "ONSUCCESS", FTO_ON_SUCCESS }
};


const struct Translation JobActionTranslation[] = {
	{ "Hold", JA_HOLD_JOBS },
	{ "Release", JA_RELEASE_JOBS },
	{ "Remove", JA_REMOVE_JOBS },
	{ "Remove-Force", JA_REMOVE_X_JOBS },
	{ "Vacate", JA_VACATE_JOBS },
	{ "Vacate-Fast", JA_VACATE_FAST_JOBS },
	{ "ClearDirtyAttrs", JA_CLEAR_DIRTY_JOB_ATTRS },
	{ "Suspend", JA_SUSPEND_JOBS },
	{ "Continue", JA_CONTINUE_JOBS },
	{ "NULL", 0 }
};


const struct Translation CronAutoPublishTranslation[] = {
	{ "Never", CAP_NEVER },
	{ "False", CAP_NEVER },
	{ "Always", CAP_ALWAYS },
	{ "IfChanged", CAP_IF_CHANGED },
	{ "If_Changed", CAP_IF_CHANGED },
	{ "IfDifferent", CAP_IF_CHANGED },
	{ "If_Different", CAP_IF_CHANGED },
	{ "NULL", 0 }
};


const char*
getClaimTypeString( ClaimType type )
{
	return getNameFromNum( (int)type, ClaimTypeTranslation );
}


ClaimType
getClaimTypeNum( const char* str )
{
	return (ClaimType)getNumFromName( str, ClaimTypeTranslation );
}


const char*
getClaimStateString( ClaimState type )
{
	return getNameFromNum( (int)type, ClaimStateTranslation );
}


ClaimState
getClaimStateNum( const char* str )
{
	return (ClaimState)getNumFromName( str, ClaimStateTranslation );
}


const char*
getHookTypeString( HookType type )
{
	return getNameFromNum( (int)type, HookTypeTranslation );
}


HookType
getHookTypeNum( const char* str )
{
	return (HookType)getNumFromName( str, HookTypeTranslation );
}


const char*
getVacateTypeString( VacateType type )
{
	return getNameFromNum( (int)type, VacateTypeTranslation );
}


VacateType
getVacateTypeNum( const char* str )
{
	return (VacateType)getNumFromName( str, VacateTypeTranslation );
}


const char*
getShouldTransferFilesString( ShouldTransferFiles_t type )
{
	return getNameFromNum( (int)type, ShouldTransferFilesTranslation );
}


ShouldTransferFiles_t
getShouldTransferFilesNum( const char* str )
{
	return (ShouldTransferFiles_t)
		getNumFromName( str, ShouldTransferFilesTranslation ); 
}


const char*
getFileTransferOutputString( FileTransferOutput_t type )
{
	return getNameFromNum( (int)type, FileTransferOutputTranslation );
}


FileTransferOutput_t
getFileTransferOutputNum( const char* str )
{
	return (FileTransferOutput_t)
		getNumFromName( str, FileTransferOutputTranslation ); 
}


const char*
getJobActionString( JobAction action )
{
	return getNameFromNum( (int)action, JobActionTranslation );
}


JobAction
getJobActionNum( const char* str )
{
	return (JobAction)getNumFromName( str, JobActionTranslation );
}


const char*
getCronAutoPublishString( CronAutoPublish_t val )
{
	return getNameFromNum( (int)val, CronAutoPublishTranslation );
}


CronAutoPublish_t
getCronAutoPublishNum( const char* str )
{
	return (CronAutoPublish_t)getNumFromName(str, CronAutoPublishTranslation);
}


