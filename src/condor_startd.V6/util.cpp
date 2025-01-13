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
#include "startd.h"
#include "directory.h"
#include "dynuser.h"	// used in cleanup_execute_dir() for WinNT
#include "daemon.h"
#include "filesystem_remap.h"
#include "docker-api.h"

// helper method to determine whether the given execute directory
// is root-squashed. this function assumes that the given directory
// is owned and writable by condor, and that our real UID is 0. it
// returns true if we can verify that root squash is NOT in effect,
// false if not (i.e. false is returned if we detemine root squash
// is in effect or we hit an error)
//
static bool
not_root_squashed( char const *exec_path )
{
	std::string test_dir;
	formatstr(test_dir,"%s/.root_squash_test", exec_path);

	if (rmdir(test_dir.c_str()) == -1) {
		if (errno != ENOENT) {
			dprintf(D_FULLDEBUG,
			        "not_root_squashed: rmdir of %s failed: %s\n",
			        test_dir.c_str(),
			        strerror(errno));
			return false;
		}
	}
	priv_state priv = set_root_priv();
	int rv = mkdir(test_dir.c_str(), 0755);
	set_priv(priv);
	if (rv == -1) {
		if (errno == EACCES) {
			dprintf(D_FULLDEBUG,
			        "execute directory %s root-squashed\n",
			        exec_path);
		}
		else {
			dprintf(D_FULLDEBUG,
			        "not_root_squashed: mkdir of %s failed: %s\n",
			        test_dir.c_str(),
			        strerror(errno));
		}
		return false;
	}
	struct stat st;
	if (stat(test_dir.c_str(), &st) == -1) {
		dprintf(D_FULLDEBUG,
		        "not_root_squashed: stat of %s failed: %s\n",
		        test_dir.c_str(),
		        strerror(errno));
		return false;
	}
	if (rmdir(test_dir.c_str()) == -1) {
		dprintf(D_FULLDEBUG,
		        "rmdir of %s failed: %s\n",
		        test_dir.c_str(),
		        strerror(errno));
		return false;
	}

	bool not_squashed = (st.st_uid == 0);
	dprintf(D_FULLDEBUG,
	        "execute directory %s %s root-squashed\n",
	        exec_path,
		not_squashed ? "not" : "");
	return not_squashed;
}

bool
check_execute_dir_perms( char const *exec_path, bool abort_on_error )
{
	struct stat st;
	if (stat(exec_path, &st) < 0) {
		dprintf(D_ERROR, "stat() failed on execute path (%s), errno: %d (%s)\n", exec_path, errno,
				strerror( errno ) );
		if (abort_on_error) {
			EXCEPT("Invalid execute directory: %s", exec_path);
		}
		return false;
	}

	// On Windows, we rely on the installer to set the necessary ACLs
	// for the EXECUTE directory.
#if !defined(WIN32)
	// we want to avoid having our execute directory world-writable
	// if possible. it's possible if the execute directory is owned
	// by condor and either:
	//   - we're not switching UIDs. in this case, job sandbox dirs
	//     will just be owned by the condor UID, so we just need
	//     owner-writability
	//   - there's no root squash on the execute dir (since then we
	//     can do a mkdir as the condor UID then a chown to the job
	//     owner UID)
	//
	mode_t desired_mode = 0;
	if (st.st_uid != get_condor_uid()) {
		dprintf(D_ERROR, "Execute path (%s) owned by uid %d (not user %s as required)\n", exec_path, (int)st.st_uid, get_condor_username());
		if (abort_on_error) {
			EXCEPT("Invalid execute directory: %s", exec_path);
		}
		return false;
	}
	if (!can_switch_ids() || not_root_squashed(exec_path))
	{
		// The starter will create execute dirs as condor and then
		// chown them to the user (if running as root).
		// The directory should be writeable only by condor but
		// accessible by all users.
		desired_mode = 0755;
	}
	else {
		// The starter will create execute dirs as the user.
		// The directory should be world-writable with the sticky bit.
		desired_mode = 01777;
		dprintf(D_ALWAYS,
				"WARNING: %s root-squashed: "
				"requiring world-writability\n",
				exec_path);
	}
	if ((st.st_mode & 07777) != desired_mode) {
		dprintf(D_ERROR, "Execute path (%s) doesn't have permissions 0%o\n", exec_path, (int)desired_mode);
		if (abort_on_error) {
			EXCEPT("Invalid execute directory: %s", exec_path);
		}
		return false;
	}
#endif

	return true;
}

void
check_recovery_file( const char *sandbox_dir, bool abnormal_exit )
{
	FILE *recovery_fp = NULL;
	ClassAd *recovery_ad = NULL;
	if ( sandbox_dir == NULL ) {
		return;
	}

	// the caller might pass the path to a job sandbox dir
	// or a path to the recovery file itself.  so if the passed-in
	// filename does not end in .recovery, append it.
	std::string recovery_file(sandbox_dir);
	if ( ! ends_with(recovery_file, ".recover")) {
		recovery_file += ".recover";
	}

	recovery_fp = safe_fopen_wrapper_follow( recovery_file.c_str(), "r" );
	if ( recovery_fp == NULL ) {
		int err = errno;
		if (errno != ENOENT) {
			dprintf( D_ALWAYS, "check_recovery_file: could not open '%s', error %d : %s\n",
				recovery_file.c_str(), err, strerror(err));
			if (unlink(recovery_file.c_str()) < 0) {
				dprintf( D_FULLDEBUG, "check_recovery_file: Failed to remove file '%s'\n", recovery_file.c_str() );
			}
		} else if (abnormal_exit) {
			dprintf(D_FULLDEBUG, "check_recovery_file: '%s' does not exist\n", recovery_file.c_str());
		}
		return;
	}

	int eof = 0;
	int error = 0;
	int empty = 0;
	recovery_ad = new ClassAd;
	InsertFromFile( recovery_fp, *recovery_ad, "***", eof, error, empty );
	if ( error || empty ) {
		dprintf( D_ALWAYS, "check_recovery_file(%s): read error %d %s\n",
			recovery_file.c_str(), error, empty?"empty file":"");
		fclose( recovery_fp );
		if (unlink(recovery_file.c_str()) < 0) {
			dprintf( D_FULLDEBUG, "check_recovery_file: Failed to remove file '%s'\n", recovery_file.c_str() );
		} 
		return;
	}

	if (abnormal_exit) {
		std::string buf;
		dprintf(D_FULLDEBUG, "check_recovery_file('%s') ad:%s\n",
			recovery_file.c_str(), formatAd(buf, *recovery_ad, "\t"));
	}

	int universe = 0;
	recovery_ad->LookupInteger( ATTR_JOB_UNIVERSE, universe );
	if ( universe == CONDOR_UNIVERSE_VM ) {
		std::string vm_id;
		recovery_ad->LookupString( "JobVMId", vm_id );
		if (vm_id.length() > 0) {
			resmgr->m_vmuniverse_mgr.killVM( vm_id.c_str() );
		}
	}

	// Check if it is a lost Docker container, and remove it if so
	std::string containerId;
	recovery_ad->LookupString("DockerContainerName", containerId);
	if ( !containerId.empty() ) {
		CondorError err;
		dprintf(D_ALWAYS, "Removing orphaned docker container %s\n", containerId.c_str());
		int rval = DockerAPI::rm(containerId, err);
		if (rval == DockerAPI::docker_hung) {
			dprintf(D_ALWAYS, "DockerAPI::rm returned docker_hung. Taking Docker universe offline\n");
			ClassAd update;
			update.Assign( ATTR_HAS_DOCKER, false );
			update.Assign( "DockerOfflineReason", "Docker hung trying to rm an orphaned container" );
			resmgr->updateExtrasClassAd(&update);
		}
	} 

	delete recovery_ad;
	fclose( recovery_fp );
	if (unlink(recovery_file.c_str()) < 0) {
		dprintf( D_FULLDEBUG, "check_recovery_file: Failed to remove file '%s'\n", recovery_file.c_str() );
	}
}

void
cleanup_execute_dirs(const std::string &exec_path)
{
	{ // A for-loop used to be here.
		if (exec_path.empty()) {
			return;
		}
#if defined(WIN32)
		dynuser nobody_login;
		// remove all users matching this prefix
		nobody_login.cleanup_condor_users("condor-run-");

		// get rid of everything in the execute directory
		Directory execute_dir(exec_path.c_str());

		execute_dir.Rewind();
		while ( execute_dir.Next() ) {
			check_recovery_file( execute_dir.GetFullPath(), true );
		}

		execute_dir.Remove_Entire_Directory();
#else
		std::string dirbuf;
		pair_strings_vector root_dirs = root_dir_list();
		for (pair_strings_vector::const_iterator it=root_dirs.begin(); it != root_dirs.end(); ++it) {
			const char * exec_path_full = dirscat(it->second.c_str(), exec_path.c_str(), dirbuf);
			if(exec_path_full) {
				dprintf(D_FULLDEBUG, "Looking at %s\n",exec_path_full);
				Directory execute_dir( exec_path_full, PRIV_ROOT );

				execute_dir.Rewind();
				while ( execute_dir.Next() ) {
					check_recovery_file( execute_dir.GetFullPath(), true );
				}

				execute_dir.Remove_Entire_Directory();
			}
		}
#endif
	}
}

bool retry_cleanup_user_account(const std::string & name, int /*options*/, int & err)
{
	err = 0;
	if (name.empty()) {
		// name is empty, return 'sure, that user is gone'
		return true;
	}

	// TODO: write this.
	EXCEPT("retry_cleanup_user_account is not implemented");

	return false;
}


bool retry_cleanup_execute_dir(const std::string & path, int /*options*/, int & err)
{
	err = 0;
	if (path.empty()) {
		// path is empty, return 'sure, I deleted *everything*...'
		return true;
	}

	StatInfo si( path.c_str() );
	if (si.Error() == SINoFile) {
		// it's gone now. return true
		err = EALREADY;
		return true;
	}

	Directory dir( path.c_str() );
	bool success = dir.Remove_Full_Path(path.c_str());
	if ( ! success) {
		// unfortunately Remove_Full_path doesn't tell us why we failed, so assume it's a permissions issue... <sigh>
		err = EPERM;
	}
	return success;
}

void
cleanup_execute_dir(int pid, const char *exec_path, const char * lv_name, bool remove_exec_path, bool abnormal_exit, bool lv_encrypted)
{
	ASSERT( pid );

#ifdef WIN32
	std::string buf;
	dynuser nobody_login;

	// note: reusing nobody accounts is the default on Windows, so this code seldom executes...
	if ( nobody_login.reuse_accounts() == false ) {
		// before removing subdir, remove any nobody-user account associated
		// with this starter pid.  this account might have been left around
		// if the starter did not clean up completely.
		//sprintf(buf,"condor-run-dir_%d",pid);
		formatstr(buf,"condor-run-%d",pid);
		if ( nobody_login.deleteuser(buf.c_str()) ) {
			dprintf(D_FULLDEBUG,"Removed account %s left by starter\n",buf.c_str());
		}
	}

	// now remove the subdirectory.  NOTE: we only remove the 
	// subdirectory _after_ removing the nobody account, because the
	// existence of the subdirectory persistantly tells us that the
	// account may still exist [in case the startd blows up as well].
#endif


	// We're trying to delete a specific subdirectory, either
	// b/c a starter just exited and we might need to clean up
	// after it, or because we're in a recursive call.
	std::string	pid_dir, dirbuf;
	formatstr(pid_dir, "dir_%d", pid );
	const char * pid_dir_path = dirscat(exec_path, pid_dir.c_str(), dirbuf);

	check_recovery_file(pid_dir_path, abnormal_exit);

	// TODO: move this retry loop to a self-draining queue or similar
	// we *should* only need to do this when the starter has an abnormal exit
	// and we normally poll the LVM for a status of all LVs, so we could detect
	// leaked LVs there rather than here.
	// NOTE: The Starter can currently Fail to cleanup an LV and exit normally
	auto * volman = resmgr->getVolumeManager();
	if (lv_name && volman && volman->is_enabled()) {
		// Attempt LV cleanup
		CondorError err;
		// Attempt LV cleanup n times to prevent race condition between
		// killing of family processes and LV cleanup causing failure
		int max_attempts = 5;
		for (int attempt=1; attempt<=max_attempts; attempt++) {
			// Attempt a cleanup
			dprintf(D_FULLDEBUG, "LV cleanup attempt %d/%d\n", attempt, max_attempts);
			int ret = volman->CleanupLV(lv_name, err, lv_encrypted);
			if (ret) {
				if (!abnormal_exit && ret == 2) {
					dprintf(D_FULLDEBUG, "Skipping remaining attempts for %s (%s|%d): %s\n",
					        lv_name, abnormal_exit ? "T" : "F", ret, ret < 0 ? err.getFullText().c_str() : "");
					break; // If starter exited normally and we failed to find LV assume it is cleaned up
				} else if (attempt == max_attempts){
					// We have failed and this was the last attempt so output error message
					dprintf(D_ALWAYS, "Failed to cleanup LV %s: %s", lv_name, err.getFullText().c_str());
				}
				err.clear();
			} else {
				dprintf(D_FULLDEBUG, "LVM cleanup succesful.\n");
				break;
			}
			sleep(1);
		}
	}

#ifdef WIN32

	int err = 0;
	if ( ! retry_cleanup_execute_dir(pid_dir_path, 0, err)) {
		dprintf(D_ALWAYS, "Delete of execute directory '%s' failed. will try again later\n", pid_dir_path);
		add_exec_dir_cleanup_reminder(pid_dir_path, 0);
	}

#else /* UNIX */

	// Instantiate a directory object pointing at the execute directory
	pair_strings_vector root_dirs = root_dir_list();
	for (pair_strings_vector::const_iterator it=root_dirs.begin(); it != root_dirs.end(); ++it) {
		const char * exec_path_full = dirscat(it->second.c_str(), exec_path, dirbuf);

		Directory execute_dir( exec_path_full, PRIV_ROOT );

		if (remove_exec_path) {
			// Remove entire subdirectory; used to remove
			// an encrypted execute directory
			execute_dir.Remove_Full_Path(exec_path_full);
		} else {
			// Look for specific pid_dir subdir
			if ( execute_dir.Find_Named_Entry( pid_dir.c_str() ) ) {
				// Remove the execute directory
				execute_dir.Remove_Current_File();
			}
		}
	}
#endif  /* UNIX */
}

extern void register_cleanup_reminder_timer();
extern int cleanup_reminder_timer_interval;

void add_exec_dir_cleanup_reminder(const std::string & dir, int opts)
{
	// a timer interval of 0 or negative will disable cleanup reminders
	if (cleanup_reminder_timer_interval <= 0)
		return;
	CleanupReminder rd(dir, CleanupReminder::category::exec_dir, opts);
	if (cleanup_reminders.find(rd) == cleanup_reminders.end()) {
		dprintf(D_FULLDEBUG, "Adding cleanup reminder for exec_dir %s\n", dir.c_str());
		cleanup_reminders[rd] = 0;
		register_cleanup_reminder_timer();
	}
}

void add_account_cleanup_reminder(const std::string & name)
{
	// a timer interval of 0 or negative will disable cleanup reminders
	if (cleanup_reminder_timer_interval <= 0)
		return;
	CleanupReminder rd(name, CleanupReminder::category::account);
	if (cleanup_reminders.find(rd) == cleanup_reminders.end()) {
		dprintf(D_FULLDEBUG, "Adding cleanup reminder for account %s\n", name.c_str());
		cleanup_reminders[rd] = 0;
		register_cleanup_reminder_timer();
	}
}

bool
reply( Stream* s, int cmd )
{
	s->encode();
	if( !s->code( cmd ) || !s->end_of_message() ) {
		return false;
	} else {
		return true;
	}
}


bool
refuse( Stream* s )
{
	s->end_of_message();
	s->encode();
	if( !s->put(NOT_OK) ) {
		return false;
	} 
	if( !s->end_of_message() ) {
		return false;
	}
	return true;
}


bool
caInsert( ClassAd* target, ClassAd* source, const char* attr,
		  const char* prefix )
{
	ExprTree* tree;

	if( !attr ) {
		EXCEPT( "caInsert called with NULL attribute" );
	}
	if( !target || !source ) {
		dprintf(D_ALWAYS | D_BACKTRACE, "caInsert called with NULL classad\n");
		EXCEPT( "caInsert called with NULL classad" );
	}

	std::string new_attr;
	if( prefix ) {
		new_attr = prefix;
	}
	new_attr += attr;

	tree = source->LookupExpr( attr );
	if( !tree ) {
		target->Delete(new_attr);
		return false;
	}
	tree = tree->Copy();
	if ( !target->Insert(new_attr, tree) ) {
		dprintf( D_ALWAYS, "caInsert: Can't insert %s into target classad.\n", attr );
		delete tree;
		return false;
	}
	return true;
}

bool caRevertToParent(ClassAd* target, const char * attr)
{
	if( !attr ) {
		EXCEPT( "caRevertToParent called with NULL attribute" );
	}
	if( !target ) {
		dprintf(D_ALWAYS | D_BACKTRACE, "caRevertToParent called with NULL classad\n");
		EXCEPT( "caRevertToParent called with NULL classad" );
	}

	ClassAd * parent = target->GetChainedParentAd();
	if ( ! parent) {
		dprintf(D_ALWAYS | D_BACKTRACE, "caRevertToParent called with parentless classad\n");
	}

	target->Unchain();
	ExprTree * tree = target->Remove(attr);
	target->ChainToAd(parent);
	delete tree;
	return tree != NULL;
}

void caDeleteThruParent(ClassAd* target, const char * attr, const char * prefix)
{
	if( !attr ) {
		EXCEPT( "caDeleteThruParent called with NULL attribute" );
	}
	if( !target ) {
		dprintf(D_ALWAYS | D_BACKTRACE, "caDeleteThruParent called with NULL classad\n");
		EXCEPT( "caDeleteThruParent called with NULL classad" );
	}

	std::string new_attr;
	if (prefix) {
		new_attr = prefix;
	}
	new_attr += attr;

	// we have to unchain before we delete, otherwise we end up setting attr to undefined, not deleting.
	ClassAd * parent = target->GetChainedParentAd();
	target->Unchain();
	target->Delete(new_attr);
	if (parent) {
		parent->Delete(new_attr);
		target->ChainToAd(parent);
	}
}

/*
  This method takes a pointer to a classad, the name of an attribute
  in the config file, and a flag that says if that attribute isn't
  there, if it should be a fatal error or not.  If the attribute is
  defined, we insert "attribute = value" into the given classad and
  return true.  If the attribute wasn't defined, we return false, and
  if the is_fatal flag is set, we EXCEPT().  If we found the attribute
  but failed to insert it into the ClassAd, there's a syntax error and
  we should EXCEPT(), even if is_fatal is false.  If it's it's in the
  config file, the admin is trying to use it, so if there's a syntax
  error, we should let them know right away. 
  -Derek Wright <wright@cs.wisc.edu> 4/12/00
  -Syntax error checking by Derek on 6/25/03
*/
bool
configInsert( ClassAd* ad, const char* attr, bool is_fatal )
{
	return configInsert( ad, attr, attr, is_fatal );
}


/*
  This version just allows for the name of the thing you're looking
  for in the config file to be different than the classad attribute
  name you want to insert it as.
*/
bool
configInsert( ClassAd* ad, const char* param_name, 
			  const char* attr, bool is_fatal ) 
{
	char* val = param( param_name );
	if( ! val ) {
		if( is_fatal ) {
			EXCEPT( "Required attribute \"%s\" is not defined", attr );
		}
		return false;
	}

	if ( ! ad->AssignExpr( attr, val ) ) {
		EXCEPT( "Syntax error in %s expression: '%s'", attr, val );
	}

	free( val );
	return true;
}


/* 
   This function reads of a ClaimId string and eom from the
   given stream.  It looks up that ClaimId in the resmgr to find
   the corresponding Resource*.  If such a Resource is found, we
   return the pointer to it, otherwise, we return NULL.  */
Resource*
stream_to_rip( Stream* stream )
{
	char* id = NULL;
	Resource* rip;

	stream->decode();
	if( ! stream->get_secret(id) ) {
		dprintf( D_ALWAYS, "Can't read ClaimId\n" );
		free( id );
		return NULL;
	}
	if( ! stream->end_of_message() ) {
		dprintf( D_ALWAYS, "Can't read end_of_message\n" );
		free( id );
		return NULL;
	}
	rip = resmgr->get_by_cur_id( id );
	if( !rip ) {
		ClaimIdParser idp( id );
		dprintf( D_ALWAYS, 
				 "Error: can't find resource with ClaimId (%s) -- perhaps this claim was already removed?\n", idp.publicClaimId() );
		free( id );
		return NULL;
	}
	free( id );
	return rip;
}


VacateType
getVacateType( ClassAd* ad )
{
	VacateType vac_t;
	char* vac_t_str = NULL;
	if( ! ad->LookupString(ATTR_VACATE_TYPE, &vac_t_str) ) { 
		return VACATE_GRACEFUL;
	}
	vac_t = getVacateTypeNum( vac_t_str );
	free( vac_t_str );
	return vac_t;
}


