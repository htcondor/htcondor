
#include "file_types.h"
#include "condor_syscall_mode.h"
#include "condor_debug.h"
#include "condor_sys.h"

#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>

// XXX Where is the header for this?
extern "C" int syscall( int kind, ... );

void file_warning( char *format, ... )
{
	va_list	args;
	va_start(args,format);

	static char text[1024];
	vsprintf(text,format,args);

	dprintf(D_ALWAYS,text);
	fprintf(stderr,text);

	if(RemoteSysCalls()) REMOTE_syscall( CONDOR_perm_error, text );

	va_end(args);
}

void File::dump() {
	dprintf(D_ALWAYS,
		"rfd: %d r: %d w: %d size: %d users: %d kind: '%s' name: '%s'",
		fd,readable,writeable,size,use_count,kind,name);
}

int File::illegal( char *op )
{
	file_warning("File system call '%s' is not supported for file '%s', which is mapped to a %s.\n",op,get_name(),get_kind());
	errno = ENOTSUP;
	return -1;
}

int File::fstat( struct stat *buf )
{
	return illegal("fstat");
}

int File::ioctl( int cmd, int arg )
{
	return illegal("ioctl");
}

int File::flock( int op )
{
	return illegal("flock");
}

int File::fstatfs( struct statfs *buf )
{
	return illegal("fstatfs");
}

int File::fchown( uid_t owner, gid_t group )
{
	return illegal("fchown");
}

int File::fchmod( mode_t mode )
{
	return illegal("fchmod");
}

int File::ftruncate( size_t length )
{
	return illegal("ftruncate");
}

int File::fsync()
{
	return illegal("fsync");
}

int File::fcntl(int cmd, int arg)
{
	return illegal("fcntl");
}

LocalFile::LocalFile()
{
	fd = -1;
	readable = writeable = 0;
	strcpy(kind,"local file");
	name[0] = 0;
	size = 0;
	use_count = 0;
	forced = 0;
	suspended = 0;
	bufferable = 0;
}

int LocalFile::open(const char *path, int flags, int mode ) {

	strncpy(name,path,_POSIX_PATH_MAX);

	fd = syscall( SYS_open, path, flags, mode );

	switch( flags & O_ACCMODE ) {
		case O_RDONLY:
			readable = 1;
			writeable = 0;
			break;
		case O_WRONLY:
			readable = 0;
			writeable = 1;
			break;
		case O_RDWR:
			readable = 1;
			writeable = 1;
			break;
		default:
			return -1;
	}

	size = syscall( SYS_lseek, fd, 0 , SEEK_END );
	if(size==-1) size=0;
	syscall( SYS_lseek, fd, 0, SEEK_SET );

	return fd;
}

int LocalFile::close() {
	return syscall( SYS_close, fd );
}

int LocalFile::read(int pos, char *data, int length) {
	// XXX Optimize this where possible
	syscall(SYS_lseek, fd, pos, SEEK_SET);
	return syscall( SYS_read, fd, data, length );
}

int LocalFile::write(int pos, char *data, int length) {
	// XXX Optimize this where possible
	syscall(SYS_lseek, fd, pos, SEEK_SET);
	return syscall( SYS_write, fd, data, length );
}

int LocalFile::fcntl( int cmd, int arg )
{
	return syscall( SYS_fcntl, fd, cmd, arg );
}

int LocalFile::fstat( struct stat *buf )
{
	return syscall( SYS_fstat, fd, buf );
}

int LocalFile::ioctl( int cmd, int arg )
{
	return syscall( SYS_ioctl, fd, cmd, arg );
}

int LocalFile::flock( int op )
{
#ifdef SYS_flock
	return syscall( SYS_flock, fd, op );
#else
	return File::flock(op);
#endif
}

int LocalFile::fstatfs( struct statfs *buf )
{
#ifdef SYS_fstatfs
	return syscall( SYS_fstatfs, fd, buf );
#else
	return File::fstatfs(buf);
#endif
}

int LocalFile::fchown( uid_t owner, gid_t group )
{
	return syscall( SYS_fchown, fd, owner, group );
}

int LocalFile::fchmod( mode_t mode )
{
	return syscall( SYS_fchmod, fd, mode );
}

int LocalFile::ftruncate( size_t length )
{
#ifdef SYS_ftruncate
	// Buffer flush happened from above
	size = length;
	return syscall( SYS_ftruncate, fd, length );
#else
	return File::ftruncate(length);
#endif
}

int LocalFile::fsync()
{
#ifdef SYS_fsync
	return syscall( SYS_fsync, fd );
#else
	return File::fsync();
#endif
}

void LocalFile::checkpoint()
{
}

void LocalFile::suspend()
{
	if(suspended) return;
	suspended = 1;

	if(!forced) syscall( SYS_close, fd );
}

void LocalFile::resume()
{
	if(!suspended) return;
	suspended = 0;

	int flags;

	if( readable&&writeable ) {
		flags = O_RDWR;
	} else if( writeable ) {
		flags = O_WRONLY;
	} else {
		flags = O_RDONLY;
	}

	if(!forced) {
		fd = syscall( SYS_open, name, flags, 0 );
		if( fd==-1 ) {
			file_warning("Unable to reopen local file %s after a checkpoint!\n",name);
		}
	}
}

int LocalFile::map_fd_hack()
{
	return fd;
}

int LocalFile::local_access_hack()
{
	return 0;
}

RemoteFile::RemoteFile()
{
	fd = -1;
	readable = writeable = 0;
	strcpy(kind,"remote file");
	name[0] = 0;
	size = 0;
	use_count = 0;
	forced = 0;
	suspended = 0;
	bufferable = 0;
}

int RemoteFile::open(const char *path, int flags, int mode )
{
	strncpy(name,path,_POSIX_PATH_MAX);

	fd = REMOTE_syscall( CONDOR_open, path, flags, mode );

	if( fd<0 ) return fd;

	switch( flags & O_ACCMODE ) {
		case O_RDONLY:
			readable = 1;
			writeable = 0;
			break;
		case O_WRONLY:
			readable = 0;
			writeable = 1;
			break;
		case O_RDWR:
			readable = 1;
			writeable = 1;
			break;
		default:
			return -1;
	}

	size = REMOTE_syscall( CONDOR_lseek, fd, 0, SEEK_END );
	if(size<0) size=0;
	REMOTE_syscall( CONDOR_lseek, fd, 0, SEEK_SET );

	return fd;
}

int RemoteFile::close() {
	return REMOTE_syscall( CONDOR_close, fd );
}

int RemoteFile::read(int pos, char *data, int length) {
	// XXX optimize this to to read when possible
       	return REMOTE_syscall( CONDOR_lseekread, fd, pos, SEEK_SET, data, length );
}

int RemoteFile::write(int pos, char *data, int length) {
	// XXX optimize this to do write when possible
	return REMOTE_syscall( CONDOR_lseekwrite, fd, pos, SEEK_SET, data, length );
}

void RemoteFile::checkpoint()
{
}

void RemoteFile::suspend() {
	if(suspended) return;
	suspended = 1;

	if(!forced) REMOTE_syscall( CONDOR_close, fd );
}

void RemoteFile::resume() {

	if(!suspended) return;
	suspended = 0;

	int flags;

	if( readable&&writeable ) {
		flags = O_RDWR;
	} else if( writeable ) {
		flags = O_WRONLY;
	} else {
		flags = O_RDONLY;
	}

	if(!forced) {
		fd=REMOTE_syscall( CONDOR_open, name, flags, 0 );
		if(fd<0) {
			file_warning("Unable to re-open remote file %s after checkpoint!\n",name);
		}
	}
}

int RemoteFile::fcntl( int cmd, int arg )
{
	return REMOTE_syscall( CONDOR_fcntl, fd, cmd, arg );
}

int RemoteFile::fstat( struct stat *buf )
{
	return REMOTE_syscall( CONDOR_fstat, fd, buf );
}

int RemoteFile::ioctl( int cmd, int arg )
{
	return REMOTE_syscall( CONDOR_ioctl, fd, cmd, arg );
}

int RemoteFile::flock( int op )
{
	return REMOTE_syscall( CONDOR_flock, fd, op );
}

int RemoteFile::fstatfs( struct statfs *buf )
{
	return REMOTE_syscall( CONDOR_fstatfs, fd, buf );
}

int RemoteFile::fchown( uid_t owner, gid_t group )
{
	return REMOTE_syscall( CONDOR_fchown, fd, owner, group );
}

int RemoteFile::fchmod( mode_t mode )
{
	return REMOTE_syscall( CONDOR_fchmod, fd, mode );
}

int RemoteFile::ftruncate( size_t length )
{
	size = length;
	return REMOTE_syscall( CONDOR_ftruncate, fd, length );
}

int RemoteFile::fsync()
{
	return REMOTE_syscall( CONDOR_fsync, fd );
}

int RemoteFile::map_fd_hack()
{
	return fd;
}

int RemoteFile::local_access_hack()
{
	return 0;
}

