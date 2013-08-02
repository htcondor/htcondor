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

 
/*
  This file holds utility functions that rely on ClassAds.
*/

#include "condor_common.h"
#include "condor_classad.h"
#include "condor_attributes.h"
#include "sig_name.h"
#include "exit.h"
#include "enum_utils.h"
#include "condor_adtypes.h"
#include "condor_config.h"
#include "filename_tools.h"
#include "proc.h"
#include "condor_version.h"

// Remove/replace characters from the string so it can be used as an attribute name
// it changes the string that is passed to it.  first leading an trailing spaces
// are removed, then Characters that are invalid in compatible classads 
// (basically anthing but [a-zA-Z0-9_]) is replaced with chReplace.
// if chReplace is 0, then invalid characters are removed. 
// if compact is true, then multiple consecutive runs of chReplace
// are changed to a single instance.
// return value is the length of the resulting string.
//
int cleanStringForUseAsAttr(MyString &str, char chReplace/*=0*/, bool compact/*=true*/)
{
   // have 0 mean 'remove' since we can't actually use it as a replacement char
   // we'll actually implement it by replacing invalid chars with spaces,
   // and then compacting to remove all of the spaces.
   if (0 == chReplace) {
      chReplace = ' ';
      compact = true;
   }

   // trim the input and replace invalid chars with chReplace
   str.trim();
   for (int ii = 0; ii < str.Length(); ++ii) {
      char ch = str[ii];
      if (ch == '_' || (ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z'))
         continue;
      str.setChar(ii,chReplace);
      }

   // if compact, convert runs of chReplace with a single instance,
   // unless chReplace is ' ', then remove them entirely.
   if (compact) {
      if (chReplace == ' ')
         str.replaceString(" ","");
      else {
         MyString tmp; tmp += chReplace; tmp += chReplace;
         str.replaceString(tmp.Value(), tmp.Value()+1);
      }
   }
   str.trim();
   return str.Length();
}

/*
  This method is static to this file and shouldn't be used directly.
  it just does the actual work for findSoftKillSig() and
  findRmKillSig().  In here, we lookup the given attribute name, which
  specifies a signal we want to use.  The signal could either be
  stored as an int, or as a string.  if it's a string, we want to
  translate it into the appropriate signal number for this platform. 
*/
static int
findSignal( ClassAd* ad, const char* attr_name )
{
	if( ! ad ) {
		return -1;
	}
	MyString name;
	int signal;

	if ( ad->LookupInteger( attr_name, signal ) ) {
		return signal;
	} else if ( ad->LookupString( attr_name, name ) ) {
		return signalNumber( name.Value() );
	} else {
		return -1;
	}
}


int
findSoftKillSig( ClassAd* ad )
{
	return findSignal( ad, ATTR_KILL_SIG );
}


int
findRmKillSig( ClassAd* ad )
{
	return findSignal( ad, ATTR_REMOVE_KILL_SIG );
}


int
findHoldKillSig( ClassAd* ad )
{
	return findSignal( ad, ATTR_HOLD_KILL_SIG );
}


bool
printExitString( ClassAd* ad, int exit_reason, MyString &str )
{
		// first handle a bunch of cases that don't really need any
		// info from the ClassAd at all.

	switch ( exit_reason ) {

	case JOB_KILLED:
		str += "was removed by the user";
		return true;
		break;

	case JOB_NOT_CKPTED:
		str += "was evicted by condor, without a checkpoint";
		return true;
		break;

	case JOB_NOT_STARTED:
		str += "was never started";
		return true;
		break;

	case JOB_SHADOW_USAGE:
		str += "had incorrect arguments to the condor_shadow ";
		str += "(internal error)";
		return true;
		break;

	case JOB_COREDUMPED:
	case JOB_EXITED:
			// these two need more work...  we handle both below
		break;

	default:
		str += "has a strange exit reason code of ";
		str += exit_reason;
		return true;
		break;

	} // switch()

		// if we're here, it's because we hit JOB_EXITED or
		// JOB_COREDUMPED in the above switch.  now we've got to pull
		// a bunch of info out of the ClassAd to finish our task...

	int int_value;
	bool exited_by_signal = false;
	int exit_value = -1;

		// first grab everything from the ad we must have for this to
		// work at all...
	if( ad->LookupBool(ATTR_ON_EXIT_BY_SIGNAL, int_value) ) {
		if( int_value ) {
			exited_by_signal = true;
		} else {
			exited_by_signal = false;
		}
	} else {
		dprintf( D_ALWAYS, "ERROR in printExitString: %s not found in ad\n",
				 ATTR_ON_EXIT_BY_SIGNAL );
		return false;
	}

	if( exited_by_signal ) {
		if( ad->LookupInteger(ATTR_ON_EXIT_SIGNAL, int_value) ) {
			exit_value = int_value;
		} else {
			dprintf( D_ALWAYS, "ERROR in printExitString: %s is true but "
					 "%s not found in ad\n", ATTR_ON_EXIT_BY_SIGNAL,
					 ATTR_ON_EXIT_SIGNAL);
			return false;
		}
	} else { 
		if( ad->LookupInteger(ATTR_ON_EXIT_CODE, int_value) ) {
			exit_value = int_value;
		} else {
			dprintf( D_ALWAYS, "ERROR in printExitString: %s is false but "
					 "%s not found in ad\n", ATTR_ON_EXIT_BY_SIGNAL,
					 ATTR_ON_EXIT_CODE);
			return false;
		}
	}

		// now we can grab all the optional stuff that might help...
	char* ename = NULL;
	int got_exception = ad->LookupString(ATTR_EXCEPTION_NAME, &ename); 
	char* reason_str = NULL;
	ad->LookupString( ATTR_EXIT_REASON, &reason_str );

		// finally, construct the right string
	if( exited_by_signal ) {
		if( got_exception ) {
			str += "died with exception ";
			str += ename;
		} else {
			if( reason_str ) {
				str += reason_str;
			} else {
				str += "died on signal ";
				str += exit_value;
			}
		}
	} else {
		str += "exited normally with status ";
		str += exit_value;
	}

	if( ename ) {
		free( ename );
	}
	if( reason_str ) {
		free( reason_str );
	}		
	return true;
}

/* Utility function to create a generic job ad. The caller can then fill
 * in the relevant details.
 */
ClassAd *CreateJobAd( const char *owner, int universe, const char *cmd )
{
	ClassAd *job_ad = new ClassAd();

	SetMyTypeName(*job_ad, JOB_ADTYPE);
	SetTargetTypeName(*job_ad, STARTD_ADTYPE);

	if ( owner ) {
		job_ad->Assign( ATTR_OWNER, owner );
	} else {
		job_ad->AssignExpr( ATTR_OWNER, "Undefined" );
	}
	job_ad->Assign( ATTR_JOB_UNIVERSE, universe );
	job_ad->Assign( ATTR_JOB_CMD, cmd );

	job_ad->Assign( ATTR_Q_DATE, (int)time(NULL) );
	job_ad->Assign( ATTR_COMPLETION_DATE, 0 );

	job_ad->Assign( ATTR_JOB_REMOTE_WALL_CLOCK, 0.0 );
	job_ad->Assign( ATTR_JOB_LOCAL_USER_CPU, 0.0 );
	job_ad->Assign( ATTR_JOB_LOCAL_SYS_CPU, 0.0 );
	job_ad->Assign( ATTR_JOB_REMOTE_USER_CPU, 0.0 );
	job_ad->Assign( ATTR_JOB_REMOTE_SYS_CPU, 0.0 );

		// This is a magic cookie, see how condor_submit sets it
	job_ad->Assign( ATTR_CORE_SIZE, -1 );

		// Are these ones really necessary?
	job_ad->Assign( ATTR_JOB_EXIT_STATUS, 0 );
	job_ad->Assign( ATTR_ON_EXIT_BY_SIGNAL, false );

	job_ad->Assign( ATTR_NUM_CKPTS, 0 );
	job_ad->Assign( ATTR_NUM_JOB_STARTS, 0 );
	job_ad->Assign( ATTR_NUM_RESTARTS, 0 );
	job_ad->Assign( ATTR_NUM_SYSTEM_HOLDS, 0 );
	job_ad->Assign( ATTR_JOB_COMMITTED_TIME, 0 );
	job_ad->Assign( ATTR_CUMULATIVE_SLOT_TIME, 0 );
	job_ad->Assign( ATTR_COMMITTED_SLOT_TIME, 0 );
	job_ad->Assign( ATTR_TOTAL_SUSPENSIONS, 0 );
	job_ad->Assign( ATTR_LAST_SUSPENSION_TIME, 0 );
	job_ad->Assign( ATTR_CUMULATIVE_SUSPENSION_TIME, 0 );
	job_ad->Assign( ATTR_COMMITTED_SUSPENSION_TIME, 0 );

	job_ad->Assign( ATTR_JOB_ROOT_DIR, "/" );

	job_ad->Assign( ATTR_MIN_HOSTS, 1 );
	job_ad->Assign( ATTR_MAX_HOSTS, 1 );
	job_ad->Assign( ATTR_CURRENT_HOSTS, 0 );

	job_ad->Assign( ATTR_WANT_REMOTE_SYSCALLS, false );
	job_ad->Assign( ATTR_WANT_CHECKPOINT, false );
	job_ad->Assign( ATTR_WANT_REMOTE_IO, true );

	job_ad->Assign( ATTR_JOB_STATUS, IDLE );
	job_ad->Assign( ATTR_ENTERED_CURRENT_STATUS, (int)time(NULL) );

	job_ad->Assign( ATTR_JOB_PRIO, 0 );
	job_ad->Assign( ATTR_NICE_USER, false );

	job_ad->Assign( ATTR_JOB_NOTIFICATION, NOTIFY_NEVER );

	

	job_ad->Assign( ATTR_IMAGE_SIZE, 100 );

	job_ad->Assign( ATTR_JOB_IWD, "/tmp" );
	job_ad->Assign( ATTR_JOB_INPUT, NULL_FILE );
	job_ad->Assign( ATTR_JOB_OUTPUT, NULL_FILE );
	job_ad->Assign( ATTR_JOB_ERROR, NULL_FILE );

		// Not sure what to do with these. If stdin/out/err is unset in the
		// submit file, condor_submit sets In/Out/Err to the NULL_FILE and
		// these transfer attributes to false. Otherwise, it leaves the
		// transfer attributes unset (which is treated as true). If we
		// explicitly set these to false here, our caller needs to reset
		// them to true if it changes In/Out/Err and wants the default
		// behavior of transfering them. This will probably be a common
		// oversite. Leaving them unset should be safe if our caller doesn't
		// change In/Out/Err.
	//job_ad->Assign( ATTR_TRANSFER_INPUT, false );
	//job_ad->Assign( ATTR_TRANSFER_OUTPUT, false );
	//job_ad->Assign( ATTR_TRANSFER_ERROR, false );
	//job_ad->Assign( ATTR_TRANSFER_EXECUTABLE, false );

	job_ad->Assign( ATTR_BUFFER_SIZE, 512*1024 );
	job_ad->Assign( ATTR_BUFFER_BLOCK_SIZE, 32*1024 );

	job_ad->Assign( ATTR_SHOULD_TRANSFER_FILES,
					getShouldTransferFilesString( STF_YES ) );
	job_ad->Assign( ATTR_WHEN_TO_TRANSFER_OUTPUT,
					getFileTransferOutputString( FTO_ON_EXIT ) );

	job_ad->Assign( ATTR_REQUIREMENTS, true );

	job_ad->Assign( ATTR_PERIODIC_HOLD_CHECK, false );
	job_ad->Assign( ATTR_PERIODIC_REMOVE_CHECK, false );
	job_ad->Assign( ATTR_PERIODIC_RELEASE_CHECK, false );

	job_ad->Assign( ATTR_ON_EXIT_HOLD_CHECK, false );
	job_ad->Assign( ATTR_ON_EXIT_REMOVE_CHECK, true );

	job_ad->Assign( ATTR_JOB_ARGUMENTS1, "" );

	job_ad->Assign( ATTR_JOB_LEAVE_IN_QUEUE, false );

	job_ad->AssignExpr( ATTR_REQUEST_MEMORY, "ifthenelse(MemoryUsage isnt undefined,MemoryUsage,( ImageSize + 1023 ) / 1024)" );
	job_ad->AssignExpr( ATTR_REQUEST_DISK, "DiskUsage" );
	job_ad->Assign( ATTR_DISK_USAGE, 1 );
	job_ad->Assign( ATTR_REQUEST_CPUS, 1 );

	// Without these, the starter will not automatically remap the stdout/err (look at sha 422735d9)
	// and possibly won't put them in the correct directory.
	job_ad->Assign( ATTR_STREAM_OUTPUT, false );
	job_ad->Assign( ATTR_STREAM_ERROR, false );

	job_ad->Assign( ATTR_VERSION, CondorVersion() );
	job_ad->Assign( ATTR_PLATFORM, CondorPlatform() );

	job_ad->Assign( ATTR_Q_DATE, time(NULL) );

	return job_ad;
}

bool getPathToUserLog(ClassAd *job_ad, MyString &result,
					   const char* ulog_path_attr = ATTR_ULOG_FILE)
{
	bool ret_val = true;
	char *global_log = NULL;

	if ( job_ad == NULL || 
	     job_ad->LookupString(ulog_path_attr,result) == 0 ) 
	{
		// failed to find attribute, check config file
		global_log = param("EVENT_LOG");
		if ( global_log ) {
			// canonicalize to UNIX_NULL_FILE even on Win32
			result = UNIX_NULL_FILE;
		} else {
			ret_val = false;
		}
	}

	if ( global_log ) free(global_log);

	if( ret_val && is_relative_to_cwd(result.Value()) ) {
		MyString iwd;
		if( job_ad && job_ad->LookupString(ATTR_JOB_IWD,iwd) ) {
			iwd += "/";
			iwd += result;
			result = iwd;
		}
	}

	return ret_val;
}
