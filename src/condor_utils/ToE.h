#ifndef   _CONDOR_TOE_H
#define   _CONDOR_TOE_H

namespace ToE {
    extern const int OfItsOwnAccord;
    extern const int DeactivateClaim;
    extern const int DeactivateClaimForcibly;

    extern const char * strings[];
    extern const char * itself;

    bool writeTag( classad::ClassAd * tag, const std::string & jobAdFileName );

    class Tag {
      public:
        std::string who;
        std::string how;
        std::string when;
        unsigned int howCode;

        bool writeToString( std::string & out ) const;
        bool readFromString( const std::string & in );
    };

    bool decode( classad::ClassAd * tagIn, Tag & tagOut );
    bool encode( const Tag & tagIn, classad::ClassAd * tagOut );
};

#endif /* _CONDOR_TOE_H */
