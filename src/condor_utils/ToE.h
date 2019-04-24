#ifndef   _CONDOR_TOE_H
#define   _CONDOR_TOE_H

namespace ToE {
    extern const int OfItsOwnAccord;
    extern const int DeactivateClaim;
    extern const int DeactivateClaimForcibly;

    extern const char * strings[];
    extern const char * itself;

    bool writeTag( classad::ClassAd * tag, const std::string & jobAdFileName );

    // This asinine construction is required to be able to forward-reference
    // ToE::Tag in condor_event.h, which doesn't need it for its API, but
    // must declare protected/private data members which would rather have it
    // as data member rather than a pointer.  *sigh*
    struct tag_t {
        std::string who;
        std::string how;
        std::string when;
        unsigned int howCode;
    };
    typedef struct tag_t Tag;

    bool decode( classad::ClassAd * tagIn, Tag & tagOut );
};

#endif /* _CONDOR_TOE_H */
