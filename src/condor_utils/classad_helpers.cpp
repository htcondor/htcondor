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
#include "basename.h"
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
   for (int ii = 0; ii < str.length(); ++ii) {
      char ch = str[ii];
      if (ch == '_' || (ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z'))
         continue;
      str.setAt(ii,chReplace);
      }

   // if compact, convert runs of chReplace with a single instance,
   // unless chReplace is ' ', then remove them entirely.
   if (compact) {
      if (chReplace == ' ')
         str.replaceString(" ","");
      else {
         MyString tmp; tmp += chReplace; tmp += chReplace;
         str.replaceString(tmp.c_str(), tmp.c_str()+1);
      }
   }
   str.trim();
   return str.length();
}

int cleanStringForUseAsAttr(std::string &str, char chReplace/*=0*/, bool compact/*=true*/)
{
	MyString my_str = str;
	int rc = cleanStringForUseAsAttr(my_str, chReplace, compact);
	str = my_str.c_str();
	return rc;
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
	std::string name;
	int signal;

	if ( ad->LookupInteger( attr_name, signal ) ) {
		return signal;
	} else if ( ad->LookupString( attr_name, name ) ) {
		return signalNumber( name.c_str() );
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

int
findCheckpointSig( ClassAd* ad )
{
	return findSignal( ad, ATTR_CHECKPOINT_SIG );
}

bool
printExitString( ClassAd* ad, int exit_reason, std::string &str )
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
		str += std::to_string( exit_reason );
		return true;
		break;

	} // switch()

		// if we're here, it's because we hit JOB_EXITED or
		// JOB_COREDUMPED in the above switch.  now we've got to pull
		// a bunch of info out of the ClassAd to finish our task...

	int int_value;
	bool bool_value;
	bool exited_by_signal = false;
	int exit_value = -1;

		// first grab everything from the ad we must have for this to
		// work at all...
	if( ad->LookupBool(ATTR_ON_EXIT_BY_SIGNAL, bool_value) ) {
		exited_by_signal = bool_value;
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
				str += std::to_string( exit_value );
			}
		}
	} else {
		str += "exited normally with status ";
		str += std::to_string( exit_value );
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
	job_ad->Assign( ATTR_JOB_REMOTE_USER_CPU, 0.0 );
	job_ad->Assign( ATTR_JOB_REMOTE_SYS_CPU, 0.0 );

		// This is a magic cookie, see how condor_submit sets it
	job_ad->Assign( ATTR_CORE_SIZE, -1 );

		// Are these ones really necessary?
	job_ad->Assign( ATTR_JOB_EXIT_STATUS, 0 );
	job_ad->Assign( ATTR_ON_EXIT_BY_SIGNAL, false );

	job_ad->Assign( ATTR_NUM_CKPTS, 0 );
	job_ad->Assign( ATTR_NUM_JOB_STARTS, 0 );
	job_ad->Assign( ATTR_NUM_JOB_COMPLETIONS, 0 );
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
#ifdef NO_DEPRECATE_NICE_USER
	job_ad->Assign( ATTR_NICE_USER, false );
#endif

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

// tokenize the input string, and insert tokens into the attrs set
bool add_attrs_from_string_tokens(classad::References & attrs, const char * str, const char * delims=NULL)
{
	if (str && str[0]) {
		StringTokenIterator it(str, 40, delims ? delims : ", \t\r\n");
		const std::string * attr;
		while ((attr = it.next_string())) { attrs.insert(*attr); }
		return true;
	}
	return false;
}

void add_attrs_from_StringList(const StringList & list, classad::References & attrs)
{
	StringList &constList = const_cast<StringList &>(list);
	for (const char * p = constList.first(); p != NULL; p = constList.next()) {
		attrs.insert(p);
	}
}

// print attributes to a std::string, returning the result as a const char *
const char *print_attrs(std::string &out, bool append, const classad::References & attrs, const char * delim)
{
	if ( ! append) { out.clear(); }
	size_t start = out.size();

	int cchAttr = 24; // assume 24ish characters per attribute.
	if (delim) cchAttr += strlen(delim);
	out.reserve(out.size() + (attrs.size()*cchAttr));

	for (classad::References::const_iterator it = attrs.begin(); it != attrs.end(); ++it) {
		if (delim && (out.size() > start)) out += delim;
		out += *it;
	}
	return out.c_str();
}

// copy attrs into stringlist, returns true if list was modified
// if append is false, list is cleared first.
// if check_exist is true, items are only added if the are not already in the list. comparison is case-insensitive.
bool initStringListFromAttrs(StringList & list, bool append, const classad::References & attrs, bool check_exist /*=false*/)
{
	bool modified = false;
	if ( ! append) {
		if ( ! list.isEmpty()) {
			modified = true;
			list.clearAll();
		}
		check_exist = false; // no need to do this if we cleared the list
	}
	for (classad::References::const_iterator it = attrs.begin(); it != attrs.end(); ++it) {
		if (check_exist && list.contains_anycase(it->c_str())) {
			continue;
		}
		list.append(it->c_str());
		modified = true;
	}
	return modified;
}



