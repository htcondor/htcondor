#ifndef   _CONDOR_SINGLE_PROVIDER_SYNDICATE_H
#define   _CONDOR_SINGLE_PROVIDER_SYNDICATE_H

/*
    SingleProviderSyndicate

    The single-provider syndicate elects a single provider for a named resource
    from a syndicate of worker processes.  The provider prepares and maintains
    the named resource.  As long as that single provider process neither
    releases the lock nor fails to update the lease, all other processes,
    including ones that join the syndicate later, will be told that the
    resource is UNREADY.  After the provider signals that the resouce is READY,
    then under the same condition, each (and every other) worker will be told
    that the resource is READY.

    The syndicate does not block.

    A process joins the syndicate by calling acquire().  If acquire() returns
    PROVIDER, the process is (now) the provider.  See above for [UN]READY.  If
    acquire() returns INVALID, something has gone terribly wrong somewhere.

    The provider MUST call touch() more frequently than the lease duration
    throughout its lifetime.  If the provider deallocates this object, a
    new provider will be elected (and the message erased).  Thus, every worker
    in the syndicate must be prepared to become the provider after _any_
    call to acquire().

    The provider MUST call ready() when the resource is ready.  The provider
    supplies information about the particular resource in the associated
    message, which will be returned to the other workers via acquire().

    Each worker MUST call release() when it is done using the resource.  If
    the provider calls release(), it will only succeed if no consumer is
    currently using the resource, allowing the provider to poll to determine
    if it is safe to deprovision the resource.  The provider MUST continue
    to call touch() more frequently than the lease duration until release()
    returns true.
*/

#include <string>
#include <filesystem>

class SingleProviderSyndicate {

    public:

        enum Status : short {
            MIN = 0,
            PROVIDER,
            UNREADY,
            READY,
            INVALID,
            MAX
        };

    public:

        SingleProviderSyndicate( const std::string & key );
        virtual ~SingleProviderSyndicate();

        // Returns PROVIDER, UNREADY, or READY on success or INVALID on failure.
        [[nodiscard]] Status acquire( std::string & message );

        [[nodiscard]] bool   touch();
        [[nodiscard]] bool   ready( const std::string & message );
        [[nodiscard]] bool   release();

    protected:

        std::string             key;
        std::filesystem::path   keyfile;
        std::filesystem::path   hardlink;

#ifdef    WINDOWS
#else
        bool   cleanup();

        int                     keyfile_fd = -1;
        bool                    lockholder = false;
#endif /* WINDOWS */
};

#endif /* _CONDOR_SINGLE_PROVIDER_SYNDICATE_H */
