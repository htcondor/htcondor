#ifndef   _CONDOR_ON_DISK_SEMAPHORE_H
#define   _CONDOR_ON_DISK_SEMAPHORE_H

//
// This class does five different things:
// - mutex: only one caller will be asked to PREPARE.
// - refcount: the other callers will maintain a refcount so that the
//   PREPAREr will know when nobody else care any more.
// - lease: the PREPAREr maintains a five-minute lease.
// - status xfer: the PREPAREr changes the status of the ODS to READY.
// - announcement: the PREPAREr sets a string to share with everyone
//   (just before it sets the ODS to READY).
//

//
// acquire(): can return PREPARING, UNREADY, or READY.
//   touch(): extend the lease granted to the PREPARING holder.
//   ready(): advance the state from PREPARING to READY.
// release(): decrement the reference count.
//
// An on-disk semaphore is created (O_CREAT | O_EXCL) with the given name
// in a configurable location on disk.  The creator (the kernel will ensure
// that there is only one) will get the return value PREPARING.  All other
// querents will get UNREADY until the creator calls ready(), after which
// point they will get READY.  (The acquire() function DOES NOT BLOCK.)  The
// creator may call release() after calling ready(); a querent may call
// release() after acquire() returns READY.  (The semaphore return READY from
// the first acquire() call.)
//
// Reference counting (acquire() and release()) will be implemented with
// hardlinks.  For a querent, release() will always return true; for the
// creator (lock holder), it will return false unless the reference count
// on the lock file was already 1, in which case it will unlink it and
// return true.
//
// The touch() function should only be called by the creator / lock holder,
// and it touches the (original) lockfile; this allows acquire() to implement
// a lease.  It's not clear if acquire() should return EXPIRED or just
// immediately try to become the lock holder.
//
// (One possibility is to use lockfile names with sequence numbers; the
//  obvious way to handle clean-up assumes that all other querents survived,
//  but maybe the querents can touch their hardlinks on every query to
//  allow for a clean-up effort there.  --  It might be nice if there were
//  only a single alternate lock-file name, to help reduce race conditions.)
//
// (That is, whoever notices the EXPIRED lockfile tries to acquire() the
//  alternate name and unlinks their hardlink of the other one, and checks
//  to see if the other one's refcount has dropped to 1; if so, it unlinks
//  it as well, since everyone has hardlinked the other one.)
//
// [special handling for hung processes that resume?]
//
// The destructor behaves like release(), except that the on-disk clean-up
// is not optional.
//

#include <string>
#include <filesystem>

class OnDiskSemaphore {

    public:

        enum Status : short {
            MIN = 0,
            PREPARING,
            UNREADY,
            READY,
            INVALID,
            MAX
        };

    public:

        OnDiskSemaphore( const std::string & key );
        virtual ~OnDiskSemaphore();

        // Returns PREPARING, UNREADY, READY on success or INVALID on failure.
        [[nodiscard]] Status acquire( std::string & message );

        [[nodiscard]] bool   touch();
        [[nodiscard]] bool   ready( const std::string & message );
        [[nodiscard]] bool   release();

    protected:

        bool   cleanup();

        std::string             key;
        std::filesystem::path   keyfile;
        std::filesystem::path   hardlink;

        int                     keyfile_fd = -1;
        bool                    lockholder = false;
};

#endif /* _CONDOR_ON_DISK_SEMAPHORE_H */
