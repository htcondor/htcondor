#ifndef   _NULL_FILE_TRANSFER_H
#define   _NULL_FILE_TRANSFER_H

class NullFileTransfer /* : public Service */ {
    public:
        NullFileTransfer();

        // Consider making these part of the constructor.
        void setPeerVersion( const CondorVersionInfo & peerVersion );
        void setTransferAddress( const std::string & address );
        void setTransferKey( const std::string & key );

        // Used for match-session security, if available.
        void setSecuritySession( char const * session_id );

        // A hack; see the comment in jic_shadow.cpp.
        void receiveFilesFromPeer();

    protected:
        std::string         transfer_address;
        std::string         transfer_key;
        std::string         security_session;
};

#endif /* _NULL_FILE_TRANSFER_H */
