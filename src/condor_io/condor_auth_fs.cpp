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


#if !defined(SKIP_AUTHENTICATION) && !defined(WIN32)
#include "condor_auth_fs.h"
#include "condor_config.h"
#include "condor_environ.h"
#include "CondorError.h"
#include "condor_mkstemp.h"
#include "ipv6_hostname.h"

Condor_Auth_FS :: Condor_Auth_FS(ReliSock * sock, int remote)
    : Condor_Auth_Base    ( sock, CAUTH_FILESYSTEM ),
      remote_             ( remote )
{
}

Condor_Auth_FS :: ~Condor_Auth_FS()
{
}

int Condor_Auth_FS::authenticate(const char * /* remoteHost */, CondorError* errstack, bool non_blocking)
{
	int client_result = -1;
	int server_result = -1;
	int fail = -1 == 0;

	if ( mySock_->isClient() ) {
		char *new_dir = NULL;

		// receive the directory name to create
		mySock_->decode();
		if (!mySock_->code( new_dir )) { 
			dprintf(D_SECURITY, "Protocol failure at %s, %d!\n",
				__FUNCTION__, __LINE__);
			return fail; 
		}

		if (!mySock_->end_of_message()) { 
			dprintf(D_SECURITY, "Protocol failure at %s, %d!\n",
				__FUNCTION__, __LINE__);
			if ( new_dir ) {
				free( new_dir );
			}
			return fail; 
		}

		// Until session caching supports clients that present
		// different identities to the same service at different
		// times, we want condor daemons to always authenticate
		// as condor priv, rather than as the current euid.
		// For tools and daemons not started as root, this
		// is a no-op.
		priv_state saved_priv = set_condor_priv();

		// try to create the directory the server sent
		if ( new_dir ) {
			if (*new_dir) {
				// mkdir has just the properties we need here.
				// it will fail if it already exists (like O_EXCL) and
				// can be created with initial proper permissions.
				client_result = mkdir(new_dir, 0700);
				if (client_result == -1) {
					errstack->pushf((remote_?"FS":"FS_REMOTE"), 1000,
							"mkdir(%s, 0700): %s (%i)",
							new_dir, strerror(errno), errno);
				}
			} else {
				// server sends null string if it's mktemp failed
				client_result = -1;  // redundant, but safety first!
				if (remote_) {
					errstack->push("FS_REMOTE", 1001,
							"Server Error, check server log.  "
							"FS_REMOTE_DIR is likely misconfigured.");
				} else {
					errstack->push("FS", 1001,
							"Server Error, check server log.");
				}
			}
		}

		// send over result as a success/failure indicator (-1 == failure)
		mySock_->encode();
		if (!mySock_->code( client_result ) || !mySock_->end_of_message()) {
			dprintf(D_SECURITY, "Protocol failure at %s, %d!\n",
				__FUNCTION__, __LINE__);
			if ( new_dir ) {
				if (*new_dir) {
					rmdir( new_dir );
				}
				free( new_dir );
			}
			set_priv(saved_priv);
			return fail; 
		}

		// now let the server verify that we created the dir correctly
		mySock_->decode();
		if (!mySock_->code( server_result ) || !mySock_->end_of_message()) { 
			dprintf(D_SECURITY, "Protocol failure at %s, %d!\n",
				__FUNCTION__, __LINE__);
			if ( new_dir ) {
				if (*new_dir) {
					rmdir( new_dir );
				}
				free( new_dir );
			}
			set_priv(saved_priv);
			return fail; 
		}

		if (client_result != -1) {
			rmdir( new_dir );
		}
		set_priv(saved_priv);

		dprintf( D_SECURITY, "AUTHENTICATE_FS%s: used dir %s, status: %d\n",
				(remote_?"_REMOTE":""),
				(new_dir ? new_dir : "(null)"), (server_result == 0) );

		if ( new_dir ) {
			free( new_dir );
		}

		// this function returns TRUE on success, FALSE on failure,
		// which is just the opposite of server_result.
		return( server_result == 0 );

	} else {

		// server code
		setRemoteUser( NULL );

		if ( remote_ ) {
			// for FS_REMOTE, we need a good unique filename base, as many
			// machines are likely sharing the same directory
	        pid_t    mypid = 0;
#ifdef WIN32
	        mypid = ::GetCurrentProcessId();
#else
	        mypid = ::getpid();
#endif

			std::string filename;
			char * rendezvous_dir = param("FS_REMOTE_DIR");
			if (rendezvous_dir) {
				filename = rendezvous_dir;
				free(rendezvous_dir);
			} else {
				// misconfiguration.  complain, and then use /tmp
				dprintf (D_ALWAYS, "AUTHENTICATE_FS: FS_REMOTE was used but no FS_REMOTE_DIR defined!\n");
				filename = "/tmp";
			}
			formatstr_cat( filename, "/FS_REMOTE_%s_%d_XXXXXXXXX",
			               get_local_hostname().c_str(), mypid );
			dprintf( D_SECURITY, "FS_REMOTE: client template is %s\n", filename.c_str() );

			int sync_fd;
			char *new_dir = strdup(filename.c_str());
			sync_fd = condor_mkstemp(new_dir);
			m_new_dir = new_dir;
			free(new_dir);
			if( sync_fd < 0 ) {
				// path must be invalid?
				//
				// this means that mktemp set new_dir to the empty string.  we
				// code this across.  old clients will try it and fail, and new
				// clients will know that the server had trouble.  for the
				// error message we print the filename, not new_dir which is
				// empty.
				int en = errno;  // strerror could replace errno!
				errstack->pushf("FS_REMOTE", 1002,
						"condor_mkstemp(%s) failed: %s (%i)",
						filename.c_str(), strerror(en), en);
				m_new_dir = "";			//the other process will expect an
										//empty string on failure
			} else {
				::close( sync_fd );
				unlink( m_new_dir.c_str() );
				dprintf( D_SECURITY, "FS_REMOTE: client filename is %s\n", m_new_dir.c_str() );
			}
		} else {
			std::string filename;
			char * rendezvous_dir = param("FS_LOCAL_DIR");
			if (rendezvous_dir) {
				filename = rendezvous_dir;
				free(rendezvous_dir);
			} else {
				filename = "/tmp";
			}
			filename += "/FS_XXXXXXXXX";
			dprintf( D_SECURITY, "FS: client template is %s\n", filename.c_str() );

			int sync_fd;
			char * new_dir = strdup(filename.c_str());
			sync_fd = condor_mkstemp(new_dir);
			m_new_dir = new_dir;
			free(new_dir);
			if( sync_fd < 0 ) {
				// path must be invalid?
				//
				// this means that mktemp set new_dir to the empty string.  we
				// code this across.  old clients will try it and fail, and new
				// clients will know that the server had trouble.  for the
				// error message we print the filename, not new_dir which is
				// empty.
				int en = errno;  // strerror could replace errno!
				errstack->pushf("FS", 1002,
						"condor_mkstemp(%s) failed: %s (%i)",
						filename.c_str(), strerror(en), en);
				m_new_dir = "";			//the other process will expect an
										//empty string on failure
			} else {
				::close( sync_fd );
				unlink( m_new_dir.c_str() );
				dprintf( D_SECURITY, "FS: client filename is %s\n", m_new_dir.c_str() );
			}			
		}

		// now, send over directory name for client to create
		// man page says string returned by tempnam has been malloc()'d
		// except in the case of error, where we malloc an empty string.
		mySock_->encode();
		if (!mySock_->code( m_new_dir ) || !mySock_->end_of_message()) {
			dprintf(D_SECURITY, "Protocol failure at %s, %d!\n",
				__FUNCTION__, __LINE__);
			return fail;
		}
		return authenticate_continue(errstack, non_blocking);
	}
}

int Condor_Auth_FS::authenticate_continue(CondorError* errstack, bool non_blocking)
{
	bool used_file = false;
	int client_result = -1;
	int server_result = -1;
	int fail = -1 == 0;

	if( non_blocking && !mySock_->readReady() ) {
		return 2;
	}

	// see if the client claims success (it could be lying of course!)
	mySock_->decode();
	if (!mySock_->code( client_result ) || !mySock_->end_of_message()) {
		dprintf(D_SECURITY, "Protocol failure at %s, %d!\n",
			__FUNCTION__, __LINE__);
		return fail;
	}

	// we're going to respond to the client with success or failure
	mySock_->encode();

	// assume failure.  i am glass-half-empty man.
	server_result = -1;

	// but first, we need to verify that everything is correct
	if ( client_result != -1  && m_new_dir.size() && m_new_dir[0]) {

		if (remote_) {
			// now, if this is a remote filesystem, it is possibly NFS.
			// before we stat the file, we need to do something that causes the
			// NFS client to sync to the NFS server.  fsync() does not do this.
			// in practice, creating a file or directory should force the NFS
			// client to sync in order to avoid race conditions.
			std::string filename_template = "/tmp";

			char * rendezvous_dir = param("FS_REMOTE_DIR");
			if (rendezvous_dir) {
				filename_template = rendezvous_dir;
				free(rendezvous_dir);
			}

			pid_t    mypid = 0;
#ifdef WIN32
			mypid = ::GetCurrentProcessId();
#else
			mypid = ::getpid();
#endif

			// construct the template. mkstemp modifies the string you pass
			// in so we create a dup of it just in case MyString does
			// anything funny or uses string spaces, etc.
			formatstr_cat( filename_template, "/FS_REMOTE_%s_%d_XXXXXX",
			               get_local_hostname().c_str(), mypid );
			char* filename_inout = strdup(filename_template.c_str());

			dprintf( D_SECURITY, "FS_REMOTE: sync filename is %s\n", filename_inout );

			int sync_fd = condor_mkstemp(filename_inout);
			if (sync_fd >= 0) {
				::close(sync_fd);
				unlink( filename_inout );
			} else {
				// we could have an else that fails here, but we may as well still
				// check for the file -- this was just an attempt to make NFS sync.
				// if the file still isn't there, we'll fail anyways.
				dprintf( D_ALWAYS, "FS_REMOTE: warning, failed to make temp file %s\n", filename_inout );
			}

			free (filename_inout);

			// at this point we should have created and deleted a unique file
			// from the server side, forcing the nfs client to flush everything
			// and sync with the server.  now, we can finally stat the file.
		}

		//check dir to ensure that claimant is owner
		struct stat stat_buf;

		// verify the dir's attributes by stat()ing the dir
		if ( lstat( m_new_dir.c_str(), &stat_buf ) < 0 ) {
			server_result = -1;  
			errstack->pushf((remote_?"FS_REMOTE":"FS"), 1004,
					"Unable to lstat(%s)", m_new_dir.c_str());
		} else {
			// Authentication should fail if a) owner match fails, or b) the
			// directory is either a hard or soft link (no links allowed
			// because they could spoof the owner match).  -Todd 3/98
			//
			// also, it must now be a directory instead of a file, due to
			// hardlink trickery where you could in fact create a hard link
			// (with count 1!) as another user with the supplied filename.
			// however, normal users cannot create hardlinks to
			// directories, only root.  -Zach 8/06

			// The test of link count is historical.  In btrfs,
			// directories have link count 1, unlike other
			// filesystems in which the link count is 2.  We do
			// not believe there is any reason to care about the
			// link count, but since it was already required to be
			// 2, and we want to make it work on btrfs, we are
			// making a conservative change in 7.6.5 that allows
			// the link count to be 1 or 2.  -Dan 11/11

			// The CREATE_LOCKS_ON_LOCAL_DISK code relies on
			// creating directories with mode=0777 that may be
			// owned by any user who happens to need a lock. This
			// is especially true for users with running
			// jobs. Because local lock code uses a tree of these
			// directories, it is possibly for a malicious user to
			// satify FS authentication requirements by renamed a
			// target user's lock directory into /tmp. To avoid
			// this attack, the server side must also fail
			// authentication for any directories whose mode is
			// not 0700. -matt July 2012

			// assume it is wrong until we see otherwise
			bool stat_is_okay = false;

			if ((stat_buf.st_nlink == 1 || stat_buf.st_nlink == 2) &&
				(!S_ISLNK(stat_buf.st_mode)) && // check for soft link
				(S_ISDIR(stat_buf.st_mode)) &&      // check for directory
				((stat_buf.st_mode & 07777) == 0700)) // check for mode=0700
			{
					// client created a directory as instructed
				stat_is_okay = true;
			} else {
				// it wasn't proper.  however, there is a config knob for
				// backwards compatibility that allows either a file or a
				// directory.  THIS IS NOT SECURE.  it is off by default.
				if (param_boolean("FS_ALLOW_UNSAFE", false)) {
					if ((stat_buf.st_nlink == 1) &&	    // check hard link count
						(!S_ISLNK(stat_buf.st_mode)) && // check for soft link
						(S_ISREG(stat_buf.st_mode)) )   // check for regular file
					{
						stat_is_okay = true;
						used_file = true;
					}
				}
			}

			if (!stat_is_okay) {
				// something was wrong with the stat info
				server_result = -1;
				errstack->pushf((remote_?"FS_REMOTE":"FS"), 1005,
						"Bad attributes on (%s)", m_new_dir.c_str());
			} else {
				// need to lookup username from dir and do setOwner()
				char *tmpOwner = my_username( stat_buf.st_uid );
				if (!tmpOwner) {
					// this could happen if, for instance,
					// getpwuid() failed.
					server_result = -1;
					errstack->pushf((remote_?"FS_REMOTE":"FS"), 1006,
							"Unable to lookup uid %i", stat_buf.st_uid);
				} else {
					server_result = 0;	// 0 means success here. sigh.
					setRemoteUser( tmpOwner );
					setAuthenticatedName( tmpOwner );
					free( tmpOwner );
					setRemoteDomain( getLocalDomain() );
				}
			}
		}
	} else {
		server_result = -1;
		if (m_new_dir.size() && m_new_dir[0]) {
			errstack->pushf((remote_?"FS_REMOTE":"FS"), 1007,
					"Client unable to create dir (%s)", m_new_dir.c_str());
		}
		// else we gave the client a bogus name to begin with!
	}

	if (!mySock_->code( server_result ) || !mySock_->end_of_message())
	{
		dprintf(D_SECURITY, "Protocol failure at %s, %d!\n",
			__FUNCTION__, __LINE__);
		return fail;
	}
	// leave new_dir allocated until after dprintf below

	dprintf( D_SECURITY, "AUTHENTICATE_FS%s: used %s %s, status: %d\n", 
			(remote_?"_REMOTE":""), (used_file?"file":"dir"),
			(m_new_dir.size() ? m_new_dir.c_str() : "(null)"), (server_result == 0) );

	// this function returns TRUE on success, FALSE on failure,
	// which is just the opposite of server_result.
	return( server_result == 0 );
}

int Condor_Auth_FS :: isValid() const
{
    return TRUE;
}

#endif
