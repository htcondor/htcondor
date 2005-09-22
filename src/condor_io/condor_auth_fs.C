/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#if !defined(SKIP_AUTHENTICATION) && !defined(WIN32)
#include "condor_auth_fs.h"
#include "condor_string.h"
#include "condor_environ.h"
#include "CondorError.h"
#include "my_hostname.h"

Condor_Auth_FS :: Condor_Auth_FS(ReliSock * sock, int remote)
    : Condor_Auth_Base    ( sock, CAUTH_FILESYSTEM ),
      remote_             ( remote )
{
}

Condor_Auth_FS :: ~Condor_Auth_FS()
{
}

int Condor_Auth_FS::authenticate(const char * remoteHost, CondorError* errstack)
{
    char *new_file = NULL;
    int fd = -1;
    int retval = -1;
	int fail = -1 == 0;
    
    if ( mySock_->isClient() ) {
        mySock_->decode();
        if (!mySock_->code( new_file )) { 
			dprintf(D_SECURITY, "Protocol failure at %s, %d!\n",
				__FUNCTION__, __LINE__);
			return fail; 
		}
        if ( new_file ) {
			/* XXX hope we aren't root when we do this test */
            fd = open(new_file, O_RDONLY | O_CREAT | O_TRUNC, 0666);
			if (fd >= 0) {
            	::close(fd);
			}
        }
        if (!mySock_->end_of_message()) { 
			dprintf(D_SECURITY, "Protocol failure at %s, %d!\n",
				__FUNCTION__, __LINE__);
        	if ( new_file ) {
            	unlink( new_file ); /* XXX hope we aren't root */
            	free( new_file );
        	}
			return fail; 
		}
        mySock_->encode();
        //send over fd as a success/failure indicator (-1 == failure)
        if (!mySock_->code( fd ) || 
        	!mySock_->end_of_message()) 
		{
			dprintf(D_SECURITY, "Protocol failure at %s, %d!\n",
				__FUNCTION__, __LINE__);
        	if ( new_file ) {
            	unlink( new_file ); /* XXX hope we aren't root */
            	free( new_file );
        	}
			return fail; 
		}
        mySock_->decode();
        if (!mySock_->code( retval ) || 
       		!mySock_->end_of_message()) 
		{ 
			dprintf(D_SECURITY, "Protocol failure at %s, %d!\n",
				__FUNCTION__, __LINE__);
        	if ( new_file ) {
            	unlink( new_file ); /* XXX hope we aren't root */
            	free( new_file );
        	}
			return fail; 
		}
		// leave new_file allocated until after dprintf below
    }
    else {  //server 
        setRemoteUser( NULL );
        
        if ( remote_ ) {
	        pid_t    mypid = 0;
#ifdef WIN32
	        mypid = ::GetCurrentProcessId();
#else
	        mypid = ::getpid();
#endif

			MyString filename;
			char * rendezvous_dir = param("FS_REMOTE_DIR");
			if (rendezvous_dir) {
				filename = rendezvous_dir;
				free(rendezvous_dir);
			} else {
				// misconfiguration.  complain, and then use /tmp
				dprintf (D_SECURITY, "AUTHENTICATE_FS: FS_REMOTE was used but no FS_REMOTE_DIR defined!\n");
				filename = "/tmp";
			}
			filename += "/FS_REMOTE_";
			filename += my_hostname();
			filename += "_";
			filename += mypid;
			filename += "_XXXXXXXXX";
			new_file = strdup( filename.Value() );
			dprintf( D_SECURITY, "FS_REMOTE: client template is %s\n", new_file );
			mktemp(new_file);
			dprintf( D_SECURITY, "FS_REMOTE: client filename is %s\n", new_file );
		} else {
			new_file = tempnam("/tmp", "FS_");
		}
        //now, send over filename for client to create
        // man page says string returned by tempnam has been malloc()'d
        mySock_->encode();
        if (!mySock_->code( new_file ) ||
        	!mySock_->end_of_message())
		{
			dprintf(D_SECURITY, "Protocol failure at %s, %d!\n",
				__FUNCTION__, __LINE__);
			if (new_file != NULL) {
				free(new_file);
			}
			return fail;
		}
        mySock_->decode();
        if (!mySock_->code( fd ) ||
        	!mySock_->end_of_message())
		{
			dprintf(D_SECURITY, "Protocol failure at %s, %d!\n",
				__FUNCTION__, __LINE__);
			free(new_file);
			return fail;
		}
        mySock_->encode();

        retval = -1;	// assume failure.  i am glass-half-empty man.
        if ( fd > -1 ) {
			if (remote_) {
				// now, if this is a remote filesystem, it is likely to be NFS.
				// before we stat the file, we need to do something that causes the
				// NFS client to sync to the NFS server.  fsync() does not do this.
				// in practice, creating a file or directory should force the NFS
				// client to sync in order to avoid race conditions.
				MyString filename_template = "/tmp";

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
				filename_template += "/FS_REMOTE_";
				filename_template += my_hostname();
				filename_template += "_";
				filename_template += mypid;
				filename_template += "_XXXXXX";
				char* filename_inout = strdup(filename_template.Value());

				dprintf( D_SECURITY, "FS_REMOTE: sync filename is %s\n", filename_inout );

				int sync_fd = mkstemp(filename_inout);
				if (sync_fd >= 0) {
					::close(sync_fd);
					unlink( filename_inout ); /* XXX hope we aren't root */
				} else {
					// we could have an else that fails here, but we may as well still
					// check for the file -- this was just an attempt to make NFS sync.
					// if the file still isn't there, we'll fail anyways.
					dprintf( D_ALWAYS, "FS_REMOTE: warning, failed to make temp file %s\n");
				}

				free (filename_inout);

				// at this point we should have created and deleted a unique file
				// from the server side, forcing the nfs client to flush everything
				// and sync with the server.  now, we can finally stat the file.
			}

            //check file to ensure that claimant is owner
            struct stat stat_buf;

            if ( lstat( new_file, &stat_buf ) < 0 ) {
                retval = -1;  
            }
            else {
                // Authentication should fail if a) owner match fails, or b) the
                // file is either a hard or soft link (no links allowed because they
                // could spoof the owner match).  -Todd 3/98
                if ( 
                    (stat_buf.st_nlink > 1) ||	 // check for hard link
                    (S_ISLNK(stat_buf.st_mode)) ) 
                    {
                        retval = -1;
                    } 
                else {
                    //need to lookup username from file and do setOwner()
                    char *tmpOwner = my_username( stat_buf.st_uid );
					if (!tmpOwner) {
							// this could happen if, for instance,
							// getpwuid() failed.
							retval = -1;
					} else {
							retval = 0;	// 0 means success here. sigh.
							setRemoteUser( tmpOwner );
							free( tmpOwner );
							setRemoteDomain( getLocalDomain());
					}
                }
            }
        } 
        else {
            retval = -1;
            dprintf(D_ALWAYS,
                    "invalid state in authenticate_filesystem (file %s)\n",
                    new_file ? new_file : "(null)" );
        }
        
        if (!mySock_->code( retval ) || 
        	!mySock_->end_of_message())
		{
			dprintf(D_SECURITY, "Protocol failure at %s, %d!\n",
				__FUNCTION__, __LINE__);
			free(new_file);
			return fail;
		}
		// leave new_file allocated until after dprintf below
    }

   	dprintf( D_SECURITY, "AUTHENTICATE_FS: used file %s, status: %d\n", 
             (new_file ? new_file : "(null)"), (retval == 0) );

	if ( new_file ) {
   		// client responsible for deleting the file
		if (mySock_->isClient()) {
			unlink( new_file ); /* XXX hope we aren't root */
		}
		free( new_file );
	}

    return( retval == 0 );
}

int Condor_Auth_FS :: isValid() const
{
    return TRUE;
}

#endif
