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

#if !defined(SKIP_AUTHENTICATION) && !defined(WIN32)
#include "condor_auth_fs.h"

Condor_Auth_FS :: Condor_Auth_FS(ReliSock * sock, int remote)
    : Condor_Auth_Base( sock, CAUTH_FILESYSTEM ),
      remote_         ( remote )
{
    RendezvousDirectory = strnewp( getenv( "RENDEZVOUS_DIRECTORY" ) );
}

Condor_Auth_FS :: ~Condor_Auth_FS()
{
    if (RendezvousDirectory) {
        free(RendezvousDirectory);
    }
}

int Condor_Auth_FS::authenticate(const char * remoteHost)
{
    char *new_file = NULL;
    int fd = -1;
    char *owner = NULL;
    int retval = -1;
    
    if ( mySock_->isClient() ) {
        if ( remote_ ) {
            //send over the directory
            mySock_->encode();
            mySock_->code( RendezvousDirectory );
            mySock_->end_of_message();
        }
        mySock_->decode();
        mySock_->code( new_file );
        if ( new_file ) {
            fd = open(new_file, O_RDONLY | O_CREAT | O_TRUNC, 0666);
            ::close(fd);
        }
        mySock_->end_of_message();
        mySock_->encode();
        //send over fd as a success/failure indicator (-1 == failure)
        mySock_->code( fd );
        mySock_->end_of_message();
        mySock_->decode();
        mySock_->code( retval );
        mySock_->end_of_message();
        if ( new_file ) {
            unlink( new_file );
            free( new_file );
        }
    }
    else {  //server 
        setRemoteUser( NULL );
        
        if ( remote_ ) {
	    //get RendezvousDirectory from client
            mySock_->decode();
            mySock_->code( RendezvousDirectory );
            mySock_->end_of_message();
            dprintf(D_FULLDEBUG,"RendezvousDirectory: %s\n", RendezvousDirectory );
            new_file = tempnam( RendezvousDirectory, "qmgr_");
        }
        else {
            new_file = tempnam("/tmp", "qmgr_");
        }
        //now, send over filename for client to create
        // man page says string returned by tempnam has been malloc()'d
        mySock_->encode();
        mySock_->code( new_file );
        mySock_->end_of_message();
        mySock_->decode();
        mySock_->code( fd );
        mySock_->end_of_message();
        mySock_->encode();
        
        retval = 0;
        if ( fd > -1 ) {
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
                    setRemoteUser( tmpOwner );
                    free( tmpOwner );
                }
            }
        } 
        else {
            retval = -1;
            dprintf(D_ALWAYS,
                    "invalid state in authenticate_filesystem (file %s)\n",
                    new_file ? new_file : "(null)" );
        }
        
        mySock_->code( retval );
        mySock_->end_of_message();
        free( new_file );
    }
    dprintf( D_FULLDEBUG, "authentcate_filesystem%s status: %d\n", 
             remote_ ? "(REMOTE)" : "()", retval == 0 );
    return( retval == 0 );
}

int Condor_Auth_FS :: isValid() const
{
    return TRUE;
}

#endif
