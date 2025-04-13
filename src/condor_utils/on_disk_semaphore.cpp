#include "condor_common.h"
#include "condor_config.h"
#include "condor_debug.h"

#include "on_disk_semaphore.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "shortfile.h"


OnDiskSemaphore::OnDiskSemaphore( const std::string & k ) : key(k) {
    std::string LOCK = param("LOCK");
    std::filesystem::path lock(LOCK);

    // Make sure that replace_extension() acts like add_extension().
    std::replace( key.begin(), key.end(), '.', '_' );

    std::error_code ec;
    if(! std::filesystem::create_directories(lock, ec)) {
        // My reading of the spec is that it is explicitly forbidden to
        // return false from create_directories() because the directories
        // already exist.
        if( ec.value() != 0 ) {
            dprintf( D_ALWAYS, "OnDiskSemaphore(%s): failed to create lock directory '%s': %s (%d)\n", k.c_str(), lock.string().c_str(), ec.message().c_str(), ec.value() );
        }
    }

    this->keyfile = lock/key;
}


OnDiskSemaphore::~OnDiskSemaphore() {
    this->cleanup();
    if( keyfile_fd != -1 ) { close(keyfile_fd); }
}


OnDiskSemaphore::Status
OnDiskSemaphore::acquire( std::string & message ) {
    std::error_code ec;

    // FIXME: we should be doing this as PRIV_CONDOR, right?
    int fd = open( keyfile.string().c_str(), O_CREAT | O_EXCL | O_RDWR, 0400 );
    if( fd == -1 ) {
        if( errno == EEXIST ) {
            // We lost the race.
            lockholder = false;

            // FIXME: check for lease expiration.

            // Create a hardlink to make the kernel handle reference-counting.
            std::string pid = std::to_string( getpid() );
            hardlink = keyfile;
            hardlink.replace_extension( pid );
            std::filesystem::create_hard_link( keyfile, hardlink, ec );

            // Read the lock file to determine the status.
            fd = open( keyfile.string().c_str(), O_RDONLY );
            if( fd == -1 ) {
                dprintf( D_ALWAYS, "OnDiskSemaphore::acquire(): failed to open(%s): %s %d\n", keyfile.string().c_str(), strerror(errno), errno );
                return OnDiskSemaphore::INVALID;
            }

            char status_byte = '\0';
            ssize_t bytes_read = read(fd, (void *)&status_byte, 1);
            switch( bytes_read ) {
                case 0:
                    // We managed to run between the creation of the file
                    // and the immdiately subsequent write of the status
                    // byte.  If we did, the status is clearly UNREADY.
                    status_byte = OnDiskSemaphore::UNREADY;
                    break;
                case 1:
                    // We successfully read the status byte.
                    break;
                default:
                    dprintf( D_ALWAYS, "OnDiskSemaphore::acquire(): failed to read() 1 byte (%zu): %s (%d)\n", bytes_read, strerror(errno), errno );
                    close(fd);
                    return OnDiskSemaphore::INVALID;
                    break;
            }

            if( OnDiskSemaphore::MIN < status_byte && status_byte < OnDiskSemaphore::MAX ) {
                close(fd);

                if( status_byte == OnDiskSemaphore::READY ) {
                    std::filesystem::path messagePath = keyfile;
                    messagePath.replace_extension( "message" );
                    if(! htcondor::readShortFile( messagePath.string(), message )) {
                        dprintf( D_ALWAYS, "OnDiskSemaphore::acquire(): readShortFile() failed to read message file.\n" );
                        return OnDiskSemaphore::INVALID;
                    }
                }

                return OnDiskSemaphore::Status(status_byte);
            } else {
                dprintf( D_ALWAYS, "OnDiskSemaphore::acquire(): read invalid lock byte %d\n", (int)status_byte );
                return OnDiskSemaphore::INVALID;
            }
        } else {
            dprintf( D_ALWAYS, "OnDiskSemaphore::acquire(): failed to open(%s): %s (%d)\n", keyfile.string().c_str(), strerror(errno), errno );
            return OnDiskSemaphore::INVALID;
        }
    } else {
        // We won the race.
        keyfile_fd = fd;
        lockholder = true;

        // FIMXE: Consider _also_ making a .pid file here so that we know
        // which shadow is holding the lock; it doesn't have to be a hardlink.

        // The semaphore status is now UNREADY.
        off_t r = lseek( keyfile_fd, 0, SEEK_SET );
        if( r == -1 ) { return OnDiskSemaphore::INVALID; }

        char status = (char)OnDiskSemaphore::UNREADY;
        ssize_t w  = write( keyfile_fd, (void*)&status, 1 );
        if( w != 1 ) { return OnDiskSemaphore::INVALID; }

        return OnDiskSemaphore::PREPARING;
    }
}


bool
OnDiskSemaphore::touch() {
    if(! lockholder ) { return false; }

    int rv = futimens( keyfile_fd, NULL );
    return rv == 0;
}


bool
OnDiskSemaphore::ready( const std::string & message ) {
    if(! lockholder) { return false; }

    std::filesystem::path messagePath = keyfile;
    messagePath.replace_extension( "message" );
    if(! htcondor::writeShortFile( messagePath.string(), message )) {
        // FIXME: ...?
    }

    off_t r = lseek( keyfile_fd, 0, SEEK_SET );
    if( r == -1 ) { return false; }

    char status = (char)OnDiskSemaphore::READY;
    ssize_t w  = write( keyfile_fd, (void*)&status, 1 );

    return w == 1;
}


bool
OnDiskSemaphore::release() {
    std::error_code ec;
    dprintf( D_ALWAYS, "OnDiskSemaphore::release()\n" );

    if(! lockholder) {
        return cleanup();
    } else {
        int count = std::filesystem::hard_link_count( keyfile, ec );
        if( count == 1 ) {
            return cleanup();
        } else {
            return false;
        }
    }
}


bool
OnDiskSemaphore::cleanup() {
    std::error_code ec;
    dprintf( D_ALWAYS, "OnDiskSemaphore::cleanup()\n" );

    if(! lockholder) {
        std::filesystem::remove( hardlink, ec );
        return true;
    } else {
        std::filesystem::remove( keyfile, ec );

        std::filesystem::path messagePath = keyfile;
        messagePath.replace_extension( "message" );
        std::filesystem::remove( messagePath, ec );
    }

    return true;
}
