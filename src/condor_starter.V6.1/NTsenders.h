
#ifndef NT_SENDERS_H
#define NT_SENDERS_H

#include "condor_common.h"

class ClassAd;

extern "C" {
	int REMOTE_CONDOR_register_machine_info( char *uiddomain, char *fsdomain, char *address, char *fullHostname, int key );
	int REMOTE_CONDOR_register_mpi_master_info( ClassAd *ad );
	int REMOTE_CONDOR_register_starter_info( ClassAd *ad );
	int REMOTE_CONDOR_get_job_info( ClassAd *ad );
	int REMOTE_CONDOR_get_user_info( ClassAd *ad );
	int REMOTE_CONDOR_get_executable( char *destination );
	int REMOTE_CONDOR_job_exit( int status, int reason, ClassAd *ad );
	int REMOTE_CONDOR_begin_execution( void );
	int REMOTE_CONDOR_open( char *path, open_flags_t flags, int mode );
	int REMOTE_CONDOR_close( int fd );
	int REMOTE_CONDOR_unlink( char *path );
	int REMOTE_CONDOR_rename( char *path, char *newpath );
	int REMOTE_CONDOR_read( int fd, void *data, size_t length );
	int REMOTE_CONDOR_write( int fd, void *data, size_t length );
	int REMOTE_CONDOR_lseek( int fd, off_t offset, int whence );
	int REMOTE_CONDOR_mkdir( char *path, int mode );
	int REMOTE_CONDOR_rmdir( char *path );
	int REMOTE_CONDOR_fsync( int fd );
}

#endif




