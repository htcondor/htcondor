#include "condor_common.h"
#include "condor_config.h"
#include "condor_debug.h"

#include "single_provider_syndicate.h"

#ifdef    WINDOWS

SingleProviderSyndicate::SingleProviderSyndicate( const std::string & k ) : key(k) {
}


SingleProviderSyndicate::~SingleProviderSyndicate() {
}


SingleProviderSyndicate::Status
SingleProviderSyndicate::acquire( std::string & message ) {
    return SingleProviderSyndicate::INVALID;
}


bool
SingleProviderSyndicate::touch() {
    return false;
}


bool
SingleProviderSyndicate::ready( const std::string & message ) {
    return false;
}


bool
SingleProviderSyndicate::release() {
    return false;
}


#else /* Linux and Mac */


#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "condor_uid.h"
#include "shortfile.h"
#include <chrono>
using namespace std::chrono_literals;


/*

    The election of a provider is performed by attempting to open() the
    "keyfile", a file in $(LOCK) with the same name as the resource, with
    the O_CREAT | O_EXCL flags; the kernel ensures that only one process
    succeeds in doing so.

    Workers using a resource use hardlinks to atomically increment and
    decrement the count of workers using the resource.

    The modification ("last write time") of the keyfile stores the lease
    timer, and the file itself stores the single-byte (and therefore
    atomically-updated) status flag.

    The message is stored in a different file; it's updated before the
    status byte becomes READY and never updated, so atomic updates aren't
    necessary.

    Some of the individual operations are a little complicated to account
    for race conditions.
*/


SingleProviderSyndicate::SingleProviderSyndicate( const std::string & k ) : key(k) {
    // FIXME: Some damn fools configure this on shared filesystems.  If
    // CREATE_LOCKS_ON_LOCAL_DISK is set, call FileLock::getTempPath(p)
    // to get the lock directory instead.

    char *LOCK_cstr = param("LOCK");
    std::string LOCK;
    if (LOCK_cstr != nullptr) {
        LOCK = LOCK_cstr;
        free(LOCK_cstr);
    }
    std::filesystem::path lock(LOCK);
    std::filesystem::path syndicate = lock / "syndicate";

    // Make sure that replace_extension() acts like add_extension().
    std::replace( key.begin(), key.end(), '.', '_' );


    TemporaryPrivSentry tps(PRIV_CONDOR);

    std::error_code ec;
    if(! std::filesystem::create_directories(syndicate, ec)) {
        // My reading of the spec is that it is explicitly forbidden to
        // return false from create_directories() because the directories
        // already exist.
        if( ec.value() != 0 ) {
            dprintf( D_ALWAYS, "SingleProviderSyndicate(%s): failed to create lock directory '%s': %s (%d)\n", k.c_str(), lock.string().c_str(), ec.message().c_str(), ec.value() );
        }
    }

    this->keyfile = syndicate/key;
}


SingleProviderSyndicate::~SingleProviderSyndicate() {
    this->cleanup();
    if( keyfile_fd != -1 ) { close(keyfile_fd); }
}


bool
take_remove_lock( const std::filesystem::path & keyfile, int depth ) {
    // Try to grab the remove lock at the indicated depth.
    std::filesystem::path rmfile = keyfile;
    std::string d = ".rm_" + std::to_string( depth );
    rmfile.replace_extension( d );
    int fd = open( rmfile.string().c_str(), O_CREAT | O_EXCL | O_RDWR, 0400 );
    close( fd );
    if( fd != -1 ) {
        return true;
    }

    // Check to see if the existing remove lock has expired.
    std::error_code ec;
    auto then = std::filesystem::last_write_time( rmfile, ec );
    if( ec.value() != 0 ) {
        dprintf( D_ALWAYS, "take_remove_lock(): failed to read last_write_time(%s): %s %d\n", rmfile.string().c_str(), strerror(errno), errno );
        return false;
    }
    auto diff = std::chrono::file_clock::now() - then;
    if( diff >= 300s ) {
        return take_remove_lock( keyfile, depth + 1 );
    }

    // Somebody else is removing the keyfile.
    return false;
}


void
remove_remove_locks( const std::filesystem::path & keyfile ) {
    std::filesystem::path rmfile = keyfile;

    for( int depth = 0; ; ++depth ) {
        std::string d = std::to_string( depth );
        rmfile.replace_extension( d );

        std::error_code ec;
        if(! std::filesystem::exists( rmfile, ec )) { return; }
        std::filesystem::remove( rmfile, ec );
    }
}


SingleProviderSyndicate::Status
SingleProviderSyndicate::acquire( std::string & message ) {
    std::error_code ec;


    TemporaryPrivSentry tps(PRIV_CONDOR);

    int fd = open( keyfile.string().c_str(), O_CREAT | O_EXCL | O_RDWR, 0400 );
    if( fd == -1 ) {
        if( errno == EEXIST ) {
            // We lost the race.
            lockholder = false;

            // Check the lease.  If it's expired, remove the keyfile and
            // re-run the race.
            auto then = std::filesystem::last_write_time( keyfile, ec );
            if( ec.value() != 0 ) {
                dprintf( D_ALWAYS, "SingleProviderSyndicate::acquire(): failed to read last_write_time(%s): %s %d\n", keyfile.string().c_str(), strerror(errno), errno );
                return SingleProviderSyndicate::INVALID;
            }
            auto diff = std::chrono::file_clock::now() - then;
            if( diff >= 300s ) {
                dprintf( D_FULLDEBUG, "SingleProviderSyndicate::acquire(): lease expired.\n" );

                //
                // We can't just remove and re-create the keyfile, because
                // another process could create the keyfile between when we
                // check the lease and when we remove the keyfile.
                //
                // Instead, recursively try to open() keyfile.rm-<k>, where
                // `k` starts at 0.  If we grab the k'th rm lock, remove
                // the keyfile, create it, and then remove the 0-to-k'th
                // rm lockfiles.  If we don't grab the k'th rm lock, and
                // the lease hasn't expired, re-acquire(); if it has expired,
                // recursively try to grab the `k+1`th rm lock.
                //
                // Thus, we won't remove the keyfile until we hold the
                // rm-lock, and we won't remove the rm-locks until we've
                // removed the keyfile _and_ successfully locked the keyfile.
                //

                if(! take_remove_lock( keyfile, 0 )) {
                    return acquire( message );
                }

                std::filesystem::remove( keyfile, ec );
                if( ec.value() != 0 ) {
                    dprintf( D_ALWAYS, "SingleProviderSyndicate::acquire(): failed to remove(%s): %s %d\n", keyfile.string().c_str(), strerror(errno), errno );
                    // We must remove the keyfile for this algorithm to work.
                    return SingleProviderSyndicate::INVALID;
                }

                return acquire(message);
            }

            // Create a hardlink to make the kernel handle reference-counting.
            std::string pid = std::to_string( getpid() );
            hardlink = keyfile;
            hardlink.replace_extension( pid );
            // If this fails because of an error, try to create the hard link
            // and report that error, instead.
            if(! std::filesystem::exists( hardlink, ec )) {
                std::filesystem::create_hard_link( keyfile, hardlink, ec );
                if( ec.value() != 0 ) {
                    // If we failed to create the hardlink, the original keyfile
                    // may be gone, and we may become the new provider.
                    dprintf( D_ALWAYS, "SingleProviderSyndicate::acquire(): create_hard_link() failed: %s (%d)\n", ec.message().c_str(), ec.value() );
                    return acquire(message);
                }
            }

            // Read the lock file to determine the status.
            fd = open( keyfile.string().c_str(), O_RDONLY );
            if( fd == -1 ) {
                dprintf( D_ALWAYS, "SingleProviderSyndicate::acquire(): failed to open(%s): %s %d\n", keyfile.string().c_str(), strerror(errno), errno );
                return SingleProviderSyndicate::INVALID;
            }

            char status_byte = '\0';
            ssize_t bytes_read = read(fd, (void *)&status_byte, 1);
            switch( bytes_read ) {
                case 0:
                    // We managed to run between the creation of the file
                    // and the immdiately subsequent write of the status
                    // byte.  If we did, the status is clearly UNREADY.
                    status_byte = SingleProviderSyndicate::UNREADY;
                    break;
                case 1:
                    // We successfully read the status byte.
                    break;
                default:
                    dprintf( D_ALWAYS, "SingleProviderSyndicate::acquire(): failed to read() 1 byte (%zu): %s (%d)\n", bytes_read, strerror(errno), errno );
                    close(fd);
                    return SingleProviderSyndicate::INVALID;
                    break;
            }

            if( SingleProviderSyndicate::MIN < status_byte && status_byte < SingleProviderSyndicate::MAX ) {
                close(fd);

                if( status_byte == SingleProviderSyndicate::READY ) {
                    std::filesystem::path messagePath = keyfile;
                    messagePath.replace_extension( "message" );
                    if(! htcondor::readShortFile( messagePath.string(), message )) {
                        dprintf( D_ALWAYS, "SingleProviderSyndicate::acquire(): readShortFile() failed to read message file.\n" );
                        return SingleProviderSyndicate::INVALID;
                    }
                }

                return SingleProviderSyndicate::Status(status_byte);
            } else {
                dprintf( D_ALWAYS, "SingleProviderSyndicate::acquire(): read invalid lock byte %d\n", (int)status_byte );
                return SingleProviderSyndicate::INVALID;
            }
        } else {
            dprintf( D_ALWAYS, "SingleProviderSyndicate::acquire(): failed to open(%s): %s (%d)\n", keyfile.string().c_str(), strerror(errno), errno );
            return SingleProviderSyndicate::INVALID;
        }
    } else {
        // We won the race.
        keyfile_fd = fd;
        lockholder = true;

        // Remove the old remove locks, if any.  This isn't necessary,
        // but it leaves the on-disk state less confusing.
        remove_remove_locks( keyfile );

        // Remove the old message file, if any.  This isn't necessary,
        // but it leaves the on-disk state less confusing.
        std::filesystem::path messagePath = keyfile;
        messagePath.replace_extension( "message" );
        std::filesystem::remove( messagePath, ec );

        // Consider making a .pid file here so that we know which shadow
        // is holding the lock; it doesn't have to be a hardlink.

        // The semaphore status is now UNREADY.
        off_t r = lseek( keyfile_fd, 0, SEEK_SET );
        if( r == -1 ) { return SingleProviderSyndicate::INVALID; }

        char status = (char)SingleProviderSyndicate::UNREADY;
        ssize_t w  = write( keyfile_fd, (void*)&status, 1 );
        if( w != 1 ) { return SingleProviderSyndicate::INVALID; }

        return SingleProviderSyndicate::PROVIDER;
    }
}


bool
SingleProviderSyndicate::touch() {
    if(! lockholder ) { return false; }


    TemporaryPrivSentry tps(PRIV_CONDOR);

    int rv = futimens( keyfile_fd, NULL );
    return rv == 0;
}


bool
SingleProviderSyndicate::ready( const std::string & message ) {
    if(! lockholder) { return false; }


    TemporaryPrivSentry tps(PRIV_CONDOR);

    std::filesystem::path messagePath = keyfile;
    messagePath.replace_extension( "message" );
    if(! htcondor::writeShortFile( messagePath.string(), message )) {
        dprintf( D_ALWAYS, "SingleProviderSyndicate::ready(): writeShortFile() failed to write message file.\n" );
        return false;
    }

    off_t r = lseek( keyfile_fd, 0, SEEK_SET );
    if( r == -1 ) {
        dprintf( D_ALWAYS, "SingleProviderSyndicate::ready(): failed to seek() on keyfile.\n" );
        return false;
    }

    char status = (char)SingleProviderSyndicate::READY;
    ssize_t w  = write( keyfile_fd, (void*)&status, 1 );
    if( w != 1 ) {
        dprintf( D_ALWAYS, "SingleProviderSyndicate::ready(): failed to write() stats byte to keyfile.\n" );
        return false;
    }

    return true;
}


bool
SingleProviderSyndicate::release() {
    std::error_code ec;
    dprintf( D_ZKM, "SingleProviderSyndicate::release()\n" );

    if(! lockholder) {
        return cleanup();
    } else {
        //
        // A consumer could of course make a new hardlink between the
        // refcount check and the actual removal of the keyfile.  Suppose they
        // do.  The problem isn't that they would believe themselves to
        // be waiting for the producer -- the lease would expire.  The
        // problem is that they would see the READY state in the lockfile
        // and proceed to (fail to) use the resource.
        //
        // Instead, rename() the keyfile (.<keyfile>.<pid>), and _then_ check
        // the hardlink count.  The consumer could still try to create a
        // hardlink between these two steps, but it would fail (causing the
        // consumer to try grabbing the keyfile) because it's path-oriented.
        //
        // Once the keyfile is gone, only existing clients have a refcount,
        // and none of them will ever increment it.  Check the new refcount
        // and behave appropriately.
        //
        TemporaryPrivSentry tps(PRIV_CONDOR);

        std::string pid = std::to_string( getpid() );
        std::filesystem::path key = keyfile.filename();
        std::filesystem::path hiddenKeyFile = keyfile;
        hiddenKeyFile.replace_filename("." + key.string()).replace_extension(pid);
        if( std::filesystem::exists( keyfile ) ) {
            std::filesystem::rename( keyfile, hiddenKeyFile, ec );
            if( ec.value() != 0 ) {
                dprintf( D_ALWAYS, "SingleProviderSyndicate::release(): failed to rename keyfile: %s (%d).\n", ec.message().c_str(), ec.value() );
                return false;
            }
        }

        int count = std::filesystem::hard_link_count( hiddenKeyFile, ec );
        if( ec.value() != 0 ) {
            dprintf( D_ALWAYS, "SingleProviderSyndicate::release(): hard_link_count() failed: %s (%d)\n", ec.message().c_str(), ec.value() );
            return false;
        }

        if( count == 1 ) {
            // A failure to remove the hidden key file won't break anything.
            std::filesystem::remove( hiddenKeyFile, ec );

            std::filesystem::path messagePath = keyfile;
            messagePath.replace_extension( "message" );
            // A failure to remove the message file won't break anything.
            std::filesystem::remove( messagePath, ec );

            return true;
        } else {
            return false;
        }
    }
}


bool
SingleProviderSyndicate::cleanup() {
    std::error_code ec;
    dprintf( D_ZKM, "SingleProviderSyndicate::cleanup()\n" );


    TemporaryPrivSentry tps(PRIV_CONDOR);

    if(! lockholder) {
        std::filesystem::remove( hardlink, ec );
        return true;
    } else {
        //
        // It doesn't matter which file we remove first; in either case,
        // there's an order of operations in which a call to acquire()
        // returns INVALID.
        //
        // We could change the state byte to UNREADY first, but that doesn't
        // help: the producer still needs to exit without waiting for all
        // consumers to be done.
        //
        // This function doesn't have to remove both files: the lease will
        // eventually kick in and elect a new producer.  However, by
        // removing these files immediately, we narrow the window of time
        // in which acquire() could return READY for a resource without a
        // producer process.
        //
        std::filesystem::remove( keyfile, ec );

        std::filesystem::path messagePath = keyfile;
        messagePath.replace_extension( "message" );
        std::filesystem::remove( messagePath, ec );
    }

    return true;
}


#endif /* WINDOWS */
